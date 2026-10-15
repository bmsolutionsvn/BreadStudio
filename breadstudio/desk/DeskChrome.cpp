#include "DeskChrome.hpp"
#include "Desk.hpp"

#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QDockWidget>

#include <cstring>

namespace desk {

/* The docks the desk expects OBS to have and does not use, by object name (cleanup spec §6).
 * Only used to warn when one is missing: the sweep below hides them. OBS's Scene Transitions
 * (transitionsDock) is NOT here: it is the desk's own transition control and the layout places
 * it beside the mixer. */
static const char *const HiddenDocks[] = {
	"scenesDock", "sourcesDock", "mixerDock", "controlsDock", "statsDock",
};

/* The docks that must never be closable, as vMix (K10): the desk's own two, and OBS's Scene
 * Transitions, which the desk depends on for the transition and its duration. */
static const char *const UndismissableDocks[] = {
	InputsDockId,
	MixerDockId,
	"transitionsDock",
};

void applyChrome()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main) {
		blog(LOG_WARNING, "[bread-desk] the main window was not found; OBS's surface was not hidden");
		return;
	}

	for (const char *name : HiddenDocks) {
		if (!main->findChild<QDockWidget *>(name)) {
			blog(LOG_WARNING, "[bread-desk] dock '%s' was not found; it was left alone", name);
		}
	}

	/* one pass over every dock (cleanup spec §6/§7): a dock in the keep list is made movable and
	 * floatable but not closable (K10); every other dock - OBS's own and any a loaded plugin
	 * added - is hidden. The sweep hides every dock not in the keep list, so any dock the desk
	 * adds later must be added to the keep list or it will vanish. */
	for (QDockWidget *dock : main->findChildren<QDockWidget *>()) {
		const QByteArray name = dock->objectName().toUtf8();
		bool keep = false;
		for (const char *kept : UndismissableDocks) {
			keep = keep || strcmp(name.constData(), kept) == 0;
		}
		if (keep) {
			dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
			continue;
		}
		dock->hide();
	}
}

} // namespace desk
