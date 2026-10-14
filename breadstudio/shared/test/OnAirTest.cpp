#include "OnAir.hpp"

#include <cstdio>

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

static obs_source_t *scene(const char *name)
{
	return obs_scene_get_source(obs_scene_create_private(name));
}

int main()
{
	if (!obs_startup("en-US", nullptr, nullptr)) {
		std::printf("FAIL: obs_startup\n");
		return 1;
	}

	/* scenes stand in for inputs: on the desk an input is a scene */
	obs_source_t *cam = scene("cam");
	obs_source_t *program = scene("program");
	obs_source_t *box = scene("box");
	obs_source_t *other = scene("other");

	/* the input itself, as when it is on Program alone */
	CHECK(bread::draws(cam, cam));
	CHECK(!bread::draws(nullptr, cam));
	CHECK(!bread::draws(program, nullptr));

	/* absent */
	CHECK(!bread::draws(program, cam));

	/* an item, then hidden */
	obs_sceneitem_t *alone = obs_scene_add(obs_scene_from_source(program), cam);
	CHECK(bread::draws(program, cam));
	obs_sceneitem_set_visible(alone, false);
	CHECK(!bread::draws(program, cam));

	/* a layer of an input that is drawn, then that layer hidden */
	obs_scene_add(obs_scene_from_source(box), cam);
	obs_sceneitem_t *layer = obs_scene_add(obs_scene_from_source(program), box);
	CHECK(bread::draws(program, cam));
	obs_sceneitem_set_visible(layer, false);
	CHECK(!bread::draws(program, cam));

	/* in a group */
	obs_sceneitem_t *group = obs_scene_add_group(obs_scene_from_source(other), "group");
	obs_scene_add(obs_sceneitem_group_get_scene(group), cam);
	CHECK(bread::draws(other, cam));

	/* never the other way round */
	CHECK(!bread::draws(cam, other));

	/* Nothing is released and OBS is not shut down: without a graphics thread libobs destroys
	 * nested scenes badly ("Double destroy", then a crash, seen 2026-10-14). The exit frees it all. */
	if (failures == 0) {
		std::printf("OnAir: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
