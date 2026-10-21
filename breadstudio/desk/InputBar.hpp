#pragma once

#include <obs.hpp>

#include <QPoint>
#include <QTimer>
#include <QWidget>

#include <vector>

class QFrame;
class QLabel;
class QMenu;
class QPushButton;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

namespace desk {

class TileDisplay;

/* The input bar dock: one tile per input in input order, wrapped into as many rows as the
 * width allows, and under them a footer with Add Input, Record and Stream. */
class InputBar : public QWidget {
	Q_OBJECT

public:
	explicit InputBar(QWidget *parent = nullptr);

	void refresh();     /* rebuild the tiles from OBS's scene list */
	void updateTally(); /* tally on the title bars and frames, and the Studio Mode warning */
	void clear();       /* drop every reference; safe to call twice */

	void syncAudioButtons(); /* set each tile's audio button from its base's mute state */
	void updateOutputs();    /* Record and Stream follow OBS's outputs */
	void openStreamWindow(); /* the three RTMP destinations (outputs spec §3.2) */

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	int tilesPerRow() const;
	void layoutTiles(); /* make the rows again from `sources` */
	void clearTiles();

	QWidget *makeFooter(int index);
	void openSettings(int index);

	void showMenu(int index, const QPoint &globalPos);
	void renameDialog(obs_source_t *input);
	void deleteDialog(obs_source_t *input);
	void togglePause();
	void pauseMedia(obs_source_t *source);
	bool anyPlaying() const;
	void updatePauseButton();

	std::vector<OBSSignal> muteSignals;
	std::vector<OBSSource> pausedSources; /* what Pause stopped, so Resume starts only those */

	QFrame *warning;
	QScrollArea *scroll;
	QVBoxLayout *rows;
	QPushButton *record;
	QPushButton *stream;
	QPushButton *pauseButton;
	bool paused = false;
	QTimer streamTimer;
	int perRow = 0;
	std::vector<TileDisplay *> displays;     /* one per row */
	std::vector<QLabel *> headers;           /* one per tile */
	std::vector<QToolButton *> audioButtons; /* one per tile */
	std::vector<OBSSource> sources;
};

} // namespace desk
