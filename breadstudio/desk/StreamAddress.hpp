#pragma once

#include <string>

namespace desk {

/* the kinds of network stream the Stream / SRT page offers (spec §5.2) */
enum class StreamType { SrtCaller, SrtListener, SrtRendezvous, RtspUdp, RtspTcp, TsUdp, TsTcp, TsTcpPull };

/* What the page holds (spec §5.1). For the two RTSP types `host` is the whole rtsp:// address
 * and `port` is not used. */
struct StreamForm {
	StreamType type = StreamType::SrtCaller;
	std::string host;
	int port = 5000;
	int latencyMs = 200;
	std::string passphrase;
	int keyLength = 32;
	std::string streamId;
	bool hardwareDecoder = true;
};

bool operator==(const StreamForm &a, const StreamForm &b);

/* what OBS's Media Source is given */
struct StreamTarget {
	std::string input;   /* the address */
	std::string format;  /* "mpegts", or empty to let FFmpeg find out */
	std::string options; /* FFmpeg options: key=value pairs with a space between */
};

/* the name a type is saved under, and back; a name that is not known gives SrtCaller */
const char *streamTypeId(StreamType type);
StreamType streamTypeFromId(const std::string &id);

bool isSrt(StreamType type);
bool isRtsp(StreamType type);

/* Why the form cannot be used yet, as a locale key; null when it can. */
const char *streamProblem(const StreamForm &form);

/* The address and options for a form. Call only when streamProblem() is null. */
StreamTarget streamTarget(const StreamForm &form);

} // namespace desk
