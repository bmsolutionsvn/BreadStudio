#pragma once

#include <obs.hpp>

#include <QDialog>

class QListWidget;

namespace desk {

/* The window behind a tile's gear: the base's properties and the layers list. It is not
 * modal, because layers are placed by dragging them in OBS's Preview pane. */
class InputSettings : public QDialog {
	Q_OBJECT

public:
	InputSettings(obs_source_t *input, QWidget *parent);

	obs_source_t *input() const { return source; }
	void reload(); /* read the layers again */

private:
	int64_t selected() const;

	OBSSource source;
	QListWidget *list;
	bool loading = false;
};

} // namespace desk
