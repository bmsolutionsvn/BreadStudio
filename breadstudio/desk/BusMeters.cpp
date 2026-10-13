#include "BusMeters.hpp"

#include <obs-audio-controls.h>

#include <cmath>

namespace desk {

BusMeters &busMeters()
{
	static BusMeters instance;
	return instance;
}

void BusMeters::start()
{
	if (running) {
		return;
	}
	audio_t *audio = obs_get_audio();
	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		/* no conversion: OBS's own format, float planar */
		audio_output_connect(audio, mix, nullptr, onAudio, this);
	}
	running = true;
}

void BusMeters::stop()
{
	if (!running) {
		return;
	}
	audio_t *audio = obs_get_audio();
	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		audio_output_disconnect(audio, mix, onAudio, this);
	}
	running = false;
}

/* runs on OBS's audio thread */
void BusMeters::onAudio(void *param, size_t mix, struct audio_data *data)
{
	auto *self = static_cast<BusMeters *>(param);
	const size_t channels = audio_output_get_channels(obs_get_audio());

	float highest = 0.0f;
	for (size_t ch = 0; ch < channels; ch++) {
		const float *samples = reinterpret_cast<const float *>(data->data[ch]);
		if (!samples) {
			continue;
		}
		for (uint32_t i = 0; i < data->frames; i++) {
			const float level = std::fabs(samples[i]);
			if (level > highest) {
				highest = level;
			}
		}
	}

	float seen = self->peak[mix].load();
	while (highest > seen && !self->peak[mix].compare_exchange_weak(seen, highest)) {
	}
}

float BusMeters::takePeakDb(size_t bus)
{
	return obs_mul_to_db(peak[bus].exchange(0.0f));
}

} // namespace desk
