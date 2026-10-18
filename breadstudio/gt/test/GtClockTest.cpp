#include "GtClock.hpp"

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

	if (failures == 0) {
		std::printf("GtClock: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
