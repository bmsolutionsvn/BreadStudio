#pragma once

#include <array>

namespace desk {

/* vMix's default layout (outputs spec §5): Preview and Program across the top half, eight input
 * slots along the bottom half. Slot 0 is Preview, slot 1 is Program, slots 2 to 9 are the inputs. */
inline constexpr int MultiviewSlots = 10;

struct SlotRect {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

/* The ten boxes for a frame that is `width` by `height`. They tile it exactly, whatever the size. */
std::array<SlotRect, MultiviewSlots> multiviewSlots(int width, int height);

} // namespace desk
