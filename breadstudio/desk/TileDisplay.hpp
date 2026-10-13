#pragma once

#include <obs.hpp>

#include <QWidget>

#include <mutex>
#include <vector>

namespace desk {

/* One display that draws every tile's live picture and tally frame, side by side. */
class TileDisplay : public QWidget {
	Q_OBJECT

public:
	static constexpr int TileW = 160;
	static constexpr int TileH = 90;

	explicit TileDisplay(QWidget *parent = nullptr);
	~TileDisplay() override;

	/* replaces the tiles; an empty list drops every reference */
	void setInputs(const std::vector<OBSSource> &inputs);
	/* one colour per tile, in order */
	void setFrames(const std::vector<uint32_t> &frames);

	QPaintEngine *paintEngine() const override { return nullptr; }

signals:
	void clicked(int index);
	void moved(int from, int to);
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
	int indexAt(const QPoint &pos) const;
	static void draw(void *param, uint32_t cx, uint32_t cy);

	OBSDisplay display;
	std::mutex mutex; /* guards tiles: draw() runs on OBS's graphics thread */
	std::vector<Tile> tiles;
	int count = 0; /* tiles.size(), for the UI thread */
	int pressed = -1;
};

} // namespace desk
