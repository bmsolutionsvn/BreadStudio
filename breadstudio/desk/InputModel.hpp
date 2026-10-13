#pragma once

#include <obs.hpp>

#include "LayerGraph.hpp"

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

/* every input's uuid -> the uuids of the scenes it holds as items */
LayerGraph graph();

/* A new input with that base, placed last. `unversionedId` is an OBS source type such as
 * "dshow_input". Null when the type does not exist on this machine. */
OBSSource createInput(const char *unversionedId, const char *name, obs_data_t *settings);

/* false when the name is empty or already used by any source */
bool renameInput(obs_source_t *input, const char *name);

enum class DeleteCheck { Ok, OnProgram, LastInput };
DeleteCheck canDelete(obs_source_t *input);

/* how many other inputs hold this one as a layer */
int layerUseCount(obs_source_t *input);

/* removes the input and every layer that shows it. Check canDelete first. */
void deleteInput(obs_source_t *input);

/* Reordering goes through OBS's scene list widget, found by name (spec §6.2).
 * canReorder is false when the widget is not found. */
bool canReorder();
bool moveInput(int from, int to);

} // namespace desk
