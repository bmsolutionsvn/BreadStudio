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
		/* no empty slot; at a tiny frame a row of four can round a slot away, so only insist
		 * on a positive size once the frame is big enough to give every slot a pixel */
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

/* The fitted rectangle: the whole picture, in its own shape, inside the box and centred. */
static void fits(int boxW, int boxH, int pw, int ph, int wantW, int wantH)
{
	const SlotRect box{10, 20, boxW, boxH};
	const SlotRect fit = fitInside(box, pw, ph);

	CHECK(fit.w == wantW && fit.h == wantH);
	/* never outside the box it was fitted into */
	CHECK(fit.x >= box.x && fit.y >= box.y);
	CHECK(fit.x + fit.w <= box.x + box.w);
	CHECK(fit.y + fit.h <= box.y + box.h);
	/* the centre is the box's centre: integer division moves it by at most one pixel, which is
	 * deliberate, so the margin is split as evenly as whole pixels allow */
	const int dx = (fit.x + fit.w / 2) - (box.x + box.w / 2);
	const int dy = (fit.y + fit.h / 2) - (box.y + box.h / 2);
	CHECK(dx >= -1 && dx <= 1);
	CHECK(dy >= -1 && dy <= 1);
}

int main()
{
	/* Preview is the top-left half, Program the top-right half */
	{
		const std::array<SlotRect, MultiviewSlots> slots = multiviewSlots(1920, 1080);
		CHECK(slots[0].x == 0 && slots[0].y == 0 && slots[0].w == 960 && slots[0].h == 540);
		CHECK(slots[1].x == 960 && slots[1].y == 0 && slots[1].w == 960 && slots[1].h == 540);
	}

	/* the eight input slots are the bottom half in two rows of four, read left to right and top
	 * row first: slot 2 is the top-left, slot 5 the top-right, slot 6 the bottom-left */
	{
		const int fw = 1920;
		const int fh = 1080;
		const std::array<SlotRect, MultiviewSlots> slots = multiviewSlots(fw, fh);
		const int colW = 480;
		const int rowH = 270;
		for (int i = 0; i < 8; i++) {
			const SlotRect &s = slots[(size_t)(2 + i)];
			CHECK(s.w == colW);
			CHECK(s.h == rowH);
			CHECK(s.x == (i % 4) * colW);
			CHECK(s.y == 540 + (i / 4) * rowH);
			/* each box is the frame's own shape, which is the whole point of two-by-four */
			CHECK(s.w * fh == s.h * fw);
		}
	}

	/* the point of the change: a 16:9 picture fills a bottom box exactly, where an eight-across
	 * 4:9 box could only ever letterbox it into a quarter of the box */
	{
		const std::array<SlotRect, MultiviewSlots> slots = multiviewSlots(1920, 1080);
		for (int i = 2; i < MultiviewSlots; i++) {
			const SlotRect &box = slots[(size_t)i];
			const SlotRect fit = fitInside(box, 1920, 1080);
			CHECK(fit.x == box.x && fit.y == box.y && fit.w == box.w && fit.h == box.h);
		}
	}

	/* they tile exactly at a few sizes, including odd ones that do not divide evenly */
	covers(1920, 1080);
	covers(1280, 720);
	covers(3840, 2160);
	covers(1000, 575);
	covers(1921, 1081);
	covers(7, 3);

	/* a 16:9 picture in a tall 4:9 box keeps its shape: full width 240, letterboxed to height 135.
	 * (The task's prose example gives full height and width 960 for this pair; that is a picture
	 * larger than its box, i.e. cropped, which is not fitting inside it - the review and the whole
	 * "never stretched" intent are the smaller-of-two-factors fit asserted here.) */
	fits(240, 540, 1920, 1080, 240, 135);

	/* a 4:3 picture in a wide 16:9 box: full height 540, pillarboxed to width 720 */
	fits(960, 540, 640, 480, 720, 540);

	/* a picture whose shape is the box's fills it exactly */
	fits(1920, 1080, 1920, 1080, 1920, 1080);
	fits(960, 720, 640, 480, 960, 720);

	/* no size on either side is an empty rectangle - never a divide by zero or a negative */
	{
		const SlotRect box{10, 20, 240, 540};
		const SlotRect none = fitInside(box, 0, 1080);
		CHECK(none.w == 0 && none.h == 0);
		CHECK(fitInside(box, 1920, 0).w == 0);
		CHECK(fitInside(box, -16, 1080).w == 0);
		CHECK(fitInside(box, 1920, -16).w == 0);
		CHECK(fitInside({10, 20, 0, 540}, 1920, 1080).w == 0);
		CHECK(fitInside({10, 20, 240, 0}, 1920, 1080).w == 0);
		CHECK(fitInside({10, 20, -8, 540}, 1920, 1080).w == 0);
	}

	if (failures == 0) {
		std::printf("MultiviewLayout: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
