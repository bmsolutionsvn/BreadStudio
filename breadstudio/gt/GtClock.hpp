#pragma once

#include <string>

namespace gt {

/* The local time a clock formats, split the way a .NET format string reads it (§4.5). */
struct GtClockTime {
	int year, month, day, hour, minute, second, millis, weekday; /* weekday 0 = Sunday */
};

/* The PC's local time (§4.5). */
GtClockTime gtNow();

/* Formats one .NET custom date-and-time format (§4.5). False when it cannot be
 * formatted (a one-letter standard format); the caller then shows the text as
 * written, as vMix does. */
bool gtFormatDate(const std::string &format, const GtClockTime &t, std::string &out);

/* What a title text shows once its clock runs. */
struct GtClockResult {
	std::string text;
	bool ticking = false;   /* a {0:FORMAT} expanded: a title using it redraws once a second */
	bool fractions = false; /* the format shows fractions: it redraws every frame */
};

/* Expands the {0:FORMAT} placeholders in a text value (§4.5). Text it cannot
 * format, and any brace that is not a placeholder or an escape, come back as
 * written (Review Focus 3). */
GtClockResult gtExpandClock(const std::string &text, const GtClockTime &now);

} // namespace gt
