#include "SourceList.hpp"

#include <algorithm>
#include <cctype>

namespace desk {

/* "HOST (name)": the PC is before the first " (", the name runs to the last ")" */
static bool split(const std::string &address, std::string &pc, std::string &name)
{
	const size_t open = address.find(" (");
	if (open == std::string::npos || open == 0 || address.size() < open + 3 || address.back() != ')') {
		return false;
	}
	pc = address.substr(0, open);
	name = address.substr(open + 2, address.size() - open - 3);
	return true;
}

static std::string lower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return text;
}

std::string sourceName(const std::string &address)
{
	std::string pc;
	std::string name;
	return split(address, pc, name) ? name : address;
}

std::vector<SenderGroup> groupBySender(const std::vector<std::pair<std::string, std::string>> &addressAndDetail)
{
	std::vector<SenderGroup> groups;
	for (const auto &[address, detail] : addressAndDetail) {
		std::string pc;
		std::string name;
		if (!split(address, pc, name)) {
			pc.clear();
			name = address;
		}

		auto group = std::find_if(groups.begin(), groups.end(),
					  [&pc](const SenderGroup &each) { return each.pc == pc; });
		if (group == groups.end()) {
			groups.push_back({pc, {}});
			group = groups.end() - 1;
		}
		group->sources.push_back({address, name, detail});
	}

	std::sort(groups.begin(), groups.end(),
		  [](const SenderGroup &a, const SenderGroup &b) { return lower(a.pc) < lower(b.pc); });
	for (SenderGroup &group : groups) {
		std::sort(group.sources.begin(), group.sources.end(),
			  [](const ListedSource &a, const ListedSource &b) { return lower(a.name) < lower(b.name); });
	}
	return groups;
}

} // namespace desk
