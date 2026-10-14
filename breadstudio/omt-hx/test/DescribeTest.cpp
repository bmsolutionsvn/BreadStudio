#include "Describe.hpp"

#include <cstdio>
#include <string>

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

#define DOT " \xC2\xB7 "
#define TIMES "\xC3\x97"

static omthx::SourceInfo info(omthx::Mode mode, omthx::Codec codec, int w, int h, int n, int d, bool alpha)
{
	omthx::SourceInfo result;
	result.mode = mode;
	result.codec = codec;
	result.format = omthx::VideoFormat{w, h, n, d, alpha};
	return result;
}

int main()
{
	using omthx::Codec;
	using omthx::Mode;

	/* frame rates as people say them */
	CHECK(omt::frameRate(60, 1) == "60");
	CHECK(omt::frameRate(50, 1) == "50");
	CHECK(omt::frameRate(60000, 1001) == "59.94");
	CHECK(omt::frameRate(30000, 1001) == "29.97");
	CHECK(omt::frameRate(25, 0) == "?");

	/* the three formats (spec §4.2) */
	CHECK(omt::formatLine(info(Mode::Full, Codec::Hevc, 1920, 1080, 50, 1, false)) == "OMT" DOT "VMX" DOT
											  "1920" TIMES "1080" DOT "50");
	CHECK(omt::formatLine(info(Mode::Hx, Codec::Hevc, 1920, 1080, 60, 1, false)) == "OMT-HX" DOT "HEVC" DOT
											"1920" TIMES "1080" DOT "60");
	CHECK(omt::formatLine(info(Mode::Hx, Codec::H264, 1280, 720, 60000, 1001, true)) ==
	      "OMT-HX" DOT "H.264" DOT "1280" TIMES "720" DOT "59.94" DOT "alpha");

	/* what an input receives now (spec §4.3) */
	omthx::ReceiverStats stats;
	stats.mode = Mode::Hx;
	stats.codec = Codec::Hevc;
	stats.decoder = "hevc (d3d11va)";
	CHECK(omt::receivingLine(stats, "GPU decode", "software decode") == "OMT-HX" DOT "HEVC" DOT "GPU decode");
	stats.codec = Codec::H264;
	stats.decoder = "h264 (software)";
	CHECK(omt::receivingLine(stats, "GPU decode", "software decode") == "OMT-HX" DOT "H.264" DOT "software decode");
	stats.mode = Mode::Full;
	stats.decoder = "vmx";
	CHECK(omt::receivingLine(stats, "GPU decode", "software decode") == "OMT" DOT "VMX");

	if (failures == 0) {
		std::printf("Describe: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
