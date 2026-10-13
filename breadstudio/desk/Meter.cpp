#include "Meter.hpp"

#include <QPainter>

#include <algorithm>

namespace desk {

static const float FloorDb = -60.0f;

static float toLevel(float db)
{
	/* false for negative infinity and for NaN, which both mean silence */
	if (!(db > FloorDb)) {
		return 0.0f;
	}
	return std::min(1.0f, (db - FloorDb) / -FloorDb);
}

Meter::Meter(QWidget *parent) : QWidget(parent)
{
	setFixedWidth(10);
	setMinimumHeight(80);
}

void Meter::setDb(float db)
{
	const float next = toLevel(db);
	if (next != level) {
		level = next;
		update();
	}
}

void Meter::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.fillRect(rect(), QColor(0x20, 0x20, 0x20));

	/* green to -20 dB, yellow to -9 dB, red above: the bands OBS's own meters use */
	auto band = [&](float from, float to, const QColor &color) {
		const float top = std::min(level, to);
		if (top <= from) {
			return;
		}
		const int y0 = height() - (int)(top * (float)height());
		const int y1 = height() - (int)(from * (float)height());
		painter.fillRect(0, y0, width(), y1 - y0, color);
	};

	const float yellowAt = toLevel(-20.0f);
	const float redAt = toLevel(-9.0f);
	band(0.0f, yellowAt, QColor(0x4C, 0xC8, 0x4C));
	band(yellowAt, redAt, QColor(0xE0, 0xC8, 0x30));
	band(redAt, 1.0f, QColor(0xE0, 0x40, 0x40));
}

} // namespace desk
