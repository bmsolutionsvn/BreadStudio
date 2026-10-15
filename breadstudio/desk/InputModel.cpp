#include "InputModel.hpp"

#include "AudioBed.hpp"
#include "OutputEngine.hpp"

#include <obs-frontend-api.h>

#include <graphics/vec2.h>

#include <QListWidget>
#include <QMainWindow>
#include <QSignalBlocker>
#include <QTimer>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace desk {

std::vector<OBSSource> inputs()
{
	std::vector<OBSSource> result;

	obs_frontend_source_list list = {};
	obs_frontend_get_scenes(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		result.emplace_back(list.sources.array[i]);
	}
	obs_frontend_source_list_free(&list);

	return result;
}

OBSSceneItem baseItem(obs_source_t *input)
{
	OBSSceneItem first;
	obs_scene_t *scene = obs_scene_from_source(input);
	if (!scene) {
		return first;
	}

	/* items are enumerated bottom first; stop at the first */
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			*static_cast<OBSSceneItem *>(param) = item;
			return false;
		},
		&first);

	return first;
}

OBSSource baseSource(obs_source_t *input)
{
	OBSSceneItem item = baseItem(input);
	return item ? OBSSource(obs_sceneitem_get_source(item)) : OBSSource();
}

bool hasAudio(obs_source_t *source)
{
	return source && (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0;
}

/* the desk's own per-input values, kept in the input's private settings as AudioBed keeps
 * bread_afv (spec §4) */
static const char *CategoryKey = "bread_category";
static const char *ClickKey = "bread_click";
static const char *TriggersKey = "bread_triggers";

std::string category(obs_source_t *input)
{
	if (!input) {
		return {};
	}
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	const char *value = obs_data_get_string(settings, CategoryKey);
	return value ? value : "";
}

void setCategory(obs_source_t *input, const std::string &hex)
{
	if (!input) {
		return;
	}
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	obs_data_set_string(settings, CategoryKey, hex.c_str());
	obs_frontend_save();
}

uint32_t categoryColour(obs_source_t *input)
{
	const std::string hex = category(input);
	if (hex.size() != 7 || hex[0] != '#') {
		return 0;
	}
	char *end = nullptr;
	const unsigned long value = strtoul(hex.c_str() + 1, &end, 16);
	if (end != hex.c_str() + 7) {
		return 0;
	}
	return 0xFF000000u | (uint32_t)(value & 0xFFFFFFu);
}

ClickAction clickAction(obs_source_t *input)
{
	if (!input) {
		return ClickAction::Preview;
	}
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	if (obs_data_has_user_value(settings, ClickKey)) {
		const int value = (int)obs_data_get_int(settings, ClickKey);
		if (value >= (int)ClickAction::None && value <= (int)ClickAction::Program) {
			return (ClickAction)value;
		}
	}
	return ClickAction::Preview;
}

void setClickAction(obs_source_t *input, ClickAction action)
{
	if (!input) {
		return;
	}
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	obs_data_set_int(settings, ClickKey, (int)action);
	obs_frontend_save();
}

/* the key of `when` inside the bread_triggers object */
static const char *triggerKey(TriggerWhen when)
{
	switch (when) {
	case TriggerWhen::Program:
		return "program";
	case TriggerWhen::Preview:
		return "preview";
	case TriggerWhen::Click:
		return "click";
	}
	return "program";
}

TriggerAction trigger(obs_source_t *input, TriggerWhen when)
{
	if (!input) {
		return TriggerAction::Nothing;
	}
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	OBSDataAutoRelease triggers = obs_data_get_obj(settings, TriggersKey);
	if (!triggers) {
		return TriggerAction::Nothing;
	}
	const int value = (int)obs_data_get_int(triggers, triggerKey(when));
	if (value < (int)TriggerAction::Nothing || value > (int)TriggerAction::SendToProgram) {
		return TriggerAction::Nothing;
	}
	return (TriggerAction)value;
}

void setTrigger(obs_source_t *input, TriggerWhen when, TriggerAction action)
{
	if (!input) {
		return;
	}
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	OBSDataAutoRelease triggers = obs_data_get_obj(settings, TriggersKey);
	if (!triggers) {
		triggers = obs_data_create();
	}
	obs_data_set_int(triggers, triggerKey(when), (int)action);
	obs_data_set_obj(settings, TriggersKey, triggers);
	obs_frontend_save();
}

void copyDeskSettings(obs_source_t *to, obs_source_t *from)
{
	if (!to || !from || to == from) {
		return;
	}
	bed().setAfv(to, AudioBed::afv(from));
	bed().setAudioOn(to, AudioBed::audioOn(from));
	setCategory(to, category(from));
	setClickAction(to, clickAction(from));
}

/* A tile's Cut is done with the frontend API alone: select the Cut transition, switch,
 * and put back the operator's transition and Preview when the cut has finished. */
static OBSSource restoreTransition;
static OBSSource restorePreview;
static bool restorePending = false;
static int restoreTries = 0;

void toPreview(obs_source_t *input)
{
	if (obs_frontend_preview_program_mode_active()) {
		obs_frontend_set_current_preview_scene(input);
	} else {
		obs_frontend_set_current_scene(input);
	}
}

static OBSSource cutTransition()
{
	OBSSource cut;

	obs_frontend_source_list list = {};
	obs_frontend_get_transitions(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		if (strcmp(obs_source_get_unversioned_id(list.sources.array[i]), "cut_transition") == 0) {
			cut = list.sources.array[i];
			break;
		}
	}
	obs_frontend_source_list_free(&list);

	return cut;
}

void cutToProgram(obs_source_t *input)
{
	if (!obs_frontend_preview_program_mode_active()) {
		obs_frontend_set_current_scene(input);
		return;
	}
	if (restorePending) {
		return;
	}

	/* OBS does nothing when asked for the scene already on Program, and then no
	 * transition ever stops, so nothing would be restored */
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	if (program.Get() == input) {
		return;
	}

	OBSSourceAutoRelease current = obs_frontend_get_current_transition();
	if (!current) {
		return;
	}
	if (obs_transition_is_active(current)) {
		return; /* a transition is running; OBS ignores a new one, and a change of transition */
	}

	OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
	restorePreview = preview.Get();

	OBSSource cut = cutTransition();
	if (cut && cut.Get() != current.Get()) {
		restoreTransition = current.Get();
		obs_frontend_set_current_transition(cut);
	}

	restorePending = true;
	restoreTries = 0;
	obs_frontend_set_current_scene(input);

	/* in case the stop event never comes */
	QTimer::singleShot(500, [] { onTransitionStopped(); });
}

void onTransitionStopped()
{
	if (!restorePending) {
		return;
	}

	/* OBS raises "stopped" when the picture has switched, but refuses a change of
	 * transition until the audio side has stopped too, a moment later. Wait for it, for
	 * two seconds at most. */
	OBSSourceAutoRelease running = obs_frontend_get_current_transition();
	if (running && obs_transition_is_active(running) && restoreTries++ < 100) {
		QTimer::singleShot(20, [] { onTransitionStopped(); });
		return;
	}
	restorePending = false;

	OBSSource transition = restoreTransition;
	OBSSource preview = restorePreview;
	restoreTransition = nullptr;
	restorePreview = nullptr;

	if (transition) {
		obs_frontend_set_current_transition(transition);
	}
	/* OBS's "swap" option has just put the old Program on Preview; a tile's Cut
	 * leaves Preview as it was */
	if (preview && obs_frontend_preview_program_mode_active()) {
		obs_frontend_set_current_preview_scene(preview);
	}
}

/* the input and moment whose trigger is running, so a trigger cannot re-enter itself when its
 * own action raises the event that would run it again (e.g. Send to Program on Program) */
static obs_source_t *runningTrigger = nullptr;
static TriggerWhen runningTriggerWhen = TriggerWhen::Program;

/* a chain can still walk input to input (e.g. Program sends to Preview, Preview's trigger runs),
 * so a small budget stops a long or endless chain; the per-(input, when) guard above stops the
 * one-step self re-entry */
static const int MaxTriggerDepth = 3;
static int runningTriggerDepth = 0;

static void runTrigger(obs_source_t *input, TriggerAction action)
{
	switch (action) {
	case TriggerAction::Nothing:
		break;
	case TriggerAction::StartRecording:
		if (!obs_frontend_recording_active()) {
			obs_frontend_recording_start();
		}
		break;
	case TriggerAction::StopRecording:
		if (obs_frontend_recording_active()) {
			obs_frontend_recording_stop();
		}
		break;
	case TriggerAction::StartStream:
		OutputEngine::instance().setStreaming(true); /* the desk's stream, not OBS's own */
		break;
	case TriggerAction::StopStream:
		OutputEngine::instance().setStreaming(false);
		break;
	case TriggerAction::SendToPreview:
		toPreview(input);
		break;
	case TriggerAction::SendToProgram:
		cutToProgram(input);
		break;
	}
}

void runTriggers(obs_source_t *input, TriggerWhen when)
{
	if (!input) {
		return;
	}
	const TriggerAction action = trigger(input, when);
	if (action == TriggerAction::Nothing || (runningTrigger == input && runningTriggerWhen == when)) {
		return;
	}
	if (runningTriggerDepth >= MaxTriggerDepth) {
		return;
	}

	obs_source_t *savedInput = runningTrigger;
	const TriggerWhen savedWhen = runningTriggerWhen;
	runningTrigger = input;
	runningTriggerWhen = when;
	runningTriggerDepth++;

	runTrigger(input, action);

	runningTriggerDepth--;
	runningTrigger = savedInput;
	runningTriggerWhen = savedWhen;
}

void clearModel()
{
	restorePending = false;
	restoreTransition = nullptr;
	restorePreview = nullptr;
	runningTrigger = nullptr;
	runningTriggerDepth = 0;
}

static std::string uuidOf(obs_source_t *source)
{
	return obs_source_get_uuid(source);
}

LayerGraph graph()
{
	LayerGraph result;

	for (OBSSource &input : inputs()) {
		std::vector<std::string> &shown = result[uuidOf(input)];
		obs_scene_enum_items(
			obs_scene_from_source(input),
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				obs_source_t *source = obs_sceneitem_get_source(item);
				if (obs_scene_from_source(source)) {
					static_cast<std::vector<std::string> *>(param)->push_back(
						obs_source_get_uuid(source));
				}
				return true;
			},
			&shown);
	}

	return result;
}

/* scenes and sources share one set of names in OBS */
static std::string uniqueName(const std::string &wanted)
{
	std::string name = wanted;
	for (int n = 2;; n++) {
		OBSSourceAutoRelease existing = obs_get_source_by_name(name.c_str());
		if (!existing) {
			return name;
		}
		name = wanted + " " + std::to_string(n);
	}
}

OBSSource createInput(const char *unversionedId, const char *name, obs_data_t *settings)
{
	const char *id = obs_get_latest_input_type_id(unversionedId);
	if (!id) {
		return OBSSource();
	}

	const std::string sceneName = uniqueName(name);
	const std::string baseName = uniqueName(sceneName + " source");

	OBSSourceAutoRelease base = obs_source_create(id, baseName.c_str(), settings, nullptr);
	if (!base) {
		return OBSSource();
	}

	/* OBS's main window adds the scene to its list as soon as it exists */
	OBSSceneAutoRelease scene = obs_scene_create(sceneName.c_str());
	obs_sceneitem_t *item = obs_scene_add(scene, base);

	/* the fixed shape: the base fills the canvas and is locked (spec §3) */
	if (obs_source_get_output_flags(base) & OBS_SOURCE_VIDEO) {
		obs_video_info ovi = {};
		obs_get_video_info(&ovi);
		struct vec2 bounds;
		vec2_set(&bounds, (float)ovi.base_width, (float)ovi.base_height);
		obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
		obs_sceneitem_set_bounds(item, &bounds);
	}
	obs_sceneitem_set_locked(item, true);

	OBSSource input = obs_scene_get_source(scene);

	/* OBS puts a new scene after the selected one; a new input goes last */
	std::vector<OBSSource> all = inputs();
	for (size_t i = 0; i < all.size(); i++) {
		if (all[i].Get() == input.Get()) {
			moveInput((int)i, (int)all.size() - 1);
			break;
		}
	}

	return input;
}

bool renameInput(obs_source_t *input, const char *name)
{
	if (!name || !*name) {
		return false;
	}
	OBSSourceAutoRelease existing = obs_get_source_by_name(name);
	if (existing) {
		return false;
	}
	obs_source_set_name(input, name);
	return true;
}

DeleteCheck canDelete(obs_source_t *input)
{
	if (inputs().size() <= 1) {
		return DeleteCheck::LastInput;
	}

	/* on Program directly, or as a layer of what is on Program (spec §8) */
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	if (program && reaches(graph(), uuidOf(program), uuidOf(input))) {
		return DeleteCheck::OnProgram;
	}

	return DeleteCheck::Ok;
}

int layerUseCount(obs_source_t *input)
{
	const std::string id = uuidOf(input);
	int hosts = 0;
	for (const auto &[host, shown] : graph()) {
		if (host != id && std::find(shown.begin(), shown.end(), id) != shown.end()) {
			hosts++;
		}
	}
	return hosts;
}

void deleteInput(obs_source_t *input)
{
	struct Hits {
		obs_source_t *target;
		std::vector<OBSSceneItem> items;
	};

	for (OBSSource &host : inputs()) {
		if (host.Get() == input) {
			continue;
		}

		Hits hits{input, {}};
		obs_scene_enum_items(
			obs_scene_from_source(host),
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				auto *hits = static_cast<Hits *>(param);
				if (obs_sceneitem_get_source(item) == hits->target) {
					hits->items.emplace_back(item);
				}
				return true;
			},
			&hits);

		for (OBSSceneItem &item : hits.items) {
			obs_sceneitem_remove(item);
		}
	}

	/* OBS's main window takes the scene out of its list when it is removed */
	obs_source_remove(input);
}

static QListWidget *sceneList()
{
	static bool warned = false;

	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QListWidget *list = main ? main->findChild<QListWidget *>("scenes") : nullptr;
	if (!list && !warned) {
		warned = true;
		blog(LOG_WARNING, "[bread-desk] OBS's scene list 'scenes' was not found; reordering inputs is off");
	}
	return list;
}

bool canReorder()
{
	return sceneList() != nullptr;
}

bool moveInput(int from, int to)
{
	QListWidget *list = sceneList();
	if (!list || from == to || from < 0 || to < 0 || from >= list->count() || to >= list->count()) {
		return false;
	}

	{
		/* OBS switches scene when the list's selection changes; keep it quiet and
		 * put the selection back */
		QSignalBlocker quiet(list);
		QListWidgetItem *current = list->currentItem();
		QListWidgetItem *moved = list->takeItem(from);
		list->insertItem(to, moved);
		list->setCurrentItem(current);
	}

	/* the signal OBS's own drag raises; the multiview follows it */
	if (!QMetaObject::invokeMethod(list, "scenesReordered")) {
		blog(LOG_WARNING,
		     "[bread-desk] could not raise 'scenesReordered'; the multiview may show the old order");
	}

	obs_frontend_save();
	return true;
}

std::vector<Layer> layers(obs_source_t *input)
{
	std::vector<Layer> all;
	obs_scene_enum_items(
		obs_scene_from_source(input),
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			static_cast<std::vector<Layer> *>(param)->push_back(
				{obs_sceneitem_get_id(item), obs_source_get_name(obs_sceneitem_get_source(item)),
				 obs_sceneitem_visible(item)});
			return true;
		},
		&all);

	/* enumerated bottom first: drop the base, then put the top first */
	if (!all.empty()) {
		all.erase(all.begin());
	}
	std::reverse(all.begin(), all.end());
	return all;
}

static obs_sceneitem_t *findLayer(obs_source_t *host, int64_t id)
{
	return obs_scene_find_sceneitem_by_id(obs_scene_from_source(host), id);
}

void removeLayer(obs_source_t *host, int64_t id)
{
	if (obs_sceneitem_t *item = findLayer(host, id)) {
		obs_sceneitem_remove(item);
	}
}

void setLayerVisible(obs_source_t *host, int64_t id, bool visible)
{
	if (obs_sceneitem_t *item = findLayer(host, id)) {
		obs_sceneitem_set_visible(item, visible);
	}
}

void moveLayer(obs_source_t *host, int64_t id, bool up)
{
	const std::vector<Layer> all = layers(host); /* top first */
	const auto it = std::find_if(all.begin(), all.end(), [id](const Layer &layer) { return layer.id == id; });
	if (it == all.end()) {
		return;
	}
	if (up && it == all.begin()) {
		return;
	}
	if (!up && it + 1 == all.end()) {
		return; /* the next one down is the base */
	}

	obs_sceneitem_set_order(findLayer(host, id), up ? OBS_ORDER_MOVE_UP : OBS_ORDER_MOVE_DOWN);
}

} // namespace desk
