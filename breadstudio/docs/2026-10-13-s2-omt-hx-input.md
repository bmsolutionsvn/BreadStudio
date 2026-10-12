# BreadStudio S2a — OMT-HX input source on omthx Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An OBS input source, `OMT-HX Source`, that plays any Open Media Transport source — a BreadCamera over plain OMT or OMT-HX, with audio and alpha — by driving the merged `omthx` library, and sends tally back to the sender from OBS's program and preview state.

**Architecture:** A new OBS plugin `plugins/omt-hx` with one async source. It owns an `omthx::Receiver` and a worker thread that pulls video and audio and hands them to OBS with `obs_source_output_video`/`obs_source_output_audio`; OBS renders them. The plugin compiles omthx's source files (`src/pure.cpp`, `src/decoder.cpp`, `src/link.cpp`) directly against **OBS's own FFmpeg** (the `FFmpeg::` targets), so there is one FFmpeg in the process, not two; only the patched `libomt.dll`/`libvmx.dll` are added, next to `obs64.exe`. OBS's `activate`/`deactivate`/`show`/`hide` callbacks map to the tally the receiver sends back.

**Tech Stack:** OBS Studio 32.2.2 fork (BreadStudio), C++20 for the plugin translation units (omthx uses `std::span`), OBS's FFmpeg, the `omthx` sources from the OMT-HX checkout, the patched `libomt`, CMake + Visual Studio 2026, Windows PowerShell 5.1 scripts. Windows x64 only.

**Spec:** `docs/superpowers/specs/2026-10-12-broadcast-roadmap-design.md` (R12, S2), the omthx design `../OMT-HX/docs/superpowers/specs/2026-10-12-omt-hx-design.md`, and this repo's `breadstudio/UPSTREAM-EDITS.md` rules.

## Global Constraints

- All BreadStudio S0 Global Constraints apply: base OBS tag `32.2.2`; our code is separate from OBS; every OBS file edited or added outside `breadstudio/` is listed in `breadstudio/UPSTREAM-EDITS.md` in the same commit; `breadstudio/check-edits.ps1` passes before every commit; commit messages `breadstudio: <what changed>`; PowerShell 5.1 scripts; build from a VS 2026 developer shell; never rebase or force-push `main`.
- The plugin is a MODULE that OBS loads; it must build with the OBS tree and land in `obs-plugins/64bit`. `set_target_properties_obs` does the deployment and copies the plugin's `data/` — do not hand-roll deployment for the plugin DLL.
- The plugin translation units that include `omthx/omthx.h` and the omthx sources are compiled as **C++20** (`std::span`). OBS's default is C++17; the plugin target sets `CXX_STANDARD 20`.
- `omthx` is consumed as source files from a checkout at `../OMT-HX`, after that checkout's `scripts/build-libomt.ps1` has populated `third_party/libomt/`. Do not add_subdirectory the OMT-HX CMake project — it calls `find_package(FFMPEG)` (vcpkg), which would pull a second FFmpeg into OBS's process.
- The plugin needs only the omthx **decoder** path: `pure.cpp`, `decoder.cpp`, `link.cpp`. Do not compile `encoder.cpp` (Studio never sends).
- The plugin links OBS's FFmpeg through the `FFmpeg::avcodec`, `FFmpeg::avutil`, `FFmpeg::swscale` targets from OBS's own `cmake/finders/FindFFmpeg.cmake`. If those exact target names are absent in this tree, read that module and use the names it creates; do not add a second FFmpeg.
- This PC has an AMD Radeon RX 6600 and no NVIDIA GPU; the sender side is BreadCamera. Studio only decodes, so the encoder question does not arise here.
- OBS has no plugin unit-test harness. Verification for this plugin is **build + a scripted load check + a manual smoke in OBS**; each task says exactly what to run and see. This is the OBS norm, not a placeholder.

## Review Focus

1. **A source that is not running yet.** Selecting an address that never appears must leave OBS stable (no crash, no busy loop), and the source must recover when the sender appears. Owned by Task 2 and Task 3.
2. **Alpha.** BreadCamera can send alpha (packed). The receiver returns premultiplied BGRA when the source has alpha; OBS treats a frame with `flags == 0` as premultiplied, so it passes through unchanged. Owned by Task 2, verified by eye in Task 6.
3. **Audio and video stay together.** Both come from one OMT stream; timestamps are offset from one OBS base clock, not minted per call. Owned by Task 3.
4. **Tally reaches BreadCamera.** A source dropped on program must make the sender's `tally().program` true; on preview, `tally().preview`. Owned by Task 5.
5. **The plugin does not fight OBS's own FFmpeg.** Exactly one FFmpeg in the process; the plugin links `FFmpeg::` targets and adds no `av*.dll` of its own. Owned by Task 1.

## File Structure

| Path | Responsibility |
|---|---|
| `plugins/omt-hx/CMakeLists.txt` | Build the plugin: omthx sources + OBS FFmpeg + libomt, C++20, deploy libomt/libvmx |
| `plugins/omt-hx/omt-hx.cpp` | The `omt_hx_source`: callbacks, receiver thread, video/audio output, tally |
| `plugins/omt-hx/data/locale/en-US.ini` | The source's UI name |
| `plugins/CMakeLists.txt` | `add_obs_plugin(omt-hx PLATFORMS WINDOWS)` (OBS file — listed in UPSTREAM-EDITS) |
| `breadstudio/UPSTREAM-EDITS.md` | Add the two OBS entries above |
| `breadstudio/docs/SMOKE.md` | Add the OMT-HX smoke steps |

---

### Task 1: The plugin builds and OBS lists the source

**Files:**
- Create: `plugins/omt-hx/CMakeLists.txt`, `plugins/omt-hx/omt-hx.cpp`, `plugins/omt-hx/data/locale/en-US.ini`
- Modify: `plugins/CMakeLists.txt` (OBS file), `breadstudio/UPSTREAM-EDITS.md`

**Interfaces:**
- Consumes: a built OMT-HX checkout at `../OMT-HX` (`third_party/libomt/libomt.lib`, `src/decoder.cpp`); OBS's `FFmpeg::` targets and `OBS::libobs`.
- Produces: the OBS source id `omt_hx_source`, named `OMT-HX Source`; a build that puts `omt-hx.dll` in `obs-plugins/64bit` and `libomt.dll`/`libvmx.dll` beside `obs64.exe`.

- [ ] **Step 1: Prerequisite — build the patched libomt once**

Run:
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\OMT-HX\scripts\build-libomt.ps1
```
Expected: `..\OMT-HX\third_party\libomt\libomt.lib` exists. If the OMT-HX checkout is missing, stop: this plan needs it (see the OMT-HX README).

- [ ] **Step 2: Write the plugin build file**

Create `plugins/omt-hx/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.28...3.30)

# omthx sources come from the OMT-HX checkout. We compile them against OBS's own FFmpeg
# (the FFmpeg:: targets below) so the process has one FFmpeg, not two.
set(OMTHX_ROOT "${CMAKE_SOURCE_DIR}/../OMT-HX" CACHE PATH "OMT-HX checkout")
if(NOT EXISTS "${OMTHX_ROOT}/third_party/libomt/libomt.lib" OR NOT EXISTS "${OMTHX_ROOT}/src/decoder.cpp")
  message(FATAL_ERROR "omt-hx plugin needs a built OMT-HX checkout at '${OMTHX_ROOT}'. Run its scripts/build-libomt.ps1, or set -DOMTHX_ROOT=<path>.")
endif()

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil swscale)

add_library(omt-hx MODULE)
add_library(OBS::omt-hx ALIAS omt-hx)

set_target_properties(omt-hx PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

target_sources(
  omt-hx
  PRIVATE
    omt-hx.cpp
    "${OMTHX_ROOT}/src/pure.cpp"
    "${OMTHX_ROOT}/src/decoder.cpp"
    "${OMTHX_ROOT}/src/link.cpp"
)

target_include_directories(omt-hx PRIVATE "${OMTHX_ROOT}/include" "${OMTHX_ROOT}/src" "${OMTHX_ROOT}/third_party/libomt")

target_link_libraries(
  omt-hx
  PRIVATE
    OBS::libobs
    $<$<PLATFORM_ID:Windows>:OBS::w32-pthreads>
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swscale
    "${OMTHX_ROOT}/third_party/libomt/libomt.lib"
)

# The patched libomt and its runtime libvmx.dll must sit beside obs64.exe: the run directory's
# bin/64bit, where OBS bundles its own dependencies. OBS_EXECUTABLE_DESTINATION is bin/64bit.
add_custom_command(
  TARGET omt-hx
  POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_EXECUTABLE_DESTINATION}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${OMTHX_ROOT}/third_party/libomt/libomt.dll"
          "${OMTHX_ROOT}/third_party/libomt/libvmx.dll"
          "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_EXECUTABLE_DESTINATION}"
  VERBATIM
)

set_target_properties_obs(omt-hx PROPERTIES FOLDER plugins PREFIX "")
```

- [ ] **Step 3: Write the plugin source (registers the source, no media yet)**

Create `plugins/omt-hx/omt-hx.cpp`:
```cpp
// OMT-HX input source: plays an Open Media Transport source (plain OMT or OMT-HX) via omthx.
#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("omt-hx", "en-US")

namespace {
struct omt_source {
	obs_source_t *source = nullptr;
};

const char *omt_get_name(void *) { return obs_module_text("OMTHXSource"); }

void *omt_create(obs_data_t *, obs_source_t *source) { return new omt_source{source}; }

void omt_destroy(void *data) { delete static_cast<omt_source *>(data); }

void omt_defaults(obs_data_t *settings) { obs_data_set_default_string(settings, "address", ""); }

obs_properties_t *omt_properties(void *) {
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_text(p, "address", obs_module_text("Source"), OBS_TEXT_DEFAULT);
	return p;
}
}  // namespace

static const struct obs_source_info omt_hx_source = {
        .id = "omt_hx_source",
        .type = OBS_SOURCE_TYPE_INPUT,
        .output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO,
        .get_name = omt_get_name,
        .create = omt_create,
        .destroy = omt_destroy,
        .get_defaults = omt_defaults,
        .get_properties = omt_properties,
        .icon_type = OBS_ICON_TYPE_CAMERA,
};

bool obs_module_load(void)
{
	obs_register_source(&omt_hx_source);
	return true;
}
```
Note the `struct omt_source` above is a placeholder shape; Task 2 replaces both callbacks with a real `omt_source`. This step's only job is a source that registers and shows a text field.

Create `plugins/omt-hx/data/locale/en-US.ini`:
```ini
OMTHXSource="OMT-HX Source"
Source="Source (host and name)"
```

- [ ] **Step 4: Register the plugin and the edits list**

In `plugins/CMakeLists.txt`, insert (alphabetical position, between `obs-x264` and `oss-audio`):
```cmake
add_obs_plugin(omt-hx PLATFORMS WINDOWS)
```
In `breadstudio/UPSTREAM-EDITS.md`, under **Edited** add:
```
- `plugins/CMakeLists.txt` - register the omt-hx plugin
```
and under **Added outside breadstudio/** add one line per file (the check matches exact paths, not a directory):
```
- `plugins/omt-hx/CMakeLists.txt` - the OMT-HX input plugin build file
- `plugins/omt-hx/omt-hx.cpp` - the OMT-HX input source
- `plugins/omt-hx/data/locale/en-US.ini` - the source's UI strings
```
Stage the files first (the check reads the staged tree), then run it:
```powershell
git add plugins/CMakeLists.txt plugins/omt-hx breadstudio/UPSTREAM-EDITS.md
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
```
Expected: passes.

- [ ] **Step 5: Configure and build**

Run (from a VS 2026 developer shell, from the repo root):
```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --target omt-hx
```
(The exact preset names are in `CMakePresets.json`; `breadstudio/build.ps1` wraps this with the pinned OBS version.)
Expected: `omt-hx.dll` builds. If `std::span` errors appear, the `CXX_STANDARD 20` line is not taking effect — fix that, do not rewrite omthx.

- [ ] **Step 6: Check the artifacts land where OBS looks**

Run:
```powershell
Get-ChildItem build_x64\rundir\RelWithDebInfo\obs-plugins\64bit\omt-hx.dll
Get-ChildItem build_x64\rundir\RelWithDebInfo\bin\64bit\libomt.dll, build_x64\rundir\RelWithDebInfo\bin\64bit\libvmx.dll
```
Expected: all three exist. If `omt-hx.dll` is elsewhere, `set_target_properties_obs` is not being applied — check Step 2's last line.

- [ ] **Step 7: Run OBS and confirm the source appears**

Run the built `obs64.exe` (portable). In the Sources panel click **+**; **OMT-HX Source** must appear in the add-source list. Expected: it appears and can be added, showing a text field labelled "Source (host and name)". Do not expect a picture yet.

- [ ] **Step 8: Commit**

```powershell
git add plugins/CMakeLists.txt plugins/omt-hx breadstudio/UPSTREAM-EDITS.md
git commit -m "breadstudio: add the omt-hx input plugin skeleton"
```

---

### Task 2: The receiver thread outputs video

**Files:**
- Modify: `plugins/omt-hx/omt-hx.cpp`

**Interfaces:**
- Consumes: `omthx::Receiver`, `omthx::VideoFrame`.
- Produces: `omt_source` with an `omthx::Receiver`, a worker thread, and `update()` that (re)creates the receiver when the address setting changes; video frames handed to OBS as `VIDEO_FORMAT_BGRA`.

- [ ] **Step 1: Write the receiver and the video path**

Replace `plugins/omt-hx/omt-hx.cpp` with:
```cpp
// OMT-HX input source: plays an Open Media Transport source (plain OMT or OMT-HX) via omthx.
#include <obs-module.h>
#include <util/platform.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "omthx/omthx.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("omt-hx", "en-US")

namespace {
struct omt_source {
	obs_source_t *source = nullptr;
	std::string address;
	std::unique_ptr<omthx::Receiver> rx;
	std::thread worker;
	std::atomic<bool> run{false};
	uint64_t time_base = 0;      // OBS ns minus the first OMT timestamp times 100
	std::mutex rxMutex;          // guards rx against the tally callbacks
	std::vector<uint8_t> scratch;  // premultiplied copy, when the source sends straight alpha
};

// OMT timestamps are 100 ns units; OBS wants nanoseconds on its own clock. Anchor the first
// frame of either kind to now so video and audio share one base.
uint64_t to_obs_time(omt_source *s, int64_t omt) {
	return s->time_base + static_cast<uint64_t>(omt) * 100;
}

void worker_main(omt_source *s) {
	omthx::VideoFrame vf;
	omthx::AudioFrame af;
	bool have_base = false;
	auto anchor = [&](int64_t omt) {
		if (!have_base) {
			s->time_base = os_gettime_ns() - static_cast<uint64_t>(omt) * 100;
			have_base = true;
		}
	};
	while (s->run.load()) {
		if (s->rx->receiveVideo(vf, 20)) {
			anchor(vf.timestamp);
			struct obs_source_frame f = {};
			f.format = VIDEO_FORMAT_BGRA;
			f.width = static_cast<uint32_t>(vf.format.width);
			f.height = static_cast<uint32_t>(vf.format.height);
			f.linesize[0] = static_cast<uint32_t>(vf.format.width) * 4;
			f.timestamp = to_obs_time(s, vf.timestamp);
			f.flags = 0;  // premultiplied: what OBS expects; straight-alpha sources are converted below
			if (vf.format.alpha && !vf.premultiplied) {
				s->scratch.resize(vf.bgra.size());
				for (size_t i = 0; i < vf.bgra.size(); i += 4) {
					const unsigned a = vf.bgra[i + 3];
					for (int k = 0; k < 3; ++k)
						s->scratch[i + k] = static_cast<uint8_t>((vf.bgra[i + k] * a + 127) / 255);
					s->scratch[i + 3] = static_cast<uint8_t>(a);
				}
				f.data[0] = s->scratch.data();
			} else {
				f.data[0] = vf.bgra.data();
			}
			obs_source_output_video(s->source, &f);
		}
		while (s->rx->receiveAudio(af, 0)) {
			// Task 3 fills this in.
		}
	}
}

void stop_receiver(omt_source *s) {
	s->run = false;
	if (s->worker.joinable()) s->worker.join();
	std::lock_guard lock(s->rxMutex);
	s->rx.reset();
}

void omt_update(void *data, obs_data_t *settings) {
	auto *s = static_cast<omt_source *>(data);
	const std::string address = obs_data_get_string(settings, "address");
	if (s->address == address) return;
	stop_receiver(s);
	s->address = address;
	if (address.empty()) return;
	std::unique_ptr<omthx::Receiver> rx;
	try {
		rx = std::make_unique<omthx::Receiver>(address);
	} catch (const std::exception &e) {
		blog(LOG_WARNING, "[omt-hx] cannot open '%s': %s", address.c_str(), e.what());
		return;
	}
	{
		std::lock_guard lock(s->rxMutex);
		s->rx = std::move(rx);
	}
	s->run = true;
	s->worker = std::thread(worker_main, s);
}

const char *omt_get_name(void *) { return obs_module_text("OMTHXSource"); }

void *omt_create(obs_data_t *settings, obs_source_t *source) {
	auto *s = new omt_source();
	s->source = source;
	omt_update(s, settings);
	return s;
}

void omt_destroy(void *data) {
	auto *s = static_cast<omt_source *>(data);
	stop_receiver(s);
	delete s;
}

void omt_defaults(obs_data_t *settings) { obs_data_set_default_string(settings, "address", ""); }

obs_properties_t *omt_properties(void *) {
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_text(p, "address", obs_module_text("Source"), OBS_TEXT_DEFAULT);
	return p;
}
}  // namespace

static const struct obs_source_info omt_hx_source = {
        .id = "omt_hx_source",
        .type = OBS_SOURCE_TYPE_INPUT,
        .output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO,
        .get_name = omt_get_name,
        .create = omt_create,
        .destroy = omt_destroy,
        .update = omt_update,
        .get_defaults = omt_defaults,
        .get_properties = omt_properties,
        .icon_type = OBS_ICON_TYPE_CAMERA,
};

bool obs_module_load(void)
{
	obs_register_source(&omt_hx_source);
	return true;
}
```

- [ ] **Step 2: Build**

Run: `cmake --build --preset windows-x64 --target omt-hx`
Expected: builds.

- [ ] **Step 3: Smoke — a BreadCamera picture in OBS**

Start a BreadCamera (or the OMT-HX `omthx-send.exe` for a quick test) with a known name, then in OBS add `OMT-HX Source` and type the exact `HOST (name)` into the address field (or use `omthx-recv --list` to read it). Expected: the picture appears within about two seconds. If it never appears, check the OBS log for `[omt-hx]` and whether `libomt.dll` sits beside `obs64.exe`.

- [ ] **Step 4: Commit**

```powershell
git add plugins/omt-hx/omt-hx.cpp
git commit -m "breadstudio: omt-hx source shows the incoming picture"
```

---

### Task 3: Audio

**Files:**
- Modify: `plugins/omt-hx/omt-hx.cpp`

**Interfaces:**
- Consumes: `omthx::AudioFrame`.
- Produces: audio handed to OBS as `AUDIO_FORMAT_FLOAT_PLANAR`, mono or stereo.

- [ ] **Step 1: Fill in the audio path**

In `worker_main`, replace the empty `while (s->rx->receiveAudio(...))` body with:
```cpp
		while (s->rx->receiveAudio(af, 0)) {
			if (af.channels <= 0 || af.samples <= 0) continue;
			anchor(af.timestamp);
			struct obs_source_audio a = {};
			for (int c = 0; c < af.channels && c < MAX_AV_PLANES; ++c)
				a.data[c] = reinterpret_cast<const uint8_t *>(af.planar.data() +
									      static_cast<size_t>(c) * af.samples);
			a.frames = static_cast<uint32_t>(af.samples);
			a.speakers = af.channels == 1 ? SPEAKERS_MONO : SPEAKERS_STEREO;
			a.format = AUDIO_FORMAT_FLOAT_PLANAR;
			a.samples_per_sec = static_cast<uint32_t>(af.sampleRate);
			a.timestamp = to_obs_time(s, af.timestamp);
			obs_source_output_audio(s->source, &a);
		}
```

- [ ] **Step 2: Build and smoke**

Run: `cmake --build --preset windows-x64 --target omt-hx`
Then in OBS, add the source against a BreadCamera sending audio. Expected: the OBS audio meter moves and the sound is clean. Two channels show as stereo.

- [ ] **Step 3: Commit**

```powershell
git add plugins/omt-hx/omt-hx.cpp
git commit -m "breadstudio: omt-hx source carries audio"
```

---

### Task 4: A source picker built from discovery

**Files:**
- Modify: `plugins/omt-hx/omt-hx.cpp`

**Interfaces:**
- Consumes: `omthx::discover()`.
- Produces: a `source` list property (plus a free-text `address` fallback), refreshed by a button.

- [ ] **Step 1: Replace the text field with a discovered list**

Add includes: `#include <util/dstr.h>` is not needed. Add a refresh callback and a helper:
```cpp
namespace {
bool omt_refresh(obs_properties_t *props, obs_property_t *, void *) {
	obs_property_t *list = obs_properties_get(props, "address");
	obs_property_list_clear(list);
	obs_property_list_add_string(list, obs_module_text("Choose"), "");
	for (const std::string &a : omthx::discover()) obs_property_list_add_string(list, a.c_str(), a.c_str());
	return true;
}
}  // namespace
```
Replace `omt_properties` with:
```cpp
obs_properties_t *omt_properties(void *) {
	obs_properties_t *p = obs_properties_create();
	obs_property_t *list =
		obs_properties_add_list(p, "address", obs_module_text("Source"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(list, obs_module_text("Choose"), "");
	for (const std::string &a : omthx::discover()) obs_property_list_add_string(list, a.c_str(), a.c_str());
	obs_properties_add_button2(p, "refresh", obs_module_text("Refresh"), omt_refresh, nullptr);
	return p;
}
```
Add to `en-US.ini`:
```ini
Choose="Choose a source..."
Refresh="Refresh"
```

- [ ] **Step 2: Build and smoke**

Run: `cmake --build --preset windows-x64 --target omt-hx`
In OBS, open the source's Properties with a BreadCamera running. Expected: the list holds the source name; picking it shows the picture; `Refresh` finds a source started after the dialog opened.

- [ ] **Step 3: Commit**

```powershell
git add plugins/omt-hx/omt-hx.cpp plugins/omt-hx/data/locale/en-US.ini
git commit -m "breadstudio: pick an OMT source from discovery"
```

---

### Task 5: Tally from program and preview

**Files:**
- Modify: `plugins/omt-hx/omt-hx.cpp`

**Interfaces:**
- Consumes: `omthx::Receiver::setTally`.
- Produces: OBS `activate`/`deactivate` (preview) and `show`/`hide` (program) map to the tally the receiver sends back.

- [ ] **Step 1: Track state and send the tally**

Add to `omt_source`:
```cpp
	std::atomic<bool> active{false};   // in the main view: on program
	std::atomic<bool> showing{false};  // shown in any view: program or studio preview
```
Add helpers:
```cpp
namespace {
// OBS reports "active" (in the main view, i.e. the program output) and "showing" (in any
// view, which includes the studio-mode preview scene, the multiview and projectors). So
// program follows activate, and preview is showing-but-not-program.
void send_tally(omt_source *s) {
	std::lock_guard lock(s->rxMutex);
	if (!s->rx) return;
	const bool program = s->active.load();
	const bool preview = s->showing.load() && !program;
	s->rx->setTally(omthx::Tally{.preview = preview, .program = program});
}

void omt_activate(void *data) {
	auto *s = static_cast<omt_source *>(data);
	s->active = true;
	send_tally(s);
}
void omt_deactivate(void *data) {
	auto *s = static_cast<omt_source *>(data);
	s->active = false;
	send_tally(s);
}
void omt_show(void *data) {
	auto *s = static_cast<omt_source *>(data);
	s->showing = true;
	send_tally(s);
}
void omt_hide(void *data) {
	auto *s = static_cast<omt_source *>(data);
	s->showing = false;
	send_tally(s);
}
}  // namespace
```
In `omt_update`, after `s->worker = std::thread(worker_main, s);`, re-send the current state: `send_tally(s);`.
Add the four callbacks to `omt_hx_source`:
```cpp
        .activate = omt_activate,
        .deactivate = omt_deactivate,
        .show = omt_show,
        .hide = omt_hide,
```

- [ ] **Step 2: Build and smoke the return path**

Run: `cmake --build --preset windows-x64 --target omt-hx`
Add the source to a scene shown on program. On the BreadCamera side, its output's `tally` node (`outputs/<id>/tally`) must read `{"program":true,"preview":false}`; move the source to the preview scene in Studio mode and it must read `{"preview":true,...,"program":false}`. (`setTally` asks the far end; the far end is BreadCamera's `OmthOutput`, which publishes it.) Expected: the node flips with the scene.

- [ ] **Step 3: Commit**

```powershell
git add plugins/omt-hx/omt-hx.cpp
git commit -m "breadstudio: send program and preview tally to the OMT sender"
```

---

### Task 6: Acceptance and the smoke checklist

**Files:**
- Create if absent, else modify: `breadstudio/docs/SMOKE.md`
- Modify: `breadstudio/UPSTREAM-EDITS.md` (confirm nothing else changed), `breadstudio/docs/2026-10-12-breadstudio-s0-fork-baseline.md` (note S2a done)

- [ ] **Step 1: Add the smoke steps**

Create `breadstudio/docs/SMOKE.md` if it does not exist; append this section (keep anything already there):
```markdown
## OMT-HX input

1. Start a BreadCamera (or `omthx-send.exe`) with a known name.
2. Add `OMT-HX Source`; refresh and pick the source; the picture appears within 2 s.
3. With alpha on the sender, the picture shows the keyed edges.
4. Audio meters move.
5. Studio mode: put the source on program, then preview; BreadCamera's output `tally` follows.
6. Stop the sender; OBS stays responsive; restart it; the picture returns.
```

- [ ] **Step 2: Confirm the edits list is complete and green**

Run:
```powershell
git add breadstudio
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
```
Expected: passes. `UPSTREAM-EDITS.md` must list `plugins/CMakeLists.txt` (edited) and each of `plugins/omt-hx/CMakeLists.txt`, `plugins/omt-hx/omt-hx.cpp`, `plugins/omt-hx/data/locale/en-US.ini` (added).

- [ ] **Step 3: Full build and smoke**

Run `breadstudio/build.ps1`, then run OBS and walk the six smoke steps. Expected: all six hold.

- [ ] **Step 4: Commit**

```powershell
git add breadstudio
git commit -m "breadstudio: note the OMT-HX input in the smoke checklist"
```