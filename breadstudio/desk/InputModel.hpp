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

/* to Preview; with Studio Mode off this goes straight to Program, as OBS does */
void toPreview(obs_source_t *input);

/* to Program as a cut, whatever transition is selected. Ignored while a transition is
 * running or when the input is already on Program. */
void cutToProgram(obs_source_t *input);

/* call on OBS_FRONTEND_EVENT_TRANSITION_STOPPED */
void onTransitionStopped();

/* drop every reference the model holds; safe to call twice */
void clearModel();

} // namespace desk
