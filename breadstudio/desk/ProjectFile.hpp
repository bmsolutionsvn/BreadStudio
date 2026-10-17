#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace desk {

/* One entry of a .bstd container: a path inside it, and the bytes at that path. */
struct ProjectEntry {
	std::string path;
	std::vector<uint8_t> bytes;
};

/* The manifest a Project carries, from its project.json. */
struct ProjectManifest {
	std::string project;
	std::string breadstudio;
	std::string obs;
};

/* Writes `entries` as a .bstd at `path`: a zip whose entries are stored, so a show
 * is text and already-compressed media and nothing is bought by deflating it. The
 * manifest must be one of the entries, under the name ManifestPath. An empty string
 * is returned on success, and the reason otherwise. */
std::string writeProjectFile(const std::string &path, const std::vector<ProjectEntry> &entries);

/* Reads a .bstd back. False, with `error` set, when the file is not one. */
bool readProjectFile(const std::string &path, std::vector<ProjectEntry> &entries, std::string &error);

/* The manifest's fields as a project.json. */
std::string manifestJson(const ProjectManifest &manifest);
bool manifestFromJson(const std::string &json, ProjectManifest &manifest, std::string &error);

/* the name project.json is stored under */
inline constexpr const char *ManifestPath = "project.json";

/* True when `path` is safe as a container entry: a relative path of non-empty
 * segments, none of them "." or "..", with '/' as the only separator and no drive.
 * The writer refuses an entry that is not, and the reader refuses a container that
 * holds one, so unpacking a received .bstd (P6 is "send the file") can never be
 * handed a name that walks out of the folder it is joined to. */
bool validEntryPath(const std::string &path);

} // namespace desk
