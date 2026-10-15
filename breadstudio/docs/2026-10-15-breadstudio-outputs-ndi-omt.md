# BreadStudio — NDI and OMT outputs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An output set to NDI or OMT is sent, from its own canvas, over NDI and over plain OMT / OMT-HX, and the Outputs window shows each sender's receiver count — so a monitor PC, vMix or another BreadStudio receives an output.

**Architecture:** Two raw OBS outputs, each taking the output's canvas frames and its bus's audio directly (no encoder): `bread_ndi_output` in `breadstudio/ndi`, cut down from DistroAV 6.2.2's `ndi-output.cpp`, sending with NDI's own API; and `bread_omt_output` in `breadstudio/omt-hx`, on omthx's `Sender`. Both offer a `get_receivers` proc, which Plan D's `OutputEngine` already reads. The engine already creates, starts, stops and greys these by type name; this plan makes the types exist, gives each a `name` (and OMT a `mode`), and makes the canvas BGRA for an output that sends OMT, because omthx's `Sender` takes BGRA.

**Tech Stack:** C++17, libobs and obs-frontend-api from OBS Studio 32.2.2, NDI 6 SDK headers (in the tree) and its runtime DLL, the `omthx` library (the OMT-HX checkout), CMake (OBS's `windows-x64` preset), Visual Studio 2026, Windows PowerShell 5.1.

**Spec:** `breadstudio/docs/2026-10-15-breadstudio-outputs-design.md` §4.3 (the senders), §3.3, §7, §9 and §1.1 items 1 to 3. Read it before any task. Section numbers below (§) refer to it.

**Needs first:** Plans B, C and D merged. This plan fills in the raw senders Plan D's `OutputEngine::startSenders()` already creates by type name (`bread_ndi_output`, `bread_omt_output`) through its `startRaw` lambda, and whose receiver counts it already reads with `procInt(output, "get_receivers", "count", -1)`. Read `OutputEngine.hpp/.cpp`, `breadstudio/ndi/{Ndi.hpp,NdiLoad.hpp,ndi.cpp,ndi-source.cpp}`, `breadstudio/omt-hx/{omt-hx.cpp,CMakeLists.txt}` and `F:\Coding\BreadMedia\OMT-HX\include\omthx\omthx.h` as they are on `main` before those tasks; where they differ from this plan's text, keep their differences.

## What reading the code changed

Checked on 2026-10-15 against OBS 32.2.2 on `main` at `04529a5e6`, DistroAV 6.2.2 (`9b1635f`, `src/ndi-output.cpp`), the NDI 6 SDK headers in the tree, and `omthx.h`:

| Found | Consequence |
|---|---|
| A raw OBS output (`OBS_OUTPUT_AV`, not `OBS_OUTPUT_ENCODED`) takes `raw_video`/`raw_audio` callbacks and is fed with `obs_output_set_media(output, video, audio)` + `obs_output_set_mixer(output, bus)`. | Both senders are raw outputs; Plan D's `startRaw` already sets media and mixer, so neither needs an encoder or a service. |
| The output's canvas is `output_format = run.alpha ? BGRA : NV12` (Plan D). DistroAV's output maps NV12 and BGRA straight to NDI FourCCs, with no conversion. | `bread_ndi_output` needs **no** pixel conversion: it passes NV12 or BGRA through. (DistroAV's I444→UYVY converter, its `uses_video`/`uses_audio`, its groups and its Qt name-sanitiser are cut.) |
| `omthx::Sender::sendVideo(format, bgra, premultiplied, timestamp)` takes **BGRA**, `width * height * 4`, whatever the OMT mode. | The engine must give an OMT output a BGRA canvas. Task 3 makes the canvas BGRA when the output sends OMT (one line in `reconcile`), which NDI, SRT and RTMP all accept. |
| `omthx::Sender` takes the name and mode at construction; `sendAudio` takes planar float; `stats()` gives `connections`. | `bread_omt_output` builds one `Sender` in `start`, feeds `sendVideo`/`sendAudio` from the raw callbacks, and answers `get_receivers` from `stats()`. |
| NDI's `send_get_no_connections(sender, 10)` gives the receiver count; `NDIlib_video_frame_v2_t.frame_rate_N/_D` set the rate. | `get_receivers` reads it; the frame rate comes from the video's `fps_num`/`fps_den`, as the input side already does. |
| The NDI plugin registers its source whether or not NDI loaded, so a collection's NDI inputs always load (Plan C Task 1). An output type that is **not** registered greys its column (`hasOutputType`). | `bread_ndi_output` is registered **only when NDI loaded**, so the NDI column greys when the DLL is missing. The reason is shown by the Outputs window from the plugin's `ndiStatus()`, reached through a new global proc (Task 3). |
| `desk::OutputEngine::status()` reads `procInt(output, "get_receivers", "count", -1)` and shows `Outputs.State.Receivers` when it is >= 0 (Plan D). | Neither plugin needs a desk change for the count; only the `get_receivers` proc. |

## Global Constraints

- Base: OBS Studio 32.2.2. New code goes under `breadstudio/ndi`, `breadstudio/omt-hx` and `breadstudio/desk`. No OBS file is edited by this plan.
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1` passes before every commit.
- Commit messages: `breadstudio: <what changed>`. Never rebase or force-push `main`. Nothing is pushed: every `git push` is an owner gate.
- C++17 (OBS's setting): no designated initialisers; fill OBS's info structs field by field. Warnings are errors (`/W3 /WX /permissive-`): fix a warning with the smallest change, do not switch it off.
- OBS's code style: tabs, 120 columns, braces on every `if`.
- Every string the operator sees is in a locale file: `breadstudio/ndi/data/locale/en-US.ini` or `breadstudio/omt-hx/data/locale/en-US.ini` (`obs_module_text`), or `breadstudio/desk/data/locale/en-US.ini` (`desk::text`).
- The two files derived from DistroAV keep DistroAV's copyright header, followed by a line naming the release and commit they come from. NDI's DLL is never committed. The source id `bread_ndi_output` and the setting key `name` are this plan's; there is no DistroAV id to preserve for an **output** (DistroAV's own output id is `ndi_output`, and nothing in a BreadStudio collection references it).
- Windows x64 only. Build from a Visual Studio 2026 developer shell with `cmake --build --preset windows-x64 --config RelWithDebInfo --target <target>`; the program must be closed first. Run it from `build_x64\rundir\RelWithDebInfo\bin\64bit\obs64.exe --portable`; before a run, `build_x64\rundir` holds no `*sync-conflict*` file; after a normal close, the newest log ends with `Number of memory leaks: 0`.
- Test receivers on this PC: NDI Studio Monitor (NDI Tools) or `ffplay`; a second BreadStudio over OMT; vMix's NDI input.

## Review Focus

1. **A sender that is visible on the far end.** An output set to NDI appears in NDI Studio Monitor; one set to OMT is received by BreadStudio's OMT input, in all three modes. Owned by Tasks 1, 2 and 4's hand-checks.
2. **The receiver count.** The Outputs window's NDI and OMT columns show the number of receivers, and 0 when none. Owned by Task 4's hand-check, on `get_receivers`.
3. **Nothing else is disturbed.** Turning an NDI or OMT output on, off, or changing its size, leaves every other sender running (Plan D's `outputSteps`). Owned by Task 4's hand-check.
4. **No NDI DLL.** The NDI column is greyed, with the reason, and every other output still works. Owned by Task 3's hand-check.
5. **Closing clean.** Closing with an NDI and an OMT output running leaves no leaked sender or thread. Owned by Task 4's hand-check (the log's `Number of memory leaks: 0`).

## File Structure

| Path | Responsibility |
|---|---|
| `breadstudio/ndi/ndi-output.cpp` | `bread_ndi_output`: the NDI sender (after DistroAV's `ndi-output.cpp`) |
| `breadstudio/ndi/Ndi.hpp`, `ndi.cpp`, `data/locale/en-US.ini` | `ndiOutputInfo()`; registering the output when NDI loaded; the status proc; strings |
| `breadstudio/ndi/CMakeLists.txt` | The new source |
| `breadstudio/omt-hx/omt-output.cpp` | `bread_omt_output`: the OMT sender, on `omthx::Sender` |
| `breadstudio/omt-hx/omt-hx.cpp`, `data/locale/en-US.ini`, `CMakeLists.txt` | Registering the output; strings; the new source |
| `breadstudio/desk/OutputEngine.cpp` | A BGRA canvas for an output that sends OMT |
| `breadstudio/desk/OutputsWindow.cpp` | The NDI-off reason in the NDI column |
| `breadstudio/docs/DESK-SMOKE.md` | Section H, two more items |

---

### Task 1: The NDI output

**Files:**
- Create: `breadstudio/ndi/ndi-output.cpp`
- Modify: `breadstudio/ndi/Ndi.hpp`, `breadstudio/ndi/ndi.cpp`, `breadstudio/ndi/CMakeLists.txt`, `breadstudio/ndi/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `ndiLib` (null while NDI is off), `ndiStatus()` (`Ndi.hpp`).
- Produces:
  - `obs_source_info`-style `obs_output_info ndiOutputInfo();` in `Ndi.hpp`.
  - The OBS output type `bread_ndi_output`, flags `OBS_OUTPUT_AV`, with `raw_video`/`raw_audio`, setting `name` (the sender's NDI name).
  - On its proc handler: `void get_receivers(out int count)`.
  - Registered in `obs_module_load` **only while NDI is on**.

- [ ] **Step 1: Write the output**

Create `breadstudio/ndi/ndi-output.cpp`:
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

/* BreadStudio: after DistroAV 6.2.2 (9b1635f), src/ndi-output.cpp, cut down to what the desk's
 * Outputs window sets: one name, both picture and sound, no groups, no per-stream switches, and no
 * config file or Qt. DistroAV's I444 -> UYVY converter is gone: the engine's canvas is NV12 or
 * BGRA, and NDI takes both as they are (outputs spec §4.3). Its receiver count is offered through
 * `get_receivers`, which the engine reads. */

#include "Ndi.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>

namespace {

struct NdiOutput {
	obs_output_t *output = nullptr;
	std::string name;

	NDIlib_send_instance_t sender = nullptr;
	std::mutex senderMutex;

	uint32_t width = 0;
	uint32_t height = 0;
	int fpsNum = 0;
	int fpsDen = 1;
	NDIlib_FourCC_video_type_e fourcc = NDIlib_FourCC_video_type_BGRA;
	uint32_t sampleRate = 0;
	size_t channels = 0;
	bool started = false;
	std::vector<uint8_t> audioBuffer; /* the planes, one channel after another, for FLTP */
};

const char *ndiOutputName(void *)
{
	return obs_module_text("NdiOutput");
}

void ndiOutputDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "name", "BreadStudio output");
}

void ndiOutputUpdate(void *data, obs_data_t *settings)
{
	auto *o = static_cast<NdiOutput *>(data);
	o->name = obs_data_get_string(settings, "name");
}

void *ndiOutputCreate(obs_data_t *settings, obs_output_t *output)
{
	auto *o = new NdiOutput();
	o->output = output;
	ndiOutputUpdate(o, settings);
	proc_handler_add(obs_output_get_proc_handler(output), "void get_receivers(out int count)", getReceivers, o);
	return o;
}

void ndiOutputDestroy(void *data)
{
	delete static_cast<NdiOutput *>(data);
}

bool ndiOutputStart(void *data)
{
	auto *o = static_cast<NdiOutput *>(data);
	if (o->started || !ndiLib) {
		return false;
	}

	uint32_t flags = 0;
	video_t *video = obs_output_video(o->output);
	audio_t *audio = obs_output_audio(o->output);

	if (video) {
		const video_output_info *info = video_output_get_info(video);
		o->width = info->width;
		o->height = info->height;
		o->fpsNum = (int)info->fps_num;
		o->fpsDen = (int)info->fps_den;
		switch (info->format) {
		case VIDEO_FORMAT_NV12:
			o->fourcc = NDIlib_FourCC_video_type_NV12;
			break;
		case VIDEO_FORMAT_BGRA:
			o->fourcc = NDIlib_FourCC_video_type_BGRA;
			break;
		case VIDEO_FORMAT_BGRX:
			o->fourcc = NDIlib_FourCC_video_type_BGRX;
			break;
		default:
			/* the engine's canvas is NV12 or BGRA only (Plan D, outputs spec §4.2) */
			blog(LOG_WARNING, "[bread-ndi] output '%s': unsupported video format %d", o->name.c_str(),
			     (int)info->format);
			return false;
		}
		flags |= OBS_OUTPUT_VIDEO;
	}
	if (audio) {
		o->sampleRate = audio_output_get_sample_rate(audio);
		o->channels = audio_output_get_channels(audio);
		flags |= OBS_OUTPUT_AUDIO;
	}
	if (!flags) {
		return false;
	}

	NDIlib_send_create_t desc;
	desc.p_ndi_name = o->name.c_str();
	desc.p_groups = nullptr;
	desc.clock_video = false;
	desc.clock_audio = false;

	std::lock_guard lock(o->senderMutex);
	o->sender = ndiLib->send_create(&desc);
	if (!o->sender) {
		blog(LOG_WARNING, "[bread-ndi] output '%s': cannot create the NDI sender", o->name.c_str());
		return false;
	}
	o->started = obs_output_begin_data_capture(o->output, flags);
	if (!o->started) {
		ndiLib->send_destroy(o->sender);
		o->sender = nullptr;
	}
	return o->started;
}

void ndiOutputStop(void *data, uint64_t)
{
	auto *o = static_cast<NdiOutput *>(data);
	if (!o->started) {
		return;
	}
	o->started = false;
	obs_output_end_data_capture(o->output);

	std::lock_guard lock(o->senderMutex);
	if (o->sender) {
		ndiLib->send_destroy(o->sender);
		o->sender = nullptr;
	}
}

void getReceivers(void *data, calldata_t *call)
{
	auto *o = static_cast<NdiOutput *>(data);
	int count = 0;
	std::lock_guard lock(o->senderMutex);
	if (o->sender) {
		count = ndiLib ? ndiLib->send_get_no_connections(o->sender, 10) : 0;
	}
	calldata_set_int(call, "count", count);
}

void ndiOutputRawVideo(void *data, video_data *frame)
{
	auto *o = static_cast<NdiOutput *>(data);
	if (!o->started || o->width == 0 || o->height == 0) {
		return;
	}

	std::lock_guard lock(o->senderMutex);
	if (!o->sender) {
		return;
	}

	NDIlib_video_frame_v2_t video;
	video.xres = (int)o->width;
	video.yres = (int)o->height;
	video.frame_rate_N = o->fpsNum;
	video.frame_rate_D = o->fpsDen;
	video.fourCC = o->fourcc;
	video.frame_format_type = NDIlib_frame_format_type_progressive;
	video.timecode = NDIlib_send_timecode_synthesize;
	video.timestamp = (int64_t)(frame->timestamp / 100);
	video.p_data = frame->data[0];
	video.line_stride_in_bytes = (int)frame->linesize[0];
	video.p_metadata = nullptr;

	/* async: the frame stays OBS's until the next one, exactly as DistroAV sends it */
	ndiLib->send_send_video_async_v2(o->sender, &video);
}

void ndiOutputRawAudio(void *data, audio_data *frame)
{
	auto *o = static_cast<NdiOutput *>(data);
	if (!o->started || o->sampleRate == 0 || o->channels == 0) {
		return;
	}

	std::lock_guard lock(o->senderMutex);
	if (!o->sender) {
		return;
	}

	NDIlib_audio_frame_v3_t audio;
	audio.sample_rate = (int)o->sampleRate;
	audio.no_channels = (int)o->channels;
	audio.no_samples = (int)frame->frames;
	audio.timecode = NDIlib_send_timecode_synthesize;
	audio.timestamp = (int64_t)(frame->timestamp / 100);
	audio.FourCC = NDIlib_FourCC_audio_type_FLTP;
	audio.channel_stride_in_bytes = (int)(frame->frames * sizeof(float));

	/* OBS hands planar float with one plane per channel; NDI's FLTP wants the channels one after
	 * another, so a plane's samples are copied into a buffer that outlives the send */
	const size_t plane = (size_t)frame->frames * sizeof(float);
	o->audioBuffer.resize((size_t)o->channels * plane);
	for (size_t c = 0; c < o->channels; c++) {
		std::memcpy(o->audioBuffer.data() + c * plane, frame->data[c], plane);
	}
	audio.p_data = o->audioBuffer.data();

	ndiLib->send_send_audio_v3(o->sender, &audio);
}

} // namespace

obs_output_info ndiOutputInfo()
{
	obs_output_info info = {};
	info.id = "bread_ndi_output";
	info.flags = OBS_OUTPUT_AV;
	info.get_name = ndiOutputName;
	info.get_defaults = ndiOutputDefaults;
	info.create = ndiOutputCreate;
	info.start = ndiOutputStart;
	info.stop = ndiOutputStop;
	info.update = ndiOutputUpdate;
	info.destroy = ndiOutputDestroy;
	info.raw_video = ndiOutputRawVideo;
	info.raw_audio = ndiOutputRawAudio;
	return info;
}
```

> **Note:** OBS's `raw_audio` gives planar float with one plane per channel (`frame->data[0..channels-1]`), not one contiguous buffer; the copy above is what turns it into NDI's FLTP. Task 2 does the same for omthx's planar `sendAudio`.

- [ ] **Step 2: Declare and register it**

In `breadstudio/ndi/Ndi.hpp`, add before the closing `}`:
```cpp
/* the output type, filled in ndi-output.cpp */
obs_output_info ndiOutputInfo();
```

In `breadstudio/ndi/ndi.cpp`, in `obs_module_load`, after `obs_register_source(&info);`, add:
```cpp
	/* the output only exists while NDI is on, so the Outputs window greys the NDI column otherwise */
	if (ndiLib) {
		const obs_output_info output = ndiOutputInfo();
		obs_register_output(&output);
	}
```

- [ ] **Step 3: Build files and strings**

In `breadstudio/ndi/CMakeLists.txt`, add `ndi-output.cpp` to `target_sources(bread-ndi PRIVATE …)`.

Append to `breadstudio/ndi/data/locale/en-US.ini`:
```ini
NdiOutput="NDI output"
```

- [ ] **Step 4: Build, then check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-ndi bread-desk
```
Expected: builds with no warning.

Start the program, add an input, and in the Outputs window tick Output 2's NDI. Start NDI Studio Monitor on this PC.

| Do | Check |
|---|---|
| NDI Studio Monitor | Lists `BreadStudio Output 2`; choosing it shows Output 2's picture with sound. |
| The Outputs window's NDI column | Reads `1 receiver(s)` within five seconds. |
| Close Studio Monitor | Back to `0`. |

- [ ] **Step 5: Commit**

```powershell
git add breadstudio/ndi
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the NDI output, sending an output over NDI"
```

---

### Task 2: The OMT output

**Files:**
- Create: `breadstudio/omt-hx/omt-output.cpp`
- Modify: `breadstudio/omt-hx/omt-hx.cpp`, `breadstudio/omt-hx/CMakeLists.txt`, `breadstudio/omt-hx/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `omthx::Sender`, `omthx::Mode`, `omthx::Quality`, `omthx::Codec`, `omthx::VideoFormat` (`omthx.h`).
- Produces:
  - `obs_output_info omtOutputInfo();`.
  - The output type `bread_omt_output`, flags `OBS_OUTPUT_AV`, settings `name` and `mode` (`vmx`, `hevc`, `h264` as the engine's `startRaw` passes them), with `raw_video`/`raw_audio`.
  - On its proc handler: `void get_receivers(out int count)`.

- [ ] **Step 1: Write the output**

Create `breadstudio/omt-hx/omt-output.cpp`:
```cpp
/* bread_omt_output: an output's picture and sound sent as plain OMT (VMX) or OMT-HX, on omthx's
 * Sender (outputs spec §4.3). The engine gives it a BGRA canvas and one audio bus, so the picture
 * needs no conversion. Its receiver count is offered through `get_receivers`. */

#include <obs-module.h>

#include <omthx/omthx.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <cstring>
#include <exception>

namespace {

/* OMT timestamps are 10,000,000 per second, and the sender wants one per frame duration apart */
static const int64_t TicksPerSecond = 10000000;

struct OmtOutput {
	obs_output_t *output = nullptr;
	std::string name;
	omthx::Mode mode = omthx::Mode::Full;
	omthx::Codec codec = omthx::Codec::Hevc;

	std::unique_ptr<omthx::Sender> sender;
	std::mutex senderMutex;

	uint32_t width = 0;
	uint32_t height = 0;
	int fpsNum = 0;
	int fpsDen = 1;
	bool alpha = false;
	int sampleRate = 0;
	size_t channels = 0;
	bool started = false;
};

const char *omtOutputName(void *)
{
	return obs_module_text("OmtOutput");
}

void omtOutputDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "name", "BreadStudio output");
	obs_data_set_default_string(settings, "mode", "vmx");
}

void omtOutputUpdate(void *data, obs_data_t *settings)
{
	auto *o = static_cast<OmtOutput *>(data);
	o->name = obs_data_get_string(settings, "name");
	const std::string mode = obs_data_get_string(settings, "mode");
	/* the engine passes "vmx", "hevc" or "h264"; OMT (VMX) is the default */
	o->mode = mode == "hevc" || mode == "h264" ? omthx::Mode::Hx : omthx::Mode::Full;
	o->codec = mode == "h264" ? omthx::Codec::H264 : omthx::Codec::Hevc;
}

void *omtOutputCreate(obs_data_t *settings, obs_output_t *output)
{
	auto *o = new OmtOutput();
	o->output = output;
	omtOutputUpdate(o, settings);
	proc_handler_add(obs_output_get_proc_handler(output), "void get_receivers(out int count)", getReceivers, o);
	return o;
}

void omtOutputDestroy(void *data)
{
	delete static_cast<OmtOutput *>(data);
}

bool omtOutputStart(void *data)
{
	auto *o = static_cast<OmtOutput *>(data);
	if (o->started) {
		return false;
	}

	video_t *video = obs_output_video(o->output);
	audio_t *audio = obs_output_audio(o->output);
	uint32_t flags = 0;
	if (video) {
		const video_output_info *info = video_output_get_info(video);
		o->width = info->width;
		o->height = info->height;
		o->fpsNum = (int)info->fps_num;
		o->fpsDen = (int)info->fps_den;
		o->alpha = info->format == VIDEO_FORMAT_BGRA;
		flags |= OBS_OUTPUT_VIDEO;
	}
	if (audio) {
		o->sampleRate = audio_output_get_sample_rate(audio);
		o->channels = audio_output_get_channels(audio);
		flags |= OBS_OUTPUT_AUDIO;
	}
	if (!flags) {
		return false;
	}

	std::lock_guard lock(o->senderMutex);
	try {
		o->sender = std::make_unique<omthx::Sender>(o->name, o->mode, omthx::Quality::High, o->codec);
	} catch (const std::exception &error) {
		blog(LOG_WARNING, "[bread-omt] output '%s': cannot start: %s", o->name.c_str(), error.what());
		return false;
	}
	o->started = obs_output_begin_data_capture(o->output, flags);
	if (!o->started) {
		o->sender.reset();
	}
	return o->started;
}

void omtOutputStop(void *data, uint64_t)
{
	auto *o = static_cast<OmtOutput *>(data);
	if (!o->started) {
		return;
	}
	o->started = false;
	obs_output_end_data_capture(o->output);

	std::lock_guard lock(o->senderMutex);
	o->sender.reset();
}

void getReceivers(void *data, calldata_t *call)
{
	auto *o = static_cast<OmtOutput *>(data);
	int count = 0;
	std::lock_guard lock(o->senderMutex);
	if (o->sender) {
		count = o->sender->stats().connections;
	}
	calldata_set_int(call, "count", count);
}

void omtOutputRawVideo(void *data, video_data *frame)
{
	auto *o = static_cast<OmtOutput *>(data);
	if (!o->started || o->width == 0 || o->height == 0) {
		return;
	}

	std::lock_guard lock(o->senderMutex);
	if (!o->sender) {
		return;
	}

	omthx::VideoFormat format;
	format.width = (int)o->width;
	format.height = (int)o->height;
	format.fpsN = o->fpsNum;
	format.fpsD = o->fpsDen;
	format.alpha = o->alpha;

	const size_t size = (size_t)o->width * o->height * 4;
	const std::span<const uint8_t> bgra(frame->data[0], size);
	/* OMT ticks: the frame's timestamp is in OBS nanoseconds in 100 ns units */
	const int64_t timestamp = (int64_t)frame->timestamp / 100;
	o->sender->sendVideo(format, bgra, o->alpha, timestamp);
}

void omtOutputRawAudio(void *data, audio_data *frame)
{
	auto *o = static_cast<OmtOutput *>(data);
	if (!o->started || o->sampleRate == 0 || o->channels == 0) {
		return;
	}

	std::lock_guard lock(o->senderMutex);
	if (!o->sender) {
		return;
	}

	/* the raw callback's planes are one channel after another already: omthx wants the same */
	std::vector<float> planar((size_t)frame->frames * o->channels);
	for (size_t c = 0; c < o->channels; c++) {
		std::memcpy(planar.data() + c * (size_t)frame->frames, frame->data[c],
			    (size_t)frame->frames * sizeof(float));
	}
	o->sender->sendAudio(planar, o->sampleRate, (int)o->channels, (int64_t)frame->timestamp / 100);
}

} // namespace

obs_output_info omtOutputInfo()
{
	obs_output_info info = {};
	info.id = "bread_omt_output";
	info.flags = OBS_OUTPUT_AV;
	info.get_name = omtOutputName;
	info.get_defaults = omtOutputDefaults;
	info.create = omtOutputCreate;
	info.start = omtOutputStart;
	info.stop = omtOutputStop;
	info.update = omtOutputUpdate;
	info.destroy = omtOutputDestroy;
	info.raw_video = omtOutputRawVideo;
	info.raw_audio = omtOutputRawAudio;
	return info;
}
```
Add `#include <cstring>` for `std::memcpy`.

> **Note:** `omthx::Sender` takes the codec at construction; the engine's `mode` string carries it (`h264` → `Codec::H264`, else `Codec::Hevc`), and the mode collapse to `Mode::Hx` for both Hx cases is intended. The constructor may throw on a bad name, hence the `try`.

- [ ] **Step 2: Register it**

In `breadstudio/omt-hx/omt-hx.cpp`, add `#include "omt-output.hpp"` (create `breadstudio/omt-hx/omt-output.hpp` with `obs_output_info omtOutputInfo();`), and in `obs_module_load`, after the source is registered, add:
```cpp
	const obs_output_info output = omtOutputInfo();
	obs_register_output(&output);
```

- [ ] **Step 3: Build files and strings**

In `breadstudio/omt-hx/CMakeLists.txt`, add `omt-output.cpp omt-output.hpp` to the plugin's sources.

Append to `breadstudio/omt-hx/data/locale/en-US.ini`:
```ini
OmtOutput="OMT output"
```

- [ ] **Step 4: Build, then check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target omt-hx bread-desk
```
Expected: builds with no warning.

Start the program; in the Outputs window tick Output 2's OMT (mode OMT-HX HEVC). In the same program's Input Select, add an OMT input for this PC's `BreadStudio Output 2`.

| Do | Check |
|---|---|
| The OMT input | Shows Output 2's picture with sound. |
| The Outputs window's OMT column | Reads `1 receiver(s)`. |
| Change Output 2's OMT to OMT (VMX); OK | The input comes back in plain OMT by itself; the receiver count returns. |
| Set it to OMT-HX H.264; OK | The same, in H.264. |

- [ ] **Step 5: Commit**

```powershell
git add breadstudio/omt-hx
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the OMT output, sending an output over plain OMT and OMT-HX"
```

---

### Task 3: A BGRA canvas for OMT, and the NDI-off reason

**Files:**
- Modify: `breadstudio/desk/OutputEngine.cpp`, `breadstudio/ndi/ndi.cpp`, `breadstudio/ndi/Ndi.hpp`, `breadstudio/desk/OutputsWindow.cpp`, `breadstudio/desk/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `OutputConfig::omt` (`OutputPlan.hpp`); the NDI plugin's `ndiStatus()`.
- Produces: a global proc `void bread_ndi_status(out string status)`; a canvas that is BGRA for an output that sends OMT.

- [ ] **Step 1: The canvas is BGRA for an output that sends OMT**

In `breadstudio/desk/OutputEngine.cpp`, in `reconcile()`, change the canvas format line:
```cpp
			canvasInfo.output_format = run.alpha ? VIDEO_FORMAT_BGRA : VIDEO_FORMAT_NV12;
```
to:
```cpp
			/* omthx's Sender takes BGRA, so an OMT output's canvas is BGRA too (outputs spec §4.3) */
			const OutputConfig &canvasOutput = wanted.outputs[(size_t)i];
			canvasInfo.output_format = (run.alpha || canvasOutput.omt != OmtMode::Off) ? VIDEO_FORMAT_BGRA
												    : VIDEO_FORMAT_NV12;
```

- [ ] **Step 2: The NDI plugin's status, on a proc**

In `breadstudio/ndi/ndi.cpp`, add a proc `bread_ndi_status` (out string status) beside the two Extra-IP procs, answering `ndiStatus()`:
```cpp
static void getNdiStatus(void *, calldata_t *data)
{
	calldata_set_string(data, "status", status.c_str());
}
```
and register it in `obs_module_load` with the others:
```cpp
	proc_handler_add(procs, "void bread_ndi_status(out string status)", getNdiStatus, nullptr);
```

- [ ] **Step 3: The Outputs window shows the NDI-off reason**

In `breadstudio/desk/OutputsWindow.cpp`, the NDI column notes `Outputs.Missing` when the type is absent (Plan D). When the NDI **output** type is absent but the NDI **plugin** is loaded and off, show its reason instead. Add a helper that reads the proc and, in the `missing(NdiType, 5, grid)` case, prefer the reason:
```cpp
/* the NDI plugin says why NDI is off, if it is loaded at all */
static QString ndiReason()
{
	calldata_t data;
	calldata_init(&data);
	QString reason;
	if (proc_handler_call(obs_get_proc_handler(), "bread_ndi_status", &data)) {
		const char *value = calldata_string(&data, "status");
		reason = QString::fromUtf8(value ? value : "");
	}
	calldata_free(&data);
	return reason;
}
```
and where the NDI column's missing note is added, use `ndiReason()` as the text when it is not empty, else `Outputs.Missing`. Add `#include <callback/calldata.h>` and `#include <callback/proc.h>`.

- [ ] **Step 4: Build, then check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk bread-ndi omt-hx
```
Expected: builds with no warning.

| Do | Check |
|---|---|
| Rename `bin\64bit\Processing.NDI.Lib.x64.dll` away; `$env:NDI_RUNTIME_DIR_V6 = ''`; start the program; open Outputs | The NDI column is greyed, with the reason about the missing library (not "Not in this build"); OMT and SRT still work. Close, rename the DLL back. |
| An output set to OMT | Its canvas is BGRA: the picture has no colour shift on the receiving OMT input (Task 2's check). |

- [ ] **Step 5: Commit**

```powershell
git add breadstudio/desk breadstudio/ndi
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: give an OMT output a BGRA canvas, and show why NDI is off in the Outputs window"
```

---

### Task 4: The smoke checklist, and the run

**Files:**
- Modify: `breadstudio/docs/DESK-SMOKE.md`

- [ ] **Step 1: Add two items to section H**

In `breadstudio/docs/DESK-SMOKE.md`, in section H, after its last item, add:
```markdown
10. Output 2 set to NDI, and a second PC running NDI Studio Monitor or vMix:
    the output appears by name within five seconds, plays with sound, and the
    Outputs window reads one receiver; closing the receiver returns it to 0.
    With the DLL renamed away, the NDI column is greyed with the reason and
    every other output still works.
11. Output 2 set to OMT, received by another BreadStudio's OMT input, in all
    three modes (OMT, OMT-HX HEVC, OMT-HX H.264): picture and sound, the
    receiver count in the Outputs window, and a mode change followed without
    touching the input.
```

- [ ] **Step 2: Run the automated tests**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-multiview-test bread-outputs-test bread-stream-test bread-sources-test bread-desk-test bread-omt-test bread-onair-test
foreach ($t in 'desk\test-bin\RelWithDebInfo\bread-multiview-test','desk\test-bin\RelWithDebInfo\bread-outputs-test','desk\test-bin\RelWithDebInfo\bread-stream-test','desk\test-bin\RelWithDebInfo\bread-sources-test','desk\test-bin\RelWithDebInfo\bread-desk-test','omt-hx\test-bin\RelWithDebInfo\bread-omt-test','omt-hx\test-bin\RelWithDebInfo\bread-onair-test') { & "build_x64\breadstudio\$t.exe" | Select-Object -Last 1; "exit $LASTEXITCODE" }
```
Expected: every one ends `exit 0`.

- [ ] **Step 3: Run section H, and record**

Build everything (`breadstudio\build.ps1`), run section H on this PC (with a second PC for items 10 and 11, spec §1.1 items 1 to 3), and add an entry under `## Runs` in the existing form.

- [ ] **Step 4: Commit**

```powershell
git add breadstudio/docs
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the NDI and OMT outputs to the smoke checklist, and record the run"
```

---

## Left for later plans

| Item | Where |
|---|---|
| Mix inputs, their window, the on-air rule's extra step | Plan G |