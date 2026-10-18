#pragma once

#include <cstdint>
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

/* A per-field countdown's settings, as a .vmix preset stores them (§4.5). */
struct GtCountdownSettings {
	std::string start = "00:00:00", duration = "00:00:00", format = "mm:ss";
	bool reverse = false, reverseDisplay = false;
};

/* Parses "hh:mm:ss", "hh:mm:ss.fff" or "mm:ss" into milliseconds. False when
 * the text is not a span. */
bool gtTimeSpanMs(const std::string &text, int64_t &ms);

/* A field's countdown (§4.5). Every method that reads the clock takes nowMs, so
 * a test and the source both read the same time and nothing hides a clock. */
class GtCountdown {
public:
	explicit GtCountdown(const GtCountdownSettings &settings);

	bool active() const; /* duration > 0; a 00:00:00 countdown is none (§6) */
	void start(int64_t nowMs);
	void pause(int64_t nowMs);
	void stop(int64_t nowMs);
	void reset();
	bool running() const;

	/* What the field shows, always; it replaces the text value (§4.5). */
	std::string text(int64_t nowMs) const;

private:
	enum class State { Idle, Running, Paused, Stopped };
	int64_t elapsedAt(int64_t nowMs) const;

	GtCountdownSettings settings_;
	int64_t startMs_ = 0;
	int64_t durationMs_ = 0;
	State state_ = State::Idle;
	int64_t startedAtMs_ = 0;
	int64_t elapsedMs_ = 0; /* accumulated while running, frozen while paused or stopped */
};

} // namespace gt
