#pragma once

#include <string>
#include <utility>
#include <vector>

namespace desk {

/* one source as a plugin lists it */
struct ListedSource {
	std::string address; /* "HOST (name)" as announced, or whatever was typed */
	std::string name;    /* the part in brackets; the whole address when it has none */
	std::string detail;  /* the tile's second line */
};

/* the sources of one sender PC; `pc` is empty for addresses that name none */
struct SenderGroup {
	std::string pc;
	std::vector<ListedSource> sources;
};

/* "HOST (name)" -> "name"; anything else -> itself */
std::string sourceName(const std::string &address);

/* Groups (address, detail) pairs by the PC in the address, as vMix's NDI page does. The groups,
 * and the sources in each, are in name order, ignoring case. */
std::vector<SenderGroup> groupBySender(const std::vector<std::pair<std::string, std::string>> &addressAndDetail);

} // namespace desk
