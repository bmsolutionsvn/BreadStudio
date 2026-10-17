#include "ProjectFile.hpp"

#include <cctype>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <utility>

#include <zlib.h>

namespace desk {

namespace {

/* zip records, stored little-endian: the local header before each entry's bytes, one central
 * header per entry, and the end record after the lot */
constexpr uint32_t LocalHeader = 0x04034b50u;   /* PK\x03\x04 */
constexpr uint32_t CentralHeader = 0x02014b50u; /* PK\x01\x02 */
constexpr uint32_t EndOfCentral = 0x06054b50u;  /* PK\x05\x06 */
constexpr size_t LocalHeaderSize = 30;
constexpr size_t CentralHeaderSize = 46;
constexpr size_t EndOfCentralSize = 22;

void put16(std::vector<uint8_t> &out, uint16_t value)
{
	out.push_back((uint8_t)(value & 0xff));
	out.push_back((uint8_t)((value >> 8) & 0xff));
}

void put32(std::vector<uint8_t> &out, uint32_t value)
{
	for (int i = 0; i < 4; i++) {
		out.push_back((uint8_t)((value >> (8 * i)) & 0xff));
	}
}

void putBytes(std::vector<uint8_t> &out, const void *data, size_t size)
{
	const uint8_t *first = (const uint8_t *)data;
	out.insert(out.end(), first, first + size);
}

uint16_t get16(const std::vector<uint8_t> &data, size_t at)
{
	return (uint16_t)(data[at] | (data[at + 1] << 8));
}

uint32_t get32(const std::vector<uint8_t> &data, size_t at)
{
	return (uint32_t)(data[at] | (data[at + 1] << 8) | (data[at + 2] << 16) | ((uint32_t)data[at + 3] << 24));
}

/* one field of the manifest, as `"key"`'s string value: whitespace around the tokens is
 * accepted, and \" and \\ are read back */
bool readField(const std::string &json, const std::string &key, std::string &value, std::string &error)
{
	const std::string needle = "\"" + key + "\"";
	/* the key, not a value that happens to read like one: the quoted name must be followed by
	 * whitespace and a colon */
	size_t at = std::string::npos;
	for (size_t found = json.find(needle); found != std::string::npos; found = json.find(needle, found + 1)) {
		size_t colon = found + needle.size();
		while (colon < json.size() && std::isspace((unsigned char)json[colon]) != 0) {
			colon++;
		}
		if (colon < json.size() && json[colon] == ':') {
			at = colon + 1;
			break;
		}
	}
	if (at == std::string::npos) {
		error = "the manifest has no " + key;
		return false;
	}
	while (at < json.size() && std::isspace((unsigned char)json[at]) != 0) {
		at++;
	}
	if (at >= json.size() || json[at] != '"') {
		error = "the manifest has no string for " + key;
		return false;
	}
	at++;
	value.clear();
	while (at < json.size()) {
		const char c = json[at++];
		if (c == '\\') {
			if (at >= json.size()) {
				error = "the manifest ends in a backslash";
				return false;
			}
			const char escaped = json[at++];
			if (escaped == '"' || escaped == '\\') {
				value.push_back(escaped);
			} else if (escaped == 'n') {
				value.push_back('\n');
			} else if (escaped == 't') {
				value.push_back('\t');
			} else {
				error = "the manifest has a bad escape";
				return false;
			}
		} else if (c == '"') {
			return true;
		} else {
			value.push_back(c);
		}
	}
	error = "the manifest has an unterminated string";
	return false;
}

void appendEscaped(std::string &out, const std::string &value)
{
	for (char c : value) {
		if (c == '"' || c == '\\') {
			out.push_back('\\');
		}
		out.push_back(c);
	}
}

} // namespace

std::string writeProjectFile(const std::string &path, const std::vector<ProjectEntry> &entries)
{
	bool hasManifest = false;
	for (const ProjectEntry &entry : entries) {
		if (entry.path == ManifestPath) {
			hasManifest = true;
		}
	}
	if (!hasManifest) {
		return std::string("the project has no ") + ManifestPath;
	}

	struct Central {
		uint32_t crc;
		uint32_t size;
		uint32_t offset;
		std::string name;
	};

	std::vector<uint8_t> out;
	std::vector<Central> directory;

	for (const ProjectEntry &entry : entries) {
		/* ponytail: a plain zip is 32-bit; a show packing media over 4 GiB needs zip64 */
		if (entry.bytes.size() > 0xffffffffu || entry.path.size() > 0xffffu || out.size() > 0xffffffffu) {
			return "the entry " + entry.path + " is too large for a .bstd";
		}
		const uint32_t size = (uint32_t)entry.bytes.size();
		const uint32_t offset = (uint32_t)out.size();
		const uint32_t crc = (uint32_t)crc32(0, (const Bytef *)entry.bytes.data(), (uInt)size);

		put32(out, LocalHeader);
		put16(out, 20); /* version needed to extract */
		put16(out, 0);  /* flags */
		put16(out, 0);  /* method: stored */
		put16(out, 0);  /* time */
		put16(out, 0);  /* date */
		put32(out, crc);
		put32(out, size); /* compressed */
		put32(out, size); /* uncompressed */
		put16(out, (uint16_t)entry.path.size());
		put16(out, 0); /* extra */
		putBytes(out, entry.path.data(), entry.path.size());
		putBytes(out, entry.bytes.data(), entry.bytes.size());

		directory.push_back(Central{crc, size, offset, entry.path});
	}

	const uint32_t directoryStart = (uint32_t)out.size();
	for (const Central &c : directory) {
		put32(out, CentralHeader);
		put16(out, 20); /* version made by */
		put16(out, 20); /* version needed */
		put16(out, 0);  /* flags */
		put16(out, 0);  /* method */
		put16(out, 0);  /* time */
		put16(out, 0);  /* date */
		put32(out, c.crc);
		put32(out, c.size);
		put32(out, c.size);
		put16(out, (uint16_t)c.name.size());
		put16(out, 0); /* extra */
		put16(out, 0); /* comment */
		put16(out, 0); /* disk */
		put16(out, 0); /* internal attributes */
		put32(out, 0); /* external attributes */
		put32(out, c.offset);
		putBytes(out, c.name.data(), c.name.size());
	}
	const uint32_t directorySize = (uint32_t)out.size() - directoryStart;

	put32(out, EndOfCentral);
	put16(out, 0); /* this disk */
	put16(out, 0); /* disk with the directory */
	put16(out, (uint16_t)directory.size());
	put16(out, (uint16_t)directory.size());
	put32(out, directorySize);
	put32(out, directoryStart);
	put16(out, 0); /* comment */

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) {
		return "cannot open " + path + " to write";
	}
	file.write((const char *)out.data(), (std::streamsize)out.size());
	if (!file) {
		return "cannot write " + path;
	}
	return "";
}

bool readProjectFile(const std::string &path, std::vector<ProjectEntry> &entries, std::string &error)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		error = "cannot open " + path;
		return false;
	}
	const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	if (data.size() < LocalHeaderSize || get32(data, 0) != LocalHeader) {
		error = path + " is not a zip";
		return false;
	}

	/* the end record is the last thing in the file, before any trailing comment */
	size_t end = SIZE_MAX;
	for (size_t p = data.size() - EndOfCentralSize + 1; p-- > 0;) {
		if (get32(data, p) == EndOfCentral) {
			end = p;
			break;
		}
	}
	if (end == SIZE_MAX) {
		error = path + " has no zip end record";
		return false;
	}

	const uint16_t count = get16(data, end + 10);
	const uint32_t directorySize = get32(data, end + 12);
	const uint32_t directoryStart = get32(data, end + 16);
	if ((size_t)directoryStart + directorySize > data.size()) {
		error = path + " has a broken zip directory";
		return false;
	}

	std::vector<ProjectEntry> found;
	size_t at = directoryStart;
	for (uint16_t i = 0; i < count; i++) {
		if (at + CentralHeaderSize > data.size() || get32(data, at) != CentralHeader) {
			error = path + " has a broken zip entry";
			return false;
		}
		const uint16_t method = get16(data, at + 10);
		const uint32_t size = get32(data, at + 20);
		const uint16_t nameLen = get16(data, at + 28);
		const uint16_t extraLen = get16(data, at + 30);
		const uint16_t commentLen = get16(data, at + 32);
		const uint32_t localStart = get32(data, at + 42);
		if (at + CentralHeaderSize + nameLen > data.size()) {
			error = path + " has a broken zip entry";
			return false;
		}
		const std::string name((const char *)&data[at + CentralHeaderSize], nameLen);
		at += CentralHeaderSize + nameLen + extraLen + commentLen;
		if (method != 0) {
			error = path + ": " + name + " is compressed, not stored";
			return false;
		}
		if ((size_t)localStart + LocalHeaderSize > data.size() || get32(data, localStart) != LocalHeader) {
			error = path + ": " + name + " has no zip header";
			return false;
		}
		const uint16_t localNameLen = get16(data, localStart + 26);
		const uint16_t localExtraLen = get16(data, localStart + 28);
		const size_t body = (size_t)localStart + LocalHeaderSize + localNameLen + localExtraLen;
		if (body + size > data.size()) {
			error = path + ": " + name + " runs past the end";
			return false;
		}
		ProjectEntry entry;
		entry.path = name;
		entry.bytes.assign(data.begin() + (std::ptrdiff_t)body, data.begin() + (std::ptrdiff_t)(body + size));
		found.push_back(std::move(entry));
	}

	bool hasManifest = false;
	for (const ProjectEntry &entry : found) {
		if (entry.path == ManifestPath) {
			hasManifest = true;
		}
	}
	if (!hasManifest) {
		error = path + " has no " + ManifestPath;
		return false;
	}

	entries = std::move(found);
	error.clear();
	return true;
}

std::string manifestJson(const ProjectManifest &manifest)
{
	std::string out = "{\n";
	out += "\t\"project\": \"";
	appendEscaped(out, manifest.project);
	out += "\",\n\t\"breadstudio\": \"";
	appendEscaped(out, manifest.breadstudio);
	out += "\",\n\t\"obs\": \"";
	appendEscaped(out, manifest.obs);
	out += "\"\n}\n";
	return out;
}

bool manifestFromJson(const std::string &json, ProjectManifest &manifest, std::string &error)
{
	ProjectManifest parsed;
	if (!readField(json, "project", parsed.project, error)) {
		return false;
	}
	if (!readField(json, "breadstudio", parsed.breadstudio, error)) {
		return false;
	}
	if (!readField(json, "obs", parsed.obs, error)) {
		return false;
	}
	manifest = parsed;
	error.clear();
	return true;
}

} // namespace desk
