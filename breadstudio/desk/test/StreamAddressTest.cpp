#include "StreamAddress.hpp"

#include <cstdio>
#include <cstring>
#include <string>

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

static bool problemIs(const StreamForm &form, const char *key)
{
	const char *problem = streamProblem(form);
	if (!key) {
		return problem == nullptr;
	}
	return problem && std::strcmp(problem, key) == 0;
}

static StreamForm srt(StreamType type, const char *host)
{
	StreamForm form;
	form.type = type;
	form.host = host;
	return form;
}

int main()
{
	/* SRT caller, nothing optional: only mode and latency, latency in microseconds */
	{
		const StreamTarget t = streamTarget(srt(StreamType::SrtCaller, "10.0.0.5"));
		CHECK(t.input == "srt://10.0.0.5:5000");
		CHECK(t.format == "mpegts");
		CHECK(t.options == "mode=caller latency=200000");
	}

	/* SRT listener: an empty host means every network card */
	{
		StreamForm form = srt(StreamType::SrtListener, "");
		form.port = 9000;
		form.latencyMs = 120;
		CHECK(problemIs(form, nullptr));
		const StreamTarget t = streamTarget(form);
		CHECK(t.input == "srt://0.0.0.0:9000");
		CHECK(t.options == "mode=listener latency=120000");
	}

	/* SRT rendezvous */
	{
		const StreamTarget t = streamTarget(srt(StreamType::SrtRendezvous, "cam.example"));
		CHECK(t.input == "srt://cam.example:5000");
		CHECK(t.options == "mode=rendezvous latency=200000");
	}

	/* passphrase and stream ID are quoted, so a space, & and = stay part of the value;
	 * the key length goes with the passphrase and not without it */
	{
		StreamForm form = srt(StreamType::SrtCaller, "10.0.0.5");
		form.passphrase = "secret pass&word=1";
		form.keyLength = 16;
		form.streamId = "#!::r=live,m=request";
		const StreamTarget t = streamTarget(form);
		CHECK(t.input == "srt://10.0.0.5:5000");
		CHECK(t.options == "mode=caller latency=200000 passphrase='secret pass&word=1' pbkeylen=16 "
				   "streamid='#!::r=live,m=request'");
	}

	/* a quote in a value is written '\'' */
	{
		StreamForm form = srt(StreamType::SrtCaller, "10.0.0.5");
		form.streamId = "it's";
		CHECK(streamTarget(form).options == "mode=caller latency=200000 streamid='it'\\''s'");
	}

	/* a key length that is not 16, 24 or 32 becomes 32 */
	{
		StreamForm form = srt(StreamType::SrtCaller, "10.0.0.5");
		form.passphrase = "0123456789";
		form.keyLength = 99;
		CHECK(streamTarget(form).options == "mode=caller latency=200000 passphrase='0123456789' pbkeylen=32");
	}

	/* the host is trimmed; an IPv6 address gets brackets, once */
	{
		CHECK(streamTarget(srt(StreamType::SrtCaller, "  10.0.0.5 ")).input == "srt://10.0.0.5:5000");
		CHECK(streamTarget(srt(StreamType::SrtCaller, "::1")).input == "srt://[::1]:5000");
		CHECK(streamTarget(srt(StreamType::SrtCaller, "[fe80::1]")).input == "srt://[fe80::1]:5000");
	}

	/* RTSP: the address as typed, trimmed; the transport is an option */
	{
		StreamForm form = srt(StreamType::RtspTcp, " rtsp://cam.local:554/stream1 ");
		StreamTarget t = streamTarget(form);
		CHECK(t.input == "rtsp://cam.local:554/stream1");
		CHECK(t.format.empty());
		CHECK(t.options == "rtsp_transport=tcp");
		form.type = StreamType::RtspUdp;
		CHECK(streamTarget(form).options == "rtsp_transport=udp");
	}

	/* transport stream over UDP: with no host it listens on the port */
	{
		StreamForm form = srt(StreamType::TsUdp, "");
		form.port = 1234;
		StreamTarget t = streamTarget(form);
		CHECK(t.input == "udp://@:1234");
		CHECK(t.format == "mpegts");
		CHECK(t.options.empty());
		form.host = "239.1.1.1";
		CHECK(streamTarget(form).input == "udp://239.1.1.1:1234");
	}

	/* transport stream over TCP: listening, and pulling */
	{
		StreamForm form = srt(StreamType::TsTcp, "");
		form.port = 7000;
		StreamTarget t = streamTarget(form);
		CHECK(t.input == "tcp://0.0.0.0:7000");
		CHECK(t.options == "listen=1");
		CHECK(t.format == "mpegts");
		form.type = StreamType::TsTcpPull;
		form.host = "10.0.0.9";
		t = streamTarget(form);
		CHECK(t.input == "tcp://10.0.0.9:7000");
		CHECK(t.options.empty());
	}

	/* what stops OK */
	{
		CHECK(problemIs(srt(StreamType::SrtCaller, ""), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtCaller, "   "), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtRendezvous, ""), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::TsTcpPull, ""), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtListener, ""), nullptr));
		CHECK(problemIs(srt(StreamType::TsUdp, ""), nullptr));
		CHECK(problemIs(srt(StreamType::TsTcp, ""), nullptr));

		/* a host must be a host: nothing that starts a path, a query or a second address */
		CHECK(problemIs(srt(StreamType::SrtCaller, "10.0.0.5/x"), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtCaller, "10.0.0.5?mode=listener"), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtCaller, "a b"), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtCaller, "user@host"), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtCaller, "srt://10.0.0.5"), "Stream.Problem.Host"));

		StreamForm form = srt(StreamType::SrtCaller, "10.0.0.5");
		form.port = 0;
		CHECK(problemIs(form, "Stream.Problem.Port"));
		form.port = 65536;
		CHECK(problemIs(form, "Stream.Problem.Port"));
		form.port = 65535;
		CHECK(problemIs(form, nullptr));

		form.latencyMs = -1;
		CHECK(problemIs(form, "Stream.Problem.Latency"));
		form.latencyMs = 60001;
		CHECK(problemIs(form, "Stream.Problem.Latency"));
		form.latencyMs = 0;
		CHECK(problemIs(form, nullptr));

		/* SRT wants 10 to 79 characters, or none */
		form.passphrase = "123456789";
		CHECK(problemIs(form, "Stream.Problem.Passphrase"));
		form.passphrase = "1234567890";
		CHECK(problemIs(form, nullptr));
		form.passphrase = std::string(79, 'x');
		CHECK(problemIs(form, nullptr));
		form.passphrase = std::string(80, 'x');
		CHECK(problemIs(form, "Stream.Problem.Passphrase"));

		/* a line break cannot be typed in the form; it must not get through another way */
		form.passphrase = "12345\n67890";
		CHECK(problemIs(form, "Stream.Problem.Text"));
		form.passphrase.clear();
		form.streamId = "a\tb";
		CHECK(problemIs(form, "Stream.Problem.Text"));

		/* latency and passphrase are SRT's; they do not stop another type */
		StreamForm udp = srt(StreamType::TsUdp, "");
		udp.latencyMs = -5;
		udp.passphrase = "x";
		CHECK(problemIs(udp, nullptr));

		CHECK(problemIs(srt(StreamType::RtspTcp, ""), "Stream.Problem.Url"));
		CHECK(problemIs(srt(StreamType::RtspTcp, "rtsp://"), "Stream.Problem.Url"));
		CHECK(problemIs(srt(StreamType::RtspTcp, "http://cam/stream"), "Stream.Problem.Url"));
		CHECK(problemIs(srt(StreamType::RtspTcp, "rtsp://cam/a b"), "Stream.Problem.Url"));
		CHECK(problemIs(srt(StreamType::RtspUdp, "rtsp://cam.local/stream1"), nullptr));
	}

	/* the saved names, there and back */
	{
		const StreamType all[] = {StreamType::SrtCaller, StreamType::SrtListener, StreamType::SrtRendezvous,
					  StreamType::RtspUdp,   StreamType::RtspTcp,     StreamType::TsUdp,
					  StreamType::TsTcp,     StreamType::TsTcpPull};
		for (StreamType type : all) {
			CHECK(streamTypeFromId(streamTypeId(type)) == type);
			CHECK(std::strlen(streamTypeId(type)) > 0);
		}
		CHECK(streamTypeFromId("no such type") == StreamType::SrtCaller);
		CHECK(isSrt(StreamType::SrtListener) && !isSrt(StreamType::TsUdp));
		CHECK(isRtsp(StreamType::RtspUdp) && !isRtsp(StreamType::SrtCaller));
	}

	/* two forms are the same when every field is */
	{
		StreamForm a = srt(StreamType::SrtCaller, "h");
		StreamForm b = a;
		CHECK(a == b);
		b.streamId = "x";
		CHECK(!(a == b));
		b = a;
		b.hardwareDecoder = false;
		CHECK(!(a == b));
	}

	if (failures == 0) {
		std::printf("StreamAddress: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
