#include "MultiviewLayout.hpp"

namespace desk {

std::array<SlotRect, MultiviewSlots> multiviewSlots(int width, int height)
{
	std::array<SlotRect, MultiviewSlots> slots{};
	const int topH = height / 2;
	const int bottomY = topH;
	const int bottomH = height - topH;
	const int leftW = width / 2;

	/* Preview and Program: the top half, left and right */
	slots[0] = {0, 0, leftW, topH};
	slots[1] = {leftW, 0, width - leftW, topH};

	/* the eight inputs: the bottom half, eight across, so no pixel is lost to rounding */
	for (int i = 0; i < 8; i++) {
		const int x0 = width * i / 8;
		const int x1 = width * (i + 1) / 8;
		slots[(size_t)(2 + i)] = {x0, bottomY, x1 - x0, bottomH};
	}
	return slots;
}

SlotRect fitInside(const SlotRect &box, int pw, int ph)
{
	/* an empty box, or a picture with no size, has nothing to fit */
	if (box.w <= 0 || box.h <= 0 || pw <= 0 || ph <= 0) {
		return {};
	}

	/* the smaller of the two scale factors keeps the whole picture inside, so whichever side
	 * runs out of room first sets the fitted size and the picture is never stretched. The two
	 * factors are compared as cross-products so the whole sum stays in whole pixels. */
	int w;
	int h;
	if ((long long)box.w * ph <= (long long)box.h * pw) {
		w = box.w;
		h = (int)((long long)ph * box.w / pw);
	} else {
		h = box.h;
		w = (int)((long long)pw * box.h / ph);
	}

	/* integer division splits the margin as evenly as whole pixels allow, so the picture's
	 * centre lands at most one pixel from the box's own */
	return {box.x + (box.w - w) / 2, box.y + (box.h - h) / 2, w, h};
}

} // namespace desk
