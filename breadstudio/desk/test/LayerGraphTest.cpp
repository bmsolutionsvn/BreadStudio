#include "LayerGraph.hpp"

#include <cstdio>

using desk::LayerGraph;
using desk::reaches;
using desk::wouldLoop;

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
	/* a layer on itself */
	{
		LayerGraph g;
		CHECK(wouldLoop(g, "a", "a"));
	}

	/* a loop through two inputs: b shows a, so a must not take b */
	{
		LayerGraph g{{"b", {"a"}}};
		CHECK(wouldLoop(g, "a", "b"));
		CHECK(!wouldLoop(g, "b", "a")); /* a second copy of a on b is not a loop */
	}

	/* a loop through three inputs: c shows b, b shows a */
	{
		LayerGraph g{{"c", {"b"}}, {"b", {"a"}}};
		CHECK(wouldLoop(g, "a", "c"));
		CHECK(wouldLoop(g, "b", "c"));
		CHECK(!wouldLoop(g, "c", "a"));
	}

	/* a deep chain that is not a loop: a shows b shows c shows d */
	{
		LayerGraph g{{"a", {"b"}}, {"b", {"c"}}, {"c", {"d"}}};
		CHECK(!wouldLoop(g, "d", "e")); /* a new input at the end */
		CHECK(!wouldLoop(g, "a", "d")); /* d shown twice inside a, by two routes */
		CHECK(wouldLoop(g, "d", "a"));  /* closing the chain */
	}

	/* a graph that already holds a loop still gives an answer */
	{
		LayerGraph g{{"a", {"b"}}, {"b", {"a"}}};
		CHECK(!reaches(g, "a", "z"));
	}

	/* "is this input on Program": Program is c, which shows b, which shows a */
	{
		LayerGraph g{{"c", {"b"}}, {"b", {"a"}}};
		CHECK(reaches(g, "c", "c"));
		CHECK(reaches(g, "c", "a"));
		CHECK(!reaches(g, "a", "c"));
		CHECK(!reaches(g, "c", "unknown"));
	}

	if (failures == 0) {
		std::printf("LayerGraph: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
