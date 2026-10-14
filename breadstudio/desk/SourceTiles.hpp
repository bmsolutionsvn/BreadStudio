#pragma once

#include "SourceList.hpp"

#include <QScrollArea>
#include <QString>

#include <map>
#include <string>
#include <utility>
#include <vector>

class QButtonGroup;
class QToolButton;

namespace desk {

/* (address, detail) pairs from a source type's settings lists (spec §6.1). Each detail item's value is
 * the address it describes. Without `detailKey` every detail is empty. */
std::vector<std::pair<std::string, std::string>> announced(const char *typeId, const char *listKey,
							   const char *detailKey);

/* the description of one property of a source type's settings; empty when there is none */
QString typeNote(const char *typeId, const char *key);

/* Announced sources as tiles under their sender PC's name, as vMix's NDI page shows them (spec §4.1). */
class SourceTiles : public QScrollArea {
	Q_OBJECT

public:
	/* `none` is shown while there is no source */
	SourceTiles(const QString &none, QWidget *parent);

	/* The tiles are made again only when the set of addresses changes; their lines always change. */
	void setGroups(const std::vector<SenderGroup> &groups);

	const std::string &chosen() const { return picked; }

	/* picks that address's tile, now or when it appears */
	void choose(const std::string &address);

	/* no tile picked */
	void unpick();

signals:
	void pickedChanged();

private:
	void rebuild(const std::vector<SenderGroup> &groups);

	QString none;
	std::string picked;
	std::vector<std::string> shown; /* the addresses the tiles were made for, in order */
	std::map<std::string, QToolButton *> tiles;
	QButtonGroup *buttons = nullptr;
};

} // namespace desk
