#include "OmtTab.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "SourceList.hpp"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace desk {

static const int TilesPerRow = 4;
static const int TileWidth = 200;
static const int TileHeight = 64;

/* the OMT plugin's sources with their tile lines (spec §6.1) */
static std::vector<std::pair<std::string, std::string>> listed()
{
	std::vector<std::pair<std::string, std::string>> result;
	const char *id = obs_get_latest_input_type_id(OmtTab::TypeId);
	obs_properties_t *props = id ? obs_get_source_properties(id) : nullptr;
	if (!props) {
		return result;
	}

	obs_property_t *addresses = obs_properties_get(props, "address");
	obs_property_t *details = obs_properties_get(props, "detail");
	const size_t count = addresses ? obs_property_list_item_count(addresses) : 0;
	const size_t detailCount = details ? obs_property_list_item_count(details) : 0;
	for (size_t i = 0; i < count; i++) {
		const std::string address = obs_property_list_item_string(addresses, i);
		std::string detail;
		/* each detail's value is the address it describes */
		for (size_t j = 0; j < detailCount; j++) {
			if (address == obs_property_list_item_string(details, j)) {
				detail = obs_property_list_item_name(details, j);
				break;
			}
		}
		result.emplace_back(address, detail);
	}

	obs_properties_destroy(props);
	return result;
}

OmtTab::OmtTab(QWidget *parent) : InputPage(parent)
{
	build();
	refresh();
}

OmtTab::OmtTab(obs_source_t *base_, QWidget *parent) : InputPage(parent), base(base_)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	chosen = obs_data_get_string(settings, "address");

	build();

	/* what the input receives now, including the decoder (spec §4.3) */
	receiving = new QLabel(this);
	static_cast<QVBoxLayout *>(layout())->insertWidget(0, receiving);

	refresh();
}

void OmtTab::build()
{
	auto *layout = new QVBoxLayout(this);

	scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	layout->addWidget(scroll, 1);

	buttons = new QButtonGroup(this);
	connect(buttons, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button) {
		byAddress->clear();
		chosen = button->property("address").toString().toStdString();
		emit readyChanged();
	});

	/* for a network where announcements do not arrive (spec §4.1) */
	auto *row = new QHBoxLayout();
	row->addWidget(new QLabel(text("Omt.ByAddress"), this));
	byAddress = new QLineEdit(this);
	byAddress->setPlaceholderText(text("Omt.ByAddress.Hint"));
	row->addWidget(byAddress, 1);
	layout->addLayout(row);

	/* typing an address unpicks the tile */
	connect(byAddress, &QLineEdit::textEdited, this, [this](const QString &typed) {
		if (!typed.isEmpty() && buttons->checkedButton()) {
			buttons->setExclusive(false);
			buttons->checkedButton()->setChecked(false);
			buttons->setExclusive(true);
			chosen.clear();
		}
		emit readyChanged();
	});

	/* once a second while the window is open (spec §4.1) */
	connect(&timer, &QTimer::timeout, this, &OmtTab::refresh);
	timer.start(1000);
}

void OmtTab::refresh()
{
	const std::vector<SenderGroup> groups = groupBySender(listed());

	std::vector<std::string> now;
	for (const SenderGroup &group : groups) {
		for (const ListedSource &source : group.sources) {
			now.push_back(source.address);
		}
	}
	if (now != shown || !scroll->widget()) {
		rebuild(groups);
		shown = now;
	}

	/* the formats change while the tiles stay */
	for (const SenderGroup &group : groups) {
		for (const ListedSource &source : group.sources) {
			tiles[source.address]->setText(QString::fromStdString(source.name + "\n" + source.detail));
		}
	}

	if (receiving) {
		obs_properties_t *props = obs_source_properties(base);
		obs_property_t *line = obs_properties_get(props, "receiving");
		receiving->setText(line ? text("Omt.Receiving").arg(QString::fromUtf8(obs_property_description(line)))
					: QString());
		obs_properties_destroy(props);
	}
}

/* The tiles are made again only when the set of sources changes. Setting the new content deletes
 * the old, and a deleted button leaves its group by itself. */
void OmtTab::rebuild(const std::vector<SenderGroup> &groups)
{
	auto *content = new QWidget();
	auto *column = new QVBoxLayout(content);
	column->setAlignment(Qt::AlignTop);
	tiles.clear();

	for (const SenderGroup &group : groups) {
		column->addWidget(
			new QLabel(group.pc.empty() ? text("Omt.OtherPc") : QString::fromStdString(group.pc), content));
		auto *grid = new QGridLayout();
		grid->setAlignment(Qt::AlignLeft);
		column->addLayout(grid);

		int i = 0;
		for (const ListedSource &source : group.sources) {
			auto *tile = new QToolButton(content);
			tile->setCheckable(true);
			tile->setFixedSize(TileWidth, TileHeight);
			tile->setProperty("address", QString::fromStdString(source.address));
			tile->setToolTip(QString::fromStdString(source.address));
			tile->setChecked(source.address == chosen && byAddress->text().isEmpty());
			buttons->addButton(tile);
			grid->addWidget(tile, i / TilesPerRow, i % TilesPerRow);
			tiles[source.address] = tile;
			i++;
		}
	}

	if (groups.empty()) {
		auto *none = new QLabel(text("Omt.None"), content);
		none->setWordWrap(true);
		column->addWidget(none);
	}

	scroll->setWidget(content);
}

std::string OmtTab::selected() const
{
	const std::string typed = byAddress->text().trimmed().toStdString();
	return typed.empty() ? chosen : typed;
}

bool OmtTab::ready() const
{
	return !selected().empty();
}

OBSSource OmtTab::create()
{
	const std::string address = selected();
	if (base || address.empty()) {
		return OBSSource(); /* nothing to make: the page is for a source that exists, or nothing is picked */
	}

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "address", address.c_str());
	/* as vMix does, the input is named after its source */
	return createInput(TypeId, sourceName(address).c_str(), settings);
}

/* a new address only: any change restarts the receiver */
void OmtTab::apply()
{
	const std::string address = selected();
	if (!base || address.empty()) {
		return;
	}

	OBSDataAutoRelease now = obs_source_get_settings(base);
	if (address == obs_data_get_string(now, "address")) {
		return;
	}

	OBSDataAutoRelease change = obs_data_create();
	obs_data_set_string(change, "address", address.c_str());
	obs_source_update(base, change);
}

} // namespace desk
