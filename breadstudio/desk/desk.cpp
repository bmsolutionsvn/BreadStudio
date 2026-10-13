#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QApplication>
#include <QTimer>

#include "AudioBed.hpp"
#include "BusMeters.hpp"
#include "Desk.hpp"
#include "InputBar.hpp"
#include "InputModel.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bread-desk", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "BreadStudio desk";
}

using namespace desk;

/* true between a scene collection finishing loading and starting to unload */
static bool loaded = false;

static InputBar *bar = nullptr; /* owned by its dock */

namespace desk {
void inputsChanged()
{
	if (!loaded) {
		return;
	}
	bed().sync();
	bar->refresh();
}
} // namespace desk

/* OBS raises no frontend event when a scene is renamed, so follow libobs's own signal.
 * It can come from any thread. */
static void onSourceRename(void *, calldata_t *)
{
	QMetaObject::invokeMethod(qApp, [] { inputsChanged(); }, Qt::QueuedConnection);
}

static void load()
{
	loaded = true;
	bed().rebuild();
	bar->refresh();
}

static void unload()
{
	loaded = false;
	bar->clear();
	clearModel();
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
		signal_handler_disconnect(obs_get_signal_handler(), "source_rename", onSourceRename, nullptr);
		break;
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		inputsChanged();
		break;
	case OBS_FRONTEND_EVENT_TRANSITION_STOPPED:
		onTransitionStopped();
		bar->updateTally();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		if (loaded) {
			bar->updateTally();
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

	bar = new InputBar();
	obs_frontend_add_dock_by_id(InputsDockId, obs_module_text("Inputs"), bar);
	signal_handler_connect(obs_get_signal_handler(), "source_rename", onSourceRename, nullptr);

	obs_frontend_add_event_callback(onEvent, nullptr);

	obs_frontend_add_tools_menu_item("Desk probe: toggle AFV of the Preview input", probeToggleAfv, nullptr);
	obs_frontend_add_tools_menu_item("Desk probe: log bus peaks", probeLogPeaks, nullptr);

	blog(LOG_INFO, "[bread-desk] loaded");
	return true;
}
