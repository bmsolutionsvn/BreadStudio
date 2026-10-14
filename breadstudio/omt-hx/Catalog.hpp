#pragma once

#include <string>
#include <vector>

namespace omt {

/* one announced source, with its tile line (spec §4.2) */
struct Seen {
	std::string address;
	std::string detail;
};

/* Every OMT source announced on the network, in libomt's order. Asking starts the format check
 * for new ones; the check runs only while something has asked within the last five seconds, and
 * starts again from nothing after that, so a codec changed between shows is seen (spec §6.1).
 * Any thread. */
std::vector<Seen> seen();

/* stops the check; from obs_module_unload */
void stopCatalog();

} // namespace omt
