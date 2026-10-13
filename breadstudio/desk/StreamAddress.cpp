#include "StreamAddress.hpp"

#include <cstring>

namespace desk {

struct TypeName {
	StreamType type;
	const char *id;
};

static const TypeName typeNames[] = {
	{StreamType::SrtCaller, "srt_caller"},
	{StreamType::SrtListener, "srt_listener"},
	{StreamType::SrtRendezvous, "srt_rendezvous"},
	{StreamType::RtspUdp, "rtsp_udp"},
	{StreamType::RtspTcp, "rtsp_tcp"},
	{StreamType::TsUdp, "ts_udp"},
	{StreamType::TsTcp, "ts_tcp"},
	{StreamType::TsTcpPull, "ts_tcp_pull"},
};

bool operator==(const StreamForm &a, const StreamForm &b)
{
	return a.type == b.type && a.host == b.host && a.port == b.port && a.latencyMs == b.latencyMs &&
	       a.passphrase == b.passphrase && a.keyLength == b.keyLength && a.streamId == b.streamId &&
	       a.hardwareDecoder == b.hardwareDecoder;
}

const char *streamTypeId(StreamType type)
{
	for (const TypeName &name : typeNames) {
		if (name.type == type) {
			return name.id;
		}
	}
	return typeNames[0].id;
}

StreamType streamTypeFromId(const std::string &id)
{
	for (const TypeName &name : typeNames) {
		if (id == name.id) {
			return name.type;
		}
	}
	return StreamType::SrtCaller;
}

bool isSrt(StreamType type)
{
	return type == StreamType::SrtCaller || type == StreamType::SrtListener || type == StreamType::SrtRendezvous;
}

bool isRtsp(StreamType type)
{
	return type == StreamType::RtspUdp || type == StreamType::RtspTcp;
}

static std::string trimmed(const std::string &value)
{
	const char *blank = " \t\r\n";
	const size_t first = value.find_first_not_of(blank);
	if (first == std::string::npos) {
		return std::string();
	}
	return value.substr(first, value.find_last_not_of(blank) - first + 1);
}

static bool hasControl(const std::string &value)
{
	for (unsigned char c : value) {
		if (c < 0x20 || c == 0x7F) {
			return true;
		}
	}
	return false;
}

/* a type that connects out needs somewhere to connect to; one that listens does not */
static bool needsHost(StreamType type)
{
	return type == StreamType::SrtCaller || type == StreamType::SrtRendezvous || type == StreamType::TsTcpPull;
}

const char *streamProblem(const StreamForm &form)
{
	const std::string host = trimmed(form.host);

	if (isRtsp(form.type)) {
		static const char *scheme = "rtsp://";
		const bool ok = host.size() > std::strlen(scheme) &&
				host.compare(0, std::strlen(scheme), scheme) == 0 &&
				host.find_first_of(" \t") == std::string::npos && !hasControl(host);
		return ok ? nullptr : "Stream.Problem.Url";
	}

	/* a host is a name or an address, never the start of a path, a query or a login */
	if (host.find_first_of(" \t/?#@") != std::string::npos || hasControl(host) ||
	    host.find("://") != std::string::npos) {
		return "Stream.Problem.Host";
	}
	if (host.empty() && needsHost(form.type)) {
		return "Stream.Problem.Host";
	}
	if (form.port < 1 || form.port > 65535) {
		return "Stream.Problem.Port";
	}

	if (isSrt(form.type)) {
		if (form.latencyMs < 0 || form.latencyMs > 60000) {
			return "Stream.Problem.Latency";
		}
		if (hasControl(form.passphrase) || hasControl(form.streamId)) {
			return "Stream.Problem.Text";
		}
		/* SRT's own rule */
		if (!form.passphrase.empty() && (form.passphrase.size() < 10 || form.passphrase.size() > 79)) {
			return "Stream.Problem.Passphrase";
		}
	}

	return nullptr;
}

/* FFmpeg reads an option's value with av_get_token: between '...' nothing is special, and a
 * quote itself is written '\'' */
static std::string quoted(const std::string &value)
{
	std::string out = "'";
	for (char c : value) {
		if (c == '\'') {
			out += "'\\''";
		} else {
			out += c;
		}
	}
	out += "'";
	return out;
}

static std::string hostPort(const std::string &typed, const char *whenEmpty, int port)
{
	std::string host = trimmed(typed);
	if (host.empty()) {
		host = whenEmpty;
	} else if (host.find(':') != std::string::npos && host.front() != '[') {
		host = "[" + host + "]"; /* an IPv6 address */
	}
	return host + ":" + std::to_string(port);
}

StreamTarget streamTarget(const StreamForm &form)
{
	StreamTarget target;

	switch (form.type) {
	case StreamType::SrtCaller:
	case StreamType::SrtListener:
	case StreamType::SrtRendezvous: {
		const char *mode = form.type == StreamType::SrtCaller     ? "caller"
				   : form.type == StreamType::SrtListener ? "listener"
									  : "rendezvous";
		target.input = "srt://" + hostPort(form.host, "0.0.0.0", form.port);
		target.format = "mpegts";
		/* FFmpeg takes SRT's latency in microseconds */
		target.options =
			std::string("mode=") + mode + " latency=" + std::to_string((long long)form.latencyMs * 1000);
		if (!form.passphrase.empty()) {
			const int length = (form.keyLength == 16 || form.keyLength == 24) ? form.keyLength : 32;
			target.options +=
				" passphrase=" + quoted(form.passphrase) + " pbkeylen=" + std::to_string(length);
		}
		if (!form.streamId.empty()) {
			target.options += " streamid=" + quoted(form.streamId);
		}
		break;
	}
	case StreamType::RtspUdp:
	case StreamType::RtspTcp:
		target.input = trimmed(form.host);
		target.options = form.type == StreamType::RtspUdp ? "rtsp_transport=udp" : "rtsp_transport=tcp";
		break;
	case StreamType::TsUdp:
		target.input = "udp://" + hostPort(form.host, "@", form.port);
		target.format = "mpegts";
		break;
	case StreamType::TsTcp:
		target.input = "tcp://" + hostPort(form.host, "0.0.0.0", form.port);
		target.format = "mpegts";
		target.options = "listen=1";
		break;
	case StreamType::TsTcpPull:
		target.input = "tcp://" + hostPort(form.host, "0.0.0.0", form.port);
		target.format = "mpegts";
		break;
	}

	return target;
}

} // namespace desk
