#pragma once

#include <obs-module.h>

namespace desk {

inline constexpr const char *MultiviewSourceId = "bread_multiview";

/* bread_multiview: the desk's own monitoring view (outputs spec §5) — Preview and Program across
 * the top, the inputs below, each with its number, name and a red or green frame by the input
 * bar's rule. Drawn at the main mix's size; an output's canvas scales it. Hidden from every list
 * of sources. */
void registerMultiviewSource();

/* Gather Program, Preview, the inputs and the studio-mode flag, and lay out the labels, for the
 * source the engine holds. UI thread only; the engine calls it from refreshSources(). */
void refreshMultiview(obs_source_t *source);

/* Empty the view: the scene collection it showed is going (the engine's detachSources). */
void clearMultiview(obs_source_t *source);

} // namespace desk
