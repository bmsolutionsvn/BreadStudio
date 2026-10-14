#pragma once

/* What Program and Preview draw (network inputs spec §8). OBS's "active" and "showing" cannot be
 * used on the desk: the input bar and the multiview keep every input showing, and the audio bed
 * keeps an input with AFV unlit active. Shared by the OMT and NDI plugins. */

#include <obs.h>
#include <obs-frontend-api.h>

namespace bread {

/* True when root is target, or draws it: a visible item at any depth of a scene or a group, or
 * either side of a running transition. Each item's own visible flag is read, not OBS's active tree,
 * which keeps a hidden item until the audio thread has caught up. */
inline bool draws(obs_source_t *root, obs_source_t *target)
{
	if (!root || !target) {
		return false;
	}
	if (root == target) {
		return true;
	}

	struct Search {
		obs_source_t *target;
		bool found;
	} search{target, false};

	if (obs_scene_t *scene = obs_group_or_scene_from_source(root)) {
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				auto *s = static_cast<Search *>(param);
				if (obs_sceneitem_visible(item) && draws(obs_sceneitem_get_source(item), s->target)) {
					s->found = true;
				}
				return !s->found;
			},
			&search);
	} else if (obs_source_get_type(root) == OBS_SOURCE_TYPE_TRANSITION) {
		obs_source_enum_active_sources(
			root,
			[](obs_source_t *, obs_source_t *child, void *param) {
				auto *s = static_cast<Search *>(param);
				if (!s->found && draws(child, s->target)) {
					s->found = true;
				}
			},
			&search);
	}
	return search.found;
}

struct OnAir {
	bool program = false;
	bool preview = false;
};

/* UI thread only: the frontend is asked for the Preview scene. */
inline OnAir onAir(obs_source_t *source)
{
	obs_source_t *program = obs_get_output_source(0);
	obs_source_t *preview = obs_frontend_preview_program_mode_active() ? obs_frontend_get_current_preview_scene()
									   : nullptr;

	OnAir result;
	result.program = draws(program, source);
	result.preview = draws(preview, source);

	obs_source_release(program);
	obs_source_release(preview);
	return result;
}

} // namespace bread
