#include "GtPackage.hpp"

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>

#include <zlib.h>

namespace gt {

namespace {

/* zip records, stored little-endian (the shape ProjectFile.cpp walks): a local header
 * before each entry's bytes, one central header per entry, an end record after the lot */
constexpr uint32_t LocalHeader = 0x04034b50u;   /* PK\x03\x04 */
constexpr uint32_t CentralHeader = 0x02014b50u; /* PK\x01\x02 */
constexpr uint32_t EndOfCentral = 0x06054b50u;  /* PK\x05\x06 */
constexpr size_t LocalHeaderSize = 30;
constexpr size_t CentralHeaderSize = 46;
constexpr size_t EndOfCentralSize = 22;

/* a part this large in a title is a broken or hostile header, not a picture */
constexpr uint32_t MaxPartSize = 256u * 1024u * 1024u;

uint16_t get16(const std::vector<uint8_t> &data, size_t at)
{
	return (uint16_t)(data[at] | (data[at + 1] << 8));
}

uint32_t get32(const std::vector<uint8_t> &data, size_t at)
{
	return (uint32_t)(data[at] | (data[at + 1] << 8) | (data[at + 2] << 16) | ((uint32_t)data[at + 3] << 24));
}

bool isNameChar(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
	       c == '.' || c == ':';
}

/* a leading / names the package root; the rest of the code keys parts without it */
std::string plainName(const std::string &name)
{
	return !name.empty() && name[0] == '/' ? name.substr(1) : name;
}

/* raw inflate: windowBits -15, no zlib wrapper */
bool inflateRaw(const uint8_t *in, size_t inSize, size_t outSize, std::vector<uint8_t> &out)
{
	out.assign(outSize, 0);
	z_stream stream = {};
	if (inflateInit2(&stream, -15) != Z_OK) {
		return false;
	}
	stream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(in));
	stream.avail_in = (uInt)inSize;
	stream.next_out = reinterpret_cast<Bytef *>(out.data());
	stream.avail_out = (uInt)outSize;
	const int result = inflate(&stream, Z_FINISH);
	const size_t produced = (size_t)stream.total_out;
	inflateEnd(&stream);
	if (result != Z_STREAM_END || produced != outSize) {
		out.clear();
		return false;
	}
	return true;
}

/* every entry's bytes, keyed by its part name with a leading / removed */
bool readZip(const std::vector<uint8_t> &data, std::map<std::string, std::vector<uint8_t>> &parts, std::string &error)
{
	if (data.size() < LocalHeaderSize || get32(data, 0) != LocalHeader) {
		error = "not a zip: it has no local header";
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
		error = "not a zip: it has no end record";
		return false;
	}

	const uint16_t count = get16(data, end + 10);
	const uint32_t directorySize = get32(data, end + 12);
	const uint32_t directoryStart = get32(data, end + 16);
	if ((size_t)directoryStart + directorySize > data.size()) {
		error = "the zip directory runs past the end";
		return false;
	}

	size_t at = directoryStart;
	for (uint16_t i = 0; i < count; i++) {
		if (at + CentralHeaderSize > data.size() || get32(data, at) != CentralHeader) {
			error = "a zip entry has no central header";
			return false;
		}
		const uint16_t method = get16(data, at + 10);
		const uint32_t packedSize = get32(data, at + 20); /* compressed size */
		const uint32_t plainSize = get32(data, at + 24);  /* uncompressed size */
		const uint16_t nameLen = get16(data, at + 28);
		const uint16_t extraLen = get16(data, at + 30);
		const uint16_t commentLen = get16(data, at + 32);
		const uint32_t localStart = get32(data, at + 42);
		if (at + CentralHeaderSize + nameLen > data.size()) {
			error = "a zip entry has no central header";
			return false;
		}
		const std::string name((const char *)&data[at + CentralHeaderSize], nameLen);
		at += CentralHeaderSize + nameLen + extraLen + commentLen;

		if (plainSize > MaxPartSize) {
			error = name + " is too large to be a title part";
			return false;
		}
		if ((size_t)localStart + LocalHeaderSize > data.size() || get32(data, localStart) != LocalHeader) {
			error = name + " has no local header";
			return false;
		}
		const uint16_t localNameLen = get16(data, localStart + 26);
		const uint16_t localExtraLen = get16(data, localStart + 28);
		const size_t body = (size_t)localStart + LocalHeaderSize + localNameLen + localExtraLen;
		const size_t packed = method == 0 ? plainSize : packedSize;
		if (body + packed > data.size()) {
			error = name + " runs past the end";
			return false;
		}

		std::vector<uint8_t> bytes;
		if (method == 0) {
			bytes.assign(data.begin() + (std::ptrdiff_t)body, data.begin() + (std::ptrdiff_t)(body + plainSize));
		} else if (method == 8) {
			if (!inflateRaw(&data[body], packedSize, plainSize, bytes)) {
				error = name + " is not readable deflate";
				return false;
			}
		} else {
			error = name + " uses an unsupported compression method";
			return false;
		}
		parts[plainName(name)] = std::move(bytes);
	}
	return true;
}

std::string decodeEntities(const std::string &text)
{
	std::string out;
	out.reserve(text.size());
	for (size_t i = 0; i < text.size();) {
		if (text[i] != '&') {
			out.push_back(text[i++]);
		} else if (text.compare(i, 5, "&amp;") == 0) {
			out.push_back('&');
			i += 5;
		} else if (text.compare(i, 4, "&lt;") == 0) {
			out.push_back('<');
			i += 4;
		} else if (text.compare(i, 4, "&gt;") == 0) {
			out.push_back('>');
			i += 4;
		} else if (text.compare(i, 6, "&quot;") == 0) {
			out.push_back('"');
			i += 6;
		} else if (text.compare(i, 6, "&apos;") == 0) {
			out.push_back('\'');
			i += 6;
		} else {
			out.push_back(text[i++]);
		}
	}
	return out;
}

/* the entity-decoded value of name="..." (or '...') inside the tag [from, to); "" when absent */
std::string attribute(const std::string &text, size_t from, size_t to, const std::string &name)
{
	size_t at = from;
	while ((at = text.find(name, at)) != std::string::npos && at < to) {
		const size_t after = at + name.size();
		const bool wholeWord = at == from || !isNameChar(text[at - 1]);
		size_t eq = after;
		while (eq < to && std::isspace((unsigned char)text[eq]) != 0) {
			eq++;
		}
		if (wholeWord && eq < to && text[eq] == '=') {
			size_t q = eq + 1;
			while (q < to && std::isspace((unsigned char)text[q]) != 0) {
				q++;
			}
			if (q < to && (text[q] == '"' || text[q] == '\'')) {
				const char quote = text[q];
				const size_t close = text.find(quote, q + 1);
				if (close == std::string::npos || close > to) {
					return "";
				}
				return decodeEntities(text.substr(q + 1, close - q - 1));
			}
		}
		at = after;
	}
	return "";
}

/* resources.xml names each picture by a filename and stores it as the part /<source guid>. A
 * resource whose part is absent adds nothing; the missing picture is not a broken title. */
void readResources(const std::vector<uint8_t> &xml, const std::map<std::string, std::vector<uint8_t>> &parts,
		   std::map<std::string, std::vector<uint8_t>> &out)
{
	const std::string text(xml.begin(), xml.end());
	size_t at = 0;
	while ((at = text.find("<resource", at)) != std::string::npos) {
		const size_t tagEnd = text.find('>', at);
		if (tagEnd == std::string::npos) {
			break;
		}
		/* "<resources" also starts with "<resource"; the char after the word decides */
		if (at + 9 < text.size() && isNameChar(text[at + 9])) {
			at = tagEnd + 1;
			continue;
		}
		const std::string filename = attribute(text, at, tagEnd, "filename");

		/* this resource ends at the next <resource, or at </resources */
		size_t limit = text.find("<resource", tagEnd);
		const size_t close = text.find("</resources", tagEnd);
		if (limit == std::string::npos || (close != std::string::npos && close < limit)) {
			limit = close;
		}
		const size_t source = text.find("<source", tagEnd);
		if (!filename.empty() && source != std::string::npos && (limit == std::string::npos || source < limit)) {
			const size_t sourceEnd = text.find('>', source);
			if (sourceEnd != std::string::npos) {
				const std::string guid = plainName(attribute(text, source, sourceEnd, "guid"));
				auto part = parts.find(guid);
				if (!guid.empty() && part != parts.end()) {
					out[filename] = part->second;
				}
			}
		}
		at = tagEnd + 1;
	}
}

} // namespace

bool readGtPackage(const std::vector<uint8_t> &zip, GtPackage &out, std::string &error)
{
	out = GtPackage();
	error.clear();

	std::map<std::string, std::vector<uint8_t>> parts;
	if (!readZip(zip, parts, error)) {
		return false;
	}

	auto document = parts.find("document.xml");
	if (document == parts.end()) {
		error = "the package has no document.xml";
		return false;
	}
	out.documentXml.assign(document->second.begin(), document->second.end());

	auto thumbnail = parts.find("thumbnail.png");
	if (thumbnail != parts.end()) {
		out.thumbnail = thumbnail->second;
	}

	auto resources = parts.find("resources.xml");
	if (resources != parts.end()) {
		readResources(resources->second, parts, out.resources);
	}
	return true;
}

bool readGtFile(const std::string &path, GtPackage &out, std::string &error)
{
	std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
	if (!file) {
		error = "cannot open " + path;
		return false;
	}
	const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return readGtPackage(data, out, error);
}

} // namespace gt
