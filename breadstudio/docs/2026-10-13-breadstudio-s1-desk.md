# BreadStudio S1 — The Desk Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A vMix-style desk in BreadStudio: a bar of numbered inputs with live tiles, Preview and Program, Cut, layers, and a mixer with buses, so an operator runs a show without opening OBS's scene list.

**Architecture:** An input is an ordinary OBS scene with a fixed shape: a base source at the bottom and, above it, layers that are other inputs. OBS's Studio Mode is the Preview and Program engine and OBS's multiview is the multiview. One new plugin, `bread-desk`, adds an input bar dock and a mixer dock through the frontend API, and keeps one hidden scene on a spare output channel (the audio bed) so an input can be heard whatever is on Program.

**Tech Stack:** C++17, Qt 6 Widgets, libobs and obs-frontend-api from OBS Studio 32.2.2, CMake (OBS's `windows-x64` preset), Visual Studio 2026, Windows PowerShell 5.1.

**Spec:** `breadstudio/docs/2026-10-13-breadstudio-s1-desk-design.md`. Read it before any task. Section numbers below (§) refer to it.

## Status (2026-10-14)

All nine tasks are built and committed. The checks by hand in Tasks 4 to 8 were run for the first time on 2026-10-14, on the build folder; the first run found five defects, which are fixed in `f92157f57`:

| Found by | Defect | Fix |
|---|---|---|
| Task 5 Step 7 | "+ Add" opened no menu: the dock is made before the plugins that own the input types have loaded, so every type was skipped. | The menu is filled when it opens. |
| Task 4 Step 8 | A tile's Cut sometimes left the Cut transition selected: OBS refuses a change of transition until the audio side of the cut has stopped. | The restore waits for that. |
| Task 4 Step 8 | The buttons under the tiles were cut off. | The bar's scroll area has a minimum height. |
| Task 8 Step 5 | OBS's Scene Transitions dock sat beside the input bar, so the bar was not across the width. | The layout puts it between the mixer and Controls. |
| Task 6 Step 6 | An open settings window kept a layer after that layer's input was renamed or deleted. | The window reads its layers again when the inputs change. |

With those fixes every row of every table in Task 4 Step 8, Task 5 Step 7, Task 6 Step 6, Task 7 Step 6 and Task 8 Step 5 passes. The tables were worked by posted clicks, with OBS's state read back through its websocket, and two rows were not run as written:

- Task 5 Step 7, the hotkey row: "Input 2 to Preview" was triggered through OBS, not bound to a key and pressed.
- Task 7 Step 6, the microphone row: the new strip is there with AFV unlit and its source is live whatever is on Program, but nobody spoke into it.

The audio bed readings (Task 3 Step 8, Task 7 Step 6) are -12.0 dB on bus M for all three of A, B and C.

**Still open**

- Task 9 Step 4, the owner gate: `DESK-SMOKE.md` with two cameras, a microphone and a second monitor. The run of 2026-10-14 used stand-ins and is recorded in that file.
- The two things owed to S0, at the end of this plan.

**Seen, not changed** (each is OBS's own behaviour, or outside the spec):

- After Scene Collection → New in Studio Mode, OBS has no Program scene and no quick transition buttons until the first cut. Stock OBS 32.2.2 does the same.
- During a fade the bar keeps the old frames until the fade has finished; OBS's own state names the new Program from the start.
- An input's base keeps the name it was made with ("Media file source 2") after the input is renamed. It shows only in OBS's own windows, such as Advanced Audio Properties.
- The mixer's strips share their dock with the six bus columns. In a dock under about 400 px wide only one strip shows without scrolling.

## Global Constraints

- Base: OBS Studio tag `32.2.2`. Branch: `s1-desk` (already created from `main`). Local path `F:\Coding\BreadMedia\BreadStudio`.
- **Never rebase or force-push.** **Nothing is pushed by this plan.** Any `git push` is an owner gate; a worker stops and waits.
- Windows x64 only. macOS and Linux code is not built, not tested and not deleted.
- All plugin code lives in `breadstudio/desk/`. Exactly two OBS files are edited by this plan: `plugins/CMakeLists.txt` (Task 1) and `frontend/OBSApp.cpp` (Task 8). Each is added to `breadstudio/UPSTREAM-EDITS.md` in the same commit, in the form: a dash, the path in backticks, ` - `, the reason.
- Before every commit, in this order: `git add -A`, then `powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1` (must print that the list matches and exit 0), then `git commit`. The check reads the staged tree; an unstaged new file is invisible to it.
- Commit messages: `breadstudio: <what changed>`, imperative, lowercase after the prefix.
- All builds run from a Visual Studio 2026 developer shell: `& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64`, then from the repo root `.\breadstudio\build.ps1`. It configures and builds everything incrementally and must exit 0.
- **OBS builds with warnings as errors.** Cast every narrowing conversion explicitly (`size_t` to `int`, `double` to `float`), and name no unused parameter.
- **The C++ in this plan was written against the 32.2.2 headers and has not been compiled.** Where the compiler disagrees, make the smallest change that keeps the behaviour described in the step, and say what changed in the commit message. Do not redesign.
- C++ follows OBS style: tabs, 120 columns, braces on every `if`. Everything is in `namespace desk`.
- Log lines start with `[bread-desk]`.
- User-visible text comes from `breadstudio/desk/data/locale/en-US.ini` through `desk::text()`. No other locale file is added. No file under `frontend/data/locale/` or `plugins/*/data/locale/` is edited.
- The plugin may hold references to scenes and sources only between a scene collection finishing loading and starting to unload. Every unit has a `clear()` that drops all of them, called on `OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING`, `OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP` and `OBS_FRONTEND_EVENT_EXIT`. `clear()` is safe to call twice.
- Tally colours (D6): Program `0xFFD00000` (red), Preview `0xFF00D000` (green). These are the values OBS's multiview uses.
- Buses (D7): bus index 0 is Master (OBS track 1); 1 to 5 are A to E (tracks 2 to 6).
- **Running the build.** Until S0 Task 3 lands, the program is still named `obs64.exe`:

  ```powershell
  Start-Process -WorkingDirectory build_x64\rundir\RelWithDebInfo\bin\64bit build_x64\rundir\RelWithDebInfo\bin\64bit\obs64.exe -ArgumentList '--portable'
  ```

  `--portable` keeps settings inside the build folder. After S0 Task 3 the file is `BreadStudio.exe`; use whichever exists.
- **Reading the log.** Close the program first, then:

  ```powershell
  Get-ChildItem build_x64\rundir\RelWithDebInfo\config\obs-studio\logs | Sort-Object LastWriteTime | Select-Object -Last 1 | Get-Content | Select-String 'bread-desk|memory leaks|remaining'
  ```

## Review Focus

1. **The scene collection is switched, or the program is closed, while the desk holds references.** OBS must unload and close cleanly: the log ends with `Number of memory leaks: 0` and has no line about sources remaining. Pinned by Task 3 Step 9, Task 4 Step 9, Task 7 Step 7.
2. **A scene collection made in stock OBS:** scenes holding several ordinary sources, a group, a nested scene, and one empty scene. Each appears as an input and switches; the empty one has a black tile, a disabled ♪, and a disabled "Base properties" button. Nothing crashes. Pinned by Task 4 Step 8 and Task 6 Step 6.
3. **Cut on a tile while a transition is running, or on the input that is already on Program.** The click is ignored, and the transition selected in OBS's transition controls is the same afterwards as before. Pinned by Task 4 Step 8.
4. **Renaming an input to an empty name or to a name another source already has.** Refused with a message; the input keeps its name. Pinned by Task 5 Step 7.
5. **Deleting the last input, or an input that is on Program directly or as a layer of what is on Program.** Refused with a message. Pinned by Task 2 (the reachability test) and Task 5 Step 7.

---

## File Structure

New files, all under `breadstudio/desk/`:

| Path | Responsibility |
|---|---|
| `CMakeLists.txt` | Builds the `bread-desk` module and the `bread-desk-test` program. |
| `cmake/windows/obs-module.rc.in` | Windows version resource for the module. |
| `data/locale/en-US.ini` | Every user-visible string. |
| `Desk.hpp` | Dock ids, colours, `desk::text()`. |
| `desk.cpp` | Module entry. Routes frontend events to the units. Number hotkeys. |
| `LayerGraph.hpp`, `LayerGraph.cpp` | The loop check, with no OBS types. |
| `test/LayerGraphTest.cpp` | The one automated test. |
| `test/make-tone.ps1` | Writes the test tone for the audio bed check. |
| `InputModel.hpp`, `InputModel.cpp` | Inputs, bases, layers, create, rename, delete, order, to Preview, cut to Program. No widgets. |
| `AudioBed.hpp`, `AudioBed.cpp` | The hidden scene, its channel, the AFV flag. |
| `BusMeters.hpp`, `BusMeters.cpp` | Peak level of each of the six buses. |
| `TileDisplay.hpp`, `TileDisplay.cpp` | One display that draws every tile's picture and frame. |
| `InputBar.hpp`, `InputBar.cpp` | The input bar dock. |
| `InputSettings.hpp`, `InputSettings.cpp` | The ⚙ window. |
| `Meter.hpp`, `Meter.cpp` | A level bar widget. |
| `Mixer.hpp`, `Mixer.cpp` | The mixer dock. |
| `DeskLayout.hpp`, `DeskLayout.cpp` | First-run arrangement and the "Desk layout" menu item. |

Also new: `breadstudio/docs/DESK-SMOKE.md` (Task 9).

OBS files edited:

| Path | Edit | Task |
|---|---|---|
| `plugins/CMakeLists.txt` | One line that adds `breadstudio/desk` to the build. | 1 |
| `frontend/OBSApp.cpp` | Default of `SceneDuplicationMode`, true to false. | 8 |

Task order and why: 1 makes the plugin build and load. 2 is the pure loop check. 3 proves the audio bed with a test tone before any mixer interface exists (§5.3); if it fails, work stops for the owner. 4 to 6 build the input bar. 7 builds the mixer and removes Task 3's probe. 8 arranges the window. 9 writes the smoke checklist and runs the done-when.

---

### Task 1: The plugin builds and loads

**Files:**
- Create: `breadstudio/desk/CMakeLists.txt`
- Create: `breadstudio/desk/cmake/windows/obs-module.rc.in`
- Create: `breadstudio/desk/data/locale/en-US.ini`
- Create: `breadstudio/desk/Desk.hpp`
- Create: `breadstudio/desk/desk.cpp`
- Modify: `plugins/CMakeLists.txt` (append one line at the end)
- Modify: `breadstudio/UPSTREAM-EDITS.md`

**Interfaces:**
- Consumes: nothing.
- Produces: `desk::text(const char *key) -> QString`; `desk::InputsDockId`, `desk::MixerDockId`; `desk::ProgramColor`, `desk::PreviewColor`, `desk::IdleColor`; the CMake target `bread-desk`, to which later tasks add sources.

- [x] **Step 1: Write the CMake file**

`breadstudio/desk/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.28...3.30)

find_package(Qt6 REQUIRED Widgets)

add_library(bread-desk MODULE)
add_library(OBS::bread-desk ALIAS bread-desk)

target_sources(bread-desk PRIVATE Desk.hpp desk.cpp)

target_link_libraries(bread-desk PRIVATE OBS::frontend-api OBS::libobs Qt::Widgets)

if(OS_WINDOWS)
  configure_file(cmake/windows/obs-module.rc.in bread-desk.rc)
  target_sources(bread-desk PRIVATE bread-desk.rc)
endif()

set_target_properties_obs(
  bread-desk
  PROPERTIES FOLDER plugins/bread-desk
             PREFIX ""
             AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON
)
```

- [x] **Step 2: Write the version resource**

`breadstudio/desk/cmake/windows/obs-module.rc.in`:

```
1 VERSIONINFO
FILEVERSION ${OBS_VERSION_MAJOR},${OBS_VERSION_MINOR},${OBS_VERSION_PATCH},0
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904B0"
    BEGIN
      VALUE "CompanyName", "${OBS_COMPANY_NAME}"
      VALUE "FileDescription", "BreadStudio Desk"
      VALUE "FileVersion", "${OBS_VERSION_CANONICAL}"
      VALUE "ProductName", "${OBS_PRODUCT_NAME}"
      VALUE "ProductVersion", "${OBS_VERSION_CANONICAL}"
      VALUE "Comments", "${OBS_COMMENTS}"
      VALUE "LegalCopyright", "${OBS_LEGAL_COPYRIGHT}"
      VALUE "InternalName", "bread-desk"
      VALUE "OriginalFilename", "bread-desk"
    END
  END

  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x0409, 0x04B0
  END
END
```

- [x] **Step 3: Write every user-visible string**

`breadstudio/desk/data/locale/en-US.ini` (later tasks use these keys; none is added later):

```ini
Inputs="Inputs"
Mixer="Mixer"
Add="+ Add"
Cut="Cut"
StudioModeOff="Studio Mode is off. A click now cuts straight to Program."
StudioModeOn="Switch Studio Mode on"
Type.Camera="Camera"
Type.Media="Media file"
Type.Screen="Screen"
Type.Window="Window"
Type.Browser="Browser page"
Type.Image="Image"
Type.Colour="Colour"
Type.Blank="Blank"
Type.Audio="Audio device"
Rename="Rename"
Delete="Delete"
Rename.Prompt="New name"
Rename.Taken="That name is empty or already in use."
Delete.OnProgram="This input is on Program. Take it off Program first."
Delete.Last="The last input cannot be deleted."
Delete.Confirm="Delete '%1'?"
Delete.ConfirmLayers="Delete '%1'? It is a layer in %2 other input(s). Those layers are removed too."
Settings.Title="Input: %1"
Settings.Base="Base properties..."
Settings.Layers="Layers, top first"
Settings.AddLayer="Add layer"
Settings.Remove="Remove"
Settings.Up="Up"
Settings.Down="Down"
Settings.Hint="Drag a layer in the Preview pane to place it."
Settings.Loop="That would make this input contain itself."
Audio.Tip="Audio on or off"
AFV.Tip="Audio follows video: heard only while the input is on Program"
Headphones.Tip="Hear this input in the headphones"
Bus.Tip="Send to bus %1"
DeskLayout="Desk layout"
Hotkey.InputToPreview="Input %1 to Preview"
```

- [x] **Step 4: Write the shared header**

`breadstudio/desk/Desk.hpp`:

```cpp
#pragma once

#include <obs-module.h>

#include <QString>

#include <cstdint>

namespace desk {

inline constexpr const char *InputsDockId = "breadDeskInputs";
inline constexpr const char *MixerDockId = "breadDeskMixer";

/* the values OBS's multiview uses */
inline constexpr uint32_t ProgramColor = 0xFFD00000;
inline constexpr uint32_t PreviewColor = 0xFF00D000;
inline constexpr uint32_t IdleColor = 0xFF303030;

inline QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

} // namespace desk
```

- [x] **Step 5: Write the module entry**

`breadstudio/desk/desk.cpp`:

```cpp
#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bread-desk", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "BreadStudio desk";
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[bread-desk] loaded");
	return true;
}
```

- [x] **Step 6: Add the plugin to OBS's build**

Append this as the last line of `plugins/CMakeLists.txt`:

```cmake
add_subdirectory("${CMAKE_SOURCE_DIR}/breadstudio/desk" "${CMAKE_BINARY_DIR}/breadstudio/desk")
```

In `breadstudio/UPSTREAM-EDITS.md`, under `## Edited`, add:

```
- `plugins/CMakeLists.txt` - one line that adds breadstudio/desk to the build
```

- [x] **Step 7: Build**

Run: `.\breadstudio\build.ps1`
Expected: exit 0, and both of these exist:

```powershell
Test-Path build_x64\rundir\RelWithDebInfo\obs-plugins\64bit\bread-desk.dll
Test-Path build_x64\rundir\RelWithDebInfo\data\obs-plugins\bread-desk\locale\en-US.ini
```

Expected: `True` twice.

- [x] **Step 8: Run and read the log**

Start the program (Global Constraints), close it, read the log.
Expected: a line containing `[bread-desk] loaded`, and `Number of memory leaks: 0`.

- [x] **Step 9: Commit**

```powershell
git add -A
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the desk plugin skeleton"
```

Expected from the check: `UPSTREAM-EDITS.md matches the 2 file(s) changed since 32.2.2.`

---

### Task 2: The loop check

**Files:**
- Create: `breadstudio/desk/LayerGraph.hpp`
- Create: `breadstudio/desk/LayerGraph.cpp`
- Create: `breadstudio/desk/test/LayerGraphTest.cpp`
- Modify: `breadstudio/desk/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `using desk::LayerGraph = std::unordered_map<std::string, std::vector<std::string>>` — input id to the ids of the inputs it shows as layers.
  - `bool desk::reaches(const LayerGraph &graph, const std::string &from, const std::string &to)` — true when `to` is `from`, or is shown inside `from` through any depth of layers.
  - `bool desk::wouldLoop(const LayerGraph &graph, const std::string &host, const std::string &layer)` — true when adding `layer` on `host` would make an input contain itself.

- [x] **Step 1: Write the header and a stub**

`breadstudio/desk/LayerGraph.hpp`:

```cpp
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace desk {

/* input id -> ids of the inputs it shows as layers */
using LayerGraph = std::unordered_map<std::string, std::vector<std::string>>;

/* true when `to` is `from`, or is shown inside `from` through any depth of layers */
bool reaches(const LayerGraph &graph, const std::string &from, const std::string &to);

/* true when adding `layer` on `host` would make an input contain itself */
inline bool wouldLoop(const LayerGraph &graph, const std::string &host, const std::string &layer)
{
	return reaches(graph, layer, host);
}

} // namespace desk
```

`breadstudio/desk/LayerGraph.cpp`:

```cpp
#include "LayerGraph.hpp"

namespace desk {

bool reaches(const LayerGraph &, const std::string &, const std::string &)
{
	return false;
}

} // namespace desk
```

- [x] **Step 2: Write the failing test**

`breadstudio/desk/test/LayerGraphTest.cpp`:

```cpp
#include "LayerGraph.hpp"

#include <cstdio>

using desk::LayerGraph;
using desk::reaches;
using desk::wouldLoop;

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

int main()
{
	/* a layer on itself */
	{
		LayerGraph g;
		CHECK(wouldLoop(g, "a", "a"));
	}

	/* a loop through two inputs: b shows a, so a must not take b */
	{
		LayerGraph g{{"b", {"a"}}};
		CHECK(wouldLoop(g, "a", "b"));
		CHECK(!wouldLoop(g, "b", "a")); /* a second copy of a on b is not a loop */
	}

	/* a loop through three inputs: c shows b, b shows a */
	{
		LayerGraph g{{"c", {"b"}}, {"b", {"a"}}};
		CHECK(wouldLoop(g, "a", "c"));
		CHECK(wouldLoop(g, "b", "c"));
		CHECK(!wouldLoop(g, "c", "a"));
	}

	/* a deep chain that is not a loop: a shows b shows c shows d */
	{
		LayerGraph g{{"a", {"b"}}, {"b", {"c"}}, {"c", {"d"}}};
		CHECK(!wouldLoop(g, "d", "e")); /* a new input at the end */
		CHECK(!wouldLoop(g, "a", "d")); /* d shown twice inside a, by two routes */
		CHECK(wouldLoop(g, "d", "a"));  /* closing the chain */
	}

	/* a graph that already holds a loop still gives an answer */
	{
		LayerGraph g{{"a", {"b"}}, {"b", {"a"}}};
		CHECK(!reaches(g, "a", "z"));
	}

	/* "is this input on Program": Program is c, which shows b, which shows a */
	{
		LayerGraph g{{"c", {"b"}}, {"b", {"a"}}};
		CHECK(reaches(g, "c", "c"));
		CHECK(reaches(g, "c", "a"));
		CHECK(!reaches(g, "a", "c"));
		CHECK(!reaches(g, "c", "unknown"));
	}

	if (failures == 0) {
		std::printf("LayerGraph: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
```

Add to the end of `breadstudio/desk/CMakeLists.txt`:

```cmake
target_sources(bread-desk PRIVATE LayerGraph.cpp LayerGraph.hpp)

# The one automated test: no OBS, no Qt.
add_executable(bread-desk-test test/LayerGraphTest.cpp LayerGraph.cpp)
target_include_directories(bread-desk-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(
  bread-desk-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/desk/test-bin" FOLDER plugins/bread-desk
)
```

- [x] **Step 3: Run the test and see it fail**

```powershell
.\breadstudio\build.ps1
build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-desk-test.exe; $LASTEXITCODE
```

Expected: several `FAIL line …` lines (every check that expects `true`), then `1`.

- [x] **Step 4: Implement**

Replace the body of `breadstudio/desk/LayerGraph.cpp`:

```cpp
#include "LayerGraph.hpp"

#include <unordered_set>

namespace desk {

bool reaches(const LayerGraph &graph, const std::string &from, const std::string &to)
{
	std::unordered_set<std::string> seen;
	std::vector<std::string> todo{from};

	while (!todo.empty()) {
		std::string id = std::move(todo.back());
		todo.pop_back();

		if (id == to) {
			return true;
		}
		if (!seen.insert(id).second) {
			continue;
		}

		auto it = graph.find(id);
		if (it != graph.end()) {
			todo.insert(todo.end(), it->second.begin(), it->second.end());
		}
	}

	return false;
}

} // namespace desk
```

- [x] **Step 5: Run the test and see it pass**

```powershell
.\breadstudio\build.ps1
build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-desk-test.exe; $LASTEXITCODE
```

Expected: `LayerGraph: all checks passed`, then `0`.

- [x] **Step 6: Commit**

```powershell
git add -A
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the layer loop check and its test"
```

---

### Task 3: The audio bed, proven with a test tone

This task decides whether §5.3 holds. If Step 8 fails, stop and report to the owner; do not start Task 4.

**Files:**
- Create: `breadstudio/desk/InputModel.hpp`, `breadstudio/desk/InputModel.cpp` (the read side only; later tasks add to both)
- Create: `breadstudio/desk/AudioBed.hpp`, `breadstudio/desk/AudioBed.cpp`
- Create: `breadstudio/desk/BusMeters.hpp`, `breadstudio/desk/BusMeters.cpp`
- Create: `breadstudio/desk/test/make-tone.ps1`
- Modify: `breadstudio/desk/desk.cpp` (replaced whole)
- Modify: `breadstudio/desk/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces:
  - `std::vector<OBSSource> desk::inputs()` — every scene, in OBS's scene order, which is input-bar order.
  - `OBSSceneItem desk::baseItem(obs_source_t *input)` — the bottom item of the input's scene, or null.
  - `OBSSource desk::baseSource(obs_source_t *input)` — that item's source, or null.
  - `bool desk::hasAudio(obs_source_t *source)` — false for null.
  - `desk::AudioBed &desk::bed()` with `void rebuild()`, `void sync()`, `void clear()`, `static bool afv(obs_source_t *input)`, `void setAfv(obs_source_t *input, bool on)`.
  - `desk::BusMeters &desk::busMeters()` with `void start()`, `void stop()`, `float takePeakDb(size_t bus)` — the peak since the last call, in dBFS, negative infinity when silent.
  - In `desk.cpp`: the functions `load()` and `unload()` and the event switch that later tasks add lines to.

- [x] **Step 1: Write the read side of the input model**

`breadstudio/desk/InputModel.hpp`:

```cpp
#pragma once

#include <obs.hpp>

#include <vector>

namespace desk {

/* every scene, in OBS's scene order, which is input-bar order */
std::vector<OBSSource> inputs();

/* the bottom item of the input's scene: the base. Null for an empty scene. */
OBSSceneItem baseItem(obs_source_t *input);
OBSSource baseSource(obs_source_t *input);

/* false for null */
bool hasAudio(obs_source_t *source);

} // namespace desk
```

`breadstudio/desk/InputModel.cpp`:

```cpp
#include "InputModel.hpp"

#include <obs-frontend-api.h>

namespace desk {

std::vector<OBSSource> inputs()
{
	std::vector<OBSSource> result;

	obs_frontend_source_list list = {};
	obs_frontend_get_scenes(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		result.emplace_back(list.sources.array[i]);
	}
	obs_frontend_source_list_free(&list);

	return result;
}

OBSSceneItem baseItem(obs_source_t *input)
{
	OBSSceneItem first;
	obs_scene_t *scene = obs_scene_from_source(input);
	if (!scene) {
		return first;
	}

	/* items are enumerated bottom first; stop at the first */
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			*static_cast<OBSSceneItem *>(param) = item;
			return false;
		},
		&first);

	return first;
}

OBSSource baseSource(obs_source_t *input)
{
	OBSSceneItem item = baseItem(input);
	return item ? OBSSource(obs_sceneitem_get_source(item)) : OBSSource();
}

bool hasAudio(obs_source_t *source)
{
	return source && (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0;
}

} // namespace desk
```

- [x] **Step 2: Write the audio bed**

`breadstudio/desk/AudioBed.hpp`:

```cpp
#pragma once

#include <obs.hpp>

namespace desk {

/* One hidden scene held on a spare output channel. The base of every input with AFV off
 * is an item in it, placed outside the canvas, so OBS mixes that base whatever is on
 * Program. libobs counts a source once when it is also on Program (obs-audio.c,
 * push_audio_tree2). */
class AudioBed {
public:
	void rebuild(); /* after a scene collection has loaded */
	void sync();    /* after inputs or AFV flags changed */
	void clear();   /* before a scene collection unloads; safe to call twice */

	/* audio follows video: heard only while the input is on Program */
	static bool afv(obs_source_t *input);
	void setAfv(obs_source_t *input, bool on);

private:
	OBSSceneAutoRelease scene;
	uint32_t channel = 0;
};

AudioBed &bed();

} // namespace desk
```

`breadstudio/desk/AudioBed.cpp`:

```cpp
#include "AudioBed.hpp"
#include "InputModel.hpp"

#include <obs-frontend-api.h>
#include <graphics/vec2.h>

#include <string>
#include <unordered_set>
#include <vector>

namespace desk {

static const char *AfvKey = "bread_afv";

/* OBS's frontend uses channels 0 to 6. The bed takes the highest free one. */
static const uint32_t FirstChannel = MAX_CHANNELS - 1;
static const uint32_t LastChannel = 7;

/* far outside any canvas, so the bed draws nothing */
static const float OffCanvas = 100000.0f;

AudioBed &bed()
{
	static AudioBed instance;
	return instance;
}

bool AudioBed::afv(obs_source_t *input)
{
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	if (obs_data_has_user_value(settings, AfvKey)) {
		return obs_data_get_bool(settings, AfvKey);
	}

	/* default: an input with a picture follows video; an audio-only input is always on */
	OBSSource base = baseSource(input);
	return base && (obs_source_get_output_flags(base) & OBS_SOURCE_VIDEO) != 0;
}

void AudioBed::setAfv(obs_source_t *input, bool on)
{
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	obs_data_set_bool(settings, AfvKey, on);
	sync();
	obs_frontend_save();
}

void AudioBed::rebuild()
{
	clear();

	for (uint32_t candidate = FirstChannel; candidate >= LastChannel; candidate--) {
		OBSSourceAutoRelease taken = obs_get_output_source(candidate);
		if (!taken) {
			channel = candidate;
			break;
		}
	}

	if (channel == 0) {
		blog(LOG_ERROR, "[bread-desk] no free output channel for the audio bed; AFV off will not work");
		return;
	}
	if (channel != FirstChannel) {
		blog(LOG_INFO, "[bread-desk] audio bed is on output channel %u", channel);
	}

	scene = obs_scene_create_private("Bread audio bed");
	obs_set_output_source(channel, obs_scene_get_source(scene));
	sync();
}

void AudioBed::clear()
{
	if (!scene) {
		return;
	}

	/* OBS may already have emptied every channel; only take down what is still ours */
	OBSSourceAutoRelease current = obs_get_output_source(channel);
	if (current.Get() == obs_scene_get_source(scene)) {
		obs_set_output_source(channel, nullptr);
	}

	scene = nullptr;
	channel = 0;
}

void AudioBed::sync()
{
	if (!scene) {
		return;
	}

	/* the bases that should be in the bed */
	std::unordered_set<std::string> wanted;
	std::vector<OBSSource> wantedSources;
	for (OBSSource &input : inputs()) {
		if (afv(input)) {
			continue;
		}
		OBSSource base = baseSource(input);
		if (!hasAudio(base)) {
			continue;
		}
		if (wanted.insert(obs_source_get_uuid(base)).second) {
			wantedSources.push_back(base);
		}
	}

	/* what is in the bed now */
	struct Found {
		const std::unordered_set<std::string> *wanted;
		std::unordered_set<std::string> present;
		std::vector<OBSSceneItem> extra;
	} found;
	found.wanted = &wanted;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			auto *found = static_cast<Found *>(param);
			std::string uuid = obs_source_get_uuid(obs_sceneitem_get_source(item));
			if (found->wanted->count(uuid)) {
				found->present.insert(uuid);
			} else {
				found->extra.emplace_back(item);
			}
			return true;
		},
		&found);

	/* change only what differs, so an input that stays in the bed is not interrupted */
	for (OBSSceneItem &item : found.extra) {
		obs_sceneitem_remove(item);
	}

	for (OBSSource &base : wantedSources) {
		if (found.present.count(obs_source_get_uuid(base))) {
			continue;
		}
		obs_sceneitem_t *item = obs_scene_add(scene, base);
		struct vec2 pos;
		vec2_set(&pos, OffCanvas, OffCanvas);
		obs_sceneitem_set_pos(item, &pos);
	}
}

} // namespace desk
```

- [x] **Step 3: Write the bus meters**

`breadstudio/desk/BusMeters.hpp`:

```cpp
#pragma once

#include <obs.h>

#include <atomic>
#include <cstddef>

namespace desk {

/* Peak level of each of OBS's six audio tracks: bus 0 is Master, 1 to 5 are A to E. */
class BusMeters {
public:
	void start();
	void stop(); /* safe to call twice */

	/* the peak since the last call, in dBFS; negative infinity when silent */
	float takePeakDb(size_t bus);

private:
	static void onAudio(void *param, size_t mix, struct audio_data *data);

	std::atomic<float> peak[MAX_AUDIO_MIXES] = {};
	bool running = false;
};

BusMeters &busMeters();

} // namespace desk
```

`breadstudio/desk/BusMeters.cpp`:

```cpp
#include "BusMeters.hpp"

#include <obs-audio-controls.h>

#include <cmath>

namespace desk {

BusMeters &busMeters()
{
	static BusMeters instance;
	return instance;
}

void BusMeters::start()
{
	if (running) {
		return;
	}
	audio_t *audio = obs_get_audio();
	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		/* no conversion: OBS's own format, float planar */
		audio_output_connect(audio, mix, nullptr, onAudio, this);
	}
	running = true;
}

void BusMeters::stop()
{
	if (!running) {
		return;
	}
	audio_t *audio = obs_get_audio();
	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		audio_output_disconnect(audio, mix, onAudio, this);
	}
	running = false;
}

/* runs on OBS's audio thread */
void BusMeters::onAudio(void *param, size_t mix, struct audio_data *data)
{
	auto *self = static_cast<BusMeters *>(param);
	const size_t channels = audio_output_get_channels(obs_get_audio());

	float highest = 0.0f;
	for (size_t ch = 0; ch < channels; ch++) {
		const float *samples = reinterpret_cast<const float *>(data->data[ch]);
		if (!samples) {
			continue;
		}
		for (uint32_t i = 0; i < data->frames; i++) {
			const float level = std::fabs(samples[i]);
			if (level > highest) {
				highest = level;
			}
		}
	}

	float seen = self->peak[mix].load();
	while (highest > seen && !self->peak[mix].compare_exchange_weak(seen, highest)) {
	}
}

float BusMeters::takePeakDb(size_t bus)
{
	return obs_mul_to_db(peak[bus].exchange(0.0f));
}

} // namespace desk
```

- [x] **Step 4: Write the tone generator**

`breadstudio/desk/test/make-tone.ps1`:

```powershell
# Writes a 10 s, 1 kHz, -12 dBFS, 48 kHz stereo 16-bit WAV for the audio bed check.
# 10 s is a whole number of cycles, so the file loops without a click.
param([string] $Path = "$env:TEMP\bread-tone.wav")

$ErrorActionPreference = 'Stop'
$rate = 48000
$frames = $rate * 10
$amplitude = [math]::Pow(10, -12 / 20) * 32767
$dataBytes = $frames * 4
$ascii = [Text.Encoding]::ASCII

$stream = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($stream)
$writer.Write($ascii.GetBytes('RIFF'))
$writer.Write([int32](36 + $dataBytes))
$writer.Write($ascii.GetBytes('WAVEfmt '))
$writer.Write([int32]16)           # fmt chunk size
$writer.Write([int16]1)            # PCM
$writer.Write([int16]2)            # channels
$writer.Write([int32]$rate)
$writer.Write([int32]($rate * 4))  # bytes per second
$writer.Write([int16]4)            # bytes per frame
$writer.Write([int16]16)           # bits per sample
$writer.Write($ascii.GetBytes('data'))
$writer.Write([int32]$dataBytes)
for ($i = 0; $i -lt $frames; $i++) {
    $sample = [int16][math]::Round($amplitude * [math]::Sin(2 * [math]::PI * 1000 * $i / $rate))
    $writer.Write($sample)
    $writer.Write($sample)
}
$writer.Flush()
[IO.File]::WriteAllBytes($Path, $stream.ToArray())
Write-Output $Path
```

- [x] **Step 5: Replace the module entry, with a temporary probe**

The probe is two Tools menu items that exist only until Task 7 replaces them with the real mixer.

`breadstudio/desk/desk.cpp` (whole file):

```cpp
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QTimer>

#include "AudioBed.hpp"
#include "BusMeters.hpp"
#include "Desk.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bread-desk", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "BreadStudio desk";
}

using namespace desk;

/* true between a scene collection finishing loading and starting to unload */
static bool loaded = false;

static void load()
{
	loaded = true;
	bed().rebuild();
}

static void unload()
{
	loaded = false;
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
		break;
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		if (loaded) {
			bed().sync();
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
	obs_frontend_add_event_callback(onEvent, nullptr);

	obs_frontend_add_tools_menu_item("Desk probe: toggle AFV of the Preview input", probeToggleAfv, nullptr);
	obs_frontend_add_tools_menu_item("Desk probe: log bus peaks", probeLogPeaks, nullptr);

	blog(LOG_INFO, "[bread-desk] loaded");
	return true;
}
```

- [x] **Step 6: Add the sources to the build**

In `breadstudio/desk/CMakeLists.txt`, replace the line added in Task 2

```cmake
target_sources(bread-desk PRIVATE LayerGraph.cpp LayerGraph.hpp)
```

with

```cmake
target_sources(
  bread-desk
  PRIVATE
    AudioBed.cpp
    AudioBed.hpp
    BusMeters.cpp
    BusMeters.hpp
    InputModel.cpp
    InputModel.hpp
    LayerGraph.cpp
    LayerGraph.hpp
)
```

- [x] **Step 7: Build**

Run: `.\breadstudio\build.ps1`
Expected: exit 0.

- [x] **Step 8: Prove the audio bed**

Make the tone:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\desk\test\make-tone.ps1
```

Expected: prints the path of `bread-tone.wav`.

Start the program and set up, using OBS's own interface (the desk has none yet):

1. Settings → Audio: set every Global Audio Device to Disabled. Apply.
2. View → Studio Mode on. In the Studio Mode gear menu between Preview and Program, untick "Duplicate Scene".
3. Scenes dock: make two scenes, `Tone` and `Other`. `Other` stays empty.
4. In `Tone`, add a Media Source on `bread-tone.wav`, with Loop ticked and "Restart playback when source becomes active" unticked.
5. Edit → Advanced Audio Properties: the media source has Audio Monitoring "Monitor Off".

Then take three readings. For each: put the scenes where the table says with Preview and the Transition button, wait two seconds, choose Tools → "Desk probe: log bus peaks", wait two seconds.

| Reading | AFV of `Tone` | On Program |
|---|---|---|
| A: Program only | on (the default; do not toggle) | `Tone` |
| B: bed only | off: put `Tone` on Preview, choose Tools → "Desk probe: toggle AFV of the Preview input" | `Other` |
| C: both | off (leave it) | `Tone` |

Close the program and read the log.
Expected: `probe: AFV of 'Tone' is now off` once, and three groups of six `probe: bus … peak` lines. In every group, bus M reads between `-12.5 dB` and `-11.5 dB`.

- Reading C at about `-6.0 dB` means the tone was counted twice: §5.3 does not hold. **Stop and report to the owner.**
- Reading B at `-inf dB` means the bed is not mixed: §5.3 does not hold. **Stop and report to the owner.**

- [x] **Step 9: Check that the collection unloads cleanly**

Start the program. With `Tone` still AFV off: Scene Collection → New, give it any name; then Scene Collection → switch back to the first one; then close the program. Read the log.
Expected: `Number of memory leaks: 0`, and no line containing `remaining`.

- [x] **Step 10: Commit**

```powershell
git add -A
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the audio bed and bus meters, proven with a test tone"
```

Record the three readings of bus M in the commit body.

---

### Task 4: The input bar shows the inputs and switches them

**Files:**
- Modify: `breadstudio/desk/InputModel.hpp`, `breadstudio/desk/InputModel.cpp`
- Create: `breadstudio/desk/TileDisplay.hpp`, `breadstudio/desk/TileDisplay.cpp`
- Create: `breadstudio/desk/InputBar.hpp`, `breadstudio/desk/InputBar.cpp`
- Modify: `breadstudio/desk/Desk.hpp`, `breadstudio/desk/desk.cpp`, `breadstudio/desk/CMakeLists.txt`

**Interfaces:**
- Consumes: `desk::inputs()`, `desk::baseSource()`, `desk::bed()`, `desk::text()`, the colours and dock ids in `Desk.hpp`, and `load()`, `unload()`, `onEvent()` in `desk.cpp`.
- Produces:
  - `void desk::toPreview(obs_source_t *input)` — to Preview; with Studio Mode off it goes straight to Program, as OBS does.
  - `void desk::cutToProgram(obs_source_t *input)` — to Program as a cut, whatever transition is selected; ignored while a transition runs or when the input is already on Program.
  - `void desk::onTransitionStopped()` and `void desk::clearModel()`.
  - `void desk::inputsChanged()` — declared in `Desk.hpp`, defined in `desk.cpp`; call it after anything changes the set, order or names of inputs.
  - `class desk::TileDisplay : public QWidget` with `setInputs(const std::vector<OBSSource> &)`, `setFrames(const std::vector<uint32_t> &)`, constants `TileW = 160`, `TileH = 90`, and signals `clicked(int index)`, `moved(int from, int to)`, `menuRequested(int index, const QPoint &globalPos)`.
  - `class desk::InputBar : public QWidget` with `refresh()`, `updateTally()`, `clear()`, and the private members `row` (a `QHBoxLayout *`), `footerRow`, `sources`, `display`, and `QWidget *makeFooter(int index)`, which Tasks 5 and 6 extend.

- [x] **Step 1: Add switching to the input model**

Append to `breadstudio/desk/InputModel.hpp`, inside `namespace desk`:

```cpp
/* to Preview; with Studio Mode off this goes straight to Program, as OBS does */
void toPreview(obs_source_t *input);

/* to Program as a cut, whatever transition is selected. Ignored while a transition is
 * running or when the input is already on Program. */
void cutToProgram(obs_source_t *input);

/* call on OBS_FRONTEND_EVENT_TRANSITION_STOPPED */
void onTransitionStopped();

/* drop every reference the model holds; safe to call twice */
void clearModel();
```

In `breadstudio/desk/InputModel.cpp`, add `#include <QTimer>` and `#include <cstring>` to the includes, and append inside `namespace desk`:

```cpp
/* A tile's Cut is done with the frontend API alone: select the Cut transition, switch,
 * and put back the operator's transition and Preview when the cut has finished. */
static OBSSource restoreTransition;
static OBSSource restorePreview;
static bool restorePending = false;

void toPreview(obs_source_t *input)
{
	if (obs_frontend_preview_program_mode_active()) {
		obs_frontend_set_current_preview_scene(input);
	} else {
		obs_frontend_set_current_scene(input);
	}
}

static OBSSource cutTransition()
{
	OBSSource cut;

	obs_frontend_source_list list = {};
	obs_frontend_get_transitions(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		if (strcmp(obs_source_get_unversioned_id(list.sources.array[i]), "cut_transition") == 0) {
			cut = list.sources.array[i];
			break;
		}
	}
	obs_frontend_source_list_free(&list);

	return cut;
}

void cutToProgram(obs_source_t *input)
{
	if (!obs_frontend_preview_program_mode_active()) {
		obs_frontend_set_current_scene(input);
		return;
	}
	if (restorePending) {
		return;
	}

	/* OBS does nothing when asked for the scene already on Program, and then no
	 * transition ever stops, so nothing would be restored */
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	if (program.Get() == input) {
		return;
	}

	OBSSourceAutoRelease current = obs_frontend_get_current_transition();
	if (!current) {
		return;
	}
	const float time = obs_transition_get_time(current);
	if (time > 0.0f && time < 1.0f) {
		return; /* a transition is running; OBS ignores a new one */
	}

	OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
	restorePreview = preview.Get();

	OBSSource cut = cutTransition();
	if (cut && cut.Get() != current.Get()) {
		restoreTransition = current.Get();
		obs_frontend_set_current_transition(cut);
	}

	restorePending = true;
	obs_frontend_set_current_scene(input);

	/* in case the stop event never comes */
	QTimer::singleShot(500, [] { onTransitionStopped(); });
}

void onTransitionStopped()
{
	if (!restorePending) {
		return;
	}
	restorePending = false;

	OBSSource transition = restoreTransition;
	OBSSource preview = restorePreview;
	restoreTransition = nullptr;
	restorePreview = nullptr;

	if (transition) {
		obs_frontend_set_current_transition(transition);
	}
	/* OBS's "swap" option has just put the old Program on Preview; a tile's Cut
	 * leaves Preview as it was */
	if (preview && obs_frontend_preview_program_mode_active()) {
		obs_frontend_set_current_preview_scene(preview);
	}
}

void clearModel()
{
	restorePending = false;
	restoreTransition = nullptr;
	restorePreview = nullptr;
}
```

- [x] **Step 2: Write the tile display**

`breadstudio/desk/TileDisplay.hpp`:

```cpp
#pragma once

#include <obs.hpp>

#include <QWidget>

#include <mutex>
#include <vector>

namespace desk {

/* One display that draws every tile's live picture and tally frame, side by side. */
class TileDisplay : public QWidget {
	Q_OBJECT

public:
	static constexpr int TileW = 160;
	static constexpr int TileH = 90;

	explicit TileDisplay(QWidget *parent = nullptr);
	~TileDisplay() override;

	/* replaces the tiles; an empty list drops every reference */
	void setInputs(const std::vector<OBSSource> &inputs);
	/* one colour per tile, in order */
	void setFrames(const std::vector<uint32_t> &frames);

	QPaintEngine *paintEngine() const override { return nullptr; }

signals:
	void clicked(int index);
	void moved(int from, int to);
	void menuRequested(int index, const QPoint &globalPos);

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	struct Tile {
		OBSSource input;
		uint32_t frame;
	};

	void createDisplay();
	QSize pixelSize() const;
	int indexAt(const QPoint &pos) const;
	static void draw(void *param, uint32_t cx, uint32_t cy);

	OBSDisplay display;
	std::mutex mutex; /* guards tiles: draw() runs on OBS's graphics thread */
	std::vector<Tile> tiles;
	int count = 0; /* tiles.size(), for the UI thread */
	int pressed = -1;
};

} // namespace desk
```

`breadstudio/desk/TileDisplay.cpp`:

```cpp
#include "TileDisplay.hpp"
#include "Desk.hpp"

#include <QMouseEvent>
#include <QWindow>

#include <algorithm>

namespace desk {

TileDisplay::TileDisplay(QWidget *parent) : QWidget(parent)
{
	/* the same attributes OBS's own display widget sets */
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);
	setFixedSize(TileW, TileH);

	connect(windowHandle(), &QWindow::visibleChanged, this, [this](bool visible) {
		if (visible) {
			createDisplay();
		}
	});
}

TileDisplay::~TileDisplay()
{
	if (display) {
		obs_display_remove_draw_callback(display, draw, this);
	}
	display = nullptr;
	setInputs({});
}

QSize TileDisplay::pixelSize() const
{
	const qreal ratio = devicePixelRatioF();
	return QSize((int)(width() * ratio), (int)(height() * ratio));
}

void TileDisplay::createDisplay()
{
	if (display || !windowHandle() || !windowHandle()->isExposed()) {
		return;
	}

	const QSize size = pixelSize();

	gs_init_data info = {};
	info.cx = (uint32_t)size.width();
	info.cy = (uint32_t)size.height();
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
	info.window.hwnd = reinterpret_cast<void *>(windowHandle()->winId());

	display = obs_display_create(&info, 0xFF000000);
	obs_display_add_draw_callback(display, draw, this);
}

void TileDisplay::paintEvent(QPaintEvent *event)
{
	createDisplay();
	QWidget::paintEvent(event);
}

void TileDisplay::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	createDisplay();

	if (display) {
		const QSize size = pixelSize();
		obs_display_resize(display, (uint32_t)size.width(), (uint32_t)size.height());
	}
}

void TileDisplay::setInputs(const std::vector<OBSSource> &inputs)
{
	/* a source drawn in a display must be marked as showing, or cameras stay off */
	std::vector<Tile> next;
	for (const OBSSource &input : inputs) {
		next.push_back({input, IdleColor});
		obs_source_inc_showing(input);
	}

	{
		std::lock_guard<std::mutex> lock(mutex);
		tiles.swap(next);
	}

	for (Tile &old : next) {
		obs_source_dec_showing(old.input);
	}

	count = (int)inputs.size();
	setFixedSize(std::max(1, count) * TileW, TileH);
}

void TileDisplay::setFrames(const std::vector<uint32_t> &frames)
{
	std::lock_guard<std::mutex> lock(mutex);
	for (size_t i = 0; i < tiles.size() && i < frames.size(); i++) {
		tiles[i].frame = frames[i];
	}
}

/* runs on OBS's graphics thread */
void TileDisplay::draw(void *param, uint32_t cx, uint32_t cy)
{
	auto *self = static_cast<TileDisplay *>(param);
	std::lock_guard<std::mutex> lock(self->mutex);
	if (self->tiles.empty()) {
		return;
	}

	obs_video_info ovi = {};
	if (!obs_get_video_info(&ovi)) {
		return;
	}

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	auto box = [&](float w, float h, uint32_t argb) {
		gs_effect_set_color(color, argb);
		while (gs_effect_loop(solid, "Solid")) {
			gs_draw_sprite(nullptr, 0, (uint32_t)w, (uint32_t)h);
		}
	};

	const float height = (float)cy;
	const float tileW = (float)cx / (float)self->tiles.size();
	const float border = std::max(2.0f, height / 30.0f);
	const float canvasW = (float)ovi.base_width;
	const float canvasH = (float)ovi.base_height;

	for (size_t i = 0; i < self->tiles.size(); i++) {
		const Tile &tile = self->tiles[i];
		const float x = tileW * (float)i;

		gs_viewport_push();
		gs_projection_push();

		/* the frame: the whole tile in the tally colour */
		gs_set_viewport((int)x, 0, (int)tileW, (int)height);
		gs_ortho(0.0f, tileW, 0.0f, height, -100.0f, 100.0f);
		box(tileW, height, tile.frame);

		/* the picture, fitted inside the frame */
		const float scale = std::min((tileW - border * 2.0f) / canvasW, (height - border * 2.0f) / canvasH);
		const float w = canvasW * scale;
		const float h = canvasH * scale;
		gs_set_viewport((int)(x + (tileW - w) / 2.0f), (int)((height - h) / 2.0f), (int)w, (int)h);
		gs_ortho(0.0f, canvasW, 0.0f, canvasH, -100.0f, 100.0f);
		box(canvasW, canvasH, 0xFF000000);
		obs_source_video_render(tile.input);

		gs_projection_pop();
		gs_viewport_pop();
	}
}

int TileDisplay::indexAt(const QPoint &pos) const
{
	if (count == 0) {
		return -1;
	}
	return std::clamp(pos.x() / TileW, 0, count - 1);
}

void TileDisplay::mousePressEvent(QMouseEvent *event)
{
	const int index = indexAt(event->position().toPoint());
	if (index < 0) {
		return;
	}

	if (event->button() == Qt::RightButton) {
		emit menuRequested(index, event->globalPosition().toPoint());
	} else if (event->button() == Qt::LeftButton) {
		pressed = index;
	}
}

void TileDisplay::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton || pressed < 0) {
		return;
	}

	const int from = pressed;
	pressed = -1;

	/* released on the tile it was pressed on: a click. On another: a drag. */
	const int to = indexAt(event->position().toPoint());
	if (to == from) {
		emit clicked(from);
	} else if (to >= 0) {
		emit moved(from, to);
	}
}

} // namespace desk
```

- [x] **Step 3: Write the input bar**

`breadstudio/desk/InputBar.hpp`:

```cpp
#pragma once

#include <obs.hpp>

#include <QWidget>

#include <vector>

class QFrame;
class QHBoxLayout;

namespace desk {

class TileDisplay;

/* The input bar dock: one tile per input, in input order. */
class InputBar : public QWidget {
	Q_OBJECT

public:
	explicit InputBar(QWidget *parent = nullptr);

	void refresh();     /* rebuild the tiles from OBS's scene list */
	void updateTally(); /* tally frames, and the Studio Mode warning */
	void clear();       /* drop every reference; safe to call twice */

private:
	QWidget *makeFooter(int index);

	QFrame *warning;
	QHBoxLayout *row; /* the scroll area, then whatever later tasks add */
	TileDisplay *display;
	QHBoxLayout *footerRow;
	std::vector<OBSSource> sources;
};

} // namespace desk
```

`breadstudio/desk/InputBar.cpp`:

```cpp
#include "InputBar.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "TileDisplay.hpp"

#include <obs-frontend-api.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace desk {

InputBar::InputBar(QWidget *parent) : QWidget(parent)
{
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(4, 4, 4, 4);

	/* shown only while Studio Mode is off (spec §8) */
	warning = new QFrame(this);
	auto *warningRow = new QHBoxLayout(warning);
	warningRow->setContentsMargins(0, 0, 0, 0);
	warningRow->addWidget(new QLabel(text("StudioModeOff"), warning), 1);
	auto *studioOn = new QPushButton(text("StudioModeOn"), warning);
	connect(studioOn, &QPushButton::clicked, this, [] { obs_frontend_set_preview_program_mode(true); });
	warningRow->addWidget(studioOn);
	warning->setVisible(false);
	outer->addWidget(warning);

	row = new QHBoxLayout();
	outer->addLayout(row, 1);

	auto *scroll = new QScrollArea(this);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	row->addWidget(scroll, 1);

	/* the strip: one wide display on top, a row of footers under it, column by column */
	auto *strip = new QWidget(scroll);
	auto *stripLayout = new QVBoxLayout(strip);
	stripLayout->setContentsMargins(0, 0, 0, 0);
	stripLayout->setSpacing(0);
	stripLayout->setSizeConstraint(QLayout::SetFixedSize);

	display = new TileDisplay(strip);
	stripLayout->addWidget(display, 0, Qt::AlignLeft);

	auto *footers = new QWidget(strip);
	footerRow = new QHBoxLayout(footers);
	footerRow->setContentsMargins(0, 0, 0, 0);
	footerRow->setSpacing(0);
	stripLayout->addWidget(footers, 0, Qt::AlignLeft);

	scroll->setWidget(strip);

	connect(display, &TileDisplay::clicked, this, [this](int index) {
		if ((size_t)index < sources.size()) {
			toPreview(sources[(size_t)index]);
		}
	});
}

QWidget *InputBar::makeFooter(int index)
{
	obs_source_t *input = sources[(size_t)index];

	auto *footer = new QWidget();
	footer->setFixedWidth(TileDisplay::TileW);
	auto *column = new QVBoxLayout(footer);
	column->setContentsMargins(2, 2, 2, 2);
	column->setSpacing(2);

	auto *name = new QLabel(QString("%1  %2").arg(index + 1).arg(QString::fromUtf8(obs_source_get_name(input))),
				footer);
	name->setToolTip(name->text());
	column->addWidget(name);

	auto *buttons = new QHBoxLayout();
	buttons->setSpacing(2);
	column->addLayout(buttons);

	auto *cut = new QToolButton(footer);
	cut->setText(text("Cut"));
	connect(cut, &QToolButton::clicked, this, [this, index] {
		if ((size_t)index < sources.size()) {
			cutToProgram(sources[(size_t)index]);
		}
	});
	buttons->addWidget(cut);
	buttons->addStretch(1);

	return footer;
}

void InputBar::refresh()
{
	sources = inputs();

	while (QLayoutItem *item = footerRow->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	for (int i = 0; i < (int)sources.size(); i++) {
		footerRow->addWidget(makeFooter(i));
	}

	display->setInputs(sources);
	updateTally();
}

void InputBar::updateTally()
{
	const bool studio = obs_frontend_preview_program_mode_active();
	warning->setVisible(!studio);

	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	OBSSourceAutoRelease preview;
	if (studio) {
		preview = obs_frontend_get_current_preview_scene();
	}

	std::vector<uint32_t> frames;
	for (OBSSource &input : sources) {
		if (input.Get() == program.Get()) {
			frames.push_back(ProgramColor);
		} else if (studio && input.Get() == preview.Get()) {
			frames.push_back(PreviewColor);
		} else {
			frames.push_back(IdleColor);
		}
	}
	display->setFrames(frames);
}

void InputBar::clear()
{
	sources.clear();
	while (QLayoutItem *item = footerRow->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	display->setInputs({});
}

} // namespace desk
```

- [x] **Step 4: Declare `inputsChanged` in the shared header**

In `breadstudio/desk/Desk.hpp`, inside `namespace desk`, after `text()`:

```cpp
/* call after anything changes the set, order or names of inputs; defined in desk.cpp */
void inputsChanged();
```

- [x] **Step 5: Wire the bar into the module**

In `breadstudio/desk/desk.cpp`:

Add to the includes:

```cpp
#include <QApplication>

#include "InputBar.hpp"
#include "InputModel.hpp"
```

Replace `load()`, `unload()` and `onEvent()` with:

```cpp
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
```

In `obs_module_load`, before `obs_frontend_add_event_callback`:

```cpp
	bar = new InputBar();
	obs_frontend_add_dock_by_id(InputsDockId, obs_module_text("Inputs"), bar);
	signal_handler_connect(obs_get_signal_handler(), "source_rename", onSourceRename, nullptr);
```

- [x] **Step 6: Add the sources to the build**

In `breadstudio/desk/CMakeLists.txt`, add to the `target_sources(bread-desk PRIVATE …)` list, keeping it alphabetical: `InputBar.cpp`, `InputBar.hpp`, `TileDisplay.cpp`, `TileDisplay.hpp`.

- [x] **Step 7: Build**

Run: `.\breadstudio\build.ps1`
Expected: exit 0.

- [x] **Step 8: Check it by hand**

Start the program. Docks menu → tick "Inputs"; drag the floating window to the bottom of the main window to dock it. Studio Mode on. In OBS's Scenes dock make four scenes: `One` (a Color Source, red), `Two` (a Color Source, blue), `Mixed` (a group holding a text source, plus `One` added as a scene), and `Empty` (nothing).

| Do | Expect |
|---|---|
| Look at the bar | Four tiles, numbered 1 to 4, each with its live picture. `Empty` is black. |
| Click tile 2 | Tile 2 has a green frame and is in OBS's Preview pane. |
| Press OBS's Transition button | Tile 2 turns red. The tile that was red is green. |
| Press Cut on tile 3 | Tile 3 is red at once. Preview is unchanged. The transition named in OBS's Scene Transitions dock is the one it was before. |
| Press Cut on the red tile | Nothing changes. |
| Set the transition duration to 3000 ms, press Transition, and within a second press Cut on another tile | The fade finishes as if Cut was never pressed. Afterwards the Scene Transitions dock still shows Fade. |
| View → Studio Mode off | The warning row appears over the tiles. Its button switches Studio Mode back on and the row goes. |
| Rename a scene in OBS's Scenes dock | The tile's name follows. |

If the transition selection does not come back to what it was after a tile's Cut, that is a defect in `cutToProgram`/`onTransitionStopped`; fix it in this task.

- [x] **Step 9: Check that the collection unloads cleanly**

With the bar docked and showing tiles: Scene Collection → New; switch back; close the program. Read the log.
Expected: `Number of memory leaks: 0`, and no line containing `remaining`.

- [x] **Step 10: Commit**

```powershell
git add -A
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the input bar with live tiles, tally, preview and cut"
```

---

### Task 5: Add, rename, delete, reorder, audio on and off, number hotkeys

**Files:**
- Modify: `breadstudio/desk/InputModel.hpp`, `breadstudio/desk/InputModel.cpp`
- Modify: `breadstudio/desk/InputBar.hpp`, `breadstudio/desk/InputBar.cpp`
- Modify: `breadstudio/desk/desk.cpp`

**Interfaces:**
- Consumes: everything Task 4 produces; `desk::LayerGraph`, `desk::reaches()` from Task 2.
- Produces:
  - `desk::LayerGraph desk::graph()` — every input's uuid mapped to the uuids of the scenes it holds as items.
  - `OBSSource desk::createInput(const char *unversionedId, const char *name, obs_data_t *settings)` — a new input with that base, placed last; null when the source type does not exist.
  - `bool desk::renameInput(obs_source_t *input, const char *name)` — false when the name is empty or taken.
  - `enum class desk::DeleteCheck { Ok, OnProgram, LastInput }` and `DeleteCheck desk::canDelete(obs_source_t *input)`.
  - `int desk::layerUseCount(obs_source_t *input)` — how many other inputs hold it as a layer.
  - `void desk::deleteInput(obs_source_t *input)`.
  - `bool desk::canReorder()` and `bool desk::moveInput(int from, int to)`.
  - In `InputBar`: `void syncAudioButtons()` (public), the Add button, the right-click menu, drag to reorder.

- [x] **Step 1: Add the write side to the input model**

Append to `breadstudio/desk/InputModel.hpp`, after adding `#include "LayerGraph.hpp"` at the top:

```cpp
/* every input's uuid -> the uuids of the scenes it holds as items */
LayerGraph graph();

/* A new input with that base, placed last. `unversionedId` is an OBS source type such as
 * "dshow_input". Null when the type does not exist on this machine. */
OBSSource createInput(const char *unversionedId, const char *name, obs_data_t *settings);

/* false when the name is empty or already used by any source */
bool renameInput(obs_source_t *input, const char *name);

enum class DeleteCheck { Ok, OnProgram, LastInput };
DeleteCheck canDelete(obs_source_t *input);

/* how many other inputs hold this one as a layer */
int layerUseCount(obs_source_t *input);

/* removes the input and every layer that shows it. Check canDelete first. */
void deleteInput(obs_source_t *input);

/* Reordering goes through OBS's scene list widget, found by name (spec §6.2).
 * canReorder is false when the widget is not found. */
bool canReorder();
bool moveInput(int from, int to);
```

In `breadstudio/desk/InputModel.cpp`, add to the includes:

```cpp
#include <graphics/vec2.h>

#include <QListWidget>
#include <QMainWindow>
#include <QSignalBlocker>

#include <algorithm>
#include <string>
```

and append inside `namespace desk`:

```cpp
static std::string uuidOf(obs_source_t *source)
{
	return obs_source_get_uuid(source);
}

LayerGraph graph()
{
	LayerGraph result;

	for (OBSSource &input : inputs()) {
		std::vector<std::string> &shown = result[uuidOf(input)];
		obs_scene_enum_items(
			obs_scene_from_source(input),
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				obs_source_t *source = obs_sceneitem_get_source(item);
				if (obs_scene_from_source(source)) {
					static_cast<std::vector<std::string> *>(param)->push_back(
						obs_source_get_uuid(source));
				}
				return true;
			},
			&shown);
	}

	return result;
}

/* scenes and sources share one set of names in OBS */
static std::string uniqueName(const std::string &wanted)
{
	std::string name = wanted;
	for (int n = 2;; n++) {
		OBSSourceAutoRelease existing = obs_get_source_by_name(name.c_str());
		if (!existing) {
			return name;
		}
		name = wanted + " " + std::to_string(n);
	}
}

OBSSource createInput(const char *unversionedId, const char *name, obs_data_t *settings)
{
	const char *id = obs_get_latest_input_type_id(unversionedId);
	if (!id) {
		return OBSSource();
	}

	const std::string sceneName = uniqueName(name);
	const std::string baseName = uniqueName(sceneName + " source");

	OBSSourceAutoRelease base = obs_source_create(id, baseName.c_str(), settings, nullptr);
	if (!base) {
		return OBSSource();
	}

	/* OBS's main window adds the scene to its list as soon as it exists */
	OBSSceneAutoRelease scene = obs_scene_create(sceneName.c_str());
	obs_sceneitem_t *item = obs_scene_add(scene, base);

	/* the fixed shape: the base fills the canvas and is locked (spec §3) */
	if (obs_source_get_output_flags(base) & OBS_SOURCE_VIDEO) {
		obs_video_info ovi = {};
		obs_get_video_info(&ovi);
		struct vec2 bounds;
		vec2_set(&bounds, (float)ovi.base_width, (float)ovi.base_height);
		obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
		obs_sceneitem_set_bounds(item, &bounds);
	}
	obs_sceneitem_set_locked(item, true);

	OBSSource input = obs_scene_get_source(scene);

	/* OBS puts a new scene after the selected one; a new input goes last */
	std::vector<OBSSource> all = inputs();
	for (size_t i = 0; i < all.size(); i++) {
		if (all[i].Get() == input.Get()) {
			moveInput((int)i, (int)all.size() - 1);
			break;
		}
	}

	return input;
}

bool renameInput(obs_source_t *input, const char *name)
{
	if (!name || !*name) {
		return false;
	}
	OBSSourceAutoRelease existing = obs_get_source_by_name(name);
	if (existing) {
		return false;
	}
	obs_source_set_name(input, name);
	return true;
}

DeleteCheck canDelete(obs_source_t *input)
{
	if (inputs().size() <= 1) {
		return DeleteCheck::LastInput;
	}

	/* on Program directly, or as a layer of what is on Program (spec §8) */
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	if (program && reaches(graph(), uuidOf(program), uuidOf(input))) {
		return DeleteCheck::OnProgram;
	}

	return DeleteCheck::Ok;
}

int layerUseCount(obs_source_t *input)
{
	const std::string id = uuidOf(input);
	int hosts = 0;
	for (const auto &[host, shown] : graph()) {
		if (host != id && std::find(shown.begin(), shown.end(), id) != shown.end()) {
			hosts++;
		}
	}
	return hosts;
}

void deleteInput(obs_source_t *input)
{
	struct Hits {
		obs_source_t *target;
		std::vector<OBSSceneItem> items;
	};

	for (OBSSource &host : inputs()) {
		if (host.Get() == input) {
			continue;
		}

		Hits hits{input, {}};
		obs_scene_enum_items(
			obs_scene_from_source(host),
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				auto *hits = static_cast<Hits *>(param);
				if (obs_sceneitem_get_source(item) == hits->target) {
					hits->items.emplace_back(item);
				}
				return true;
			},
			&hits);

		for (OBSSceneItem &item : hits.items) {
			obs_sceneitem_remove(item);
		}
	}

	/* OBS's main window takes the scene out of its list when it is removed */
	obs_source_remove(input);
}

static QListWidget *sceneList()
{
	static bool warned = false;

	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QListWidget *list = main ? main->findChild<QListWidget *>("scenes") : nullptr;
	if (!list && !warned) {
		warned = true;
		blog(LOG_WARNING, "[bread-desk] OBS's scene list 'scenes' was not found; reordering inputs is off");
	}
	return list;
}

bool canReorder()
{
	return sceneList() != nullptr;
}

bool moveInput(int from, int to)
{
	QListWidget *list = sceneList();
	if (!list || from == to || from < 0 || to < 0 || from >= list->count() || to >= list->count()) {
		return false;
	}

	{
		/* OBS switches scene when the list's selection changes; keep it quiet and
		 * put the selection back */
		QSignalBlocker quiet(list);
		QListWidgetItem *current = list->currentItem();
		QListWidgetItem *moved = list->takeItem(from);
		list->insertItem(to, moved);
		list->setCurrentItem(current);
	}

	/* the signal OBS's own drag raises; the multiview follows it */
	if (!QMetaObject::invokeMethod(list, "scenesReordered")) {
		blog(LOG_WARNING, "[bread-desk] could not raise 'scenesReordered'; the multiview may show the old order");
	}

	obs_frontend_save();
	return true;
}
```

- [x] **Step 2: Add the bar's new members**

In `breadstudio/desk/InputBar.hpp`:

Add to the includes: `#include <QPoint>`.

Add to the public section:

```cpp
	void syncAudioButtons(); /* set each tile's audio button from its base's mute state */
```

Add to the private section:

```cpp
	void addInput(const char *textKey, const char *typeId, bool blank);
	void showMenu(int index, const QPoint &globalPos);
	void renameDialog(obs_source_t *input);
	void deleteDialog(obs_source_t *input);

	std::vector<OBSSignal> muteSignals;
```

- [x] **Step 3: Give each tile its audio button**

In `breadstudio/desk/InputBar.cpp`:

Add to the includes:

```cpp
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
```

After `namespace desk {`, add:

```cpp
/* U+266A, the eighth note */
static const char *AudioGlyph = "\xE2\x99\xAA";

/* runs on whichever thread changed the mute state */
static void onMute(void *param, calldata_t *)
{
	auto *self = static_cast<InputBar *>(param);
	QMetaObject::invokeMethod(self, [self] { self->syncAudioButtons(); }, Qt::QueuedConnection);
}
```

In `makeFooter`, replace the line `buttons->addStretch(1);` with:

```cpp
	/* the same switch as the mixer strip's: the base's mute, inverted (spec §5.1) */
	OBSSource base = baseSource(input);
	auto *audio = new QToolButton(footer);
	audio->setObjectName("audio");
	audio->setText(QString::fromUtf8(AudioGlyph));
	audio->setToolTip(text("Audio.Tip"));
	audio->setCheckable(true);
	audio->setEnabled(hasAudio(base));
	audio->setChecked(hasAudio(base) && !obs_source_muted(base));
	connect(audio, &QToolButton::clicked, this, [this, index](bool on) {
		if ((size_t)index >= sources.size()) {
			return;
		}
		OBSSource target = baseSource(sources[(size_t)index]);
		if (target) {
			obs_source_set_muted(target, !on);
		}
	});
	buttons->addWidget(audio);
	buttons->addStretch(1);

	if (hasAudio(base)) {
		muteSignals.emplace_back(obs_source_get_signal_handler(base), "mute", onMute, this);
	}
```

In `refresh()`, add `muteSignals.clear();` as the first line. In `clear()`, add `muteSignals.clear();` as the first line.

Append inside `namespace desk`:

```cpp
void InputBar::syncAudioButtons()
{
	for (int i = 0; i < footerRow->count() && (size_t)i < sources.size(); i++) {
		QWidget *footer = footerRow->itemAt(i)->widget();
		auto *audio = footer ? footer->findChild<QToolButton *>("audio") : nullptr;
		OBSSource base = baseSource(sources[(size_t)i]);
		if (audio && base) {
			audio->setChecked(!obs_source_muted(base));
		}
	}
}
```

- [x] **Step 4: Add the Add button, the right-click menu and drag to reorder**

In `breadstudio/desk/InputBar.cpp`, at the end of the constructor:

```cpp
	/* + Add: one entry per base type that exists on this machine (spec §3) */
	struct InputType {
		const char *textKey;
		const char *id;
		bool blank;
	};
	static const InputType types[] = {
		{"Type.Camera", "dshow_input", false},     {"Type.Media", "ffmpeg_source", false},
		{"Type.Screen", "monitor_capture", false}, {"Type.Window", "window_capture", false},
		{"Type.Browser", "browser_source", false}, {"Type.Image", "image_source", false},
		{"Type.Colour", "color_source", false},    {"Type.Blank", "color_source", true},
		{"Type.Audio", "wasapi_input_capture", false},
	};

	auto *add = new QPushButton(text("Add"), this);
	auto *addMenu = new QMenu(add);
	for (const InputType &type : types) {
		if (!obs_get_latest_input_type_id(type.id)) {
			continue;
		}
		QAction *action = addMenu->addAction(text(type.textKey));
		connect(action, &QAction::triggered, this,
			[this, type] { addInput(type.textKey, type.id, type.blank); });
	}
	add->setMenu(addMenu);
	row->addWidget(add, 0, Qt::AlignTop);

	connect(display, &TileDisplay::menuRequested, this, &InputBar::showMenu);
	connect(display, &TileDisplay::moved, this, [](int from, int to) {
		if (moveInput(from, to)) {
			inputsChanged();
		}
	});
```

Append inside `namespace desk`:

```cpp
void InputBar::addInput(const char *textKey, const char *typeId, bool blank)
{
	OBSDataAutoRelease settings = obs_data_create();
	if (blank) {
		obs_data_set_int(settings, "color", 0xFF000000);
	}

	OBSSource input = createInput(typeId, obs_module_text(textKey), settings);
	if (!input) {
		return;
	}

	inputsChanged();
	toPreview(input);

	/* a Blank needs no setting up; everything else opens OBS's own properties window */
	OBSSource base = baseSource(input);
	if (!blank && base) {
		obs_frontend_open_source_properties(base);
	}
}

void InputBar::showMenu(int index, const QPoint &globalPos)
{
	if ((size_t)index >= sources.size()) {
		return;
	}
	OBSSource input = sources[(size_t)index];

	QMenu menu(this);
	QAction *rename = menu.addAction(text("Rename"));
	QAction *remove = menu.addAction(text("Delete"));

	QAction *chosen = menu.exec(globalPos);
	if (chosen == rename) {
		renameDialog(input);
	} else if (chosen == remove) {
		deleteDialog(input);
	}
}

void InputBar::renameDialog(obs_source_t *input)
{
	const QString current = QString::fromUtf8(obs_source_get_name(input));

	bool accepted = false;
	const QString name = QInputDialog::getText(this, text("Rename"), text("Rename.Prompt"), QLineEdit::Normal,
						   current, &accepted)
				     .trimmed();
	if (!accepted || name == current) {
		return;
	}

	if (!renameInput(input, name.toUtf8().constData())) {
		QMessageBox::warning(this, text("Rename"), text("Rename.Taken"));
		return;
	}
	inputsChanged();
}

void InputBar::deleteDialog(obs_source_t *input)
{
	switch (canDelete(input)) {
	case DeleteCheck::OnProgram:
		QMessageBox::warning(this, text("Delete"), text("Delete.OnProgram"));
		return;
	case DeleteCheck::LastInput:
		QMessageBox::warning(this, text("Delete"), text("Delete.Last"));
		return;
	case DeleteCheck::Ok:
		break;
	}

	const QString name = QString::fromUtf8(obs_source_get_name(input));
	const int hosts = layerUseCount(input);
	const QString question = hosts > 0 ? text("Delete.ConfirmLayers").arg(name).arg(hosts)
					   : text("Delete.Confirm").arg(name);
	if (QMessageBox::question(this, text("Delete"), question) != QMessageBox::Yes) {
		return;
	}

	/* OBS raises "scene list changed" when the scene goes, which refreshes the bar */
	deleteInput(input);
}
```

- [x] **Step 5: Add the nine number hotkeys**

In `breadstudio/desk/desk.cpp`:

Add to the includes: `#include <QString>` and `#include <vector>`.

Before `obs_module_load`, add:

```cpp
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
```

In `obs_module_load`, before the `blog` line:

```cpp
	for (size_t i = 0; i < NumberHotkeys; i++) {
		const QByteArray name = QString("BreadDesk.InputToPreview.%1").arg(i + 1).toUtf8();
		const QByteArray label = text("Hotkey.InputToPreview").arg(i + 1).toUtf8();
		numberHotkey[i] = obs_hotkey_register_frontend(name.constData(), label.constData(), onNumberHotkey,
							       (void *)(uintptr_t)i);
	}
	obs_frontend_add_save_callback(saveNumberHotkeys, nullptr);
```

- [x] **Step 6: Build**

Run: `.\breadstudio\build.ps1`
Expected: exit 0.

- [x] **Step 7: Check it by hand**

Start the program with the Inputs dock docked and Studio Mode on. Leave OBS's Scenes dock visible for this task only, to compare.

| Do | Expect |
|---|---|
| + Add → Colour | A new tile appears last, on Preview. OBS's properties window opens for it. In OBS's Sources dock the item fills the canvas and shows a lock. |
| + Add → Blank | A new black tile appears last. No window opens. |
| + Add → Media file, choose `bread-tone.wav` (from Task 3) | A new tile. Its ♪ is lit. Clicking ♪ unlights it, and OBS's Audio Mixer shows the source muted. Unmuting it in OBS's Audio Mixer lights ♪ again. |
| Look at the Colour tile's ♪ | Greyed out. |
| Right-click a tile → Rename → a new name | The tile and OBS's Scenes dock show the new name. |
| Rename → an empty name; then Rename → the name of another input | Each time: the "empty or already in use" message, and the name is unchanged. |
| Drag tile 1 onto tile 3 | The tile moves and all tiles renumber. OBS's Scenes dock shows the same new order. Preview and Program did not change. |
| Open View → Multiview (Windowed), then drag a tile | The multiview's order follows. |
| Close and restart the program | The order is as you left it. |
| Right-click the red (Program) tile → Delete | The "on Program" message. Nothing is deleted. |
| Right-click another tile → Delete → Yes | The tile goes, from the bar and from OBS's Scenes dock. |
| Delete tiles until one is left, then delete that one | The "last input" message. |
| Settings → Hotkeys: bind "Input 2 to Preview" to a key; press it | Whatever is tile 2 goes to Preview. Drag another tile into place 2 and press again: the new tile 2 goes to Preview. |

- [x] **Step 8: Commit**

```powershell
git add -A
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add, rename, delete and reorder inputs, with audio and number hotkeys"
```

---

### Task 6: Input settings and layers

**Files:**
- Modify: `breadstudio/desk/InputModel.hpp`, `breadstudio/desk/InputModel.cpp`
- Create: `breadstudio/desk/InputSettings.hpp`, `breadstudio/desk/InputSettings.cpp`
- Modify: `breadstudio/desk/InputBar.hpp`, `breadstudio/desk/InputBar.cpp`, `breadstudio/desk/CMakeLists.txt`

**Interfaces:**
- Consumes: `desk::inputs()`, `desk::baseSource()`, `desk::graph()`, `desk::wouldLoop()`, `desk::toPreview()`, `desk::text()`.
- Produces:
  - `struct desk::Layer { int64_t id; std::string name; bool visible; }`.
  - `std::vector<desk::Layer> desk::layers(obs_source_t *input)` — every item above the base, top first.
  - `bool desk::addLayer(obs_source_t *host, obs_source_t *layerInput)` — false when it would loop.
  - `void desk::removeLayer(obs_source_t *host, int64_t id)`, `void desk::moveLayer(obs_source_t *host, int64_t id, bool up)`, `void desk::setLayerVisible(obs_source_t *host, int64_t id, bool visible)`.
  - `class desk::InputSettings : public QDialog` with `InputSettings(obs_source_t *input, QWidget *parent)` and `obs_source_t *input() const`.

- [x] **Step 1: Add layers to the input model**

Append to `breadstudio/desk/InputModel.hpp`, after adding `#include <string>` at the top:

```cpp
struct Layer {
	int64_t id; /* the scene item's id */
	std::string name;
	bool visible;
};

/* every item above the base, top first */
std::vector<Layer> layers(obs_source_t *input);

/* puts `layerInput` on top of `host` at half size, centred. False when that would make
 * an input contain itself; nothing is added then. */
bool addLayer(obs_source_t *host, obs_source_t *layerInput);

void removeLayer(obs_source_t *host, int64_t id);
void moveLayer(obs_source_t *host, int64_t id, bool up); /* never below the base */
void setLayerVisible(obs_source_t *host, int64_t id, bool visible);
```

Append to `breadstudio/desk/InputModel.cpp`, inside `namespace desk`:

```cpp
std::vector<Layer> layers(obs_source_t *input)
{
	std::vector<Layer> all;
	obs_scene_enum_items(
		obs_scene_from_source(input),
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			static_cast<std::vector<Layer> *>(param)->push_back(
				{obs_sceneitem_get_id(item), obs_source_get_name(obs_sceneitem_get_source(item)),
				 obs_sceneitem_visible(item)});
			return true;
		},
		&all);

	/* enumerated bottom first: drop the base, then put the top first */
	if (!all.empty()) {
		all.erase(all.begin());
	}
	std::reverse(all.begin(), all.end());
	return all;
}

bool addLayer(obs_source_t *host, obs_source_t *layerInput)
{
	if (wouldLoop(graph(), uuidOf(host), uuidOf(layerInput))) {
		return false;
	}

	obs_sceneitem_t *item = obs_scene_add(obs_scene_from_source(host), layerInput);

	/* half size in the middle, so the base still shows and the layer is easy to grab */
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	struct vec2 scale;
	vec2_set(&scale, 0.5f, 0.5f);
	obs_sceneitem_set_scale(item, &scale);
	struct vec2 pos;
	vec2_set(&pos, (float)ovi.base_width / 4.0f, (float)ovi.base_height / 4.0f);
	obs_sceneitem_set_pos(item, &pos);

	return true;
}

static obs_sceneitem_t *findLayer(obs_source_t *host, int64_t id)
{
	return obs_scene_find_sceneitem_by_id(obs_scene_from_source(host), id);
}

void removeLayer(obs_source_t *host, int64_t id)
{
	if (obs_sceneitem_t *item = findLayer(host, id)) {
		obs_sceneitem_remove(item);
	}
}

void setLayerVisible(obs_source_t *host, int64_t id, bool visible)
{
	if (obs_sceneitem_t *item = findLayer(host, id)) {
		obs_sceneitem_set_visible(item, visible);
	}
}

void moveLayer(obs_source_t *host, int64_t id, bool up)
{
	const std::vector<Layer> all = layers(host); /* top first */
	const auto it = std::find_if(all.begin(), all.end(), [id](const Layer &layer) { return layer.id == id; });
	if (it == all.end()) {
		return;
	}
	if (up && it == all.begin()) {
		return;
	}
	if (!up && it + 1 == all.end()) {
		return; /* the next one down is the base */
	}

	obs_sceneitem_set_order(findLayer(host, id), up ? OBS_ORDER_MOVE_UP : OBS_ORDER_MOVE_DOWN);
}
```

- [x] **Step 2: Write the settings window**

`breadstudio/desk/InputSettings.hpp`:

```cpp
#pragma once

#include <obs.hpp>

#include <QDialog>

class QListWidget;

namespace desk {

/* The window behind a tile's gear: the base's properties and the layers list. It is not
 * modal, because layers are placed by dragging them in OBS's Preview pane. */
class InputSettings : public QDialog {
	Q_OBJECT

public:
	InputSettings(obs_source_t *input, QWidget *parent);

	obs_source_t *input() const { return source; }

private:
	void reload();
	int64_t selected() const;

	OBSSource source;
	QListWidget *list;
	bool loading = false;
};

} // namespace desk
```

`breadstudio/desk/InputSettings.cpp`:

```cpp
#include "InputSettings.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"

#include <obs-frontend-api.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace desk {

InputSettings::InputSettings(obs_source_t *input, QWidget *parent) : QDialog(parent), source(input)
{
	setWindowTitle(text("Settings.Title").arg(QString::fromUtf8(obs_source_get_name(source))));
	setAttribute(Qt::WA_DeleteOnClose);

	auto *layout = new QVBoxLayout(this);

	/* an empty scene made outside the desk has no base */
	auto *baseButton = new QPushButton(text("Settings.Base"), this);
	baseButton->setEnabled(baseSource(source).Get() != nullptr);
	connect(baseButton, &QPushButton::clicked, this, [this] {
		OBSSource base = baseSource(source);
		if (base) {
			obs_frontend_open_source_properties(base);
		}
	});
	layout->addWidget(baseButton);

	layout->addWidget(new QLabel(text("Settings.Layers"), this));
	list = new QListWidget(this);
	layout->addWidget(list, 1);
	connect(list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
		if (!loading) {
			setLayerVisible(source, item->data(Qt::UserRole).toLongLong(),
					item->checkState() == Qt::Checked);
		}
	});

	auto *buttons = new QHBoxLayout();
	layout->addLayout(buttons);

	/* a layer can only be another input (spec D2) */
	auto *add = new QPushButton(text("Settings.AddLayer"), this);
	auto *addMenu = new QMenu(add);
	add->setMenu(addMenu);
	connect(addMenu, &QMenu::aboutToShow, this, [this, addMenu] {
		addMenu->clear();
		for (OBSSource &other : inputs()) {
			if (other.Get() == source.Get()) {
				continue;
			}
			QAction *action = addMenu->addAction(QString::fromUtf8(obs_source_get_name(other)));
			connect(action, &QAction::triggered, this, [this, other] {
				if (!addLayer(source, other)) {
					QMessageBox::warning(this, windowTitle(), text("Settings.Loop"));
				}
				reload();
			});
		}
	});
	connect(addMenu, &QMenu::aboutToHide, addMenu, [addMenu] {
		/* the actions hold references to inputs; let them go once a choice has run */
		QMetaObject::invokeMethod(addMenu, [addMenu] { addMenu->clear(); }, Qt::QueuedConnection);
	});
	buttons->addWidget(add);

	auto *remove = new QPushButton(text("Settings.Remove"), this);
	connect(remove, &QPushButton::clicked, this, [this] {
		removeLayer(source, selected());
		reload();
	});
	buttons->addWidget(remove);

	auto *up = new QPushButton(text("Settings.Up"), this);
	connect(up, &QPushButton::clicked, this, [this] {
		moveLayer(source, selected(), true);
		reload();
	});
	buttons->addWidget(up);

	auto *down = new QPushButton(text("Settings.Down"), this);
	connect(down, &QPushButton::clicked, this, [this] {
		moveLayer(source, selected(), false);
		reload();
	});
	buttons->addWidget(down);

	layout->addWidget(new QLabel(text("Settings.Hint"), this));

	/* so the layers can be dragged in the Preview pane (spec §4.3) */
	toPreview(source);
	reload();
}

int64_t InputSettings::selected() const
{
	QListWidgetItem *item = list->currentItem();
	return item ? item->data(Qt::UserRole).toLongLong() : -1;
}

void InputSettings::reload()
{
	const int64_t keep = selected();

	loading = true;
	list->clear();
	for (const Layer &layer : layers(source)) {
		auto *item = new QListWidgetItem(QString::fromUtf8(layer.name.c_str()), list);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
		item->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(layer.id));
		if (layer.id == keep) {
			list->setCurrentItem(item);
		}
	}
	loading = false;
}

} // namespace desk
```

- [x] **Step 3: Give each tile its gear**

In `breadstudio/desk/InputBar.hpp`, add to the private section:

```cpp
	void openSettings(int index);
```

In `breadstudio/desk/InputBar.cpp`:

Add to the includes: `#include "InputSettings.hpp"` and `#include <algorithm>`.

After the `AudioGlyph` line, add:

```cpp
/* U+2699, the gear */
static const char *GearGlyph = "\xE2\x9A\x99";
```

In `makeFooter`, replace the line `buttons->addStretch(1);` (it follows `buttons->addWidget(audio);`) with:

```cpp
	auto *gear = new QToolButton(footer);
	gear->setText(QString::fromUtf8(GearGlyph));
	connect(gear, &QToolButton::clicked, this, [this, index] { openSettings(index); });
	buttons->addWidget(gear);
	buttons->addStretch(1);
```

In `refresh()`, after `sources = inputs();`, add:

```cpp
	/* close the settings window of an input that no longer exists */
	for (InputSettings *open : findChildren<InputSettings *>()) {
		const bool alive = std::any_of(sources.begin(), sources.end(),
					       [open](const OBSSource &input) { return input.Get() == open->input(); });
		if (!alive) {
			delete open;
		}
	}
```

In `clear()`, as the first line:

```cpp
	qDeleteAll(findChildren<InputSettings *>());
```

Append inside `namespace desk`:

```cpp
void InputBar::openSettings(int index)
{
	if ((size_t)index >= sources.size()) {
		return;
	}
	obs_source_t *input = sources[(size_t)index];

	for (InputSettings *open : findChildren<InputSettings *>()) {
		if (open->input() == input) {
			open->raise();
			open->activateWindow();
			return;
		}
	}

	(new InputSettings(input, this))->show();
}
```

- [x] **Step 4: Add the sources to the build**

In `breadstudio/desk/CMakeLists.txt`, add to the `target_sources(bread-desk PRIVATE …)` list, keeping it alphabetical: `InputSettings.cpp`, `InputSettings.hpp`.

- [x] **Step 5: Build**

Run: `.\breadstudio\build.ps1`
Expected: exit 0.

- [x] **Step 6: Check it by hand**

Start the program, Inputs dock docked, Studio Mode on. Use + Add to make `Red` (Colour, red), `Blue` (Colour, blue) and `Box` (Blank). In OBS's Scenes dock also make an empty scene `Empty`.

| Do | Expect |
|---|---|
| Press ⚙ on `Box` | The settings window opens; `Box` goes to Preview. The layers list is empty. |
| Add layer → `Red`; Add layer → `Blue` | Both appear in the list, `Blue` on top, both ticked. In Preview each is a half-size picture in the middle. `Box`'s tile shows them too. |
| In OBS's Preview pane, drag `Red` to the left half and `Blue` to the right half | They move; the tile follows. The black base cannot be dragged. |
| Untick `Blue` | It disappears from Preview and the tile. Tick it: it returns. |
| Select `Red`, press Up | `Red` is now on top in the list and in the picture where they overlap. |
| Select the bottom layer, press Down | Nothing changes. |
| Select `Blue`, press Remove | It leaves the list and the picture. |
| Press ⚙ on `Red`; Add layer → `Box` | The "contain itself" message. `Red`'s list stays empty. |
| Press ⚙ on `Box` again while its window is open | The existing window comes to the front; no second window. |
| Right-click `Red` → Delete | The question says it is a layer in 1 other input. Yes: `Red` goes, and `Box` loses that layer. `Red`'s settings window, if open, closes. |
| Add `Blue` back as a layer of `Box`, cut `Box` to Program, then right-click `Blue` → Delete | The "on Program" message: `Blue` is a layer of what is on Program. Nothing is deleted. |
| Look at `Empty`'s tile, then press its ⚙ | The tile is black and its ♪ is greyed out. In the window, "Base properties…" is greyed out. Nothing crashes. |
| With `Box` on Program, close and restart the program | `Box` still has its layers where you put them. |

- [x] **Step 7: Commit**

```powershell
git add -A
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add input settings with layers"
```

---

### Task 7: The mixer

**Files:**
- Create: `breadstudio/desk/Meter.hpp`, `breadstudio/desk/Meter.cpp`
- Create: `breadstudio/desk/Mixer.hpp`, `breadstudio/desk/Mixer.cpp`
- Modify: `breadstudio/desk/desk.cpp` (the probe is removed), `breadstudio/desk/CMakeLists.txt`

**Interfaces:**
- Consumes: `desk::inputs()`, `desk::baseSource()`, `desk::hasAudio()`, `desk::bed()`, `desk::AudioBed::afv()`, `desk::busMeters()`, `desk::text()`, `desk::MixerDockId`.
- Produces:
  - `class desk::Meter : public QWidget` with `void setDb(float db)`.
  - `class desk::Mixer : public QWidget` with `void rebuild()`, `void clear()`, `void syncMute()`.

- [x] **Step 1: Write the level bar**

`breadstudio/desk/Meter.hpp`:

```cpp
#pragma once

#include <QWidget>

namespace desk {

/* A vertical level bar: -60 dB at the bottom, 0 dB at the top. */
class Meter : public QWidget {
public:
	explicit Meter(QWidget *parent = nullptr);

	void setDb(float db);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	float level = 0.0f; /* 0 to 1 */
};

} // namespace desk
```

`breadstudio/desk/Meter.cpp`:

```cpp
#include "Meter.hpp"

#include <QPainter>

#include <algorithm>

namespace desk {

static const float FloorDb = -60.0f;

static float toLevel(float db)
{
	/* false for negative infinity and for NaN, which both mean silence */
	if (!(db > FloorDb)) {
		return 0.0f;
	}
	return std::min(1.0f, (db - FloorDb) / -FloorDb);
}

Meter::Meter(QWidget *parent) : QWidget(parent)
{
	setFixedWidth(10);
	setMinimumHeight(80);
}

void Meter::setDb(float db)
{
	const float next = toLevel(db);
	if (next != level) {
		level = next;
		update();
	}
}

void Meter::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.fillRect(rect(), QColor(0x20, 0x20, 0x20));

	/* green to -20 dB, yellow to -9 dB, red above: the bands OBS's own meters use */
	auto band = [&](float from, float to, const QColor &color) {
		const float top = std::min(level, to);
		if (top <= from) {
			return;
		}
		const int y0 = height() - (int)(top * (float)height());
		const int y1 = height() - (int)(from * (float)height());
		painter.fillRect(0, y0, width(), y1 - y0, color);
	};

	const float yellowAt = toLevel(-20.0f);
	const float redAt = toLevel(-9.0f);
	band(0.0f, yellowAt, QColor(0x4C, 0xC8, 0x4C));
	band(yellowAt, redAt, QColor(0xE0, 0xC8, 0x30));
	band(redAt, 1.0f, QColor(0xE0, 0x40, 0x40));
}

} // namespace desk
```

- [x] **Step 2: Write the mixer**

`breadstudio/desk/Mixer.hpp`:

```cpp
#pragma once

#include <obs.hpp>

#include <QTimer>
#include <QWidget>

#include <memory>
#include <vector>

class QHBoxLayout;
class QLabel;

namespace desk {

class Meter;

/* The mixer dock: one strip per input with audio and per OBS audio device, and a level
 * for each of the six buses (spec §5). */
class Mixer : public QWidget {
	Q_OBJECT

public:
	explicit Mixer(QWidget *parent = nullptr);
	~Mixer() override;

	void rebuild();  /* make the strips again from the inputs */
	void clear();    /* drop every reference; safe to call twice */
	void syncMute(); /* set each strip's audio button from its source's mute state */

private:
	struct Strip;

	void addStrip(const QString &title, obs_source_t *input, obs_source_t *source);
	void tick();

	QHBoxLayout *stripRow;
	Meter *busMeter[MAX_AUDIO_MIXES];
	QLabel *busNumber[MAX_AUDIO_MIXES];
	float busHold[MAX_AUDIO_MIXES];
	int ticks = 0;
	std::vector<std::unique_ptr<Strip>> strips;
	QTimer timer;
};

} // namespace desk
```

`breadstudio/desk/Mixer.cpp`:

```cpp
#include "Mixer.hpp"
#include "AudioBed.hpp"
#include "BusMeters.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "Meter.hpp"

#include <obs-audio-controls.h>

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QScrollArea>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <limits>

namespace desk {

static const float Silent = -std::numeric_limits<float>::infinity();
static const char *BusLetters = "MABCDE";

/* U+266A, the eighth note; U+1F3A7, headphones */
static const char *AudioGlyph = "\xE2\x99\xAA";
static const char *HeadphonesGlyph = "\xF0\x9F\x8E\xA7";

/* The AFV icon is drawn here, not copied from vMix: a screen with sound beside it. */
static QIcon afvIcon(const QPalette &palette)
{
	QPixmap pixmap(20, 20);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(palette.color(QPalette::ButtonText), 1.5));
	painter.drawRoundedRect(QRectF(2, 4, 11, 9), 1.5, 1.5);    /* the screen */
	painter.drawLine(QPointF(5, 16), QPointF(10, 16));         /* its stand */
	painter.drawArc(QRectF(12, 5, 5, 7), -60 * 16, 120 * 16);  /* sound */
	painter.drawArc(QRectF(13, 3, 7, 11), -60 * 16, 120 * 16);

	return QIcon(pixmap);
}

/* runs on whichever thread changed the mute state */
static void onMute(void *param, calldata_t *)
{
	auto *self = static_cast<Mixer *>(param);
	QMetaObject::invokeMethod(self, [self] { self->syncMute(); }, Qt::QueuedConnection);
}

struct Mixer::Strip {
	OBSSource source; /* the base of an input, or an OBS audio device */
	obs_fader_t *fader = nullptr;
	obs_volmeter_t *volmeter = nullptr;
	std::atomic<float> peakDb{Silent};
	OBSSignal muteSignal;
	QWidget *widget = nullptr;
	Meter *meter = nullptr;
	QToolButton *audio = nullptr;

	~Strip()
	{
		muteSignal.Disconnect();
		obs_volmeter_remove_callback(volmeter, onLevel, this);
		obs_volmeter_destroy(volmeter);
		obs_fader_destroy(fader);
		delete widget;
	}

	/* runs on OBS's audio thread; levels arrive in dB */
	static void onLevel(void *param, const float[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS],
			    const float[MAX_AUDIO_CHANNELS])
	{
		auto *self = static_cast<Strip *>(param);

		float highest = Silent;
		for (int ch = 0; ch < MAX_AUDIO_CHANNELS; ch++) {
			highest = std::max(highest, peak[ch]);
		}

		float seen = self->peakDb.load();
		while (highest > seen && !self->peakDb.compare_exchange_weak(seen, highest)) {
		}
	}
};

Mixer::Mixer(QWidget *parent) : QWidget(parent)
{
	auto *outer = new QHBoxLayout(this);
	outer->setContentsMargins(4, 4, 4, 4);

	auto *scroll = new QScrollArea(this);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidgetResizable(true);
	auto *holder = new QWidget(scroll);
	stripRow = new QHBoxLayout(holder);
	stripRow->setContentsMargins(0, 0, 0, 0);
	stripRow->setSpacing(4);
	stripRow->setAlignment(Qt::AlignLeft);
	scroll->setWidget(holder);
	outer->addWidget(scroll, 1);

	/* the buses: a level and a number each, no fader (spec D7) */
	auto *buses = new QHBoxLayout();
	buses->setSpacing(4);
	outer->addLayout(buses);
	for (int bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		auto *column = new QVBoxLayout();
		busMeter[bus] = new Meter(this);
		busNumber[bus] = new QLabel("-", this);
		busNumber[bus]->setFixedWidth(40);
		busNumber[bus]->setAlignment(Qt::AlignHCenter);
		busHold[bus] = Silent;

		column->addWidget(new QLabel(QString(QChar(BusLetters[bus])), this), 0, Qt::AlignHCenter);
		column->addWidget(busMeter[bus], 1, Qt::AlignHCenter);
		column->addWidget(busNumber[bus], 0, Qt::AlignHCenter);
		buses->addLayout(column);
	}

	connect(&timer, &QTimer::timeout, this, &Mixer::tick);
	timer.start(33);
}

Mixer::~Mixer() = default;

void Mixer::addStrip(const QString &title, obs_source_t *input, obs_source_t *source)
{
	auto owned = std::make_unique<Strip>();
	Strip *strip = owned.get();
	strip->source = source;

	strip->widget = new QWidget(this);
	strip->widget->setFixedWidth(108);
	auto *column = new QVBoxLayout(strip->widget);
	column->setContentsMargins(2, 2, 2, 2);
	column->setSpacing(2);

	auto *name = new QLabel(title, strip->widget);
	name->setToolTip(title);
	column->addWidget(name);

	/* level and fader */
	auto *levels = new QHBoxLayout();
	column->addLayout(levels, 1);
	strip->meter = new Meter(strip->widget);
	levels->addWidget(strip->meter);

	strip->fader = obs_fader_create(OBS_FADER_LOG);
	obs_fader_attach_source(strip->fader, source);
	auto *slider = new QSlider(Qt::Vertical, strip->widget);
	slider->setRange(0, 1000);
	slider->setValue((int)(obs_fader_get_deflection(strip->fader) * 1000.0f));
	connect(slider, &QSlider::valueChanged, this,
		[strip](int value) { obs_fader_set_deflection(strip->fader, (float)value / 1000.0f); });
	levels->addWidget(slider);

	strip->volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(strip->volmeter, source);
	obs_volmeter_add_callback(strip->volmeter, Strip::onLevel, strip);

	/* audio on/off, AFV, headphones */
	auto *switches = new QHBoxLayout();
	switches->setSpacing(2);
	column->addLayout(switches);

	strip->audio = new QToolButton(strip->widget);
	strip->audio->setText(QString::fromUtf8(AudioGlyph));
	strip->audio->setToolTip(text("Audio.Tip"));
	strip->audio->setCheckable(true);
	strip->audio->setChecked(!obs_source_muted(source));
	connect(strip->audio, &QToolButton::clicked, this,
		[strip](bool on) { obs_source_set_muted(strip->source, !on); });
	switches->addWidget(strip->audio);
	strip->muteSignal.Connect(obs_source_get_signal_handler(source), "mute", onMute, this);

	/* an OBS audio device is not an input: it is always on and has no AFV */
	if (input) {
		auto *afv = new QToolButton(strip->widget);
		afv->setIcon(afvIcon(palette()));
		afv->setToolTip(text("AFV.Tip"));
		afv->setCheckable(true);
		afv->setChecked(AudioBed::afv(input));
		OBSSource keep = input;
		connect(afv, &QToolButton::clicked, this, [keep](bool on) { bed().setAfv(keep, on); });
		switches->addWidget(afv);
	}

	auto *phones = new QToolButton(strip->widget);
	phones->setText(QString::fromUtf8(HeadphonesGlyph));
	phones->setToolTip(text("Headphones.Tip"));
	phones->setCheckable(true);
	phones->setChecked(obs_source_get_monitoring_type(source) != OBS_MONITORING_TYPE_NONE);
	connect(phones, &QToolButton::clicked, this, [strip](bool on) {
		obs_source_set_monitoring_type(strip->source, on ? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT
								  : OBS_MONITORING_TYPE_NONE);
	});
	switches->addWidget(phones);
	switches->addStretch(1);

	/* bus sends: OBS's per-source track switches */
	auto *sends = new QHBoxLayout();
	sends->setSpacing(0);
	column->addLayout(sends);
	const uint32_t mixers = obs_source_get_audio_mixers(source);
	for (int bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		const QString letter(QChar(BusLetters[bus]));
		auto *send = new QToolButton(strip->widget);
		send->setText(letter);
		send->setToolTip(text("Bus.Tip").arg(letter));
		send->setFixedWidth(17);
		send->setCheckable(true);
		send->setChecked((mixers & (1u << bus)) != 0);
		connect(send, &QToolButton::clicked, this, [strip, bus](bool on) {
			uint32_t mask = obs_source_get_audio_mixers(strip->source);
			mask = on ? (mask | (1u << bus)) : (mask & ~(1u << bus));
			obs_source_set_audio_mixers(strip->source, mask);
		});
		sends->addWidget(send);
	}

	stripRow->addWidget(strip->widget);
	strips.push_back(std::move(owned));
}

/* ponytail: every strip is made again on any change to the inputs, and a volume or bus
 * change made in OBS's own Advanced Audio Properties shows only after the next rebuild.
 * Follow the sources' "volume" and "audio_mixers" signals if that becomes a nuisance. */
void Mixer::rebuild()
{
	clear();

	std::vector<OBSSource> all = inputs();
	for (size_t i = 0; i < all.size(); i++) {
		OBSSource base = baseSource(all[i]);
		if (hasAudio(base)) {
			addStrip(QString("%1  %2").arg(i + 1).arg(QString::fromUtf8(obs_source_get_name(all[i]))),
				 all[i], base);
		}
	}

	/* the devices set in OBS's Settings -> Audio sit on output channels 1 to 6 */
	for (uint32_t channel = 1; channel <= 6; channel++) {
		OBSSourceAutoRelease device = obs_get_output_source(channel);
		if (hasAudio(device)) {
			addStrip(QString::fromUtf8(obs_source_get_name(device)), nullptr, device);
		}
	}
}

void Mixer::clear()
{
	strips.clear();
}

void Mixer::syncMute()
{
	for (auto &strip : strips) {
		strip->audio->setChecked(!obs_source_muted(strip->source));
	}
}

void Mixer::tick()
{
	for (auto &strip : strips) {
		strip->meter->setDb(strip->peakDb.exchange(Silent));
	}

	for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		const float db = busMeters().takePeakDb(bus);
		busMeter[bus]->setDb(db);
		busHold[bus] = std::max(busHold[bus], db);
	}

	/* the numbers change three times a second, so they can be read */
	if (++ticks < 10) {
		return;
	}
	ticks = 0;
	for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		busNumber[bus]->setText(busHold[bus] > -100.0f ? QString::number((double)busHold[bus], 'f', 1)
							      : QString("-"));
		busHold[bus] = Silent;
	}
}

} // namespace desk
```

- [x] **Step 3: Wire the mixer in and take the probe out**

In `breadstudio/desk/desk.cpp`:

- Delete everything from the line `/* ---- temporary probe for the audio bed check; removed in the mixer task ---- */` to the line `/* ---- end of probe ---- */`, both included.
- Delete the two `obs_frontend_add_tools_menu_item("Desk probe: …` lines in `obs_module_load`.
- Delete `#include <QTimer>`.
- Add `#include "Mixer.hpp"` to the includes.
- After `static InputBar *bar = nullptr;` add:

```cpp
static Mixer *mixer = nullptr; /* owned by its dock */
```

- In `inputsChanged()`, after `bar->refresh();` add `mixer->rebuild();`.
- In `load()`, after `bar->refresh();` add `mixer->rebuild();`.
- In `unload()`, after `bar->clear();` add `mixer->clear();`.
- In `obs_module_load`, after the two lines that make the input bar's dock, add:

```cpp
	mixer = new Mixer();
	obs_frontend_add_dock_by_id(MixerDockId, obs_module_text("Mixer"), mixer);
```

- [x] **Step 4: Add the sources to the build**

In `breadstudio/desk/CMakeLists.txt`, add to the `target_sources(bread-desk PRIVATE …)` list, keeping it alphabetical: `Meter.cpp`, `Meter.hpp`, `Mixer.cpp`, `Mixer.hpp`.

- [x] **Step 5: Build**

Run: `.\breadstudio\build.ps1`
Expected: exit 0.

Run: `Select-String -Path breadstudio\desk\*.cpp -Pattern 'probe'`
Expected: no output.

- [x] **Step 6: Check it by hand, and repeat the audio bed readings**

Start the program. Docks menu → tick "Mixer" and dock it. Inputs dock docked, Studio Mode on. Settings → Audio: every Global Audio Device Disabled. Use + Add to make `Tone` (Media file, `bread-tone.wav`, Loop ticked, "Restart playback when source becomes active" unticked) and `Other` (Blank).

| Do | Expect |
|---|---|
| Look at the mixer | One strip, `1  Tone` (or whichever number it has). No strip for `Other`. Six bus columns M A B C D E. |
| Reading A: `Tone` on Program, its AFV button lit | The number under bus M settles between -12.5 and -11.5. The strip's meter moves. |
| Reading B: `Other` on Program, then unlight AFV on the `Tone` strip | Bus M: between -12.5 and -11.5. |
| Reading C: `Tone` on Program, AFV still unlit | Bus M: between -12.5 and -11.5. **Not** about -6. |
| Light AFV again with `Other` on Program | Bus M shows `-`. |
| With `Tone` on Program: unlight send M on the strip | Bus M shows `-`. Light send A: bus A shows about -12. |
| Pull the fader down | The bus number falls. |
| Click ♪ on the strip | It unlights; the tile's ♪ unlights too; the bus goes to `-`. Click the tile's ♪: both light. |
| Light the headphones button | OBS's Advanced Audio Properties shows "Monitor and Output" for the source. |
| + Add → Audio device, choose a microphone | A new strip. Its AFV button is **unlit**: an audio-only input starts always on. Speaking moves bus M whatever is on Program. |
| Close and restart | AFV, sends, ♪ and fader are as you left them. |

- [x] **Step 7: Check that the collection unloads cleanly**

With the mixer showing strips and one settings window (⚙) open: Scene Collection → New; switch back; close the program. Read the log.
Expected: `Number of memory leaks: 0`, and no line containing `remaining`.

- [x] **Step 8: Commit**

```powershell
git add -A
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the mixer with bus sends, afv and bus levels"
```

---

### Task 8: The desk layout

**Files:**
- Create: `breadstudio/desk/DeskLayout.hpp`, `breadstudio/desk/DeskLayout.cpp`
- Modify: `breadstudio/desk/desk.cpp`, `breadstudio/desk/CMakeLists.txt`
- Modify: `frontend/OBSApp.cpp:358`
- Modify: `breadstudio/UPSTREAM-EDITS.md`

**Interfaces:**
- Consumes: `desk::InputsDockId`, `desk::MixerDockId`.
- Produces: `void desk::applyLayout()`, `void desk::applyLayoutOnFirstRun()`.

- [x] **Step 1: Write the layout**

`breadstudio/desk/DeskLayout.hpp`:

```cpp
#pragma once

namespace desk {

/* Studio Mode on, OBS's Scenes, Sources and Audio Mixer docks hidden, the desk's docks
 * placed (spec §4.5). Does nothing but log when a dock is not found (spec §6.2). */
void applyLayout();

/* applyLayout, the first time the program runs with the desk */
void applyLayoutOnFirstRun();

} // namespace desk
```

`breadstudio/desk/DeskLayout.cpp`:

```cpp
#include "DeskLayout.hpp"
#include "Desk.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QDockWidget>
#include <QMainWindow>

namespace desk {

void applyLayout()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main) {
		return;
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
	if (!inputs || !mixer || !scenes || !sources || !obsMixer || !controls) {
		return;
	}

	obs_frontend_set_preview_program_mode(true);

	scenes->setVisible(false);
	sources->setVisible(false);
	obsMixer->setVisible(false);

	for (QDockWidget *dock : {inputs, mixer, controls}) {
		dock->setFloating(false);
		main->addDockWidget(Qt::BottomDockWidgetArea, dock);
		dock->setVisible(true);
	}

	/* the input bar across the width; under it the mixer, with OBS's Controls to its right */
	main->splitDockWidget(inputs, mixer, Qt::Vertical);
	main->splitDockWidget(mixer, controls, Qt::Horizontal);
	main->resizeDocks({inputs, mixer}, {150, 260}, Qt::Vertical);
}

void applyLayoutOnFirstRun()
{
	config_t *config = obs_frontend_get_user_config();
	if (config_get_bool(config, "BreadDesk", "LayoutApplied")) {
		return;
	}

	applyLayout();
	config_set_bool(config, "BreadDesk", "LayoutApplied", true);
	config_save_safe(config, "tmp", nullptr);
}

} // namespace desk
```

- [x] **Step 2: Call it**

In `breadstudio/desk/desk.cpp`:

Add `#include "DeskLayout.hpp"` to the includes.

In `onEvent`, replace

```cpp
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		load();
		break;
```

with

```cpp
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		load();
		applyLayoutOnFirstRun();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		load();
		break;
```

In `obs_module_load`, before the `blog` line:

```cpp
	obs_frontend_add_tools_menu_item(obs_module_text("DeskLayout"), [](void *) { applyLayout(); }, nullptr);
```

In `breadstudio/desk/CMakeLists.txt`, add to the `target_sources(bread-desk PRIVATE …)` list, keeping it alphabetical: `DeskLayout.cpp`, `DeskLayout.hpp`.

- [x] **Step 3: Switch scene duplication off by default**

In `frontend/OBSApp.cpp`, line 358, change

```cpp
	config_set_default_bool(userConfig, "BasicWindow", "SceneDuplicationMode", true);
```

to

```cpp
	config_set_default_bool(userConfig, "BasicWindow", "SceneDuplicationMode", false);
```

In `breadstudio/UPSTREAM-EDITS.md`, under `## Edited`, add:

```
- `frontend/OBSApp.cpp` - Studio Mode's scene duplication is off by default, so an edit to an input shows wherever the input is
```

(If S0 Task 3 has already put `frontend/OBSApp.cpp` on the list, add this reason to its line instead of a second line.)

- [x] **Step 4: Build**

Run: `.\breadstudio\build.ps1`
Expected: exit 0.

- [x] **Step 5: Check the first run**

Remove the build's portable settings, so this is a first run. This deletes the test scenes made in earlier tasks, which are no longer needed:

```powershell
Remove-Item -Recurse -Force build_x64\rundir\RelWithDebInfo\config
```

Start the program.

| Look at | Expect |
|---|---|
| The main window | Preview and Program side by side on top. Under them the Inputs dock across the width. Under that the Mixer, with OBS's Controls dock to its right. |
| The Docks menu | Scenes, Sources and Audio Mixer are unticked. Ticking one shows it. |
| The Studio Mode gear menu, between Preview and Program | "Duplicate Scene" is unticked. |
| Move some docks about, then Tools → Desk layout | The arrangement comes back. |
| Close and restart | The arrangement is as you left it; the layout is not forced again. |

The three `addDockWidget`/`splitDockWidget`/`resizeDocks` lines are the part to adjust by eye if the picture differs from spec §4. Hiding the three OBS docks, switching Studio Mode on, and the first-run flag are not to be changed.

- [x] **Step 6: Commit**

```powershell
git add -A
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the desk layout and switch scene duplication off by default"
```

Expected from the check: `UPSTREAM-EDITS.md matches the 3 file(s) changed since 32.2.2.`

---

### Task 9: The smoke checklist and the done-when run

**Files:**
- Create: `breadstudio/docs/DESK-SMOKE.md`

**Interfaces:**
- Consumes: the finished desk.
- Produces: the checklist that is run after every upstream merge.

- [x] **Step 1: Write the checklist**

`breadstudio/docs/DESK-SMOKE.md`:

````markdown
# Desk smoke checklist

Run on an installed build before a release and after every upstream merge.
Needs two cameras, a media file, a microphone and a second monitor.
Spec: `2026-10-13-breadstudio-s1-desk-design.md`.

## A. The show (spec §1.1)

Start from a first run (no settings folder).

1. With + Add alone, make: camera 1, camera 2, the media file, a Blank with
   both cameras as layers placed side by side, and the microphone.
2. Open View → Multiview on the second monitor. Cut and fade between the four
   picture inputs for two minutes. On the bar and on the multiview, the red
   frame is always what is on Program and the green one what is on Preview.
3. The microphone is heard whatever is on Program. A camera's audio is heard
   only while that camera is on Program, alone or inside the two-box.
4. Test tone (`breadstudio/desk/test/make-tone.ps1`, added as a Media file
   input, looping): bus M reads -12 ± 0.5 with the tone on Program and AFV
   lit; with something else on Program and AFV unlit; and with the tone on
   Program and AFV unlit.
5. Switch the microphone's send A off and every other input's send A on.
   Settings → Output → Recording: record tracks 1 and 2. Record 20 seconds
   while speaking. The file's first audio track has the voice; its second
   does not.
6. Close and restart. Inputs, order, layers, levels, sends, ♪ and AFV are as
   they were.
7. OBS's Scenes and Sources docks were not opened at any point above.

## B. Failures (spec §8)

1. Studio Mode off: the warning row shows, and its button switches Studio
   Mode back on.
2. Delete the input on Program, and an input that is a layer of what is on
   Program: both refused.
3. Add an input as a layer of one of its own layers: refused.
4. Unplug a camera: its tile goes black; nothing else changes. Plug it back:
   the picture returns.
5. Scene Collection → New, then switch back, with a ⚙ window open: the bar
   and mixer follow each time. After closing, the log has
   `Number of memory leaks: 0` and no line containing `remaining`.
6. Make 26 inputs (Blank): the bar scrolls; the multiview shows the first 25.
7. Bind "Input 2 to Preview": it follows whatever is in place 2.
8. Press Cut on a tile during a three-second fade: ignored; the selected
   transition is unchanged afterwards.

## C. Things found by name (spec §6.2): check after every upstream merge

1. Drag a tile to a new place: it moves, the numbers follow, and the
   multiview follows. The log has no line
   `scene list 'scenes' was not found` and none about `scenesReordered`.
2. Tools → Desk layout: the arrangement is applied. The log has no line
   `dock '…' was not found`.

## D. A collection from stock OBS

1. Load a scene collection made in stock OBS that has: a scene with several
   sources, a scene with a group, a scene holding another scene, and an
   empty scene. Every scene is a tile and switches. The empty one is black,
   its ♪ is greyed out, and its ⚙ window has "Base properties…" greyed out.
````

- [x] **Step 2: Run the automated test once more**

```powershell
.\breadstudio\build.ps1
build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-desk-test.exe; $LASTEXITCODE
```

Expected: `LayerGraph: all checks passed`, then `0`.

- [x] **Step 3: Commit the checklist**

```powershell
git add -A
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the desk smoke checklist"
```

- [ ] **Step 4: OWNER GATE — run the checklist**

Sections A to D need two cameras, a microphone and a second monitor, so the owner runs them, on the build folder if no installer exists yet. A worker stops here and reports which items it could run without that hardware (B1, B3, B5 to B8, C, D) and their results.

Any item that fails is fixed in the task that owns the code, then the item is run again.

- [x] **Step 5: Record the result**

Add a dated line at the end of `breadstudio/docs/DESK-SMOKE.md` for each run: the date, the build (commit), installed or build folder, and the items that failed, or "all passed". Commit it with the message `breadstudio: record the desk smoke run`.

---

## Owed to S0

S1 can be built and checked before S0 is finished, but two things wait for it:

| Owed | Waits for | What to do then |
|---|---|---|
| The loop check running in CI (spec §9) | S0 Task 8, which creates `.github/workflows/breadstudio-windows.yaml` | Add one step after that workflow's build step: `build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-desk-test.exe`, failing the job on a non-zero exit. |
| The smoke run on an installed build (spec §1.1) | S0 Tasks 3 to 7, which make the branded build and the installer | Run `DESK-SMOKE.md` on the installed build and record it. S1 is signed off only after that run. |

Merging `s1-desk` into `main` after S0 Task 3 lands will meet both branches' additions to `breadstudio/UPSTREAM-EDITS.md` and both edits to `frontend/OBSApp.cpp`. Keep both in each file; they are on different lines.

