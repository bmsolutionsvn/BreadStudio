# BreadStudio — Outputs engine, SRT and RTMP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Four vMix-style outputs that each carry Program, Preview or one input at their own size, sent over SRT and to up to three RTMP destinations started by the desk's Stream button, with nothing else on the desk interrupted when one of them changes or fails.

**Architecture:** The desk gains a plain function, `outputSteps()`, that turns the outputs' settings into what must start, stop or restart, and an `OutputEngine` that carries those steps out on OBS: an OBS canvas per output 2 to 4, one video and one audio encoder per output shared by its SRT and RTMP senders, OBS's `rtmp_output`, and a new `bread_srt_output` from a small plugin, `breadstudio/srt`, that muxes to MPEG-TS and sends over FFmpeg's own `srt` protocol with its settings passed as options, never in the address. An output set to Program shows the main mix's finished picture through a tiny `bread_program` source. The Outputs window and the Stream window edit the settings; the engine saves them in the scene collection.

**Tech Stack:** C++17, Qt 6 Widgets, libobs and obs-frontend-api from OBS Studio 32.2.2 (canvases, encoders, outputs, services), OBS's FFmpeg (libavformat with libsrt), CMake (OBS's `windows-x64` preset), Visual Studio 2026, Windows PowerShell 5.1; `ffmpeg.exe` and `ffplay.exe` (with SRT and RTMP) for the hand-checks.

**Spec:** `breadstudio/docs/2026-10-15-breadstudio-outputs-design.md`. This is its Plan D (§10): §3 (but the MultiView, NDI and OMT columns only as far as §3.3 needs them), §4, §7, §8, the output-plan and SRT-options tests of §9, and §13's first two risks. Read it before any task. Section numbers below (§) refer to it.

## What reading the code changed

Checked on 2026-10-15 against OBS 32.2.2 on `main` at `f19cfd6be`:

| Found | Consequence |
|---|---|
| `output_frames()` in `libobs/obs-video.c` draws the mixes in the order they were made, the main mix first, and `render_video()` draws the main mix's texture every frame whether or not anything is live. `obs_render_main_texture()` draws that texture. | `bread_program` is about twenty lines: it calls `obs_render_main_texture()`. An output set to Program shows the frame Program shows. |
| `obs_canvas_reset_video()` refuses while any output is live, and every canvas runs at the main mix's frame rate whatever it is given. `obs_canvas_create_private()` builds a canvas's mix at once and is not refused. | A size or alpha change makes a new canvas (§4.2). Canvases are private and `EPHEMERAL`: the engine saves its own settings. |
| OBS's `ffmpeg_mpegts_muxer`, built with `ENABLE_NEW_MPEGTS_OUTPUT` (on by default), sends SRT through its own copy of FFmpeg's libsrt code (`obs-ffmpeg-srt.h`) and reads SRT's settings from the address. | As the spec says, `bread_srt_output` replaces it. FFmpeg's own `srt` protocol takes `mode`, `latency` (microseconds), `passphrase`, `pbkeylen` and `streamid` as options, as the Stream / SRT input already gives them. |
| OBS's six audio tracks are all the desk's buses (M, A to E); `MAX_AUDIO_MIXES` is 6. An audio encoder or a raw output takes exactly one track. | The spec's "Audio: none" cannot be built: there is no seventh, silent track. An output's audio is one of M or A to E; for silence, pick a bus nothing is sent to. The spec is amended in this plan's commit (§3.1). |
| `rtmp_output` needs an `rtmp_custom` service holding `server` and `key`. `obs_output_set_reconnect_settings()` sets its retries. | Each destination is a private service plus an `rtmp_output`. |
| The Controls dock's Start Streaming button is a `QPushButton` named `streamButton` (`frontend/forms/OBSBasicControls.ui`). | Hidden by name, as the desk finds OBS's docks. |
| An OBS output's proc handler (`obs_output_get_proc_handler`) can carry procedures the output adds. | `bread_srt_output` offers `get_state`; Plan F's NDI and OMT outputs will offer `get_receivers`. The desk reads both without linking either plugin. |

## Global Constraints

- Base: OBS Studio 32.2.2. New code goes under `breadstudio/desk` and `breadstudio/srt`. The one OBS file edited is `plugins/CMakeLists.txt`, already on `breadstudio/UPSTREAM-EDITS.md`; its reason is reworded in the same commit (Task 2).
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1` passes before every commit.
- Commit messages: `breadstudio: <what changed>`. Never rebase or force-push `main`. Nothing is pushed: every `git push` is an owner gate.
- C++17 (OBS's setting): no designated initialisers; fill OBS's info structs field by field. Warnings are errors (`/W3 /WX /permissive-`): fix a warning with the smallest change, do not switch it off. FFmpeg's `av_err2str` is a C compound literal and does not compile as C++: use `av_strerror` into a buffer.
- OBS's code style: tabs, 120 columns, braces on every `if`.
- Every string the operator sees is in a locale file: `breadstudio/desk/data/locale/en-US.ini` (`desk::text`) or `breadstudio/srt/data/locale/en-US.ini` (`obs_module_text`).
- The desk links no protocol library. It makes outputs by type name: `bread_srt_output`, `rtmp_output`, and, after Plan F, `bread_ndi_output` and `bread_omt_output`. A type that is not registered greys its column out with `Outputs.Missing`.
- Contract for Plan F's raw outputs, used by this plan's engine and window: settings `name` (the sender's name) and, for OMT, `mode` (`vmx`, `hevc` or `h264`); video and audio given with `obs_output_set_media()` and the bus with `obs_output_set_mixer()`; a proc `void get_receivers(out int count)`.
- Windows x64 only. Build from a Visual Studio 2026 developer shell with `cmake --build --preset windows-x64 --config RelWithDebInfo --target <target>`; the program must be closed first. Run it from `build_x64\rundir\RelWithDebInfo\bin\64bit\obs64.exe --portable`; before a run, `build_x64\rundir` holds no `*sync-conflict*` file; after a normal close, the newest log ends with `Number of memory leaks: 0`.
- Test receivers on this PC: `ffplay` for SRT (`ffplay -mode listener -passphrase "…" srt://0.0.0.0:9000`, or `ffplay srt://127.0.0.1:9000` for a caller to call) and for RTMP (`ffplay -listen 1 rtmp://127.0.0.1:1935/live/test`, which waits for one publisher).

## Review Focus

1. **A change that must not interrupt.** Starting the Stream button, changing a destination's key, or changing an output's source must leave every other running sender untouched, with no gap on its receiver. Owned by Task 3's `outputSteps` tests and Task 4's and Task 6's hand-checks.
2. **A passphrase or stream ID with `&`, `=`, `?`, a space or a quote.** It must reach the SRT receiver unchanged. Owned by Task 1's tests and Task 2's hand-check.
3. **A receiver that never comes, or goes away.** An SRT listener nobody calls, a caller whose listener is gone, an RTMP server that is down: the program stays responsive, closes within five seconds, and the sender recovers by itself when the far end returns. Owned by Task 2's and Task 6's hand-checks.
4. **The picture going away.** Deleting the input an output shows, or switching scene collection, keeps the output's senders running, black, and its receivers connected. Owned by Task 4's hand-check.
5. **Old and odd scene collections.** A collection without outputs loads with all off; one whose output names an input that no longer exists loads with that output black and its source reading "none". Owned by Task 4's and Task 5's hand-checks.

## File Structure

| Path | Responsibility |
|---|---|
| `breadstudio/desk/StreamAddress.hpp`, `.cpp` | `srtOutputTarget()`: an SRT form as an address plus separate options |
| `breadstudio/desk/test/StreamAddressTest.cpp` | Its tests, beside the input side's |
| `breadstudio/srt/CMakeLists.txt`, `srt-output.cpp`, `data/locale/en-US.ini` | `bread_srt_output` |
| `breadstudio/desk/OutputPlan.hpp`, `.cpp` | The outputs' settings as data; `outputRun()` and `outputSteps()`. No OBS, no Qt. |
| `breadstudio/desk/test/OutputPlanTest.cpp` | Their test, built as `bread-outputs-test` |
| `breadstudio/desk/ProgramSource.hpp`, `.cpp` | `bread_program` |
| `breadstudio/desk/OutputEngine.hpp`, `.cpp` | Canvases, encoders, senders, destinations; saving and loading; status |
| `breadstudio/desk/SrtDialog.hpp`, `.cpp` | An output's SRT settings |
| `breadstudio/desk/OutputsWindow.hpp`, `.cpp` | The Outputs window |
| `breadstudio/desk/StreamWindow.hpp`, `.cpp` | The three RTMP destinations |
| `breadstudio/desk/InputBar.cpp`, `.hpp` | Outputs button; the Stream button drives the destinations; its ⚙ |
| `breadstudio/desk/DeskLayout.cpp`, `.hpp` | Hiding OBS's Start Streaming button |
| `breadstudio/desk/desk.cpp` | Wiring: registering `bread_program`, saving, loading, events |
| `breadstudio/desk/CMakeLists.txt`, `data/locale/en-US.ini` | New files, the test, strings |
| `plugins/CMakeLists.txt`, `breadstudio/UPSTREAM-EDITS.md` | One more `add_subdirectory` line, and its reason |
| `breadstudio/docs/DESK-SMOKE.md` | Section H |
| `breadstudio/docs/2026-10-15-breadstudio-outputs-design.md` | "Audio: none" removed |

---

### Task 1: An SRT form as an address and separate options

**Files:**
- Modify: `breadstudio/desk/StreamAddress.hpp`, `breadstudio/desk/StreamAddress.cpp`, `breadstudio/desk/test/StreamAddressTest.cpp`

**Interfaces:**
- Consumes: `StreamForm`, `StreamType`, `streamProblem()` (Plan A).
- Produces:
  - `struct SrtOutputTarget { std::string url; std::vector<std::pair<std::string, std::string>> options; };`
  - `SrtOutputTarget srtOutputTarget(const StreamForm &form);` — for an SRT type whose `streamProblem()` is null. `url` is `srt://host:port` only; `options` are FFmpeg's `srt` protocol options, each value exactly as typed.

- [ ] **Step 1: Write the failing tests**

In `breadstudio/desk/test/StreamAddressTest.cpp`, add before the final `if (failures == 0)`:
```cpp
	/* An output's SRT: the address is host and port only, and every value is its own option,
	 * unquoted, because it goes to FFmpeg one by one (outputs spec §4.4) */
	{
		StreamForm form = srt(StreamType::SrtCaller, "receiver.example");
		form.port = 9000;
		form.latencyMs = 120;
		SrtOutputTarget target = srtOutputTarget(form);
		CHECK(target.url == "srt://receiver.example:9000");
		CHECK(target.options.size() == 2);
		CHECK(target.options[0] == std::make_pair(std::string("mode"), std::string("caller")));
		CHECK(target.options[1] == std::make_pair(std::string("latency"), std::string("120000")));
	}
	{
		StreamForm form = srt(StreamType::SrtListener, "");
		form.port = 9001;
		form.passphrase = "a&b=c?d e'f\"g";
		form.keyLength = 16;
		form.streamId = "#!::r=live/one,m=publish";
		SrtOutputTarget target = srtOutputTarget(form);
		CHECK(target.url == "srt://0.0.0.0:9001");
		CHECK(target.options.size() == 5);
		CHECK(target.options[0].second == "listener");
		CHECK(target.options[2] == std::make_pair(std::string("passphrase"), std::string("a&b=c?d e'f\"g")));
		CHECK(target.options[3] == std::make_pair(std::string("pbkeylen"), std::string("16")));
		CHECK(target.options[4] == std::make_pair(std::string("streamid"), std::string("#!::r=live/one,m=publish")));
	}
	{
		StreamForm form = srt(StreamType::SrtRendezvous, "fd00::5");
		form.passphrase = "0123456789";
		form.keyLength = 7; /* not one of SRT's: 32 */
		SrtOutputTarget target = srtOutputTarget(form);
		CHECK(target.url == "srt://[fd00::5]:5000");
		CHECK(target.options[0].second == "rendezvous");
		CHECK(target.options[3].second == "32");
	}
```
Add `#include <utility>` beside the test's other includes.

In `breadstudio/desk/StreamAddress.hpp`, add `#include <utility>` and `#include <vector>` after `#include <string>`, and before the closing `} // namespace desk`:
```cpp

/* What an output's SRT sender is given: the address with host and port only, and FFmpeg's `srt`
 * options one by one, each value exactly as typed (outputs spec §4.4). */
struct SrtOutputTarget {
	std::string url;
	std::vector<std::pair<std::string, std::string>> options;
};

/* Call only for an SRT type whose streamProblem() is null. */
SrtOutputTarget srtOutputTarget(const StreamForm &form);
```

In `breadstudio/desk/StreamAddress.cpp`, add before the closing `} // namespace desk` a stub so the test builds and fails:
```cpp
SrtOutputTarget srtOutputTarget(const StreamForm &)
{
	return {};
}
```

- [ ] **Step 2: Run them and see them fail**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-stream-test
& build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-stream-test.exe; $LASTEXITCODE
```
Expected: `FAIL line …` for the new checks, and `1`.

- [ ] **Step 3: Implement it**

Replace the stub with:
```cpp
SrtOutputTarget srtOutputTarget(const StreamForm &form)
{
	SrtOutputTarget target;
	const char *mode = form.type == StreamType::SrtCaller     ? "caller"
			   : form.type == StreamType::SrtListener ? "listener"
							  : "rendezvous";
	target.url = "srt://" + hostPort(form.host, "0.0.0.0", form.port);

	/* FFmpeg takes SRT's latency in microseconds */
	target.options.emplace_back("mode", mode);
	target.options.emplace_back("latency", std::to_string((long long)form.latencyMs * 1000));
	if (!form.passphrase.empty()) {
		const int length = (form.keyLength == 16 || form.keyLength == 24) ? form.keyLength : 32;
		target.options.emplace_back("passphrase", form.passphrase);
		target.options.emplace_back("pbkeylen", std::to_string(length));
	}
	if (!form.streamId.empty()) {
		target.options.emplace_back("streamid", form.streamId);
	}
	return target;
}
```

- [ ] **Step 4: Run them and see them pass**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-stream-test
& build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-stream-test.exe; $LASTEXITCODE
```
Expected: `StreamAddress: all checks passed` and `0`.

- [ ] **Step 5: Commit**

```powershell
git add breadstudio/desk/StreamAddress.hpp breadstudio/desk/StreamAddress.cpp breadstudio/desk/test/StreamAddressTest.cpp
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: give an output's SRT settings as an address and separate options"
```

---

### Task 2: The SRT output, on FFmpeg's own protocol

**Files:**
- Create: `breadstudio/srt/CMakeLists.txt`, `breadstudio/srt/srt-output.cpp`, `breadstudio/srt/data/locale/en-US.ini`
- Modify: `plugins/CMakeLists.txt` (OBS file), `breadstudio/UPSTREAM-EDITS.md`

**Interfaces:**
- Consumes: OBS's encoders, given to the output by the engine (Task 4).
- Produces:
  - The OBS output type `bread_srt_output` (encoded, video and audio). Settings: `url` (string) and `options` (an object whose every item is a string option for FFmpeg's `srt` protocol).
  - On its proc handler: `void get_state(out int state)`: 0 off, 1 waiting (a listener listening, a caller calling), 2 connected, 3 trying again.
  - `bread-srt.dll` in `obs-plugins\64bit`.

- [ ] **Step 1: Write the output**

Create `breadstudio/srt/srt-output.cpp`:
```cpp
/* bread_srt_output: an output's encoded picture and sound, muxed to MPEG-TS by FFmpeg and sent over
 * FFmpeg's own `srt` protocol, with SRT's settings as options and never in the address (outputs
 * spec §4.4). OBS's own MPEG-TS output carries its own copy of the SRT code and reads the settings
 * from the address, where a passphrase with '&' is cut short. */
#include <obs-module.h>
#include <media-io/audio-io.h>
#include <media-io/video-io.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bread-srt", "en-US")

namespace {

/* what get_state answers; the desk words them (outputs spec §3.3) */
enum State { StateOff = 0, StateWaiting = 1, StateConnected = 2, StateRetrying = 3 };

/* About ten seconds of a 60 fps picture with its sound. More means the network has fallen behind:
 * the queue is dropped and sending starts again at the next keyframe. */
constexpr size_t MaxQueued = 1200;

struct Packet {
	std::vector<uint8_t> data;
	int64_t pts = 0;
	int64_t dts = 0;
	AVRational timebase = {1, 1};
	bool video = false;
	bool keyframe = false;
};

struct SrtOutput {
	obs_output_t *output = nullptr;
	std::string url;
	std::vector<std::pair<std::string, std::string>> options;
	bool listener = false;

	std::thread worker;
	std::atomic<bool> stopping{false};
	std::atomic<int> state{StateOff};
	std::atomic<uint64_t> bytes{0};

	std::mutex mutex; /* guards everything below */
	std::condition_variable wake;
	std::deque<Packet> queue;
	bool connected = false; /* packets are kept only while connected */
	bool waitKey = true;    /* after a connect or a drop: nothing until a video keyframe */
};

std::string errorText(int code)
{
	char text[AV_ERROR_MAX_STRING_SIZE] = {};
	av_strerror(code, text, sizeof(text));
	return text;
}

/* FFmpeg calls this while it waits on the network: non-zero ends the wait */
int interrupted(void *opaque)
{
	return static_cast<SrtOutput *>(opaque)->stopping ? 1 : 0;
}

AVStream *addStream(AVFormatContext *context, obs_encoder_t *encoder)
{
	const AVCodecDescriptor *codec = avcodec_descriptor_get_by_name(obs_encoder_get_codec(encoder));
	if (!codec) {
		return nullptr;
	}
	AVStream *stream = avformat_new_stream(context, nullptr);
	if (!stream) {
		return nullptr;
	}

	AVCodecParameters *par = stream->codecpar;
	par->codec_type = codec->type;
	par->codec_id = codec->id;
	if (codec->type == AVMEDIA_TYPE_VIDEO) {
		const video_output_info *info = video_output_get_info(obs_encoder_video(encoder));
		par->width = (int)obs_encoder_get_width(encoder);
		par->height = (int)obs_encoder_get_height(encoder);
		stream->time_base = AVRational{(int)info->fps_den, (int)info->fps_num};
	} else {
		par->sample_rate = (int)obs_encoder_get_sample_rate(encoder);
		par->frame_size = (int)obs_encoder_get_frame_size(encoder);
		av_channel_layout_default(&par->ch_layout, (int)audio_output_get_channels(obs_encoder_audio(encoder)));
		stream->time_base = AVRational{1, par->sample_rate};
	}

	/* the H.264/HEVC headers and AAC's config; FFmpeg repeats them at keyframes in MPEG-TS */
	uint8_t *extra = nullptr;
	size_t size = 0;
	if (obs_encoder_get_extra_data(encoder, &extra, &size) && size > 0) {
		par->extradata = static_cast<uint8_t *>(av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE));
		std::memcpy(par->extradata, extra, size);
		par->extradata_size = (int)size;
	}
	return stream;
}

/* Sends queued packets until a write fails or the output stops. */
void sendUntilFailure(SrtOutput *s, AVFormatContext *context, AVStream *video, AVStream *audio)
{
	AVPacket *packet = av_packet_alloc();
	while (!s->stopping) {
		Packet next;
		{
			std::unique_lock lock(s->mutex);
			s->wake.wait(lock, [s] { return s->stopping || !s->queue.empty(); });
			if (s->stopping) {
				break;
			}
			next = std::move(s->queue.front());
			s->queue.pop_front();
		}

		AVStream *stream = next.video ? video : audio;
		if (!stream) {
			continue;
		}
		av_packet_unref(packet);
		if (av_new_packet(packet, (int)next.data.size()) < 0) {
			break;
		}
		std::memcpy(packet->data, next.data.data(), next.data.size());
		packet->stream_index = stream->index;
		packet->pts = av_rescale_q(next.pts, next.timebase, stream->time_base);
		packet->dts = av_rescale_q(next.dts, next.timebase, stream->time_base);
		packet->flags = next.keyframe ? AV_PKT_FLAG_KEY : 0;

		const int written = av_interleaved_write_frame(context, packet);
		if (written < 0) {
			blog(LOG_WARNING, "[bread-srt] '%s': sending stopped: %s", obs_output_get_name(s->output),
			     errorText(written).c_str());
			break;
		}
		s->bytes += next.data.size();
	}
	av_packet_free(&packet);
}

/* Connects, sends, and on a failure connects again, until the output stops. */
void run(SrtOutput *s)
{
	obs_encoder_t *videoEncoder = obs_output_get_video_encoder(s->output);
	obs_encoder_t *audioEncoder = obs_output_get_audio_encoder(s->output, 0);

	while (!s->stopping) {
		s->state = StateWaiting;

		AVFormatContext *context = nullptr;
		int result = avformat_alloc_output_context2(&context, nullptr, "mpegts", nullptr);
		AVStream *video = result >= 0 ? addStream(context, videoEncoder) : nullptr;
		AVStream *audio = result >= 0 && audioEncoder ? addStream(context, audioEncoder) : nullptr;

		if (video) {
			AVDictionary *options = nullptr;
			for (const auto &[key, value] : s->options) {
				av_dict_set(&options, key.c_str(), value.c_str(), 0);
			}
			AVIOInterruptCB interrupt = {interrupted, s};
			/* a listener waits here for its caller; stopping ends the wait */
			result = avio_open2(&context->pb, s->url.c_str(), AVIO_FLAG_WRITE, &interrupt, &options);
			av_dict_free(&options);
			if (result >= 0) {
				result = avformat_write_header(context, nullptr);
			}

			if (result >= 0) {
				{
					std::lock_guard lock(s->mutex);
					s->queue.clear();
					s->connected = true;
					s->waitKey = true;
				}
				s->state = StateConnected;
				blog(LOG_INFO, "[bread-srt] '%s': connected to %s", obs_output_get_name(s->output),
				     s->url.c_str());
				sendUntilFailure(s, context, video, audio);
				{
					std::lock_guard lock(s->mutex);
					s->connected = false;
					s->queue.clear();
				}
			} else if (!s->stopping) {
				blog(LOG_WARNING, "[bread-srt] '%s': cannot open %s: %s", obs_output_get_name(s->output),
				     s->url.c_str(), errorText(result).c_str());
			}
		}

		if (context) {
			if (context->pb) {
				avio_closep(&context->pb);
			}
			avformat_free_context(context);
		}
		if (s->stopping) {
			break;
		}

		/* a caller tries again after two seconds; a listener listens again almost at once */
		s->state = StateRetrying;
		std::unique_lock lock(s->mutex);
		s->wake.wait_for(lock, s->listener ? std::chrono::milliseconds(200) : std::chrono::seconds(2),
				 [s] { return s->stopping.load(); });
	}
	s->state = StateOff;
}

void getState(void *data, calldata_t *call)
{
	calldata_set_int(call, "state", static_cast<SrtOutput *>(data)->state.load());
}

const char *srtName(void *)
{
	return obs_module_text("SrtOutput");
}

void *srtCreate(obs_data_t *, obs_output_t *output)
{
	auto *s = new SrtOutput();
	s->output = output;
	proc_handler_add(obs_output_get_proc_handler(output), "void get_state(out int state)", getState, s);
	return s;
}

void stopWorker(SrtOutput *s)
{
	s->stopping = true;
	s->wake.notify_all();
	if (s->worker.joinable()) {
		s->worker.join();
	}
}

void srtDestroy(void *data)
{
	auto *s = static_cast<SrtOutput *>(data);
	stopWorker(s);
	delete s;
}

bool srtStart(void *data)
{
	auto *s = static_cast<SrtOutput *>(data);

	obs_data_t *settings = obs_output_get_settings(s->output);
	s->url = obs_data_get_string(settings, "url");
	s->options.clear();
	obs_data_t *options = obs_data_get_obj(settings, "options");
	for (obs_data_item_t *item = obs_data_first(options); item; obs_data_item_next(&item)) {
		s->options.emplace_back(obs_data_item_get_name(item), obs_data_item_get_string(item));
	}
	obs_data_release(options);
	obs_data_release(settings);

	s->listener = false;
	for (const auto &[key, value] : s->options) {
		if (key == "mode" && value == "listener") {
			s->listener = true;
		}
	}

	if (s->url.empty() || !obs_output_can_begin_data_capture(s->output, 0) ||
	    !obs_output_initialize_encoders(s->output, 0)) {
		return false;
	}

	s->stopping = false;
	s->worker = std::thread(run, s);
	return obs_output_begin_data_capture(s->output, 0);
}

void srtStop(void *data, uint64_t)
{
	auto *s = static_cast<SrtOutput *>(data);
	stopWorker(s);
	obs_output_end_data_capture(s->output);
}

/* OBS's encoder threads: never wait on the network here */
void srtPacket(void *data, encoder_packet *packet)
{
	auto *s = static_cast<SrtOutput *>(data);
	if (!packet) {
		obs_output_signal_stop(s->output, OBS_OUTPUT_ENCODE_ERROR);
		return;
	}

	const bool video = packet->type == OBS_ENCODER_VIDEO;
	std::lock_guard lock(s->mutex);
	if (!s->connected) {
		return;
	}
	if (s->waitKey) {
		if (!video || !packet->keyframe) {
			return;
		}
		s->waitKey = false;
	}
	if (s->queue.size() >= MaxQueued) {
		blog(LOG_WARNING, "[bread-srt] '%s': the network fell behind; starting again at the next keyframe",
		     obs_output_get_name(s->output));
		s->queue.clear();
		s->waitKey = true;
		return;
	}

	Packet copy;
	copy.data.assign(packet->data, packet->data + packet->size);
	copy.pts = packet->pts;
	copy.dts = packet->dts;
	copy.timebase = AVRational{packet->timebase_num, packet->timebase_den};
	copy.video = video;
	copy.keyframe = packet->keyframe;
	s->queue.push_back(std::move(copy));
	s->wake.notify_one();
}

uint64_t srtBytes(void *data)
{
	return static_cast<SrtOutput *>(data)->bytes.load();
}

} // namespace

bool obs_module_load(void)
{
	obs_output_info info = {};
	info.id = "bread_srt_output";
	info.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED;
	info.encoded_video_codecs = "h264;hevc";
	info.encoded_audio_codecs = "aac";
	info.get_name = srtName;
	info.create = srtCreate;
	info.destroy = srtDestroy;
	info.start = srtStart;
	info.stop = srtStop;
	info.encoded_packet = srtPacket;
	info.get_total_bytes = srtBytes;
	obs_register_output(&info);
	return true;
}
```

Create `breadstudio/srt/data/locale/en-US.ini`:
```ini
SrtOutput="SRT (BreadStudio)"
```

Create `breadstudio/srt/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.28...3.30)

# OBS's own FFmpeg, as the OMT plugin uses it: one FFmpeg in the process
find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil)

add_library(bread-srt MODULE)
add_library(OBS::bread-srt ALIAS bread-srt)

target_sources(bread-srt PRIVATE srt-output.cpp)
target_link_libraries(bread-srt PRIVATE OBS::libobs FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil)

set_target_properties_obs(bread-srt PROPERTIES FOLDER plugins/bread-srt PREFIX "")
```

In `plugins/CMakeLists.txt`, under the line that adds `breadstudio/omt-hx`, add:
```cmake
add_subdirectory("${CMAKE_SOURCE_DIR}/breadstudio/srt" "${CMAKE_BINARY_DIR}/breadstudio/srt")
```
In `breadstudio/UPSTREAM-EDITS.md`, replace the `plugins/CMakeLists.txt` line with one that names every `breadstudio/` folder it adds, in the file's order. On `main` today:
```
- `plugins/CMakeLists.txt` - three lines that add breadstudio/desk, breadstudio/omt-hx and breadstudio/srt to the build
```
(If Plan C has landed, the line also names `breadstudio/ndi`, and says "four lines".)

- [ ] **Step 2: Build**

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-srt
Get-ChildItem build_x64\rundir\RelWithDebInfo\obs-plugins\64bit\bread-srt.dll
```
Expected: builds with no warning; the file exists. The output is driven by the engine in Task 4; its hand-check is there and in Task 5.

- [ ] **Step 3: Commit**

```powershell
git add plugins/CMakeLists.txt breadstudio/srt breadstudio/UPSTREAM-EDITS.md
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add an SRT output on FFmpeg's own srt protocol, with its settings as options"
```

---

### Task 3: The output plan

**Files:**
- Create: `breadstudio/desk/OutputPlan.hpp`, `breadstudio/desk/OutputPlan.cpp`, `breadstudio/desk/test/OutputPlanTest.cpp`
- Modify: `breadstudio/desk/CMakeLists.txt`

**Interfaces:**
- Consumes: `StreamForm` and its `operator==` (Plan A).
- Produces, in namespace `desk`:
  - `inline constexpr int OutputCount = 4;`, `inline constexpr int DestinationCount = 3;`
  - `enum class OutputSource { Program, Preview, MultiView, Input };`, `enum class OmtMode { Off, Vmx, Hevc, H264 };`, `enum class VideoCodec { H264, Hevc };`
  - `struct OutputConfig`, `struct Destination`, `struct OutputsConfig` (below), with `operator==` for the first two
  - `struct OutputRun` and `OutputRun outputRun(int index, const OutputsConfig &config, bool streaming, int programWidth, int programHeight);`
  - `struct OutputSteps` and `OutputSteps outputSteps(int index, const OutputsConfig &now, bool nowStreaming, const OutputsConfig &next, bool nextStreaming, int programWidth, int programHeight);`

- [ ] **Step 1: Write the header**

Create `breadstudio/desk/OutputPlan.hpp`:
```cpp
#pragma once

#include "StreamAddress.hpp"

#include <array>
#include <string>

namespace desk {

inline constexpr int OutputCount = 4;
inline constexpr int DestinationCount = 3;

/* what an output shows (outputs spec §3.1); output 1 always shows Program */
enum class OutputSource { Program, Preview, MultiView, Input };
enum class OmtMode { Off, Vmx, Hevc, H264 };
enum class VideoCodec { H264, Hevc };

struct OutputConfig {
	OutputSource source = OutputSource::Program;
	std::string input; /* the input's uuid, when the source is Input */
	int width = 0;     /* 0 and 0: the size of Program */
	int height = 0;
	bool alpha = false;
	int bus = 0; /* OBS's audio track: 0 is the desk's bus M, 1 to 5 are A to E */
	bool ndi = false;
	OmtMode omt = OmtMode::Off;
	bool srt = false;
	StreamForm srtForm; /* its type is one of the three SRT types */
	VideoCodec codec = VideoCodec::H264;
	int bitrateKbps = 6000;
};

struct Destination {
	bool on = false;
	int output = 0; /* 0 to 3 */
	std::string server;
	std::string key;
};

struct OutputsConfig {
	std::array<OutputConfig, OutputCount> outputs;
	std::array<Destination, DestinationCount> destinations;
};

bool operator==(const OutputConfig &a, const OutputConfig &b);
bool operator==(const Destination &a, const Destination &b);

/* What must run for one output. Output 1 (index 0) is the main mix and never has a canvas. */
struct OutputRun {
	bool canvas = false;
	int width = 0; /* the size sent */
	int height = 0;
	bool alpha = false; /* a BGRA canvas */
	bool encoders = false;
	bool ndi = false;
	OmtMode omt = OmtMode::Off;
	bool srt = false;
	std::array<bool, DestinationCount> rtmp{}; /* the destinations sending this output */
};

OutputRun outputRun(int index, const OutputsConfig &config, bool streaming, int programWidth, int programHeight);

/* What a change does to one output. A true piece is stopped and, if it is still wanted, started
 * again; a false piece keeps running untouched (outputs spec §3.3, §4.2). `source` alone means only
 * what the output shows changes, and nothing restarts. */
struct OutputSteps {
	bool canvas = false;
	bool encoders = false;
	bool ndi = false;
	bool omt = false;
	bool srt = false;
	std::array<bool, DestinationCount> rtmp{};
	bool source = false;
};

OutputSteps outputSteps(int index, const OutputsConfig &now, bool nowStreaming, const OutputsConfig &next,
			bool nextStreaming, int programWidth, int programHeight);

} // namespace desk
```

- [ ] **Step 2: Write the failing test**

Create `breadstudio/desk/test/OutputPlanTest.cpp`:
```cpp
#include "OutputPlan.hpp"

#include <cstdio>

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

static const int W = 1920;
static const int H = 1080;

static OutputSteps steps(int index, const OutputsConfig &now, bool nowLive, const OutputsConfig &next, bool nextLive)
{
	return outputSteps(index, now, nowLive, next, nextLive, W, H);
}

static bool nothing(const OutputSteps &s)
{
	return !s.canvas && !s.encoders && !s.ndi && !s.omt && !s.srt && !s.rtmp[0] && !s.rtmp[1] && !s.rtmp[2] &&
	       !s.source;
}

int main()
{
	/* all off: nothing runs; output 1 is Program's size */
	{
		OutputsConfig config;
		for (int i = 0; i < OutputCount; i++) {
			OutputRun run = outputRun(i, config, false, W, H);
			CHECK(!run.canvas && !run.encoders && !run.ndi && !run.srt);
		}
		CHECK(outputRun(0, config, false, W, H).width == W);
	}

	/* output 1 never has a canvas, whatever it sends */
	{
		OutputsConfig config;
		config.outputs[0].ndi = true;
		config.outputs[0].srt = true;
		OutputRun run = outputRun(0, config, false, W, H);
		CHECK(!run.canvas && run.ndi && run.encoders && run.srt);
	}

	/* output 2 with NDI: a canvas at Program's size, no encoders */
	{
		OutputsConfig config;
		config.outputs[1].ndi = true;
		OutputRun run = outputRun(1, config, false, W, H);
		CHECK(run.canvas && run.width == W && run.height == H && !run.encoders);
	}

	/* alpha only for an input */
	{
		OutputsConfig config;
		config.outputs[2].ndi = true;
		config.outputs[2].alpha = true;
		CHECK(!outputRun(2, config, false, W, H).alpha);
		config.outputs[2].source = OutputSource::Input;
		config.outputs[2].input = "uuid";
		CHECK(outputRun(2, config, false, W, H).alpha);
	}

	/* a destination on an output that sends nothing else: a canvas and encoders, only while streaming */
	{
		OutputsConfig config;
		config.destinations[1] = Destination{true, 3, "rtmp://a/live", "k"};
		CHECK(!outputRun(3, config, false, W, H).canvas);
		OutputRun run = outputRun(3, config, true, W, H);
		CHECK(run.canvas && run.encoders && run.rtmp[1] && !run.rtmp[0]);
	}

	/* starting the stream on an output that already sends SRT and NDI touches only that destination */
	{
		OutputsConfig config;
		config.outputs[1].ndi = true;
		config.outputs[1].srt = true;
		config.destinations[0] = Destination{true, 1, "rtmp://a/live", "k"};
		OutputSteps s = steps(1, config, false, config, true);
		CHECK(s.rtmp[0] && !s.canvas && !s.encoders && !s.ndi && !s.srt && !s.source);
		CHECK(nothing(steps(0, config, false, config, true)));
	}

	/* a destination's key changes while streaming: that destination only */
	{
		OutputsConfig now;
		now.outputs[0].srt = true;
		now.destinations[0] = Destination{true, 0, "rtmp://a/live", "k1"};
		now.destinations[1] = Destination{true, 0, "rtmp://b/live", "k"};
		OutputsConfig next = now;
		next.destinations[0].key = "k2";
		OutputSteps s = steps(0, now, true, next, true);
		CHECK(s.rtmp[0] && !s.rtmp[1] && !s.encoders && !s.srt);
	}

	/* a destination moves from output 1 to output 2: both outputs see it */
	{
		OutputsConfig now;
		now.destinations[0] = Destination{true, 0, "rtmp://a/live", "k"};
		OutputsConfig next = now;
		next.destinations[0].output = 1;
		CHECK(steps(0, now, true, next, true).rtmp[0]);
		CHECK(steps(1, now, true, next, true).rtmp[0]);
	}

	/* a new size restarts every sender of that output */
	{
		OutputsConfig now;
		now.outputs[2].ndi = true;
		now.outputs[2].srt = true;
		now.destinations[2] = Destination{true, 2, "rtmp://a/live", "k"};
		OutputsConfig next = now;
		next.outputs[2].width = 1280;
		next.outputs[2].height = 720;
		OutputSteps s = steps(2, now, true, next, true);
		CHECK(s.canvas && s.encoders && s.ndi && s.srt && s.rtmp[2]);
	}

	/* a new bitrate restarts what uses the encoders, not NDI */
	{
		OutputsConfig now;
		now.outputs[1].ndi = true;
		now.outputs[1].srt = true;
		OutputsConfig next = now;
		next.outputs[1].bitrateKbps = 8000;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(s.encoders && s.srt && !s.ndi && !s.canvas);
	}

	/* a new bus restarts what carries the sound */
	{
		OutputsConfig now;
		now.outputs[1].ndi = true;
		OutputsConfig next = now;
		next.outputs[1].bus = 3;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(s.ndi && !s.canvas);
	}

	/* a new SRT setting restarts SRT only */
	{
		OutputsConfig now;
		now.outputs[1].srt = true;
		now.outputs[1].ndi = true;
		OutputsConfig next = now;
		next.outputs[1].srtForm.port = 6000;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(s.srt && !s.encoders && !s.ndi);
	}

	/* a new source changes only what is shown */
	{
		OutputsConfig now;
		now.outputs[1].ndi = true;
		now.outputs[1].srt = true;
		OutputsConfig next = now;
		next.outputs[1].source = OutputSource::Preview;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(s.source && !s.canvas && !s.encoders && !s.ndi && !s.srt);
		next = now;
		next.outputs[1].source = OutputSource::Input;
		next.outputs[1].input = "a";
		OutputsConfig other = next;
		other.outputs[1].input = "b";
		CHECK(steps(1, next, false, other, false).source);
	}

	/* alpha on an output that shows Program changes nothing */
	{
		OutputsConfig now;
		now.outputs[1].ndi = true;
		OutputsConfig next = now;
		next.outputs[1].alpha = true;
		OutputSteps s = steps(1, now, false, next, false);
		CHECK(!s.canvas && !s.ndi);
	}

	/* the same settings: nothing */
	{
		OutputsConfig config;
		config.outputs[1].ndi = true;
		config.outputs[1].omt = OmtMode::Hevc;
		config.destinations[0] = Destination{true, 1, "rtmp://a/live", "k"};
		for (int i = 0; i < OutputCount; i++) {
			CHECK(nothing(steps(i, config, true, config, true)));
		}
	}

	if (failures == 0) {
		std::printf("OutputPlan: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
```

Create `breadstudio/desk/OutputPlan.cpp` with stubs:
```cpp
#include "OutputPlan.hpp"

namespace desk {

bool operator==(const OutputConfig &, const OutputConfig &)
{
	return false;
}

bool operator==(const Destination &, const Destination &)
{
	return false;
}

OutputRun outputRun(int, const OutputsConfig &, bool, int, int)
{
	return {};
}

OutputSteps outputSteps(int, const OutputsConfig &, bool, const OutputsConfig &, bool, int, int)
{
	return {};
}

} // namespace desk
```

At the end of `breadstudio/desk/CMakeLists.txt`, add:
```cmake

# What the outputs must run, and what a change restarts: no OBS, no Qt.
add_executable(bread-outputs-test test/OutputPlanTest.cpp OutputPlan.cpp StreamAddress.cpp)
target_include_directories(bread-outputs-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(
  bread-outputs-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/desk/test-bin" FOLDER plugins/bread-desk
)
```

- [ ] **Step 3: Run it and see it fail**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-outputs-test
& build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-outputs-test.exe; $LASTEXITCODE
```
Expected: `FAIL line …` lines and `1`.

- [ ] **Step 4: Implement it**

Replace `breadstudio/desk/OutputPlan.cpp` with:
```cpp
#include "OutputPlan.hpp"

namespace desk {

bool operator==(const OutputConfig &a, const OutputConfig &b)
{
	return a.source == b.source && a.input == b.input && a.width == b.width && a.height == b.height &&
	       a.alpha == b.alpha && a.bus == b.bus && a.ndi == b.ndi && a.omt == b.omt && a.srt == b.srt &&
	       a.srtForm == b.srtForm && a.codec == b.codec && a.bitrateKbps == b.bitrateKbps;
}

bool operator==(const Destination &a, const Destination &b)
{
	return a.on == b.on && a.output == b.output && a.server == b.server && a.key == b.key;
}

OutputRun outputRun(int index, const OutputsConfig &config, bool streaming, int programWidth, int programHeight)
{
	const OutputConfig &output = config.outputs[(size_t)index];
	OutputRun run;

	bool rtmp = false;
	for (size_t d = 0; d < DestinationCount; d++) {
		const Destination &destination = config.destinations[d];
		run.rtmp[d] = streaming && destination.on && destination.output == index;
		rtmp = rtmp || run.rtmp[d];
	}

	run.ndi = output.ndi;
	run.omt = output.omt;
	run.srt = output.srt;
	run.encoders = output.srt || rtmp;

	run.width = programWidth;
	run.height = programHeight;
	if (index > 0) {
		run.canvas = run.encoders || run.ndi || run.omt != OmtMode::Off;
		if (output.width > 0 && output.height > 0) {
			run.width = output.width;
			run.height = output.height;
		}
		run.alpha = output.alpha && output.source == OutputSource::Input;
	}
	return run;
}

OutputSteps outputSteps(int index, const OutputsConfig &now, bool nowStreaming, const OutputsConfig &next,
			bool nextStreaming, int programWidth, int programHeight)
{
	const OutputRun a = outputRun(index, now, nowStreaming, programWidth, programHeight);
	const OutputRun b = outputRun(index, next, nextStreaming, programWidth, programHeight);
	const OutputConfig &x = now.outputs[(size_t)index];
	const OutputConfig &y = next.outputs[(size_t)index];

	OutputSteps steps;
	steps.canvas = a.canvas != b.canvas ||
		       (b.canvas && (a.width != b.width || a.height != b.height || a.alpha != b.alpha));
	steps.encoders = steps.canvas || a.encoders != b.encoders ||
			 (b.encoders && (x.codec != y.codec || x.bitrateKbps != y.bitrateKbps || x.bus != y.bus));
	steps.ndi = steps.canvas || a.ndi != b.ndi || (b.ndi && x.bus != y.bus);
	steps.omt = steps.canvas || a.omt != b.omt || (b.omt != OmtMode::Off && x.bus != y.bus);
	steps.srt = steps.encoders || a.srt != b.srt || (b.srt && !(x.srtForm == y.srtForm));
	for (size_t d = 0; d < DestinationCount; d++) {
		steps.rtmp[d] = a.rtmp[d] != b.rtmp[d] ||
				(b.rtmp[d] && (steps.encoders || !(now.destinations[d] == next.destinations[d])));
	}
	steps.source = x.source != y.source || x.input != y.input;
	return steps;
}

} // namespace desk
```

- [ ] **Step 5: Run it and see it pass**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-outputs-test
& build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-outputs-test.exe; $LASTEXITCODE
```
Expected: `OutputPlan: all checks passed` and `0`.

- [ ] **Step 6: Commit**

```powershell
git add breadstudio/desk/OutputPlan.hpp breadstudio/desk/OutputPlan.cpp breadstudio/desk/test/OutputPlanTest.cpp breadstudio/desk/CMakeLists.txt
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: work out what the outputs run, and what a change restarts"
```

---

### Task 4: The engine: canvases, encoders, SRT, saving

**Files:**
- Create: `breadstudio/desk/ProgramSource.hpp`, `breadstudio/desk/ProgramSource.cpp`, `breadstudio/desk/OutputEngine.hpp`, `breadstudio/desk/OutputEngine.cpp`
- Modify: `breadstudio/desk/desk.cpp`, `breadstudio/desk/CMakeLists.txt`, `breadstudio/desk/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `OutputsConfig`, `outputRun`, `outputSteps` (Task 3); `srtOutputTarget`, `streamTypeId`, `streamTypeFromId` (Task 1, Plan A); `bread_srt_output` and its `get_state` (Task 2).
- Produces:
  - `inline constexpr const char *ProgramSourceId = "bread_program";` and `void registerProgramSource();`
  - `enum class SenderState { Off, Waiting, Connected, Retrying, Live, Failed };`
  - `struct SenderStatus { SenderState state; int receivers; uint64_t bytes; int dropped; };`
  - `struct OutputStatus { int width; int height; uint32_t skipped; std::string encoder; SenderStatus ndi, omt, srt; };`
  - `class OutputEngine` with `static OutputEngine &instance()`, `const OutputsConfig &config() const`, `void apply(const OutputsConfig &)`, `bool streaming() const`, `void setStreaming(bool)`, `void refreshSources()`, `void detachSources()`, `void shutdown()`, `void save(obs_data_t *) const`, `OutputsConfig load(obs_data_t *) const`, `OutputStatus status(int output) const`, `SenderStatus destinationStatus(int destination) const`, `static bool hasOutputType(const char *id)`
  - In the scene collection: an object `bread_outputs` (§ "What is saved" in this task's Step 3).

- [ ] **Step 1: The Program picture source**

Create `breadstudio/desk/ProgramSource.hpp`:
```cpp
#pragma once

namespace desk {

inline constexpr const char *ProgramSourceId = "bread_program";

/* bread_program: the main mix's finished picture, for an output set to Program (outputs spec §4.1).
 * Hidden from every list of sources. */
void registerProgramSource();

} // namespace desk
```

Create `breadstudio/desk/ProgramSource.cpp`:
```cpp
#include "ProgramSource.hpp"

#include <obs-module.h>

namespace desk {

static const char *programName(void *)
{
	return obs_module_text("ProgramPicture");
}

static void *programCreate(obs_data_t *, obs_source_t *source)
{
	return source;
}

static void programDestroy(void *) {}

static uint32_t programWidth(void *)
{
	obs_video_info ovi = {};
	return obs_get_video_info(&ovi) ? ovi.base_width : 0;
}

static uint32_t programHeight(void *)
{
	obs_video_info ovi = {};
	return obs_get_video_info(&ovi) ? ovi.base_height : 0;
}

/* OBS draws the main mix before any other mix in every frame, so this is the frame Program shows,
 * during a fade too, and the transition is drawn only once (outputs spec §4.1) */
static void programRender(void *, gs_effect_t *)
{
	obs_render_main_texture();
}

void registerProgramSource()
{
	obs_source_info info = {};
	info.id = ProgramSourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_CAP_DISABLED;
	info.get_name = programName;
	info.create = programCreate;
	info.destroy = programDestroy;
	info.get_width = programWidth;
	info.get_height = programHeight;
	info.video_render = programRender;
	obs_register_source(&info);
}

} // namespace desk
```

- [ ] **Step 2: The engine's header**

Create `breadstudio/desk/OutputEngine.hpp`:
```cpp
#pragma once

#include "OutputPlan.hpp"

#include <obs.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace desk {

/* what the Outputs and Stream windows show for one sender (outputs spec §3.3) */
enum class SenderState { Off, Waiting, Connected, Retrying, Live, Failed };

struct SenderStatus {
	SenderState state = SenderState::Off;
	int receivers = -1; /* NDI and OMT; -1 when not known */
	uint64_t bytes = 0; /* sent so far */
	int dropped = 0;    /* frames dropped for the network (RTMP) */
};

struct OutputStatus {
	int width = 0; /* the picture sent; 0 while the output sends nothing */
	int height = 0;
	uint32_t skipped = 0; /* frames the PC fell behind on */
	std::string encoder;  /* the video encoder's name; empty while none runs */
	SenderStatus ndi;
	SenderStatus omt;
	SenderStatus srt;
};

/* The four outputs and the three RTMP destinations, running (outputs spec §4). UI thread only. */
class OutputEngine {
public:
	static OutputEngine &instance();

	const OutputsConfig &config() const { return wanted; }

	/* starts, stops and restarts only what outputSteps() says */
	void apply(const OutputsConfig &next);

	bool streaming() const { return live; }
	void setStreaming(bool on);

	/* what each output shows again: Preview moved, Studio Mode changed, an input went or came */
	void refreshSources();

	/* every output black, its senders still running: the scene collection is about to go */
	void detachSources();

	/* everything stopped and let go, the settings kept: OBS is closing */
	void shutdown();

	void save(obs_data_t *data) const;
	OutputsConfig load(obs_data_t *data) const;

	OutputStatus status(int output) const;
	SenderStatus destinationStatus(int destination) const;

	/* whether a plugin has registered this output type */
	static bool hasOutputType(const char *id);

private:
	void reconcile(const OutputsConfig &next, bool nextLive);
	video_t *videoOf(int output) const;
	OBSSource sourceFor(int output) const;
	void show(int output);
	void startSenders(int output);
	void startDestination(int destination);
	void stopDestination(int destination);

	struct Running {
		OBSCanvasAutoRelease canvas;
		OBSSource shown;
		OBSEncoderAutoRelease video;
		OBSEncoderAutoRelease audio;
		OBSOutputAutoRelease ndi;
		OBSOutputAutoRelease omt;
		OBSOutputAutoRelease srt;
	};

	OutputsConfig wanted;
	bool live = false;
	bool detached = false;
	std::array<Running, OutputCount> running;
	std::array<OBSServiceAutoRelease, DestinationCount> services;
	std::array<OBSOutputAutoRelease, DestinationCount> rtmp;
	OBSSourceAutoRelease program; /* one bread_program for every output that shows Program */
};

} // namespace desk
```

- [ ] **Step 3: The engine**

Create `breadstudio/desk/OutputEngine.cpp`:
```cpp
#include "OutputEngine.hpp"
#include "ProgramSource.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <media-io/video-io.h>

#include <atomic>

namespace desk {

static const char *SrtType = "bread_srt_output";
static const char *NdiType = "bread_ndi_output";
static const char *OmtType = "bread_omt_output";
static const char *RtmpType = "rtmp_output";

/* how each destination last stopped: OBS_OUTPUT_SUCCESS, or the failure; written by OBS's threads */
static std::array<std::atomic<int>, DestinationCount> lastStop;

static void onRtmpStop(void *param, calldata_t *call)
{
	lastStop[(size_t)(uintptr_t)param] = (int)calldata_int(call, "code");
}

static std::string senderName(int output)
{
	return "BreadStudio Output " + std::to_string(output + 1);
}

/* the best video encoder this PC has; HEVC falls back to H.264 (outputs spec §4.3) */
static const char *videoEncoderId(VideoCodec codec)
{
	static const char *hevc[] = {"h265_texture_amf", "obs_nvenc_hevc_tex", "obs_qsv11_hevc"};
	static const char *h264[] = {"h264_texture_amf", "obs_nvenc_h264_tex", "obs_qsv11_v2", "obs_x264"};
	if (codec == VideoCodec::Hevc) {
		for (const char *id : hevc) {
			if (obs_get_encoder_codec(id)) {
				return id;
			}
		}
	}
	for (const char *id : h264) {
		if (obs_get_encoder_codec(id)) {
			return id;
		}
	}
	return nullptr;
}

static int procInt(obs_output_t *output, const char *proc, const char *name, int fallback)
{
	calldata_t call;
	calldata_init(&call);
	int value = fallback;
	if (proc_handler_call(obs_output_get_proc_handler(output), proc, &call)) {
		value = (int)calldata_int(&call, name);
	}
	calldata_free(&call);
	return value;
}

static void stopOutput(OBSOutputAutoRelease &output)
{
	if (output) {
		obs_output_force_stop(output);
		output = nullptr;
	}
}

OutputEngine &OutputEngine::instance()
{
	static OutputEngine engine;
	return engine;
}

bool OutputEngine::hasOutputType(const char *id)
{
	return obs_output_get_display_name(id) != nullptr;
}

void OutputEngine::apply(const OutputsConfig &next)
{
	reconcile(next, live);
}

void OutputEngine::setStreaming(bool on)
{
	reconcile(wanted, on);
}

video_t *OutputEngine::videoOf(int output) const
{
	if (output == 0) {
		return obs_get_video();
	}
	const Running &r = running[(size_t)output];
	return r.canvas ? obs_canvas_get_video(r.canvas) : nullptr;
}

OBSSource OutputEngine::sourceFor(int output) const
{
	if (detached) {
		return OBSSource();
	}
	const OutputConfig &config = wanted.outputs[(size_t)output];
	switch (config.source) {
	case OutputSource::Program:
		return OBSSource(program.Get());
	case OutputSource::Preview:
		if (obs_frontend_preview_program_mode_active()) {
			OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
			return OBSSource(preview.Get());
		}
		return OBSSource(program.Get()); /* with Studio Mode off, Preview is Program */
	case OutputSource::MultiView:
		return OBSSource(); /* Plan E fills this in */
	case OutputSource::Input: {
		OBSSourceAutoRelease input = obs_get_source_by_uuid(config.input.c_str());
		if (input && !obs_source_removed(input)) {
			return OBSSource(input.Get());
		}
		return OBSSource(); /* deleted: black, the senders go on (outputs spec §7) */
	}
	}
	return OBSSource();
}

void OutputEngine::show(int output)
{
	Running &r = running[(size_t)output];
	if (!r.canvas) {
		r.shown = nullptr;
		return;
	}
	OBSSource next = sourceFor(output);
	if (next.Get() != r.shown.Get()) {
		obs_canvas_set_channel(r.canvas, 0, next);
		r.shown = next;
	}
}

void OutputEngine::refreshSources()
{
	detached = false;
	for (int i = 0; i < OutputCount; i++) {
		show(i);
	}
}

void OutputEngine::detachSources()
{
	detached = true;
	for (int i = 0; i < OutputCount; i++) {
		show(i);
	}
}

void OutputEngine::startSenders(int output)
{
	Running &r = running[(size_t)output];
	const OutputConfig &config = wanted.outputs[(size_t)output];
	const OutputRun run = outputRun(output, wanted, live, 0, 0);
	video_t *video = videoOf(output);
	if (!video) {
		return;
	}
	const std::string name = senderName(output);

	if (run.encoders && !r.video) {
		const char *id = videoEncoderId(config.codec);
		if (id) {
			OBSDataAutoRelease settings = obs_data_create();
			obs_data_set_string(settings, "rate_control", "CBR");
			obs_data_set_int(settings, "bitrate", config.bitrateKbps);
			obs_data_set_int(settings, "keyint_sec", 2);
			r.video = obs_video_encoder_create(id, (name + " video").c_str(), settings, nullptr);
			if (r.video) {
				obs_encoder_set_video(r.video, video);
			}
		} else {
			blog(LOG_WARNING, "[bread-desk] %s: no video encoder on this PC", name.c_str());
		}
		OBSDataAutoRelease audioSettings = obs_data_create();
		obs_data_set_int(audioSettings, "bitrate", 160);
		r.audio = obs_audio_encoder_create("ffmpeg_aac", (name + " audio").c_str(), audioSettings,
						   (size_t)config.bus, nullptr);
		if (r.audio) {
			obs_encoder_set_audio(r.audio, obs_get_audio());
		}
	}

	if (run.srt && !r.srt && r.video && r.audio && hasOutputType(SrtType)) {
		const SrtOutputTarget target = srtOutputTarget(config.srtForm);
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "url", target.url.c_str());
		OBSDataAutoRelease options = obs_data_create();
		for (const auto &[key, value] : target.options) {
			obs_data_set_string(options, key.c_str(), value.c_str());
		}
		obs_data_set_obj(settings, "options", options);
		r.srt = obs_output_create(SrtType, (name + " SRT").c_str(), settings, nullptr);
		if (r.srt) {
			obs_output_set_video_encoder(r.srt, r.video);
			obs_output_set_audio_encoder(r.srt, r.audio, 0);
			obs_output_start(r.srt);
		}
	}

	/* raw senders: Plan F's NDI and OMT outputs */
	auto startRaw = [&](OBSOutputAutoRelease &slot, const char *type, const char *mode) {
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "name", name.c_str());
		if (mode) {
			obs_data_set_string(settings, "mode", mode);
		}
		slot = obs_output_create(type, (name + " " + type).c_str(), settings, nullptr);
		if (slot) {
			obs_output_set_media(slot, video, obs_get_audio());
			obs_output_set_mixer(slot, (size_t)config.bus);
			obs_output_start(slot);
		}
	};
	if (run.ndi && !r.ndi && hasOutputType(NdiType)) {
		startRaw(r.ndi, NdiType, nullptr);
	}
	if (run.omt != OmtMode::Off && !r.omt && hasOutputType(OmtType)) {
		static const char *modes[] = {"", "vmx", "hevc", "h264"};
		startRaw(r.omt, OmtType, modes[(int)run.omt]);
	}
}

void OutputEngine::startDestination(int destination)
{
	const Destination &d = wanted.destinations[(size_t)destination];
	Running &r = running[(size_t)d.output];
	if (rtmp[(size_t)destination] || !r.video || !r.audio) {
		return;
	}

	const std::string name = "BreadStudio Stream " + std::to_string(destination + 1);
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "server", d.server.c_str());
	obs_data_set_string(settings, "key", d.key.c_str());
	services[(size_t)destination] = obs_service_create_private("rtmp_custom", name.c_str(), settings);

	OBSOutputAutoRelease output = obs_output_create(RtmpType, name.c_str(), nullptr, nullptr);
	if (!output || !services[(size_t)destination]) {
		return;
	}
	obs_output_set_service(output, services[(size_t)destination]);
	obs_output_set_video_encoder(output, r.video);
	obs_output_set_audio_encoder(output, r.audio, 0);
	/* as OBS: try again every two seconds; enough tries for a long outage */
	obs_output_set_reconnect_settings(output, 1000, 2);
	signal_handler_connect(obs_output_get_signal_handler(output), "stop", onRtmpStop,
			       (void *)(uintptr_t)destination);
	lastStop[(size_t)destination] = OBS_OUTPUT_SUCCESS;
	obs_output_start(output);
	rtmp[(size_t)destination] = std::move(output);
}

void OutputEngine::stopDestination(int destination)
{
	OBSOutputAutoRelease &output = rtmp[(size_t)destination];
	if (output) {
		signal_handler_disconnect(obs_output_get_signal_handler(output), "stop", onRtmpStop,
					  (void *)(uintptr_t)destination);
	}
	stopOutput(output);
	services[(size_t)destination] = nullptr;
}

void OutputEngine::reconcile(const OutputsConfig &next, bool nextLive)
{
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	const int programWidth = (int)ovi.output_width;
	const int programHeight = (int)ovi.output_height;

	std::array<OutputSteps, OutputCount> steps;
	for (int i = 0; i < OutputCount; i++) {
		steps[(size_t)i] = outputSteps(i, wanted, live, next, nextLive, programWidth, programHeight);
	}

	/* stop from the outside in: destinations, senders, encoders, canvases */
	for (int d = 0; d < DestinationCount; d++) {
		bool restart = false;
		for (const OutputSteps &s : steps) {
			restart = restart || s.rtmp[(size_t)d];
		}
		if (restart) {
			stopDestination(d);
		}
	}
	for (int i = 0; i < OutputCount; i++) {
		Running &r = running[(size_t)i];
		const OutputSteps &s = steps[(size_t)i];
		if (s.ndi) {
			stopOutput(r.ndi);
		}
		if (s.omt) {
			stopOutput(r.omt);
		}
		if (s.srt) {
			stopOutput(r.srt);
		}
		if (s.encoders) {
			r.video = nullptr;
			r.audio = nullptr;
		}
		if (s.canvas && r.canvas) {
			obs_canvas_set_channel(r.canvas, 0, nullptr);
			obs_canvas_remove(r.canvas);
			r.canvas = nullptr;
			r.shown = nullptr;
		}
	}

	wanted = next;
	live = nextLive;

	if (!program) {
		program = obs_source_create_private(ProgramSourceId, "BreadStudio Program", nullptr);
	}

	/* start from the inside out */
	for (int i = 0; i < OutputCount; i++) {
		Running &r = running[(size_t)i];
		const OutputRun run = outputRun(i, wanted, live, programWidth, programHeight);
		if (run.canvas && !r.canvas) {
			obs_video_info canvasInfo = ovi;
			/* inputs are laid out for the main mix's base size; the output size scales it */
			canvasInfo.output_width = (uint32_t)run.width;
			canvasInfo.output_height = (uint32_t)run.height;
			canvasInfo.output_format = run.alpha ? VIDEO_FORMAT_BGRA : VIDEO_FORMAT_NV12;
			const std::string name = senderName(i);
			r.canvas = obs_canvas_create_private(name.c_str(), &canvasInfo, ACTIVATE | EPHEMERAL);
			if (!r.canvas) {
				blog(LOG_WARNING, "[bread-desk] %s: OBS could not make its canvas", name.c_str());
			}
		}
		show(i);
		startSenders(i);
	}
	for (int d = 0; d < DestinationCount; d++) {
		const Destination &destination = wanted.destinations[(size_t)d];
		if (live && destination.on &&
		    outputRun(destination.output, wanted, live, programWidth, programHeight).rtmp[(size_t)d]) {
			startDestination(d);
		}
	}
}

void OutputEngine::shutdown()
{
	for (int d = 0; d < DestinationCount; d++) {
		stopDestination(d);
	}
	for (Running &r : running) {
		stopOutput(r.ndi);
		stopOutput(r.omt);
		stopOutput(r.srt);
		r.video = nullptr;
		r.audio = nullptr;
		if (r.canvas) {
			obs_canvas_set_channel(r.canvas, 0, nullptr);
			obs_canvas_remove(r.canvas);
			r.canvas = nullptr;
		}
		r.shown = nullptr;
	}
	program = nullptr;
	live = false;
}

static SenderStatus srtStatus(obs_output_t *output)
{
	SenderStatus status;
	if (!output) {
		return status;
	}
	static const SenderState states[] = {SenderState::Off, SenderState::Waiting, SenderState::Connected,
					     SenderState::Retrying};
	const int state = procInt(output, "get_state", "state", 0);
	status.state = state >= 0 && state <= 3 ? states[state] : SenderState::Off;
	status.bytes = obs_output_get_total_bytes(output);
	return status;
}

static SenderStatus rawStatus(obs_output_t *output)
{
	SenderStatus status;
	if (output) {
		status.state = obs_output_active(output) ? SenderState::Live : SenderState::Failed;
		status.receivers = procInt(output, "get_receivers", "count", -1);
	}
	return status;
}

OutputStatus OutputEngine::status(int output) const
{
	const Running &r = running[(size_t)output];
	OutputStatus status;
	video_t *video = videoOf(output);
	const bool sending = output == 0 ? (r.video || r.ndi || r.omt) : r.canvas.Get() != nullptr;
	if (video && sending) {
		status.width = (int)video_output_get_width(video);
		status.height = (int)video_output_get_height(video);
		status.skipped = video_output_get_skipped_frames(video);
	}
	if (r.video) {
		const char *name = obs_encoder_get_display_name(obs_encoder_get_id(r.video));
		status.encoder = name ? name : obs_encoder_get_id(r.video);
	}
	status.srt = srtStatus(r.srt);
	status.ndi = rawStatus(r.ndi);
	status.omt = rawStatus(r.omt);
	return status;
}

SenderStatus OutputEngine::destinationStatus(int destination) const
{
	SenderStatus status;
	obs_output_t *output = rtmp[(size_t)destination];
	if (!output) {
		return status;
	}
	if (obs_output_reconnecting(output)) {
		status.state = SenderState::Retrying;
	} else if (obs_output_active(output)) {
		status.state = SenderState::Live;
	} else if (lastStop[(size_t)destination] != OBS_OUTPUT_SUCCESS) {
		status.state = SenderState::Failed;
	} else {
		status.state = SenderState::Waiting;
	}
	status.bytes = obs_output_get_total_bytes(output);
	status.dropped = obs_output_get_frames_dropped(output);
	return status;
}

/* ---- What is saved, in the scene collection (outputs spec §8) ---- */

static const char *sourceNames[] = {"program", "preview", "multiview", "input"};
static const char *omtNames[] = {"off", "vmx", "hevc", "h264"};

template<typename E, size_t N> static E fromName(const char *(&names)[N], const char *name, E fallback)
{
	for (size_t i = 0; i < N; i++) {
		if (strcmp(names[i], name) == 0) {
			return (E)i;
		}
	}
	return fallback;
}

void OutputEngine::save(obs_data_t *data) const
{
	OBSDataAutoRelease saved = obs_data_create();

	OBSDataArrayAutoRelease outputs = obs_data_array_create();
	for (const OutputConfig &o : wanted.outputs) {
		OBSDataAutoRelease item = obs_data_create();
		obs_data_set_string(item, "source", sourceNames[(int)o.source]);
		obs_data_set_string(item, "input", o.input.c_str());
		obs_data_set_int(item, "width", o.width);
		obs_data_set_int(item, "height", o.height);
		obs_data_set_bool(item, "alpha", o.alpha);
		obs_data_set_int(item, "bus", o.bus);
		obs_data_set_bool(item, "ndi", o.ndi);
		obs_data_set_string(item, "omt", omtNames[(int)o.omt]);
		obs_data_set_bool(item, "srt", o.srt);
		obs_data_set_string(item, "srt_type", streamTypeId(o.srtForm.type));
		obs_data_set_string(item, "srt_host", o.srtForm.host.c_str());
		obs_data_set_int(item, "srt_port", o.srtForm.port);
		obs_data_set_int(item, "srt_latency_ms", o.srtForm.latencyMs);
		obs_data_set_string(item, "srt_passphrase", o.srtForm.passphrase.c_str());
		obs_data_set_int(item, "srt_key_length", o.srtForm.keyLength);
		obs_data_set_string(item, "srt_stream_id", o.srtForm.streamId.c_str());
		obs_data_set_string(item, "codec", o.codec == VideoCodec::Hevc ? "hevc" : "h264");
		obs_data_set_int(item, "bitrate", o.bitrateKbps);
		obs_data_array_push_back(outputs, item);
	}
	obs_data_set_array(saved, "outputs", outputs);

	OBSDataArrayAutoRelease destinations = obs_data_array_create();
	for (const Destination &d : wanted.destinations) {
		OBSDataAutoRelease item = obs_data_create();
		obs_data_set_bool(item, "on", d.on);
		obs_data_set_int(item, "output", d.output);
		obs_data_set_string(item, "server", d.server.c_str());
		obs_data_set_string(item, "key", d.key.c_str());
		obs_data_array_push_back(destinations, item);
	}
	obs_data_set_array(saved, "destinations", destinations);

	obs_data_set_obj(data, "bread_outputs", saved);
}

OutputsConfig OutputEngine::load(obs_data_t *data) const
{
	OutputsConfig config; /* a collection made before outputs existed: all off (outputs spec §8) */
	OBSDataAutoRelease saved = obs_data_get_obj(data, "bread_outputs");
	if (!saved) {
		return config;
	}

	OBSDataArrayAutoRelease outputs = obs_data_get_array(saved, "outputs");
	for (size_t i = 0; i < OutputCount && i < obs_data_array_count(outputs); i++) {
		OBSDataAutoRelease item = obs_data_array_item(outputs, i);
		OutputConfig &o = config.outputs[i];
		o.source = i == 0 ? OutputSource::Program
				  : fromName(sourceNames, obs_data_get_string(item, "source"), OutputSource::Program);
		o.input = obs_data_get_string(item, "input");
		o.width = (int)obs_data_get_int(item, "width");
		o.height = (int)obs_data_get_int(item, "height");
		o.alpha = i > 0 && obs_data_get_bool(item, "alpha");
		o.bus = std::clamp((int)obs_data_get_int(item, "bus"), 0, 5);
		o.ndi = obs_data_get_bool(item, "ndi");
		o.omt = fromName(omtNames, obs_data_get_string(item, "omt"), OmtMode::Off);
		o.srt = obs_data_get_bool(item, "srt");
		o.srtForm.type = streamTypeFromId(obs_data_get_string(item, "srt_type"));
		if (!isSrt(o.srtForm.type)) {
			o.srtForm.type = StreamType::SrtCaller;
		}
		o.srtForm.host = obs_data_get_string(item, "srt_host");
		o.srtForm.port = (int)obs_data_get_int(item, "srt_port");
		o.srtForm.latencyMs = (int)obs_data_get_int(item, "srt_latency_ms");
		o.srtForm.passphrase = obs_data_get_string(item, "srt_passphrase");
		o.srtForm.keyLength = (int)obs_data_get_int(item, "srt_key_length");
		o.srtForm.streamId = obs_data_get_string(item, "srt_stream_id");
		o.codec = strcmp(obs_data_get_string(item, "codec"), "hevc") == 0 ? VideoCodec::Hevc : VideoCodec::H264;
		o.bitrateKbps = std::max(500, (int)obs_data_get_int(item, "bitrate"));
		/* a sender whose settings cannot be used is off rather than failing */
		if (o.srt && streamProblem(o.srtForm)) {
			o.srt = false;
		}
	}

	OBSDataArrayAutoRelease destinations = obs_data_get_array(saved, "destinations");
	for (size_t i = 0; i < DestinationCount && i < obs_data_array_count(destinations); i++) {
		OBSDataAutoRelease item = obs_data_array_item(destinations, i);
		Destination &d = config.destinations[i];
		d.on = obs_data_get_bool(item, "on");
		d.output = std::clamp((int)obs_data_get_int(item, "output"), 0, OutputCount - 1);
		d.server = obs_data_get_string(item, "server");
		d.key = obs_data_get_string(item, "key");
	}
	return config;
}

} // namespace desk
```
Add `#include <algorithm>` and `#include <cstring>` to its includes.

- [ ] **Step 4: Wire it into the desk**

In `breadstudio/desk/desk.cpp`:

Add to the includes:
```cpp
#include "DeskLayout.hpp"
#include "OutputEngine.hpp"
#include "ProgramSource.hpp"
```
(`DeskLayout.hpp` is already included; add only the other two.)

Below `static Mixer *mixer = nullptr;`, add:
```cpp
/* a scene collection's outputs, read while it loads and started once its scenes exist */
static OutputsConfig pendingOutputs;
```

In `inputsChanged()`, after `mixer->rebuild();`, add:
```cpp
	OutputEngine::instance().refreshSources();
```

In `load()`, after `mixer->rebuild();`, add:
```cpp
	/* senders whose settings did not change keep running (outputs spec §7) */
	OutputEngine::instance().apply(pendingOutputs);
	OutputEngine::instance().refreshSources();
```

In `unload()`, at its start, add:
```cpp
	/* black, senders running, and no scene of the old collection held */
	OutputEngine::instance().detachSources();
```

In `onEvent`, in the `OBS_FRONTEND_EVENT_EXIT` case, before `unload();`, add:
```cpp
		OutputEngine::instance().shutdown();
```
and in the case list for `SCENE_CHANGED`, `PREVIEW_SCENE_CHANGED`, `STUDIO_MODE_ENABLED`, `STUDIO_MODE_DISABLED`, inside the `if (loaded)` block, add:
```cpp
			OutputEngine::instance().refreshSources();
```

Above `bool obs_module_load(void)`, add:
```cpp
static void saveOutputs(obs_data_t *data, bool saving, void *)
{
	if (saving) {
		OutputEngine::instance().save(data);
	} else {
		pendingOutputs = OutputEngine::instance().load(data);
	}
}
```

In `obs_module_load`, at its start, add:
```cpp
	registerProgramSource();
```
and after `obs_frontend_add_save_callback(saveNumberHotkeys, nullptr);`, add:
```cpp
	obs_frontend_add_save_callback(saveOutputs, nullptr);
```

In `breadstudio/desk/CMakeLists.txt`, add to the plugin's sources, in alphabetical places:
```cmake
    OutputEngine.cpp
    OutputEngine.hpp
    OutputPlan.cpp
    OutputPlan.hpp
    ProgramSource.cpp
    ProgramSource.hpp
```

Append to `breadstudio/desk/data/locale/en-US.ini`:
```ini
ProgramPicture="Program picture"
```

- [ ] **Step 5: Build, then check by hand by editing the saved collection**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk bread-srt
```
Expected: builds with no warning.

The window comes in Task 5. Until then, set outputs in the scene collection file: close the program; open `build_x64\rundir\RelWithDebInfo\config\obs-studio\basic\scenes\<collection>.json`; add at the top level (keep the rest):
```json
"bread_outputs": {
  "outputs": [
    {"source": "program", "srt": true, "srt_type": "srt_caller", "srt_host": "127.0.0.1", "srt_port": 9000, "srt_latency_ms": 200, "srt_passphrase": "a&b=c?d e'f\"g", "srt_key_length": 16, "codec": "h264", "bitrate": 6000},
    {"source": "program", "width": 1280, "height": 720, "srt": true, "srt_type": "srt_listener", "srt_port": 9001, "srt_latency_ms": 200, "codec": "hevc", "bitrate": 4000},
    {"source": "input", "input": "<uuid of an input>", "srt": true, "srt_type": "srt_listener", "srt_port": 9002, "srt_latency_ms": 200, "codec": "h264", "bitrate": 3000},
    {"source": "program"}
  ],
  "destinations": []
}
```
Take the input's uuid from the same file (its scene's `"uuid"`). Inside a JSON string the `"` of the passphrase is written `\"`. Have three Colour or media inputs on the desk.

| Do | Expect |
|---|---|
| `ffplay -mode listener -passphrase "a&b=c?d e'f\"g" -pbkeylen 16 srt://0.0.0.0:9000`, then start the program | Within five seconds ffplay shows Program, with sound from bus M. In PowerShell the `"` inside the passphrase is written `\"` as shown. |
| `ffplay srt://127.0.0.1:9001` | Program at 1280×720 (ffplay's title or `-stats` shows the size), HEVC. |
| Fade between two inputs (3 s), watching both ffplay windows | Both show the fade, not a cut. |
| `ffplay srt://127.0.0.1:9002` | The chosen input alone. |
| Delete that input on the desk | The 9002 window goes black and stays connected. |
| Close the 9000 ffplay; start it again after ten seconds | The program stays responsive; the picture returns within five seconds. |
| Close the 9001 ffplay; start it again | It reconnects within five seconds. |
| Scene Collection → New, then back | Each window goes black for a moment and stays connected; the pictures return. |
| Close every ffplay, so Output 1 is calling and Outputs 2 and 3 are listening for nobody; close the program | It closes within five seconds; the log ends with `Number of memory leaks: 0` and has no line containing `remaining`. |
| Open the collection file | `bread_outputs` is there as written, the deleted input's uuid kept. |

- [ ] **Step 6: Commit**

```powershell
git add breadstudio/desk
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: run the outputs on OBS canvases, with SRT, and save them with the scene collection"
```

---

### Task 5: The Outputs window

**Files:**
- Create: `breadstudio/desk/SrtDialog.hpp`, `breadstudio/desk/SrtDialog.cpp`, `breadstudio/desk/OutputsWindow.hpp`, `breadstudio/desk/OutputsWindow.cpp`
- Modify: `breadstudio/desk/InputBar.cpp`, `breadstudio/desk/CMakeLists.txt`, `breadstudio/desk/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `OutputEngine` (Task 4), `OutputsConfig` (Task 3), `StreamForm`, `streamProblem`, `streamTypeId` (Plan A), `inputs()` (desk).
- Produces: `class SrtDialog : public QDialog` with `SrtDialog(const StreamForm &, QWidget *)` and `StreamForm form() const`; `class OutputsWindow : public QDialog` with `explicit OutputsWindow(QWidget *)`; an **Outputs** button in the desk's footer.

- [ ] **Step 1: The SRT settings dialog**

Create `breadstudio/desk/SrtDialog.hpp`:
```cpp
#pragma once

#include "StreamAddress.hpp"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace desk {

/* An output's SRT settings: the Stream / SRT page's SRT fields (outputs spec §3.4). */
class SrtDialog : public QDialog {
	Q_OBJECT

public:
	SrtDialog(const StreamForm &form, QWidget *parent);

	StreamForm form() const;

private:
	void check();

	QComboBox *mode = nullptr;
	QLineEdit *host = nullptr;
	QSpinBox *port = nullptr;
	QSpinBox *latency = nullptr;
	QLineEdit *passphrase = nullptr;
	QComboBox *keyLength = nullptr;
	QLineEdit *streamId = nullptr;
	QLabel *problem = nullptr;
	QPushButton *ok = nullptr;
};

} // namespace desk
```

Create `breadstudio/desk/SrtDialog.cpp`:
```cpp
#include "SrtDialog.hpp"
#include "Desk.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <string>

namespace desk {

SrtDialog::SrtDialog(const StreamForm &form, QWidget *parent) : QDialog(parent)
{
	setWindowTitle(text("Outputs.Srt.Title"));
	auto *layout = new QVBoxLayout(this);
	auto *fields = new QFormLayout();
	layout->addLayout(fields);

	mode = new QComboBox(this);
	for (StreamType type : {StreamType::SrtCaller, StreamType::SrtListener, StreamType::SrtRendezvous}) {
		const std::string key = std::string("Stream.Type.") + streamTypeId(type);
		mode->addItem(text(key.c_str()), QString::fromUtf8(streamTypeId(type)));
	}
	mode->setCurrentIndex(std::max(0, mode->findData(QString::fromUtf8(streamTypeId(form.type)))));
	fields->addRow(text("Stream.Type"), mode);

	host = new QLineEdit(QString::fromStdString(form.host), this);
	fields->addRow(text("Stream.Host"), host);

	port = new QSpinBox(this);
	port->setRange(1, 65535);
	port->setValue(form.port);
	fields->addRow(text("Stream.Port"), port);

	latency = new QSpinBox(this);
	latency->setRange(0, 60000);
	latency->setValue(form.latencyMs);
	fields->addRow(text("Stream.Latency"), latency);

	passphrase = new QLineEdit(QString::fromStdString(form.passphrase), this);
	passphrase->setEchoMode(QLineEdit::PasswordEchoOnEdit);
	passphrase->setMaxLength(79);
	fields->addRow(text("Stream.Passphrase"), passphrase);

	keyLength = new QComboBox(this);
	for (int length : {16, 24, 32}) {
		keyLength->addItem(QString::number(length), length);
	}
	keyLength->setCurrentIndex(std::max(0, keyLength->findData(form.keyLength)));
	fields->addRow(text("Stream.KeyLength"), keyLength);

	streamId = new QLineEdit(QString::fromStdString(form.streamId), this);
	fields->addRow(text("Stream.StreamId"), streamId);

	problem = new QLabel(this);
	problem->setWordWrap(true);
	layout->addWidget(problem);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	ok = buttons->button(QDialogButtonBox::Ok);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

	connect(mode, &QComboBox::currentIndexChanged, this, [this](int) { check(); });
	connect(host, &QLineEdit::textChanged, this, [this] { check(); });
	connect(port, &QSpinBox::valueChanged, this, [this](int) { check(); });
	connect(latency, &QSpinBox::valueChanged, this, [this](int) { check(); });
	connect(passphrase, &QLineEdit::textChanged, this, [this] { check(); });
	connect(streamId, &QLineEdit::textChanged, this, [this] { check(); });
	check();
}

StreamForm SrtDialog::form() const
{
	StreamForm result;
	result.type = streamTypeFromId(mode->currentData().toString().toStdString());
	result.host = host->text().toStdString();
	result.port = port->value();
	result.latencyMs = latency->value();
	result.passphrase = passphrase->text().toStdString();
	result.keyLength = keyLength->currentData().toInt();
	result.streamId = streamId->text().toStdString();
	return result;
}

void SrtDialog::check()
{
	const char *why = streamProblem(form());
	problem->setText(why ? text(why) : QString());
	ok->setEnabled(why == nullptr);
}

} // namespace desk
```

- [ ] **Step 2: The Outputs window**

Create `breadstudio/desk/OutputsWindow.hpp`:
```cpp
#pragma once

#include "OutputPlan.hpp"

#include <QDialog>
#include <QTimer>

#include <array>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace desk {

/* The Outputs window: one row per output (outputs spec §3.3). OK applies; only what changed
 * restarts. */
class OutputsWindow : public QDialog {
	Q_OBJECT

public:
	explicit OutputsWindow(QWidget *parent);

	void accept() override;

private:
	struct Row {
		QComboBox *source = nullptr;
		QComboBox *size = nullptr;
		QCheckBox *alpha = nullptr;
		QComboBox *bus = nullptr;
		QCheckBox *ndi = nullptr;
		QComboBox *omt = nullptr;
		QCheckBox *srt = nullptr;
		QPushButton *srtSettings = nullptr;
		QComboBox *codec = nullptr;
		QSpinBox *bitrate = nullptr;
		QLabel *status = nullptr;
		StreamForm srtForm;
	};

	void addRow(int index);
	void syncRow(int index);
	OutputsConfig read() const;
	void updateStatus();

	std::array<Row, OutputCount> rows;
	QTimer timer;
};

} // namespace desk
```

Create `breadstudio/desk/OutputsWindow.cpp`:
```cpp
#include "OutputsWindow.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "OutputEngine.hpp"
#include "SrtDialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>
#include <string>

namespace desk {

static const char *NdiType = "bread_ndi_output";
static const char *OmtType = "bread_omt_output";
static const char *SrtType = "bread_srt_output";
static const char *MultiViewId = "bread_multiview";

/* "program", "preview", "multiview", or "input:<uuid>" */
static QString sourceKey(const OutputConfig &config)
{
	switch (config.source) {
	case OutputSource::Program:
		return "program";
	case OutputSource::Preview:
		return "preview";
	case OutputSource::MultiView:
		return "multiview";
	case OutputSource::Input:
		break;
	}
	return "input:" + QString::fromStdString(config.input);
}

static QString stateText(const SenderStatus &status, bool listener)
{
	switch (status.state) {
	case SenderState::Off:
		return text("Outputs.State.Off");
	case SenderState::Waiting:
		return text(listener ? "Outputs.State.Listening" : "Outputs.State.Calling");
	case SenderState::Connected:
		return text("Outputs.State.Connected");
	case SenderState::Retrying:
		return text("Outputs.State.Retrying");
	case SenderState::Live:
		return status.receivers >= 0 ? text("Outputs.State.Receivers").arg(status.receivers)
					     : text("Outputs.State.Live");
	case SenderState::Failed:
		return text("Outputs.State.Failed");
	}
	return QString();
}

OutputsWindow::OutputsWindow(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(text("Outputs.Title"));
	auto *layout = new QVBoxLayout(this);
	auto *grid = new QGridLayout();
	layout->addLayout(grid);

	const char *headings[] = {"Outputs.Output", "Outputs.Source", "Outputs.Size",    "Outputs.Alpha",
				  "Outputs.Audio",  "Outputs.Ndi",    "Outputs.Omt",     "Outputs.Srt",
				  "",               "Outputs.Codec",  "Outputs.Bitrate", "Outputs.Status"};
	for (int column = 0; column < (int)std::size(headings); column++) {
		if (*headings[column]) {
			grid->addWidget(new QLabel(text(headings[column]), this), 0, column);
		}
	}

	for (int i = 0; i < OutputCount; i++) {
		addRow(i);
		Row &row = rows[(size_t)i];
		grid->addWidget(new QLabel(text("Outputs.Number").arg(i + 1), this), i + 1, 0);
		grid->addWidget(row.source, i + 1, 1);
		grid->addWidget(row.size, i + 1, 2);
		grid->addWidget(row.alpha, i + 1, 3);
		grid->addWidget(row.bus, i + 1, 4);
		grid->addWidget(row.ndi, i + 1, 5);
		grid->addWidget(row.omt, i + 1, 6);
		grid->addWidget(row.srt, i + 1, 7);
		grid->addWidget(row.srtSettings, i + 1, 8);
		grid->addWidget(row.codec, i + 1, 9);
		grid->addWidget(row.bitrate, i + 1, 10);
		grid->addWidget(row.status, i + 1, 11);
	}

	/* a column whose plugin is not in this build says why */
	auto missing = [this](const char *type, int column, QGridLayout *g) {
		if (!OutputEngine::hasOutputType(type)) {
			auto *note = new QLabel(text("Outputs.Missing"), this);
			g->addWidget(note, OutputCount + 1, column);
		}
	};
	missing(NdiType, 5, grid);
	missing(OmtType, 6, grid);
	missing(SrtType, 7, grid);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &OutputsWindow::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

	connect(&timer, &QTimer::timeout, this, &OutputsWindow::updateStatus);
	timer.start(500);
	updateStatus();
}

void OutputsWindow::addRow(int index)
{
	Row &row = rows[(size_t)index];
	const OutputConfig &config = OutputEngine::instance().config().outputs[(size_t)index];

	row.source = new QComboBox(this);
	row.source->addItem(text("Outputs.Program"), "program");
	if (index > 0) {
		row.source->addItem(text("Outputs.Preview"), "preview");
		if (obs_source_get_display_name(MultiViewId)) {
			row.source->addItem(text("Outputs.MultiView"), "multiview");
		}
		for (const OBSSource &input : inputs()) {
			row.source->addItem(QString::fromUtf8(obs_source_get_name(input)),
					    "input:" + QString::fromUtf8(obs_source_get_uuid(input)));
		}
	}
	int found = row.source->findData(sourceKey(config));
	if (found < 0) {
		/* the input it showed is gone: "none" until another is chosen (outputs spec §7) */
		row.source->addItem(text("Outputs.SourceNone"), sourceKey(config));
		found = row.source->count() - 1;
	}
	row.source->setCurrentIndex(found);
	row.source->setEnabled(index > 0);

	row.size = new QComboBox(this);
	row.size->addItem(text("Outputs.SizeProgram"), QSize(0, 0));
	if (index > 0) {
		for (QSize size : {QSize(3840, 2160), QSize(1920, 1080), QSize(1280, 720)}) {
			row.size->addItem(QString("%1×%2").arg(size.width()).arg(size.height()), size);
		}
	}
	row.size->setCurrentIndex(std::max(0, row.size->findData(QSize(config.width, config.height))));

	row.alpha = new QCheckBox(this);
	row.alpha->setChecked(config.alpha);

	row.bus = new QComboBox(this);
	for (const char *bus : {"M", "A", "B", "C", "D", "E"}) {
		row.bus->addItem(bus);
	}
	row.bus->setCurrentIndex(config.bus);

	row.ndi = new QCheckBox(this);
	row.ndi->setChecked(config.ndi);
	row.ndi->setEnabled(OutputEngine::hasOutputType(NdiType));

	row.omt = new QComboBox(this);
	for (const char *key : {"Outputs.Omt.Off", "Outputs.Omt.Vmx", "Outputs.Omt.Hevc", "Outputs.Omt.H264"}) {
		row.omt->addItem(text(key));
	}
	row.omt->setCurrentIndex((int)config.omt);
	row.omt->setEnabled(OutputEngine::hasOutputType(OmtType));

	row.srt = new QCheckBox(this);
	row.srt->setChecked(config.srt);
	row.srt->setEnabled(OutputEngine::hasOutputType(SrtType));
	row.srtForm = config.srtForm;
	row.srtSettings = new QPushButton(QString::fromUtf8("\xE2\x9A\x99"), this);
	connect(row.srtSettings, &QPushButton::clicked, this, [this, index] {
		Row &r = rows[(size_t)index];
		SrtDialog dialog(r.srtForm, this);
		if (dialog.exec() == QDialog::Accepted) {
			r.srtForm = dialog.form();
			r.srt->setChecked(true);
		}
	});

	row.codec = new QComboBox(this);
	row.codec->addItem("H.264");
	row.codec->addItem("HEVC");
	row.codec->setCurrentIndex(config.codec == VideoCodec::Hevc ? 1 : 0);

	row.bitrate = new QSpinBox(this);
	row.bitrate->setRange(500, 100000);
	row.bitrate->setSingleStep(500);
	row.bitrate->setSuffix(" kbps");
	row.bitrate->setValue(config.bitrateKbps);

	row.status = new QLabel(this);
	row.status->setMinimumWidth(260);

	connect(row.source, &QComboBox::currentIndexChanged, this, [this, index](int) { syncRow(index); });
	connect(row.srt, &QCheckBox::toggled, this, [this, index](bool) { syncRow(index); });
	syncRow(index);
}

/* alpha only for an input; SRT needs settings that can be used */
void OutputsWindow::syncRow(int index)
{
	Row &row = rows[(size_t)index];
	const bool input = row.source->currentData().toString().startsWith("input:");
	row.alpha->setEnabled(index > 0 && input);
	row.srtSettings->setEnabled(row.srt->isEnabled());
	if (row.srt->isChecked() && streamProblem(row.srtForm)) {
		row.srt->setChecked(false);
		row.srtSettings->click();
	}
}

OutputsConfig OutputsWindow::read() const
{
	OutputsConfig config = OutputEngine::instance().config(); /* keeps the destinations */
	for (int i = 0; i < OutputCount; i++) {
		const Row &row = rows[(size_t)i];
		OutputConfig &o = config.outputs[(size_t)i];
		const QString key = row.source->currentData().toString();
		o.source = key == "preview"     ? OutputSource::Preview
			   : key == "multiview" ? OutputSource::MultiView
			   : key.startsWith("input:") ? OutputSource::Input
						      : OutputSource::Program;
		o.input = o.source == OutputSource::Input ? key.mid(6).toStdString() : std::string();
		const QSize size = row.size->currentData().toSize();
		o.width = size.width();
		o.height = size.height();
		o.alpha = row.alpha->isEnabled() && row.alpha->isChecked();
		o.bus = row.bus->currentIndex();
		o.ndi = row.ndi->isChecked();
		o.omt = (OmtMode)row.omt->currentIndex();
		o.srt = row.srt->isChecked() && !streamProblem(row.srtForm);
		o.srtForm = row.srtForm;
		o.codec = row.codec->currentIndex() == 1 ? VideoCodec::Hevc : VideoCodec::H264;
		o.bitrateKbps = row.bitrate->value();
	}
	return config;
}

void OutputsWindow::accept()
{
	OutputEngine::instance().apply(read());
	QDialog::accept();
}

void OutputsWindow::updateStatus()
{
	for (int i = 0; i < OutputCount; i++) {
		const OutputStatus status = OutputEngine::instance().status(i);
		const OutputConfig &config = OutputEngine::instance().config().outputs[(size_t)i];
		QStringList lines;
		if (status.width > 0) {
			lines << text("Outputs.Picture").arg(status.width).arg(status.height).arg(status.skipped);
		}
		if (!status.encoder.empty()) {
			lines << QString::fromStdString(status.encoder);
		}
		if (config.ndi) {
			lines << "NDI: " + stateText(status.ndi, false);
		}
		if (config.omt != OmtMode::Off) {
			lines << "OMT: " + stateText(status.omt, false);
		}
		if (config.srt) {
			lines << "SRT: " + stateText(status.srt, config.srtForm.type == StreamType::SrtListener);
		}
		QLabel *label = rows[(size_t)i].status;
		label->setText(lines.join("\n"));
		const bool red = status.srt.state == SenderState::Retrying || status.ndi.state == SenderState::Failed ||
				 status.omt.state == SenderState::Failed;
		label->setStyleSheet(red ? "color: #d00000;" : "");
	}
}

} // namespace desk
```

- [ ] **Step 3: The Outputs button**

In `breadstudio/desk/InputBar.cpp`, add `#include "OutputsWindow.hpp"` to the includes, and in the constructor, after `footer->addWidget(add);`, add:
```cpp
	/* the four outputs (outputs spec §3.3); one window at a time */
	auto *outputs = new QPushButton(text("Outputs"), this);
	connect(outputs, &QPushButton::clicked, this, [] {
		for (OutputsWindow *open : settingsParent()->findChildren<OutputsWindow *>()) {
			open->raise();
			open->activateWindow();
			return;
		}
		auto *window = new OutputsWindow(settingsParent());
		window->setAttribute(Qt::WA_DeleteOnClose);
		window->show();
	});
	footer->addWidget(outputs);
```
In `InputBar::clear()`, add as its first line:
```cpp
	qDeleteAll(settingsParent()->findChildren<OutputsWindow *>());
```

In `breadstudio/desk/CMakeLists.txt`, add to the plugin's sources, in alphabetical places:
```cmake
    OutputsWindow.cpp
    OutputsWindow.hpp
    SrtDialog.cpp
    SrtDialog.hpp
```

Append to `breadstudio/desk/data/locale/en-US.ini`:
```ini
Outputs="Outputs"
Outputs.Title="Outputs"
Outputs.Output="Output"
Outputs.Number="Output %1"
Outputs.Source="Source"
Outputs.Size="Size"
Outputs.Alpha="Alpha"
Outputs.Audio="Audio bus"
Outputs.Ndi="NDI"
Outputs.Omt="OMT"
Outputs.Srt="SRT"
Outputs.Codec="Encoder"
Outputs.Bitrate="Bitrate"
Outputs.Status="State"
Outputs.Program="Program"
Outputs.Preview="Preview"
Outputs.MultiView="MultiView"
Outputs.SourceNone="none (the input was deleted)"
Outputs.SizeProgram="Same as Program"
Outputs.Omt.Off="Off"
Outputs.Omt.Vmx="OMT"
Outputs.Omt.Hevc="OMT-HX HEVC"
Outputs.Omt.H264="OMT-HX H.264"
Outputs.Missing="Not in this build"
Outputs.Picture="%1×%2, %3 frames behind"
Outputs.State.Off="off"
Outputs.State.Listening="listening"
Outputs.State.Calling="calling"
Outputs.State.Connected="connected"
Outputs.State.Retrying="trying again"
Outputs.State.Live="live"
Outputs.State.Receivers="%1 receiver(s)"
Outputs.State.Failed="stopped"
Outputs.Srt.Title="SRT"
```

- [ ] **Step 4: Build and check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk
```
Expected: builds with no warning. Start the program with Task 4's collection.

| Do | Expect |
|---|---|
| Footer → Outputs | The window shows the four rows as Task 4's file set them, each with its state: `SRT: calling` for Output 1 until a listener runs, `SRT: listening` for 2 and 3. The NDI and OMT columns are greyed out with "Not in this build". |
| Start `ffplay -mode listener -passphrase "a&b=c?d e'f\"g" -pbkeylen 16 srt://0.0.0.0:9000` | Output 1's state turns `connected` within five seconds. |
| Output 2: source Preview; OK | The 9001 receiver shows Preview without reconnecting. |
| Output 2: size 1920×1080; OK | The 9001 receiver reconnects and shows 1920×1080. Output 1's receiver did not blink. |
| Output 3: tick SRT on with an empty SRT setting | The SRT dialog opens; OK stays greyed while Host is empty for a caller. |
| Output 4: source an input, Alpha ticked, SRT listener on 9003; OK | `ffplay srt://127.0.0.1:9003` shows the input (SRT carries no alpha). |
| Delete the input Output 4 shows; open Outputs | Output 4's source reads "none (the input was deleted)"; the 9003 window is black and connected. |
| Open Outputs and Cancel | Nothing changes. |
| Scene Collection → New while the window is open | The window closes; the receivers stay connected. |

- [ ] **Step 5: Commit**

```powershell
git add breadstudio/desk
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the Outputs window"
```

---

### Task 6: RTMP destinations and the Stream button

**Files:**
- Create: `breadstudio/desk/StreamWindow.hpp`, `breadstudio/desk/StreamWindow.cpp`
- Modify: `breadstudio/desk/InputBar.cpp`, `breadstudio/desk/InputBar.hpp`, `breadstudio/desk/DeskLayout.cpp`, `breadstudio/desk/DeskLayout.hpp`, `breadstudio/desk/desk.cpp`, `breadstudio/desk/CMakeLists.txt`, `breadstudio/desk/data/locale/en-US.ini`, `breadstudio/docs/2026-10-15-breadstudio-outputs-design.md`

**Interfaces:**
- Consumes: `OutputEngine::setStreaming`, `streaming`, `destinationStatus`, `apply`, `config` (Task 4).
- Produces: `class StreamWindow : public QDialog` with `explicit StreamWindow(QWidget *)`; `void hideObsStreamButton();` in `DeskLayout.hpp`; the footer's Stream button and its ⚙.

- [ ] **Step 1: The Stream window**

Create `breadstudio/desk/StreamWindow.hpp`:
```cpp
#pragma once

#include "OutputPlan.hpp"

#include <QDialog>
#include <QTimer>

#include <array>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

namespace desk {

/* The three RTMP destinations (outputs spec §3.2). OK applies; a live destination whose settings
 * did not change stays live. */
class StreamWindow : public QDialog {
	Q_OBJECT

public:
	explicit StreamWindow(QWidget *parent);

	void accept() override;

private:
	struct Row {
		QCheckBox *on = nullptr;
		QComboBox *output = nullptr;
		QLineEdit *server = nullptr;
		QLineEdit *key = nullptr;
		QLabel *status = nullptr;
		uint64_t lastBytes = 0;
	};

	void updateStatus();

	std::array<Row, DestinationCount> rows;
	QTimer timer;
};

} // namespace desk
```

Create `breadstudio/desk/StreamWindow.cpp`:
```cpp
#include "StreamWindow.hpp"
#include "Desk.hpp"
#include "OutputEngine.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace desk {

StreamWindow::StreamWindow(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(text("Stream.Title"));
	auto *layout = new QVBoxLayout(this);
	auto *grid = new QGridLayout();
	layout->addLayout(grid);

	const char *headings[] = {"Stream.On", "Outputs.Output", "Stream.Server", "Stream.Key", "Outputs.Status"};
	for (int column = 0; column < 5; column++) {
		grid->addWidget(new QLabel(text(headings[column]), this), 0, column);
	}

	for (int d = 0; d < DestinationCount; d++) {
		const Destination &destination = OutputEngine::instance().config().destinations[(size_t)d];
		Row &row = rows[(size_t)d];
		row.on = new QCheckBox(this);
		row.on->setChecked(destination.on);
		row.output = new QComboBox(this);
		for (int i = 0; i < OutputCount; i++) {
			row.output->addItem(text("Outputs.Number").arg(i + 1));
		}
		row.output->setCurrentIndex(destination.output);
		row.server = new QLineEdit(QString::fromStdString(destination.server), this);
		row.server->setPlaceholderText("rtmp://server/app");
		row.server->setMinimumWidth(280);
		row.key = new QLineEdit(QString::fromStdString(destination.key), this);
		row.key->setEchoMode(QLineEdit::PasswordEchoOnEdit);
		row.status = new QLabel(this);
		row.status->setMinimumWidth(200);

		grid->addWidget(row.on, d + 1, 0);
		grid->addWidget(row.output, d + 1, 1);
		grid->addWidget(row.server, d + 1, 2);
		grid->addWidget(row.key, d + 1, 3);
		grid->addWidget(row.status, d + 1, 4);
	}

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &StreamWindow::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

	connect(&timer, &QTimer::timeout, this, &StreamWindow::updateStatus);
	timer.start(500);
	updateStatus();
}

void StreamWindow::accept()
{
	OutputsConfig config = OutputEngine::instance().config();
	for (int d = 0; d < DestinationCount; d++) {
		const Row &row = rows[(size_t)d];
		Destination &destination = config.destinations[(size_t)d];
		const QString server = row.server->text().trimmed();
		/* a destination without a server cannot be on */
		destination.on = row.on->isChecked() && !server.isEmpty();
		destination.output = row.output->currentIndex();
		destination.server = server.toStdString();
		destination.key = row.key->text().trimmed().toStdString();
	}
	OutputEngine::instance().apply(config);
	QDialog::accept();
}

void StreamWindow::updateStatus()
{
	for (int d = 0; d < DestinationCount; d++) {
		Row &row = rows[(size_t)d];
		const SenderStatus status = OutputEngine::instance().destinationStatus(d);
		QString line;
		switch (status.state) {
		case SenderState::Live: {
			/* the window polls twice a second */
			const uint64_t kbps = (status.bytes - row.lastBytes) * 8 * 2 / 1000;
			line = text("Stream.State.Live").arg(kbps).arg(status.dropped);
			break;
		}
		case SenderState::Retrying:
			line = text("Outputs.State.Retrying");
			break;
		case SenderState::Failed:
			line = text("Outputs.State.Failed");
			break;
		case SenderState::Waiting:
			line = text("Stream.State.Starting");
			break;
		default:
			line = text("Outputs.State.Off");
			break;
		}
		row.lastBytes = status.bytes;
		row.status->setText(line);
		row.status->setStyleSheet(status.state == SenderState::Retrying || status.state == SenderState::Failed
						  ? "color: #d00000;"
						  : "");
	}
}

} // namespace desk
```

- [ ] **Step 2: The Stream button drives the destinations**

In `breadstudio/desk/InputBar.cpp`, add `#include "OutputEngine.hpp"` and `#include "StreamWindow.hpp"` to the includes. Replace the Stream button's block:
```cpp
	stream = new QPushButton(text("Stream"), this);
	connect(stream, &QPushButton::clicked, this, [] {
		if (obs_frontend_streaming_active()) {
			obs_frontend_streaming_stop();
		} else {
			obs_frontend_streaming_start();
		}
	});
	footer->addWidget(stream);
```
with:
```cpp
	/* the RTMP destinations (outputs spec §3.2), not OBS's own stream */
	stream = new QPushButton(text("Stream"), this);
	connect(stream, &QPushButton::clicked, this, [this] {
		OutputEngine &engine = OutputEngine::instance();
		if (engine.streaming()) {
			engine.setStreaming(false);
		} else {
			bool any = false;
			for (const Destination &destination : engine.config().destinations) {
				any = any || destination.on;
			}
			if (!any) {
				QMessageBox::information(window(), text("Stream"), text("Stream.NoneOn"));
				openStreamWindow();
				return;
			}
			engine.setStreaming(true);
		}
		updateOutputs();
	});
	footer->addWidget(stream);

	auto *streamSettings = new QPushButton(QString::fromUtf8(GearGlyph), this);
	streamSettings->setToolTip(text("Stream.Title"));
	connect(streamSettings, &QPushButton::clicked, this, [this] { openStreamWindow(); });
	footer->addWidget(streamSettings);

	/* the button is red while any destination is live, and says so */
	connect(&streamTimer, &QTimer::timeout, this, &InputBar::updateOutputs);
	streamTimer.start(1000);
```

Replace `InputBar::updateOutputs()` with:
```cpp
void InputBar::updateOutputs()
{
	record->setText(text(obs_frontend_recording_active() ? "Record.Stop" : "Record"));

	OutputEngine &engine = OutputEngine::instance();
	bool anyLive = false;
	for (int d = 0; d < DestinationCount; d++) {
		anyLive = anyLive || engine.destinationStatus(d).state == SenderState::Live;
	}
	stream->setText(text(engine.streaming() ? "Stream.Stop" : "Stream"));
	stream->setStyleSheet(anyLive ? "background-color: #d00000; color: white;" : "");
}

void InputBar::openStreamWindow()
{
	for (StreamWindow *open : settingsParent()->findChildren<StreamWindow *>()) {
		open->raise();
		open->activateWindow();
		return;
	}
	auto *window = new StreamWindow(settingsParent());
	window->setAttribute(Qt::WA_DeleteOnClose);
	window->show();
}
```

In `InputBar::clear()`, after the line that deletes `OutputsWindow`s, add:
```cpp
	qDeleteAll(settingsParent()->findChildren<StreamWindow *>());
```

In `breadstudio/desk/InputBar.hpp`, add `#include <QTimer>` to the includes, declare in the public part `void openStreamWindow();`, and add to the private members `QTimer streamTimer;`.

- [ ] **Step 3: Hide OBS's Start Streaming button**

In `breadstudio/desk/DeskLayout.hpp`, add before the closing `}`:
```cpp
/* OBS's own Start Streaming button: the desk's Stream button runs the RTMP destinations instead
 * (outputs spec §4.5). Found by its name; when it is not found, the log says so and it stays. */
void hideObsStreamButton();
```

In `breadstudio/desk/DeskLayout.cpp`, add `#include <QPushButton>` and, before the closing `} // namespace desk`:
```cpp
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
```

In `breadstudio/desk/desk.cpp`, in the `OBS_FRONTEND_EVENT_FINISHED_LOADING` case, after `applyLayoutOnFirstRun();`, add:
```cpp
		hideObsStreamButton();
```

In `breadstudio/desk/CMakeLists.txt`, add to the plugin's sources, in alphabetical places:
```cmake
    StreamWindow.cpp
    StreamWindow.hpp
```

Append to `breadstudio/desk/data/locale/en-US.ini`:
```ini
Stream.Title="Streaming destinations"
Stream.On="On"
Stream.Server="Server"
Stream.Key="Stream key"
Stream.NoneOn="No streaming destination is on. Set one up, tick On, and press Stream again."
Stream.State.Live="live, %1 kbps, %2 dropped"
Stream.State.Starting="starting"
```
(`Stream` and `Stream.Stop` exist already.)

- [ ] **Step 4: Bring the spec's audio row in line**

In `breadstudio/docs/2026-10-15-breadstudio-outputs-design.md`, §3.1's table, change the Audio row to:
```
| Audio | Bus M or A to E | Bus M or A to E |
```
and after the table's bullet "An output's audio is one of OBS's six tracks…", add a bullet:
```
- OBS has six audio tracks and the desk uses all six as buses, so there is no
  silent track: for an output without sound, pick a bus nothing is sent to.
```

- [ ] **Step 5: Build and check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk
```
Expected: builds with no warning. Two RTMP receivers: `ffplay -listen 1 rtmp://127.0.0.1:1935/live/one` and `ffplay -listen 1 rtmp://127.0.0.1:1936/live/two`.

| Do | Expect |
|---|---|
| Start the program | OBS's Controls dock has no Start Streaming button; the log has no `streamButton' was not found`. |
| Press Stream with no destination on | The message, then the Stream window. |
| Destination 1: On, Output 1, `rtmp://127.0.0.1:1935/live`, key `one`. Destination 2: On, Output 2, `rtmp://127.0.0.1:1936/live`, key `two`. OK; Stream | Both ffplay windows show their outputs within five seconds; the button reads Stop Stream and is red; the window shows each live with its bitrate. Output 1's and Output 2's SRT receivers, if running, did not blink. |
| Close the `two` ffplay; wait ten seconds; start it again | Destination 2 reads `trying again`, then live again; destination 1 never stopped. |
| Change destination 1's key to `one` again (unchanged) and OK | Nothing restarts. |
| Change Output 2's bitrate in the Outputs window; OK | Destination 2 and Output 2's SRT restart; destination 1 does not. |
| Stop Stream | Both ffplay windows end; the button is grey and reads Stream. |
| Close the program while streaming to a server that is down | It closes within five seconds; the log ends with `Number of memory leaks: 0`. |

- [ ] **Step 6: Commit**

```powershell
git add breadstudio/desk breadstudio/docs/2026-10-15-breadstudio-outputs-design.md
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: stream to three RTMP destinations from the desk's Stream button"
```

---

### Task 7: The smoke checklist, and the run

**Files:**
- Modify: `breadstudio/docs/DESK-SMOKE.md`

- [ ] **Step 1: Add section H**

In `breadstudio/docs/DESK-SMOKE.md`, before `## Before a run`, add:
```markdown
## H. Outputs, SRT and streaming

Spec: `2026-10-15-breadstudio-outputs-design.md`. Needs `ffplay.exe` with SRT
and RTMP; the receiver lines are in `2026-10-15-breadstudio-outputs-engine.md`,
Tasks 4 to 6.

1. Output 1 over SRT, as a caller with the passphrase `a&b=c?d e'f"g`, to an
   `ffplay` listener: picture and sound within five seconds.
2. Output 2 set to Program at 1280×720 over SRT shows the same fade as Program,
   not a cut.
3. Output 3 set to one input: deleting that input keeps its receiver
   connected, black; the Outputs window reads "none".
4. Changing Output 2's source interrupts nothing; changing its size restarts
   only its own receivers.
5. Two RTMP destinations on two outputs: the Stream button starts both, a
   receiver that goes away and comes back is retried, and stopping ends both.
   OBS's own Start Streaming button is not shown.
6. Scene Collection → New and back with everything running: every receiver
   stays connected.
7. `bread-outputs-test` and `bread-stream-test` pass.
8. After closing with every output and a destination running, the log has
   `Number of memory leaks: 0` and no line containing `remaining`.
```

In section C (checked after every upstream merge), add as its last item, numbered after the ones there:
```markdown
3. OBS's Controls dock shows no Start Streaming button, and the log has no
   line `'streamButton' was not found`. An output set to Program shows a
   fade as a fade (section H, item 2): OBS still draws the main mix first.
```

- [ ] **Step 2: Run the automated tests**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-outputs-test bread-stream-test bread-sources-test bread-desk-test
foreach ($t in 'bread-outputs-test', 'bread-stream-test', 'bread-sources-test', 'bread-desk-test') { & "build_x64\breadstudio\desk\test-bin\RelWithDebInfo\$t.exe" | Select-Object -Last 1; "exit $LASTEXITCODE" }
```
Expected: each ends `exit 0`.

- [ ] **Step 3: Run section H, and record**

Build everything (`breadstudio\build.ps1`), run section H on this PC, and add an entry under `## Runs` in the existing form. A second PC (spec §1.1 items 1 to 3) waits for Plan F; items 4 and 6 of §1.1 are covered by H.

- [ ] **Step 4: Commit**

```powershell
git add breadstudio/docs/DESK-SMOKE.md
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add outputs and streaming to the smoke checklist, and record the run"
```

---

## Left for later plans

| Item | Where |
|---|---|
| The MultiView source, and `sourceFor()`'s MultiView case | Plan E |
| `bread_ndi_output` and `bread_omt_output` to the contract in Global Constraints | Plan F |
| Mix inputs, and the on-air rule's extra step | Plan G |
| A second PC receiving over NDI and OMT (spec §1.1 items 1 to 3) | Plan F's run |
