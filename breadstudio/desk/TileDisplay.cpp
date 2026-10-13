#include "TileDisplay.hpp"
#include "Desk.hpp"

#include <QMouseEvent>
#include <QWindow>

#include <algorithm>

namespace desk {

TileDisplay::TileDisplay(QWidget *parent) : QWidget(parent)
{
	/* the same attributes OBS's own display widget sets */
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);
	setFixedSize(TileW, TileH);

	connect(windowHandle(), &QWindow::visibleChanged, this, [this](bool visible) {
		if (visible) {
			createDisplay();
		}
	});
}

TileDisplay::~TileDisplay()
{
	if (display) {
		obs_display_remove_draw_callback(display, draw, this);
	}
	display = nullptr;
	setInputs({});
}

QSize TileDisplay::pixelSize() const
{
	const qreal ratio = devicePixelRatioF();
	return QSize((int)(width() * ratio), (int)(height() * ratio));
}

void TileDisplay::createDisplay()
{
	if (display || !windowHandle() || !windowHandle()->isExposed()) {
		return;
	}

	const QSize size = pixelSize();

	gs_init_data info = {};
	info.cx = (uint32_t)size.width();
	info.cy = (uint32_t)size.height();
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
	info.window.hwnd = reinterpret_cast<void *>(windowHandle()->winId());

	display = obs_display_create(&info, 0xFF000000);
	obs_display_add_draw_callback(display, draw, this);
}

void TileDisplay::paintEvent(QPaintEvent *event)
{
	createDisplay();
	QWidget::paintEvent(event);
}

void TileDisplay::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	createDisplay();

	if (display) {
		const QSize size = pixelSize();
		obs_display_resize(display, (uint32_t)size.width(), (uint32_t)size.height());
	}
}

void TileDisplay::setInputs(const std::vector<OBSSource> &inputs)
{
	/* a source drawn in a display must be marked as showing, or cameras stay off */
	std::vector<Tile> next;
	for (const OBSSource &input : inputs) {
		next.push_back({input, IdleColor});
		obs_source_inc_showing(input);
	}

	{
		std::lock_guard<std::mutex> lock(mutex);
		tiles.swap(next);
	}

	for (Tile &old : next) {
		obs_source_dec_showing(old.input);
	}

	count = (int)inputs.size();
	setFixedSize(std::max(1, count) * TileW, TileH);
}

void TileDisplay::setFrames(const std::vector<uint32_t> &frames)
{
	std::lock_guard<std::mutex> lock(mutex);
	for (size_t i = 0; i < tiles.size() && i < frames.size(); i++) {
		tiles[i].frame = frames[i];
	}
}

/* runs on OBS's graphics thread */
void TileDisplay::draw(void *param, uint32_t cx, uint32_t cy)
{
	auto *self = static_cast<TileDisplay *>(param);
	std::lock_guard<std::mutex> lock(self->mutex);
	if (self->tiles.empty()) {
		return;
	}

	obs_video_info ovi = {};
	if (!obs_get_video_info(&ovi)) {
		return;
	}

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	auto box = [&](float w, float h, uint32_t argb) {
		gs_effect_set_color(color, argb);
		while (gs_effect_loop(solid, "Solid")) {
			gs_draw_sprite(nullptr, 0, (uint32_t)w, (uint32_t)h);
		}
	};

	const float height = (float)cy;
	const float tileW = (float)cx / (float)self->tiles.size();
	const float border = std::max(2.0f, height / 30.0f);
	const float canvasW = (float)ovi.base_width;
	const float canvasH = (float)ovi.base_height;

	for (size_t i = 0; i < self->tiles.size(); i++) {
		const Tile &tile = self->tiles[i];
		const float x = tileW * (float)i;

		gs_viewport_push();
		gs_projection_push();

		/* the frame: the whole tile in the tally colour */
		gs_set_viewport((int)x, 0, (int)tileW, (int)height);
		gs_ortho(0.0f, tileW, 0.0f, height, -100.0f, 100.0f);
		box(tileW, height, tile.frame);

		/* the picture, fitted inside the frame */
		const float scale = std::min((tileW - border * 2.0f) / canvasW, (height - border * 2.0f) / canvasH);
		const float w = canvasW * scale;
		const float h = canvasH * scale;
		gs_set_viewport((int)(x + (tileW - w) / 2.0f), (int)((height - h) / 2.0f), (int)w, (int)h);
		gs_ortho(0.0f, canvasW, 0.0f, canvasH, -100.0f, 100.0f);
		box(canvasW, canvasH, 0xFF000000);
		obs_source_video_render(tile.input);

		gs_projection_pop();
		gs_viewport_pop();
	}
}

int TileDisplay::indexAt(const QPoint &pos) const
{
	if (count == 0) {
		return -1;
	}
	return std::clamp(pos.x() / TileW, 0, count - 1);
}

void TileDisplay::mousePressEvent(QMouseEvent *event)
{
	const int index = indexAt(event->position().toPoint());
	if (index < 0) {
		return;
	}

	if (event->button() == Qt::RightButton) {
		emit menuRequested(index, event->globalPosition().toPoint());
	} else if (event->button() == Qt::LeftButton) {
		pressed = index;
	}
}

void TileDisplay::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton || pressed < 0) {
		return;
	}

	const int from = pressed;
	pressed = -1;

	/* released on the tile it was pressed on: a click. On another: a drag. */
	const int to = indexAt(event->position().toPoint());
	if (to == from) {
		emit clicked(from);
	} else if (to >= 0) {
		emit moved(from, to);
	}
}

} // namespace desk
