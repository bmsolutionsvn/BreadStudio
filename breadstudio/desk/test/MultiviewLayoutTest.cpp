#include "MultiviewLayout.hpp"

#include <cstdio>

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

/* every slot is inside the frame, they do not overlap, and together they cover every pixel */
static void covers(int width, int height)
{
	const std::array<SlotRect, MultiviewSlots> slots = multiviewSlots(width, height);

	long long area = 0;
	for (const SlotRect &s : slots) {
		CHECK(s.x >= 0 && s.y >= 0);
		CHECK(s.x + s.w <= width);
		CHECK(s.y + s.h <= height);
		/* no empty slot; below width 8 an eight-across row cannot give every input a pixel */
		if (width >= 8 && height >= 2) {
			CHECK(s.w > 0 && s.h > 0);
		}
		area += (long long)s.w * (long long)s.h;
	}
	CHECK(area == (long long)width * (long long)height);

	for (int i = 0; i < MultiviewSlots; i++) {
		for (int j = i + 1; j < MultiviewSlots; j++) {
			const SlotRect &a = slots[(size_t)i];
			const SlotRect &b = slots[(size_t)j];
			const bool apart = a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
			CHECK(apart);
		}
	}
}

int main()
{
	/* Preview is the top-left half, Program the top-right half */
	{
		const std::array<SlotRect, MultiviewSlots> slots = multiviewSlots(1920, 1080);
		CHECK(slots[0].x == 0 && slots[0].y == 0 && slots[0].w == 960 && slots[0].h == 540);
		CHECK(slots[1].x == 960 && slots[1].y == 0 && slots[1].w == 960 && slots[1].h == 540);
	}

	/* the eight input slots run along the bottom, left to right */
	{
		const std::array<SlotRect, MultiviewSlots> slots = multiviewSlots(1920, 1080);
		for (int i = 0; i < 8; i++) {
			CHECK(slots[(size_t)(2 + i)].y == 540);
			CHECK(slots[(size_t)(2 + i)].h == 540);
			CHECK(slots[(size_t)(2 + i)].w == 240);
			CHECK(slots[(size_t)(2 + i)].x == i * 240);
		}
	}

	/* they tile exactly at a few sizes, including odd ones that do not divide evenly */
	covers(1920, 1080);
	covers(1280, 720);
	covers(3840, 2160);
	covers(1000, 575);
	covers(7, 3);

	if (failures == 0) {
		std::printf("MultiviewLayout: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
