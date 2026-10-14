#pragma once

#include "MultiviewSource.hpp"
#include "OutputPlan.hpp"

#include <obs.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace desk {

/* what the Outputs and Stream windows show for one sender (outputs spec §3.3) */
enum class SenderState { Off, Waiting, Connected, Retrying, Live, Failed };

struct SenderStatus {
	SenderState state = SenderState::Off;
	int receivers = -1; /* NDI and OMT; -1 when not known */
	uint64_t bytes = 0; /* sent so far */
	int dropped = 0;    /* frames dropped for the network (RTMP) */
};

struct OutputStatus {
	int width = 0; /* the picture sent; 0 while the output sends nothing */
	int height = 0;
	uint32_t skipped = 0; /* frames the PC fell behind on */
	std::string encoder;  /* the video encoder's name; empty while none runs */
	SenderStatus ndi;
	SenderStatus omt;
	SenderStatus srt;
};

/* The four outputs and the three RTMP destinations, running (outputs spec §4). UI thread only. */
class OutputEngine {
public:
	static OutputEngine &instance();

	const OutputsConfig &config() const { return wanted; }

	/* starts, stops and restarts only what outputSteps() says */
	void apply(const OutputsConfig &next);

	bool streaming() const { return live; }
	void setStreaming(bool on);

	/* what each output shows again: Preview moved, Studio Mode changed, an input went or came */
	void refreshSources();

	/* every output black, its senders still running: the scene collection is about to go */
	void detachSources();

	/* everything stopped and let go, the settings kept: OBS is closing */
	void shutdown();

	void save(obs_data_t *data) const;
	OutputsConfig load(obs_data_t *data) const;

	OutputStatus status(int output) const;
	SenderStatus destinationStatus(int destination) const;

	/* whether a plugin has registered this output type */
	static bool hasOutputType(const char *id);

private:
	void reconcile(const OutputsConfig &next, bool nextLive);
	video_t *videoOf(int output) const;
	OBSSource sourceFor(int output) const;
	void show(int output);
	void startSenders(int output);
	void startDestination(int destination);
	void stopDestination(int destination);

	struct Running {
		OBSCanvasAutoRelease canvas;
		OBSSource shown;
		OBSEncoderAutoRelease video;
		OBSEncoderAutoRelease audio;
		OBSOutputAutoRelease ndi;
		OBSOutputAutoRelease omt;
		OBSOutputAutoRelease srt;
	};

	OutputsConfig wanted;
	bool live = false;
	bool detached = false;
	std::array<Running, OutputCount> running;
	std::array<OBSServiceAutoRelease, DestinationCount> services;
	std::array<OBSOutputAutoRelease, DestinationCount> rtmp;
	OBSSourceAutoRelease program;   /* one bread_program for every output that shows Program */
	OBSSourceAutoRelease multiview; /* one bread_multiview for every output that shows MultiView */
};

} // namespace desk
