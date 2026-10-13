#pragma once

#include <obs.hpp>

#include <QTimer>
#include <QWidget>

#include <memory>
#include <vector>

class QHBoxLayout;
class QLabel;

namespace desk {

class Meter;

/* The mixer dock: one strip per input with audio and per OBS audio device, and a level
 * for each of the six buses (spec §5). */
class Mixer : public QWidget {
	Q_OBJECT

public:
	explicit Mixer(QWidget *parent = nullptr);
	~Mixer() override;

	void rebuild();  /* make the strips again from the inputs */
	void clear();    /* drop every reference; safe to call twice */
	void syncMute(); /* set each strip's audio button from its source's mute state */

private:
	struct Strip;

	void addStrip(const QString &title, obs_source_t *input, obs_source_t *source);
	void tick();

	QHBoxLayout *stripRow;
	Meter *busMeter[MAX_AUDIO_MIXES];
	QLabel *busNumber[MAX_AUDIO_MIXES];
	float busHold[MAX_AUDIO_MIXES];
	int ticks = 0;
	std::vector<std::unique_ptr<Strip>> strips;
	QTimer timer;
};

} // namespace desk
