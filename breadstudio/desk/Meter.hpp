#pragma once

#include <QWidget>

namespace desk {

/* A vertical level bar: -60 dB at the bottom, 0 dB at the top. */
class Meter : public QWidget {
public:
	explicit Meter(QWidget *parent = nullptr);

	void setDb(float db);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	float level = 0.0f; /* 0 to 1 */
};

} // namespace desk
