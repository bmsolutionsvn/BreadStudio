#pragma once

#include <obs.hpp>

#include <QPoint>
#include <QWidget>

#include <vector>

class QFrame;
class QHBoxLayout;

namespace desk {

class TileDisplay;

/* The input bar dock: one tile per input, in input order. */
class InputBar : public QWidget {
	Q_OBJECT

public:
	explicit InputBar(QWidget *parent = nullptr);

	void refresh();     /* rebuild the tiles from OBS's scene list */
	void updateTally(); /* tally frames, and the Studio Mode warning */
	void clear();       /* drop every reference; safe to call twice */

	void syncAudioButtons(); /* set each tile's audio button from its base's mute state */

private:
	QWidget *makeFooter(int index);

	void addInput(const char *textKey, const char *typeId, bool blank);
	void showMenu(int index, const QPoint &globalPos);
	void renameDialog(obs_source_t *input);
	void deleteDialog(obs_source_t *input);

	std::vector<OBSSignal> muteSignals;

	QFrame *warning;
	QHBoxLayout *row; /* the scroll area, then whatever later tasks add */
	TileDisplay *display;
	QHBoxLayout *footerRow;
	std::vector<OBSSource> sources;
};

} // namespace desk
