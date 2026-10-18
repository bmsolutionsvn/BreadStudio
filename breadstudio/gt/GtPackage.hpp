#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace gt {

/* A vMix GT title package: the zip's document, its picture parts and its thumbnail. */
struct GtPackage {
	std::string documentXml;                               /* /document.xml, kept as UTF-8 (design §3.1) */
	std::map<std::string, std::vector<uint8_t>> resources; /* resource filename -> first source's bytes */
	std::vector<uint8_t> thumbnail;                        /* empty when absent */
};

/* Reads a .gtzip's bytes. A missing /document.xml is an error; a missing picture is not. */
bool readGtPackage(const std::vector<uint8_t> &zip, GtPackage &out, std::string &error);

/* Reads a .gtzip from disk; the path is UTF-8. */
bool readGtFile(const std::string &path, GtPackage &out, std::string &error);

} // namespace gt
