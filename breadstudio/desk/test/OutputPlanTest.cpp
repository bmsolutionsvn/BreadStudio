#include "OutputPlan.hpp"

#include <cstdio>

using namespace desk;

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

static const int W = 1920;
static const int H = 1080;

static OutputSteps steps(int index, const OutputsConfig &now, bool nowLive, const OutputsConfig &next, bool nextLive)
{
	return outputSteps(index, now, nowLive, next, nextLive, W, H);
}

static bool nothing(const OutputSteps &s)
{
	return !s.canvas && !s.encoders && !s.ndi && !s.omt && !s.srt && !s.rtmp[0] && !s.rtmp[1] && !s.rtmp[2] &&
	       !s.source;
}

int main()
{
	/* all off: nothing runs; output 1 is Program's size */
	{
		OutputsConfig config;
		for (int i = 0; i < OutputCount; i++) {
			OutputRun run = outputRun(i, config, false, W, H);
			CHECK(!run.canvas && !run.encoders && !run.ndi && !run.srt);
		}
		CHECK(outputRun(0, config, false, W, H).width == W);
	}

	/* output 1 never has a canvas, whatever it sends */
	{
		OutputsConfig config;
		config.outputs[0].ndi = true;
		config.outputs[0].srt = true;
		OutputRun run = outputRun(0, config, false, W, H);
		CHECK(!run.canvas && run.ndi && run.encoders && run.srt);
	}

	/* output 2 with NDI: a canvas at Program's size, no encoders */
	{
		OutputsConfig config;
		config.outputs[1].ndi = true;
		OutputRun run = outputRun(1, config, false, W, H);
		CHECK(run.canvas && run.width == W && run.height == H && !run.encoders);
	}

	/* alpha only for an input */
	{
		OutputsConfig config;
		config.outputs[2].ndi = true;
		config.outputs[2].alpha = true;
		CHECK(!outputRun(2, config, false, W, H).alpha);
		config.outputs[2].source = OutputSource::Input;
		config.outputs[2].input = "uuid";
		CHECK(outputRun(2, config, false, W, H).alpha);
	}

	/* a destination on an output that sends nothing else: a canvas and encoders, only while streaming */
	{
		OutputsConfig config;
		config.destinations[1] = Destination{true, 3, "rtmp://a/live", "k"};
		CHECK(!outputRun(3, config, false, W, H).canvas);
		OutputRun run = outputRun(3, config, true, W, H);
		CHECK(run.canvas && run.encoders && run.rtmp[1] && !run.rtmp[0]);
	}

	/* starting the stream on an output that already sends SRT and NDI touches only that destination */
	{
		OutputsConfig config;
		config.outputs[1].ndi = true;
		config.outputs[1].srt = true;
		config.destinations[0] = Destination{true, 1, "rtmp://a/live", "k"};
		OutputSteps s = steps(1, config, false, config, true);
		CHECK(s.rtmp[0] && !s.canvas && !s.encoders && !s.ndi && !s.srt && !s.source);
		CHECK(nothing(steps(0, config, false, config, true)));
	}

	/* a destination's key changes while streaming: that destination only */
	{
		OutputsConfig now;
		now.outputs[0].srt = true;
		now.destinations[0] = Destination{true, 0, "rtmp://a/live", "k1"};
		now.destinations[1] = Destination{true, 0, "rtmp://b/live", "k"};
		OutputsConfig next = now;
		next.destinations[0].key = "k2";
		OutputSteps s = steps(0, now, true, next, true);
		CHECK(s.rtmp[0] && !s.rtmp[1] && !s.encoders && !s.srt);
	}

	/* a destination moves from output 1 to output 2: both outputs see it */
	{
		OutputsConfig now;
		now.destinations[0] = Destination{true, 0, "rtmp://a/live", "k"};
		OutputsConfig next = now;
		next.destinations[0].output = 1;
		CHECK(steps(0, now, true, next, true).rtmp[0]);
		CHECK(steps(1, now, true, next, true).rtmp[0]);
	}

	/* a new size restarts every sender of that output */
	{
		OutputsConfig now;
		now.outputs[2].ndi = true;
		now.outputs[2].srt = true;
		now.destinations[2] = Destination{true, 2, "rtmp://a/live", "k"};
		OutputsConfig next = now;
		next.outputs[2].width = 1280;
		next.outputs[2].height = 720;
		OutputSteps s = steps(2, now, true, next, true);
		CHECK(s.canvas && s.encoders && s.ndi && s.srt && s.rtmp[2]);
	}

	/* a new bitrate restarts what uses the encoders, not NDI */
	{
		OutputsConfig now;
		now.outputs[1].ndi = true;
		now.outputs[1].srt = true;
		OutputsConfig next = now;
		next.outputs[1].bitrateKbps = 8000;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(s.encoders && s.srt && !s.ndi && !s.canvas);
	}

	/* a new bus restarts what carries the sound */
	{
		OutputsConfig now;
		now.outputs[1].ndi = true;
		OutputsConfig next = now;
		next.outputs[1].bus = 3;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(s.ndi && !s.canvas);
	}

	/* a new SRT setting restarts SRT only */
	{
		OutputsConfig now;
		now.outputs[1].srt = true;
		now.outputs[1].ndi = true;
		OutputsConfig next = now;
		next.outputs[1].srtForm.port = 6000;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(s.srt && !s.encoders && !s.ndi);
	}

	/* a new source changes only what is shown */
	{
		OutputsConfig now;
		now.outputs[1].ndi = true;
		now.outputs[1].srt = true;
		OutputsConfig next = now;
		next.outputs[1].source = OutputSource::Preview;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(s.source && !s.canvas && !s.encoders && !s.ndi && !s.srt);
		next = now;
		next.outputs[1].source = OutputSource::Input;
		next.outputs[1].input = "a";
		OutputsConfig other = next;
		other.outputs[1].input = "b";
		CHECK(steps(1, next, false, other, false).source);
	}

	/* alpha on an output that shows Program changes nothing */
	{
		OutputsConfig now;
		now.outputs[1].ndi = true;
		OutputsConfig next = now;
		next.outputs[1].alpha = true;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(!s.canvas && !s.ndi);
	}

	/* the same settings: nothing */
	{
		OutputsConfig config;
		config.outputs[1].ndi = true;
		config.outputs[1].omt = OmtMode::Hevc;
		config.destinations[0] = Destination{true, 1, "rtmp://a/live", "k"};
		for (int i = 0; i < OutputCount; i++) {
			CHECK(nothing(steps(i, config, true, config, true)));
		}
	}

	/* alpha on an input that is sent restarts its canvas (BGRA vs NV12) */
	{
		OutputsConfig now;
		now.outputs[1].ndi = true;
		now.outputs[1].source = OutputSource::Input;
		now.outputs[1].input = "uuid";
		OutputsConfig next = now;
		next.outputs[1].alpha = true;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(s.canvas && s.encoders && s.ndi);
	}

	/* OMT restarts on a bus change and on a mode change */
	{
		OutputsConfig now;
		now.outputs[1].omt = OmtMode::Hevc;
		OutputsConfig next = now;
		next.outputs[1].bus = 2;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(s.omt && !s.canvas);
		next = now;
		next.outputs[1].omt = OmtMode::H264;
		CHECK(steps(1, now, false, next, false).omt);
	}

	/* a codec change restarts what uses the encoders */
	{
		OutputsConfig now;
		now.outputs[1].srt = true;
		OutputsConfig next = now;
		next.outputs[1].codec = VideoCodec::Hevc;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(s.encoders && s.srt && !s.canvas);
	}

	if (failures == 0) {
		std::printf("OutputPlan: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
