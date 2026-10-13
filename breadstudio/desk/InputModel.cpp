#include "InputModel.hpp"

#include <obs-frontend-api.h>

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

} // namespace desk
