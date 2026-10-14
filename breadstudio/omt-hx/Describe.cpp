#include "Describe.hpp"

#include <cstdio>

namespace omt {

static const char *Dot = " \xC2\xB7 "; /* " · " */
static const char *Times = "\xC3\x97"; /* "×" */

static const char *codecText(omthx::Codec codec)
{
	return codec == omthx::Codec::H264 ? "H.264" : "HEVC";
}

std::string frameRate(int numerator, int denominator)
{
	if (denominator == 0) {
		return "?";
	}
	if (numerator % denominator == 0) {
		return std::to_string(numerator / denominator);
	}
	char text[16];
	std::snprintf(text, sizeof(text), "%.2f", (double)numerator / (double)denominator);
	return text;
}

std::string formatLine(const omthx::SourceInfo &info)
{
	std::string line = info.mode == omthx::Mode::Full ? std::string("OMT") + Dot + "VMX"
							  : std::string("OMT-HX") + Dot + codecText(info.codec);
	line += Dot + std::to_string(info.format.width) + Times + std::to_string(info.format.height);
	line += Dot + frameRate(info.format.fpsN, info.format.fpsD);
	if (info.format.alpha) {
		line += std::string(Dot) + "alpha";
	}
	return line;
}

std::string receivingLine(const omthx::ReceiverStats &stats, const char *gpu, const char *software)
{
	if (stats.mode == omthx::Mode::Full) {
		return std::string("OMT") + Dot + "VMX";
	}
	/* omthx names a software decoder "hevc (software)" or "h264 (software)" */
	const bool onCpu = stats.decoder.find("(software)") != std::string::npos;
	return std::string("OMT-HX") + Dot + codecText(stats.codec) + Dot + (onCpu ? software : gpu);
}

} // namespace omt
