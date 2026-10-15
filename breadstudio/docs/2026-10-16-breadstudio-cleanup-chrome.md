# Hiding OBS's surface — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** BreadStudio's reachable surface becomes the desk alone — the ☰ menu plus the desk's docks — with OBS's Edit/View/Docks/Tools/Help menus and its extra docks gone, nothing deleted.

**Architecture:** One new desk unit, `DeskChrome`, holds the table of OBS object names to hide and applies it at `FINISHED_LOADING` and after each scene-collection load. The ☰ menu builder in `DeskLayout` is narrowed to the menus the desk keeps. The desk's docks are made non-closable. No OBS source file is edited, so upstream merges see nothing new.

**Tech Stack:** C++20, Qt 6 Widgets, the OBS frontend API, libobs. Part of the `bread-desk` plugin (`breadstudio/desk/`).

**Spec:** `breadstudio/docs/2026-10-16-breadstudio-cleanup-design.md` §5 (what is hidden), §6 (how), K1–K10.

**Scope:** this is plan 1 of 3. Plan 2 is the Project (`.bstd`), plan 3 the vMix import. Each ships working software alone; this one only hides.

**Base:** `BreadStudio` at `main`, tip `1ab25399`. Working tree clean.

---

## Environment

- Repo: `F:\Coding\BreadMedia\BreadStudio`, branch `main`, all work on `main`.
- Shell: Windows PowerShell 5.1. Close the app before building: `Stop-Process -Name obs64 -Force -ErrorAction SilentlyContinue`.
- Build one target (cmake/cl are not on PATH; the VS dev shell is invoked inline):
  ```
  cmd.exe /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk'
  ```
  Expected: `bread-desk.vcxproj -> ...\RelWithDebInfo\bread-desk.dll`. Warnings are errors.
- clang-format every changed file, then verify (never route a file through `Set-Content`/`Out-File`):
  ```
  & "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe" -i --style=file <file>
  & "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe" --dry-run --Werror --style=file <file>
  ```
  Expected exit 0. Tabs, ≤120 columns, no BOM, a newline at end of file.
- `powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1` must exit 0 before every commit. Expected: `UPSTREAM-EDITS.md matches the 3 file(s) changed since 32.2.2.`
- Run the desk tests: `build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-desk-test.exe` (and `bread-stream-test`, `bread-sources-test`, `bread-outputs-test`, `bread-multiview-test`), each exit 0.
- No GUI check is run by the implementer. Rows belong in `breadstudio/docs/DESK-SMOKE.md`; the owner runs them. Do not write a `## Runs` entry.

**There is no automated test for this work.** Every check is a widget lookup in a live OBS window, which the desk's test programs (no OBS, no Qt) cannot reach. So each task's verification is: it builds warning-free, `check-edits.ps1` passes, the desk tests still pass, and a smoke row is added for the owner. That is deliberate, not a gap to fill with a fake test.

---

## File structure

| File | Responsibility |
|---|---|
| `breadstudio/desk/DeskChrome.hpp` (create) | Declares `applyChrome()`. |
| `breadstudio/desk/DeskChrome.cpp` (create) | The table of OBS object names to hide, the keep-list of the desk's own docks, and the dock-feature rule (K10). |
| `breadstudio/desk/CMakeLists.txt` (modify) | Adds the two new files to `bread-desk`. |
| `breadstudio/desk/desk.cpp` (modify) | Calls `applyChrome()` at `FINISHED_LOADING` and on `SCENE_COLLECTION_CHANGED`. |
| `breadstudio/desk/DeskLayout.cpp` (modify) | Narrows `obsMainMenu()` to the menus the desk keeps. |
| `breadstudio/desk/data/locale/en-US.ini` (modify) | The `Projector` menu label. |
| `breadstudio/docs/DESK-SMOKE.md` (modify) | A new section J: the chrome rows. |

`DeskLayout.cpp` already hides OBS's menu bar, the Scenes/Sources toolbars, the Scenes/Sources/Audio-Mixer/Controls docks. `DeskChrome` takes over the **docks** so one table holds them all in one place, and leaves the toolbars and menu bar where they are.

---

### Task 1: `DeskChrome` hides OBS's docks

**Files:**
- Create: `breadstudio/desk/DeskChrome.hpp`
- Create: `breadstudio/desk/DeskChrome.cpp`
- Modify: `breadstudio/desk/CMakeLists.txt`
- Modify: `breadstudio/desk/desk.cpp`
- Modify: `breadstudio/desk/DeskLayout.cpp:63-68`

- [ ] **Step 1: Create `breadstudio/desk/DeskChrome.hpp`**

```cpp
#pragma once

namespace desk {

/* Hides the OBS surface the desk does not use (cleanup spec §5/§6): OBS's extra docks, and any
 * dock a loaded plugin adds. Nothing is deleted, and a name that is not found is logged and
 * skipped, never fatal (K8). Applied at load, and again after a scene collection loads, because
 * loading one re-adds plugin docks (K7). UI thread only. */
void applyChrome();

} // namespace desk
```

- [ ] **Step 2: Create `breadstudio/desk/DeskChrome.cpp`**

```cpp
#include "DeskChrome.hpp"
#include "Desk.hpp"

#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QDockWidget>

#include <cstring>

namespace desk {

/* Every OBS dock the desk hides, by object name (cleanup spec §5). OBS's Scene Transitions
 * (transitionsDock) is NOT here: it is the desk's own transition control and the layout places
 * it beside the mixer. */
static const char *const HiddenDocks[] = {
	"scenesDock",
	"sourcesDock",
	"mixerDock",
	"controlsDock",
	"statsDock",
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
		auto *dock = main->findChild<QDockWidget *>(name);
		if (!dock) {
			blog(LOG_WARNING, "[bread-desk] dock '%s' was not found; it was left alone", name);
			continue;
		}
		dock->hide();
	}

	/* a dock a plugin added after the desk's own were made (cleanup spec §5): everything that
	 * is a dock and is not one of ours or the desk's transition control is hidden */
	for (QDockWidget *dock : main->findChildren<QDockWidget *>()) {
		const QByteArray name = dock->objectName().toUtf8();
		bool keep = false;
		for (const char *kept : UndismissableDocks) {
			keep = keep || strcmp(name.constData(), kept) == 0;
		}
		if (keep) {
			continue;
		}
		dock->hide();
	}

	for (const char *name : UndismissableDocks) {
		auto *dock = main->findChild<QDockWidget *>(name);
		if (!dock) {
			blog(LOG_WARNING, "[bread-desk] dock '%s' was not found; it keeps its close button", name);
			continue;
		}
		dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
	}
}

} // namespace desk
```

- [ ] **Step 3: Add the files to `breadstudio/desk/CMakeLists.txt`**

In the `target_sources(bread-desk PRIVATE ...)` list, insert two lines before `DeskLayout.cpp` (the list is alphabetical):

```
    DeskChrome.cpp
    DeskChrome.hpp
```

- [ ] **Step 4: Remove the docks `DeskChrome` now owns from `DeskLayout.cpp`**

In `applyLayout()`, delete the four lines that hide docks, so the dock hiding lives in one place:

```cpp
	scenes->setVisible(false);
	sources->setVisible(false);
	obsMixer->setVisible(false);
	/* OBS's Controls dock carries Studio Mode, Recording, Virtual Cam and Settings; the
	 * desk replaces all of it, so it is hidden, not laid out */
	controls->setVisible(false);
```

Leave the `find("scenesDock")` … `find("transitionsDock")` lookups and the `if (!inputs || ...)` guard exactly as they are — the guard still proves the layout's dependencies are present.

- [ ] **Step 5: Call `applyChrome()` from `desk.cpp`**

Add `#include "DeskChrome.hpp"` with the other includes (after `#include "DeskLayout.hpp"`).

In `onEvent`, in `case OBS_FRONTEND_EVENT_FINISHED_LOADING:`, after `hideObsStreamButton();`:

```cpp
		applyChrome();
```

In `case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:`, after `load();`:

```cpp
		applyChrome();
```

- [ ] **Step 6: Format, build, gate**

```powershell
cd F:\Coding\BreadMedia\BreadStudio
Stop-Process -Name obs64 -Force -ErrorAction SilentlyContinue
& "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe" -i --style=file breadstudio\desk\DeskChrome.cpp breadstudio\desk\DeskChrome.hpp breadstudio\desk\desk.cpp breadstudio\desk\DeskLayout.cpp
& "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe" --dry-run --Werror --style=file breadstudio\desk\DeskChrome.cpp breadstudio\desk\DeskChrome.hpp breadstudio\desk\desk.cpp breadstudio\desk\DeskLayout.cpp
cmd.exe /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk'
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
```

Expected: clang-format exit 0; `bread-desk.vcxproj -> ...\bread-desk.dll` with no warnings; `check-edits.ps1` exit 0.

- [ ] **Step 7: Run the desk tests**

```powershell
$d="F:\Coding\BreadMedia\BreadStudio\build_x64\breadstudio\desk\test-bin\RelWithDebInfo"
& "$d\bread-desk-test.exe"; & "$d\bread-stream-test.exe"; & "$d\bread-sources-test.exe"; & "$d\bread-outputs-test.exe"; & "$d\bread-multiview-test.exe"
```

Expected: each prints its `all checks passed` line, exit 0.

- [ ] **Step 8: Commit**

```powershell
git add breadstudio/desk/DeskChrome.cpp breadstudio/desk/DeskChrome.hpp breadstudio/desk/CMakeLists.txt breadstudio/desk/desk.cpp breadstudio/desk/DeskLayout.cpp
git commit -m "breadstudio: hide OBS's docks from one table in the desk" -m "DeskChrome holds the docks OBS has and the desk does not use - Scenes, Sources, its Audio Mixer, Controls and Stats - plus any dock a plugin adds, and hides them at load and after each scene collection load. It also makes the desk's docks, and OBS's Scene Transitions which is the desk's transition control, movable and floatable but not closable, as vMix. applyLayout keeps placing the docks it shows; the hiding now lives in one place."
```

---

### Task 2: The ☰ keeps only the desk's menus

**Files:**
- Modify: `breadstudio/desk/DeskLayout.cpp:110-135`
- Modify: `breadstudio/desk/data/locale/en-US.ini`

- [ ] **Step 1: Narrow `obsMainMenu()`**

Replace the whole of `obsMainMenu()` in `breadstudio/desk/DeskLayout.cpp` with:

```cpp
/* The menus the ☰ keeps (cleanup spec §5). Edit, View, Docks, Tools and Help are dropped (K2):
 * the desks's surface is the desk, and those menus reach OBS's scene editor, docks and help. */
static const char *const KeptMenus[] = {
	"menu_File",
	"profileMenu",
	"sceneCollectionMenu",
};

/* The File entries that stay outside the desk's job (cleanup spec §5). */
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
			for (const char *dropped : DroppedFileItems) {
				if (QAction *item = menu->findChild<QAction *>(dropped)) {
					menu->removeAction(item);
				}
			}
			tidySeparators(menu);
			popup->addMenu(menu);
		}

		/* the desk keeps OBS's Multiview projector — the one part of View that puts a view
		 * on a local monitor (K5) — under its own Projector entry */
		if (QMenu *multiview = main->findChild<QMenu *>("multiviewProjectorMenu")) {
			popup->addMenu(text("Projector"))->addMenu(multiview);
		} else {
			blog(LOG_WARNING, "[bread-desk] the Multiview projector menu was not found; the projector is not offered");
		}

		bar->setVisible(false);
	}
	return popup;
}
```

Add to the top of the file, with the other includes:

```cpp
#include <QByteArray>
#include <QList>
```

Add `#include <QList>` only if the build says it is needed; `QList` comes with `QMenu`. `#include <cstring>` is needed for `strcmp`.

- [ ] **Step 2: Add the `Projector` locale key**

In `breadstudio/desk/data/locale/en-US.ini`, next to the existing `Menu="Menu"` line, add:

```
Projector="Projector"
```

- [ ] **Step 3: Format, build, gate**

```powershell
cd F:\Coding\BreadMedia\BreadStudio
Stop-Process -Name obs64 -Force -ErrorAction SilentlyContinue
& "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe" -i --style=file breadstudio\desk\DeskLayout.cpp
& "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe" --dry-run --Werror --style=file breadstudio\desk\DeskLayout.cpp
cmd.exe /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk'
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
```

Expected: exit 0, `bread-desk.dll`, check-edits exit 0.

- [ ] **Step 4: Commit**

```powershell
git add breadstudio/desk/DeskLayout.cpp breadstudio/desk/data/locale/en-US.ini
git commit -m "breadstudio: keep only the desk's own menus in the menu button" -m "The menu button no longer carries OBS's whole menu bar: it keeps File (with the four desk-less entries dropped and the separators tidied), Profile and Scene Collection, and OBS's Multiview projector under a Projector entry. Edit, View, Docks, Tools and Help are not added at all. The menus themselves are untouched, so nothing OBS does with them changes."
```

---

### Task 3: Smoke rows

**Files:**
- Modify: `breadstudio/docs/DESK-SMOKE.md`

- [ ] **Step 1: Append section J**

Add to the end of `breadstudio/docs/DESK-SMOKE.md`:

```markdown
## J. OBS's surface is hidden

Run after every upstream merge: these are object-name lookups into OBS's window,
and an upstream rename breaks them silently.

- [ ] The ☰ menu holds **File** (Settings…, Exit), **Profile**, **Scene
      Collection** and **Projector**, and nothing else. No Edit, View, Docks,
      Tools or Help.
- [ ] File holds no Show Recordings, Remux, Show Settings Folder or Show
      Profile Folder.
- [ ] Projector → Multiview opens a Multiview projector window.
- [ ] None of OBS's Scenes, Sources, Audio Mixer, Controls or Stats docks is
      visible, and the Docks menu does not exist to bring them back.
- [ ] The desk's input bar and Mixer docks are visible, and OBS's Scene
      Transitions dock is visible beside them.
- [ ] A desk dock can be dragged out to float on a second monitor, and has no
      close button; Scene Transitions has none either.
- [ ] Settings… opens and every page of it works.
- [ ] An input's properties and its filters open from the desk.
- [ ] `%APPDATA%\BreadStudio\logs` holds no `dock '...' was not found` warning.
```

- [ ] **Step 2: Commit**

```powershell
git add breadstudio/docs/DESK-SMOKE.md
git commit -m "breadstudio: add OBS's hidden-surface rows to the smoke checklist"
```

---

## Self-review

**Spec coverage.** K1 (hide only) — no OBS file is edited in any task. K2 (the menu) — Task 2. K3/K4 (Settings, source properties) — untouched, so they keep working; asserted by smoke rows. K5 (projector) — Task 2 Step 1. K6 (hidden docks) — Task 1. K7 (applied at load and after a collection load) — Task 1 Step 5. K8 (a miss is logged, not fatal) — Task 1 Step 2. K9 (no way to show it again) — nothing is added to bring a hidden dock back; the Docks menu is dropped in Task 2. K10 (dock features) — Task 1 Step 2.

**Not in this plan, by design:** P1–P9 (the Project) and V1–V7 (the vMix import), which are plans 2 and 3. §3's "Project" and "Import vMix Preset…" menu entries arrive with them; until then the ☰ keeps Profile and Scene Collection, which do the same job by hand.

**Placeholder scan:** no TBD, no "handle errors", no "similar to Task N". Every code step carries its code.

**Type consistency:** `applyChrome()` is declared in `DeskChrome.hpp` and defined in `DeskChrome.cpp`, called as `applyChrome()` (the file is inside `namespace desk`, and `desk.cpp` already has `using namespace desk;`). `InputsDockId` and `MixerDockId` come from `Desk.hpp`, already included by `DeskChrome.cpp`. `text()` comes from `Desk.hpp`, already included by `DeskLayout.cpp`.

**One risk to watch:** `applyChrome` hides **every** dock that is not in `UndismissableDocks`, so a dock the desk ever adds later must be named there or it will vanish. The comment in the file says so.