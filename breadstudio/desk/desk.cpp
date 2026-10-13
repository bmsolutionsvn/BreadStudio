#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QApplication>
#include <QString>

#include "AudioBed.hpp"
#include "BusMeters.hpp"
#include "Desk.hpp"
#include "DeskLayout.hpp"
#include "InputBar.hpp"
#include "InputModel.hpp"
#include "Mixer.hpp"

#include <vector>

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

static Mixer *mixer = nullptr; /* owned by its dock */

namespace desk {
void inputsChanged()
{
	if (!loaded) {
		return;
	}
	bed().sync();
	bar->refresh();
	mixer->rebuild();
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
	mixer->rebuild();
}

static void unload()
{
	loaded = false;
	bar->clear();
	mixer->clear();
	clearModel();
	bed().clear();
}

static void onEvent(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		load();
		applyLayoutOnFirstRun();
		break;
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

/* "Input N to Preview": follows the number, not the scene (spec §4.6). OBS keeps
 * plugin hotkey bindings with the scene collection. */
static const size_t NumberHotkeys = 9;
static obs_hotkey_id numberHotkey[NumberHotkeys];

static void onNumberHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	const size_t index = (size_t)(uintptr_t)data;
	QMetaObject::invokeMethod(
		qApp,
		[index] {
			std::vector<OBSSource> all = inputs();
			if (loaded && index < all.size()) {
				toPreview(all[index]);
			}
		},
		Qt::QueuedConnection);
}

static void saveNumberHotkeys(obs_data_t *data, bool saving, void *)
{
	for (size_t i = 0; i < NumberHotkeys; i++) {
		const QByteArray key = QString("bread_desk_input_hotkey_%1").arg(i + 1).toUtf8();
		if (saving) {
			OBSDataArrayAutoRelease bindings = obs_hotkey_save(numberHotkey[i]);
			obs_data_set_array(data, key.constData(), bindings);
		} else {
			OBSDataArrayAutoRelease bindings = obs_data_get_array(data, key.constData());
			obs_hotkey_load(numberHotkey[i], bindings);
		}
	}
}

bool obs_module_load(void)
{
	busMeters().start();

	bar = new InputBar();
	obs_frontend_add_dock_by_id(InputsDockId, obs_module_text("Inputs"), bar);

	mixer = new Mixer();
	obs_frontend_add_dock_by_id(MixerDockId, obs_module_text("Mixer"), mixer);
	signal_handler_connect(obs_get_signal_handler(), "source_rename", onSourceRename, nullptr);

	obs_frontend_add_event_callback(onEvent, nullptr);

	for (size_t i = 0; i < NumberHotkeys; i++) {
		const QByteArray name = QString("BreadDesk.InputToPreview.%1").arg(i + 1).toUtf8();
		const QByteArray label = text("Hotkey.InputToPreview").arg(i + 1).toUtf8();
		numberHotkey[i] = obs_hotkey_register_frontend(name.constData(), label.constData(), onNumberHotkey,
							       (void *)(uintptr_t)i);
	}
	obs_frontend_add_save_callback(saveNumberHotkeys, nullptr);

	obs_frontend_add_tools_menu_item(obs_module_text("DeskLayout"), [](void *) { applyLayout(); }, nullptr);

	blog(LOG_INFO, "[bread-desk] loaded");
	return true;
}
