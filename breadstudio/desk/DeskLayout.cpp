#include "DeskLayout.hpp"
#include "Desk.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QAction>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QToolBar>
#include <QToolButton>

namespace desk {

/* Bump when a change to applyLayout() must reach installs that already ran it: an install
 * whose stored version is below this applies the layout again once. */
constexpr int LayoutVersion = 2;

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

	/* OBS's menu bar and its Scenes and Sources toolbars are OBS chrome the desk does not
	 * use; hide them wherever their object names are found */
	main->menuBar()->setVisible(false);
	for (const char *name : {"scenesToolbar", "sourcesToolbar"}) {
		auto *toolbar = main->findChild<QToolBar *>(name);
		if (!toolbar) {
			blog(LOG_WARNING, "[bread-desk] toolbar '%s' was not found; it stays", name);
			continue;
		}
		toolbar->setVisible(false);
	}

	scenes->setVisible(false);
	sources->setVisible(false);
	obsMixer->setVisible(false);
	/* OBS's Controls dock carries Studio Mode, Recording, Virtual Cam and Settings; the
	 * desk replaces all of it, so it is hidden, not laid out */
	controls->setVisible(false);

	/* as vMix: the input bar on the left, the mixer docked to its right, then OBS's Scene
	 * Transitions (the transition and its duration are chosen there) */
	for (QDockWidget *dock : {inputs, mixer, transitions}) {
		dock->setFloating(false);
		main->addDockWidget(Qt::BottomDockWidgetArea, dock);
		dock->setVisible(true);
	}

	main->splitDockWidget(inputs, mixer, Qt::Horizontal);
	main->splitDockWidget(mixer, transitions, Qt::Horizontal);
	main->resizeDocks({inputs, mixer, transitions}, {900, 420, 160}, Qt::Horizontal);
	main->resizeDocks({inputs}, {320}, Qt::Vertical);

	return true;
}

void applyLayoutOnFirstRun()
{
	config_t *config = obs_frontend_get_user_config();
	if (config_get_int(config, "BreadDesk", "LayoutVersion") >= LayoutVersion) {
		return;
	}

	if (applyLayout()) {
		config_set_int(config, "BreadDesk", "LayoutVersion", LayoutVersion);
		config_save_safe(config, "tmp", nullptr);
	}
}

void hideObsStreamButton()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QPushButton *button = main ? main->findChild<QPushButton *>("streamButton") : nullptr;
	if (!button) {
		blog(LOG_WARNING, "[bread-desk] OBS's 'streamButton' was not found; it stays");
		return;
	}
	button->setVisible(false);
}

void addTopMenu()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QMenuBar *bar = main ? main->menuBar() : nullptr;
	if (!main || !bar) {
		blog(LOG_WARNING,
		     "[bread-desk] the main window or its menu bar was not found; the top menu was not added");
		return;
	}

	/* the menu bar is hidden, so its top-level menus (File first, then Edit, View, Docks,
	 * Profile, Scene Collection, Tools and Help — the order the menu bar holds them in) move
	 * into one popup, keeping that order */
	QMenu *popup = new QMenu(main);
	for (QAction *action : bar->actions()) {
		if (action->menu()) {
			popup->addMenu(action->menu());
		} else {
			popup->addAction(action);
		}
	}
	bar->setVisible(false);

	QToolBar *toolbar = new QToolBar(main);
	toolbar->setObjectName("breadTopMenu");
	toolbar->setMovable(false);
	main->addToolBar(Qt::TopToolBarArea, toolbar);

	QToolButton *button = new QToolButton(toolbar);
	button->setText(QString::fromUtf8("\xE2\x98\xB0"));
	button->setPopupMode(QToolButton::InstantPopup);
	button->setMenu(popup);
	button->setToolTip(text("Menu"));
	toolbar->addWidget(button);
}

} // namespace desk
