#include "DeskLayout.hpp"
#include "Desk.hpp"
#include "Project.hpp"
#include "VmixPreset.hpp"
#include "VmixReport.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QAction>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include <cstring>
#include <string>

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

void ensureDeskDocks()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main) {
		return;
	}

	auto find = [main](const char *name) {
		auto *dock = main->findChild<QDockWidget *>(name);
		if (!dock) {
			blog(LOG_WARNING, "[bread-desk] dock '%s' was not found; the desk docks were not ensured",
			     name);
		}
		return dock;
	};

	QDockWidget *inputs = find(InputsDockId);
	QDockWidget *mixer = find(MixerDockId);
	if (!inputs || !mixer) {
		return;
	}

	/* the desk's arrangement wins over OBS's stored dock state, so both docks are placed at
	 * every load, not only when one was lost or floating */
	for (QDockWidget *dock : {inputs, mixer}) {
		dock->setFloating(false);
		main->addDockWidget(Qt::BottomDockWidgetArea, dock);
		dock->setVisible(true);
	}

	main->splitDockWidget(inputs, mixer, Qt::Horizontal);
	main->resizeDocks({inputs, mixer}, {900, 420}, Qt::Horizontal);
	main->resizeDocks({inputs}, {320}, Qt::Vertical);
}

void ensureStudioMode()
{
	if (obs_frontend_preview_program_mode_active()) {
		return;
	}

	obs_frontend_set_preview_program_mode(true);
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

/* The menu bar menus the ☰ keeps (K2): only File. Edit, View, Docks, Tools and Help are
 * dropped - the desk's surface is the desk, and those menus reach OBS's scene editor, docks and
 * help - and OBS's Profile and Scene Collection menus are dropped too, because the desk's own
 * Project replaces both (P3). The desk's Project menu is added by obsMainMenu() before File. */
static const char *const KeptMenus[] = {
	"menu_File",
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

/* the disk folder holding the open Project, for Reveal in Explorer; empty when there is none */
static QString projectFolder()
{
	Project *open = project();
	if (!open || open->path().empty()) {
		return {};
	}
	return QFileInfo(QString::fromStdString(open->path())).absolutePath();
}

static bool saveProjectAs(QWidget *parent);

/* The open Project is written, falling through to Save As when it has no path yet (P2/P3).
 * False when nothing was written: a cancelled dialog, or an error the operator was shown. */
static bool saveProject(QWidget *parent)
{
	Project *open = project();
	if (!open) {
		return false;
	}
	if (open->path().empty()) {
		return saveProjectAs(parent);
	}
	std::string error;
	if (!open->save(error)) {
		QMessageBox::warning(parent, text("Project.Save"), QString::fromStdString(error));
		return false;
	}
	applyProjectTitle();
	return true;
}

/* Save As: the path, then the two ticks (P6, P7). The ticks belong to the save rather than the
 * file browser, so they are asked in their own small dialog. */
static bool saveProjectAs(QWidget *parent)
{
	Project *open = project();
	if (!open) {
		return false;
	}
	const QString start = open->path().empty() ? QString() : QString::fromStdString(open->path());
	const QString path =
		QFileDialog::getSaveFileName(parent, text("Project.SaveAs"), start, QStringLiteral("*.bstd"));
	if (path.isEmpty()) {
		return false;
	}

	QDialog options(parent);
	options.setWindowTitle(text("Project.SaveAs"));
	auto *layout = new QVBoxLayout(&options);
	auto *media = new QCheckBox(text("Project.Media"), &options);
	auto *streamKey = new QCheckBox(text("Project.StreamKey"), &options);
	layout->addWidget(media);
	layout->addWidget(streamKey);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &options);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &options, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &options, &QDialog::reject);
	layout->addWidget(buttons);
	if (options.exec() != QDialog::Accepted) {
		return false;
	}

	/* the key is carried by default; "Leave out the stream key" ticked writes the copy without
	 * it (P7) */
	std::string error;
	if (!open->saveAs(path.toStdString(), media->isChecked(), !streamKey->isChecked(), error)) {
		QMessageBox::warning(parent, text("Project.SaveAs"), QString::fromStdString(error));
		return false;
	}
	applyProjectTitle();
	return true;
}

/* The P2 guard before New or Open replaces the open show: Save, Discard or Cancel. True when it
 * is safe to go on, false to leave everything as it was. */
static bool confirmDiscard(QWidget *parent)
{
	Project *open = project();
	if (!open || !open->changed()) {
		return true;
	}
	const QMessageBox::StandardButton answer =
		QMessageBox::warning(parent, text("Project"), text("Project.Unsaved"),
				     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
	if (answer == QMessageBox::Discard) {
		return true;
	}
	if (answer == QMessageBox::Save) {
		return saveProject(parent);
	}
	return false; /* Cancel, or the dialog was dismissed */
}

static void openProject(QWidget *parent, const QString &path)
{
	Project *open = project();
	if (!open || path.isEmpty() || !confirmDiscard(parent)) {
		return;
	}
	std::string error;
	if (!open->open(path.toStdString(), error)) {
		QMessageBox::warning(parent, text("Project.Open"), QString::fromStdString(error));
		return;
	}
	applyProjectTitle();
}

/* Import vMix Preset… (V2/V3): read one .vmix and build it as a new, unsaved Project, leaving the
 * open Project's file alone. The read comes before the Project is replaced, so a file that is not
 * a preset leaves the open show untouched (spec §8); the P2 prompt comes first, because the import
 * replaces the open show exactly as New does. */
static void importVmixPreset(QWidget *parent)
{
	Project *open = project();
	if (!open || !confirmDiscard(parent)) {
		return;
	}

	const QString path =
		QFileDialog::getOpenFileName(parent, text("Project.ImportVmix"), QString(), QStringLiteral("*.vmix"));
	if (path.isEmpty()) {
		return;
	}

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(parent, text("Project.ImportVmix"), file.errorString());
		return;
	}

	VmixPresetResult result;
	std::string error;
	if (!readVmixPreset(file.readAll().toStdString(), result, error)) {
		/* the reader's own message, not a generic one, so the operator can tell what
		 * the file was and how it failed (spec §7) */
		QMessageBox::warning(parent, text("Project.ImportVmix"), QString::fromStdString(error));
		return;
	}

	/* a fresh Project (V3) first. If OBS refuses to make the profile or the scene
	 * collection, the preset must not be built: building would append its inputs to
	 * the operator's open show and a later Save would overwrite their file with it. So
	 * a fresh Project that could not be made refuses the import and is surfaced through
	 * the reader's own message path, exactly as a read that failed is (§8: the open
	 * Project is left untouched and nothing is created). */
	if (!open->createNew(error)) {
		QMessageBox::warning(parent, text("Project.ImportVmix"), QString::fromStdString(error));
		return;
	}

	/* the preset built into it through the desk's own InputModel. The desk's bar scrolls
	 * and holds every input (S1 §3), so it has no limit to pass: 0 means no bar, and V5's
	 * over-limit split has nothing to trim here. */
	buildVmixPreset(result, 0);

	/* §5.2's stream, output and recording settings are carried by nobody: the reader
	 * reads none, the builder applies none, and vMix's own settings (its stream
	 * destinations, its recording path, its external outputs) have no honest OBS
	 * equivalent to map, so the new Project keeps OBS's defaults. Reported rather than
	 * left silent (V4), so the operator is told instead of finding out on air. */
	result.report.push_back({"", "vMix stream, output and recording settings",
				 "not imported: no OBS equivalent is mapped; the new Project keeps OBS's defaults"});

	/* the report is raised once, whatever the build produced: an empty report still says
	 * the whole preset came across (§5.3). It must delete itself on close, as the desk's
	 * other windows do (InputBar::openStreamWindow), or every import leaks a window. */
	auto *report = new VmixReport(result.report, parent);
	report->setAttribute(Qt::WA_DeleteOnClose);
	report->show();
}

/* The desk's own Project menu (P3), built once with the ☰ and added before File. Every action
 * reads the one Project when it runs, because the Project is made after obs_module_load and
 * released at EXIT. */
static QMenu *buildProjectMenu(QWidget *parent)
{
	auto *menu = new QMenu(text("Project"), parent);

	QAction *newAction = menu->addAction(text("Project.New"));
	QObject::connect(newAction, &QAction::triggered, [parent] {
		if (!project() || !confirmDiscard(parent)) {
			return;
		}
		std::string error;
		if (!project()->createNew(error)) {
			/* New cannot make the fresh Project; the open show is untouched, so the
			 * only thing left is to say why (spec §8) */
			QMessageBox::warning(parent, text("Project.New"), QString::fromStdString(error));
			return;
		}
		applyProjectTitle();
	});

	QAction *openAction = menu->addAction(text("Project.Open"));
	openAction->setShortcut(QKeySequence::Open);
	QObject::connect(openAction, &QAction::triggered, [parent] {
		const QString path =
			QFileDialog::getOpenFileName(parent, text("Project.Open"), QString(), QStringLiteral("*.bstd"));
		openProject(parent, path);
	});

	QAction *saveAction = menu->addAction(text("Project.Save"));
	saveAction->setShortcut(QKeySequence::Save);
	QObject::connect(saveAction, &QAction::triggered, [parent] { saveProject(parent); });

	QAction *saveAsAction = menu->addAction(text("Project.SaveAs"));
	QObject::connect(saveAsAction, &QAction::triggered, [parent] { saveProjectAs(parent); });

	QAction *importAction = menu->addAction(text("Project.ImportVmix"));
	QObject::connect(importAction, &QAction::triggered, [parent] { importVmixPreset(parent); });

	menu->addSeparator();

	/* the recent list changes as Projects are opened and written, so it is rebuilt every time
	 * the submenu is shown (P8) */
	QMenu *recentMenu = menu->addMenu(text("Project.Recent"));
	QObject::connect(recentMenu, &QMenu::aboutToShow, recentMenu, [recentMenu, parent] {
		recentMenu->clear();
		Project *open = project();
		if (!open) {
			return;
		}
		for (const std::string &path : open->recent()) {
			QAction *item = recentMenu->addAction(QString::fromStdString(path));
			QObject::connect(item, &QAction::triggered,
					 [parent, path] { openProject(parent, QString::fromStdString(path)); });
		}
	});

	QAction *revealAction = menu->addAction(text("Project.Reveal"));
	QObject::connect(revealAction, &QAction::triggered, [parent] {
		const QString folder = projectFolder();
		if (!folder.isEmpty()) {
			QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
		}
	});

	return menu;
}

bool confirmProjectClose(QWidget *parent)
{
	/* the same wording and the same three outcomes as New and Open use before they replace the
	 * open show */
	return confirmDiscard(parent);
}

void applyProjectTitle()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main) {
		return;
	}

	/* OBS owns the base title and re-sets it as it likes; the desk only owns the dot. Keep the
	 * base and strip any dot first, so applying it twice does not leave two. */
	const QString dot = QString::fromUtf8(" \xE2\x80\xA2"); /* " " + U+2022 */
	QString title = main->windowTitle();
	while (title.endsWith(dot)) {
		title.chop(dot.size());
	}

	const Project *open = project();
	if (open && open->changed()) {
		title += dot;
	}
	/* Setting the same title again would emit windowTitleChanged, which re-applies the dot; stop
	 * when nothing would change, so the signal settles instead of looping. */
	if (title != main->windowTitle()) {
		main->setWindowTitle(title);
	}
}

void followTitleBar()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main) {
		blog(LOG_WARNING, "[bread-desk] the main window was not found; the Project's dot is not followed");
		return;
	}
	QObject::connect(main, &QMainWindow::windowTitleChanged, main, [] { applyProjectTitle(); });
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

		/* the desk's own Project menu, before OBS's File (P3) */
		popup->addMenu(buildProjectMenu(main));

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
			for (const char *itemName : DroppedFileItems) {
				QAction *item = main->findChild<QAction *>(itemName);
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
