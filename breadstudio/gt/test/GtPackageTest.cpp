#include "GtPackage.hpp"

#include <cstdio>
#include <string>

using namespace gt;

/* The fixtures sit beside this file; the build passes their directory in, and a
 * run with no argument falls back to the one CMake sets at configure time. */
#ifndef GT_FIXTURE_DIR
#define GT_FIXTURE_DIR "test/fixtures"
#endif

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

int main(int argc, char **argv)
{
	const std::string dir = argc > 1 ? argv[1] : GT_FIXTURE_DIR;
	std::string e;

	/* A stored package: the document, its one picture and its thumbnail come back. */
	GtPackage p;
	CHECK(readGtFile(dir + "/minimal.gtzip", p, e));
	CHECK(p.documentXml.find("Nguyễn") != std::string::npos); /* UTF-8 kept despite the utf-16 declaration */
	CHECK(p.resources.count("abc\\BG.png") == 1);
	CHECK(!p.thumbnail.empty());

	/* The same package, deflate-compressed: a valid title, not "not a GT title" (Review Focus 1). */
	GtPackage d;
	CHECK(readGtFile(dir + "/deflated.gtzip", d, e));
	CHECK(d.documentXml == p.documentXml && d.resources == p.resources);

	/* A resource whose part is absent: the title reads, the picture is just missing. */
	GtPackage m;
	CHECK(readGtFile(dir + "/missing-part.gtzip", m, e));
	CHECK(m.resources.empty());

	/* Not a zip at all, and a file that is not there: both fail with a reason. */
	GtPackage n;
	CHECK(!readGtFile(dir + "/not-a-zip.gtzip", n, e));
	CHECK(!e.empty());
	GtPackage x;
	CHECK(!readGtFile(dir + "/absent.gtzip", x, e));

	if (failures == 0) {
		std::printf("GtPackage: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
