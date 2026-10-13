#pragma once

#include <obs.hpp>

#include <QWidget>

#include <mutex>
#include <vector>

namespace desk {

/* One display that draws the live picture and tally frame of every tile in one row of the
 * input bar, side by side. */
class TileDisplay : public QWidget {
	Q_OBJECT

public:
	static constexpr int TileW = 176;
	static constexpr int TileH = 99;

	explicit TileDisplay(QWidget *parent = nullptr);
	~TileDisplay() override;

	/* replaces the tiles; an empty list drops every reference. `first` is the place in the
	 * input bar of this row's first tile, so the signals carry input-bar places. */
	void setInputs(const std::vector<OBSSource> &inputs, int first = 0);
	/* one colour per tile, in order */
	void setFrames(const std::vector<uint32_t> &frames);

	/* the input-bar place of the tile under that point; -1 when it is outside this row */
	int indexAtGlobal(const QPoint &globalPos) const;

	QPaintEngine *paintEngine() const override { return nullptr; }

signals:
	void clicked(int index);
	/* pressed on tile `from` and released somewhere else, perhaps over another row */
	void dragged(int from, const QPoint &globalPos);
	void menuRequested(int index, const QPoint &globalPos);

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	struct Tile {
		OBSSource input;
		uint32_t frame;
	};

	void createDisplay();
	QSize pixelSize() const;
	static void draw(void *param, uint32_t cx, uint32_t cy);

	OBSDisplay display;
	std::mutex mutex; /* guards tiles: draw() runs on OBS's graphics thread */
	std::vector<Tile> tiles;
	int count = 0; /* tiles.size(), for the UI thread */
	int first = 0;
	int pressed = -1;
};

} // namespace desk
