#include "SourceTiles.hpp"

#include <obs.h>

#include <QButtonGroup>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

#include "Desk.hpp"

namespace desk {

static const int TilesPerRow = 4;
static const int TileWidth = 200;
static const int TileHeight = 64;

std::vector<std::pair<std::string, std::string>> announced(const char *typeId, const char *listKey,
							   const char *detailKey)
{
	std::vector<std::pair<std::string, std::string>> result;
	const char *id = obs_get_latest_input_type_id(typeId);
	obs_properties_t *props = id ? obs_get_source_properties(id) : nullptr;
	if (!props) {
		return result;
	}

	obs_property_t *addresses = obs_properties_get(props, listKey);
	obs_property_t *details = detailKey ? obs_properties_get(props, detailKey) : nullptr;
	const size_t count = addresses ? obs_property_list_item_count(addresses) : 0;
	const size_t detailCount = details ? obs_property_list_item_count(details) : 0;
	for (size_t i = 0; i < count; i++) {
		const std::string address = obs_property_list_item_string(addresses, i);
		std::string detail;
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

QString typeNote(const char *typeId, const char *key)
{
	const char *id = obs_get_latest_input_type_id(typeId);
	obs_properties_t *props = id ? obs_get_source_properties(id) : nullptr;
	obs_property_t *property = props ? obs_properties_get(props, key) : nullptr;
	const QString note = property ? QString::fromUtf8(obs_property_description(property)) : QString();
	obs_properties_destroy(props);
	return note;
}

SourceTiles::SourceTiles(const QString &none_, QWidget *parent) : QScrollArea(parent), none(none_)
{
	setWidgetResizable(true);

	buttons = new QButtonGroup(this);
	connect(buttons, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button) {
		picked = button->property("address").toString().toStdString();
		emit pickedChanged();
	});

	rebuild({});
}

void SourceTiles::setGroups(const std::vector<SenderGroup> &groups)
{
	std::vector<std::string> now;
	for (const SenderGroup &group : groups) {
		for (const ListedSource &source : group.sources) {
			now.push_back(source.address);
		}
	}
	if (now != shown) {
		rebuild(groups);
		shown = now;
	}

	for (const SenderGroup &group : groups) {
		for (const ListedSource &source : group.sources) {
			const std::string line = source.detail.empty() ? source.name
								       : source.name + "\n" + source.detail;
			tiles[source.address]->setText(QString::fromStdString(line));
		}
	}
}

void SourceTiles::choose(const std::string &address)
{
	picked = address;
	auto tile = tiles.find(address);
	if (tile != tiles.end()) {
		tile->second->setChecked(true);
	}
}

void SourceTiles::unpick()
{
	if (QAbstractButton *checked = buttons->checkedButton()) {
		buttons->setExclusive(false);
		checked->setChecked(false);
		buttons->setExclusive(true);
	}
	picked.clear();
}

/* Setting the new content deletes the old, and a deleted button leaves its group by itself. */
void SourceTiles::rebuild(const std::vector<SenderGroup> &groups)
{
	auto *content = new QWidget();
	auto *column = new QVBoxLayout(content);
	column->setAlignment(Qt::AlignTop);
	tiles.clear();

	for (const SenderGroup &group : groups) {
		column->addWidget(new QLabel(
			group.pc.empty() ? text("Sources.OtherPc") : QString::fromStdString(group.pc), content));
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
			tile->setChecked(source.address == picked);
			buttons->addButton(tile);
			grid->addWidget(tile, i / TilesPerRow, i % TilesPerRow);
			tiles[source.address] = tile;
			i++;
		}
	}

	if (groups.empty()) {
		auto *empty = new QLabel(none, content);
		empty->setWordWrap(true);
		column->addWidget(empty);
	}

	setWidget(content);
}

} // namespace desk
