# BreadStudio — OMT input, its tab and tally Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An operator adds a BreadCamera, vMix or any other OMT sender from an OMT tab in the Input Select window, sees each source's format before adding it, and the sender's tally follows what Program and Preview really draw.

**Architecture:** A new OBS plugin, `breadstudio/omt-hx`, holds one async source, `omt_hx_source`, that plays plain OMT, OMT-HX HEVC and OMT-HX H.264 through the `omthx` library compiled against OBS's own FFmpeg. The plugin keeps a catalog of announced sources and checks each one's format with a new `omthx::probe()`, which reads one frame header and decodes nothing. It publishes the sources and their formats as two lists in its settings, which the desk's new OMT tab reads, so the desk links neither omthx nor libomt. Tally comes from one header, `breadstudio/shared/OnAir.hpp`, that walks what output channel 0 and the Preview scene draw.

**Tech Stack:** C++20 for the plugin (omthx uses `std::span`), C++17 for the desk, Qt 6 Widgets, libobs and obs-frontend-api from OBS Studio 32.2.2, OBS's FFmpeg (`FFmpeg::` targets), the OMT-HX checkout at `../OMT-HX` with its patched libomt, doctest in the OMT-HX repo, CMake (OBS's `windows-x64` preset), Visual Studio 2026, Windows PowerShell 5.1.

**Spec:** `breadstudio/docs/2026-10-14-breadstudio-network-inputs-design.md`. This is its Plan B (§12): §4.1 to §4.3 for OMT, §6 rows 3 and 5, §6.1, §6.2 for one plugin, §6.3, §8, and the OMT parts of §9, §10 and §11. Read it before any task. Section numbers below (§) refer to it. This plan takes over `2026-10-13-s2-omt-hx-input.md`: its Tasks 1 to 5 are folded into Tasks 2 to 4 here, and its Task 6 (an Add menu entry) is replaced by Task 5's tab.

## What reading the code changed

Checked on 2026-10-14 against OMT-HX `fe6a3f8`, the libomtnet source it pins (`7d29031`), libobs 32.2.2 and the desk on `main` at `28180fc6e`:

| Found | Consequence |
|---|---|
| libomt's `Preview` and `CompressedOnly` receive flags together give one frame header with the source's **full** size: a plain OMT sender sends only the preview part of each VMX frame, the header is not rewritten unless libomt decodes, and an OMT-HX sender hands a preview receiver its whole frame. | `probe()` uses both flags. Nothing is decoded; plain OMT costs the preview's bandwidth for a fraction of a second. |
| libomt drops a video frame whose codec it does not know before omthx sees it (`OMTReceive.cs`, "Unsupported … codec"). | No `unknown format` state can be reached. Such a source reads `no signal`. The spec is amended in the same commit as this plan (§4.2, §6.3, §10). |
| libomt sends the receiver's tally again on every reconnect (`OMTReceive.cs` line 434). | The plugin sends tally only when it changes; a restarted sender still gets it. |
| `obs_source_enum_active_tree` keeps a hidden item until OBS's audio thread applies the hide; in a test without OBS's threads it never drops it (seen 2026-10-14). | `OnAir.hpp` walks scene items and reads each item's own visible flag. A hidden layer drops tally at once, and the rule can be tested without a running OBS. |
| libobs started with `obs_startup` and no graphics destroys nested scenes badly: "Double destroy", then a crash (seen 2026-10-14). | The on-air test frees nothing and does not shut OBS down; the process exit does. |
| OBS composites with premultiplied alpha (`OBS_BLEND_NORMAL` is ONE, INVSRCALPHA; image files are premultiplied on load). | A frame with `flags = 0` is taken as premultiplied, as S2a planned. A straight-alpha picture is premultiplied before it is handed over. |
| `omthx-send` sends no sound. | Task 1 gives it `--tone`, so sound can be checked on this PC without a camera. |
| `omt_shutdown()` is exported by libomt but not declared in the `libomt.h` OMT-HX ships; its tests declare it themselves. | The plugin declares it the same way and calls it last in `obs_module_unload`. |

## Global Constraints

- Base: OBS Studio 32.2.2. New code goes under `breadstudio/omt-hx`, `breadstudio/shared` and `breadstudio/desk`. The one OBS file edited is `plugins/CMakeLists.txt`, already on `breadstudio/UPSTREAM-EDITS.md`; its reason is reworded in the same commit (Task 2).
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1` passes before every BreadStudio commit.
- Commit messages: `breadstudio: <what changed>` in BreadStudio; `feat: …` or `docs: …` in OMT-HX, as that repo's history does. Never rebase or force-push `main`. Nothing is pushed in either repo: every `git push` is an owner gate.
- OBS's code style in BreadStudio: tabs, 120 columns, braces on every `if`. OBS builds with warnings as errors (`/W3 /WX /permissive-`): fix a warning with the smallest change, do not switch it off. OMT-HX keeps its own style (two spaces, `/W4`).
- The plugin is C++20 (`CXX_STANDARD 20` on its targets). The desk stays C++17. `OnAir.hpp` must compile as both.
- `omthx` is compiled into the plugin from the checkout at `../OMT-HX` (all four of `src/pure.cpp`, `src/encoder.cpp`, `src/decoder.cpp`, `src/link.cpp`: `link.cpp` holds the sender beside the receiver and does not link without the encoder). Do not `add_subdirectory` the OMT-HX CMake project: it finds a second FFmpeg through vcpkg. Link only OBS's `FFmpeg::avcodec`, `FFmpeg::avutil`, `FFmpeg::swscale`.
- `obs_source_info` is filled with designated initialisers in the order `libobs/obs-source.h` declares the fields: `id`, `type`, `output_flags`, `get_name`, `create`, `destroy`, `get_defaults`, `get_properties`, `update`, `video_tick`, `icon_type`.
- The source id `omt_hx_source` is written into scene collections. It does not change after Task 2.
- Every string the operator sees is in a locale file: `breadstudio/omt-hx/data/locale/en-US.ini` (read with `obs_module_text`) or `breadstudio/desk/data/locale/en-US.ini` (read with `desk::text`).
- Windows x64 only. Build BreadStudio from a Visual Studio 2026 developer shell with `cmake --build --preset windows-x64 --config RelWithDebInfo --target <target>`; the program must be closed first, because it holds the plugin DLLs. Build and test OMT-HX with its own `scripts\test.ps1`, which opens the developer shell itself.
- Run the program from `build_x64\rundir\RelWithDebInfo\bin\64bit\obs64.exe --portable`. Before a run, `build_x64\rundir` must hold no `*sync-conflict*` file (`DESK-SMOKE.md`, "Before a run"). After a run that closes the program normally, the newest log in `build_x64\rundir\RelWithDebInfo\config\obs-studio\logs` ends with `Number of memory leaks: 0`.
- The test sender is `..\OMT-HX\build\release\omthx-send.exe`. Its source is named `<THIS PC'S NAME> (<--name>)`; it prints `source: …` on start and, once a second, `connections N  tally PGM PVW`.

## Review Focus

1. **A sender that is not there.** An OMT input whose sender is absent at start, stops, or comes back in another codec must leave the program responsive, keep its tile black meanwhile, and return by itself. Owned by Task 2's hand-check (absent, stopped, restarted) and Task 3's (other codec).
2. **The format check while the program closes.** Closing the program with the Input Select window open and a source that never sends must not hang for more than the two-second probe. Owned by Task 3's hand-check.
3. **Tally only when it is true.** A camera that is only a tile in the bar, or that is in the audio bed with AFV unlit, must read neither Program nor Preview; a hidden layer must drop Program. Owned by Task 4's automated test and hand-check.
4. **Two PCs with the same source name** (§10). They must be two tiles under two PC names, and an input stays bound to the PC it was made from. Owned by Task 5's `SourceList` test.
5. **One FFmpeg in the process.** With an OMT-HX input playing, `obs64.exe` must have `avcodec-62.dll` loaded and no other `avcodec-*.dll`. Owned by Task 2's hand-check.

## File Structure

| Path | Responsibility |
|---|---|
| `../OMT-HX/include/omthx/omthx.h`, `src/link.cpp` | `SourceInfo` and `probe()` |
| `../OMT-HX/tests/link_test.cpp` | `probe()`'s tests |
| `../OMT-HX/tools/omthx_send.cpp` | `--tone` |
| `breadstudio/omt-hx/CMakeLists.txt` | The plugin, its two tests, libomt beside `obs64.exe` |
| `breadstudio/omt-hx/omt-hx.cpp` | The source: receiver thread, picture, sound, settings lists, tally |
| `breadstudio/omt-hx/Describe.hpp`, `.cpp` | The tile line and the "receiving now" line. No OBS. |
| `breadstudio/omt-hx/test/DescribeTest.cpp` | Their test, built as `bread-omt-test` |
| `breadstudio/omt-hx/Catalog.hpp`, `.cpp` | The announced sources and the background format check |
| `breadstudio/omt-hx/data/locale/en-US.ini` | The plugin's strings |
| `breadstudio/shared/OnAir.hpp` | The on-air rule (§8), header only, for this plugin and Plan C's |
| `breadstudio/shared/test/OnAirTest.cpp` | Its test, built as `bread-onair-test` |
| `breadstudio/desk/SourceList.hpp`, `.cpp` | Grouping `HOST (name)` addresses by PC. No OBS, no Qt. |
| `breadstudio/desk/test/SourceListTest.cpp` | Its test, built as `bread-sources-test` |
| `breadstudio/desk/OmtTab.hpp`, `.cpp` | The OMT tab, for a new input and for an existing one |
| `breadstudio/desk/InputSelect.cpp` | The OMT / Desktop Capture page with tabs; the OMT page for an existing OMT input |
| `breadstudio/desk/CMakeLists.txt`, `data/locale/en-US.ini` | New sources, the test, the strings |
| `plugins/CMakeLists.txt`, `breadstudio/UPSTREAM-EDITS.md` | One more `add_subdirectory` line, and its reason |
| `breadstudio/docs/DESK-SMOKE.md` | Section F |
| `breadstudio/docs/2026-10-13-s2-omt-hx-input.md` | A line saying this plan replaces it |

---

### Task 1: `probe()` in the OMT-HX repo

Work in `F:\Coding\BreadMedia\OMT-HX`. Its working tree has one untracked file (`docs/superpowers/plans/2026-10-13-h264-codec.md`) that is not this task's: leave it alone and stage files by name.

**Files:**
- Modify: `include/omthx/omthx.h`, `src/link.cpp`, `tests/link_test.cpp`, `tools/omthx_send.cpp`

**Interfaces:**
- Consumes: libomt's `omt_receive_create`, `omt_receive`, `omt_receive_destroy`; `codecFromTag` from `src/codec.h`.
- Produces, in namespace `omthx`:
  - `struct SourceInfo { Mode mode = Mode::Full; Codec codec = Codec::Hevc; VideoFormat format; };`
  - `bool probe(const std::string& address, int timeoutMs, SourceInfo& out);` — false when no picture arrived within `timeoutMs`
  - `omthx-send --tone`: a 1 kHz tone at -20 dBFS, stereo, 48 kHz, one block per video frame

- [ ] **Step 1: Write the failing tests**

In `tests/link_test.cpp`, add these three test cases just before `TEST_CASE("Sender rejects input it cannot use")`:
```cpp
TEST_CASE("probe reads a plain OMT source's full size from its preview") {
  const VideoFormat f{1280, 720, 50, 1, true};
  Sender sender("omthx-test-probe-full", Mode::Full, Quality::Medium);
  Pump pump(sender, f);
  SourceInfo info;
  REQUIRE(probe(sender.address(), 3000, info));
  CHECK(info.mode == Mode::Full);
  CHECK(info.format == f);  // the full size, though only the preview was sent
  CHECK(sender.tally() == Tally{});  // a probe never lights the sender's tally
}

TEST_CASE("probe names an HX source's codec") {
  Codec codec = Codec::Hevc;
  SUBCASE("hevc") { codec = Codec::Hevc; }
  SUBCASE("h264") { codec = Codec::H264; }
  if (!hardware(codec)) return;
  const VideoFormat f{640, 360, 60, 1, false};
  Sender sender(std::string("omthx-test-probe-") + codecName(codec), Mode::Hx, Quality::Medium, codec);
  Pump pump(sender, f);
  SourceInfo info;
  REQUIRE(probe(sender.address(), 3000, info));
  CHECK(info.mode == Mode::Hx);
  CHECK(info.codec == codec);
  CHECK(info.format == f);
}

TEST_CASE("probe gives up on an address with nothing there") {
  SourceInfo info;
  const auto t0 = Clock::now();
  CHECK_FALSE(probe("NOWHERE (omthx-test-absent)", 1000, info));
  CHECK(Clock::now() - t0 < std::chrono::seconds(3));
}
```

- [ ] **Step 2: Run them and see them fail**

Run, from `F:\Coding\BreadMedia\OMT-HX`:
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test.ps1 -Filter "probe*"
```
Expected: the build fails with `'SourceInfo': undeclared identifier` and `'probe': identifier not found`.

- [ ] **Step 3: Declare `SourceInfo` and `probe()`**

In `include/omthx/omthx.h`, after the declaration of `discover()` and before the closing `}  // namespace omthx`, add:
```cpp

// What a source sends, read from the header of one of its video frames.
struct SourceInfo {
  Mode mode = Mode::Full;
  Codec codec = Codec::Hevc;  // Hx only
  VideoFormat format;          // the picture at its full size
};

// Connects to one source for at most timeoutMs, reads the header of its first video frame, and disconnects.
// Nothing is decoded and no keyframe is asked for: a plain OMT sender sends this receiver only its small
// preview, an Hx sender its stream as it is. The sender's tally is not touched.
// False when no picture arrived in time. libomt drops a frame in a format it does not know, so a source
// sending one also gives false.
bool probe(const std::string& address, int timeoutMs, SourceInfo& out);
```

- [ ] **Step 4: Implement it**

In `src/link.cpp`, add before `std::vector<std::string> discover() {`:
```cpp
bool probe(const std::string& address, int timeoutMs, SourceInfo& out) {
  // Preview: a plain OMT sender sends only the preview part of each VMX frame, and the header keeps the full
  // size. CompressedOnly: libomt does not decode, so it does not rewrite the header to the preview's size.
  omt_receive_t* rx = omt_receive_create(
      address.c_str(), OMTFrameType_Video, OMTPreferredVideoFormat_BGRA,
      static_cast<OMTReceiveFlags>(OMTReceiveFlags_Preview | OMTReceiveFlags_CompressedOnly));
  if (!rx) return false;
  const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
  bool found = false;
  for (;;) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
    if (left <= 0) break;
    const OMTMediaFrame* f = omt_receive(rx, OMTFrameType_Video, static_cast<int>(left));
    if (!f || f->Width <= 0 || f->Height <= 0) continue;
    Codec codec = Codec::Hevc;
    out.mode = codecFromTag(static_cast<int>(f->Codec), codec) ? Mode::Hx : Mode::Full;
    out.codec = codec;
    out.format = VideoFormat{f->Width, f->Height, f->FrameRateN, f->FrameRateD, (f->Flags & OMTVideoFlags_Alpha) != 0};
    found = true;
    break;
  }
  omt_receive_destroy(rx);
  return found;
}

```

- [ ] **Step 5: Run the tests and see them pass**

Run:
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test.ps1 -Filter "probe*"
```
Expected: `[doctest] Status: SUCCESS!`, 3 test cases (the HX case runs both subcases on this PC's AMD encoder). Then the whole suite, to see nothing else moved:
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test.ps1
```
Expected: `Status: SUCCESS!`.

- [ ] **Step 6: Give the test sender a tone**

In `tools/omthx_send.cpp`:

Add `#include <cmath>` to the includes.

In `usage()`, change the second line of the text to:
```cpp
            "           [--size WxH] [--fps N] [--alpha] [--tone] [--seconds N]\n"
```
and add a line after the `Defaults:` lines, inside the same string:
```cpp
            "--tone adds a 1 kHz tone at -20 dBFS, stereo, 48 kHz.\n"
```
Beside the other option variables at the top of `main`, add:
```cpp
  bool tone = false;
  std::vector<float> block;
```
In the option loop, before the final `else usage();` branch, add a branch in the same style as its neighbours:
```cpp
    } else if (a == "--tone") {
      tone = true;
```
In the sending loop, right after the `sender.sendVideo(...)` line, add:
```cpp
      if (tone) {  // one block per frame, sized so the sample count never drifts from the frame count
        const int64_t first = i * 48000 * f.fpsD / f.fpsN;
        const size_t n = static_cast<size_t>((i + 1) * 48000 * f.fpsD / f.fpsN - first);
        block.resize(2 * n);
        for (size_t k = 0; k < n; ++k)
          block[k] = block[n + k] =
              0.1f * static_cast<float>(std::sin(2 * 3.141592653589793 * 1000 * static_cast<double>(first + static_cast<int64_t>(k)) / 48000));
        sender.sendAudio(block, 48000, 2, epoch + i * 10'000'000LL * f.fpsD / f.fpsN);
      }
```
Read the file first: if the option loop uses a different shape (for example `if … else if …` without braces), follow it.

- [ ] **Step 7: Build and check the tone**

Run `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1`. Expected: builds with no warning. Then in two terminals:
```powershell
build\release\omthx-send.exe --name "Tone check" --mode full --tone --seconds 20
build\release\omthx-recv.exe --list
```
Expected: the sender prints `source: <PC> (Tone check)` and keeps running; `--list` prints that address. (Sound itself is heard in Task 2's hand-check.)

- [ ] **Step 8: Commit**

```powershell
git add include/omthx/omthx.h src/link.cpp tests/link_test.cpp tools/omthx_send.cpp
git commit -m "feat: probe a source's format from one frame header, and a test tone in omthx-send"
```

---

### Task 2: The OMT source plays picture and sound

**Files:**
- Create: `breadstudio/omt-hx/CMakeLists.txt`, `breadstudio/omt-hx/omt-hx.cpp`, `breadstudio/omt-hx/data/locale/en-US.ini`
- Modify: `plugins/CMakeLists.txt` (OBS file), `breadstudio/UPSTREAM-EDITS.md`

**Interfaces:**
- Consumes: `omthx::Receiver`, `omthx::VideoFrame`, `omthx::AudioFrame`, `omthx::discover()`; the built OMT-HX checkout (`third_party/libomt/libomt.lib`, `libomt.dll`, `libvmx.dll`).
- Produces:
  - The OBS source type `omt_hx_source`, shown as "OMT Source", with one setting, `address` (a string: `HOST (name)` as announced, or anything libomt accepts).
  - `struct omt_source` in `omt-hx.cpp` with `source`, `address`, `rx`, `rxMutex`, `worker`, `run`, `scratch`. Tasks 3 and 4 add to it.
  - `omt-hx.dll` in `obs-plugins\64bit`; `libomt.dll` and `libvmx.dll` beside `obs64.exe`.

- [ ] **Step 1: Make sure the OMT-HX checkout is built**

Run `Test-Path ..\OMT-HX\third_party\libomt\libomt.lib`. Expected: `True`. If not, run `powershell -NoProfile -ExecutionPolicy Bypass -File ..\OMT-HX\scripts\build-libomt.ps1` (needs git, the .NET 8 SDK and Visual Studio).

- [ ] **Step 2: Write the plugin's build file**

Create `breadstudio/omt-hx/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.28...3.30)

# omthx comes from the OMT-HX checkout as source files, compiled against OBS's own FFmpeg (the FFmpeg::
# targets), so the process has one FFmpeg. Its CMake project is not used: it would find a second one.
set(OMTHX_ROOT "${CMAKE_SOURCE_DIR}/../OMT-HX" CACHE PATH "OMT-HX checkout")
if(NOT EXISTS "${OMTHX_ROOT}/third_party/libomt/libomt.lib" OR NOT EXISTS "${OMTHX_ROOT}/src/link.cpp")
  message(
    FATAL_ERROR
    "The OMT plugin needs a built OMT-HX checkout at '${OMTHX_ROOT}'. Run its scripts/build-libomt.ps1, or set -DOMTHX_ROOT=<path>."
  )
endif()

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil swscale)

add_library(omt-hx MODULE)
add_library(OBS::omt-hx ALIAS omt-hx)

set_target_properties(omt-hx PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# All four: link.cpp holds the sender beside the receiver, and the sender needs the encoder. Never called.
target_sources(
  omt-hx
  PRIVATE
    omt-hx.cpp
    "${OMTHX_ROOT}/src/pure.cpp"
    "${OMTHX_ROOT}/src/encoder.cpp"
    "${OMTHX_ROOT}/src/decoder.cpp"
    "${OMTHX_ROOT}/src/link.cpp"
)

target_include_directories(
  omt-hx
  PRIVATE "${OMTHX_ROOT}/include" "${OMTHX_ROOT}/src" "${OMTHX_ROOT}/third_party/libomt"
)

target_link_libraries(
  omt-hx
  PRIVATE
    OBS::libobs
    OBS::frontend-api
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swscale
    "${OMTHX_ROOT}/third_party/libomt/libomt.lib"
)

# libomt.dll loads libvmx.dll from the program's folder, so both go beside obs64.exe.
add_custom_command(
  TARGET omt-hx
  POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_EXECUTABLE_DESTINATION}"
  COMMAND
    "${CMAKE_COMMAND}" -E copy_if_different "${OMTHX_ROOT}/third_party/libomt/libomt.dll"
    "${OMTHX_ROOT}/third_party/libomt/libvmx.dll" "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_EXECUTABLE_DESTINATION}"
  VERBATIM
)

set_target_properties_obs(omt-hx PROPERTIES FOLDER plugins/omt-hx PREFIX "")
```

- [ ] **Step 3: Write the source**

Create `breadstudio/omt-hx/omt-hx.cpp`:
```cpp
/* The OMT source: plays one Open Media Transport source through omthx. One receiver reads plain OMT
 * (VMX), OMT-HX HEVC and OMT-HX H.264, and tells them apart by itself (network inputs spec N7). */
#include <obs-module.h>
#include <util/platform.h>

#include <atomic>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "omthx/omthx.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("omt-hx", "en-US")

/* exported by libomt.dll, but not declared in the libomt.h that OMT-HX ships */
extern "C" void omt_shutdown();

namespace {

struct omt_source {
	obs_source_t *source = nullptr;
	std::string address;
	std::unique_ptr<omthx::Receiver> rx;
	std::mutex rxMutex; /* guards rx against the UI thread */
	std::thread worker;
	std::atomic<bool> run{false};
	std::vector<uint8_t> scratch; /* a straight-alpha picture, premultiplied */
};

void worker_main(omt_source *s)
{
	omthx::VideoFrame video;
	omthx::AudioFrame audio;

	/* OMT timestamps are 100 ns units on the sender's clock. The first frame of either kind
	 * anchors them to OBS's clock, so picture and sound keep one base. */
	bool anchored = false;
	uint64_t base = 0;
	auto toObs = [&](int64_t omt) {
		if (!anchored) {
			base = os_gettime_ns() - (uint64_t)omt * 100;
			anchored = true;
		}
		return base + (uint64_t)omt * 100;
	};

	while (s->run) {
		if (s->rx->receiveVideo(video, 20)) {
			obs_source_frame frame = {};
			frame.format = VIDEO_FORMAT_BGRA;
			frame.width = (uint32_t)video.format.width;
			frame.height = (uint32_t)video.format.height;
			frame.linesize[0] = frame.width * 4;
			frame.timestamp = toObs(video.timestamp);
			frame.data[0] = video.bgra.data();

			/* OBS composites premultiplied pictures */
			if (video.format.alpha && !video.premultiplied) {
				s->scratch.resize(video.bgra.size());
				for (size_t i = 0; i < video.bgra.size(); i += 4) {
					const unsigned alpha = video.bgra[i + 3];
					for (size_t k = 0; k < 3; k++) {
						s->scratch[i + k] = (uint8_t)((video.bgra[i + k] * alpha + 127) / 255);
					}
					s->scratch[i + 3] = (uint8_t)alpha;
				}
				frame.data[0] = s->scratch.data();
			}
			obs_source_output_video(s->source, &frame);
		}

		while (s->rx->receiveAudio(audio, 0)) {
			/* the first two channels: OMT cameras send stereo */
			const int channels = audio.channels >= 2 ? 2 : 1;
			obs_source_audio out = {};
			for (int c = 0; c < channels; c++) {
				out.data[c] = (const uint8_t *)(audio.planar.data() + (size_t)c * (size_t)audio.samples);
			}
			out.frames = (uint32_t)audio.samples;
			out.speakers = channels == 2 ? SPEAKERS_STEREO : SPEAKERS_MONO;
			out.format = AUDIO_FORMAT_FLOAT_PLANAR;
			out.samples_per_sec = (uint32_t)audio.sampleRate;
			out.timestamp = toObs(audio.timestamp);
			obs_source_output_audio(s->source, &out);
		}
	}
}

void stop_receiver(omt_source *s)
{
	s->run = false;
	if (s->worker.joinable()) {
		s->worker.join();
	}
	std::lock_guard lock(s->rxMutex);
	s->rx.reset();
}

/* A receiver keeps trying by itself while its sender is away, and follows it when it comes back
 * in another format. */
void omt_update(void *data, obs_data_t *settings)
{
	auto *s = static_cast<omt_source *>(data);
	const std::string address = obs_data_get_string(settings, "address");
	if (address == s->address) {
		return;
	}

	stop_receiver(s);
	s->address = address;
	if (address.empty()) {
		return;
	}

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

const char *omt_get_name(void *)
{
	return obs_module_text("OMTSource");
}

void *omt_create(obs_data_t *settings, obs_source_t *source)
{
	auto *s = new omt_source();
	s->source = source;
	omt_update(s, settings);
	return s;
}

void omt_destroy(void *data)
{
	auto *s = static_cast<omt_source *>(data);
	stop_receiver(s);
	delete s;
}

void omt_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "address", "");
}

/* data is null when OBS asks for the type's settings without a source */
obs_properties_t *omt_properties(void *)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *address = obs_properties_add_list(props, "address", obs_module_text("Source"),
							  OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	for (const std::string &seen : omthx::discover()) {
		obs_property_list_add_string(address, seen.c_str(), seen.c_str());
	}
	return props;
}

} // namespace

static const obs_source_info omt_hx_source = {
	.id = "omt_hx_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO,
	.get_name = omt_get_name,
	.create = omt_create,
	.destroy = omt_destroy,
	.get_defaults = omt_defaults,
	.get_properties = omt_properties,
	.update = omt_update,
	.icon_type = OBS_ICON_TYPE_CAMERA,
};

bool obs_module_load(void)
{
	obs_register_source(&omt_hx_source);
	/* discovery needs a second or two: start it now, so the list is full when it is first shown */
	omthx::discover();
	return true;
}

/* after every source is gone */
void obs_module_unload(void)
{
	omt_shutdown();
}
```

Create `breadstudio/omt-hx/data/locale/en-US.ini`:
```ini
OMTSource="OMT Source"
Source="Source"
```

- [ ] **Step 4: Add the plugin to the build, and to the edits list**

In `plugins/CMakeLists.txt`, under the last line (the one that adds `breadstudio/desk`), add:
```cmake
add_subdirectory("${CMAKE_SOURCE_DIR}/breadstudio/omt-hx" "${CMAKE_BINARY_DIR}/breadstudio/omt-hx")
```
In `breadstudio/UPSTREAM-EDITS.md`, under **Edited**, replace the `plugins/CMakeLists.txt` line with:
```
- `plugins/CMakeLists.txt` - two lines that add breadstudio/desk and breadstudio/omt-hx to the build
```

- [ ] **Step 5: Configure and build**

From a VS 2026 developer shell in the repo root, with the program closed:
```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo --target omt-hx
```
Expected: `omt-hx.dll` builds with no warning. A `std::span` error means `CXX_STANDARD 20` is not applied; an error about the order of designated initialisers means a field is out of the order in Global Constraints.

Then:
```powershell
Get-ChildItem build_x64\rundir\RelWithDebInfo\obs-plugins\64bit\omt-hx.dll, build_x64\rundir\RelWithDebInfo\bin\64bit\libomt.dll, build_x64\rundir\RelWithDebInfo\bin\64bit\libvmx.dll
```
Expected: all three exist.

- [ ] **Step 6: Check by hand**

Start a sender: `..\OMT-HX\build\release\omthx-send.exe --name "Cam A" --mode hx --codec hevc --tone`. Start the program. Make a Blank input from Add Input and put it on Preview. Open OBS's Sources dock (Docks menu → Sources), press +, choose **OMT Source**, and in its list pick `<PC> (Cam A)`.

| Do | Expect |
|---|---|
| OK | The Blank tile shows the moving test pattern within two seconds. The mixer's strip for it moves with the tone; the tone is heard on Program with AFV lit. The sender prints `connections 1`. |
| In a PowerShell window: `tasklist /m avcodec* /fi "imagename eq obs64.exe"` | `avcodec-62.dll` only. |
| Stop the sender (Ctrl+C) for ten seconds | The picture freezes or goes black; the program stays responsive. |
| Start it again, the same command | The picture returns within five seconds. |
| Start it as `--mode full` instead | The picture returns, now plain OMT. |
| Change the source's address to `NOWHERE (nobody)` | Black; the program stays responsive; CPU use does not climb. |
| Close the program | It closes within five seconds; the log ends with `Number of memory leaks: 0`. |

Delete the Blank input afterwards (it holds the test layer).

- [ ] **Step 7: Commit**

```powershell
git add plugins/CMakeLists.txt breadstudio/omt-hx breadstudio/UPSTREAM-EDITS.md
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the OMT source, which plays plain OMT and OMT-HX with sound"
```
Expected: the check passes before the commit.

---

### Task 3: Each source's format, before it is added

**Files:**
- Create: `breadstudio/omt-hx/Describe.hpp`, `breadstudio/omt-hx/Describe.cpp`, `breadstudio/omt-hx/test/DescribeTest.cpp`, `breadstudio/omt-hx/Catalog.hpp`, `breadstudio/omt-hx/Catalog.cpp`
- Modify: `breadstudio/omt-hx/omt-hx.cpp`, `breadstudio/omt-hx/CMakeLists.txt`, `breadstudio/omt-hx/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `omthx::probe()`, `omthx::SourceInfo` (Task 1); `omthx::Receiver::stats()`, `omthx::ReceiverStats`.
- Produces:
  - In namespace `omt`: `std::string frameRate(int numerator, int denominator);`, `std::string formatLine(const omthx::SourceInfo &info);`, `std::string receivingLine(const omthx::ReceiverStats &stats, const char *gpu, const char *software);`
  - In namespace `omt`: `struct Seen { std::string address; std::string detail; };`, `std::vector<Seen> seen();`, `void stopCatalog();`
  - The source's settings lists, which the desk reads (§6.1):
    - `address`: one item per announced source; name and value are the address.
    - `detail`: hidden; one item per announced source; **value** is the address, **name** is the tile line (`OMT-HX · HEVC · 1920×1080 · 60`, `checking…`, `no signal`).
    - `receiving`: only when asked for an existing source; an info text whose **description** is what it receives now (`OMT-HX · HEVC · GPU decode`), or `no picture`.

The lines use `·` (U+00B7) and `×` (U+00D7), written as UTF-8 escapes as the desk writes its gear glyph.

- [ ] **Step 1: Write the failing test**

Create `breadstudio/omt-hx/test/DescribeTest.cpp`:
```cpp
#include "Describe.hpp"

#include <cstdio>
#include <string>

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

#define DOT " \xC2\xB7 "
#define TIMES "\xC3\x97"

static omthx::SourceInfo info(omthx::Mode mode, omthx::Codec codec, int w, int h, int n, int d, bool alpha)
{
	omthx::SourceInfo result;
	result.mode = mode;
	result.codec = codec;
	result.format = omthx::VideoFormat{w, h, n, d, alpha};
	return result;
}

int main()
{
	using omthx::Codec;
	using omthx::Mode;

	/* frame rates as people say them */
	CHECK(omt::frameRate(60, 1) == "60");
	CHECK(omt::frameRate(50, 1) == "50");
	CHECK(omt::frameRate(60000, 1001) == "59.94");
	CHECK(omt::frameRate(30000, 1001) == "29.97");
	CHECK(omt::frameRate(25, 0) == "?");

	/* the three formats (spec §4.2) */
	CHECK(omt::formatLine(info(Mode::Full, Codec::Hevc, 1920, 1080, 50, 1, false)) ==
	      "OMT" DOT "VMX" DOT "1920" TIMES "1080" DOT "50");
	CHECK(omt::formatLine(info(Mode::Hx, Codec::Hevc, 1920, 1080, 60, 1, false)) ==
	      "OMT-HX" DOT "HEVC" DOT "1920" TIMES "1080" DOT "60");
	CHECK(omt::formatLine(info(Mode::Hx, Codec::H264, 1280, 720, 60000, 1001, true)) ==
	      "OMT-HX" DOT "H.264" DOT "1280" TIMES "720" DOT "59.94" DOT "alpha");

	/* what an input receives now (spec §4.3) */
	omthx::ReceiverStats stats;
	stats.mode = Mode::Hx;
	stats.codec = Codec::Hevc;
	stats.decoder = "hevc (d3d11va)";
	CHECK(omt::receivingLine(stats, "GPU decode", "software decode") == "OMT-HX" DOT "HEVC" DOT "GPU decode");
	stats.codec = Codec::H264;
	stats.decoder = "h264 (software)";
	CHECK(omt::receivingLine(stats, "GPU decode", "software decode") ==
	      "OMT-HX" DOT "H.264" DOT "software decode");
	stats.mode = Mode::Full;
	stats.decoder = "vmx";
	CHECK(omt::receivingLine(stats, "GPU decode", "software decode") == "OMT" DOT "VMX");

	if (failures == 0) {
		std::printf("Describe: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
```

Create `breadstudio/omt-hx/Describe.hpp`:
```cpp
#pragma once

#include "omthx/omthx.h"

#include <string>

namespace omt {

/* "60", "59.94"; "?" when the denominator is 0 */
std::string frameRate(int numerator, int denominator);

/* A tile's line on the OMT tab (spec §4.2), e.g. "OMT-HX · HEVC · 1920×1080 · 60". */
std::string formatLine(const omthx::SourceInfo &info);

/* What an input receives now (spec §4.3), e.g. "OMT-HX · HEVC · GPU decode". `gpu` and `software`
 * are the words for the two kinds of decoder. Call only once a picture has arrived. */
std::string receivingLine(const omthx::ReceiverStats &stats, const char *gpu, const char *software);

} // namespace omt
```

Create `breadstudio/omt-hx/Describe.cpp` with stubs, so the test builds and fails:
```cpp
#include "Describe.hpp"

namespace omt {

std::string frameRate(int, int)
{
	return "";
}

std::string formatLine(const omthx::SourceInfo &)
{
	return "";
}

std::string receivingLine(const omthx::ReceiverStats &, const char *, const char *)
{
	return "";
}

} // namespace omt
```

At the end of `breadstudio/omt-hx/CMakeLists.txt`, add:
```cmake
# The tile and "receiving now" lines: no OBS. Includes omthx.h, so C++20.
add_executable(bread-omt-test test/DescribeTest.cpp Describe.cpp)
target_include_directories(bread-omt-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${OMTHX_ROOT}/include")
set_target_properties(
  bread-omt-test
  PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/omt-hx/test-bin"
    FOLDER plugins/omt-hx
)
```

- [ ] **Step 2: Run it and see it fail**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-omt-test
& build_x64\breadstudio\omt-hx\test-bin\RelWithDebInfo\bread-omt-test.exe; $LASTEXITCODE
```
Expected: `FAIL line …` for every check, and `1`.

- [ ] **Step 3: Implement the lines**

Replace `breadstudio/omt-hx/Describe.cpp` with:
```cpp
#include "Describe.hpp"

#include <cstdio>

namespace omt {

static const char *Dot = " \xC2\xB7 ";  /* " · " */
static const char *Times = "\xC3\x97"; /* "×" */

static const char *codecText(omthx::Codec codec)
{
	return codec == omthx::Codec::H264 ? "H.264" : "HEVC";
}

std::string frameRate(int numerator, int denominator)
{
	if (denominator == 0) {
		return "?";
	}
	if (numerator % denominator == 0) {
		return std::to_string(numerator / denominator);
	}
	char text[16];
	std::snprintf(text, sizeof(text), "%.2f", (double)numerator / (double)denominator);
	return text;
}

std::string formatLine(const omthx::SourceInfo &info)
{
	std::string line = info.mode == omthx::Mode::Full ? std::string("OMT") + Dot + "VMX"
							  : std::string("OMT-HX") + Dot + codecText(info.codec);
	line += Dot + std::to_string(info.format.width) + Times + std::to_string(info.format.height);
	line += Dot + frameRate(info.format.fpsN, info.format.fpsD);
	if (info.format.alpha) {
		line += std::string(Dot) + "alpha";
	}
	return line;
}

std::string receivingLine(const omthx::ReceiverStats &stats, const char *gpu, const char *software)
{
	if (stats.mode == omthx::Mode::Full) {
		return std::string("OMT") + Dot + "VMX";
	}
	/* omthx names a software decoder "hevc (software)" or "h264 (software)" */
	const bool onCpu = stats.decoder.find("(software)") != std::string::npos;
	return std::string("OMT-HX") + Dot + codecText(stats.codec) + Dot + (onCpu ? software : gpu);
}

} // namespace omt
```

- [ ] **Step 4: Run it and see it pass**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-omt-test
& build_x64\breadstudio\omt-hx\test-bin\RelWithDebInfo\bread-omt-test.exe; $LASTEXITCODE
```
Expected: `Describe: all checks passed` and `0`.

- [ ] **Step 5: Write the catalog**

Create `breadstudio/omt-hx/Catalog.hpp`:
```cpp
#pragma once

#include <string>
#include <vector>

namespace omt {

/* one announced source, with its tile line (spec §4.2) */
struct Seen {
	std::string address;
	std::string detail;
};

/* Every OMT source announced on the network, in libomt's order. Asking starts the format check
 * for new ones; the check runs only while something has asked within the last five seconds, and
 * starts again from nothing after that, so a codec changed between shows is seen (spec §6.1).
 * Any thread. */
std::vector<Seen> seen();

/* stops the check; from obs_module_unload */
void stopCatalog();

} // namespace omt
```

Create `breadstudio/omt-hx/Catalog.cpp`:
```cpp
#include "Catalog.hpp"
#include "Describe.hpp"

#include <obs-module.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

#include "omthx/omthx.h"

namespace omt {

namespace {

using Clock = std::chrono::steady_clock;

/* how long the check goes on after the last question */
constexpr auto Idle = std::chrono::seconds(5);
/* a source that sent nothing is asked again after this */
constexpr auto Retry = std::chrono::seconds(5);
/* "no signal": no picture within this (spec §4.2) */
constexpr int ProbeMs = 2000;

enum class Check { Waiting, Found, NoSignal };

struct Entry {
	Check check = Check::Waiting;
	omthx::SourceInfo info;
	Clock::time_point checkedAt;
};

std::mutex mutex;
std::condition_variable wake;
std::map<std::string, Entry> table; /* by address */
Clock::time_point asked;
std::thread worker;
bool stopping = false;

bool due(const Entry &entry, Clock::time_point now)
{
	return entry.check == Check::Waiting || (entry.check == Check::NoSignal && now - entry.checkedAt > Retry);
}

/* one probe at a time, never under the lock */
void work()
{
	std::unique_lock lock(mutex);
	while (!stopping) {
		if (Clock::now() - asked > Idle) {
			table.clear();
			wake.wait(lock, [] { return stopping || Clock::now() - asked <= Idle; });
			continue;
		}

		const Clock::time_point now = Clock::now();
		auto next = std::find_if(table.begin(), table.end(),
					 [now](const auto &entry) { return due(entry.second, now); });
		if (next == table.end()) {
			wake.wait_for(lock, std::chrono::milliseconds(250));
			continue;
		}

		const std::string address = next->first;
		lock.unlock();
		omthx::SourceInfo info;
		const bool found = omthx::probe(address, ProbeMs, info);
		lock.lock();

		auto entry = table.find(address);
		if (entry != table.end()) {
			entry->second.check = found ? Check::Found : Check::NoSignal;
			entry->second.info = info;
			entry->second.checkedAt = Clock::now();
		}
	}
}

std::string describe(const Entry &entry)
{
	switch (entry.check) {
	case Check::Found:
		return formatLine(entry.info);
	case Check::NoSignal:
		return obs_module_text("Detail.NoSignal");
	case Check::Waiting:
		break;
	}
	return obs_module_text("Detail.Checking");
}

} // namespace

std::vector<Seen> seen()
{
	const std::vector<std::string> addresses = omthx::discover();

	std::vector<Seen> result;
	std::lock_guard lock(mutex);
	if (stopping) {
		return result;
	}
	asked = Clock::now();
	if (!worker.joinable()) {
		worker = std::thread(work);
	}
	for (const std::string &address : addresses) {
		result.push_back({address, describe(table[address])});
	}
	wake.notify_one();
	return result;
}

void stopCatalog()
{
	{
		std::lock_guard lock(mutex);
		stopping = true;
	}
	wake.notify_one();
	if (worker.joinable()) {
		worker.join();
	}
}

} // namespace omt
```

- [ ] **Step 6: Publish the lists from the source**

In `breadstudio/omt-hx/omt-hx.cpp`:

Add after `#include "omthx/omthx.h"`:
```cpp
#include "Catalog.hpp"
#include "Describe.hpp"
```

Add, inside the anonymous namespace, before `omt_properties`:
```cpp
/* what this input receives now (spec §4.3) */
std::string receiving(omt_source *s)
{
	std::lock_guard lock(s->rxMutex);
	if (!s->rx) {
		return obs_module_text("Receiving.None");
	}
	const omthx::ReceiverStats stats = s->rx->stats();
	if (stats.framesReceived == 0) {
		return obs_module_text("Receiving.None");
	}
	return omt::receivingLine(stats, obs_module_text("Decode.Gpu"), obs_module_text("Decode.Software"));
}
```

Replace `omt_properties` with:
```cpp
/* The desk reads `address` and `detail` from the type's settings, and `receiving` from an input's
 * (spec §6.1). data is null when OBS asks for the type's settings without a source. */
obs_properties_t *omt_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *address = obs_properties_add_list(props, "address", obs_module_text("Source"),
							  OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_t *detail =
		obs_properties_add_list(props, "detail", "", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_set_visible(detail, false);
	for (const omt::Seen &seen : omt::seen()) {
		obs_property_list_add_string(address, seen.address.c_str(), seen.address.c_str());
		obs_property_list_add_string(detail, seen.detail.c_str(), seen.address.c_str());
	}

	if (data) {
		obs_properties_add_text(props, "receiving", receiving(static_cast<omt_source *>(data)).c_str(),
					OBS_TEXT_INFO);
	}
	return props;
}
```

In `obs_module_unload`, stop the catalog before libomt:
```cpp
void obs_module_unload(void)
{
	omt::stopCatalog();
	omt_shutdown();
}
```

In `breadstudio/omt-hx/CMakeLists.txt`, add the new files to the plugin, after `omt-hx.cpp` in `target_sources(omt-hx …)`:
```cmake
    Catalog.cpp
    Catalog.hpp
    Describe.cpp
    Describe.hpp
```

Append to `breadstudio/omt-hx/data/locale/en-US.ini`:
```ini
Detail.Checking="checking..."
Detail.NoSignal="no signal"
Receiving.None="no picture"
Decode.Gpu="GPU decode"
Decode.Software="software decode"
```

- [ ] **Step 7: Build and check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target omt-hx
```
Expected: builds with no warning.

The desk's tab comes in Task 5; until then the lists are read through obs-websocket. Switch it on (Tools → WebSocket Server Settings, port 4455, with a password) and use any websocket client to send `GetInputPropertiesListPropertyItems` with `inputName` set to an OMT input's name and `propertyName` set to `detail`. Make the input as in Task 2 Step 6. Start three senders:
```powershell
..\OMT-HX\build\release\omthx-send.exe --name "Plain" --mode full --size 1920x1080 --fps 50
..\OMT-HX\build\release\omthx-send.exe --name "Hevc" --mode hx --codec hevc --size 1920x1080 --fps 60
..\OMT-HX\build\release\omthx-send.exe --name "H264" --mode hx --codec h264 --size 1280x720 --fps 60 --alpha
```

| Do | Expect |
|---|---|
| Ask for `detail` twice, two seconds apart | First `checking...` items; then `OMT · VMX · 1920×1080 · 50`, `OMT-HX · HEVC · 1920×1080 · 60`, `OMT-HX · H.264 · 1280×720 · 60 · alpha`, each with its sender's address as value. |
| While asking, read the senders' output | `connections` rises by one for under a second on each; `tally` never shows. |
| Restart the `Hevc` sender as `--codec h264`; wait ten seconds; ask twice | Its item reads `OMT-HX · H.264 …` (the table was dropped after five idle seconds). |
| Start `omthx-send --name "Late" --seconds 1`, wait for it to end, ask | `no signal` for `Late` until discovery drops it. |
| Ask for the input's `receiving` (`GetInputPropertiesListPropertyItems` cannot; use OBS's own properties window on the source instead) | The info line reads `OMT-HX · HEVC · GPU decode` on this PC's AMD GPU. |
| Close the program while a `no signal` source is listed and the properties window is open | It closes within five seconds; the log ends with `Number of memory leaks: 0`. |

Switch obs-websocket off again.

- [ ] **Step 8: Commit**

```powershell
git add breadstudio/omt-hx
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: name each OMT source's format before it is added"
```

---

### Task 4: Tally from what Program and Preview draw

**Files:**
- Create: `breadstudio/shared/OnAir.hpp`, `breadstudio/shared/test/OnAirTest.cpp`
- Modify: `breadstudio/omt-hx/omt-hx.cpp`, `breadstudio/omt-hx/CMakeLists.txt`

**Interfaces:**
- Consumes: `omthx::Receiver::setTally`, `omthx::Tally`; `obs_get_output_source(0)`; `obs_frontend_preview_program_mode_active`, `obs_frontend_get_current_preview_scene`; `obs_queue_task`.
- Produces, in namespace `bread` (Plan C's NDI plugin uses them too):
  - `bool draws(obs_source_t *root, obs_source_t *target);`
  - `struct OnAir { bool program = false; bool preview = false; };`
  - `OnAir onAir(obs_source_t *source);` — UI thread only

The rule (§8): Program is what output channel 0 draws: the transition that feeds Program, so during a fade both sides. Preview is what the Preview scene draws, while Studio Mode is on. "Draws" is a visible item at any depth: in the scene, in an input that is a layer of it, or in a group. The audio bed is on channel 63 and does not count.

- [ ] **Step 1: Write the failing test**

Create `breadstudio/shared/test/OnAirTest.cpp`:
```cpp
#include "OnAir.hpp"

#include <cstdio>

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

static obs_source_t *scene(const char *name)
{
	return obs_scene_get_source(obs_scene_create_private(name));
}

int main()
{
	if (!obs_startup("en-US", nullptr, nullptr)) {
		std::printf("FAIL: obs_startup\n");
		return 1;
	}

	/* scenes stand in for inputs: on the desk an input is a scene */
	obs_source_t *cam = scene("cam");
	obs_source_t *program = scene("program");
	obs_source_t *box = scene("box");
	obs_source_t *other = scene("other");

	/* the input itself, as when it is on Program alone */
	CHECK(bread::draws(cam, cam));
	CHECK(!bread::draws(nullptr, cam));
	CHECK(!bread::draws(program, nullptr));

	/* absent */
	CHECK(!bread::draws(program, cam));

	/* an item, then hidden */
	obs_sceneitem_t *alone = obs_scene_add(obs_scene_from_source(program), cam);
	CHECK(bread::draws(program, cam));
	obs_sceneitem_set_visible(alone, false);
	CHECK(!bread::draws(program, cam));

	/* a layer of an input that is drawn, then that layer hidden */
	obs_scene_add(obs_scene_from_source(box), cam);
	obs_sceneitem_t *layer = obs_scene_add(obs_scene_from_source(program), box);
	CHECK(bread::draws(program, cam));
	obs_sceneitem_set_visible(layer, false);
	CHECK(!bread::draws(program, cam));

	/* in a group */
	obs_sceneitem_t *group = obs_scene_add_group(obs_scene_from_source(other), "group");
	obs_scene_add(obs_sceneitem_group_get_scene(group), cam);
	CHECK(bread::draws(other, cam));

	/* never the other way round */
	CHECK(!bread::draws(cam, other));

	/* Nothing is released and OBS is not shut down: without a graphics thread libobs destroys
	 * nested scenes badly ("Double destroy", then a crash, seen 2026-10-14). The exit frees it all. */
	if (failures == 0) {
		std::printf("OnAir: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
```

Create `breadstudio/shared/OnAir.hpp` with a stub, so the test builds and fails:
```cpp
#pragma once

#include <obs.h>
#include <obs-frontend-api.h>

namespace bread {

inline bool draws(obs_source_t *, obs_source_t *)
{
	return false;
}

} // namespace bread
```

At the end of `breadstudio/omt-hx/CMakeLists.txt`, add:
```cmake
# The on-air rule against a small scene tree. Runs with libobs and needs obs.dll on PATH. Plan C's NDI
# plugin shares the header; the test lives here, with its first user.
add_executable(bread-onair-test "${CMAKE_SOURCE_DIR}/breadstudio/shared/test/OnAirTest.cpp")
target_include_directories(bread-onair-test PRIVATE "${CMAKE_SOURCE_DIR}/breadstudio/shared")
target_link_libraries(bread-onair-test PRIVATE OBS::libobs OBS::frontend-api)
set_target_properties(
  bread-onair-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/omt-hx/test-bin" FOLDER plugins/omt-hx
)
```

- [ ] **Step 2: Run it and see it fail**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-onair-test
$env:PATH = "$PWD\build_x64\rundir\RelWithDebInfo\bin\64bit;$env:PATH"
& build_x64\breadstudio\omt-hx\test-bin\RelWithDebInfo\bread-onair-test.exe; $LASTEXITCODE
```
Expected: OBS's start-up lines, then `FAIL line …` for each check that wants `true` (four of them), and `1`.

- [ ] **Step 3: Write the rule**

Replace `breadstudio/shared/OnAir.hpp` with:
```cpp
#pragma once

/* What Program and Preview draw (network inputs spec §8). OBS's "active" and "showing" cannot be
 * used on the desk: the input bar and the multiview keep every input showing, and the audio bed
 * keeps an input with AFV unlit active. Shared by the OMT and NDI plugins. */

#include <obs.h>
#include <obs-frontend-api.h>

namespace bread {

/* True when root is target, or draws it: a visible item at any depth of a scene or a group, or
 * either side of a running transition. Each item's own visible flag is read, not OBS's active tree,
 * which keeps a hidden item until the audio thread has caught up. */
inline bool draws(obs_source_t *root, obs_source_t *target)
{
	if (!root || !target) {
		return false;
	}
	if (root == target) {
		return true;
	}

	struct Search {
		obs_source_t *target;
		bool found;
	} search{target, false};

	if (obs_scene_t *scene = obs_group_or_scene_from_source(root)) {
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				auto *s = static_cast<Search *>(param);
				if (obs_sceneitem_visible(item) && draws(obs_sceneitem_get_source(item), s->target)) {
					s->found = true;
				}
				return !s->found;
			},
			&search);
	} else if (obs_source_get_type(root) == OBS_SOURCE_TYPE_TRANSITION) {
		obs_source_enum_active_sources(
			root,
			[](obs_source_t *, obs_source_t *child, void *param) {
				auto *s = static_cast<Search *>(param);
				if (!s->found && draws(child, s->target)) {
					s->found = true;
				}
			},
			&search);
	}
	return search.found;
}

struct OnAir {
	bool program = false;
	bool preview = false;
};

/* UI thread only: the frontend is asked for the Preview scene. */
inline OnAir onAir(obs_source_t *source)
{
	obs_source_t *program = obs_get_output_source(0);
	obs_source_t *preview =
		obs_frontend_preview_program_mode_active() ? obs_frontend_get_current_preview_scene() : nullptr;

	OnAir result;
	result.program = draws(program, source);
	result.preview = draws(preview, source);

	obs_source_release(program);
	obs_source_release(preview);
	return result;
}

} // namespace bread
```

- [ ] **Step 4: Run it and see it pass**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-onair-test
& build_x64\breadstudio\omt-hx\test-bin\RelWithDebInfo\bread-onair-test.exe; $LASTEXITCODE
```
(with `PATH` set as in Step 2). Expected: `OnAir: all checks passed` and `0`.

- [ ] **Step 5: Send the tally from the source**

In `breadstudio/omt-hx/omt-hx.cpp`:

Add after `#include "Describe.hpp"`:
```cpp
#include "OnAir.hpp"
```

Add to `struct omt_source`:
```cpp
	float tallyWait = 0.0f;  /* seconds since the tally was last worked out; graphics thread */
	bool tallySent = false;  /* false until rx has been told; guarded by rxMutex */
	omthx::Tally sentTally;  /* guarded by rxMutex */
```

In `omt_update`, where the new receiver is stored, clear the flag under the same lock:
```cpp
	{
		std::lock_guard lock(s->rxMutex);
		s->rx = std::move(rx);
		s->tallySent = false;
	}
```

Add, inside the anonymous namespace, after `stop_receiver`:
```cpp
/* Runs on the UI thread, where the frontend may be asked. param is a weak reference taken by
 * omt_tick; holding the source keeps its data alive. libomt sends the tally again by itself
 * whenever the receiver reconnects, so it is sent here only when it changes. */
void update_tally(void *param)
{
	auto *weak = static_cast<obs_weak_source_t *>(param);
	obs_source_t *source = obs_weak_source_get_source(weak);
	obs_weak_source_release(weak);
	if (!source) {
		return;
	}

	const bread::OnAir on = bread::onAir(source);
	omthx::Tally tally;
	tally.preview = on.preview;
	tally.program = on.program;

	auto *s = static_cast<omt_source *>(obs_obj_get_data(source));
	if (s) {
		std::lock_guard lock(s->rxMutex);
		if (s->rx && (!s->tallySent || tally != s->sentTally)) {
			s->rx->setTally(tally);
			s->sentTally = tally;
			s->tallySent = true;
		}
	}
	obs_source_release(source);
}

/* OBS calls this every frame for every source, on screen or not: ten times a second is enough */
void omt_tick(void *data, float seconds)
{
	auto *s = static_cast<omt_source *>(data);
	s->tallyWait += seconds;
	if (s->tallyWait < 0.1f) {
		return;
	}
	s->tallyWait = 0.0f;
	obs_queue_task(OBS_TASK_UI, update_tally, obs_source_get_weak_source(s->source), false);
}
```

Add the callback to `omt_hx_source`, between `.update` and `.icon_type`:
```cpp
	.video_tick = omt_tick,
```

In `breadstudio/omt-hx/CMakeLists.txt`, add the shared folder to the plugin's include directories:
```cmake
target_include_directories(omt-hx PRIVATE "${CMAKE_SOURCE_DIR}/breadstudio/shared")
```

- [ ] **Step 6: Build and check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target omt-hx
```
Expected: builds with no warning.

Start `..\OMT-HX\build\release\omthx-send.exe --name "Cam"` and watch its `tally` column. In the program, Studio Mode on, make three inputs: `Cam` (an OMT input on that sender: a Blank with an OMT Source layer as in Task 2, or Task 5's tab if it is done), `Other` (Blank) and `Box` (Blank). Set the fade to 3000 ms.

| Do | The sender's `tally` reads |
|---|---|
| `Other` on Program and on Preview; `Cam` is only a tile in the bar | nothing |
| Click `Cam`'s tile | `PVW` |
| Transition | `PGM` |
| Add `Cam` as a layer of `Box`; `Box` on Program, `Other` on Preview | `PGM` |
| Untick the `Cam` layer in `Box`'s settings | nothing, within a second |
| `Other` on Program and on Preview; switch AFV off on `Cam`'s strip | nothing: the audio bed does not light it |
| `Cam` on Program, `Other` on Preview; Transition (3 s fade) | `PGM` until the fade ends, then nothing |
| Studio Mode off; click `Cam`'s tile | `PGM` |
| Restart the sender while `Cam` is on Program | `PGM` again within five seconds |

- [ ] **Step 7: Commit**

```powershell
git add breadstudio/shared breadstudio/omt-hx
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: send the OMT sender Program and Preview tally from what they draw"
```

---

### Task 5: The OMT tab

**Files:**
- Create: `breadstudio/desk/SourceList.hpp`, `breadstudio/desk/SourceList.cpp`, `breadstudio/desk/test/SourceListTest.cpp`, `breadstudio/desk/OmtTab.hpp`, `breadstudio/desk/OmtTab.cpp`
- Modify: `breadstudio/desk/InputSelect.cpp`, `breadstudio/desk/CMakeLists.txt`, `breadstudio/desk/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `InputPage` (`ready()`, `create()`, `apply()`, signal `readyChanged()`), `desk::createInput`, `desk::baseSource`, `desk::text`; `DesktopPage` in `InputSelect.cpp`; the OMT source's settings lists (Task 3: `address`, `detail` with the address as each item's value, `receiving` as a description).
- Produces:
  - `struct ListedSource { std::string address; std::string name; std::string detail; };`
  - `struct SenderGroup { std::string pc; std::vector<ListedSource> sources; };`
  - `std::string sourceName(const std::string &address);`
  - `std::vector<SenderGroup> groupBySender(const std::vector<std::pair<std::string, std::string>> &addressAndDetail);`
  - `class OmtTab : public InputPage` with `static constexpr const char *TypeId = "omt_hx_source";`, `explicit OmtTab(QWidget *parent)` and `OmtTab(obs_source_t *base, QWidget *parent)`
  - The row "OMT / Desktop Capture" with tabs OMT and Desktop Capture. Plan C adds NDI in front and renames the row.

- [ ] **Step 1: Write the failing test**

Create `breadstudio/desk/test/SourceListTest.cpp`:
```cpp
#include "SourceList.hpp"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace desk;

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
	/* a source's name is what is in brackets */
	CHECK(sourceName("CAM-PC (Cam 1)") == "Cam 1");
	CHECK(sourceName("CAM-PC (Cam (wide))") == "Cam (wide)");
	CHECK(sourceName("omt://10.0.0.5:6400") == "omt://10.0.0.5:6400");
	CHECK(sourceName("CAM-PC (") == "CAM-PC (");
	CHECK(sourceName("") == "");

	/* grouped by PC, PCs and sources in name order, ignoring case; details kept */
	{
		const std::vector<SenderGroup> groups = groupBySender({
			{"B-PC (z)", "d1"},
			{"a-pc (Y)", "d2"},
			{"B-PC (a)", "d3"},
		});
		CHECK(groups.size() == 2);
		if (groups.size() == 2) {
			CHECK(groups[0].pc == "a-pc");
			CHECK(groups[0].sources.size() == 1);
			CHECK(groups[1].pc == "B-PC");
			CHECK(groups[1].sources.size() == 2);
			if (groups[1].sources.size() == 2) {
				CHECK(groups[1].sources[0].name == "a");
				CHECK(groups[1].sources[0].detail == "d3");
				CHECK(groups[1].sources[0].address == "B-PC (a)");
				CHECK(groups[1].sources[1].name == "z");
			}
		}
	}

	/* the same source name on two PCs: two tiles, under two PCs (spec §10) */
	{
		const std::vector<SenderGroup> groups = groupBySender({{"PC1 (Cam)", ""}, {"PC2 (Cam)", ""}});
		CHECK(groups.size() == 2);
		if (groups.size() == 2) {
			CHECK(groups[0].sources[0].address == "PC1 (Cam)");
			CHECK(groups[1].sources[0].address == "PC2 (Cam)");
		}
	}

	/* an address with no PC in it goes in a group with no name */
	{
		const std::vector<SenderGroup> groups = groupBySender({{"omt://10.0.0.5:6400", ""}});
		CHECK(groups.size() == 1);
		if (groups.size() == 1) {
			CHECK(groups[0].pc.empty());
			CHECK(groups[0].sources[0].name == "omt://10.0.0.5:6400");
		}
	}

	CHECK(groupBySender({}).empty());

	if (failures == 0) {
		std::printf("SourceList: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
```

Create `breadstudio/desk/SourceList.hpp`:
```cpp
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace desk {

/* one source as a plugin lists it */
struct ListedSource {
	std::string address; /* "HOST (name)" as announced, or whatever was typed */
	std::string name;    /* the part in brackets; the whole address when it has none */
	std::string detail;  /* the tile's second line */
};

/* the sources of one sender PC; `pc` is empty for addresses that name none */
struct SenderGroup {
	std::string pc;
	std::vector<ListedSource> sources;
};

/* "HOST (name)" -> "name"; anything else -> itself */
std::string sourceName(const std::string &address);

/* Groups (address, detail) pairs by the PC in the address, as vMix's NDI page does. The groups,
 * and the sources in each, are in name order, ignoring case. */
std::vector<SenderGroup> groupBySender(const std::vector<std::pair<std::string, std::string>> &addressAndDetail);

} // namespace desk
```

Create `breadstudio/desk/SourceList.cpp` with stubs:
```cpp
#include "SourceList.hpp"

namespace desk {

std::string sourceName(const std::string &)
{
	return "";
}

std::vector<SenderGroup> groupBySender(const std::vector<std::pair<std::string, std::string>> &)
{
	return {};
}

} // namespace desk
```

At the end of `breadstudio/desk/CMakeLists.txt`, add:
```cmake

# Grouping announced sources by PC: no OBS, no Qt.
add_executable(bread-sources-test test/SourceListTest.cpp SourceList.cpp)
target_include_directories(bread-sources-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(
  bread-sources-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/desk/test-bin" FOLDER plugins/bread-desk
)
```

- [ ] **Step 2: Run it and see it fail**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-sources-test
& build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-sources-test.exe; $LASTEXITCODE
```
Expected: `FAIL line …` lines and `1`.

- [ ] **Step 3: Implement the grouping**

Replace `breadstudio/desk/SourceList.cpp` with:
```cpp
#include "SourceList.hpp"

#include <algorithm>
#include <cctype>

namespace desk {

/* "HOST (name)": the PC is before the first " (", the name runs to the last ")" */
static bool split(const std::string &address, std::string &pc, std::string &name)
{
	const size_t open = address.find(" (");
	if (open == std::string::npos || open == 0 || address.size() < open + 3 || address.back() != ')') {
		return false;
	}
	pc = address.substr(0, open);
	name = address.substr(open + 2, address.size() - open - 3);
	return true;
}

static std::string lower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(),
		       [](unsigned char c) { return (char)std::tolower(c); });
	return text;
}

std::string sourceName(const std::string &address)
{
	std::string pc;
	std::string name;
	return split(address, pc, name) ? name : address;
}

std::vector<SenderGroup> groupBySender(const std::vector<std::pair<std::string, std::string>> &addressAndDetail)
{
	std::vector<SenderGroup> groups;
	for (const auto &[address, detail] : addressAndDetail) {
		std::string pc;
		std::string name;
		if (!split(address, pc, name)) {
			pc.clear();
			name = address;
		}

		auto group = std::find_if(groups.begin(), groups.end(),
					  [&pc](const SenderGroup &each) { return each.pc == pc; });
		if (group == groups.end()) {
			groups.push_back({pc, {}});
			group = groups.end() - 1;
		}
		group->sources.push_back({address, name, detail});
	}

	std::sort(groups.begin(), groups.end(),
		  [](const SenderGroup &a, const SenderGroup &b) { return lower(a.pc) < lower(b.pc); });
	for (SenderGroup &group : groups) {
		std::sort(group.sources.begin(), group.sources.end(),
			  [](const ListedSource &a, const ListedSource &b) { return lower(a.name) < lower(b.name); });
	}
	return groups;
}

} // namespace desk
```

- [ ] **Step 4: Run it and see it pass**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-sources-test
& build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-sources-test.exe; $LASTEXITCODE
```
Expected: `SourceList: all checks passed` and `0`.

- [ ] **Step 5: Write the tab**

Create `breadstudio/desk/OmtTab.hpp`:
```cpp
#pragma once

#include "InputPage.hpp"

#include <QTimer>

#include <map>
#include <string>
#include <vector>

class QButtonGroup;
class QLabel;
class QLineEdit;
class QScrollArea;
class QToolButton;

namespace desk {

struct SenderGroup;

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
	void rebuild(const std::vector<SenderGroup> &groups);
	std::string selected() const;

	OBSSource base;                 /* null for a new input */
	std::string chosen;             /* the address of the tile picked */
	std::vector<std::string> shown; /* the addresses the tiles were made for, in order */
	std::map<std::string, QToolButton *> tiles;

	QScrollArea *scroll = nullptr;
	QButtonGroup *buttons = nullptr;
	QLineEdit *byAddress = nullptr;
	QLabel *receiving = nullptr; /* for an input that exists */
	QTimer timer;
};

} // namespace desk
```

Create `breadstudio/desk/OmtTab.cpp`:
```cpp
#include "OmtTab.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "SourceList.hpp"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace desk {

static const int TilesPerRow = 4;
static const int TileWidth = 200;
static const int TileHeight = 64;

/* the OMT plugin's sources with their tile lines (spec §6.1) */
static std::vector<std::pair<std::string, std::string>> listed()
{
	std::vector<std::pair<std::string, std::string>> result;
	const char *id = obs_get_latest_input_type_id(OmtTab::TypeId);
	obs_properties_t *props = id ? obs_get_source_properties(id) : nullptr;
	if (!props) {
		return result;
	}

	obs_property_t *addresses = obs_properties_get(props, "address");
	obs_property_t *details = obs_properties_get(props, "detail");
	const size_t count = addresses ? obs_property_list_item_count(addresses) : 0;
	const size_t detailCount = details ? obs_property_list_item_count(details) : 0;
	for (size_t i = 0; i < count; i++) {
		const std::string address = obs_property_list_item_string(addresses, i);
		std::string detail;
		/* each detail's value is the address it describes */
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

OmtTab::OmtTab(QWidget *parent) : InputPage(parent)
{
	build();
	refresh();
}

OmtTab::OmtTab(obs_source_t *base_, QWidget *parent) : InputPage(parent), base(base_)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	chosen = obs_data_get_string(settings, "address");

	build();

	/* what the input receives now, including the decoder (spec §4.3) */
	receiving = new QLabel(this);
	static_cast<QVBoxLayout *>(layout())->insertWidget(0, receiving);

	refresh();
}

void OmtTab::build()
{
	auto *layout = new QVBoxLayout(this);

	scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	layout->addWidget(scroll, 1);

	buttons = new QButtonGroup(this);
	connect(buttons, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button) {
		byAddress->clear();
		chosen = button->property("address").toString().toStdString();
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
		if (!typed.isEmpty() && buttons->checkedButton()) {
			buttons->setExclusive(false);
			buttons->checkedButton()->setChecked(false);
			buttons->setExclusive(true);
			chosen.clear();
		}
		emit readyChanged();
	});

	/* once a second while the window is open (spec §4.1) */
	connect(&timer, &QTimer::timeout, this, &OmtTab::refresh);
	timer.start(1000);
}

void OmtTab::refresh()
{
	const std::vector<SenderGroup> groups = groupBySender(listed());

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

	/* the formats change while the tiles stay */
	for (const SenderGroup &group : groups) {
		for (const ListedSource &source : group.sources) {
			tiles[source.address]->setText(QString::fromStdString(source.name + "\n" + source.detail));
		}
	}

	if (receiving) {
		obs_properties_t *props = obs_source_properties(base);
		obs_property_t *line = obs_properties_get(props, "receiving");
		receiving->setText(line ? text("Omt.Receiving").arg(QString::fromUtf8(obs_property_description(line)))
					: QString());
		obs_properties_destroy(props);
	}
}

/* The tiles are made again only when the set of sources changes. Setting the new content deletes
 * the old, and a deleted button leaves its group by itself. */
void OmtTab::rebuild(const std::vector<SenderGroup> &groups)
{
	auto *content = new QWidget();
	auto *column = new QVBoxLayout(content);
	column->setAlignment(Qt::AlignTop);
	tiles.clear();

	for (const SenderGroup &group : groups) {
		column->addWidget(
			new QLabel(group.pc.empty() ? text("Omt.OtherPc") : QString::fromStdString(group.pc), content));
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
			tile->setChecked(source.address == chosen && byAddress->text().isEmpty());
			buttons->addButton(tile);
			grid->addWidget(tile, i / TilesPerRow, i % TilesPerRow);
			tiles[source.address] = tile;
			i++;
		}
	}

	if (groups.empty()) {
		auto *none = new QLabel(text("Omt.None"), content);
		none->setWordWrap(true);
		column->addWidget(none);
	}

	scroll->setWidget(content);
}

std::string OmtTab::selected() const
{
	const std::string typed = byAddress->text().trimmed().toStdString();
	return typed.empty() ? chosen : typed;
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

- [ ] **Step 6: Put the tab in the window**

In `breadstudio/desk/InputSelect.cpp`:

Add to the includes:
```cpp
#include "OmtTab.hpp"
```
```cpp
#include <QTabWidget>
```
```cpp
#include <cstring>
```

After the `DesktopPage` class and before `struct Kind`, add:
```cpp
/* The OMT / Desktop Capture row: one tab per kind, as vMix groups them (spec §4). A tab whose plugin
 * is not in the build is left out. Plan C puts NDI in front. */
class CapturePage : public InputPage {
public:
	explicit CapturePage(QWidget *parent) : InputPage(parent)
	{
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);
		tabs = new QTabWidget(this);
		layout->addWidget(tabs);

		if (obs_get_latest_input_type_id(OmtTab::TypeId)) {
			addTab(new OmtTab(tabs), "Tab.Omt");
		}
		addTab(new DesktopPage(tabs), "Tab.Desktop");

		connect(tabs, &QTabWidget::currentChanged, this, &InputPage::readyChanged);
	}

	bool ready() const override { return current()->ready(); }
	OBSSource create() override { return current()->create(); }

private:
	void addTab(InputPage *page, const char *key)
	{
		tabs->addTab(page, text(key));
		connect(page, &InputPage::readyChanged, this, &InputPage::readyChanged);
	}

	InputPage *current() const { return static_cast<InputPage *>(tabs->currentWidget()); }

	QTabWidget *tabs;
};
```

In `kinds[]`, the `Kind.Desktop` row makes the new page:
```cpp
	{"Kind.Desktop", "monitor_capture",
	 [](QWidget *parent) -> InputPage * {
		 return new CapturePage(parent);
	 }},
```

In the constructor for an input that exists, choose the OMT tab for an OMT base. Replace:
```cpp
	InputPage *only = StreamPage::isStream(base) ? static_cast<InputPage *>(new StreamPage(base.Get(), pages))
						     : static_cast<InputPage *>(new PropertiesPage(base.Get(), pages));
```
with:
```cpp
	InputPage *only = nullptr;
	if (StreamPage::isStream(base)) {
		only = new StreamPage(base.Get(), pages);
	} else if (base && std::strcmp(obs_source_get_unversioned_id(base), OmtTab::TypeId) == 0) {
		only = new OmtTab(base.Get(), pages);
	} else {
		only = new PropertiesPage(base.Get(), pages);
	}
```

In `breadstudio/desk/CMakeLists.txt`, add to the plugin's `target_sources` list, in its alphabetical places:
```cmake
    OmtTab.cpp
    OmtTab.hpp
```
```cmake
    SourceList.cpp
    SourceList.hpp
```

In `breadstudio/desk/data/locale/en-US.ini`, change the `Kind.Desktop` line to:
```ini
Kind.Desktop="OMT / Desktop Capture"
```
and append:
```ini
Tab.Omt="OMT"
Tab.Desktop="Desktop Capture"
Omt.ByAddress="Add by address"
Omt.ByAddress.Hint="HOST (name), as the sender shows it"
Omt.OtherPc="Other"
Omt.None="No OMT source found yet. A sender appears here a few seconds after it starts."
Omt.Receiving="Receiving: %1"
```

- [ ] **Step 7: Build and check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk
```
Expected: builds with no warning. Start the three senders of Task 3 Step 7 (`Plain`, `Hevc`, `H264`), then the program.

| Do | Expect |
|---|---|
| Add Input; the row "OMT / Desktop Capture" | Tabs OMT and Desktop Capture; OMT is open. Within two seconds three tiles under this PC's name, each naming its format as in Task 3. OK is greyed out. |
| Click `Hevc`; OK | One new input named `Hevc`, last, on Preview, showing the test pattern. |
| Add Input again, the same row, OMT | `Hevc`'s tile is not picked. Type `<PC> (H264)` in Add by address; OK | An input named `H264` showing the pattern with its alpha. |
| The Desktop Capture tab, Screen, OK | A screen input, as before this plan. |
| ⚙ on `Hevc`, Base properties… | The OMT tab alone, `Hevc` picked, and `Receiving: OMT-HX · HEVC · GPU decode`. |
| Pick `Plain`; OK | The same input now shows the plain OMT pattern; no second input is made. |
| ⚙, Base properties… again; Cancel | Nothing changes. |
| Stop all senders; Add Input, OMT tab | Within a few seconds `no signal` on each tile, then the tiles go as discovery drops them, and the "No OMT source found yet" line shows. |
| Close the program with the window open | It closes within five seconds; the log ends with `Number of memory leaks: 0`. |

- [ ] **Step 8: Commit**

```powershell
git add breadstudio/desk
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the OMT tab to Input Select"
```

---

### Task 6: The smoke checklist, and the run

**Files:**
- Modify: `breadstudio/docs/DESK-SMOKE.md`, `breadstudio/docs/2026-10-13-s2-omt-hx-input.md`

- [ ] **Step 1: Add section F**

In `breadstudio/docs/DESK-SMOKE.md`, after section E and before `## Before a run`, add:
```markdown
## F. OMT inputs and tally

Spec: `2026-10-14-breadstudio-network-inputs-design.md`. Needs the OMT-HX test
sender, `..\OMT-HX\build\release\omthx-send.exe`; then BreadCamera and vMix
29's OMT output as real senders.

1. With three test senders running (`--mode full`, `--mode hx --codec hevc`,
   `--mode hx --codec h264 --alpha`), each tile on the OMT tab names its
   format, size and frame rate within two seconds, before anything is added.
2. An input made from each shows its picture; the H.264 one shows its alpha.
   With `--tone` on a sender, its strip moves and the tone is heard.
3. A sender restarted in the other codec comes back by itself in the same
   input, and ⚙, Base properties… then names the new format and the decoder.
4. Tally, read in the sender's `tally` column: only a tile in the bar reads
   nothing; on Preview `PVW`; on Program, alone or as a layer, `PGM`; a layer
   unticked drops `PGM`; AFV unlit with something else on Program reads
   nothing; a three-second fade holds `PGM` to its end; with Studio Mode off
   a click reads `PGM`.
5. A BreadCamera on another PC, and vMix 29's OMT output, each appear under
   their PC's name with the right format, play, and show tally.
6. `bread-omt-test`, `bread-onair-test` and `bread-sources-test` pass.
7. After closing with one OMT input of each format in the collection, the
   log has `Number of memory leaks: 0`.
```

- [ ] **Step 2: Mark the S2a plan as taken over**

In `breadstudio/docs/2026-10-13-s2-omt-hx-input.md`, directly under the title line, add:
```markdown

> **Replaced by `2026-10-14-breadstudio-omt-input.md`** (2026-10-14). That plan folds Tasks 1 to 5 below into its Tasks 2 to 4, replaces Task 6 with the OMT tab, adds the format check, and reads tally from each item's visible flag. Keep this file for its history only.
```

- [ ] **Step 3: Run the automated tests**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-omt-test bread-onair-test bread-sources-test bread-stream-test bread-desk-test
$env:PATH = "$PWD\build_x64\rundir\RelWithDebInfo\bin\64bit;$env:PATH"
foreach ($t in 'omt-hx\test-bin\RelWithDebInfo\bread-omt-test', 'omt-hx\test-bin\RelWithDebInfo\bread-onair-test', 'desk\test-bin\RelWithDebInfo\bread-sources-test', 'desk\test-bin\RelWithDebInfo\bread-stream-test', 'desk\test-bin\RelWithDebInfo\bread-desk-test') { & "build_x64\breadstudio\$t.exe" | Select-Object -Last 1; "exit $LASTEXITCODE" }
```
Expected: each prints its `all checks passed` line (or, for `bread-desk-test`, its pass line) and `exit 0`.

- [ ] **Step 4: Run sections E and F, and record**

Build everything (`breadstudio\build.ps1`), run `DESK-SMOKE.md` sections E and F on this PC with the test senders, and add an entry under `## Runs` in the existing form: the date, the commit, build folder, what passed, what was run with stand-ins, and what is still owed. F5 (BreadCamera, vMix 29) and F2's real camera audio are owed to the owner's run unless a BreadCamera is reachable.

- [ ] **Step 5: Commit**

```powershell
git add breadstudio/docs
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add OMT inputs and tally to the smoke checklist, and record the run"
```

---

## Left for Plan C and later

| Item | Where |
|---|---|
| The NDI tab in front of OMT, the row renamed "NDI / OMT / Desktop Capture", the NDI plugin using `OnAir.hpp` | Plan C |
| BreadCamera and vMix 29 as real OMT senders (F5) | The owner's run of `DESK-SMOKE.md` |
| CI fetching and building OMT-HX before configuring BreadStudio | S0 Task 8 |
| Pushing the OMT-HX commit from Task 1 | Owner gate |
