#include "LayerGraph.hpp"

#include <unordered_set>

namespace desk {

bool reaches(const LayerGraph &graph, const std::string &from, const std::string &to)
{
	std::unordered_set<std::string> seen;
	std::vector<std::string> todo{from};

	while (!todo.empty()) {
		std::string id = std::move(todo.back());
		todo.pop_back();

		if (id == to) {
			return true;
		}
		if (!seen.insert(id).second) {
			continue;
		}

		auto it = graph.find(id);
		if (it != graph.end()) {
			todo.insert(todo.end(), it->second.begin(), it->second.end());
		}
	}

	return false;
}

} // namespace desk
