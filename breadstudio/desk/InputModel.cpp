#include "InputModel.hpp"

#include <obs-frontend-api.h>

#include <graphics/vec2.h>

#include <QListWidget>
#include <QMainWindow>
#include <QSignalBlocker>
#include <QTimer>

#include <algorithm>
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

/* A tile's Cut is done with the frontend API alone: select the Cut transition, switch,
 * and put back the operator's transition and Preview when the cut has finished. */
static OBSSource restoreTransition;
static OBSSource restorePreview;
static bool restorePending = false;

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
	const float time = obs_transition_get_time(current);
	if (time > 0.0f && time < 1.0f) {
		return; /* a transition is running; OBS ignores a new one */
	}

	OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
	restorePreview = preview.Get();

	OBSSource cut = cutTransition();
	if (cut && cut.Get() != current.Get()) {
		restoreTransition = current.Get();
		obs_frontend_set_current_transition(cut);
	}

	restorePending = true;
	obs_frontend_set_current_scene(input);

	/* in case the stop event never comes */
	QTimer::singleShot(500, [] { onTransitionStopped(); });
}

void onTransitionStopped()
{
	if (!restorePending) {
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

void clearModel()
{
	restorePending = false;
	restoreTransition = nullptr;
	restorePreview = nullptr;
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
		blog(LOG_WARNING, "[bread-desk] could not raise 'scenesReordered'; the multiview may show the old order");
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

bool addLayer(obs_source_t *host, obs_source_t *layerInput)
{
	if (wouldLoop(graph(), uuidOf(host), uuidOf(layerInput))) {
		return false;
	}

	obs_sceneitem_t *item = obs_scene_add(obs_scene_from_source(host), layerInput);

	/* half size in the middle, so the base still shows and the layer is easy to grab */
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	struct vec2 scale;
	vec2_set(&scale, 0.5f, 0.5f);
	obs_sceneitem_set_scale(item, &scale);
	struct vec2 pos;
	vec2_set(&pos, (float)ovi.base_width / 4.0f, (float)ovi.base_height / 4.0f);
	obs_sceneitem_set_pos(item, &pos);

	return true;
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
