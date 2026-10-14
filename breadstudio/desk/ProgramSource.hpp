#pragma once

namespace desk {

inline constexpr const char *ProgramSourceId = "bread_program";

/* bread_program: the main mix's finished picture, for an output set to Program (outputs spec §4.1).
 * Hidden from every list of sources. */
void registerProgramSource();

} // namespace desk
