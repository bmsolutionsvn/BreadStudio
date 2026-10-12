#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace desk {

/* input id -> ids of the inputs it shows as layers */
using LayerGraph = std::unordered_map<std::string, std::vector<std::string>>;

/* true when `to` is `from`, or is shown inside `from` through any depth of layers */
bool reaches(const LayerGraph &graph, const std::string &from, const std::string &to);

/* true when adding `layer` on `host` would make an input contain itself */
inline bool wouldLoop(const LayerGraph &graph, const std::string &host, const std::string &layer)
{
	return reaches(graph, layer, host);
}

} // namespace desk
