# The Project (`.bstd`) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** BreadStudio saves and opens a show as one file — a **Project** (`.bstd`) holding OBS's profile and scene collection together — from a **Project** menu in the ☰, with the media optionally packed in.

**Architecture:** One new desk unit, `ProjectFile`, is the container: a zip with stored entries, written and read by hand, holding `project.json` and the profile and scene files. A second unit, `Project`, is the desk's single owner of the open Project's path and unsaved state; it uses the OBS frontend API to switch profiles and scene collections, so none of OBS's own save/load is reimplemented. The ☰ menu gains a Project menu in place of Profile and Scene Collection.

**Tech Stack:** C++20, Qt 6 Widgets, zlib (already linked by libobs), the OBS frontend API. Part of the `bread-desk` plugin (`breadstudio/desk/`).

**Spec:** `breadstudio/docs/2026-10-16-breadstudio-cleanup-design.md` §2.2 (P1–P9), §4 and §5.

**Depends on:** nothing. **Plan 3** (the vMix import) depends on this: it produces a Project.

**Base:** `BreadStudio` at `main`, tip `760fddd68`.

---

## Environment

- Repo: `F:\Coding\BreadMedia\BreadStudio`, branch `main`, all work on `main`. Close the app before building: `Stop-Process -Name BreadStudio,obs64 -Force -ErrorAction SilentlyContinue`.
- **This machine builds with the VS 2022 preset.** Visual Studio 18 is not installed here; the CI runner has 2026. So build the preset, not a bare tree:
  ```
  cmd.exe /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cmake --preset windows-x64-vs2022 "-DOBS_VERSION_OVERRIDE=32.2.2" -DENABLE_WHATSNEW=OFF'
  cmd.exe /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cmake --build build_vs2022 --config RelWithDebInfo --target bread-desk'
  ```
  A single-target build of what you changed is usually enough; the app is at `build_vs2022\rundir\RelWithDebInfo\bin\64bit\BreadStudio.exe` (run with `--portable`).
- clang-format every changed file, then `--dry-run --Werror` must exit 0 (`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-format.exe`). Tabs, ≤120 columns, no BOM, a newline at end of file (clang-format will not add it — check the last byte is `0x0A`).
- `powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1` must exit 0 before every commit. It reads `UPSTREAM-EDITS.md`, and an entry may name a whole tree.
- Tests: `build_vs2022\breadstudio\desk\test-bin\RelWithDebInfo\bread-*-test.exe`, each exit 0. **This plan adds one: `bread-project-test`.**
- No GUI check is run by the implementer. Rows belong in `breadstudio/docs/DESK-SMOKE.md`; the owner runs them. **Append** a row to its section; never insert, never renumber — the file says so at the top because `## Runs` cites item numbers.
- **Do not touch the OMT-HX checkout, `breadstudio/omt-hx/` beyond your task, or anything the owner has uncommitted.** Stage explicit paths; check `git diff --cached` before every commit.

---

## The container (pinned here, because every task depends on it)

A `.bstd` is a **zip** whose entries are **stored**, not deflated. A show is text and already-compressed video, so compression buys nothing and costs a deflate implementation; zlib is used only for `crc32()`.

| Entry | What it is |
|---|---|
| `project.json` | the manifest: `{"project": "<name>", "breadstudio": "<version>", "obs": "<version>"}` |
| `profile/<relative path>` | every file of the OBS profile folder, as it sits |
| `scenes/<collection>.json` | the scene collection file, as it sits |
| `media/<relative path>` | the media the show uses — only when Save As... packs it (P6) |

OBS's own files are stored byte for byte, so a Project round-trips without loss. `project.json` is the only file BreadStudio writes by hand.

---

## File structure

| File | Responsibility |
|---|---|
| `breadstudio/desk/ProjectFile.hpp` / `.cpp` (create) | The container: write a set of files into a `.bstd`, read one back, read its manifest. No OBS types, no Qt widgets — paths and bytes only. |
| `breadstudio/desk/Project.hpp` / `.cpp` (create) | The desk's open Project: its path, its unsaved state, New/Open/Save/Save As, the recent list, and the profile + scene collection switching through the frontend API. |
| `breadstudio/desk/DeskLayout.cpp` (modify) | The ☰'s **Project** menu in place of Profile and Scene Collection. |
| `breadstudio/desk/InputBar.cpp` (modify) | Nothing, unless the unsaved dot needs a title update hook — see Task 4. |
| `breadstudio/desk/CMakeLists.txt` (modify) | The four new files, and the `bread-project-test` executable. |
| `breadstudio/desk/test/ProjectFileTest.cpp` (create) | The container's tests: no OBS, no Qt. |
| `breadstudio/desk/data/locale/en-US.ini` (modify) | The Project menu's labels. |
| `breadstudio/docs/DESK-SMOKE.md` (modify) | The owner's rows. |

---

### Task 1: `ProjectFile` writes and reads the container

**Files:**
- Create: `breadstudio/desk/ProjectFile.hpp`, `breadstudio/desk/ProjectFile.cpp`
- Create: `breadstudio/desk/test/ProjectFileTest.cpp`
- Modify: `breadstudio/desk/CMakeLists.txt`

**Step 1: write the failing test.** In `breadstudio/desk/test/ProjectFileTest.cpp`, following the style of the desk's other tests (plain C++, no OBS, no Qt, assert-and-print, ends with `exit 0`):

```cpp
// a zip with stored entries, and the manifest, round-trip exactly
void testRoundTrip();
// the manifest's fields survive
void testManifest();
// a file that is not a zip, and one whose manifest is missing, are refused
void testRefused();
```

Cases to assert:
- write the entries `{"project.json": <json>, "profile/basic.ini": "a=1", "scenes/Untitled.json": "{}"}` into a temp file, read them back, and compare every byte;
- the manifest's `project`, `breadstudio` and `obs` come back as written;
- reading a file of random bytes, and a zip with no `project.json`, both report a clear failure (a `std::string` error) rather than returning empty entries;
- **a file the plan's own writer made is a zip another program can read**: assert the four signature bytes `PK\x03\x04` at the start, and that the first entry's name is `project.json`.

**Step 2: run it and watch it fail** (it will not compile — the header does not exist):
```powershell
cmake --build build_vs2022 --config RelWithDebInfo --target bread-project-test
& "build_vs2022\breadstudio\desk\test-bin\RelWithDebInfo\bread-project-test.exe"
```

**Step 3: implement `ProjectFile`.** The interface, in `ProjectFile.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace desk {

/* One entry of a .bstd container: a path inside it, and the bytes at that path. */
struct ProjectEntry {
	std::string path;
	std::vector<uint8_t> bytes;
};

/* The manifest a Project carries, from its project.json. */
struct ProjectManifest {
	std::string project;
	std::string breadstudio;
	std::string obs;
};

/* Writes `entries` as a .bstd at `path`: a zip whose entries are stored, so a show
 * is text and already-compressed media and nothing is bought by deflating it. The
 * manifest must be one of the entries, under the name ManifestPath. An empty string
 * is returned on success, and the reason otherwise. */
std::string writeProjectFile(const std::string &path, const std::vector<ProjectEntry> &entries);

/* Reads a .bstd back. False, with `error` set, when the file is not one. */
bool readProjectFile(const std::string &path, std::vector<ProjectEntry> &entries, std::string &error);

/* The manifest's fields as a project.json. */
std::string manifestJson(const ProjectManifest &manifest);
bool manifestFromJson(const std::string &json, ProjectManifest &manifest, std::string &error);

/* the name project.json is stored under */
inline constexpr const char *ManifestPath = "project.json";

} // namespace desk
```

Implementation notes that matter:
- a zip local header is `PK\x03\x04` (30 bytes) then the name then the data; the central directory is `PK\x01\x02` per entry and ends with `PK\x05\x06`; sizes and offsets are little-endian `uint32_t`;
- **CRC32 comes from zlib** (`#include <zlib.h>`, `crc32(0, data, len)`), which libobs already links — add `ZLIB::ZLIB` to `bread-desk`'s and `bread-project-test`'s `target_link_libraries` if the link needs it;
- the manifest is small hand-written JSON: escape `"` and `\` in values, and accept whitespace when reading. Do not add a JSON library for three strings.
- keep it in one file with `namespace desk` and no globals.

**Step 4: run the test again** — it must pass and print its own `all checks passed` line with exit 0.

**Step 5: commit** (stage only these four files):
```
breadstudio: write and read the Project container, a zip with stored entries
```

---

### Task 2: Every `bread-project-test` case is proven, and the CMake entry is right

**Files:**
- Modify: `breadstudio/desk/CMakeLists.txt`
- Modify: `breadstudio/desk/test/ProjectFileTest.cpp`

**Step 1:** in `breadstudio/desk/CMakeLists.txt`, add the four new files to the `bread-desk` `target_sources` list (alphabetically, beside `Project`/`ProjectFile`) — `ProjectFile.cpp` and `ProjectFile.hpp` in Task 1, `Project.cpp`/`Project.hpp` in Task 3 — and a test executable modelled exactly on `bread-multiview-test`:
```cmake
# The Project container: no OBS, no Qt.
add_executable(bread-project-test test/ProjectFileTest.cpp ProjectFile.cpp)
target_include_directories(bread-project-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bread-project-test PRIVATE ZLIB::ZLIB)
set_target_properties(
  bread-project-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/desk/test-bin" FOLDER plugins/bread-desk
)
```
**If `ZLIB::ZLIB` is not found in that directory scope, use the `zlibstatic` the OBS deps ship** and say so in the commit message.

**Step 2:** build and run `bread-project-test`; every case from Task 1 must pass.

**Step 3:** run the other four desk test exes; each must still exit 0.

**Step 4: commit:**
```
breadstudio: build the Project container's test beside the desk's other tests
```

---

### Task 3: `Project` owns the open file and switches profile and collection

**Files:**
- Create: `breadstudio/desk/Project.hpp`, `breadstudio/desk/Project.cpp`
- Modify: `breadstudio/desk/CMakeLists.txt`
- Modify: `breadstudio/desk/desk.cpp`

**Step 1:** implement `Project`, one object with the desk's whole Project lifecycle. Put the plain decision logic in **pure functions** so they can be tested, and keep the OBS calls to thin wrappers:

```cpp
namespace desk {

/* The Project the desk has open. One at a time (P2). */
class Project {
public:
	/* the open Project's path, empty for one never saved */
	std::string path() const;
	/* the name shown in the title bar: the file's stem, or "Untitled" */
	std::string name() const;
	/* true while it has changes not written (P9) */
	bool changed() const;

	/* a fresh, empty Project (P3) */
	void createNew();
	/* reads `path` and switches the profile and the scene collection to what it holds */
	bool open(const std::string &path, std::string &error);
	/* writes to the open path, or asks for one when there is none */
	bool save(std::string &error);
	/* writes a copy at `path`, packing or leaving out the media per `withMedia`, and
	 * leaving the stream key out per `withStreamKey` (P6, P7) */
	bool saveAs(const std::string &path, bool withMedia, bool withStreamKey, std::string &error);

	/* the recent list, most recent first, at most ten (P8) */
	std::vector<std::string> recent() const;
};

/* Pure, and tested: the manifest for a Project whose name is `name`. */
ProjectManifest manifestFor(const std::string &name);
/* Pure, and tested: where a Project's profile and collection are unpacked inside
 * the app's own config, given the manifest (P4). */
std::string profileDirFor(const std::string &appData, const std::string &name);
std::string sceneFileFor(const std::string &appData, const std::string &name);

} // namespace desk
```

OBS's own save/load does the work, as the spec says (P4):
- **the profile**: `obs_frontend_get_current_profile()` and `obs_frontend_get_current_profile_path()` to find the folder; `obs_frontend_set_current_profile()` to switch. Its files are read from disk and put in the container under `profile/`.
- **the scene collection**: `obs_frontend_get_current_scene_collection()` and the config's collection path; `obs_frontend_set_current_scene_collection()` to switch. OBS writes it on save; call `obs_frontend_save()` before reading it into the container.
- **changedness** comes from `obs_frontend_add_save_callback` (OBS calls it when the profile or the collection is written) and from the desk's own edits; keep it a plain bool with a setter the callback calls.
- **the recent list** lives in the plugin's config beside `BreadDesk/LayoutVersion` (`obs_frontend_get_user_config()`, section `BreadDesk`, key `RecentProjects`, one path per line) — reuse the pattern `applyLayoutOnFirstRun` already uses.
- **the stream key**: the profile holds it. With `withStreamKey` false, blank the service settings the profile carries (`Basic/Stream/Service`, `Server`, `Key`) in the copy put in the container, never in the live profile.

**Step 2:** add a pure test to `ProjectFileTest.cpp` (or a second small test file, your call — say which) for `manifestFor`, `profileDirFor` and `sceneFileFor`: a name with a path separator, spaces and dots must not escape the config root; the manifest carries the versions it is given.

**Step 3:** wire it into `desk.cpp`: construct the one `Project` in `obs_module_load`, release it on `OBS_FRONTEND_EVENT_EXIT`, and register the save callback.

**Step 4:** build `bread-desk` and `bread-project-test`; no warnings; all tests exit 0; `check-edits.ps1` exit 0.

**Step 5: commit:**
```
breadstudio: give the desk a Project, one file holding a profile and a scene collection
```

---

### Task 4: The ☰'s Project menu, and the title bar's unsaved dot

**Files:**
- Modify: `breadstudio/desk/DeskLayout.cpp`, `breadstudio/desk/DeskLayout.hpp`
- Modify: `breadstudio/desk/desk.cpp`
- Modify: `breadstudio/desk/data/locale/en-US.ini`
- Modify: `breadstudio/docs/DESK-SMOKE.md`

**Step 1:** in `obsMainMenu()` (`DeskLayout.cpp`), the ☰ currently keeps `menu_File`, `profileMenu` and `sceneCollectionMenu`. Change the keep list to `menu_File` and `menu_File`'s siblings minus the two that Project replaces, and **build the desk's own Project menu** in the popup, before File:

```
Project ▸ New
          Open…               Ctrl+O
          Save                Ctrl+S
          Save As…
          Import vMix Preset…      ← plan 3 adds this; leave it out here
          ─────
          Recent            ▸ <the ten>
          Reveal in Explorer
```

Its actions call the `Project` object; `Save As…` asks for a path with `QFileDialog::getSaveFileName` (filter `*.bstd`), the media tick and the stream-key tick as two `QCheckBox`es in a small dialog, and `Recent` is rebuilt from `Project::recent()` each time the menu is shown (`aboutToShow`), because it changes.
- The locale keys go in `breadstudio/desk/data/locale/en-US.ini` beside the desk's other labels: `Project`, `Project.New`, `Project.Open`, `Project.Save`, `Project.SaveAs`, `Project.Recent`, `Project.Reveal`, `Project.Media`, `Project.StreamKey`.
- Look up OBS's own actions the way the About entry already is (`main->findChild<QAction *>("actionShowAbout")`) only for what stays; the two dropped menus are simply not added, and their actions are not deleted.
- **Keep the separators tidy**: the File menu's drop loop already removes trailing and leading separators when it removes something.

**Step 2:** the unsaved dot. The title bar is OBS's (`OBSBasic::UpdateTitleBar`). Do **not** edit `OBSBasic`. Instead, in `desk.cpp`, follow `OBS_FRONTEND_EVENT_*` and the Project's changed state, and set the window title from the desk: keep OBS's title as its base, and append ` •` while there are unsaved changes. Implement it in `DeskLayout` as `void applyProjectTitle()` — read the current title, strip any trailing ` •`, and re-add it when `Project::changed()` — and call it at load and whenever the changed state flips.

**Step 3:** append the owner's rows to `breadstudio/docs/DESK-SMOKE.md`, in the Project's own section (add one if there is none), **appended, never inserted**:
1. the ☰ has a **Project** menu with New, Open…, Save, Save As…, Recent, Reveal in Explorer, and no Profile or Scene Collection menu;
2. a show is built, **Save As…** writes a `.bstd`, the app is closed, and **Open…** brings back the inputs, order, layers, levels, bus sends and AFV;
3. **Save** after a change writes without asking for a path, and the title bar shows ` •` until it does;
4. a `.bstd` opened by hand in Windows Explorer, or by any zip tool, shows `project.json`, `profile/` and `scenes/`;
5. **Save As…** with the media tick packs the media, and the file opens on another machine with every file in place;
6. **Save As…** with the stream-key tick off writes a copy whose profile carries no stream key.

**Step 4:** build, `check-edits.ps1` exit 0, all six test exes exit 0, clang-format clean, every touched file ends with a newline.

**Step 5: commit:**
```
breadstudio: put the Project in the menu, and mark unsaved work in the title
```

---

## Self-review

**Spec coverage.** P1 (one file, everything) — Tasks 1 and 3. P2 (one at a time, New/Open/Save/Save As/Recent) — Tasks 3 and 4. P3 (the menu, Profile and Scene Collection dropped) — Task 4. P4 (it *is* OBS's pair, unpacked and switched through the frontend API) — Task 3. P5 (the container and its manifest) — Task 1. P6 (sharing is the file; media may be packed) — Task 3 and the Save As dialog in Task 4. P7 (the stream key, left out on request) — Task 3. P8 (the recent list in the plugin's config) — Task 3. P9 (the unsaved dot) — Task 4. §7's failures (a file that is not a Project, a name that collides, unsaved changes before opening another) — Tasks 1 and 3; the collision and the unsaved-changes prompt are thin wrappers over what Task 3 already exposes, and if they turn out to need the plan to name them, say so rather than inventing a design.

**Not in this plan:** the vMix import (plan 3) and the installer step. The Import entry is deliberately absent from the menu above so plan 3 adds it in one line.

**Placeholder scan:** no TBD; every code step carries its code; the two pure functions named in Task 3 are specified tightly enough to write a test first.

**Type consistency:** `ProjectEntry`, `ProjectManifest`, `ManifestPath`, `writeProjectFile`, `readProjectFile`, `manifestJson`, `manifestFromJson` are defined once, in Task 1, and used by name thereafter. `Project::path/name/changed/createNew/open/save/saveAs/recent` and the three pure helpers are defined once, in Task 3, and Task 4 uses those names.

**Two risks worth stating up front.**
1. **`ZLIB::ZLIB` in the test target** — the desk's test executables link nothing but libobs-free sources today; Task 2 says what to do if the link target is not visible there.
2. **The profile folder's location is OBS's business** — `obs_frontend_get_current_profile_path()` is the frontend's answer, and it is what the container packs. If it turns out to name the *settings* file rather than the folder, follow what the code shows and correct this plan in the commit that discovers it, rather than guessing a path.