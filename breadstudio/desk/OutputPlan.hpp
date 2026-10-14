#pragma once

#include "StreamAddress.hpp"

#include <array>
#include <string>

namespace desk {

inline constexpr int OutputCount = 4;
inline constexpr int DestinationCount = 3;

/* what an output shows (outputs spec §3.1); output 1 always shows Program */
enum class OutputSource { Program, Preview, MultiView, Input };
enum class OmtMode { Off, Vmx, Hevc, H264 };
enum class VideoCodec { H264, Hevc };

struct OutputConfig {
	OutputSource source = OutputSource::Program;
	std::string input; /* the input's uuid, when the source is Input */
	int width = 0;     /* 0 and 0: the size of Program */
	int height = 0;
	bool alpha = false;
	int bus = 0; /* OBS's audio track: 0 is the desk's bus M, 1 to 5 are A to E */
	bool ndi = false;
	OmtMode omt = OmtMode::Off;
	bool srt = false;
	StreamForm srtForm; /* its type is one of the three SRT types */
	VideoCodec codec = VideoCodec::H264;
	int bitrateKbps = 6000;
};

struct Destination {
	bool on = false;
	int output = 0; /* 0 to 3 */
	std::string server;
	std::string key;
};

struct OutputsConfig {
	std::array<OutputConfig, OutputCount> outputs;
	std::array<Destination, DestinationCount> destinations;
};

bool operator==(const OutputConfig &a, const OutputConfig &b);
bool operator==(const Destination &a, const Destination &b);

/* What must run for one output. Output 1 (index 0) is the main mix and never has a canvas. */
struct OutputRun {
	bool canvas = false;
	int width = 0; /* the size sent */
	int height = 0;
	bool alpha = false; /* a BGRA canvas */
	bool encoders = false;
	bool ndi = false;
	OmtMode omt = OmtMode::Off;
	bool srt = false;
	std::array<bool, DestinationCount> rtmp{}; /* the destinations sending this output */
};

OutputRun outputRun(int index, const OutputsConfig &config, bool streaming, int programWidth, int programHeight);

/* What a change does to one output. A true piece is stopped and, if it is still wanted, started
 * again; a false piece keeps running untouched (outputs spec §3.3, §4.2). `source` alone means only
 * what the output shows changes, and nothing restarts. */
struct OutputSteps {
	bool canvas = false;
	bool encoders = false;
	bool ndi = false;
	bool omt = false;
	bool srt = false;
	std::array<bool, DestinationCount> rtmp{};
	bool source = false;
};

OutputSteps outputSteps(int index, const OutputsConfig &now, bool nowStreaming, const OutputsConfig &next,
			bool nextStreaming, int programWidth, int programHeight);

} // namespace desk
