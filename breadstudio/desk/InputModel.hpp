#pragma once

#include <obs.hpp>

#include "LayerGraph.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace desk {

/* every scene, in OBS's scene order, which is input-bar order */
std::vector<OBSSource> inputs();

/* the bottom item of the input's scene: the base. Null for an empty scene. */
OBSSceneItem baseItem(obs_source_t *input);
OBSSource baseSource(obs_source_t *input);

/* false for null */
bool hasAudio(obs_source_t *source);

/* the input's category colour tag, a "#rrggbb" string in the private settings key
 * bread_category; empty for none (inputs spec §2/I4) */
std::string category(obs_source_t *input);
void setCategory(obs_source_t *input, const std::string &hex);

/* the category as a packed ARGB colour, 0 when the input has none: the tint the tile and the
 * MultiView slot draw with (inputs spec §2/I4) */
uint32_t categoryColour(obs_source_t *input);

/* what a click on the input's tile does (inputs spec §3.2); default Preview */
enum class ClickAction { None, Preview, Program };
ClickAction clickAction(obs_source_t *input);
void setClickAction(obs_source_t *input, ClickAction action);

/* when a trigger fires: the input becoming Program or Preview, or a click on its tile */
enum class TriggerWhen { Program, Preview, Click };

/* what the desk can run on a trigger (inputs spec §3.2) */
enum class TriggerAction {
	Nothing,
	StartRecording,
	StopRecording,
	StartStream,
	StopStream,
	SendToPreview,
	SendToProgram
};

/* the input's trigger for `when`, from the private settings key bread_triggers, a JSON object;
 * Nothing when unset (inputs spec §3.2) */
TriggerAction trigger(obs_source_t *input, TriggerWhen when);
void setTrigger(obs_source_t *input, TriggerWhen when, TriggerAction action);

/* runs the input's trigger for `when`. A dispatch that would re-enter the same input and
 * moment is skipped, so an action cannot loop through the scene change it causes. */
void runTriggers(obs_source_t *input, TriggerWhen when);

/* copies the input's desk settings — AFV, the on/off switch, category and click action — from
 * another input; not its source (inputs spec §2/I6) */
void copyDeskSettings(obs_source_t *to, obs_source_t *from);

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

struct Layer {
	int64_t id; /* the scene item's id */
	std::string name;
	bool visible;
};

/* every item above the base, top first */
std::vector<Layer> layers(obs_source_t *input);

/* puts `layerInput` on top of `host` at half size, centred. False when that would make
 * an input contain itself; nothing is added then. */
bool addLayer(obs_source_t *host, obs_source_t *layerInput);

void removeLayer(obs_source_t *host, int64_t id);
void moveLayer(obs_source_t *host, int64_t id, bool up); /* never below the base */
void setLayerVisible(obs_source_t *host, int64_t id, bool visible);

} // namespace desk
