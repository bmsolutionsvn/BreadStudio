#include "SourceList.hpp"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

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

int main()
{
	/* a source's name is what is in brackets */
	CHECK(sourceName("CAM-PC (Cam 1)") == "Cam 1");
	CHECK(sourceName("CAM-PC (Cam (wide))") == "Cam (wide)");
	CHECK(sourceName("omt://10.0.0.5:6400") == "omt://10.0.0.5:6400");
	CHECK(sourceName("CAM-PC (") == "CAM-PC (");
	CHECK(sourceName("") == "");

	/* grouped by PC, PCs and sources in name order, ignoring case; details kept */
	{
		const std::vector<SenderGroup> groups = groupBySender({
			{"B-PC (z)", "d1"},
			{"a-pc (Y)", "d2"},
			{"B-PC (a)", "d3"},
		});
		CHECK(groups.size() == 2);
		if (groups.size() == 2) {
			CHECK(groups[0].pc == "a-pc");
			CHECK(groups[0].sources.size() == 1);
			CHECK(groups[1].pc == "B-PC");
			CHECK(groups[1].sources.size() == 2);
			if (groups[1].sources.size() == 2) {
				CHECK(groups[1].sources[0].name == "a");
				CHECK(groups[1].sources[0].detail == "d3");
				CHECK(groups[1].sources[0].address == "B-PC (a)");
				CHECK(groups[1].sources[1].name == "z");
			}
		}
	}

	/* the same source name on two PCs: two tiles, under two PCs (spec §10) */
	{
		const std::vector<SenderGroup> groups = groupBySender({{"PC1 (Cam)", ""}, {"PC2 (Cam)", ""}});
		CHECK(groups.size() == 2);
		if (groups.size() == 2) {
			CHECK(groups[0].sources[0].address == "PC1 (Cam)");
			CHECK(groups[1].sources[0].address == "PC2 (Cam)");
		}
	}

	/* an address with no PC in it goes in a group with no name */
	{
		const std::vector<SenderGroup> groups = groupBySender({{"omt://10.0.0.5:6400", ""}});
		CHECK(groups.size() == 1);
		if (groups.size() == 1) {
			CHECK(groups[0].pc.empty());
			CHECK(groups[0].sources[0].name == "omt://10.0.0.5:6400");
		}
	}

	CHECK(groupBySender({}).empty());

	if (failures == 0) {
		std::printf("SourceList: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
