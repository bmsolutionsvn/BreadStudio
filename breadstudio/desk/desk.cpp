#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QTimer>

#include "AudioBed.hpp"
#include "BusMeters.hpp"
#include "Desk.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bread-desk", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "BreadStudio desk";
}

using namespace desk;

/* true between a scene collection finishing loading and starting to unload */
static bool loaded = false;

static void load()
{
	loaded = true;
	bed().rebuild();
}

static void unload()
{
	loaded = false;
	bed().clear();
}

static void onEvent(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		load();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		unload();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		unload();
		busMeters().stop();
		break;
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		if (loaded) {
			bed().sync();
		}
		break;
	default:
		break;
	}
}

/* ---- temporary probe for the audio bed check; removed in the mixer task ---- */

static void probeToggleAfv(void *)
{
	OBSSourceAutoRelease input = obs_frontend_get_current_preview_scene();
	if (!input) {
		input = obs_frontend_get_current_scene();
	}
	if (!input) {
		return;
	}

	const bool on = !AudioBed::afv(input);
	bed().setAfv(input, on);
	blog(LOG_INFO, "[bread-desk] probe: AFV of '%s' is now %s", obs_source_get_name(input), on ? "on" : "off");
}

static void probeLogPeaks(void *)
{
	/* throw away what has built up, then measure one second */
	for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		busMeters().takePeakDb(bus);
	}
	QTimer::singleShot(1000, [] {
		for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
			blog(LOG_INFO, "[bread-desk] probe: bus %c peak %.1f dB", "MABCDE"[bus],
			     (double)busMeters().takePeakDb(bus));
		}
	});
}

/* ---- end of probe ---- */

bool obs_module_load(void)
{
	busMeters().start();
	obs_frontend_add_event_callback(onEvent, nullptr);

	obs_frontend_add_tools_menu_item("Desk probe: toggle AFV of the Preview input", probeToggleAfv, nullptr);
	obs_frontend_add_tools_menu_item("Desk probe: log bus peaks", probeLogPeaks, nullptr);

	blog(LOG_INFO, "[bread-desk] loaded");
	return true;
}
