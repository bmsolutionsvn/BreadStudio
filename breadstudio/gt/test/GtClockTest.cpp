#include "GtClock.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

using namespace gt;

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

int main()
{
	/* A fixed local time (weekday 1 = Monday), so no case depends on the clock. */
	const GtClockTime t{2026, 10, 19, 21, 5, 9, 42, 1};

	/* The clock formats of §4.5, and whether a title with them must redraw. */
	CHECK(gtExpandClock("{0:HH:mm:ss}", t).text == "21:05:09" && gtExpandClock("{0:HH:mm:ss}", t).ticking);
	CHECK(gtExpandClock("{0:hh:mm:ss tt}", t).text == "09:05:09 PM");
	CHECK(gtExpandClock("{0:H:m:s}", t).text == "21:5:9");
	CHECK(gtExpandClock("{0:dddd d MMMM yyyy}", t).text == "Monday 19 October 2026");
	CHECK(gtExpandClock("{0:ss.fff}", t).text == "09.042" && gtExpandClock("{0:ss.fff}", t).fractions);
	CHECK(gtExpandClock("Kick-off {0:HH:mm}", t).text == "Kick-off 21:05");
	CHECK(gtExpandClock("{0:'at' HH}", t).text == "at 21");
	CHECK(gtExpandClock("{0:HH\\h}", t).text == "21h");

	/* Review Focus 3: braces in ordinary text are shown as written (never throw or blank). */
	CHECK(gtExpandClock("{TEAM}", t).text == "{TEAM}" && !gtExpandClock("{TEAM}", t).ticking);
	CHECK(gtExpandClock("a { b", t).text == "a { b");
	CHECK(gtExpandClock("{{0}}", t).text == "{0}");
	CHECK(gtExpandClock("{0:H}", t).text == "{0:H}"); /* one-letter = .NET standard format: unsupported, as written */
	CHECK(gtExpandClock("player name", t).text == "player name");

	/* Countdowns (§4.5). The plan's hypothesis: it counts down from Duration and holds at
	 * zero; Reverse counts up from Start to Duration; ReverseDisplay shows the opposite of
	 * the counting direction's own reading; Stop holds the reading, Reset clears it. The
	 * on-screen behaviour is owner-verified, not confirmed here. */
	GtCountdown c({"00:00:00", "00:30:00", "mm:ss", false, false});
	CHECK(c.active() && !c.running() && c.text(0) == "30:00");
	c.start(1000);
	CHECK(c.text(1000 + 159000) == "27:21"); /* CACAS24's frozen reading after 2:39 */
	c.pause(200000);
	CHECK(c.text(999999) == c.text(200000));
	c.start(300000);
	CHECK(c.text(300000 + 3600000) == "00:00"); /* holds at zero */
	c.reset();
	CHECK(!c.running() && c.text(0) == "30:00");
	CHECK(!GtCountdown({"00:00:00", "00:00:00", "mm:ss", false, false}).active());

	/* Reverse: counts up from Start to Duration, holding at Duration. */
	GtCountdown r({"00:00:00", "00:30:00", "mm:ss", true, false});
	CHECK(r.active() && !r.running() && r.text(0) == "00:00");
	r.start(1000);
	CHECK(r.text(1000 + 159000) == "02:39");
	CHECK(r.text(1000 + 3600000) == "30:00");

	/* ReverseDisplay: counts down but shows the elapsed reading. */
	GtCountdown d({"00:00:00", "00:30:00", "mm:ss", false, true});
	CHECK(d.active() && !d.running() && d.text(0) == "00:00");
	d.start(1000);
	CHECK(d.text(1000 + 159000) == "02:39");
	CHECK(d.text(1000 + 3600000) == "30:00");

	/* Stop: holds the reading; a later start begins a fresh countdown. */
	GtCountdown s({"00:00:00", "00:30:00", "mm:ss", false, false});
	s.start(1000);
	s.stop(1000 + 159000);
	CHECK(!s.running() && s.text(999999) == "27:21");
	s.start(2000000);
	CHECK(s.text(2000000) == "30:00");

	/* Span parsing: hh:mm:ss, hh:mm:ss.fff and mm:ss. */
	int64_t ms = 0;
	CHECK(gtTimeSpanMs("00:00:59", ms) && ms == 59000);
	CHECK(gtTimeSpanMs("30:00", ms) && ms == 1800000);
	CHECK(gtTimeSpanMs("00:30:00", ms) && ms == 1800000);
	CHECK(gtTimeSpanMs("01:02:03.250", ms) && ms == 3723250);
	CHECK(!gtTimeSpanMs("soon", ms));
	CHECK(!gtTimeSpanMs("", ms));
	CHECK(!gtTimeSpanMs("1:2:3:4", ms));

	if (failures == 0) {
		std::printf("GtClock: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
