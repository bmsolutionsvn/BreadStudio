#include "DeskLayout.hpp"
#include "Desk.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QDockWidget>
#include <QMainWindow>

namespace desk {

bool applyLayout()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main) {
		return false;
	}

	/* OBS's docks are found by the names in its main window (spec §6.2) */
	auto find = [main](const char *name) {
		auto *dock = main->findChild<QDockWidget *>(name);
		if (!dock) {
			blog(LOG_WARNING, "[bread-desk] dock '%s' was not found; the desk layout was not applied",
			     name);
		}
		return dock;
	};

	QDockWidget *inputs = find(InputsDockId);
	QDockWidget *mixer = find(MixerDockId);
	QDockWidget *scenes = find("scenesDock");
	QDockWidget *sources = find("sourcesDock");
	QDockWidget *obsMixer = find("mixerDock");
	QDockWidget *controls = find("controlsDock");
	QDockWidget *transitions = find("transitionsDock");
	if (!inputs || !mixer || !scenes || !sources || !obsMixer || !controls || !transitions) {
		return false;
	}

	obs_frontend_set_preview_program_mode(true);

	scenes->setVisible(false);
	sources->setVisible(false);
	obsMixer->setVisible(false);

	for (QDockWidget *dock : {inputs, mixer, transitions, controls}) {
		dock->setFloating(false);
		main->addDockWidget(Qt::BottomDockWidgetArea, dock);
		dock->setVisible(true);
	}

	/* as vMix: the input bar on the left, the mixer docked to its right, then OBS's Scene
	 * Transitions (the transition and its duration are chosen there) and Controls */
	main->splitDockWidget(inputs, mixer, Qt::Horizontal);
	main->splitDockWidget(mixer, transitions, Qt::Horizontal);
	main->splitDockWidget(transitions, controls, Qt::Horizontal);
	main->resizeDocks({inputs, mixer, transitions, controls}, {900, 420, 160, 160}, Qt::Horizontal);
	main->resizeDocks({inputs}, {320}, Qt::Vertical);

	return true;
}

void applyLayoutOnFirstRun()
{
	config_t *config = obs_frontend_get_user_config();
	if (config_get_bool(config, "BreadDesk", "LayoutApplied")) {
		return;
	}

	if (applyLayout()) {
		config_set_bool(config, "BreadDesk", "LayoutApplied", true);
		config_save_safe(config, "tmp", nullptr);
	}
}

} // namespace desk
