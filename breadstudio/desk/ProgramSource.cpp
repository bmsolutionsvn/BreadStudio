#include "ProgramSource.hpp"

#include <obs-module.h>

namespace desk {

static const char *programName(void *)
{
	return obs_module_text("ProgramPicture");
}

static void *programCreate(obs_data_t *, obs_source_t *source)
{
	return source;
}

static void programDestroy(void *) {}

static uint32_t programWidth(void *)
{
	obs_video_info ovi = {};
	return obs_get_video_info(&ovi) ? ovi.base_width : 0;
}

static uint32_t programHeight(void *)
{
	obs_video_info ovi = {};
	return obs_get_video_info(&ovi) ? ovi.base_height : 0;
}

/* OBS draws the main mix before any other mix in every frame, so this is the frame Program shows,
 * during a fade too, and the transition is drawn only once (outputs spec §4.1) */
static void programRender(void *, gs_effect_t *)
{
	obs_render_main_texture();
}

void registerProgramSource()
{
	obs_source_info info = {};
	info.id = ProgramSourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_CAP_DISABLED;
	info.get_name = programName;
	info.create = programCreate;
	info.destroy = programDestroy;
	info.get_width = programWidth;
	info.get_height = programHeight;
	info.video_render = programRender;
	obs_register_source(&info);
}

} // namespace desk
