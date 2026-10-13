#pragma once

#include <obs.hpp>

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

private:
	QWidget *makeFooter(int index);

	QFrame *warning;
	QHBoxLayout *row; /* the scroll area, then whatever later tasks add */
	TileDisplay *display;
	QHBoxLayout *footerRow;
	std::vector<OBSSource> sources;
};

} // namespace desk
