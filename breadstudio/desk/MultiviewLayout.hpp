#pragma once

#include <array>

namespace desk {

/* vMix's "Preview / Output + 8 Inputs": Preview and Program across the top half, the eight
 * inputs along the bottom in two rows of four, so each box has the frame's own shape and a
 * picture of that shape fills it. Slot 0 is Preview, slot 1 is Program, slots 2 to 9 are the
 * inputs, read left to right and top row first. */
inline constexpr int MultiviewSlots = 10;

struct SlotRect {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

/* The ten boxes for a frame that is `width` by `height`. They tile it exactly, whatever the size. */
std::array<SlotRect, MultiviewSlots> multiviewSlots(int width, int height);

/* The rectangle a `pw` by `ph` picture fills inside `box`, keeping its own aspect and centred -
 * the whole picture, with letterbox or pillarbox margins, never stretched. An empty rectangle
 * when the box or the picture has no size. */
SlotRect fitInside(const SlotRect &box, int pw, int ph);

} // namespace desk
