#include "OmtTab.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "SourceList.hpp"
#include "SourceTiles.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace desk {

OmtTab::OmtTab(QWidget *parent) : InputPage(parent)
{
	build();
	refresh();
}

OmtTab::OmtTab(obs_source_t *base_, QWidget *parent) : InputPage(parent), base(base_)
{
	build();

	OBSDataAutoRelease settings = obs_source_get_settings(base);
	tiles->choose(obs_data_get_string(settings, "address"));

	/* what the input receives now, including the decoder (spec §4.3) */
	receiving = new QLabel(this);
	static_cast<QVBoxLayout *>(layout())->insertWidget(0, receiving);

	refresh();
}

void OmtTab::build()
{
	auto *layout = new QVBoxLayout(this);

	tiles = new SourceTiles(text("Omt.None"), this);
	layout->addWidget(tiles, 1);
	connect(tiles, &SourceTiles::pickedChanged, this, [this] {
		byAddress->clear();
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
		if (!typed.isEmpty()) {
			tiles->unpick();
		}
		emit readyChanged();
	});

	/* once a second while the window is open (spec §4.1) */
	connect(&timer, &QTimer::timeout, this, &OmtTab::refresh);
	timer.start(1000);
}

void OmtTab::refresh()
{
	tiles->setGroups(groupBySender(announced(TypeId, "address", "detail")));

	if (receiving) {
		obs_properties_t *props = obs_source_properties(base);
		obs_property_t *line = obs_properties_get(props, "receiving");
		receiving->setText(line ? text("Omt.Receiving").arg(QString::fromUtf8(obs_property_description(line)))
					: QString());
		obs_properties_destroy(props);
	}
}

std::string OmtTab::selected() const
{
	const std::string typed = byAddress->text().trimmed().toStdString();
	return typed.empty() ? tiles->chosen() : typed;
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
