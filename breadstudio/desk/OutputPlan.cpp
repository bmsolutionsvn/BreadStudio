#include "OutputPlan.hpp"

namespace desk {

bool operator==(const OutputConfig &a, const OutputConfig &b)
{
	return a.source == b.source && a.input == b.input && a.width == b.width && a.height == b.height &&
	       a.alpha == b.alpha && a.bus == b.bus && a.ndi == b.ndi && a.omt == b.omt && a.srt == b.srt &&
	       a.srtForm == b.srtForm && a.codec == b.codec && a.bitrateKbps == b.bitrateKbps;
}

bool operator==(const Destination &a, const Destination &b)
{
	return a.on == b.on && a.output == b.output && a.server == b.server && a.key == b.key;
}

OutputRun outputRun(int index, const OutputsConfig &config, bool streaming, int programWidth, int programHeight)
{
	const OutputConfig &output = config.outputs[(size_t)index];
	OutputRun run;

	bool rtmp = false;
	for (size_t d = 0; d < DestinationCount; d++) {
		const Destination &destination = config.destinations[d];
		run.rtmp[d] = streaming && destination.on && destination.output == index;
		rtmp = rtmp || run.rtmp[d];
	}

	run.ndi = output.ndi;
	run.omt = output.omt;
	run.srt = output.srt;
	run.encoders = output.srt || rtmp;

	run.width = programWidth;
	run.height = programHeight;
	if (index > 0) {
		run.canvas = run.encoders || run.ndi || run.omt != OmtMode::Off;
		if (output.width > 0 && output.height > 0) {
			run.width = output.width;
			run.height = output.height;
		}
		run.alpha = output.alpha && output.source == OutputSource::Input;
	}
	return run;
}

OutputSteps outputSteps(int index, const OutputsConfig &now, bool nowStreaming, const OutputsConfig &next,
			bool nextStreaming, int programWidth, int programHeight)
{
	const OutputRun a = outputRun(index, now, nowStreaming, programWidth, programHeight);
	const OutputRun b = outputRun(index, next, nextStreaming, programWidth, programHeight);
	const OutputConfig &x = now.outputs[(size_t)index];
	const OutputConfig &y = next.outputs[(size_t)index];

	OutputSteps steps;
	steps.canvas = a.canvas != b.canvas ||
		       (b.canvas && (a.width != b.width || a.height != b.height || a.alpha != b.alpha));
	steps.encoders = steps.canvas || a.encoders != b.encoders ||
			 (b.encoders && (x.codec != y.codec || x.bitrateKbps != y.bitrateKbps || x.bus != y.bus));
	steps.ndi = steps.canvas || a.ndi != b.ndi || (b.ndi && x.bus != y.bus);
	steps.omt = steps.canvas || a.omt != b.omt || (b.omt != OmtMode::Off && x.bus != y.bus);
	steps.srt = steps.encoders || a.srt != b.srt || (b.srt && !(x.srtForm == y.srtForm));
	for (size_t d = 0; d < DestinationCount; d++) {
		steps.rtmp[d] = a.rtmp[d] != b.rtmp[d] ||
				(b.rtmp[d] && (steps.encoders || !(now.destinations[d] == next.destinations[d])));
	}
	steps.source = x.source != y.source || x.input != y.input;
	return steps;
}

} // namespace desk
