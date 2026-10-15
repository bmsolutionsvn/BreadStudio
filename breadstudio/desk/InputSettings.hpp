#pragma once

#include <obs.hpp>

#include <QDialog>

class QListWidget;
class QPushButton;
class QStackedWidget;

namespace desk {

/* The window behind a tile's gear, shaped like vMix's Input Settings: the input's name as
 * the title, its pages down the left, the chosen page on the right, and Copy From… / Close
 * along the bottom (inputs spec §3). It is not modal, so a layer can still be dragged into
 * the Preview pane while it is open. */
class InputSettings : public QDialog {
	Q_OBJECT

public:
	InputSettings(obs_source_t *input, QWidget *parent);

	obs_source_t *input() const { return source; }
	void reload(); /* read the pages again */

private:
	void rebuild();

	OBSSource source;
	QListWidget *list = nullptr;
	QStackedWidget *pages = nullptr;
	QPushButton *copyFrom = nullptr; /* copies another input's desk settings (spec §2/I6) */
};

} // namespace desk
