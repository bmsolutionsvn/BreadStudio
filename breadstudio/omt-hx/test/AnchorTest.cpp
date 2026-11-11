#include "Anchor.hpp"

#include <cstdio>

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("Anchor: FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

/* 100 ns ticks: a second is 10,000,000, a tenth of one 1,000,000 */
static const int64_t s = 10000000;

int anchorTests()
{
	failures = 0;

	/* the first timestamp maps to now and sets the offset */
	Anchor a;
	CHECK(a.toLocal(0, 1000000000ULL) == 1000000000ULL);
	/* a later one keeps the offset: 0.9 s of sender time is 0.9 s of this PC's */
	CHECK(a.toLocal(9000000, 1100000000ULL) == 1900000000ULL);
	/* 0.9 s into the past keeps it too */
	CHECK(a.toLocal(0, 1900000000ULL) == 1000000000ULL);
	/* exactly a second off keeps it: the rule is "more than a second" */
	CHECK(a.toLocal(0, 2000000000ULL) == 1000000000ULL);
	CHECK(a.toLocal(1 * s, 1000000000ULL) == 2000000000ULL);

	/* a timestamp that maps more than 1 s into the past sets the offset again */
	Anchor b;
	CHECK(b.toLocal(0, 1000000000ULL) == 1000000000ULL);
	CHECK(b.toLocal(0, 3000000000ULL) == 3000000000ULL);
	CHECK(b.toLocal(1000000, 3000000000ULL) == 3100000000ULL);

	/* and one more than 1 s into the future */
	Anchor c;
	CHECK(c.toLocal(0, 1000000000ULL) == 1000000000ULL);
	CHECK(c.toLocal(2 * s, 1500000000ULL) == 1500000000ULL);
	CHECK(c.toLocal(2 * s + 1000000, 1600000000ULL) == 1600000000ULL);

	if (failures == 0) {
		std::printf("Anchor: all checks passed\n");
	}
	return failures;
}