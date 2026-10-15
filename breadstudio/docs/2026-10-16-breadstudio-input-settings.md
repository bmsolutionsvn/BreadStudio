# BreadStudio — Input Settings window (vMix-shaped) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The window behind a tile's ⚙ becomes vMix's Input Settings: a named title, a left list of pages, the chosen page on the right, and a `Copy From…` / `Close` row — with only the pages something in BreadStudio or OBS can back.

**Architecture:** `InputSettings` becomes a shell — `QListWidget` (pages) + `QStackedWidget` (page widgets) + a bottom row. A page is one `QWidget` built from the input, added to the stack only when the input's type backs it. The General page's controls write the desk's private settings (`bread_afv`, `bread_category`, `bread_click`, the automatic behaviours) and the base's OBS settings; the colour/effect pages and Advanced embed an OBS filter's or the base's own properties through OBS's reusable properties view (`shared/properties-view/OBSPropertiesView`), the same one the Input Select page already hosts. The category tint reaches the tile (`TileDisplay`) and the MultiView slot (`MultiviewSource`), and the mouse-click action reaches `InputBar`'s tile click.

**Tech Stack:** C++17, Qt 6 Widgets, libobs and obs-frontend-api from OBS Studio 32.2.2, OBS's `OBSPropertiesView`, CMake (OBS's `windows-x64` preset), Visual Studio 2026, Windows PowerShell 5.1.

**Spec:** `breadstudio/docs/2026-10-16-breadstudio-input-settings-design.md`. Read it before any task; §3 is the window, §3.1 the page set, §3.2 what each control changes.

**Needs first:** S1 (the desk) and Plan D merged (for `MultiviewSourceId`). Read `breadstudio/desk/{InputSettings,InputSelect,InputModel,InputBar,Mixer,TileDisplay,AudioBed}.hpp/.cpp`, `breadstudio/desk/MultiviewSource.cpp`, and `shared/properties-view/OBSPropertiesView.hpp` as they are on `main` before those tasks.

## What reading the code changed

Checked on 2026-10-16 against OBS 32.2.2 on `main`:

| Found | Consequence |
|---|---|
| OBS ships a reusable properties panel, `OBSPropertiesView` (`shared/properties-view/`); the Input Select window already hosts it to show a source's settings before it exists. | A filter page or the Advanced page is that widget for the filter/base, not new controls — the page is small. |
| The desk's AFV is `AudioBed::afv`/`setAfv`; the mixer strip and the tile already toggle it. | "Automatically mix audio" is the AFV toggle; the General page sets the same private key. |
| The desk renames through `renameInput()` (refuses a taken name); the base's settings go through `obs_source_update`. | The General page's Name reuses the former; Deinterlacing and Restart with transition reuse the latter. |
| `TileDisplay` draws each tile's header colour and `MultiviewSource` each slot's frame from `Desk.hpp`'s `ProgramColor`/`PreviewColor`/`IdleColor`. | The category tint is one more colour input to those two draws. |
| `InputBar`'s tile click always calls `toPreview`. | The mouse-click action is a read of a private key at that one call site. |

## Global Constraints

- Base: OBS Studio 32.2.2. New code goes under `breadstudio/desk`. No OBS file is edited by this plan.
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1` passes before every commit.
- Commit messages: `breadstudio: <what changed>`. Never rebase or force-push `main`. Nothing is pushed: every `git push` is an owner gate.
- C++17, warnings are errors (`/W3 /WX /permissive-`); OBS's code style: tabs, 120 columns, braces on every `if`.
- Every string the operator sees is in `breadstudio/desk/data/locale/en-US.ini` (`desk::text`).
- The window is not modal (drags into Preview must work). Windows x64 only; build with `cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk` from a VS 2026 dev shell, obs64 closed.

## Review Focus

1. **The shape.** The gear opens the window titled `Input: N <name>` with the page list of §3.1 and the input's General page shown. Owned by Tasks 1 and 5.
2. **Only backed pages.** A source type with no aspect-ratio or deinterlacing setting shows neither control; a filter page whose plugin is absent is not listed. Owned by Tasks 1, 3 and 5.
3. **The controls do what they say.** Rename, Category, AFV and Mouse-Click each take effect on the tile/mixer and survive a reload. Owned by Tasks 2 and 5.
4. **Copy From… copies desk settings only.** Owned by Tasks 2 and 5.
5. **Nothing regresses.** The layers list still drives layers; the filter pages do not disturb the base. Owned by Tasks 3, 4 and 5.

## File Structure

| Path | Responsibility |
|---|---|
| `breadstudio/desk/InputSettings.hpp`, `.cpp` | The shell: title, page list, page stack, bottom row |
| `breadstudio/desk/InputPages.hpp`, `.cpp` | One builder per page, and `pagesFor()` (which pages an input gets) |
| `breadstudio/desk/InputModel.hpp`, `.cpp` | `category()`, `setCategory()`, `clickAction()`, `setClickAction()`, `copyDeskSettings()` |
| `breadstudio/desk/TileDisplay.cpp` | The category tint on a tile |
| `breadstudio/desk/MultiviewSource.cpp` | The category tint on a slot |
| `breadstudio/desk/InputBar.cpp` | The tile click follows the input's action |
| `breadstudio/desk/CMakeLists.txt`, `data/locale/en-US.ini` | New sources, strings |
| `breadstudio/docs/DESK-SMOKE.md` | Section I |

---

### Task 1: The shell, and the General page

**Files:**
- Modify: `breadstudio/desk/InputSettings.hpp`, `.cpp`, `CMakeLists.txt`, `data/locale/en-US.ini`
- Create: `breadstudio/desk/InputPages.hpp`, `.cpp`

**Interfaces:**
- Produces:
  - `InputSettings` keeps `InputSettings(obs_source_t*, QWidget*)`, `input()`, `reload()`.
  - In `InputPages.hpp`: `struct InputSettingPage { QString title; QWidget *widget; };` and `std::vector<InputSettingPage> pagesFor(obs_source_t *input, InputSettings *window);` returning the pages §3.1 lists **for that input**, in order. Each builder may return `nullptr` when the input's type does not back it. The struct is named `InputSettingPage`, not `InputPage`, so it does not clash with the existing `class InputPage` in `InputPage.hpp` (both in `namespace desk`).

- [ ] **Step 1: The page-list helper**

Create `breadstudio/desk/InputPages.hpp`:
```cpp
#pragma once

#include <obs.hpp>

#include <QString>
#include <QWidget>

#include <string>
#include <vector>

class QVBoxLayout;

namespace desk {

class InputSettings;

/* one page of the Input Settings window */
struct InputSettingPage {
	QString title;
	QWidget *widget = nullptr; /* owned by the window's stack */
};

/* The pages an input gets, in order (inputs spec §3.1). A page whose backing is absent is
 * left out. `window` is the shell, for the pages that must close or reload it. */
std::vector<InputSettingPage> pagesFor(obs_source_t *input, InputSettings *window);

/* the General page's helpers, also used by Copy From… */
std::string categoryOf(obs_source_t *input);
void setCategory(obs_source_t *input, const std::string &colour);

} // namespace desk
```

- [ ] **Step 2: The General page**

In `InputPages.cpp`, build the General page: a `QFormLayout` with Name (`QLineEdit`, on `editingFinished` calls `renameInput`), Deinterlacing, Category (a row of colour swatch buttons), `Automatically mix audio` (= AFV) and, where the base backs it, `Restart with transition`, Mouse Click Action (`QComboBox`: None/Preview/Program), read-only Resolution and Frame Rate labels, and an `Audio Settings…` button that opens OBS's Advanced Audio for the base.

Aspect Ratio is not an OBS source property, so no such control is shown — do not invent a key. Deinterlacing is a source-level setting, not a property: list the combo only when the base is an async-video source (`obs_source_get_output_flags(base) & OBS_SOURCE_ASYNC_VIDEO`), take the options from the `OBS_DEINTERLACE_MODE_*` enum, the current value from `obs_source_get_deinterlace_mode(base)`, and write a change with `obs_source_set_deinterlace_mode(base, mode)`.

`Restart with transition` is the base's own `restart_on_activate` bool property, which the media and slideshow sources have: show the checkbox only when `obs_source_properties(base)` + `obs_properties_get` find it, read it from the base's settings, and write it back with `obs_source_update`. Play and Pause with transition are the input's scene-item behaviour, not a source setting, so they are not shown.

Add the locale keys the page needs (`Settings.Title`, `Settings.Page.General`, `Settings.Name`, `Settings.Deinterlacing` and its `Settings.Deinterlacing.<Mode>` names, `Settings.Category`, `Settings.Afv`, `Settings.Restart`, `Settings.Click`, `Settings.Click.None/Preview/Program`, `Settings.Resolution`, `Settings.FrameRate`, `Settings.AudioSettings`, `Settings.CopyFrom`, `Settings.Close`) to `breadstudio/desk/data/locale/en-US.ini`.

The other page titles (`Settings.Page.Position`, `…ColourAdjust`, `…ColourKey`, `…ColourCorrection`, `…Effects`, `…Layers`, `…Triggers`, `…Advanced`) are added now too, and `pagesFor` returns just General for this task — the rest arrive in Tasks 3 and 4.

- [ ] **Step 3: Rewrite the shell**

Rewrite `InputSettings.hpp/.cpp`: constructor builds the title (`Input: %1 %2` where `%1` is the input's 1-based index in `inputs()` and `%2` its name), a `QListWidget` on the left, a `QStackedWidget` on the right filled from `pagesFor(input, this)`, and a bottom `QHBoxLayout` with `Copy From…` (wired in Task 2) and `Close`. Keep it non-modal, keep `input()` and `reload()` (rebuild the layer page when it arrives; for now clear and rebuild the stack), and keep the existing "close when the input is deleted" behaviour in `InputBar::refresh()`.

- [ ] **Step 4: Build and check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk
```
Expected: builds with no warning. The gear opens the vMix-shaped window with the General page, a live page list, and `Copy From…`/`Close`; the title names the input.

- [ ] **Step 5: Commit**

```powershell
git add breadstudio/desk
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: shape the input settings window like vMix, with a General page"
```

---

### Task 2: Category, mouse-click action, and Copy From…

**Files:**
- Modify: `breadstudio/desk/InputModel.hpp`, `.cpp`, `InputPages.cpp`, `InputSettings.cpp`, `TileDisplay.cpp`, `MultiviewSource.cpp`, `InputBar.cpp`, `data/locale/en-US.ini`

**Interfaces:**
- Produces, in `InputModel.hpp`:
  - `std::string category(obs_source_t *input);` and `void setCategory(obs_source_t *input, const std::string &hex);` — private settings key `bread_category`, a `#rrggbb` string, empty for none.
  - `enum class ClickAction { None, Preview, Program };`, `ClickAction clickAction(obs_source_t *input);`, `void setClickAction(obs_source_t *input, ClickAction);` — key `bread_click`, default Preview.
  - `void copyDeskSettings(obs_source_t *to, obs_source_t *from);` — copies `bread_afv`, `bread_category`, `bread_click` and the automatic behaviours' keys.

- [ ] **Step 1: The model**

Add the category and click-action helpers (private settings, as `AudioBed` does for AFV), and `copyDeskSettings`.

- [ ] **Step 2: The General page uses them**

Wire the General page's Category swatches and Mouse Click Action to the model, and the `Copy From…` button: a small dialog listing `inputs()` (the input itself excluded), calling `copyDeskSettings(chosen, thisInput)` then `reload()`.

- [ ] **Step 3: The tints**

- `TileDisplay.cpp`: where the tile's header/`frames` colour is chosen, use the input's `category()` when it is non-empty, else the tally colour.
- `MultiviewSource.cpp`: in the render's per-input frame choice, prefer the category colour when set.

- [ ] **Step 4: The tile click**

`InputBar.cpp`: the tile click (around line 246-250) calls `toPreview` today; make it follow `clickAction(input)` — None does nothing, Preview is `toPreview`, Program is `cutToProgram`.

- [ ] **Step 5: Build, check by hand, commit**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: let an input carry a category colour and a click action"
```
Hand-check: setting a category tints the tile and the MultiView slot and survives a reload; the click action changes what a click does; `Copy From…` copies the desk settings from another input.

---

### Task 3: Position, and the colour and effect pages

**Files:**
- Modify: `breadstudio/desk/InputPages.cpp`, `data/locale/en-US.ini`

**Interfaces:**
- Consumes: OBS's `OBSPropertiesView` (as `InputSelect` hosts it) and OBS's filter API (`obs_source_add_filter`/`obs_source_filter_add`, `obs_source_get_filter_by_name`, `obs_properties_create_filter`-style creation).
- Produces: the Position page (the base's scene-item transform) and the Colour Adjust / Colour Key / Colour Correction / Effects pages, each backed by an OBS filter on the base, listed only when the filter's type is registered.

- [ ] **Step 1: Position**

Build the Position page from the base's transform: X, Y, scale, rotation and crop, reading and writing `obs_sceneitem_get_pos`/`set_pos`, `get_/set_scale`, `get_/set_rot`, `get_/set_crop`. Use `baseItem(input)` from `InputModel.hpp` to reach the scene item.

- [ ] **Step 2: One page per colour/effect filter**

Write one builder that, for a given filter id and page title, finds or creates that filter on the base and hosts its properties:
- Colour Adjust → `color_filter_v2`
- Colour Key → `chroma_key_filter_v2`
- Colour Correction → `color_correction_filter`
- Effects → the base's filter **list** plus each filter's properties (add/remove).

List a page only when OBS has the filter type (`obs_source_get_display_name(id)` non-null), as the design says. Host the filter's settings through `OBSPropertiesView` (create it over the filter's properties), so the controls are OBS's.

- [ ] **Step 3: Build, check by hand, commit**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the Position, colour and effect pages to the input window"
```
Hand-check: each colour page adds, configures and removes its filter on the base and the picture changes live; Position moves the base within its input.

---

### Task 4: Layers / MultiView, Triggers, Advanced

**Files:**
- Modify: `breadstudio/desk/InputPages.cpp`, `InputSettings.cpp`, `data/locale/en-US.ini`

**Interfaces:**
- Consumes: the desk's layers API (`layers()`, `addLayer`, `removeLayer`, `moveLayer`, `setLayerVisible` in `InputModel.hpp`); OBS's base properties view and hotkeys; `MultiviewSourceId`.

- [ ] **Step 1: Layers / MultiView**

Move the current window's layers list into a page: a `QListWidget` of `layers(input)` with hidden/shown, up/down and remove, and a small live view of the input — reuse `TileDisplay` (already draws one input) sized to a slot.

- [ ] **Step 2: Triggers**

A page for the subset §3.2 allows: a row per trigger (on Program, on Preview, on click), each a combo of the action (Nothing, Start recording, Stop recording, Start stream, Stop stream, Send to Preview, Send to Program). Keep the rules in the input's private settings (a JSON object, key `bread_triggers`), and run them from the desk: on a scene change (`desk.cpp`'s `SCENE_CHANGED`/`PREVIEW_SCENE_CHANGED`) and on a tile click (`InputBar`).

- [ ] **Step 3: Advanced**

The base's own Advanced settings through `OBSPropertiesView` (sync offset, hotkeys), plus a read-only line naming what BreadStudio does not offer (PTZ, Tally Lights, Virtual Input), so the absence is explained.

- [ ] **Step 4: Build, check by hand, commit**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the Layers, Triggers and Advanced pages to the input window"
```

---

### Task 5: The smoke checklist, and the run

**Files:**
- Modify: `breadstudio/docs/DESK-SMOKE.md`

- [ ] **Step 1: Add section I**

Add a section, in the existing form, whose items are the design's §6 hand-checks: the window's shape and page list per type; rename/category/AFV/click taking effect and surviving a reload; each filter page adding and removing its filter; `Copy From…`; a type with no aspect-ratio control showing none.

- [ ] **Step 2: Run the automated tests**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-multiview-test bread-outputs-test bread-stream-test bread-sources-test bread-desk-test bread-omt-test bread-onair-test
foreach ($t in 'desk\test-bin\RelWithDebInfo\bread-multiview-test','desk\test-bin\RelWithDebInfo\bread-outputs-test','desk\test-bin\RelWithDebInfo\bread-stream-test','desk\test-bin\RelWithDebInfo\bread-sources-test','desk\test-bin\RelWithDebInfo\bread-desk-test','omt-hx\test-bin\RelWithDebInfo\bread-omt-test','omt-hx\test-bin\RelWithDebInfo\bread-onair-test') { & "build_x64\breadstudio\$t.exe" | Select-Object -Last 1; "exit $LASTEXITCODE" }
```
Expected: every one ends `exit 0`.

- [ ] **Step 3: Run section I, and record** [GUI, owner] — no `## Runs` entry from the implementer.

- [ ] **Step 4: Commit**

```powershell
git add breadstudio/docs
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the input settings window to the smoke checklist, and record the run"
```

---

## Left for later

| Item | Where |
|---|---|
| A vMix-style top bar replacing OBS's menu bar (so Settings is reachable) | Its own spec |
| Mix inputs, their window, the on-air rule's extra step | The outputs plan's Plan G |