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

} // namespace desk
