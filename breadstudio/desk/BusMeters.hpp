#pragma once

#include <obs.h>

#include <atomic>
#include <cstddef>

namespace desk {

/* Peak level of each of OBS's six audio tracks: bus 0 is Master, 1 to 5 are A to E. */
class BusMeters {
public:
	void start();
	void stop(); /* safe to call twice */

	/* the peak since the last call, in dBFS; negative infinity when silent */
	float takePeakDb(size_t bus);

private:
	static void onAudio(void *param, size_t mix, struct audio_data *data);

	std::atomic<float> peak[MAX_AUDIO_MIXES] = {};
	bool running = false;
};

BusMeters &busMeters();

} // namespace desk
