#pragma once

#include <obs.hpp>

#include <vector>

namespace desk {

/* every scene, in OBS's scene order, which is input-bar order */
std::vector<OBSSource> inputs();

/* the bottom item of the input's scene: the base. Null for an empty scene. */
OBSSceneItem baseItem(obs_source_t *input);
OBSSource baseSource(obs_source_t *input);

/* false for null */
bool hasAudio(obs_source_t *source);

} // namespace desk
