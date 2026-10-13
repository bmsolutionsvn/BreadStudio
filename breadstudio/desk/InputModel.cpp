#include "InputModel.hpp"

#include <obs-frontend-api.h>

#include <QTimer>

#include <cstring>

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

} // namespace desk
