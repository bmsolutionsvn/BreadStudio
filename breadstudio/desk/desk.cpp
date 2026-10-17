#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QApplication>
#include <QString>
#include <QTimer>

#include "AudioBed.hpp"
#include "BusMeters.hpp"
#include "Desk.hpp"
#include "DeskChrome.hpp"
#include "DeskLayout.hpp"
#include "InputBar.hpp"
#include "InputModel.hpp"
#include "Mixer.hpp"
#include "MultiviewSource.hpp"
#include "OutputEngine.hpp"
#include "ProgramSource.hpp"
#include "Project.hpp"

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

/* true while a collection is loading: OBS raises the scene events after the collection event, so
 * a scene trigger must not run merely because OBS was opened or a collection was loaded */
static bool suppressTriggers = false;

static InputBar *bar = nullptr; /* owned by its dock */

static Mixer *mixer = nullptr; /* owned by its dock */

/* the one Project the desk has open (P2) */
static Project *project = nullptr;

/* declared here, defined with the other save callbacks: the EXIT case takes it off */
static void onProjectSaved(obs_data_t *save_data, bool saving, void *private_data);

/* a scene collection's outputs, read while it loads and started once its scenes exist */
static OutputsConfig pendingOutputs;

namespace desk {
void inputsChanged()
{
	if (!loaded) {
		return;
	}
	bed().sync();
	bar->refresh();
	mixer->rebuild();
	OutputEngine::instance().refreshSources();
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
	/* The desk takes its microphones and system audio as inputs and does not use OBS's global
	 * audio devices, so they are cleared at every load - keeping them out of the mixer and out
	 * of the mix. OBS keeps them on channels 1-6; the desk's bed uses the highest free channel
	 * from 63 down, so it never lives there. */
	for (uint32_t channel = 1; channel <= 6; channel++) {
		OBSSourceAutoRelease source = obs_get_output_source(channel);
		if (source) {
			obs_source_remove(source.Get());
		}
	}
	/* the scene events that follow are OBS replaying the collection's scene, not the operator
	 * changing it, so hold triggers off until the next event-loop turn */
	suppressTriggers = true;
	QTimer::singleShot(0, qApp, [] { suppressTriggers = false; });
	bed().rebuild();
	bar->refresh();
	mixer->rebuild();
	/* senders whose settings did not change keep running (outputs spec §7) */
	OutputEngine::instance().apply(pendingOutputs);
	OutputEngine::instance().refreshSources();
}

static void unload()
{
	/* black, senders running, and no scene of the old collection held */
	OutputEngine::instance().detachSources();
	loaded = false;
	bar->clear();
	mixer->clear();
	clearModel();
	bed().clear();
}

/* An input can carry a trigger for the moment it becomes Program or Preview (inputs spec
 * §3.2); run it here, where the desk already follows the two scene events. */
static void runSceneTriggers(enum obs_frontend_event event)
{
	if (suppressTriggers) {
		return;
	}
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		OBSSourceAutoRelease program = obs_frontend_get_current_scene();
		runTriggers(program.Get(), TriggerWhen::Program);
	} else if (event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED && obs_frontend_preview_program_mode_active()) {
		OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
		runTriggers(preview.Get(), TriggerWhen::Preview);
	}
}

static void onEvent(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		load();
		applyLayoutOnFirstRun();
		/* the desk cannot work without its own two docks, and applyLayout places them only
		 * once per layout version, so they are put back at every load */
		ensureDeskDocks();
		/* the desk cannot work without Studio Mode, and OBS stores it off, so it is put on
		 * here at every load - not only when the layout is applied (S1 §4.1). A no-op when
		 * applyLayout already switched it on. */
		ensureStudioMode();
		hideObsStreamButton();
		applyChrome();
		/* last word on the Scene Transitions dock, after OBS restored its own dock state
		 * and the chrome sweep ran, so the stored pin always wins (backlog B3) */
		applyTransitionsPin();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		load();
		applyChrome();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		unload();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		OutputEngine::instance().shutdown();
		unload();
		busMeters().stop();
		signal_handler_disconnect(obs_get_signal_handler(), "source_rename", onSourceRename, nullptr);
		/* the save callback is taken off before the Project it writes back to is
		 * released, so a save during teardown cannot call a freed object */
		obs_frontend_remove_save_callback(onProjectSaved, nullptr);
		delete project;
		project = nullptr;
		break;
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		inputsChanged();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		bar->updateOutputs();
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
			bed().sync();
			bar->updateTally();
			OutputEngine::instance().refreshSources();
			runSceneTriggers(event);
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

static void saveOutputs(obs_data_t *data, bool saving, void *)
{
	if (saving) {
		OutputEngine::instance().save(data);
	} else {
		pendingOutputs = OutputEngine::instance().load(data);
	}
}

/* OBS writes the scene collection: the show has moved on since the .bstd was last
 * written, so the open Project has unsaved changes (P9). A save the desk asked for
 * itself is absorbed by the Project, so its own write does not bring the dot back. */
static void onProjectSaved(obs_data_t *, bool saving, void *)
{
	if (project) {
		project->noteObSave(saving);
	}
}

bool obs_module_load(void)
{
	registerProgramSource();
	registerMultiviewSource();
	busMeters().start();

	bar = new InputBar();
	obs_frontend_add_dock_by_id(InputsDockId, obs_module_text("Inputs"), bar);

	mixer = new Mixer();
	obs_frontend_add_dock_by_id(MixerDockId, obs_module_text("Mixer"), mixer);
	signal_handler_connect(obs_get_signal_handler(), "source_rename", onSourceRename, nullptr);

	project = new Project();

	obs_frontend_add_event_callback(onEvent, nullptr);

	for (size_t i = 0; i < NumberHotkeys; i++) {
		const QByteArray name = QString("BreadDesk.InputToPreview.%1").arg(i + 1).toUtf8();
		const QByteArray label = text("Hotkey.InputToPreview").arg(i + 1).toUtf8();
		numberHotkey[i] = obs_hotkey_register_frontend(name.constData(), label.constData(), onNumberHotkey,
							       (void *)(uintptr_t)i);
	}
	obs_frontend_add_save_callback(saveNumberHotkeys, nullptr);
	obs_frontend_add_save_callback(saveOutputs, nullptr);
	obs_frontend_add_save_callback(onProjectSaved, nullptr);

	obs_frontend_add_tools_menu_item(obs_module_text("DeskLayout"), [](void *) { applyLayout(); }, nullptr);

	blog(LOG_INFO, "[bread-desk] loaded");
	return true;
}
