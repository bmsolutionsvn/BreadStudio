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

#include <cstring>

namespace desk {

/* Bump when a change to applyLayout() must reach installs that already ran it: an install
 * whose stored version is below this applies the layout again once. */
constexpr int LayoutVersion = 3;

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
	QDockWidget *transitions = find("transitionsDock");
	if (!inputs || !mixer || !transitions) {
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

	/* as vMix: the input bar on the left and the mixer docked to its right. OBS's Scene
	 * Transitions (the transition and its duration are chosen there) is placed only while it
	 * is pinned (backlog B3); otherwise it is hidden and the layout is not shaped for it. */
	for (QDockWidget *dock : {inputs, mixer}) {
		dock->setFloating(false);
		main->addDockWidget(Qt::BottomDockWidgetArea, dock);
		dock->setVisible(true);
	}
	main->splitDockWidget(inputs, mixer, Qt::Horizontal);

	applyTransitionsPin();
	main->resizeDocks({inputs}, {320}, Qt::Vertical);

	return true;
}

void applyTransitionsPin()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QDockWidget *transitions = main ? main->findChild<QDockWidget *>("transitionsDock") : nullptr;
	QDockWidget *inputs = main ? main->findChild<QDockWidget *>(InputsDockId) : nullptr;
	QDockWidget *mixer = main ? main->findChild<QDockWidget *>(MixerDockId) : nullptr;
	if (!transitions || !inputs || !mixer) {
		blog(LOG_WARNING,
		     "[bread-desk] a dock the transitions pin needs was not found; the pin was not applied");
		return;
	}

	if (!transitionsPinned()) {
		transitions->hide();
		main->resizeDocks({inputs, mixer}, {900, 420}, Qt::Horizontal);
		return;
	}

	/* the bottom area, split to the right of the mixer, at the width applyLayout gives it */
	transitions->setFloating(false);
	main->addDockWidget(Qt::BottomDockWidgetArea, transitions);
	main->splitDockWidget(mixer, transitions, Qt::Horizontal);
	transitions->setVisible(true);
	main->resizeDocks({inputs, mixer, transitions}, {900, 420, 160}, Qt::Horizontal);
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

bool transitionsPinned()
{
	config_t *config = obs_frontend_get_user_config();
	return config && config_get_bool(config, "BreadDesk", "TransitionsPinned");
}

void setTransitionsPinned(bool pinned)
{
	/* save only once the dock the setting is about exists, so a missing dock cannot leave a
	 * "pinned" flag stored with nothing pinned */
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main || !main->findChild<QDockWidget *>("transitionsDock")) {
		blog(LOG_WARNING,
		     "[bread-desk] dock 'transitionsDock' was not found; the pinned state was not applied");
		return;
	}

	config_t *config = obs_frontend_get_user_config();
	if (config) {
		config_set_bool(config, "BreadDesk", "TransitionsPinned", pinned);
		config_save_safe(config, "tmp", nullptr);
	}

	applyTransitionsPin();
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

/* The menus the ☰ keeps (K2). Edit, View, Docks, Tools and Help are dropped: the desk's
 * surface is the desk, and those menus reach OBS's scene editor, docks and help. */
static const char *const KeptMenus[] = {
	"menu_File",
	"profileMenu",
	"sceneCollectionMenu",
};

/* The File entries that stay outside the desk's job (cleanup spec §6). */
static const char *const DroppedFileItems[] = {
	"actionShow_Recordings",
	"actionRemux",
	"actionShowSettingsFolder",
	"actionShowProfileFolder",
};

/* Dropping an entry leaves its separators behind; take out the ones now at an edge. */
static void tidySeparators(QMenu *menu)
{
	QList<QAction *> actions = menu->actions();
	if (!actions.isEmpty() && actions.first()->isSeparator()) {
		menu->removeAction(actions.first());
	}
	actions = menu->actions();
	if (!actions.isEmpty() && actions.last()->isSeparator()) {
		menu->removeAction(actions.last());
	}
}

static bool keptMenu(const char *name)
{
	for (const char *kept : KeptMenus) {
		if (strcmp(name, kept) == 0) {
			return true;
		}
	}
	return false;
}

QMenu *obsMainMenu()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QMenuBar *bar = main ? main->menuBar() : nullptr;
	if (!main || !bar) {
		blog(LOG_WARNING, "[bread-desk] the main window or its menu bar was not found; the menu was not built");
		return nullptr;
	}

	/* built once, then cached. The menu bar is hidden, so the menus the desk keeps move into
	 * one popup; the rest are never added to it */
	static QMenu *popup = nullptr;
	if (!popup) {
		popup = new QMenu(main);
		for (QAction *action : bar->actions()) {
			QMenu *menu = action->menu();
			if (!menu) {
				continue;
			}
			const QByteArray name = menu->objectName().toUtf8();
			if (!keptMenu(name.constData())) {
				continue;
			}
			bool dropped = false;
			for (const char *name : DroppedFileItems) {
				QAction *item = main->findChild<QAction *>(name);
				if (item && menu->actions().contains(item)) {
					menu->removeAction(item);
					dropped = true;
				}
			}
			if (dropped) {
				tidySeparators(menu);
			}
			popup->addMenu(menu);
		}

		/* the desk keeps OBS's Multiview projector — the one part of View that puts a
		 * view on a local monitor (K5). OBS fills this submenu's monitor list from the
		 * View menu's hover, and View is dropped, so the list is whatever it was at
		 * launch: a monitor added while BreadStudio runs is offered only after a
		 * restart. */
		if (QMenu *multiview = main->findChild<QMenu *>("multiviewProjectorMenu")) {
			popup->addMenu(text("Projector"))->addMenu(multiview);
		} else {
			blog(LOG_WARNING,
			     "[bread-desk] the Multiview projector menu was not found; the projector is not offered");
		}

		/* Help is dropped, so OBS's About — and the licence and authors notices it
		 * opens — would be unreachable. File is where Windows apps put About, so
		 * re-home OBS's action there, after Settings and before Exit. OBS keeps
		 * owning the action and its slot. */
		if (QMenu *file = main->findChild<QMenu *>("menu_File")) {
			QAction *about = main->findChild<QAction *>("actionShowAbout");
			if (about) {
				/* insert before the last separator so Exit stays last */
				QAction *before = nullptr;
				for (QAction *item : file->actions()) {
					if (item->isSeparator()) {
						before = item;
					}
				}
				file->insertAction(before, about);
			} else {
				blog(LOG_WARNING,
				     "[bread-desk] OBS's About action was not found; About is not offered");
			}
		} else {
			blog(LOG_WARNING, "[bread-desk] the File menu was not found; About is not offered");
		}

		bar->setVisible(false);
	}
	return popup;
}

} // namespace desk
