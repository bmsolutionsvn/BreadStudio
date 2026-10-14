# BreadStudio — NDI input, its tab and the DLL Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An operator adds vMix, NDI Tools or any other NDI sender from an NDI tab in the Input Select window, with vMix's three checkboxes, on a PC where nothing was installed but BreadStudio, and the sender's tally follows what Program and Preview really draw.

**Architecture:** A new OBS plugin, `breadstudio/ndi` (built as `bread-ndi.dll`), holds one async source with DistroAV's id, `ndi_source`, cut down from DistroAV 6.2.2's input code. It loads NDI's DLL at run time, from BreadStudio's own folder first and NDI's runtime folder second; the build copies that DLL from the NDI SDK beside `obs64.exe`. The plugin publishes the announced sources and, when NDI is off, the reason, through its settings, and takes the "Also search" addresses through two procedures on OBS's global proc handler, so the desk links nothing of NDI. The desk's tile grid is moved out of Plan B's `OmtTab` into a `SourceTiles` widget that the OMT and NDI tabs share. Tally comes from Plan B's `OnAir.hpp`.

**Tech Stack:** C++17, Qt 6 Widgets, libobs and obs-frontend-api from OBS Studio 32.2.2, the NDI 6 SDK's MIT-licensed headers (copied into the tree) and its runtime DLL (copied at build time from `%NDI_SDK_DIR%`), Win32 `LoadLibraryExW`, CMake (OBS's `windows-x64` preset), Visual Studio 2026, Windows PowerShell 5.1.

**Spec:** `breadstudio/docs/2026-10-14-breadstudio-network-inputs-design.md`. This is its Plan C (§12): §4.1 for NDI, §6 row 4, §6.1, §6.2 for one more plugin, §7, §8 for NDI, and the NDI parts of §9, §10 and §11. Read it before any task. Section numbers below (§) refer to it.

**Needs first:** Plan B (`2026-10-14-breadstudio-omt-input.md`) merged. Task 4 moves code out of Plan B's `OmtTab` and Task 3 includes Plan B's `breadstudio/shared/OnAir.hpp`. Read both files as they are on `main` before those tasks; where they differ from Plan B's text, keep their differences.

## What reading the code changed

Checked on 2026-10-14 against DistroAV 6.2.2 (`9b1635f7be1aaa0c23db59c81b24e73daada34fa`), the NDI 6 SDK 6.3.2 installed on this PC, and `main` at `29ac9df5a`:

| Found | Consequence |
|---|---|
| DistroAV is "GPL version 2, or (at your option) any later version", the same terms as OBS. Its NDI input is `src/ndi-source.cpp` (1310 lines); its DLL loader is `load_ndilib()` in `src/plugin-main.cpp`, built on `QLibrary`. | §7.1's first step is done here. The plugin keeps DistroAV's copyright header on the two files derived from it. The loader is rewritten on `LoadLibraryExW`, so the plugin needs no Qt. |
| Most of DistroAV's source is options the desk does not show: PTZ, behaviour on hide, timeout, sync mode, frame sync, hardware-acceleration request, alpha fix filter, colour range and space, web control URL. | Cut. What stays: the source name, bandwidth (Low Bandwidth, Audio Only), and OBS buffering (Increase Buffer Size). Colour is fixed at DistroAV's defaults (BT.709, partial range); timestamps use DistroAV's default (the sender's timecode). |
| DistroAV registers `ndi_source` with settings `ndi_source_name`, `ndi_bw_mode` (0 highest, 1 lowest, 2 audio only) and `latency` (2 = unbuffered). | The same id and keys, so a scene collection made in OBS with DistroAV opens here as NDI inputs. §6.1 is amended: NDI's list is `ndi_source_name`, not `address`. |
| DistroAV asks NDI for `UYVY_BGRA` in its default latency mode and for `fastest` otherwise. `fastest` gives `UYVA` for a source with alpha, which DistroAV hands to OBS as `UYVY`, losing the alpha. | Always `UYVY_BGRA`: UYVY without alpha, BGRA with it. |
| NDI's receiver "the moment that we do connect to something … will automatically be sent the tally state" (`Processing.NDI.Recv.h`). | As with OMT, tally is sent only when it changes. |
| NDI names a source `MACHINE (name)` (`Processing.NDI.structs.h`), the same form as OMT. | Plan B's `groupBySender` and `sourceName` serve NDI unchanged. |
| A finder's `p_extra_ips` takes "a comma separated list" of IP addresses to ask directly. | "Also search" is that list. The plugin keeps it in its own config file and makes a new finder when it changes. |
| OBS loads every DLL in `obs-plugins\64bit` as a plugin. | NDI's DLL goes beside `obs64.exe`, as libomt does, not beside the plugin. |
| No installer script exists yet (S0's `Rename-Product.ps1` is the only packaging script). | §7.2's "the installer script refuses a build with no DLL" waits for S0's installer task. The configure warns now. |
| OBS's About box is OBS's own file (`frontend/`). NDI's licence asks for its trademark line in About and for a link to ndi.video "close to all locations where NDI is used / selected". | The trademark line, the link and the credit to DistroAV go on the NDI tab, under the list. About is not edited; the spec is amended. |
| The NDI headers do not say whether BGRA from NDI is premultiplied. DistroAV hands it to OBS as it is and offers a filter to premultiply for senders that need it. | The plugin hands BGRA over as DistroAV does. Task 2's hand-check looks at an alpha sender's edges; the owner's run with vMix repeats it. |

The spec is amended in the same commit as this plan (§4.1, §6.1, §7.1, §7.2).

## Global Constraints

- Base: OBS Studio 32.2.2. New code goes under `breadstudio/ndi` and `breadstudio/desk`. The one OBS file edited is `plugins/CMakeLists.txt`, already on `breadstudio/UPSTREAM-EDITS.md`; its reason is reworded in the same commit (Task 1).
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1` passes before every commit.
- Commit messages: `breadstudio: <what changed>`. Never rebase or force-push `main`. Nothing is pushed: every `git push` is an owner gate.
- C++17 (OBS's setting): no designated initialisers; fill `obs_source_info` field by field, as DistroAV does. OBS builds with warnings as errors (`/W3 /WX /permissive-`): fix a warning with the smallest change, do not switch it off.
- OBS's code style: tabs, 120 columns, braces on every `if`.
- `Processing.NDI.Lib.x64.dll` is never committed. It is copied at build time from `%NDI_SDK_DIR%\Bin\x64` to the folder that holds `obs64.exe`, with `Processing.NDI.Lib.Licenses.txt`. NDI's headers (MIT) are committed under `breadstudio/ndi/ndi-sdk/include`.
- The two files derived from DistroAV (`ndi-source.cpp`, `NdiLoad.hpp`) keep DistroAV's copyright header, followed by a line naming the release and commit they come from.
- The source id `ndi_source` and the settings keys `ndi_source_name`, `ndi_bw_mode`, `latency` are DistroAV's and are not renamed.
- Every string the operator sees is in a locale file: `breadstudio/ndi/data/locale/en-US.ini` (read with `obs_module_text`) or `breadstudio/desk/data/locale/en-US.ini` (read with `desk::text`).
- Windows x64 only. Build from a Visual Studio 2026 developer shell with `cmake --build --preset windows-x64 --config RelWithDebInfo --target <target>`; the program must be closed first. Run the program from `build_x64\rundir\RelWithDebInfo\bin\64bit\obs64.exe --portable`; before a run, `build_x64\rundir` holds no `*sync-conflict*` file; after a normal close, the newest log ends with `Number of memory leaks: 0`.
- This PC has the NDI 6 SDK (`NDI_SDK_DIR=C:\Program Files\NDI\NDI 6 SDK`, 6.3.2), the NDI 6 Runtime (`NDI_RUNTIME_DIR_V6`) and NDI Tools (Test Patterns, Screen Capture, Studio Monitor).

## Review Focus

1. **A PC with no NDI.** With the DLL gone from BreadStudio's folder and no NDI runtime, the program must start, every other kind of input must work, NDI inputs already in the collection must load as black tiles and save back unchanged, and the NDI tab must say why NDI is off. Owned by Task 1's and Task 4's hand-checks.
2. **A sender that is not there.** An NDI input whose sender is absent, stops, or comes back must leave the program responsive and return by itself. Owned by Task 2's hand-check.
3. **Audio Only and Low Bandwidth.** Ticking Audio Only must leave the tile black with the sound still playing; Low Bandwidth must still give a picture. Changing either restarts the receiver; changing only Increase Buffer Size must not. Owned by Task 2's and Task 4's hand-checks.
4. **Tally only when it is true** (§8): as for OMT, a tile in the bar, or an input in the audio bed with AFV unlit, reads nothing. Owned by Task 3's hand-check, on the rule Plan B's `bread-onair-test` already pins.
5. **A collection made with DistroAV.** Its `ndi_source` inputs, saved with DistroAV's keys, must play here, and ⚙ must open them on the NDI tab with the right boxes ticked. Owned by Task 4's hand-check.

## File Structure

| Path | Responsibility |
|---|---|
| `breadstudio/ndi/CMakeLists.txt` | The plugin; NDI's DLL and licence beside `obs64.exe`; the test sender |
| `breadstudio/ndi/ndi-sdk/include/*.h`, `README.md` | NDI's MIT-licensed headers, and where they came from |
| `breadstudio/ndi/NdiLoad.hpp` | Finding and loading NDI's DLL (after DistroAV's `load_ndilib`) |
| `breadstudio/ndi/Ndi.hpp` | What the plugin's two files share |
| `breadstudio/ndi/ndi.cpp` | The module: loading NDI, the reason it is off, the finder, "Also search" |
| `breadstudio/ndi/ndi-source.cpp` | The source (after DistroAV's `ndi-source.cpp`): receiver thread, picture, sound, settings, tally |
| `breadstudio/ndi/test/NdiSend.cpp` | `bread-ndi-send`: a test sender that prints its tally |
| `breadstudio/ndi/data/locale/en-US.ini` | The plugin's strings |
| `breadstudio/desk/SourceTiles.hpp`, `.cpp` | The tile grid of announced sources, and reading a plugin's lists, shared by both tabs |
| `breadstudio/desk/OmtTab.hpp`, `.cpp` | Plan B's tab, now on `SourceTiles` |
| `breadstudio/desk/NdiTab.hpp`, `.cpp` | The NDI tab, for a new input and for an existing one |
| `breadstudio/desk/InputSelect.cpp` | NDI first in the row's tabs; the NDI tab for an existing NDI input |
| `breadstudio/desk/CMakeLists.txt`, `data/locale/en-US.ini` | New sources, the strings |
| `plugins/CMakeLists.txt`, `breadstudio/UPSTREAM-EDITS.md` | One more `add_subdirectory` line, and its reason |
| `breadstudio/docs/DESK-SMOKE.md` | Section G |

---

### Task 1: The plugin loads NDI, or says why not

**Files:**
- Create: `breadstudio/ndi/CMakeLists.txt`, `breadstudio/ndi/ndi-sdk/include/*.h`, `breadstudio/ndi/ndi-sdk/README.md`, `breadstudio/ndi/NdiLoad.hpp`, `breadstudio/ndi/Ndi.hpp`, `breadstudio/ndi/ndi.cpp`, `breadstudio/ndi/ndi-source.cpp`, `breadstudio/ndi/data/locale/en-US.ini`
- Modify: `plugins/CMakeLists.txt` (OBS file), `breadstudio/UPSTREAM-EDITS.md`

**Interfaces:**
- Consumes: the NDI SDK at `%NDI_SDK_DIR%`.
- Produces:
  - `const NDIlib_v6 *loadNdi(HMODULE &module, std::wstring &from);` in `NdiLoad.hpp`
  - In `Ndi.hpp`: `extern const NDIlib_v6 *ndiLib;` (null while NDI is off), `const std::string &ndiStatus();` (why NDI is off; empty when on), `std::vector<std::string> ndiSources();`, `obs_source_info ndiSourceInfo();`
  - The OBS source type `ndi_source`, "NDI Source", registered whether or not NDI loaded, so NDI inputs in a collection always load.
  - `bread-ndi.dll` in `obs-plugins\64bit`; `Processing.NDI.Lib.x64.dll` and `Processing.NDI.Lib.Licenses.txt` beside `obs64.exe`.

- [ ] **Step 1: Copy NDI's headers into the tree**

From the repo root:
```powershell
New-Item -ItemType Directory -Force breadstudio\ndi\ndi-sdk\include | Out-Null
Copy-Item "$env:NDI_SDK_DIR\Include\*.h" breadstudio\ndi\ndi-sdk\include\
(Get-Item "$env:NDI_SDK_DIR\Bin\x64\Processing.NDI.Lib.x64.dll").VersionInfo.FileVersion
Select-String -Path breadstudio\ndi\ndi-sdk\include\*.h -Pattern 'MIT license applies to this file' | Measure-Object | Select-Object -ExpandProperty Count
(Get-ChildItem breadstudio\ndi\ndi-sdk\include\*.h).Count
```
Expected: version `6.3.2.0`, and the two counts equal (every header carries the MIT notice; 16 on this PC). If a header lacks the notice, do not commit it: stop and report.

Create `breadstudio/ndi/ndi-sdk/README.md`:
```markdown
# NDI SDK headers

Copied unchanged from the NDI 6 SDK, version 6.3.2, `Include\` (2026-10-14).
Each file carries its own MIT licence notice. The rest of the SDK, and NDI's
runtime DLL, are not in git: the build copies the DLL from `%NDI_SDK_DIR%`
(network inputs spec §7.2). NDI® is a registered trademark of Vizrt NDI AB.
```

- [ ] **Step 2: Write the loader**

Create `breadstudio/ndi/NdiLoad.hpp`:
```cpp
/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

/* BreadStudio: after load_ndilib() in DistroAV 6.2.2 (9b1635f), src/plugin-main.cpp, rewritten on
 * LoadLibraryExW so that neither the plugin nor the test sender needs Qt. */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <Processing.NDI.Lib.h>
#include <Processing.NDI.DynamicLoad.h>

#include <string>
#include <vector>

/* NDI's DLL from the folder that holds the program first, where BreadStudio ships it, then from NDI's
 * runtime folder (network inputs spec §7.2). Null when neither has a usable one. `module` is the handle
 * to free at the end; `from` the file that was loaded. */
inline const NDIlib_v6 *loadNdi(HMODULE &module, std::wstring &from)
{
	std::vector<std::wstring> folders;

	wchar_t exe[MAX_PATH] = {};
	const DWORD length = GetModuleFileNameW(nullptr, exe, MAX_PATH);
	if (length > 0 && length < MAX_PATH) {
		std::wstring folder(exe, length);
		folders.push_back(folder.substr(0, folder.find_last_of(L"\\/") + 1));
	}

	wchar_t runtime[MAX_PATH] = {};
	const DWORD runtimeLength = GetEnvironmentVariableW(L"NDI_RUNTIME_DIR_V6", runtime, MAX_PATH);
	if (runtimeLength > 0 && runtimeLength < MAX_PATH) {
		folders.push_back(std::wstring(runtime, runtimeLength) + L"\\");
	}

	for (const std::wstring &folder : folders) {
		const std::wstring path = folder + L"Processing.NDI.Lib.x64.dll";
		module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
		if (!module) {
			continue;
		}
		auto load = reinterpret_cast<const NDIlib_v6 *(*)(void)>(GetProcAddress(module, "NDIlib_v6_load"));
		const NDIlib_v6 *lib = load ? load() : nullptr;
		if (lib) {
			from = path;
			return lib;
		}
		FreeLibrary(module);
		module = nullptr;
	}
	return nullptr;
}
```

- [ ] **Step 3: Write the module and an empty source**

Create `breadstudio/ndi/Ndi.hpp`:
```cpp
#pragma once

#include "NdiLoad.hpp"

#include <obs-module.h>

#include <string>
#include <vector>

/* NDI's functions; null while NDI is off */
extern const NDIlib_v6 *ndiLib;

/* why NDI is off, for the NDI tab (spec §10); empty when it is on */
const std::string &ndiStatus();

/* the names of the NDI sources announced so far, "MACHINE (name)" */
std::vector<std::string> ndiSources();

/* the source type, filled in ndi-source.cpp */
obs_source_info ndiSourceInfo();
```

Create `breadstudio/ndi/ndi.cpp`:
```cpp
/* The NDI plugin: loads NDI's DLL, registers the NDI source, and finds senders (network inputs spec §7). */
#include "Ndi.hpp"

#include <util/platform.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bread-ndi", "en-US")

const NDIlib_v6 *ndiLib = nullptr;

namespace {

HMODULE library = nullptr;
std::string status;

} // namespace

const std::string &ndiStatus()
{
	return status;
}

std::vector<std::string> ndiSources()
{
	return {};
}

const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

bool obs_module_load(void)
{
	std::wstring from;
	ndiLib = loadNdi(library, from);
	if (!ndiLib) {
		status = obs_module_text("Status.NoLibrary");
	} else if (!ndiLib->initialize()) {
		/* NDI needs SSE4.2; DistroAV calls this an unsupported CPU */
		status = obs_module_text("Status.NoStart");
		ndiLib->destroy();
		ndiLib = nullptr;
	} else {
		char *path = nullptr;
		os_wcs_to_utf8_ptr(from.c_str(), 0, &path);
		blog(LOG_INFO, "[bread-ndi] NDI %s loaded from %s", ndiLib->version(), path ? path : "?");
		bfree(path);
	}
	if (!status.empty()) {
		blog(LOG_WARNING, "[bread-ndi] %s", status.c_str());
	}

	/* registered even while NDI is off, so NDI inputs in a collection load and save back unchanged */
	const obs_source_info info = ndiSourceInfo();
	obs_register_source(&info);
	return true;
}

/* after every source is gone */
void obs_module_unload(void)
{
	if (ndiLib) {
		ndiLib->destroy();
		ndiLib = nullptr;
	}
	if (library) {
		FreeLibrary(library);
		library = nullptr;
	}
}
```

Create `breadstudio/ndi/ndi-source.cpp` (Task 2 fills it in):
```cpp
/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

/* BreadStudio: after DistroAV 6.2.2 (9b1635f), src/ndi-source.cpp, cut down to what the desk's NDI tab
 * sets: the source, Low Bandwidth, Audio Only and Increase Buffer Size. Left out: PTZ, behaviour on
 * hide, timeout, sync modes, frame sync, the hardware-acceleration request, the alpha filter, colour
 * settings, the web control URL, and DistroAV's tally, which follows OBS's "active" and "showing";
 * tally follows breadstudio/shared/OnAir.hpp instead (network inputs spec §8). */

#include "Ndi.hpp"

namespace {

const char *ndi_get_name(void *)
{
	return obs_module_text("NDISource");
}

void *ndi_create(obs_data_t *, obs_source_t *source)
{
	return source;
}

void ndi_destroy(void *) {}

} // namespace

obs_source_info ndiSourceInfo()
{
	obs_source_info info = {};
	info.id = "ndi_source"; /* DistroAV's, so a collection made with DistroAV opens here */
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name = ndi_get_name;
	info.create = ndi_create;
	info.destroy = ndi_destroy;
	info.icon_type = OBS_ICON_TYPE_CAMERA;
	return info;
}
```

Create `breadstudio/ndi/data/locale/en-US.ini`:
```ini
Description="Receives NDI. Based on DistroAV. NDI® is a registered trademark of Vizrt NDI AB."
NDISource="NDI Source"
Status.NoLibrary="NDI is off: its library, Processing.NDI.Lib.x64.dll, is not in BreadStudio's folder, and NDI's runtime is not installed."
Status.NoStart="NDI is off: its library was found but cannot start on this PC's processor."
```

- [ ] **Step 4: Write the build file**

Create `breadstudio/ndi/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.28...3.30)

add_library(bread-ndi MODULE)
add_library(OBS::bread-ndi ALIAS bread-ndi)

target_sources(bread-ndi PRIVATE Ndi.hpp NdiLoad.hpp ndi.cpp ndi-source.cpp)
target_include_directories(bread-ndi PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ndi-sdk/include")
target_link_libraries(bread-ndi PRIVATE OBS::libobs)

# NDI's DLL ships beside obs64.exe (spec §7.2), not beside the plugin: OBS loads every DLL in
# obs-plugins as a plugin. It comes from the NDI SDK on the build machine and is never committed.
set(NDI_SDK_DIR "$ENV{NDI_SDK_DIR}" CACHE PATH "NDI 6 SDK")
if(EXISTS "${NDI_SDK_DIR}/Bin/x64/Processing.NDI.Lib.x64.dll")
  add_custom_command(
    TARGET bread-ndi
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_EXECUTABLE_DESTINATION}"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different "${NDI_SDK_DIR}/Bin/x64/Processing.NDI.Lib.x64.dll"
      "${NDI_SDK_DIR}/Bin/x64/Processing.NDI.Lib.Licenses.txt" "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_EXECUTABLE_DESTINATION}"
    VERBATIM
  )
else()
  message(
    WARNING
    "No NDI SDK at '${NDI_SDK_DIR}': BreadStudio is built without NDI's DLL, and NDI stays off unless NDI's runtime is installed. Install the NDI 6 SDK, or set -DNDI_SDK_DIR=<path>."
  )
endif()

set_target_properties_obs(bread-ndi PROPERTIES FOLDER plugins/bread-ndi PREFIX "")
```

In `plugins/CMakeLists.txt`, under the line that adds `breadstudio/omt-hx`, add:
```cmake
add_subdirectory("${CMAKE_SOURCE_DIR}/breadstudio/ndi" "${CMAKE_BINARY_DIR}/breadstudio/ndi")
```
In `breadstudio/UPSTREAM-EDITS.md`, replace the `plugins/CMakeLists.txt` line with:
```
- `plugins/CMakeLists.txt` - three lines that add breadstudio/desk, breadstudio/omt-hx and breadstudio/ndi to the build
```

- [ ] **Step 5: Configure, build, and check the files**

From a VS 2026 developer shell in the repo root, with the program closed:
```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-ndi
Get-ChildItem build_x64\rundir\RelWithDebInfo\obs-plugins\64bit\bread-ndi.dll, build_x64\rundir\RelWithDebInfo\bin\64bit\Processing.NDI.Lib.x64.dll, build_x64\rundir\RelWithDebInfo\bin\64bit\Processing.NDI.Lib.Licenses.txt
```
Expected: the configure prints no NDI warning; `bread-ndi.dll` builds with no warning; all three files exist. A warning inside an NDI header: wrap the two `#include <Processing.NDI…>` lines in `NdiLoad.hpp` in `#pragma warning(push, 0)` / `#pragma warning(pop)` and say so in the commit message; do not edit the headers.

- [ ] **Step 6: Check by hand: with NDI, and without**

| Do | Expect |
|---|---|
| Start the program; close it | The log has `[bread-ndi] NDI … 6.3.2 … loaded from …\bin\64bit\Processing.NDI.Lib.x64.dll`, and ends with `Number of memory leaks: 0`. |
| Rename `bin\64bit\Processing.NDI.Lib.x64.dll` to `….dll.off`; start the program | The log has `NDI … loaded from C:\Program Files\NDI\NDI 6 Runtime\v6\…`: the runtime is the fallback. |
| Keep it renamed; in PowerShell `$env:NDI_RUNTIME_DIR_V6 = ''`, then start `obs64.exe --portable` from that shell | The program starts. The log has `[bread-ndi] NDI is off: its library … is not in BreadStudio's folder …`. A Blank input and a Stream input still work. |
| Close; rename the DLL back | — |

- [ ] **Step 7: Commit**

```powershell
git add plugins/CMakeLists.txt breadstudio/ndi breadstudio/UPSTREAM-EDITS.md
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the NDI plugin, which loads NDI's DLL from BreadStudio's folder"
```

---

### Task 2: The NDI source plays picture and sound

**Files:**
- Modify: `breadstudio/ndi/ndi-source.cpp`, `breadstudio/ndi/CMakeLists.txt`, `breadstudio/ndi/data/locale/en-US.ini`
- Create: `breadstudio/ndi/test/NdiSend.cpp`

**Interfaces:**
- Consumes: `ndiLib`, `ndiSources()` (still empty until Task 3; the source name is typed for now).
- Produces:
  - `struct ndi_source` in `ndi-source.cpp` with `source`, `name`, `bandwidth`, `worker`, `run`, `tally`, `tallyWait`. Task 3 adds the tally code that sets `tally`.
  - Settings: `ndi_source_name` (string), `ndi_bw_mode` (0 highest, 1 lowest, 2 audio only; default 0), `latency` (2 unbuffered, anything else buffered; default 2).
  - `bread-ndi-send.exe [name]`: sends a moving bar and a 1 kHz tone as `<THIS PC> (<name>)`, and prints `connections N  tally PGM PVW` once a second.

- [ ] **Step 1: Write the source**

Replace everything in `breadstudio/ndi/ndi-source.cpp` below the BreadStudio comment block (keep both comment blocks) with:
```cpp
#include "Ndi.hpp"

#include <media-io/video-io.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <thread>

namespace {

/* DistroAV's values, kept so its scene collections open here */
constexpr int BandwidthHighest = 0;
constexpr int BandwidthLowest = 1;
constexpr int BandwidthAudioOnly = 2;
constexpr int LatencyUnbuffered = 2;

struct ndi_source {
	obs_source_t *source = nullptr;
	std::string name;   /* "MACHINE (name)" */
	int bandwidth = -1; /* -1 until the first update */
	std::thread worker;
	std::atomic<bool> run{false};
	std::atomic<int> tally{0}; /* 1 program, 2 preview; worked out on the UI thread, sent by the worker */
	float tallyWait = 0.0f;    /* graphics thread */
};

speaker_layout layoutFor(int channels)
{
	switch (channels) {
	case 1:
		return SPEAKERS_MONO;
	case 2:
		return SPEAKERS_STEREO;
	case 3:
		return SPEAKERS_2POINT1;
	case 4:
		return SPEAKERS_4POINT0;
	case 5:
		return SPEAKERS_4POINT1;
	case 6:
		return SPEAKERS_5POINT1;
	case 8:
		return SPEAKERS_7POINT1;
	default:
		return SPEAKERS_UNKNOWN;
	}
}

void outputVideo(ndi_source *s, const NDIlib_video_frame_v2_t &video)
{
	obs_source_frame frame = {};
	switch (video.FourCC) {
	case NDIlib_FourCC_type_BGRA:
		frame.format = VIDEO_FORMAT_BGRA;
		break;
	case NDIlib_FourCC_type_BGRX:
		frame.format = VIDEO_FORMAT_BGRX;
		break;
	case NDIlib_FourCC_type_UYVY:
		frame.format = VIDEO_FORMAT_UYVY;
		break;
	default:
		return; /* UYVY_BGRA gives only these three */
	}

	frame.width = (uint32_t)video.xres;
	frame.height = (uint32_t)video.yres;
	frame.linesize[0] = (uint32_t)video.line_stride_in_bytes;
	frame.data[0] = video.p_data;
	/* DistroAV's defaults: the sender's timecode, BT.709, partial range */
	frame.timestamp = (uint64_t)(video.timecode * 100);
	video_format_get_parameters(VIDEO_CS_709, VIDEO_RANGE_PARTIAL, frame.color_matrix, frame.color_range_min,
				    frame.color_range_max);
	obs_source_output_video(s->source, &frame);
}

void outputAudio(ndi_source *s, const NDIlib_audio_frame_v3_t &audio)
{
	const int channels = std::min(audio.no_channels, 8);
	obs_source_audio out = {};
	out.speakers = layoutFor(channels);
	if (out.speakers == SPEAKERS_UNKNOWN) {
		return;
	}
	out.format = AUDIO_FORMAT_FLOAT_PLANAR;
	out.samples_per_sec = (uint32_t)audio.sample_rate;
	out.frames = (uint32_t)audio.no_samples;
	out.timestamp = (uint64_t)(audio.timecode * 100);
	for (int c = 0; c < channels; c++) {
		out.data[c] = audio.p_data + (size_t)c * (size_t)audio.channel_stride_in_bytes;
	}
	obs_source_output_audio(s->source, &out);
}

/* NDI's receiver connects, and reconnects after its sender comes back, by itself */
void worker_main(ndi_source *s)
{
	NDIlib_recv_create_v3_t desc;
	desc.source_to_connect_to.p_ndi_name = s->name.c_str();
	desc.color_format = NDIlib_recv_color_format_UYVY_BGRA; /* BGRA when the sender has alpha */
	desc.bandwidth = s->bandwidth == BandwidthLowest      ? NDIlib_recv_bandwidth_lowest
			 : s->bandwidth == BandwidthAudioOnly ? NDIlib_recv_bandwidth_audio_only
							      : NDIlib_recv_bandwidth_highest;
	desc.allow_video_fields = false;
	desc.p_ndi_recv_name = "BreadStudio";

	NDIlib_recv_instance_t recv = ndiLib->recv_create_v3(&desc);
	if (!recv) {
		blog(LOG_WARNING, "[bread-ndi] cannot receive '%s'", s->name.c_str());
		return;
	}

	/* NDI sends the last tally again by itself whenever the receiver reconnects */
	int sent = -1;
	NDIlib_video_frame_v2_t video;
	NDIlib_audio_frame_v3_t audio;
	while (s->run) {
		const int want = s->tally;
		if (want != sent) {
			NDIlib_tally_t tally;
			tally.on_program = (want & 1) != 0;
			tally.on_preview = (want & 2) != 0;
			ndiLib->recv_set_tally(recv, &tally);
			sent = want;
		}

		switch (ndiLib->recv_capture_v3(recv, &video, &audio, nullptr, 100)) {
		case NDIlib_frame_type_video:
			outputVideo(s, video);
			ndiLib->recv_free_video_v2(recv, &video);
			break;
		case NDIlib_frame_type_audio:
			outputAudio(s, audio);
			ndiLib->recv_free_audio_v3(recv, &audio);
			break;
		default:
			break;
		}
	}

	ndiLib->recv_destroy(recv);
}

void stop(ndi_source *s)
{
	s->run = false;
	if (s->worker.joinable()) {
		s->worker.join();
	}
}

void ndi_update(void *data, obs_data_t *settings)
{
	auto *s = static_cast<ndi_source *>(data);

	/* Increase Buffer Size: OBS's own buffering. Changing it does not restart the receiver. */
	obs_source_set_async_unbuffered(s->source, obs_data_get_int(settings, "latency") == LatencyUnbuffered);

	const std::string name = obs_data_get_string(settings, "ndi_source_name");
	const int bandwidth = (int)obs_data_get_int(settings, "ndi_bw_mode");
	if (name == s->name && bandwidth == s->bandwidth) {
		return;
	}

	stop(s);
	s->name = name;
	s->bandwidth = bandwidth;
	/* no old picture under a new source, nor under Audio Only */
	obs_source_output_video(s->source, nullptr);
	if (name.empty() || !ndiLib) {
		return;
	}
	s->run = true;
	s->worker = std::thread(worker_main, s);
}

const char *ndi_get_name(void *)
{
	return obs_module_text("NDISource");
}

void *ndi_create(obs_data_t *settings, obs_source_t *source)
{
	auto *s = new ndi_source();
	s->source = source;
	ndi_update(s, settings);
	return s;
}

void ndi_destroy(void *data)
{
	auto *s = static_cast<ndi_source *>(data);
	stop(s);
	delete s;
}

void ndi_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "ndi_source_name", "");
	obs_data_set_default_int(settings, "ndi_bw_mode", BandwidthHighest);
	obs_data_set_default_int(settings, "latency", LatencyUnbuffered);
}

/* OBS's own window shows these; the desk's NDI tab reads the list and `status` (spec §6.1). data is null
 * when OBS asks for the type's settings without a source. */
obs_properties_t *ndi_properties(void *)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *name = obs_properties_add_list(props, "ndi_source_name", obs_module_text("Source"),
						       OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	for (const std::string &seen : ndiSources()) {
		obs_property_list_add_string(name, seen.c_str(), seen.c_str());
	}

	obs_property_t *bandwidth = obs_properties_add_list(props, "ndi_bw_mode", obs_module_text("Bandwidth"),
							    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(bandwidth, obs_module_text("Bandwidth.Highest"), BandwidthHighest);
	obs_property_list_add_int(bandwidth, obs_module_text("Bandwidth.Lowest"), BandwidthLowest);
	obs_property_list_add_int(bandwidth, obs_module_text("Bandwidth.AudioOnly"), BandwidthAudioOnly);

	obs_property_t *buffer = obs_properties_add_list(props, "latency", obs_module_text("Buffer"),
							 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(buffer, obs_module_text("Buffer.Normal"), LatencyUnbuffered);
	obs_property_list_add_int(buffer, obs_module_text("Buffer.Increased"), 0);

	if (!ndiStatus().empty()) {
		obs_properties_add_text(props, "status", ndiStatus().c_str(), OBS_TEXT_INFO);
	}
	return props;
}

} // namespace

obs_source_info ndiSourceInfo()
{
	obs_source_info info = {};
	info.id = "ndi_source"; /* DistroAV's, so a collection made with DistroAV opens here */
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name = ndi_get_name;
	info.create = ndi_create;
	info.destroy = ndi_destroy;
	info.get_defaults = ndi_defaults;
	info.get_properties = ndi_properties;
	info.update = ndi_update;
	info.icon_type = OBS_ICON_TYPE_CAMERA;
	return info;
}
```

Append to `breadstudio/ndi/data/locale/en-US.ini`:
```ini
Source="Source"
Bandwidth="Bandwidth"
Bandwidth.Highest="Highest"
Bandwidth.Lowest="Low bandwidth"
Bandwidth.AudioOnly="Audio only"
Buffer="Buffer"
Buffer.Normal="Normal"
Buffer.Increased="Increased"
```

- [ ] **Step 2: Write the test sender**

Create `breadstudio/ndi/test/NdiSend.cpp`:
```cpp
/* A test NDI sender: a moving bar and a 1 kHz tone at -20 dBFS, and once a second its connections and the
 * tally its receivers send back. Usage: bread-ndi-send [name]. Runs until Ctrl+C. */
#include "NdiLoad.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
	const std::string name = argc > 1 ? argv[1] : "Bread Test";

	HMODULE module = nullptr;
	std::wstring from;
	const NDIlib_v6 *ndi = loadNdi(module, from);
	if (!ndi || !ndi->initialize()) {
		std::puts("bread-ndi-send: NDI's library is not available");
		return 1;
	}

	NDIlib_send_create_t desc;
	desc.p_ndi_name = name.c_str();
	desc.clock_video = true; /* sending a frame waits for its time, so the loop runs at the frame rate */
	NDIlib_send_instance_t sender = ndi->send_create(&desc);
	if (!sender) {
		std::puts("bread-ndi-send: cannot create the sender");
		return 1;
	}

	const int width = 1280;
	const int height = 720;
	const int fps = 30;
	const int rate = 48000;
	const int samples = rate / fps;

	std::vector<uint8_t> picture((size_t)width * height * 4);
	std::vector<float> sound((size_t)samples * 2);

	NDIlib_video_frame_v2_t video;
	video.xres = width;
	video.yres = height;
	video.FourCC = NDIlib_FourCC_type_BGRX;
	video.frame_rate_N = fps;
	video.frame_rate_D = 1;
	video.frame_format_type = NDIlib_frame_format_type_progressive;
	video.p_data = picture.data();
	video.line_stride_in_bytes = width * 4;

	NDIlib_audio_frame_v3_t audio;
	audio.sample_rate = rate;
	audio.no_channels = 2;
	audio.no_samples = samples;
	audio.FourCC = NDIlib_FourCC_audio_type_FLTP;
	audio.p_data = reinterpret_cast<uint8_t *>(sound.data());
	audio.channel_stride_in_bytes = samples * (int)sizeof(float);

	std::printf("sending as '%s' on this PC\n", name.c_str());
	auto report = std::chrono::steady_clock::now();
	for (int64_t frame = 0;; frame++) {
		const int bar = (int)(frame * 8 % width);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				uint8_t *p = &picture[((size_t)y * width + x) * 4];
				const bool lit = x >= bar && x < bar + 64;
				p[0] = lit ? 255 : 40;
				p[1] = lit ? 255 : 40;
				p[2] = lit ? 255 : 160;
				p[3] = 255;
			}
		}
		for (int i = 0; i < samples; i++) {
			const double t = (double)(frame * samples + i) / rate;
			sound[(size_t)i] = sound[(size_t)(samples + i)] = 0.1f * (float)std::sin(2 * 3.141592653589793 * 1000 * t);
		}
		ndi->send_send_audio_v3(sender, &audio);
		ndi->send_send_video_v2(sender, &video);

		if (std::chrono::steady_clock::now() >= report) {
			report += std::chrono::seconds(1);
			NDIlib_tally_t tally;
			ndi->send_get_tally(sender, &tally, 0);
			std::printf("connections %d  tally %s%s\n", ndi->send_get_no_connections(sender, 0),
				    tally.on_program ? "PGM " : "", tally.on_preview ? "PVW" : "");
			std::fflush(stdout);
		}
	}
}
```

At the end of `breadstudio/ndi/CMakeLists.txt`, add:
```cmake

# A test sender that prints the tally it gets back. It finds NDI's DLL through NDI's runtime folder.
add_executable(bread-ndi-send test/NdiSend.cpp)
target_include_directories(bread-ndi-send PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/ndi-sdk/include")
set_target_properties(
  bread-ndi-send
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/ndi/test-bin" FOLDER plugins/bread-ndi
)
```

- [ ] **Step 3: Build**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-ndi bread-ndi-send
```
Expected: both build with no warning.

- [ ] **Step 4: Check by hand**

Start `build_x64\breadstudio\ndi\test-bin\RelWithDebInfo\bread-ndi-send.exe "Bar"`. Its NDI name is `<THIS PC'S NAME, in capitals> (Bar)`; NDI Studio Monitor (NDI Tools) lists it. Start the program, make a Blank input, put it on Preview, and in OBS's Sources dock (Docks menu → Sources) add an **NDI Source** to it, typing `<PC> (Bar)` as its source.

| Do | Expect |
|---|---|
| OK | The moving bar within two seconds; the strip moves with the tone; the sender prints `connections 1`. |
| Its properties: Bandwidth → Low bandwidth | The bar returns within two seconds, at lower resolution. |
| Bandwidth → Audio only | The picture goes black; the tone goes on. |
| Bandwidth → Highest; Buffer → Increased, then Normal | The picture stays up without a black gap: the buffer alone does not restart the receiver. |
| Stop the sender (Ctrl+C) for ten seconds, then start it again | The program stays responsive; the bar returns within five seconds. |
| Start NDI Test Patterns, choose a pattern with alpha, and add it to a second NDI Source over a bright Colour layer | The pattern's edges are clean, with no dark or light fringe. A fringe means NDI's BGRA is straight alpha: note it in the commit message and the smoke run, not fixed here (DistroAV ships the same behaviour). |
| Close the program | It closes within five seconds; the log ends with `Number of memory leaks: 0`. |

Delete the Blank input afterwards.

- [ ] **Step 5: Commit**

```powershell
git add breadstudio/ndi
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: the NDI source plays picture and sound, with Low Bandwidth, Audio Only and buffering"
```

---

### Task 3: Finding senders, "Also search", and tally

**Files:**
- Modify: `breadstudio/ndi/ndi.cpp`, `breadstudio/ndi/ndi-source.cpp`, `breadstudio/ndi/CMakeLists.txt`

**Interfaces:**
- Consumes: `bread::onAir(obs_source_t *)` from `breadstudio/shared/OnAir.hpp` (Plan B).
- Produces:
  - `ndiSources()` returns what one long-lived NDI finder has seen.
  - On OBS's global proc handler: `void bread_ndi_get_extra_ips(out string ips)` and `void bread_ndi_set_extra_ips(in string ips)`. `ips` is NDI's comma-separated list of IP addresses. The plugin keeps it in `plugin_config/bread-ndi/finder.json` and makes a new finder when it changes.
  - Each NDI source works out its tally ten times a second.

- [ ] **Step 1: The finder and "Also search"**

In `breadstudio/ndi/ndi.cpp`:

Add after `#include <util/platform.h>`:
```cpp
#include <mutex>
```

Replace the anonymous namespace and `ndiSources()` with:
```cpp
namespace {

HMODULE library = nullptr;
std::string status;

/* One finder for the life of the plugin, so the list is ready whenever it is asked for. */
std::mutex finderMutex;
NDIlib_find_instance_t finder = nullptr;
std::string extraIps; /* "Also search" (spec §4.1): NDI's comma-separated list */

std::string finderFile()
{
	char *path = obs_module_config_path("finder.json");
	std::string result = path ? path : "";
	bfree(path);
	return result;
}

void loadExtraIps()
{
	obs_data_t *saved = obs_data_create_from_json_file_safe(finderFile().c_str(), "bak");
	if (saved) {
		extraIps = obs_data_get_string(saved, "extra_ips");
		obs_data_release(saved);
	}
}

void saveExtraIps()
{
	char *folder = obs_module_config_path("");
	if (folder) {
		os_mkdirs(folder);
		bfree(folder);
	}
	obs_data_t *saved = obs_data_create();
	obs_data_set_string(saved, "extra_ips", extraIps.c_str());
	obs_data_save_json_safe(saved, finderFile().c_str(), "tmp", "bak");
	obs_data_release(saved);
}

void getExtraIps(void *, calldata_t *data)
{
	std::lock_guard lock(finderMutex);
	calldata_set_string(data, "ips", extraIps.c_str());
}

/* the next question makes a new finder that asks these addresses too */
void setExtraIps(void *, calldata_t *data)
{
	const char *ips = calldata_string(data, "ips");
	std::lock_guard lock(finderMutex);
	extraIps = ips ? ips : "";
	saveExtraIps();
	if (finder && ndiLib) {
		ndiLib->find_destroy(finder);
	}
	finder = nullptr;
}

} // namespace

const std::string &ndiStatus()
{
	return status;
}

std::vector<std::string> ndiSources()
{
	std::vector<std::string> names;
	std::lock_guard lock(finderMutex);
	if (!ndiLib) {
		return names;
	}
	if (!finder) {
		NDIlib_find_create_t desc;
		desc.show_local_sources = true;
		desc.p_extra_ips = extraIps.empty() ? nullptr : extraIps.c_str();
		finder = ndiLib->find_create_v2(&desc);
	}

	uint32_t count = 0;
	const NDIlib_source_t *found = finder ? ndiLib->find_get_current_sources(finder, &count) : nullptr;
	for (uint32_t i = 0; i < count; i++) {
		names.emplace_back(found[i].p_ndi_name);
	}
	return names;
}
```

In `obs_module_load`, before `return true;`, add:
```cpp
	loadExtraIps();
	/* the desk sets "Also search" through these, so it needs nothing of NDI (spec §6.1) */
	proc_handler_t *procs = obs_get_proc_handler();
	proc_handler_add(procs, "void bread_ndi_get_extra_ips(out string ips)", getExtraIps, nullptr);
	proc_handler_add(procs, "void bread_ndi_set_extra_ips(in string ips)", setExtraIps, nullptr);
	/* the finder needs a second or two: start it now, so the list is full when it is first shown */
	ndiSources();
```

In `obs_module_unload`, before `if (ndiLib) {`, add:
```cpp
	{
		std::lock_guard lock(finderMutex);
		if (finder && ndiLib) {
			ndiLib->find_destroy(finder);
		}
		finder = nullptr;
	}
```

- [ ] **Step 2: Tally**

In `breadstudio/ndi/ndi-source.cpp`, add after `#include "Ndi.hpp"`:
```cpp
#include "OnAir.hpp"
```

Add, inside the anonymous namespace, after `stop`:
```cpp
/* Runs on the UI thread, where the frontend may be asked. param is a weak reference taken by ndi_tick;
 * holding the source keeps its data alive. The worker sends the result when it changes. */
void update_tally(void *param)
{
	auto *weak = static_cast<obs_weak_source_t *>(param);
	obs_source_t *source = obs_weak_source_get_source(weak);
	obs_weak_source_release(weak);
	if (!source) {
		return;
	}

	const bread::OnAir on = bread::onAir(source);
	auto *s = static_cast<ndi_source *>(obs_obj_get_data(source));
	if (s) {
		s->tally = (on.program ? 1 : 0) | (on.preview ? 2 : 0);
	}
	obs_source_release(source);
}

/* OBS calls this every frame for every source, on screen or not: ten times a second is enough */
void ndi_tick(void *data, float seconds)
{
	auto *s = static_cast<ndi_source *>(data);
	s->tallyWait += seconds;
	if (s->tallyWait < 0.1f) {
		return;
	}
	s->tallyWait = 0.0f;
	obs_queue_task(OBS_TASK_UI, update_tally, obs_source_get_weak_source(s->source), false);
}
```

In `ndiSourceInfo()`, after `info.update = ndi_update;`, add:
```cpp
	info.video_tick = ndi_tick;
```

In `breadstudio/ndi/CMakeLists.txt`, give the plugin the shared folder and the frontend API:
```cmake
target_include_directories(bread-ndi PRIVATE "${CMAKE_SOURCE_DIR}/breadstudio/shared")
target_link_libraries(bread-ndi PRIVATE OBS::frontend-api)
```

- [ ] **Step 3: Build and check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-ndi
```
Expected: builds with no warning.

Start `bread-ndi-send.exe "Cam"`. In the program, Studio Mode on, make `Cam` (a Blank with an NDI Source layer picking `<PC> (Cam)` from the list, which now fills by itself), `Other` (Blank) and `Box` (Blank). Fade 3000 ms.

| Do | The sender prints |
|---|---|
| `Other` on Program and on Preview; `Cam` is only a tile in the bar | `tally ` with nothing after it |
| Click `Cam`'s tile | `PVW` |
| Transition | `PGM` |
| `Cam` as a layer of `Box`; `Box` on Program, `Other` on Preview | `PGM` |
| Untick that layer | nothing, within a second |
| `Other` on Program and Preview; AFV off on `Cam`'s strip | nothing |
| `Cam` on Program, `Other` on Preview; Transition (3 s) | `PGM` until the fade ends, then nothing |
| Studio Mode off; click `Cam`'s tile | `PGM` |
| Restart the sender while `Cam` is on Program | `PGM` again within five seconds |

The two "Also search" procedures are checked in Task 4, through the tab.

- [ ] **Step 4: Commit**

```powershell
git add breadstudio/ndi
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: find NDI senders, search given addresses, and send tally from what Program and Preview draw"
```

---

### Task 4: The NDI tab

**Files:**
- Create: `breadstudio/desk/SourceTiles.hpp`, `breadstudio/desk/SourceTiles.cpp`, `breadstudio/desk/NdiTab.hpp`, `breadstudio/desk/NdiTab.cpp`
- Modify: `breadstudio/desk/OmtTab.hpp`, `breadstudio/desk/OmtTab.cpp`, `breadstudio/desk/InputSelect.cpp`, `breadstudio/desk/CMakeLists.txt`, `breadstudio/desk/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `SenderGroup`, `ListedSource`, `groupBySender`, `sourceName` (`SourceList.hpp`, Plan B); `InputPage`; `createInput`; `CapturePage` in `InputSelect.cpp` (Plan B); the NDI plugin's `ndi_source_name` list, `status` description and two procedures (Tasks 2 and 3).
- Produces:
  - `std::vector<std::pair<std::string, std::string>> announced(const char *typeId, const char *listKey, const char *detailKey);`
  - `QString typeNote(const char *typeId, const char *key);` — the description of one property of a type's settings; empty when there is none
  - `class SourceTiles : public QScrollArea` with `SourceTiles(const QString &none, QWidget *parent)`, `void setGroups(const std::vector<SenderGroup> &)`, `const std::string &chosen() const`, `void choose(const std::string &)`, `void unpick()`, signal `pickedChanged()`
  - `class NdiTab : public InputPage` with `static constexpr const char *TypeId = "ndi_source";`, `explicit NdiTab(QWidget *parent)`, `NdiTab(obs_source_t *base, QWidget *parent)`
  - `OmtTab` with the same public interface as in Plan B

- [ ] **Step 1: Move the tiles out of the OMT tab**

Create `breadstudio/desk/SourceTiles.hpp`:
```cpp
#pragma once

#include "SourceList.hpp"

#include <QScrollArea>
#include <QString>

#include <map>
#include <string>
#include <utility>
#include <vector>

class QButtonGroup;
class QToolButton;

namespace desk {

/* (address, detail) pairs from a source type's settings lists (spec §6.1). Each detail item's value is
 * the address it describes. Without `detailKey` every detail is empty. */
std::vector<std::pair<std::string, std::string>> announced(const char *typeId, const char *listKey,
							    const char *detailKey);

/* the description of one property of a source type's settings; empty when there is none */
QString typeNote(const char *typeId, const char *key);

/* Announced sources as tiles under their sender PC's name, as vMix's NDI page shows them (spec §4.1). */
class SourceTiles : public QScrollArea {
	Q_OBJECT

public:
	/* `none` is shown while there is no source */
	SourceTiles(const QString &none, QWidget *parent);

	/* The tiles are made again only when the set of addresses changes; their lines always change. */
	void setGroups(const std::vector<SenderGroup> &groups);

	const std::string &chosen() const { return picked; }

	/* picks that address's tile, now or when it appears */
	void choose(const std::string &address);

	/* no tile picked */
	void unpick();

signals:
	void pickedChanged();

private:
	void rebuild(const std::vector<SenderGroup> &groups);

	QString none;
	std::string picked;
	std::vector<std::string> shown; /* the addresses the tiles were made for, in order */
	std::map<std::string, QToolButton *> tiles;
	QButtonGroup *buttons = nullptr;
};

} // namespace desk
```

Create `breadstudio/desk/SourceTiles.cpp`:
```cpp
#include "SourceTiles.hpp"

#include <obs.h>

#include <QButtonGroup>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

#include "Desk.hpp"

namespace desk {

static const int TilesPerRow = 4;
static const int TileWidth = 200;
static const int TileHeight = 64;

std::vector<std::pair<std::string, std::string>> announced(const char *typeId, const char *listKey,
							    const char *detailKey)
{
	std::vector<std::pair<std::string, std::string>> result;
	const char *id = obs_get_latest_input_type_id(typeId);
	obs_properties_t *props = id ? obs_get_source_properties(id) : nullptr;
	if (!props) {
		return result;
	}

	obs_property_t *addresses = obs_properties_get(props, listKey);
	obs_property_t *details = detailKey ? obs_properties_get(props, detailKey) : nullptr;
	const size_t count = addresses ? obs_property_list_item_count(addresses) : 0;
	const size_t detailCount = details ? obs_property_list_item_count(details) : 0;
	for (size_t i = 0; i < count; i++) {
		const std::string address = obs_property_list_item_string(addresses, i);
		std::string detail;
		for (size_t j = 0; j < detailCount; j++) {
			if (address == obs_property_list_item_string(details, j)) {
				detail = obs_property_list_item_name(details, j);
				break;
			}
		}
		result.emplace_back(address, detail);
	}

	obs_properties_destroy(props);
	return result;
}

QString typeNote(const char *typeId, const char *key)
{
	const char *id = obs_get_latest_input_type_id(typeId);
	obs_properties_t *props = id ? obs_get_source_properties(id) : nullptr;
	obs_property_t *property = props ? obs_properties_get(props, key) : nullptr;
	const QString note = property ? QString::fromUtf8(obs_property_description(property)) : QString();
	obs_properties_destroy(props);
	return note;
}

SourceTiles::SourceTiles(const QString &none_, QWidget *parent) : QScrollArea(parent), none(none_)
{
	setWidgetResizable(true);

	buttons = new QButtonGroup(this);
	connect(buttons, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button) {
		picked = button->property("address").toString().toStdString();
		emit pickedChanged();
	});

	rebuild({});
}

void SourceTiles::setGroups(const std::vector<SenderGroup> &groups)
{
	std::vector<std::string> now;
	for (const SenderGroup &group : groups) {
		for (const ListedSource &source : group.sources) {
			now.push_back(source.address);
		}
	}
	if (now != shown) {
		rebuild(groups);
		shown = now;
	}

	for (const SenderGroup &group : groups) {
		for (const ListedSource &source : group.sources) {
			const std::string line = source.detail.empty() ? source.name : source.name + "\n" + source.detail;
			tiles[source.address]->setText(QString::fromStdString(line));
		}
	}
}

void SourceTiles::choose(const std::string &address)
{
	picked = address;
	auto tile = tiles.find(address);
	if (tile != tiles.end()) {
		tile->second->setChecked(true);
	}
}

void SourceTiles::unpick()
{
	if (QAbstractButton *checked = buttons->checkedButton()) {
		buttons->setExclusive(false);
		checked->setChecked(false);
		buttons->setExclusive(true);
	}
	picked.clear();
}

/* Setting the new content deletes the old, and a deleted button leaves its group by itself. */
void SourceTiles::rebuild(const std::vector<SenderGroup> &groups)
{
	auto *content = new QWidget();
	auto *column = new QVBoxLayout(content);
	column->setAlignment(Qt::AlignTop);
	tiles.clear();

	for (const SenderGroup &group : groups) {
		column->addWidget(
			new QLabel(group.pc.empty() ? text("Sources.OtherPc") : QString::fromStdString(group.pc), content));
		auto *grid = new QGridLayout();
		grid->setAlignment(Qt::AlignLeft);
		column->addLayout(grid);

		int i = 0;
		for (const ListedSource &source : group.sources) {
			auto *tile = new QToolButton(content);
			tile->setCheckable(true);
			tile->setFixedSize(TileWidth, TileHeight);
			tile->setProperty("address", QString::fromStdString(source.address));
			tile->setToolTip(QString::fromStdString(source.address));
			tile->setChecked(source.address == picked);
			buttons->addButton(tile);
			grid->addWidget(tile, i / TilesPerRow, i % TilesPerRow);
			tiles[source.address] = tile;
			i++;
		}
	}

	if (groups.empty()) {
		auto *empty = new QLabel(none, content);
		empty->setWordWrap(true);
		column->addWidget(empty);
	}

	setWidget(content);
}

} // namespace desk
```

Replace `breadstudio/desk/OmtTab.hpp` with:
```cpp
#pragma once

#include "InputPage.hpp"

#include <QTimer>

#include <string>

class QLabel;
class QLineEdit;

namespace desk {

class SourceTiles;

/* The OMT tab (spec §4.1 to §4.3). The sources and their formats come from the OMT plugin's
 * settings lists (§6.1), so the desk does not link omthx. */
class OmtTab : public InputPage {
	Q_OBJECT

public:
	static constexpr const char *TypeId = "omt_hx_source";

	/* for a new OMT input */
	explicit OmtTab(QWidget *parent);

	/* for the base of an OMT input that exists */
	OmtTab(obs_source_t *base, QWidget *parent);

	bool ready() const override;
	OBSSource create() override;
	void apply() override;

private:
	void build();
	void refresh();
	std::string selected() const;

	OBSSource base; /* null for a new input */
	SourceTiles *tiles = nullptr;
	QLineEdit *byAddress = nullptr;
	QLabel *receiving = nullptr; /* for an input that exists */
	QTimer timer;
};

} // namespace desk
```

Replace `breadstudio/desk/OmtTab.cpp` with:
```cpp
#include "OmtTab.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "SourceList.hpp"
#include "SourceTiles.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace desk {

OmtTab::OmtTab(QWidget *parent) : InputPage(parent)
{
	build();
	refresh();
}

OmtTab::OmtTab(obs_source_t *base_, QWidget *parent) : InputPage(parent), base(base_)
{
	build();

	OBSDataAutoRelease settings = obs_source_get_settings(base);
	tiles->choose(obs_data_get_string(settings, "address"));

	/* what the input receives now, including the decoder (spec §4.3) */
	receiving = new QLabel(this);
	static_cast<QVBoxLayout *>(layout())->insertWidget(0, receiving);

	refresh();
}

void OmtTab::build()
{
	auto *layout = new QVBoxLayout(this);

	tiles = new SourceTiles(text("Omt.None"), this);
	layout->addWidget(tiles, 1);
	connect(tiles, &SourceTiles::pickedChanged, this, [this] {
		byAddress->clear();
		emit readyChanged();
	});

	/* for a network where announcements do not arrive (spec §4.1) */
	auto *row = new QHBoxLayout();
	row->addWidget(new QLabel(text("Omt.ByAddress"), this));
	byAddress = new QLineEdit(this);
	byAddress->setPlaceholderText(text("Omt.ByAddress.Hint"));
	row->addWidget(byAddress, 1);
	layout->addLayout(row);

	/* typing an address unpicks the tile */
	connect(byAddress, &QLineEdit::textEdited, this, [this](const QString &typed) {
		if (!typed.isEmpty()) {
			tiles->unpick();
		}
		emit readyChanged();
	});

	/* once a second while the window is open (spec §4.1) */
	connect(&timer, &QTimer::timeout, this, &OmtTab::refresh);
	timer.start(1000);
}

void OmtTab::refresh()
{
	tiles->setGroups(groupBySender(announced(TypeId, "address", "detail")));

	if (receiving) {
		obs_properties_t *props = obs_source_properties(base);
		obs_property_t *line = obs_properties_get(props, "receiving");
		receiving->setText(line ? text("Omt.Receiving").arg(QString::fromUtf8(obs_property_description(line)))
					: QString());
		obs_properties_destroy(props);
	}
}

std::string OmtTab::selected() const
{
	const std::string typed = byAddress->text().trimmed().toStdString();
	return typed.empty() ? tiles->chosen() : typed;
}

bool OmtTab::ready() const
{
	return !selected().empty();
}

OBSSource OmtTab::create()
{
	const std::string address = selected();
	if (base || address.empty()) {
		return OBSSource(); /* nothing to make: the page is for a source that exists, or nothing is picked */
	}

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "address", address.c_str());
	/* as vMix does, the input is named after its source */
	return createInput(TypeId, sourceName(address).c_str(), settings);
}

/* a new address only: any change restarts the receiver */
void OmtTab::apply()
{
	const std::string address = selected();
	if (!base || address.empty()) {
		return;
	}

	OBSDataAutoRelease now = obs_source_get_settings(base);
	if (address == obs_data_get_string(now, "address")) {
		return;
	}

	OBSDataAutoRelease change = obs_data_create();
	obs_data_set_string(change, "address", address.c_str());
	obs_source_update(base, change);
}

} // namespace desk
```
If `OmtTab.cpp` on `main` has lines Plan B's text did not (a fix made while it was built), carry them over.

In `breadstudio/desk/data/locale/en-US.ini`, rename `Omt.OtherPc` to `Sources.OtherPc` (same text, "Other").

In `breadstudio/desk/CMakeLists.txt`, add to the plugin's sources, in alphabetical places:
```cmake
    SourceTiles.cpp
    SourceTiles.hpp
```

Build and check that the OMT tab behaves as Task 5 of Plan B says (tiles, formats, Add by address, ⚙ with the tile picked and the receiving line):
```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk
```
Expected: builds with no warning; Plan B's Task 5 Step 7 table holds with the three OMT test senders.

- [ ] **Step 2: Write the NDI tab**

Create `breadstudio/desk/NdiTab.hpp`:
```cpp
#pragma once

#include "InputPage.hpp"

#include <QTimer>

class QCheckBox;
class QLabel;
class QLineEdit;

namespace desk {

class SourceTiles;

/* The NDI tab (spec §4.1): the senders found on the network, vMix's three checkboxes, Also search, and
 * why NDI is off when it is. Everything comes from the NDI plugin's settings and procedures (§6.1). */
class NdiTab : public InputPage {
	Q_OBJECT

public:
	static constexpr const char *TypeId = "ndi_source";

	/* for a new NDI input */
	explicit NdiTab(QWidget *parent);

	/* for the base of an NDI input that exists */
	NdiTab(obs_source_t *base, QWidget *parent);

	bool ready() const override;
	OBSSource create() override;
	void apply() override;

private:
	void build();
	void refresh();
	OBSDataAutoRelease settings() const;

	OBSSource base; /* null for a new input */
	SourceTiles *tiles = nullptr;
	QLabel *status = nullptr;
	QCheckBox *lowBandwidth = nullptr;
	QCheckBox *audioOnly = nullptr;
	QCheckBox *bigBuffer = nullptr;
	QLineEdit *alsoSearch = nullptr;
	QTimer timer;
};

} // namespace desk
```

Create `breadstudio/desk/NdiTab.cpp`:
```cpp
#include "NdiTab.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "SourceList.hpp"
#include "SourceTiles.hpp"

#include <callback/calldata.h>
#include <callback/proc.h>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <string>

namespace desk {

/* the NDI plugin's values (DistroAV's) */
static const int BandwidthHighest = 0;
static const int BandwidthLowest = 1;
static const int BandwidthAudioOnly = 2;
static const int LatencyBuffered = 0;
static const int LatencyUnbuffered = 2;

/* "Also search" lives in the NDI plugin, which its finder uses (spec §4.1) */
static QString extraIps()
{
	calldata_t data;
	calldata_init(&data);
	QString ips;
	if (proc_handler_call(obs_get_proc_handler(), "bread_ndi_get_extra_ips", &data)) {
		const char *value = calldata_string(&data, "ips");
		ips = QString::fromUtf8(value ? value : "");
	}
	calldata_free(&data);
	return ips;
}

static void setExtraIps(const QString &ips)
{
	calldata_t data;
	calldata_init(&data);
	calldata_set_string(&data, "ips", ips.trimmed().toUtf8().constData());
	proc_handler_call(obs_get_proc_handler(), "bread_ndi_set_extra_ips", &data);
	calldata_free(&data);
}

NdiTab::NdiTab(QWidget *parent) : InputPage(parent)
{
	build();
	refresh();
}

NdiTab::NdiTab(obs_source_t *base_, QWidget *parent) : InputPage(parent), base(base_)
{
	build();

	/* obs_data_get_int gives the plugin's default for a value never saved, as a DistroAV collection may have */
	OBSDataAutoRelease now = obs_source_get_settings(base);
	tiles->choose(obs_data_get_string(now, "ndi_source_name"));
	const long long bandwidth = obs_data_get_int(now, "ndi_bw_mode");
	lowBandwidth->setChecked(bandwidth == BandwidthLowest);
	audioOnly->setChecked(bandwidth == BandwidthAudioOnly);
	bigBuffer->setChecked(obs_data_get_int(now, "latency") != LatencyUnbuffered);

	refresh();
}

void NdiTab::build()
{
	auto *layout = new QVBoxLayout(this);

	/* why NDI is off (spec §10); hidden while it is on */
	status = new QLabel(this);
	status->setWordWrap(true);
	status->setVisible(false);
	layout->addWidget(status);

	tiles = new SourceTiles(text("Ndi.None"), this);
	layout->addWidget(tiles, 1);
	connect(tiles, &SourceTiles::pickedChanged, this, &InputPage::readyChanged);

	auto *boxes = new QHBoxLayout();
	lowBandwidth = new QCheckBox(text("Ndi.LowBandwidth"), this);
	audioOnly = new QCheckBox(text("Ndi.AudioOnly"), this);
	bigBuffer = new QCheckBox(text("Ndi.BigBuffer"), this);
	boxes->addWidget(lowBandwidth);
	boxes->addWidget(audioOnly);
	boxes->addWidget(bigBuffer);
	boxes->addStretch(1);
	layout->addLayout(boxes);

	/* for senders on another subnet: NDI's finder asks these addresses directly (spec §4.1) */
	auto *row = new QHBoxLayout();
	row->addWidget(new QLabel(text("Ndi.AlsoSearch"), this));
	alsoSearch = new QLineEdit(extraIps(), this);
	alsoSearch->setPlaceholderText(text("Ndi.AlsoSearch.Hint"));
	row->addWidget(alsoSearch, 1);
	layout->addLayout(row);
	connect(alsoSearch, &QLineEdit::editingFinished, this, [this] { setExtraIps(alsoSearch->text()); });

	/* NDI's licence asks for its trademark line and a link near where NDI is chosen (spec §7.2) */
	auto *credit = new QLabel(text("Ndi.Credit"), this);
	credit->setWordWrap(true);
	credit->setOpenExternalLinks(true);
	layout->addWidget(credit);

	/* once a second while the window is open (spec §4.1) */
	connect(&timer, &QTimer::timeout, this, &NdiTab::refresh);
	timer.start(1000);
}

void NdiTab::refresh()
{
	tiles->setGroups(groupBySender(announced(TypeId, "ndi_source_name", nullptr)));

	const QString why = typeNote(TypeId, "status");
	status->setText(why);
	status->setVisible(!why.isEmpty());
}

OBSDataAutoRelease NdiTab::settings() const
{
	OBSDataAutoRelease result = obs_data_create();
	obs_data_set_string(result, "ndi_source_name", tiles->chosen().c_str());
	/* Audio Only wins over Low Bandwidth: NDI has one bandwidth setting */
	obs_data_set_int(result, "ndi_bw_mode",
			 audioOnly->isChecked()      ? BandwidthAudioOnly
			 : lowBandwidth->isChecked() ? BandwidthLowest
						     : BandwidthHighest);
	obs_data_set_int(result, "latency", bigBuffer->isChecked() ? LatencyBuffered : LatencyUnbuffered);
	return result;
}

bool NdiTab::ready() const
{
	return !tiles->chosen().empty();
}

OBSSource NdiTab::create()
{
	if (base || tiles->chosen().empty()) {
		return OBSSource(); /* nothing to make: the page is for a source that exists, or nothing is picked */
	}
	/* as vMix does, the input is named after its source */
	return createInput(TypeId, sourceName(tiles->chosen()).c_str(), settings());
}

/* only when something changed: a new source or bandwidth restarts the receiver */
void NdiTab::apply()
{
	if (!base || tiles->chosen().empty()) {
		return;
	}

	OBSDataAutoRelease now = obs_source_get_settings(base);
	OBSDataAutoRelease wanted = settings();
	if (std::string(obs_data_get_string(now, "ndi_source_name")) ==
		    obs_data_get_string(wanted, "ndi_source_name") &&
	    obs_data_get_int(now, "ndi_bw_mode") == obs_data_get_int(wanted, "ndi_bw_mode") &&
	    obs_data_get_int(now, "latency") == obs_data_get_int(wanted, "latency")) {
		return;
	}
	obs_source_update(base, wanted);
}

} // namespace desk
```

- [ ] **Step 3: Put the tab in the window**

In `breadstudio/desk/InputSelect.cpp`:

Add to the includes:
```cpp
#include "NdiTab.hpp"
```

In `CapturePage`'s constructor, put NDI in front of OMT:
```cpp
		if (obs_get_latest_input_type_id(NdiTab::TypeId)) {
			addTab(new NdiTab(tabs), "Tab.Ndi");
		}
		if (obs_get_latest_input_type_id(OmtTab::TypeId)) {
			addTab(new OmtTab(tabs), "Tab.Omt");
		}
		addTab(new DesktopPage(tabs), "Tab.Desktop");
```
and change its comment's last sentence ("Plan C puts NDI in front.") to "NDI, then OMT, then Desktop Capture, as vMix's tabs."

In the constructor for an input that exists, add an NDI branch beside the OMT one:
```cpp
	} else if (base && std::strcmp(obs_source_get_unversioned_id(base), NdiTab::TypeId) == 0) {
		only = new NdiTab(base.Get(), pages);
```

In `breadstudio/desk/CMakeLists.txt`, add to the plugin's sources, in alphabetical places:
```cmake
    NdiTab.cpp
    NdiTab.hpp
```

In `breadstudio/desk/data/locale/en-US.ini`, change `Kind.Desktop` to:
```ini
Kind.Desktop="NDI / OMT / Desktop Capture"
```
and append:
```ini
Tab.Ndi="NDI"
Ndi.None="No NDI source found yet. A sender appears here a few seconds after it starts; for one on another network, give its PC's address in Also search."
Ndi.LowBandwidth="Low Bandwidth Mode"
Ndi.AudioOnly="Audio Only"
Ndi.BigBuffer="Increase Buffer Size"
Ndi.AlsoSearch="Also search"
Ndi.AlsoSearch.Hint="IP addresses of sender PCs, separated by commas"
Ndi.Credit="NDI® is a registered trademark of Vizrt NDI AB. <a href='https://ndi.video/'>ndi.video</a>. NDI receiving is based on DistroAV (GPL)."
```

- [ ] **Step 4: Build and check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk
```
Expected: builds with no warning. Start `bread-ndi-send.exe "Bar"` and NDI Test Patterns, then the program.

| Do | Expect |
|---|---|
| Add Input; the row "NDI / OMT / Desktop Capture" | Tabs NDI, OMT, Desktop Capture; NDI open. Under this PC's name, tiles `Bar` and the Test Patterns source. The credit line under the list; its link opens ndi.video. OK greyed out. |
| Click `Bar`; OK | An input named `Bar`, last, on Preview, showing the moving bar, with the tone on its strip. |
| ⚙ on `Bar`, Base properties… | The NDI tab alone, `Bar` picked, no box ticked. |
| Tick Increase Buffer Size; OK | The picture does not blink. ⚙ again: the box is ticked. |
| Tick Audio Only; OK | The tile goes black; the tone goes on. ⚙ again: Audio Only ticked, Low Bandwidth not. |
| Untick Audio Only, tick Low Bandwidth Mode; OK | The bar returns, softer. |
| Also search: type `127.0.0.1`, press Tab; close the program; start it again; Add Input, NDI | The field reads `127.0.0.1`; the tiles are still listed. |
| Make a collection as DistroAV saves one: Scene Collection → Export; in the file, delete the `Bar` base's `ndi_bw_mode` and `latency` keys (DistroAV saves only what was changed) and change the collection's `name`; Scene Collection → Import it and switch to it | The input plays. ⚙, Base properties… opens the NDI tab with `Bar` picked and no box ticked. |
| Close; rename `bin\64bit\Processing.NDI.Lib.x64.dll` away; `$env:NDI_RUNTIME_DIR_V6 = ''`; start the program from that shell; Add Input, NDI | The tab shows "NDI is off: its library … is not in BreadStudio's folder …" and no tiles. The `Bar` input is a black tile. OMT, Desktop Capture and Stream inputs still work. Close, start again normally: `Bar` plays. |
| Close the program with the window open | It closes within five seconds; the log ends with `Number of memory leaks: 0`. |

Rename the DLL back.

- [ ] **Step 5: Commit**

```powershell
git add breadstudio/desk
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the NDI tab to Input Select, sharing the tile grid with the OMT tab"
```

---

### Task 5: The smoke checklist, and the run

**Files:**
- Modify: `breadstudio/docs/DESK-SMOKE.md`

- [ ] **Step 1: Add section G**

In `breadstudio/docs/DESK-SMOKE.md`, after section F and before `## Before a run`, add:
```markdown
## G. NDI inputs and tally

Spec: `2026-10-14-breadstudio-network-inputs-design.md`. Needs
`bread-ndi-send.exe` (built beside the program, in
`build_x64\breadstudio\ndi\test-bin`), NDI Tools on this PC, and vMix or NDI
Tools on a second PC.

1. On an installed build, on a PC that never had NDI installed: the NDI tab
   lists a sender on another PC within five seconds, and an input made from
   it plays with sound. Nothing was installed but BreadStudio.
2. Low Bandwidth Mode gives a picture; Audio Only gives sound and a black
   tile; Increase Buffer Size changes nothing visible and does not blink the
   picture.
3. A sender stopped for ten seconds and restarted comes back by itself.
4. Tally, read in `bread-ndi-send`'s output or on vMix's input: only a tile in
   the bar reads nothing; on Preview `PVW`; on Program, alone or as a layer,
   `PGM`; a layer unticked drops `PGM`; AFV unlit with something else on
   Program reads nothing; a three-second fade holds `PGM` to its end; with
   Studio Mode off a click reads `PGM`.
5. A sender with alpha (vMix with an alpha output, or NDI Test Patterns) over
   a bright layer: clean edges.
6. Also search with a sender PC's address on a network where announcements do
   not arrive: its sources appear.
7. With the DLL renamed away and no NDI runtime: the program starts, the NDI
   tab says why NDI is off, NDI inputs are black tiles, every other kind
   works.
8. After closing with an NDI input in the collection, the log has
   `Number of memory leaks: 0`.
```

- [ ] **Step 2: Run the automated tests**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-omt-test bread-onair-test bread-sources-test bread-stream-test bread-desk-test
$env:PATH = "$PWD\build_x64\rundir\RelWithDebInfo\bin\64bit;$env:PATH"
foreach ($t in 'omt-hx\test-bin\RelWithDebInfo\bread-omt-test', 'omt-hx\test-bin\RelWithDebInfo\bread-onair-test', 'desk\test-bin\RelWithDebInfo\bread-sources-test', 'desk\test-bin\RelWithDebInfo\bread-stream-test', 'desk\test-bin\RelWithDebInfo\bread-desk-test') { & "build_x64\breadstudio\$t.exe" | Select-Object -Last 1; "exit $LASTEXITCODE" }
```
Expected: every one ends `exit 0`. (This plan adds no automated test: its logic is NDI's and OBS's; its own rules are the on-air rule and the grouping, which Plan B's tests pin.)

- [ ] **Step 3: Run sections E, F and G, and record**

Build everything (`breadstudio\build.ps1`), run `DESK-SMOKE.md` sections E, F and G on this PC, and add an entry under `## Runs` in the existing form. G1 (an installed build on a PC without NDI) and G6 (a second subnet) are owed to the owner's run; G2 to G5 and G7 are run here with `bread-ndi-send` and NDI Test Patterns standing in for a second PC.

- [ ] **Step 4: Commit**

```powershell
git add breadstudio/docs
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add NDI inputs to the smoke checklist, and record the run"
```

---

## Left for later

| Item | Where |
|---|---|
| The installer script refuses a build without NDI's DLL, and carries `Processing.NDI.Lib.Licenses.txt` | S0's installer task |
| CI installs the NDI SDK (and fetches OMT-HX) before configuring | S0 Task 8 |
| NDI output (DistroAV's `ndi-output.cpp`) | Part 2, the outputs engine |
| PTZ for NDI cameras, NDI|HX as a tested sender, the format check on the NDI tab | Out of scope (§13) |
| G1 and G6 of the smoke checklist | The owner's run |
