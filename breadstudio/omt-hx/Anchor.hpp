#pragma once

#include <cstdint>
#include <cstdlib> /* llabs */

// Maps a sender's timestamps (100 ns units) to this PC's clock (ns). The first one sets the offset; one that
// lands more than a second from now sets it again (the sender restarted, or its clock drifted that far).
struct Anchor {
	uint64_t toLocal(int64_t omt, uint64_t nowNs);

private:
	bool set = false;
	int64_t offset = 0; /* local ns - sender ns */
};

inline uint64_t Anchor::toLocal(int64_t omt, uint64_t nowNs)
{
	const int64_t local = offset + omt * 100;
	/* the sender restarted, or its clock drifted that far: trust this frame instead */
	if (!set || std::llabs(local - (int64_t)nowNs) > 1000000000) {
		offset = (int64_t)nowNs - omt * 100;
		set = true;
		return nowNs;
	}
	return (uint64_t)local;
}