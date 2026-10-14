#pragma once

#include "omthx/omthx.h"

#include <string>

namespace omt {

/* "60", "59.94"; "?" when the denominator is 0 */
std::string frameRate(int numerator, int denominator);

/* A tile's line on the OMT tab (spec §4.2), e.g. "OMT-HX · HEVC · 1920×1080 · 60". */
std::string formatLine(const omthx::SourceInfo &info);

/* What an input receives now (spec §4.3), e.g. "OMT-HX · HEVC · GPU decode". `gpu` and `software`
 * are the words for the two kinds of decoder. Call only once a picture has arrived. */
std::string receivingLine(const omthx::ReceiverStats &stats, const char *gpu, const char *software);

} // namespace omt
