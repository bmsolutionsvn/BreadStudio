#include "ProjectFile.hpp"
#include "Project.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <zlib.h>

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

static std::vector<uint8_t> bytes(const std::string &text)
{
	return std::vector<uint8_t>(text.begin(), text.end());
}

static std::string tempFile(const char *name)
{
	return (std::filesystem::temp_directory_path() / name).string();
}

static std::vector<uint8_t> readAll(const std::string &path)
{
	std::FILE *file = std::fopen(path.c_str(), "rb");
	if (file == nullptr) {
		return {};
	}
	std::vector<uint8_t> data;
	uint8_t buffer[4096];
	size_t got = 0;
	while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
		data.insert(data.end(), buffer, buffer + got);
	}
	std::fclose(file);
	return data;
}

static void writeAll(const std::string &path, const std::vector<uint8_t> &data)
{
	std::FILE *file = std::fopen(path.c_str(), "wb");
	if (file == nullptr) {
		return;
	}
	std::fwrite(data.data(), 1, data.size(), file);
	std::fclose(file);
}

static void put16(std::vector<uint8_t> &out, uint16_t value)
{
	out.push_back((uint8_t)(value & 0xff));
	out.push_back((uint8_t)((value >> 8) & 0xff));
}

static void put32(std::vector<uint8_t> &out, uint32_t value)
{
	for (int i = 0; i < 4; i++) {
		out.push_back((uint8_t)((value >> (8 * i)) & 0xff));
	}
}

static void putBytes(std::vector<uint8_t> &out, const std::string &text)
{
	out.insert(out.end(), text.begin(), text.end());
}

static uint16_t get16(const std::vector<uint8_t> &data, size_t at)
{
	return (uint16_t)(data[at] | (data[at + 1] << 8));
}

static uint32_t get32(const std::vector<uint8_t> &data, size_t at)
{
	return (uint32_t)(data[at] | (data[at + 1] << 8) | (data[at + 2] << 16) | ((uint32_t)data[at + 3] << 24));
}

/* Read the produced file as a zip, on the file's own terms: walk the local headers from the
 * start, recompute each stored body's CRC and compare it with the header, and collect the names
 * in order. This is the one check that cannot share a mistake with the writer. */
static std::vector<std::string> zipNames(const std::vector<uint8_t> &raw)
{
	std::vector<std::string> names;
	size_t at = 0;
	while (at + 30 <= raw.size() && get32(raw, at) == 0x04034b50u) {
		const uint32_t crc = get32(raw, at + 14);
		const uint32_t size = get32(raw, at + 18);
		const uint16_t nameLen = get16(raw, at + 26);
		const uint16_t extraLen = get16(raw, at + 28);
		const size_t body = at + 30 + nameLen + extraLen;
		CHECK(body + size <= raw.size());
		if (body + size > raw.size()) {
			return names;
		}
		const uint8_t *first = size == 0 ? nullptr : &raw[body];
		CHECK((uint32_t)crc32(0, (const Bytef *)first, (uInt)size) == crc);
		names.push_back(std::string((const char *)&raw[at + 30], nameLen));
		at = body + size;
	}
	/* the end record closes it and counts the same entries */
	CHECK(raw.size() >= 22 && get32(raw, raw.size() - 22) == 0x06054b50u);
	if (raw.size() >= 22) {
		CHECK(get16(raw, raw.size() - 12) == (uint16_t)names.size());
	}
	return names;
}

/* A zip with a single stored entry, made here rather than by ProjectFile, so the reader is shown
 * a container it did not write - and, when the name is not project.json, one that is not a .bstd. */
static std::string writeOtherZip(const std::string &path, const std::string &name, const std::string &body)
{
	std::vector<uint8_t> out;
	const uint32_t size = (uint32_t)body.size();
	const uint32_t crc = (uint32_t)crc32(0, (const Bytef *)body.data(), (uInt)size);

	put32(out, 0x04034b50);
	put16(out, 20);
	put16(out, 0);
	put16(out, 0);
	put16(out, 0);
	put16(out, 0);
	put32(out, crc);
	put32(out, size);
	put32(out, size);
	put16(out, (uint16_t)name.size());
	put16(out, 0);
	putBytes(out, name);
	putBytes(out, body);

	const uint32_t directoryStart = (uint32_t)out.size();
	put32(out, 0x02014b50);
	put16(out, 20);
	put16(out, 20);
	put16(out, 0);
	put16(out, 0);
	put16(out, 0);
	put16(out, 0);
	put32(out, crc);
	put32(out, size);
	put32(out, size);
	put16(out, (uint16_t)name.size());
	put16(out, 0);
	put16(out, 0);
	put16(out, 0);
	put16(out, 0);
	put32(out, 0);
	put32(out, 0);
	putBytes(out, name);

	const uint32_t directorySize = (uint32_t)out.size() - directoryStart;
	put32(out, 0x06054b50);
	put16(out, 0);
	put16(out, 0);
	put16(out, 1);
	put16(out, 1);
	put32(out, directorySize);
	put32(out, directoryStart);
	put16(out, 0);

	writeAll(path, out);
	return "";
}

/* A zip with stored entries made by hand, so the reader can be shown a hostile container that the
 * writer itself would now refuse. */
static void writeRawZip(const std::string &path, const std::vector<std::pair<std::string, std::string>> &entries)
{
	struct Central {
		uint32_t crc;
		uint32_t size;
		uint32_t offset;
		std::string name;
	};

	std::vector<uint8_t> out;
	std::vector<Central> directory;
	for (const std::pair<std::string, std::string> &entry : entries) {
		const std::string &name = entry.first;
		const std::string &body = entry.second;
		const uint32_t crc = (uint32_t)crc32(0, (const Bytef *)body.data(), (uInt)body.size());
		const uint32_t offset = (uint32_t)out.size();

		put32(out, 0x04034b50);
		put16(out, 20);
		put16(out, 0);
		put16(out, 0);
		put16(out, 0);
		put16(out, 0);
		put32(out, crc);
		put32(out, (uint32_t)body.size());
		put32(out, (uint32_t)body.size());
		put16(out, (uint16_t)name.size());
		put16(out, 0);
		putBytes(out, name);
		putBytes(out, body);

		directory.push_back(Central{crc, (uint32_t)body.size(), offset, name});
	}

	const uint32_t directoryStart = (uint32_t)out.size();
	for (const Central &c : directory) {
		put32(out, 0x02014b50);
		put16(out, 20);
		put16(out, 20);
		put16(out, 0);
		put16(out, 0);
		put16(out, 0);
		put16(out, 0);
		put32(out, c.crc);
		put32(out, c.size);
		put32(out, c.size);
		put16(out, (uint16_t)c.name.size());
		put16(out, 0);
		put16(out, 0);
		put16(out, 0);
		put16(out, 0);
		put32(out, 0);
		put32(out, c.offset);
		putBytes(out, c.name);
	}
	const uint32_t directorySize = (uint32_t)out.size() - directoryStart;

	put32(out, 0x06054b50);
	put16(out, 0);
	put16(out, 0);
	put16(out, (uint16_t)directory.size());
	put16(out, (uint16_t)directory.size());
	put32(out, directorySize);
	put32(out, directoryStart);
	put16(out, 0);
	writeAll(path, out);
}

/* a zip with stored entries, and the manifest, round-trip exactly */
static void testRoundTrip()
{
	const std::string path = tempFile("bread-project-roundtrip.bstd");
	const ProjectManifest manifest{"My Show", "0.1.0", "32.2.2"};
	const std::vector<ProjectEntry> entries = {
		{ManifestPath, bytes(manifestJson(manifest))},
		{"profile/basic.ini", bytes("a=1")},
		{"scenes/Untitled.json", bytes("{}")},
		{"media/empty.bin", {}}, /* a zero-length body */
	};

	CHECK(writeProjectFile(path, entries).empty());

	std::vector<ProjectEntry> back;
	std::string error;
	CHECK(readProjectFile(path, back, error));
	CHECK(error.empty());
	CHECK(back.size() == entries.size());
	if (back.size() == entries.size()) {
		for (size_t i = 0; i < entries.size(); i++) {
			CHECK(back[i].path == entries[i].path);
			CHECK(back[i].bytes == entries[i].bytes);
		}
	}

	/* the bytes are a zip: PK\x03\x04, a good CRC per entry recomputed from the file's own
	 * bytes, the names in the order the writer was given, and an end record closing it */
	const std::vector<uint8_t> raw = readAll(path);
	CHECK(raw.size() > 30);
	if (raw.size() > 30) {
		CHECK(raw[0] == 'P' && raw[1] == 'K' && raw[2] == 3 && raw[3] == 4);
		CHECK(get16(raw, 26) == (uint16_t)std::string(ManifestPath).size());
		CHECK(std::string((const char *)&raw[30], get16(raw, 26)) == ManifestPath);
	}
	const std::vector<std::string> names = zipNames(raw);
	CHECK(names.size() == entries.size());
	if (names.size() == entries.size()) {
		for (size_t i = 0; i < entries.size(); i++) {
			CHECK(names[i] == entries[i].path);
		}
	}
	CHECK(!names.empty() && names[0] == ManifestPath);

	std::filesystem::remove(path);
}

/* the manifest's fields survive */
static void testManifest()
{
	const ProjectManifest manifest{"Show \"A\" \\ B", "0.1.0", "32.2.2"};
	const std::string json = manifestJson(manifest);
	CHECK(!json.empty() && json.front() == '{');

	ProjectManifest back;
	std::string error;
	CHECK(manifestFromJson(json, back, error));
	CHECK(error.empty());
	CHECK(back.project == manifest.project);
	CHECK(back.breadstudio == manifest.breadstudio);
	CHECK(back.obs == manifest.obs);

	/* whitespace between the tokens is accepted */
	ProjectManifest spaced;
	CHECK(manifestFromJson("{ \"project\" : \"p\" ,\n\t\"breadstudio\" : \"b\" ,\n\t\"obs\" : \"o\" }", spaced,
			       error));
	CHECK(spaced.project == "p" && spaced.breadstudio == "b" && spaced.obs == "o");

	/* a manifest missing a field, and one that is not JSON, are refused */
	ProjectManifest bad;
	CHECK(!manifestFromJson("{ \"project\": \"p\" }", bad, error));
	CHECK(!error.empty());
	CHECK(!manifestFromJson("not json at all", bad, error));
	CHECK(!error.empty());

	/* a value that reads like a sibling key's name is still only a value */
	ProjectManifest named{"obs", "breadstudio", "project"};
	ProjectManifest found;
	CHECK(manifestFromJson(manifestJson(named), found, error));
	CHECK(found.project == "obs" && found.breadstudio == "breadstudio" && found.obs == "project");
	CHECK(manifestFromJson("{ \"project\" : \"obs\" , \"breadstudio\" : \"obs\" , \"obs\" : \"obs\" }", found,
			       error));
	CHECK(found.project == "obs" && found.breadstudio == "obs" && found.obs == "obs");
}

/* a file that is not a zip, and one whose manifest is missing, are refused */
static void testRefused()
{
	/* random bytes are not a zip */
	const std::string junk = tempFile("bread-project-junk.bstd");
	writeAll(junk, {0x00, 0xff, 0x12, 0x34, 0xde, 0xad, 0xbe, 0xef});
	std::vector<ProjectEntry> entries;
	std::string error;
	CHECK(!readProjectFile(junk, entries, error));
	CHECK(!error.empty());
	std::filesystem::remove(junk);

	/* a zip with no project.json is not a .bstd */
	const std::string other = tempFile("bread-project-other.bstd");
	CHECK(writeOtherZip(other, "scenes/Untitled.json", "{}").empty());
	entries.clear();
	error.clear();
	CHECK(!readProjectFile(other, entries, error));
	CHECK(!error.empty());
	std::filesystem::remove(other);

	/* a zip the reader can read when it is given a manifest: the other zip round-trips too */
	const std::string good = tempFile("bread-project-good.bstd");
	CHECK(writeOtherZip(good, "project.json", "{}").empty());
	entries.clear();
	error.clear();
	CHECK(readProjectFile(good, entries, error));
	CHECK(entries.size() == 1 && entries[0].path == "project.json" && entries[0].bytes == bytes("{}"));
	std::filesystem::remove(good);
}

/* A .bstd sent from elsewhere is refused whole when any entry name would walk out of the folder it
 * is joined to. The reader is the one place that cannot be forgotten, so the hostile file is built
 * by hand and handed straight to it. */
static void testRejectsUnsafeNames()
{
	const std::vector<std::string> unsafe = {
		"profile/../../evil",    /* walks up out of the profile folder */
		"../evil",               /* walks up out of the container root */
		"/etc/passwd",           /* absolute */
		"C:/evil",               /* a drive */
		"profile\\..\\..\\evil", /* a backslash separator */
		"profile//empty",        /* an empty path element */
		"",                      /* an empty name */
		".. ",                   /* ".." once Windows drops the trailing space */
		"...",                   /* an all-dot component */
		"a./b",                  /* a component ending in a dot */
		"a /b",                  /* a component ending in a space */
	};
	for (const std::string &name : unsafe) {
		const std::string path = tempFile("bread-project-unsafe.bstd");
		writeRawZip(path, {{"project.json", "{}"}, {name, "{}"}});
		std::vector<ProjectEntry> entries;
		std::string error;
		CHECK(!readProjectFile(path, entries, error));
		CHECK(!error.empty());
		std::filesystem::remove(path);
	}

	/* the writer refuses the same names, so the desk cannot make one either */
	const std::string out = tempFile("bread-project-unsafe-out.bstd");
	const std::vector<ProjectEntry> entries = {{ManifestPath, bytes("{}")}, {"profile/../../evil", bytes("x")}};
	CHECK(!writeProjectFile(out, entries).empty());
	std::filesystem::remove(out);

	/* the predicate itself, so a reader that stopped calling it is still caught */
	CHECK(validEntryPath("profile/basic.ini"));
	CHECK(validEntryPath("media/clip final.mp4"));
	CHECK(!validEntryPath(""));
	CHECK(!validEntryPath(".."));
	CHECK(!validEntryPath("a/../b"));
	CHECK(!validEntryPath("/a"));
	CHECK(!validEntryPath("C:/a"));
	CHECK(!validEntryPath("a\\b"));
	CHECK(!validEntryPath("a//b"));
	CHECK(!validEntryPath(".. "));
	CHECK(!validEntryPath("..."));
	CHECK(!validEntryPath("a./b"));
	CHECK(!validEntryPath("a /b"));
	CHECK(!validEntryPath(std::string("a\0b", 3)));
}

/* The Project's pure helpers: a name never walks out of the config root, and the
 * manifest carries the versions the build gave it. */
static void testProjectHelpers()
{
	const std::string root = "/config root";
	const std::string profilePrefix = root + "/obs-studio/basic/profiles/";
	const std::string scenesPrefix = root + "/obs-studio/basic/scenes/";

	/* a name with a path separator, spaces and dots stays one folder under the root */
	const std::string dir = profileDirFor(root, "..\\..\\My Show.v1");
	CHECK(dir.compare(0, profilePrefix.size(), profilePrefix) == 0);
	const std::string dirLeaf = dir.substr(profilePrefix.size());
	CHECK(dirLeaf.find('/') == std::string::npos);
	CHECK(dirLeaf.find('\\') == std::string::npos);
	CHECK(dirLeaf != "." && dirLeaf != "..");

	/* the same for the collection file, which also ends in .json */
	const std::string file = sceneFileFor(root, "../../Sunday Service..");
	CHECK(file.compare(0, scenesPrefix.size(), scenesPrefix) == 0);
	const std::string fileLeaf = file.substr(scenesPrefix.size());
	CHECK(fileLeaf.find('/') == std::string::npos);
	CHECK(fileLeaf.find('\\') == std::string::npos);
	CHECK(fileLeaf.size() > 5 && fileLeaf.compare(fileLeaf.size() - 5, 5, ".json") == 0);

	/* a name of only dots cannot become a parent folder */
	const std::string dots = profileDirFor(root, "..");
	CHECK(dots.compare(0, profilePrefix.size(), profilePrefix) == 0);
	CHECK(dots.size() > profilePrefix.size());
	CHECK(dots.substr(profilePrefix.size()) != "." && dots.substr(profilePrefix.size()) != "..");

	/* a name with a separator is folded into the leaf, not honoured */
	CHECK(profileDirFor(root, "a/b").find("a_b") != std::string::npos);

	const ProjectManifest manifest = manifestFor("My Show");
	CHECK(manifest.project == "My Show");
	CHECK(!manifest.breadstudio.empty());
	CHECK(!manifest.obs.empty());
#ifdef BREADSTUDIO_VERSION
	CHECK(manifest.breadstudio == BREADSTUDIO_VERSION);
#endif
#ifdef OBS_VERSION
	CHECK(manifest.obs == OBS_VERSION);
#endif
}

int main()
{
	testRoundTrip();
	testManifest();
	testRefused();
	testRejectsUnsafeNames();
	testProjectHelpers();

	if (failures != 0) {
		std::printf("ProjectFile: %d checks failed\n", failures);
		std::exit(1);
	}
	std::printf("ProjectFile: all checks passed\n");
	std::exit(0);
}
