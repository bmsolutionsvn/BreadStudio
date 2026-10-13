# BreadStudio — Input Select window and Stream / SRT inputs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The desk's Add Input button opens a vMix-style Input Select window with one page per kind of input, including a Stream / SRT page that plays SRT, RTSP and transport streams through OBS's Media Source.

**Architecture:** Everything is in the desk plugin, `breadstudio/desk`. A dialog, `InputSelect`, holds a list of kinds and one `InputPage` per kind. Existing kinds use `PropertiesPage`, which embeds OBS's own settings panel (`shared/properties-view`) on a private stand-in source. The stream kind uses `StreamPage`, a form whose values a plain function, `streamTarget()`, turns into the Media Source's address and FFmpeg options. The same window, with one page and no list, edits an existing input's base.

**Tech Stack:** C++17, Qt 6 Widgets, libobs and obs-frontend-api from OBS Studio 32.2.2, OBS's `shared/properties-view`, CMake (OBS's `windows-x64` preset), Visual Studio 2026, Windows PowerShell 5.1.

**Spec:** `breadstudio/docs/2026-10-14-breadstudio-network-inputs-design.md`. This is its Plan A (§12): §3, §4.4, §5, §6 rows 1 and 2, §9, and the parts of §10 and §11 that concern them. Read it before any task. Section numbers below (§) refer to it. The desk it builds on is `2026-10-13-breadstudio-s1-desk-design.md`.

## Global Constraints

- Base: OBS Studio 32.2.2. All new code is under `breadstudio/desk`. This plan edits no OBS file; if a task finds it must, the file goes on `breadstudio/UPSTREAM-EDITS.md` in the same commit.
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1` passes before every commit.
- Commit messages: `breadstudio: <what changed>`. Never rebase or force-push `main`. Nothing is pushed: every `git push` is an owner gate.
- C++17 (OBS's setting). OBS builds with warnings as errors: fix a warning with the smallest change, do not switch it off.
- OBS's code style: tabs, 120 columns, braces on every `if`.
- Every string the operator sees is in `breadstudio/desk/data/locale/en-US.ini` and read with `desk::text("Key")`.
- Windows x64 only. Build from a Visual Studio 2026 developer shell. Build the plugin with `cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk`; the program must be closed first, because it holds the DLL.
- Run the program from `build_x64\rundir\RelWithDebInfo\bin\64bit\obs64.exe --portable`. Before a run, `build_x64\rundir` must hold no `*sync-conflict*` file: OBS loads every DLL in `obs-plugins\64bit` as a plugin (see `DESK-SMOKE.md`, "Before a run").
- After a run that closes the program normally, the newest log in `build_x64\rundir\RelWithDebInfo\config\obs-studio\logs` ends with `Number of memory leaks: 0`, except the session that showed OBS's first-run wizard, which logs 1 in stock OBS too.
- An input is an OBS scene whose bottom item is its base (desk spec §3). New inputs are made only through `desk::createInput`, which places the input last and locks and fits its base.

## Review Focus

1. **Text that could break the FFmpeg options.** A passphrase or stream ID with a space, a quote, `=` or `&` must reach the stream unchanged and must not add or change another option. Owned by Task 1's tests.
2. **Cancel leaves nothing behind.** Visiting every row and pressing Cancel must make no input and no source that outlives the window, and a camera opened by the Camera page must be free again. Owned by Task 2's hand-check and its log check.
3. **The window open while the inputs go away.** A scene collection switched through OBS's websocket, or an input deleted, while a window is open must close that window and leave no reference behind. Owned by Task 2 (add window) and Task 4 (edit window).
4. **A listener nobody calls.** An SRT listener input waits for a caller for as long as it lives. Deleting that input, or closing the program, must not hang. Owned by Task 3's hand-check.
5. **A base this window did not make.** "Base properties…" on an input whose base is a group, on an empty scene, and on a Media Source set up as a network stream in stock OBS must open something sensible or stay greyed out, and must not crash. Owned by Task 4's hand-check.

## File Structure

| Path | Responsibility |
|---|---|
| `breadstudio/desk/StreamAddress.hpp`, `.cpp` | The stream form as data; `streamProblem()` and `streamTarget()`. No OBS, no Qt. |
| `breadstudio/desk/test/StreamAddressTest.cpp` | Its test, built as `bread-stream-test` |
| `breadstudio/desk/InputPage.hpp` | The interface every page of the window has |
| `breadstudio/desk/PropertiesPage.hpp`, `.cpp` | A page that embeds OBS's settings panel for one source type or one existing source |
| `breadstudio/desk/StreamPage.hpp`, `.cpp` | The Stream / SRT form, its saved values, the status box |
| `breadstudio/desk/InputSelect.hpp`, `.cpp` | The window: the list of kinds, the pages, OK and Cancel; and the one-page form of it for an existing input |
| `breadstudio/desk/InputBar.cpp`, `.hpp` | Add Input opens the window; the Add menu and `addInput` go |
| `breadstudio/desk/InputSettings.cpp` | "Base properties…" opens the window |
| `breadstudio/desk/CMakeLists.txt` | New sources, `OBS::properties-view`, the second test |
| `breadstudio/desk/data/locale/en-US.ini` | The new strings |
| `breadstudio/docs/DESK-SMOKE.md` | Section E, and the Add wording |

The row for NDI, OMT and Desktop Capture (§3.1) is "Desktop Capture" in this plan and has no tabs: the NDI and OMT tabs arrive with Plans B and C, which wrap this page.

---

### Task 1: The stream address function

**Files:**
- Create: `breadstudio/desk/StreamAddress.hpp`, `breadstudio/desk/StreamAddress.cpp`, `breadstudio/desk/test/StreamAddressTest.cpp`
- Modify: `breadstudio/desk/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces, in namespace `desk`:
  - `enum class StreamType { SrtCaller, SrtListener, SrtRendezvous, RtspUdp, RtspTcp, TsUdp, TsTcp, TsTcpPull };`
  - `struct StreamForm { StreamType type; std::string host; int port; int latencyMs; std::string passphrase; int keyLength; std::string streamId; bool hardwareDecoder; };` with defaults `SrtCaller`, `""`, `5000`, `200`, `""`, `32`, `""`, `true`
  - `bool operator==(const StreamForm &, const StreamForm &);`
  - `struct StreamTarget { std::string input; std::string format; std::string options; };`
  - `const char *streamTypeId(StreamType);` and `StreamType streamTypeFromId(const std::string &);`
  - `bool isSrt(StreamType);` and `bool isRtsp(StreamType);`
  - `const char *streamProblem(const StreamForm &);` — a locale key, or `nullptr` when the form can be used
  - `StreamTarget streamTarget(const StreamForm &);` — call only when `streamProblem()` is null

How the form reaches FFmpeg, and why not in the address: OBS's Media Source hands its `ffmpeg_options` string to `av_dict_parse_string(…, "=", " ", 0)`, which reads each value with `av_get_token`: inside `'…'` nothing is special, and a quote is written `'\''`. FFmpeg's SRT code does not percent-decode every query parameter, so a passphrase with `&` in the address would be cut short. So the address carries only host and port, and every other value goes in the options, quoted.

- [ ] **Step 1: Write the header**

Create `breadstudio/desk/StreamAddress.hpp`:
```cpp
#pragma once

#include <string>

namespace desk {

/* the kinds of network stream the Stream / SRT page offers (spec §5.2) */
enum class StreamType { SrtCaller, SrtListener, SrtRendezvous, RtspUdp, RtspTcp, TsUdp, TsTcp, TsTcpPull };

/* What the page holds (spec §5.1). For the two RTSP types `host` is the whole rtsp:// address
 * and `port` is not used. */
struct StreamForm {
	StreamType type = StreamType::SrtCaller;
	std::string host;
	int port = 5000;
	int latencyMs = 200;
	std::string passphrase;
	int keyLength = 32;
	std::string streamId;
	bool hardwareDecoder = true;
};

bool operator==(const StreamForm &a, const StreamForm &b);

/* what OBS's Media Source is given */
struct StreamTarget {
	std::string input;   /* the address */
	std::string format;  /* "mpegts", or empty to let FFmpeg find out */
	std::string options; /* FFmpeg options: key=value pairs with a space between */
};

/* the name a type is saved under, and back; a name that is not known gives SrtCaller */
const char *streamTypeId(StreamType type);
StreamType streamTypeFromId(const std::string &id);

bool isSrt(StreamType type);
bool isRtsp(StreamType type);

/* Why the form cannot be used yet, as a locale key; null when it can. */
const char *streamProblem(const StreamForm &form);

/* The address and options for a form. Call only when streamProblem() is null. */
StreamTarget streamTarget(const StreamForm &form);

} // namespace desk
```

- [ ] **Step 2: Write a stub so the test links**

Create `breadstudio/desk/StreamAddress.cpp`:
```cpp
#include "StreamAddress.hpp"

namespace desk {

bool operator==(const StreamForm &, const StreamForm &)
{
	return false;
}

const char *streamTypeId(StreamType)
{
	return "";
}

StreamType streamTypeFromId(const std::string &)
{
	return StreamType::SrtCaller;
}

bool isSrt(StreamType)
{
	return false;
}

bool isRtsp(StreamType)
{
	return false;
}

const char *streamProblem(const StreamForm &)
{
	return nullptr;
}

StreamTarget streamTarget(const StreamForm &)
{
	return {};
}

} // namespace desk
```

- [ ] **Step 3: Write the failing test**

Create `breadstudio/desk/test/StreamAddressTest.cpp`:
```cpp
#include "StreamAddress.hpp"

#include <cstdio>
#include <cstring>
#include <string>

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

static bool problemIs(const StreamForm &form, const char *key)
{
	const char *problem = streamProblem(form);
	if (!key) {
		return problem == nullptr;
	}
	return problem && std::strcmp(problem, key) == 0;
}

static StreamForm srt(StreamType type, const char *host)
{
	StreamForm form;
	form.type = type;
	form.host = host;
	return form;
}

int main()
{
	/* SRT caller, nothing optional: only mode and latency, latency in microseconds */
	{
		const StreamTarget t = streamTarget(srt(StreamType::SrtCaller, "10.0.0.5"));
		CHECK(t.input == "srt://10.0.0.5:5000");
		CHECK(t.format == "mpegts");
		CHECK(t.options == "mode=caller latency=200000");
	}

	/* SRT listener: an empty host means every network card */
	{
		StreamForm form = srt(StreamType::SrtListener, "");
		form.port = 9000;
		form.latencyMs = 120;
		CHECK(problemIs(form, nullptr));
		const StreamTarget t = streamTarget(form);
		CHECK(t.input == "srt://0.0.0.0:9000");
		CHECK(t.options == "mode=listener latency=120000");
	}

	/* SRT rendezvous */
	{
		const StreamTarget t = streamTarget(srt(StreamType::SrtRendezvous, "cam.example"));
		CHECK(t.input == "srt://cam.example:5000");
		CHECK(t.options == "mode=rendezvous latency=200000");
	}

	/* passphrase and stream ID are quoted, so a space, & and = stay part of the value;
	 * the key length goes with the passphrase and not without it */
	{
		StreamForm form = srt(StreamType::SrtCaller, "10.0.0.5");
		form.passphrase = "secret pass&word=1";
		form.keyLength = 16;
		form.streamId = "#!::r=live,m=request";
		const StreamTarget t = streamTarget(form);
		CHECK(t.input == "srt://10.0.0.5:5000");
		CHECK(t.options == "mode=caller latency=200000 passphrase='secret pass&word=1' pbkeylen=16 "
				   "streamid='#!::r=live,m=request'");
	}

	/* a quote in a value is written '\'' */
	{
		StreamForm form = srt(StreamType::SrtCaller, "10.0.0.5");
		form.streamId = "it's";
		CHECK(streamTarget(form).options == "mode=caller latency=200000 streamid='it'\\''s'");
	}

	/* a key length that is not 16, 24 or 32 becomes 32 */
	{
		StreamForm form = srt(StreamType::SrtCaller, "10.0.0.5");
		form.passphrase = "0123456789";
		form.keyLength = 99;
		CHECK(streamTarget(form).options == "mode=caller latency=200000 passphrase='0123456789' pbkeylen=32");
	}

	/* the host is trimmed; an IPv6 address gets brackets, once */
	{
		CHECK(streamTarget(srt(StreamType::SrtCaller, "  10.0.0.5 ")).input == "srt://10.0.0.5:5000");
		CHECK(streamTarget(srt(StreamType::SrtCaller, "::1")).input == "srt://[::1]:5000");
		CHECK(streamTarget(srt(StreamType::SrtCaller, "[fe80::1]")).input == "srt://[fe80::1]:5000");
	}

	/* RTSP: the address as typed, trimmed; the transport is an option */
	{
		StreamForm form = srt(StreamType::RtspTcp, " rtsp://cam.local:554/stream1 ");
		StreamTarget t = streamTarget(form);
		CHECK(t.input == "rtsp://cam.local:554/stream1");
		CHECK(t.format.empty());
		CHECK(t.options == "rtsp_transport=tcp");
		form.type = StreamType::RtspUdp;
		CHECK(streamTarget(form).options == "rtsp_transport=udp");
	}

	/* transport stream over UDP: with no host it listens on the port */
	{
		StreamForm form = srt(StreamType::TsUdp, "");
		form.port = 1234;
		StreamTarget t = streamTarget(form);
		CHECK(t.input == "udp://@:1234");
		CHECK(t.format == "mpegts");
		CHECK(t.options.empty());
		form.host = "239.1.1.1";
		CHECK(streamTarget(form).input == "udp://239.1.1.1:1234");
	}

	/* transport stream over TCP: listening, and pulling */
	{
		StreamForm form = srt(StreamType::TsTcp, "");
		form.port = 7000;
		StreamTarget t = streamTarget(form);
		CHECK(t.input == "tcp://0.0.0.0:7000");
		CHECK(t.options == "listen=1");
		CHECK(t.format == "mpegts");
		form.type = StreamType::TsTcpPull;
		form.host = "10.0.0.9";
		t = streamTarget(form);
		CHECK(t.input == "tcp://10.0.0.9:7000");
		CHECK(t.options.empty());
	}

	/* what stops OK */
	{
		CHECK(problemIs(srt(StreamType::SrtCaller, ""), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtCaller, "   "), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtRendezvous, ""), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::TsTcpPull, ""), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtListener, ""), nullptr));
		CHECK(problemIs(srt(StreamType::TsUdp, ""), nullptr));
		CHECK(problemIs(srt(StreamType::TsTcp, ""), nullptr));

		/* a host must be a host: nothing that starts a path, a query or a second address */
		CHECK(problemIs(srt(StreamType::SrtCaller, "10.0.0.5/x"), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtCaller, "10.0.0.5?mode=listener"), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtCaller, "a b"), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtCaller, "user@host"), "Stream.Problem.Host"));
		CHECK(problemIs(srt(StreamType::SrtCaller, "srt://10.0.0.5"), "Stream.Problem.Host"));

		StreamForm form = srt(StreamType::SrtCaller, "10.0.0.5");
		form.port = 0;
		CHECK(problemIs(form, "Stream.Problem.Port"));
		form.port = 65536;
		CHECK(problemIs(form, "Stream.Problem.Port"));
		form.port = 65535;
		CHECK(problemIs(form, nullptr));

		form.latencyMs = -1;
		CHECK(problemIs(form, "Stream.Problem.Latency"));
		form.latencyMs = 60001;
		CHECK(problemIs(form, "Stream.Problem.Latency"));
		form.latencyMs = 0;
		CHECK(problemIs(form, nullptr));

		/* SRT wants 10 to 79 characters, or none */
		form.passphrase = "123456789";
		CHECK(problemIs(form, "Stream.Problem.Passphrase"));
		form.passphrase = "1234567890";
		CHECK(problemIs(form, nullptr));
		form.passphrase = std::string(79, 'x');
		CHECK(problemIs(form, nullptr));
		form.passphrase = std::string(80, 'x');
		CHECK(problemIs(form, "Stream.Problem.Passphrase"));

		/* a line break cannot be typed in the form; it must not get through another way */
		form.passphrase = "12345\n67890";
		CHECK(problemIs(form, "Stream.Problem.Text"));
		form.passphrase.clear();
		form.streamId = "a\tb";
		CHECK(problemIs(form, "Stream.Problem.Text"));

		/* latency and passphrase are SRT's; they do not stop another type */
		StreamForm udp = srt(StreamType::TsUdp, "");
		udp.latencyMs = -5;
		udp.passphrase = "x";
		CHECK(problemIs(udp, nullptr));

		CHECK(problemIs(srt(StreamType::RtspTcp, ""), "Stream.Problem.Url"));
		CHECK(problemIs(srt(StreamType::RtspTcp, "rtsp://"), "Stream.Problem.Url"));
		CHECK(problemIs(srt(StreamType::RtspTcp, "http://cam/stream"), "Stream.Problem.Url"));
		CHECK(problemIs(srt(StreamType::RtspTcp, "rtsp://cam/a b"), "Stream.Problem.Url"));
		CHECK(problemIs(srt(StreamType::RtspUdp, "rtsp://cam.local/stream1"), nullptr));
	}

	/* the saved names, there and back */
	{
		const StreamType all[] = {StreamType::SrtCaller, StreamType::SrtListener, StreamType::SrtRendezvous,
					  StreamType::RtspUdp,   StreamType::RtspTcp,     StreamType::TsUdp,
					  StreamType::TsTcp,     StreamType::TsTcpPull};
		for (StreamType type : all) {
			CHECK(streamTypeFromId(streamTypeId(type)) == type);
			CHECK(std::strlen(streamTypeId(type)) > 0);
		}
		CHECK(streamTypeFromId("no such type") == StreamType::SrtCaller);
		CHECK(isSrt(StreamType::SrtListener) && !isSrt(StreamType::TsUdp));
		CHECK(isRtsp(StreamType::RtspUdp) && !isRtsp(StreamType::SrtCaller));
	}

	/* two forms are the same when every field is */
	{
		StreamForm a = srt(StreamType::SrtCaller, "h");
		StreamForm b = a;
		CHECK(a == b);
		b.streamId = "x";
		CHECK(!(a == b));
		b = a;
		b.hardwareDecoder = false;
		CHECK(!(a == b));
	}

	if (failures == 0) {
		std::printf("StreamAddress: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
```

- [ ] **Step 4: Add the test to the build**

In `breadstudio/desk/CMakeLists.txt`, at the end of the file, after the `bread-desk-test` lines, add:
```cmake
# The stream address function: no OBS, no Qt.
add_executable(bread-stream-test test/StreamAddressTest.cpp StreamAddress.cpp)
target_include_directories(bread-stream-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(
  bread-stream-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/desk/test-bin" FOLDER plugins/bread-desk
)
```
and in the second `target_sources(bread-desk PRIVATE …)` list, in alphabetical place after `Mixer.hpp`, add:
```cmake
    StreamAddress.cpp
    StreamAddress.hpp
```

- [ ] **Step 5: Run the test and see it fail**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-stream-test
build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-stream-test.exe; $LASTEXITCODE
```
Expected: many `FAIL line …` lines, then `1`.

- [ ] **Step 6: Implement**

Replace `breadstudio/desk/StreamAddress.cpp` with:
```cpp
#include "StreamAddress.hpp"

#include <cstring>

namespace desk {

struct TypeName {
	StreamType type;
	const char *id;
};

static const TypeName typeNames[] = {
	{StreamType::SrtCaller, "srt_caller"}, {StreamType::SrtListener, "srt_listener"},
	{StreamType::SrtRendezvous, "srt_rendezvous"}, {StreamType::RtspUdp, "rtsp_udp"},
	{StreamType::RtspTcp, "rtsp_tcp"},     {StreamType::TsUdp, "ts_udp"},
	{StreamType::TsTcp, "ts_tcp"},         {StreamType::TsTcpPull, "ts_tcp_pull"},
};

bool operator==(const StreamForm &a, const StreamForm &b)
{
	return a.type == b.type && a.host == b.host && a.port == b.port && a.latencyMs == b.latencyMs &&
	       a.passphrase == b.passphrase && a.keyLength == b.keyLength && a.streamId == b.streamId &&
	       a.hardwareDecoder == b.hardwareDecoder;
}

const char *streamTypeId(StreamType type)
{
	for (const TypeName &name : typeNames) {
		if (name.type == type) {
			return name.id;
		}
	}
	return typeNames[0].id;
}

StreamType streamTypeFromId(const std::string &id)
{
	for (const TypeName &name : typeNames) {
		if (id == name.id) {
			return name.type;
		}
	}
	return StreamType::SrtCaller;
}

bool isSrt(StreamType type)
{
	return type == StreamType::SrtCaller || type == StreamType::SrtListener || type == StreamType::SrtRendezvous;
}

bool isRtsp(StreamType type)
{
	return type == StreamType::RtspUdp || type == StreamType::RtspTcp;
}

static std::string trimmed(const std::string &value)
{
	const char *blank = " \t\r\n";
	const size_t first = value.find_first_not_of(blank);
	if (first == std::string::npos) {
		return std::string();
	}
	return value.substr(first, value.find_last_not_of(blank) - first + 1);
}

static bool hasControl(const std::string &value)
{
	for (unsigned char c : value) {
		if (c < 0x20 || c == 0x7F) {
			return true;
		}
	}
	return false;
}

/* a type that connects out needs somewhere to connect to; one that listens does not */
static bool needsHost(StreamType type)
{
	return type == StreamType::SrtCaller || type == StreamType::SrtRendezvous || type == StreamType::TsTcpPull;
}

const char *streamProblem(const StreamForm &form)
{
	const std::string host = trimmed(form.host);

	if (isRtsp(form.type)) {
		static const char *scheme = "rtsp://";
		const bool ok = host.size() > std::strlen(scheme) && host.compare(0, std::strlen(scheme), scheme) == 0 &&
				host.find_first_of(" \t") == std::string::npos && !hasControl(host);
		return ok ? nullptr : "Stream.Problem.Url";
	}

	/* a host is a name or an address, never the start of a path, a query or a login */
	if (host.find_first_of(" \t/?#@") != std::string::npos || hasControl(host) ||
	    host.find("://") != std::string::npos) {
		return "Stream.Problem.Host";
	}
	if (host.empty() && needsHost(form.type)) {
		return "Stream.Problem.Host";
	}
	if (form.port < 1 || form.port > 65535) {
		return "Stream.Problem.Port";
	}

	if (isSrt(form.type)) {
		if (form.latencyMs < 0 || form.latencyMs > 60000) {
			return "Stream.Problem.Latency";
		}
		if (hasControl(form.passphrase) || hasControl(form.streamId)) {
			return "Stream.Problem.Text";
		}
		/* SRT's own rule */
		if (!form.passphrase.empty() && (form.passphrase.size() < 10 || form.passphrase.size() > 79)) {
			return "Stream.Problem.Passphrase";
		}
	}

	return nullptr;
}

/* FFmpeg reads an option's value with av_get_token: between '...' nothing is special, and a
 * quote itself is written '\'' */
static std::string quoted(const std::string &value)
{
	std::string out = "'";
	for (char c : value) {
		if (c == '\'') {
			out += "'\\''";
		} else {
			out += c;
		}
	}
	out += "'";
	return out;
}

static std::string hostPort(const std::string &typed, const char *whenEmpty, int port)
{
	std::string host = trimmed(typed);
	if (host.empty()) {
		host = whenEmpty;
	} else if (host.find(':') != std::string::npos && host.front() != '[') {
		host = "[" + host + "]"; /* an IPv6 address */
	}
	return host + ":" + std::to_string(port);
}

StreamTarget streamTarget(const StreamForm &form)
{
	StreamTarget target;

	switch (form.type) {
	case StreamType::SrtCaller:
	case StreamType::SrtListener:
	case StreamType::SrtRendezvous: {
		const char *mode = form.type == StreamType::SrtCaller     ? "caller"
				   : form.type == StreamType::SrtListener ? "listener"
									  : "rendezvous";
		target.input = "srt://" + hostPort(form.host, "0.0.0.0", form.port);
		target.format = "mpegts";
		/* FFmpeg takes SRT's latency in microseconds */
		target.options = std::string("mode=") + mode + " latency=" + std::to_string((long long)form.latencyMs * 1000);
		if (!form.passphrase.empty()) {
			const int length = (form.keyLength == 16 || form.keyLength == 24) ? form.keyLength : 32;
			target.options += " passphrase=" + quoted(form.passphrase) + " pbkeylen=" + std::to_string(length);
		}
		if (!form.streamId.empty()) {
			target.options += " streamid=" + quoted(form.streamId);
		}
		break;
	}
	case StreamType::RtspUdp:
	case StreamType::RtspTcp:
		target.input = trimmed(form.host);
		target.options = form.type == StreamType::RtspUdp ? "rtsp_transport=udp" : "rtsp_transport=tcp";
		break;
	case StreamType::TsUdp:
		target.input = "udp://" + hostPort(form.host, "@", form.port);
		target.format = "mpegts";
		break;
	case StreamType::TsTcp:
		target.input = "tcp://" + hostPort(form.host, "0.0.0.0", form.port);
		target.format = "mpegts";
		target.options = "listen=1";
		break;
	case StreamType::TsTcpPull:
		target.input = "tcp://" + hostPort(form.host, "0.0.0.0", form.port);
		target.format = "mpegts";
		break;
	}

	return target;
}

} // namespace desk
```

- [ ] **Step 7: Run the test and see it pass**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-stream-test
build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-stream-test.exe; $LASTEXITCODE
```
Expected: `StreamAddress: all checks passed`, then `0`.

- [ ] **Step 8: Commit**

```powershell
git add breadstudio/desk/StreamAddress.hpp breadstudio/desk/StreamAddress.cpp breadstudio/desk/test/StreamAddressTest.cpp breadstudio/desk/CMakeLists.txt
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the stream address function and its test"
```

---

### Task 2: The Input Select window, for today's kinds

**Files:**
- Create: `breadstudio/desk/InputPage.hpp`, `breadstudio/desk/PropertiesPage.hpp`, `breadstudio/desk/PropertiesPage.cpp`, `breadstudio/desk/InputSelect.hpp`, `breadstudio/desk/InputSelect.cpp`
- Modify: `breadstudio/desk/InputBar.cpp`, `breadstudio/desk/InputBar.hpp`, `breadstudio/desk/CMakeLists.txt`, `breadstudio/desk/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `desk::createInput(const char *unversionedId, const char *name, obs_data_t *settings)` → `OBSSource` (null when the type is not on this machine); `desk::toPreview(obs_source_t *)`; `desk::inputsChanged()`; `desk::text(const char *key)` → `QString`; OBS's `OBSPropertiesView` from `shared/properties-view`.
- Produces:
  - `class desk::InputPage : public QWidget` with `virtual bool ready() const` (default true), `virtual OBSSource create() = 0`, `virtual void apply()` and `virtual void revert()` (default: nothing), and the signal `void readyChanged()`.
  - `class desk::PropertiesPage : public InputPage` with `PropertiesPage(const char *unversionedId, const char *nameKey, QWidget *parent)` for a new input.
  - `class desk::InputSelect : public QDialog` with `explicit InputSelect(QWidget *parent)`, `obs_source_t *input() const` (null in this form of the window), and the signal `void inputMade(obs_source_t *input)`.

A `PropertiesPage` for a new input shows OBS's settings panel on a **private stand-in** source of the real type: a source OBS never saves and never puts on air. That way every list and button in the panel works exactly as in OBS's own properties window. On OK the stand-in's settings are copied, the stand-in is destroyed, and only then is the real input made, because a capture device can be open in one source at a time.

- [ ] **Step 1: Write the page interface**

Create `breadstudio/desk/InputPage.hpp`:
```cpp
#pragma once

#include <obs.hpp>

#include <QWidget>

namespace desk {

/* One page of the Input Select window. A page is made either for a new input, and then
 * create() runs on OK, or for the base of an input that exists, and then apply() runs on OK
 * and revert() on Cancel. */
class InputPage : public QWidget {
	Q_OBJECT

public:
	using QWidget::QWidget;

	/* false while OK must stay greyed out */
	virtual bool ready() const { return true; }

	/* Makes the new input and returns it. Null when it could not be made. */
	virtual OBSSource create() = 0;

	/* for an existing input: keep what was changed, or put back what was there */
	virtual void apply() {}
	virtual void revert() {}

signals:
	void readyChanged();
};

} // namespace desk
```

- [ ] **Step 2: Write the settings-panel page**

Create `breadstudio/desk/PropertiesPage.hpp`:
```cpp
#pragma once

#include "InputPage.hpp"

#include <string>

namespace desk {

/* A page that is OBS's own settings panel for one kind of source. */
class PropertiesPage : public InputPage {
	Q_OBJECT

public:
	/* for a new input of that type; `nameKey` is the locale key of the input's name */
	PropertiesPage(const char *unversionedId, const char *nameKey, QWidget *parent);

	OBSSource create() override;

private:
	void build();

	std::string typeId;
	std::string nameKey;
	OBSSource source; /* a private stand-in of the real type; never saved, never on air */
};

} // namespace desk
```

Create `breadstudio/desk/PropertiesPage.cpp`:
```cpp
#include "PropertiesPage.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"

#include <properties-view.hpp>

#include <QLabel>
#include <QVBoxLayout>

namespace desk {

PropertiesPage::PropertiesPage(const char *unversionedId, const char *nameKey_, QWidget *parent)
	: InputPage(parent),
	  typeId(unversionedId),
	  nameKey(nameKey_)
{
	const char *id = obs_get_latest_input_type_id(unversionedId);
	if (id) {
		OBSSourceAutoRelease standIn = obs_source_create_private(id, "bread input select", nullptr);
		source = standIn.Get();
	}
	build();
}

void PropertiesPage::build()
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	if (!source) {
		layout->addWidget(new QLabel(text("InputSelect.Unavailable"), this), 0, Qt::AlignTop);
		return;
	}

	/* the panel changes the source as the operator changes a setting, as OBS's window does */
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	auto *view = new OBSPropertiesView(settings.Get(), source.Get(), (PropertiesReloadCallback)obs_source_properties,
					   (PropertiesUpdateCallback) nullptr,
					   (PropertiesVisualUpdateCb)obs_source_update);
	layout->addWidget(view);
}

OBSSource PropertiesPage::create()
{
	if (!source) {
		return OBSSource();
	}

	OBSDataAutoRelease now = obs_source_get_settings(source);
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_apply(settings, now);

	/* The stand-in goes first, and all the way: a capture device can be open in one
	 * source only. OBS destroys sources on another thread, so wait for it. */
	delete findChild<OBSPropertiesView *>();
	source = nullptr;
	obs_wait_for_destroy_queue();

	return createInput(typeId.c_str(), obs_module_text(nameKey.c_str()), settings);
}

} // namespace desk
```

- [ ] **Step 3: Write the window**

Create `breadstudio/desk/InputSelect.hpp`:
```cpp
#pragma once

#include <obs.hpp>

#include <QDialog>

#include <vector>

class QListWidget;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

namespace desk {

class InputPage;

/* The Input Select window: the kinds of input down the left, one page per kind (spec §3). */
class InputSelect : public QDialog {
	Q_OBJECT

public:
	/* the whole window, to make a new input */
	explicit InputSelect(QWidget *parent);

	/* the input this window is for; null when it makes a new one */
	obs_source_t *input() const { return existing; }

	void accept() override;
	void reject() override;

signals:
	void inputMade(obs_source_t *input);

private:
	void addButtons(QVBoxLayout *outer);
	void showKind();
	void syncOk();
	InputPage *page() const;

	OBSSource existing;
	QListWidget *list = nullptr;
	QStackedWidget *pages = nullptr;
	QPushButton *ok = nullptr;
	std::vector<InputPage *> pageOf; /* by kind; made when the kind is first shown */
};

} // namespace desk
```

Create `breadstudio/desk/InputSelect.cpp`:
```cpp
#include "InputSelect.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "InputPage.hpp"
#include "PropertiesPage.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <iterator>

namespace desk {

namespace {

/* a black input, to put layers on */
class BlankPage : public InputPage {
public:
	explicit BlankPage(QWidget *parent) : InputPage(parent)
	{
		auto *layout = new QVBoxLayout(this);
		layout->addWidget(new QLabel(text("InputSelect.BlankHint"), this), 0, Qt::AlignTop);
	}

	OBSSource create() override
	{
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_int(settings, "color", 0xFF000000);
		return createInput("color_source", obs_module_text("Type.Blank"), settings);
	}
};

/* a screen or a window of this PC (spec §4.4) */
class DesktopPage : public InputPage {
public:
	explicit DesktopPage(QWidget *parent) : InputPage(parent)
	{
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);

		auto *choice = new QHBoxLayout();
		auto *screen = new QRadioButton(text("Type.Screen"), this);
		auto *window = new QRadioButton(text("Type.Window"), this);
		choice->addWidget(screen);
		choice->addWidget(window);
		choice->addStretch(1);
		layout->addLayout(choice);

		stack = new QStackedWidget(this);
		stack->addWidget(new PropertiesPage("monitor_capture", "Type.Screen", stack));
		stack->addWidget(new PropertiesPage("window_capture", "Type.Window", stack));
		layout->addWidget(stack, 1);

		screen->setChecked(true);
		connect(window, &QRadioButton::toggled, this, [this](bool on) { stack->setCurrentIndex(on ? 1 : 0); });
	}

	OBSSource create() override { return static_cast<InputPage *>(stack->currentWidget())->create(); }

private:
	QStackedWidget *stack;
};

/* One row of the list. `typeId` is the OBS source type the row needs: without it the row
 * is left out. */
struct Kind {
	const char *textKey;
	const char *typeId;
	InputPage *(*make)(QWidget *parent);
};

const Kind kinds[] = {
	{"Kind.Video", "ffmpeg_source",
	 [](QWidget *parent) -> InputPage * { return new PropertiesPage("ffmpeg_source", "Type.Media", parent); }},
	{"Kind.Camera", "dshow_input",
	 [](QWidget *parent) -> InputPage * { return new PropertiesPage("dshow_input", "Type.Camera", parent); }},
	{"Kind.Desktop", "monitor_capture", [](QWidget *parent) -> InputPage * { return new DesktopPage(parent); }},
	{"Kind.Image", "image_source",
	 [](QWidget *parent) -> InputPage * { return new PropertiesPage("image_source", "Type.Image", parent); }},
	{"Kind.Colour", "color_source",
	 [](QWidget *parent) -> InputPage * { return new PropertiesPage("color_source", "Type.Colour", parent); }},
	{"Kind.Blank", "color_source", [](QWidget *parent) -> InputPage * { return new BlankPage(parent); }},
	{"Kind.Audio", "wasapi_input_capture",
	 [](QWidget *parent) -> InputPage * {
		 return new PropertiesPage("wasapi_input_capture", "Type.Audio", parent);
	 }},
	{"Kind.Browser", "browser_source",
	 [](QWidget *parent) -> InputPage * { return new PropertiesPage("browser_source", "Type.Browser", parent); }},
};

} // namespace

InputSelect::InputSelect(QWidget *parent) : QDialog(parent), pageOf(std::size(kinds), nullptr)
{
	setWindowTitle(text("InputSelect.Title"));
	resize(1000, 620);

	auto *outer = new QVBoxLayout(this);
	auto *body = new QHBoxLayout();
	outer->addLayout(body, 1);

	list = new QListWidget(this);
	list->setFixedWidth(200);
	body->addWidget(list);

	pages = new QStackedWidget(this);
	body->addWidget(pages, 1);

	/* The list is filled here, not when the plugin loads: by now every plugin has
	 * registered its source types. */
	for (size_t i = 0; i < std::size(kinds); i++) {
		if (!obs_get_latest_input_type_id(kinds[i].typeId)) {
			continue;
		}
		auto *item = new QListWidgetItem(text(kinds[i].textKey), list);
		item->setData(Qt::UserRole, (int)i);
	}

	addButtons(outer);

	connect(list, &QListWidget::currentRowChanged, this, [this](int) { showKind(); });
	list->setCurrentRow(0);
}

void InputSelect::addButtons(QVBoxLayout *outer)
{
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	ok = buttons->button(QDialogButtonBox::Ok);
	connect(buttons, &QDialogButtonBox::accepted, this, &InputSelect::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &InputSelect::reject);
	outer->addWidget(buttons);
}

/* A page is made when its row is first shown, so a camera or a browser is not started for
 * a row the operator never opens. */
void InputSelect::showKind()
{
	QListWidgetItem *item = list->currentItem();
	if (!item) {
		return;
	}

	const size_t kind = (size_t)item->data(Qt::UserRole).toInt();
	if (!pageOf[kind]) {
		pageOf[kind] = kinds[kind].make(pages);
		pages->addWidget(pageOf[kind]);
		connect(pageOf[kind], &InputPage::readyChanged, this, &InputSelect::syncOk);
	}
	pages->setCurrentWidget(pageOf[kind]);
	syncOk();
}

InputPage *InputSelect::page() const
{
	return static_cast<InputPage *>(pages->currentWidget());
}

void InputSelect::syncOk()
{
	InputPage *current = page();
	ok->setEnabled(current && current->ready());
}

void InputSelect::accept()
{
	InputPage *current = page();
	if (!current || !current->ready()) {
		return;
	}

	OBSSource made = current->create();
	if (!made) {
		QMessageBox::warning(this, windowTitle(), text("InputSelect.Failed"));
		return;
	}

	emit inputMade(made);
	QDialog::accept();
}

void InputSelect::reject()
{
	QDialog::reject();
}

} // namespace desk
```

- [ ] **Step 4: Open the window from Add Input**

In `breadstudio/desk/InputBar.cpp`:

Add the include, in alphabetical place after `"InputModel.hpp"`:
```cpp
#include "InputSelect.hpp"
```

Delete the whole block that starts `/* Add Input: one entry per base type that exists on this machine (spec §3) */` and ends with the closing `};` of `static const InputType types[] = { … };` (the `struct InputType` and the `types` table).

Replace this block:
```cpp
	/* filled when it opens: this dock is made while OBS is still loading modules, before
	 * the plugins that own these types have registered them */
	auto *add = new QPushButton(text("Add"), this);
	auto *addMenu = new QMenu(add);
	connect(addMenu, &QMenu::aboutToShow, this, [this, addMenu] {
		addMenu->clear();
		for (const InputType &type : types) {
			if (!obs_get_latest_input_type_id(type.id)) {
				continue;
			}
			QAction *action = addMenu->addAction(text(type.textKey));
			connect(action, &QAction::triggered, this,
				[this, type] { addInput(type.textKey, type.id, type.blank); });
		}
	});
	add->setMenu(addMenu);
	footer->addWidget(add);
```
with:
```cpp
	/* Add Input opens the Input Select window (network inputs spec §3). It blocks the
	 * main window, as vMix's does, but is not run with exec(): clear() must be able to
	 * delete it when the scene collection goes. */
	auto *add = new QPushButton(text("Add"), this);
	connect(add, &QPushButton::clicked, this, [this] {
		auto *window = new InputSelect(settingsParent());
		window->setAttribute(Qt::WA_DeleteOnClose);
		connect(window, &InputSelect::inputMade, this, [](obs_source_t *input) {
			inputsChanged();
			toPreview(input);
		});
		window->open();
	});
	footer->addWidget(add);
```

Delete the whole function `void InputBar::addInput(const char *textKey, const char *typeId, bool blank) { … }`.

In `InputBar::clear()`, make the first line:
```cpp
	qDeleteAll(settingsParent()->findChildren<InputSelect *>());
```
so the function reads:
```cpp
void InputBar::clear()
{
	qDeleteAll(settingsParent()->findChildren<InputSelect *>());
	qDeleteAll(settingsParent()->findChildren<InputSettings *>());
	sources.clear();
	clearTiles();
}
```

In `InputBar::showMenu`, change `QMenu menu(this);` to:
```cpp
	QMenu menu(window()); /* a top-level parent: the dock's content is not one, and Qt says so in the log */
```

In `breadstudio/desk/InputBar.hpp`, delete the line:
```cpp
	void addInput(const char *textKey, const char *typeId, bool blank);
```

- [ ] **Step 5: Add the strings**

In `breadstudio/desk/data/locale/en-US.ini`, append:
```ini
InputSelect.Title="Input Select"
InputSelect.Unavailable="This kind of input is not available on this PC."
InputSelect.Failed="The input could not be made."
InputSelect.BlankHint="A black input, to put layers on."
Kind.Video="Video"
Kind.Camera="Camera"
Kind.Desktop="Desktop Capture"
Kind.Image="Image"
Kind.Colour="Colour"
Kind.Blank="Blank"
Kind.Audio="Audio Input"
Kind.Browser="Web Browser"
```

- [ ] **Step 6: Add the sources and OBS's settings panel to the build**

In `breadstudio/desk/CMakeLists.txt`, after `add_library(OBS::bread-desk ALIAS bread-desk)`, add:
```cmake
if(NOT TARGET OBS::properties-view)
  add_subdirectory("${CMAKE_SOURCE_DIR}/shared/properties-view" "${CMAKE_BINARY_DIR}/shared/properties-view")
endif()
```
Change the link line to:
```cmake
target_link_libraries(bread-desk PRIVATE OBS::frontend-api OBS::libobs OBS::properties-view Qt::Widgets)
```
In the second `target_sources(bread-desk PRIVATE …)` list add, each in alphabetical place:
```cmake
    InputPage.hpp
    InputSelect.cpp
    InputSelect.hpp
    PropertiesPage.cpp
    PropertiesPage.hpp
```

- [ ] **Step 7: Build**

Run: `cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk`
Expected: builds with no warnings. `QMenu` is still included in `InputBar.cpp` for the right-click menu; if the compiler reports an unused include or variable left from the Add menu, remove exactly that.

- [ ] **Step 8: Check it by hand**

Start the program with Studio Mode on and the desk layout. Make the test tone first if it is not there: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\desk\test\make-tone.ps1`.

| Do | Expect |
|---|---|
| Press Add Input | A window titled "Input Select". The main window does not take clicks while it is open. Left list: Video, Camera, Desktop Capture, Image, Colour, Blank, Audio Input, Web Browser. Video is selected and OBS's media settings show on the right. No other window opens. |
| Colour: change the colour to red; OK | The window closes. A new red tile, last, on Preview, named "Colour". |
| Add Input; Blank; OK | A new black tile named "Blank". |
| Add Input; Video: browse to `bread-tone.wav`, tick Loop; OK | A new tile "Media file". The mixer has a strip for it, and its meter moves when it is on Program. |
| Add Input; Desktop Capture: Window; pick a window; OK | A new tile "Window" showing that window. |
| Add Input; Desktop Capture: leave Screen; OK | A new tile "Screen" showing the screen. |
| Add Input; Audio Input; OK | A new tile "Audio device", and a mixer strip with AFV unlit. |
| Add Input; Web Browser; OK | A new tile "Browser page" showing the default page. |
| Add Input; Camera; OK, on a PC with no camera | A new tile "Camera", black. Nothing crashes. |
| Add Input; click every row top to bottom, then Cancel | No new tile. |
| Right-click a tile | The Rename and Delete menu still opens and works. |
| Close the program; read the newest log | `Number of memory leaks: 0`. No line containing `remaining`. No line containing `must be a top level window`. |

The collection switched under an open window (Review Focus 3) cannot be done from the main window, which is blocked. Do it through OBS's websocket: Tools → WebSocket Server Settings → Enable WebSocket server, Enable Authentication unticked, Apply. Open Add Input and select the Camera row. Then, in PowerShell:
```powershell
$ws = New-Object Net.WebSockets.ClientWebSocket; $ct = [Threading.CancellationToken]::None
$ws.ConnectAsync([Uri]'ws://127.0.0.1:4455', $ct).Wait()
function Say($o) { $b = [Text.Encoding]::UTF8.GetBytes(($o | ConvertTo-Json -Depth 5 -Compress)); $ws.SendAsync((New-Object ArraySegment[byte] (, $b)), 'Text', $true, $ct).Wait() }
function Hear { $buf = New-Object byte[] 65536; [void]$ws.ReceiveAsync((New-Object ArraySegment[byte] (, $buf)), $ct).Result }
Hear; Say @{ op = 1; d = @{ rpcVersion = 1 } }; Hear
Say @{ op = 6; d = @{ requestType = 'CreateSceneCollection'; requestId = '1'; requestData = @{ sceneCollectionName = 'Under the window' } } }; Hear
```
Expected: the Input Select window closes by itself and the bar shows the new collection's one input. Switch back with Scene Collection → the first one, close the program: the log has `Number of memory leaks: 0` and no line containing `remaining`. Untick Enable WebSocket server afterwards.

- [ ] **Step 9: Commit**

```powershell
git add breadstudio/desk
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: replace the Add menu with the Input Select window"
```

---

### Task 3: The Stream / SRT page

**Files:**
- Create: `breadstudio/desk/StreamPage.hpp`, `breadstudio/desk/StreamPage.cpp`
- Modify: `breadstudio/desk/InputSelect.cpp`, `breadstudio/desk/CMakeLists.txt`, `breadstudio/desk/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `StreamForm`, `StreamType`, `streamProblem`, `streamTarget`, `streamTypeId`, `streamTypeFromId`, `isSrt`, `isRtsp` from Task 1; `InputPage` from Task 2; `desk::createInput`, `desk::baseSource(obs_source_t *input)` → `OBSSource`.
- Produces: `class desk::StreamPage : public InputPage` with `explicit StreamPage(QWidget *parent)` for a new input, and `static bool isStream(obs_source_t *base)`. The form is saved in the base source's private settings under the object `bread_stream`, with the fields `type` (string, from `streamTypeId`), `host`, `port`, `latency_ms`, `passphrase`, `key_length`, `stream_id`, `hardware_decoder`.

What the Media Source is given (its setting names are OBS's): `is_local_file` false; `input`, `input_format` and `ffmpeg_options` from `streamTarget()`; `reconnect_delay_sec` 2; `hw_decode` from the form; `restart_on_activate`, `close_when_inactive` and `clear_on_media_end` false, so the stream keeps running while it is not on Program and the last picture is not wiped.

The passphrase is saved in the scene collection file as text, as OBS saves stream addresses and keys.

- [ ] **Step 1: Write the page**

Create `breadstudio/desk/StreamPage.hpp`:
```cpp
#pragma once

#include "InputPage.hpp"
#include "StreamAddress.hpp"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace desk {

/* The Stream / SRT page: a network stream played by OBS's Media Source (spec §5). */
class StreamPage : public InputPage {
	Q_OBJECT

public:
	/* for a new stream input */
	explicit StreamPage(QWidget *parent);

	bool ready() const override;
	OBSSource create() override;

	/* true when this source is the base of a stream input made on this page */
	static bool isStream(obs_source_t *base);

protected:
	void buildForm();
	StreamForm form() const;
	void setForm(const StreamForm &form);

	/* the Media Source settings for a form, and the form kept with the source */
	static OBSDataAutoRelease mediaSettings(const StreamForm &form);
	static void saveForm(obs_source_t *base, const StreamForm &form);
	static bool loadForm(obs_source_t *base, StreamForm &form);

	QFormLayout *fields = nullptr;

private:
	void typeChanged();
	void formChanged();

	QComboBox *type = nullptr;
	QLabel *hostLabel = nullptr;
	QLineEdit *host = nullptr;
	QSpinBox *port = nullptr;
	QSpinBox *latency = nullptr;
	QLineEdit *passphrase = nullptr;
	QComboBox *keyLength = nullptr;
	QLineEdit *streamId = nullptr;
	QCheckBox *hardware = nullptr;
	QLabel *problem = nullptr;
};

} // namespace desk
```

Create `breadstudio/desk/StreamPage.cpp`:
```cpp
#include "StreamPage.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <string>

namespace desk {

static const char *FormKey = "bread_stream";

/* in vMix's order */
static const StreamType typeOrder[] = {
	StreamType::RtspUdp,   StreamType::RtspTcp,     StreamType::TsUdp,         StreamType::TsTcp,
	StreamType::TsTcpPull, StreamType::SrtCaller,   StreamType::SrtListener,   StreamType::SrtRendezvous,
};

StreamPage::StreamPage(QWidget *parent) : InputPage(parent)
{
	buildForm();
	setForm(StreamForm());
}

void StreamPage::buildForm()
{
	auto *layout = new QVBoxLayout(this);
	fields = new QFormLayout();
	layout->addLayout(fields);

	type = new QComboBox(this);
	for (StreamType each : typeOrder) {
		const std::string key = std::string("Stream.Type.") + streamTypeId(each);
		type->addItem(text(key.c_str()), QString::fromUtf8(streamTypeId(each)));
	}
	fields->addRow(text("Stream.Type"), type);

	hostLabel = new QLabel(text("Stream.Host"), this);
	host = new QLineEdit(this);
	fields->addRow(hostLabel, host);

	port = new QSpinBox(this);
	port->setRange(1, 65535);
	fields->addRow(text("Stream.Port"), port);

	latency = new QSpinBox(this);
	latency->setRange(0, 60000);
	fields->addRow(text("Stream.Latency"), latency);

	passphrase = new QLineEdit(this);
	passphrase->setEchoMode(QLineEdit::PasswordEchoOnEdit);
	passphrase->setMaxLength(79);
	fields->addRow(text("Stream.Passphrase"), passphrase);

	keyLength = new QComboBox(this);
	for (int length : {16, 24, 32}) {
		keyLength->addItem(QString::number(length), length);
	}
	fields->addRow(text("Stream.KeyLength"), keyLength);

	streamId = new QLineEdit(this);
	fields->addRow(text("Stream.StreamId"), streamId);

	hardware = new QCheckBox(text("Stream.Hardware"), this);
	fields->addRow(QString(), hardware);

	/* why OK is greyed out, when it is */
	problem = new QLabel(this);
	problem->setWordWrap(true);
	layout->addWidget(problem);
	layout->addStretch(1);

	connect(type, &QComboBox::currentIndexChanged, this, [this](int) { typeChanged(); });
	connect(host, &QLineEdit::textChanged, this, [this] { formChanged(); });
	connect(port, &QSpinBox::valueChanged, this, [this](int) { formChanged(); });
	connect(latency, &QSpinBox::valueChanged, this, [this](int) { formChanged(); });
	connect(passphrase, &QLineEdit::textChanged, this, [this] { formChanged(); });
	connect(streamId, &QLineEdit::textChanged, this, [this] { formChanged(); });
}

StreamForm StreamPage::form() const
{
	StreamForm result;
	result.type = streamTypeFromId(type->currentData().toString().toStdString());
	result.host = host->text().toStdString();
	result.port = port->value();
	result.latencyMs = latency->value();
	result.passphrase = passphrase->text().toStdString();
	result.keyLength = keyLength->currentData().toInt();
	result.streamId = streamId->text().toStdString();
	result.hardwareDecoder = hardware->isChecked();
	return result;
}

void StreamPage::setForm(const StreamForm &value)
{
	type->setCurrentIndex(std::max(0, type->findData(QString::fromUtf8(streamTypeId(value.type)))));
	host->setText(QString::fromStdString(value.host));
	port->setValue(value.port);
	latency->setValue(value.latencyMs);
	passphrase->setText(QString::fromStdString(value.passphrase));
	keyLength->setCurrentIndex(std::max(0, keyLength->findData(value.keyLength)));
	streamId->setText(QString::fromStdString(value.streamId));
	hardware->setChecked(value.hardwareDecoder);
	typeChanged();
}

/* each type shows the fields it uses (spec §5.1) */
void StreamPage::typeChanged()
{
	const StreamType now = streamTypeFromId(type->currentData().toString().toStdString());
	const bool srt = isSrt(now);
	const bool rtsp = isRtsp(now);

	hostLabel->setText(text(rtsp ? "Stream.Url" : "Stream.Host"));
	fields->setRowVisible(port, !rtsp);
	fields->setRowVisible(latency, srt);
	fields->setRowVisible(passphrase, srt);
	fields->setRowVisible(keyLength, srt);
	fields->setRowVisible(streamId, srt);

	formChanged();
}

void StreamPage::formChanged()
{
	const char *why = streamProblem(form());
	/* an empty form is not a mistake yet: say nothing until something is typed */
	problem->setText(why && !host->text().isEmpty() ? text(why) : QString());
	emit readyChanged();
}

bool StreamPage::ready() const
{
	return streamProblem(form()) == nullptr;
}

OBSDataAutoRelease StreamPage::mediaSettings(const StreamForm &value)
{
	const StreamTarget target = streamTarget(value);

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", false);
	obs_data_set_string(settings, "input", target.input.c_str());
	obs_data_set_string(settings, "input_format", target.format.c_str());
	obs_data_set_string(settings, "ffmpeg_options", target.options.c_str());
	obs_data_set_int(settings, "reconnect_delay_sec", 2);
	obs_data_set_bool(settings, "hw_decode", value.hardwareDecoder);
	/* keep running while not on Program, so a cut to it has no start-up delay */
	obs_data_set_bool(settings, "restart_on_activate", false);
	obs_data_set_bool(settings, "close_when_inactive", false);
	obs_data_set_bool(settings, "clear_on_media_end", false);
	return settings;
}

void StreamPage::saveForm(obs_source_t *base, const StreamForm &value)
{
	OBSDataAutoRelease saved = obs_data_create();
	obs_data_set_string(saved, "type", streamTypeId(value.type));
	obs_data_set_string(saved, "host", value.host.c_str());
	obs_data_set_int(saved, "port", value.port);
	obs_data_set_int(saved, "latency_ms", value.latencyMs);
	obs_data_set_string(saved, "passphrase", value.passphrase.c_str());
	obs_data_set_int(saved, "key_length", value.keyLength);
	obs_data_set_string(saved, "stream_id", value.streamId.c_str());
	obs_data_set_bool(saved, "hardware_decoder", value.hardwareDecoder);

	OBSDataAutoRelease keep = obs_source_get_private_settings(base);
	obs_data_set_obj(keep, FormKey, saved);
}

bool StreamPage::loadForm(obs_source_t *base, StreamForm &value)
{
	if (!base) {
		return false;
	}

	OBSDataAutoRelease keep = obs_source_get_private_settings(base);
	OBSDataAutoRelease saved = obs_data_get_obj(keep, FormKey);
	if (!saved) {
		return false;
	}

	value.type = streamTypeFromId(obs_data_get_string(saved, "type"));
	value.host = obs_data_get_string(saved, "host");
	value.port = (int)obs_data_get_int(saved, "port");
	value.latencyMs = (int)obs_data_get_int(saved, "latency_ms");
	value.passphrase = obs_data_get_string(saved, "passphrase");
	value.keyLength = (int)obs_data_get_int(saved, "key_length");
	value.streamId = obs_data_get_string(saved, "stream_id");
	value.hardwareDecoder = obs_data_get_bool(saved, "hardware_decoder");
	return true;
}

bool StreamPage::isStream(obs_source_t *base)
{
	StreamForm unused;
	return loadForm(base, unused);
}

OBSSource StreamPage::create()
{
	const StreamForm value = form();
	if (streamProblem(value)) {
		return OBSSource();
	}

	OBSDataAutoRelease settings = mediaSettings(value);
	OBSSource input = createInput("ffmpeg_source", obs_module_text("Type.Stream"), settings);
	if (input) {
		OBSSource base = baseSource(input);
		if (base) {
			saveForm(base, value);
		}
	}
	return input;
}

} // namespace desk
```

- [ ] **Step 2: Add the row**

In `breadstudio/desk/InputSelect.cpp`, add the include after `"PropertiesPage.hpp"`:
```cpp
#include "StreamPage.hpp"
```
and in the `kinds` table, after the `Kind.Desktop` row, add:
```cpp
	{"Kind.Stream", "ffmpeg_source", [](QWidget *parent) -> InputPage * { return new StreamPage(parent); }},
```

- [ ] **Step 3: Add the strings**

In `breadstudio/desk/data/locale/en-US.ini`, append:
```ini
Kind.Stream="Stream / SRT"
Type.Stream="Stream"
Stream.Type="Stream Type"
Stream.Type.rtsp_udp="RTSP over UDP"
Stream.Type.rtsp_tcp="RTSP over TCP"
Stream.Type.ts_udp="Transport Stream over UDP"
Stream.Type.ts_tcp="Transport Stream over TCP"
Stream.Type.ts_tcp_pull="Transport Stream over TCP Pull"
Stream.Type.srt_caller="SRT (Caller)"
Stream.Type.srt_listener="SRT (Listener)"
Stream.Type.srt_rendezvous="SRT (Rendezvous)"
Stream.Host="Hostname"
Stream.Url="URL"
Stream.Port="Port"
Stream.Latency="Latency (ms)"
Stream.Passphrase="Passphrase"
Stream.KeyLength="Key Length"
Stream.StreamId="Stream ID"
Stream.Hardware="Use Hardware Decoder"
Stream.Problem.Host="Type the sender's name or address, with nothing after it."
Stream.Problem.Url="Type the whole address, starting with rtsp://"
Stream.Problem.Port="The port must be between 1 and 65535."
Stream.Problem.Latency="The latency must be between 0 and 60000 ms."
Stream.Problem.Passphrase="An SRT passphrase has 10 to 79 characters."
Stream.Problem.Text="The passphrase and the stream ID must be one line of plain text."
```

- [ ] **Step 4: Add the sources to the build**

In `breadstudio/desk/CMakeLists.txt`, in the second `target_sources(bread-desk PRIVATE …)` list, after `StreamAddress.hpp`, add:
```cmake
    StreamPage.cpp
    StreamPage.hpp
```

- [ ] **Step 5: Build**

Run: `cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk`
Expected: builds with no warnings.

- [ ] **Step 6: Check it by hand**

These need an `ffmpeg.exe` that speaks SRT: `ffmpeg -hide_banner -protocols | findstr srt` prints `srt`. Each sender below is one line in its own PowerShell window; stop it with `q`. The picture is a test card with a moving counter and a 440 Hz tone.

Sender A, calls BreadStudio on port 5000, no passphrase:
```powershell
ffmpeg -hide_banner -re -f lavfi -i testsrc=size=1280x720:rate=30 -f lavfi -i sine=frequency=440 -c:v libx264 -preset ultrafast -tune zerolatency -g 30 -c:a aac -f mpegts "srt://127.0.0.1:5000?mode=caller"
```
Sender B, listens on port 5001 with a passphrase that has spaces in it. The passphrase is given as an option, not in the address, so nothing depends on how the address is decoded:
```powershell
ffmpeg -hide_banner -re -f lavfi -i testsrc=size=1280x720:rate=30 -f lavfi -i sine=frequency=440 -c:v libx264 -preset ultrafast -tune zerolatency -g 30 -c:a aac -f mpegts -passphrase "bread and butter" -pbkeylen 16 "srt://127.0.0.1:5001?mode=listener"
```
Sender C, a transport stream over UDP to port 5002:
```powershell
ffmpeg -hide_banner -re -f lavfi -i testsrc=size=1280x720:rate=30 -f lavfi -i sine=frequency=440 -c:v libx264 -preset ultrafast -tune zerolatency -g 30 -c:a aac -f mpegts "udp://127.0.0.1:5002?pkt_size=1316"
```
Sender D, an RTSP server on port 8554:
```powershell
ffmpeg -hide_banner -re -f lavfi -i testsrc=size=1280x720:rate=30 -f lavfi -i sine=frequency=440 -c:v libx264 -preset ultrafast -tune zerolatency -g 30 -c:a aac -f rtsp -rtsp_flags listen rtsp://127.0.0.1:8554/live
```

| Do | Expect |
|---|---|
| Add Input; Stream / SRT | The form as in spec §5.1. Stream Type is SRT (Caller), Port 5000, Latency 200, Key Length 32, Use Hardware Decoder ticked. OK is greyed out and no message shows. |
| Type `a b` in Hostname | The line "Type the sender's name or address, with nothing after it." OK stays greyed out. |
| Choose each Stream Type in turn | RTSP types: the first field is labelled URL, and Port, Latency, Passphrase, Key Length and Stream ID are gone. Transport Stream types: Hostname and Port only. SRT types: every field. |
| SRT (Listener), Hostname empty, Port 5000; OK. Then start sender A. | A new tile "Stream", last, on Preview. Within about five seconds of the sender starting, the tile shows the test card and the counter runs. The mixer has a strip for it. |
| Stop sender A, wait ten seconds, start it again | The tile freezes on the last picture, then runs again within about five seconds of the restart. Nothing was touched. |
| Start sender B. Add Input; Stream / SRT; SRT (Caller), Hostname `127.0.0.1`, Port 5001, Passphrase `bread and butter`, Key Length 16, Stream ID `cam1`; OK | A second stream tile with the picture. (The passphrase has spaces: it proves the quoting of Task 1 against a real receiver.) |
| The same again with Passphrase `bread and butterX` | A tile that stays black. The program stays responsive. Delete that tile. |
| Start sender C. Add Input; Stream / SRT; Transport Stream over UDP, Hostname empty, Port 5002; OK | A tile with the picture. |
| Start sender D. Add Input; Stream / SRT; RTSP over TCP, URL `rtsp://127.0.0.1:8554/live`; OK | A tile with the picture. |
| Add Input; Stream / SRT; SRT (Listener), Port 5999; OK. Nobody calls it. Right-click that tile → Delete → Yes | The tile goes at once. The program does not freeze. |
| Add another listener on Port 5999 that nobody calls, then close the program | It closes within five seconds. The log has `Number of memory leaks: 0` and no line containing `remaining`. |
| With a stream tile showing a picture, close and restart the program | The tile comes back and the picture returns once its sender is running. |

- [ ] **Step 7: Commit**

```powershell
git add breadstudio/desk
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the Stream / SRT page"
```

---

### Task 4: The same window for an input that exists

**Files:**
- Modify: `breadstudio/desk/PropertiesPage.hpp`, `breadstudio/desk/PropertiesPage.cpp`, `breadstudio/desk/StreamPage.hpp`, `breadstudio/desk/StreamPage.cpp`, `breadstudio/desk/InputSelect.hpp`, `breadstudio/desk/InputSelect.cpp`, `breadstudio/desk/InputSettings.cpp`, `breadstudio/desk/InputBar.cpp`, `breadstudio/desk/data/locale/en-US.ini`

**Interfaces:**
- Consumes: everything Tasks 2 and 3 produce; `desk::baseSource`.
- Produces:
  - `PropertiesPage(obs_source_t *existing, QWidget *parent)`: the panel on a real source; `revert()` puts its settings back as they were when the page opened.
  - `StreamPage(obs_source_t *base, QWidget *parent)`: the form filled from the saved values, with a status box; `apply()` writes a changed form to the source.
  - `InputSelect(obs_source_t *input, QWidget *parent)`: one page, no list; `input()` returns that input.

For an existing source the settings panel changes the source as the operator changes a setting, as OBS's own properties window does, and Cancel puts back what was there. The stream form changes nothing until OK, because every change to a network Media Source restarts the stream.

- [ ] **Step 1: The settings panel on a real source**

In `breadstudio/desk/PropertiesPage.hpp`, add to the public section after the first constructor:
```cpp
	/* for a source that exists: the base of an input */
	PropertiesPage(obs_source_t *existing, QWidget *parent);
```
add after `OBSSource create() override;`:
```cpp
	void revert() override;
```
and change the private data to:
```cpp
	std::string typeId;
	std::string nameKey;
	/* for a new input: a private stand-in of the real type, never saved, never on air.
	 * For an existing one: the source itself. */
	OBSSource source;
	OBSData before; /* an existing source's settings when the page opened; null for a new input */
```

In `breadstudio/desk/PropertiesPage.cpp`, add after the first constructor:
```cpp
PropertiesPage::PropertiesPage(obs_source_t *existing, QWidget *parent) : InputPage(parent), source(existing)
{
	if (source) {
		OBSDataAutoRelease now = obs_source_get_settings(source);
		OBSDataAutoRelease copy = obs_data_create();
		obs_data_apply(copy, now);
		before = copy.Get();
	}
	build();
}
```
change the start of `create()` to refuse an existing source:
```cpp
OBSSource PropertiesPage::create()
{
	if (!source || before) {
		return OBSSource(); /* nothing to make: no such type, or the page is for a source that exists */
	}
```
and add at the end of the file, before the closing `} // namespace desk`:
```cpp
/* as OBS's own properties window does on Cancel */
void PropertiesPage::revert()
{
	if (!source || !before) {
		return;
	}

	OBSDataAutoRelease settings = obs_source_get_settings(source);
	obs_data_clear(settings);
	obs_data_apply(settings, before);
	obs_source_update(source, before);
}
```

- [ ] **Step 2: The stream form on a real source, with its status box**

In `breadstudio/desk/StreamPage.hpp`, add `#include <QTimer>` after the two project includes, add `class QPlainTextEdit;` to the forward declarations, add to the public section after the first constructor:
```cpp
	/* for a stream input that exists */
	StreamPage(obs_source_t *base, QWidget *parent);
```
add after `OBSSource create() override;`:
```cpp
	void apply() override;
```
and add to the private section:
```cpp
	void pollState();

	OBSSource base;    /* null for a new input */
	StreamForm before; /* the form when the page opened */
	QPlainTextEdit *status = nullptr;
	int lastState = -1;
	QTimer timer;
```

In `breadstudio/desk/StreamPage.cpp`, add the includes `<QPlainTextEdit>`, `<QPushButton>` and `<QTime>` in alphabetical place, and add after the first constructor:
```cpp
StreamPage::StreamPage(obs_source_t *base_, QWidget *parent) : InputPage(parent), base(base_)
{
	buildForm();
	loadForm(base, before);
	setForm(before);

	/* the status box: this input's connection, as it changes (spec §5.3) */
	auto *layout = static_cast<QVBoxLayout *>(this->layout());
	status = new QPlainTextEdit(this);
	status->setReadOnly(true);
	layout->insertWidget(layout->count() - 1, status, 10);
	auto *clear = new QPushButton(text("Stream.Clear"), this);
	connect(clear, &QPushButton::clicked, status, &QPlainTextEdit::clear);
	layout->insertWidget(layout->count() - 1, clear, 0, Qt::AlignRight);

	connect(&timer, &QTimer::timeout, this, &StreamPage::pollState);
	timer.start(500);
	pollState();
}

void StreamPage::pollState()
{
	const int state = (int)obs_source_media_get_state(base);
	if (state == lastState) {
		return;
	}
	lastState = state;

	const char *key = "Stream.State.Stopped";
	switch ((enum obs_media_state)state) {
	case OBS_MEDIA_STATE_OPENING:
		key = "Stream.State.Opening";
		break;
	case OBS_MEDIA_STATE_BUFFERING:
		key = "Stream.State.Buffering";
		break;
	case OBS_MEDIA_STATE_PLAYING:
		key = "Stream.State.Playing";
		break;
	case OBS_MEDIA_STATE_ENDED:
		key = "Stream.State.Lost";
		break;
	case OBS_MEDIA_STATE_ERROR:
		key = "Stream.State.Error";
		break;
	default:
		break;
	}
	status->appendPlainText(QString("%1  %2").arg(QTime::currentTime().toString("HH:mm:ss"), text(key)));
}

/* Every change to a network Media Source restarts the stream, so nothing is written unless
 * the form changed. */
void StreamPage::apply()
{
	const StreamForm value = form();
	if (!base || streamProblem(value) || value == before) {
		return;
	}

	OBSDataAutoRelease settings = mediaSettings(value);
	obs_source_update(base, settings);
	saveForm(base, value);
	before = value;
}
```
and change the start of `create()` to refuse an existing source:
```cpp
OBSSource StreamPage::create()
{
	const StreamForm value = form();
	if (base || streamProblem(value)) {
		return OBSSource(); /* nothing to make: the page is for a source that exists, or the form is not ready */
	}
```
The `layout()->count() - 1` position is before the stretch that `buildForm()` adds last; the status box's larger stretch factor gives it nearly all the free space.

- [ ] **Step 3: The one-page window**

In `breadstudio/desk/InputSelect.hpp`, add after the first constructor:
```cpp
	/* one page and no list, for the base of an input that exists */
	InputSelect(obs_source_t *input, QWidget *parent);
```

In `breadstudio/desk/InputSelect.cpp`, add after the first constructor:
```cpp
InputSelect::InputSelect(obs_source_t *input, QWidget *parent) : QDialog(parent), existing(input)
{
	setWindowTitle(text("Settings.Title").arg(QString::fromUtf8(obs_source_get_name(existing))));
	resize(760, 620);

	auto *outer = new QVBoxLayout(this);
	pages = new QStackedWidget(this);
	outer->addWidget(pages, 1);

	OBSSource base = baseSource(existing);
	InputPage *only = StreamPage::isStream(base) ? static_cast<InputPage *>(new StreamPage(base.Get(), pages))
						     : static_cast<InputPage *>(new PropertiesPage(base.Get(), pages));
	pages->addWidget(only);
	connect(only, &InputPage::readyChanged, this, &InputSelect::syncOk);

	addButtons(outer);
	syncOk();
}
```
and replace `accept()` and `reject()` with:
```cpp
void InputSelect::accept()
{
	InputPage *current = page();
	if (!current || !current->ready()) {
		return;
	}

	if (existing) {
		current->apply();
		QDialog::accept();
		return;
	}

	OBSSource made = current->create();
	if (!made) {
		QMessageBox::warning(this, windowTitle(), text("InputSelect.Failed"));
		return;
	}

	emit inputMade(made);
	QDialog::accept();
}

void InputSelect::reject()
{
	if (existing) {
		if (InputPage *current = page()) {
			current->revert();
		}
	}
	QDialog::reject();
}
```
Deleting the window without Cancel (the scene collection going, Step 5) reverts nothing: the source is on its way out.

- [ ] **Step 4: "Base properties…" opens it**

In `breadstudio/desk/InputSettings.cpp`, add the include after `"InputModel.hpp"`:
```cpp
#include "InputSelect.hpp"
```
and replace the base button's handler:
```cpp
	connect(baseButton, &QPushButton::clicked, this, [this] {
		OBSSource base = baseSource(source);
		if (base) {
			obs_frontend_open_source_properties(base);
		}
	});
```
with:
```cpp
	/* the page this input was made on, filled in (network inputs spec §3.2) */
	connect(baseButton, &QPushButton::clicked, this, [this] {
		if (!baseSource(source)) {
			return;
		}
		auto *window = new InputSelect(source, parentWidget());
		window->setAttribute(Qt::WA_DeleteOnClose);
		window->open();
	});
```
If `obs-frontend-api.h` is then used by nothing else in that file, leave the include: `toPreview` in the same file's constructor does not need it, but removing an include is not this task.

- [ ] **Step 5: Close the window when its input goes**

In `breadstudio/desk/InputBar.cpp`, in `InputBar::refresh()`, after the loop that closes the settings windows of inputs that no longer exist, add:
```cpp
	/* the same for a base's page */
	for (InputSelect *open : settingsParent()->findChildren<InputSelect *>()) {
		const bool alive = !open->input() ||
				   std::any_of(sources.begin(), sources.end(),
					       [open](const OBSSource &input) { return input.Get() == open->input(); });
		if (!alive) {
			delete open;
		}
	}
```

- [ ] **Step 6: Add the strings**

In `breadstudio/desk/data/locale/en-US.ini`, append:
```ini
Stream.Clear="Clear"
Stream.State.Opening="Connecting"
Stream.State.Buffering="Buffering"
Stream.State.Playing="Playing"
Stream.State.Lost="Lost; trying again"
Stream.State.Error="Error"
Stream.State.Stopped="Stopped"
```

- [ ] **Step 7: Build**

Run: `cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk`
Expected: builds with no warnings.

- [ ] **Step 8: Check it by hand**

Use the senders of Task 3 Step 6. Make these inputs with Add Input: `Colour` (red), `Media file` (`bread-tone.wav`), `Stream` (SRT Listener, port 5000). In OBS's Scenes dock (Docks → Scenes) also make an empty scene `Empty`, and a scene `Grouped` holding a group with a text source in it. Hide the Scenes dock again.

| Do | Expect |
|---|---|
| ⚙ on `Colour`; Base properties… | A window titled "Input: Colour" with OBS's colour settings and no list on the left. |
| Change the colour to blue | The tile turns blue at once. |
| Cancel | The tile is red again. |
| Base properties… again; blue; OK | The tile stays blue. Close and restart the program: still blue. |
| ⚙ on `Stream`; Base properties… | "Input: Stream" with the form: SRT (Listener), Port 5000. Under it the status box with a first line, a time and "Connecting". |
| Start sender A | A new line "Playing" within about five seconds. |
| Stop sender A | A new line "Lost; trying again", then "Connecting". |
| Press Clear | The box is empty. The next change of state adds a line. |
| Change nothing; OK. Start sender A if it is stopped. | The picture does not blink: nothing was written. |
| Base properties… again; change Port to 5003; OK. Restart sender A calling port 5003 (change `5000` to `5003` in its line). | The picture comes on port 5003. Base properties… shows Port 5003. |
| Base properties… ; type `a b` in Hostname | OK is greyed out and the message shows. Cancel. |
| ⚙ on `Empty` | "Base properties…" is greyed out, as before this plan. |
| ⚙ on `Grouped`; Base properties… | A window opens with whatever settings OBS has for a group, which may be none. OK and Cancel both close it. Nothing crashes. |
| In stock OBS's way, make a network Media Source: Docks → Sources, add a Media Source to a new scene `Stock stream`, untick Local File, Input `udp://@:5002`. Then ⚙ on `Stock stream`; Base properties… | OBS's media settings panel, not the stream form: this base has no saved form. It works as in OBS. |
| Close the program with one input of each kind in the collection; read the log | `Number of memory leaks: 0`. No line containing `remaining`. |

An input deleted under its open window (Review Focus 3) cannot be done from the main window, which is blocked. Do it through OBS's websocket: Tools → WebSocket Server Settings → Enable WebSocket server, Enable Authentication unticked, Apply. Make sure `Colour` is not on Program. Open ⚙ on `Colour`, then Base properties…, and with "Input: Colour" open, in PowerShell:
```powershell
$ws = New-Object Net.WebSockets.ClientWebSocket; $ct = [Threading.CancellationToken]::None
$ws.ConnectAsync([Uri]'ws://127.0.0.1:4455', $ct).Wait()
function Say($o) { $b = [Text.Encoding]::UTF8.GetBytes(($o | ConvertTo-Json -Depth 5 -Compress)); $ws.SendAsync((New-Object ArraySegment[byte] (, $b)), 'Text', $true, $ct).Wait() }
function Hear { $buf = New-Object byte[] 65536; [void]$ws.ReceiveAsync((New-Object ArraySegment[byte] (, $buf)), $ct).Result }
Hear; Say @{ op = 1; d = @{ rpcVersion = 1 } }; Hear
Say @{ op = 6; d = @{ requestType = 'RemoveScene'; requestId = '1'; requestData = @{ sceneName = 'Colour' } } }; Hear
```
Expected: "Input: Colour" and the layers window behind it both close by themselves, and the tile is gone. Close the program: the log has `Number of memory leaks: 0` and no line containing `remaining`. Untick Enable WebSocket server afterwards.

- [ ] **Step 9: Commit**

```powershell
git add breadstudio/desk
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: open a base's settings in the Input Select window"
```

---

### Task 5: The smoke checklist

**Files:**
- Modify: `breadstudio/docs/DESK-SMOKE.md`

**Interfaces:**
- Consumes: the behaviour of Tasks 2 to 4.
- Produces: section E of the checklist, run after every upstream merge with the rest.

- [ ] **Step 1: Bring the Add wording up to date**

In `breadstudio/docs/DESK-SMOKE.md`, in section A item 1, change `With + Add alone, make:` to `With Add Input alone, make:`.

- [ ] **Step 2: Add section E**

In `breadstudio/docs/DESK-SMOKE.md`, insert before the line `## Before a run`:
```markdown
## E. Input Select and streams

Spec: `2026-10-14-breadstudio-network-inputs-design.md`. Needs an `ffmpeg.exe`
that speaks SRT; the sender lines are in
`2026-10-14-breadstudio-input-select.md`, Task 3 Step 6.

1. Add Input opens the Input Select window. Every row makes its kind of
   input, and no other window opens. Visiting every row and pressing Cancel
   makes nothing.
2. An SRT listener input shows a caller's picture within five seconds, and
   again within five seconds of that caller being restarted.
3. An SRT caller input with a passphrase that has a space in it, a key
   length of 16 and a stream ID shows the listener's picture. With one
   character of the passphrase wrong it stays black and the program stays
   responsive.
4. A Transport Stream over UDP input and an RTSP over TCP input each show
   their sender's picture.
5. ⚙, Base properties… on a stream input shows the form filled in and the
   status box. OK with nothing changed does not interrupt the picture. A
   changed port takes effect.
6. ⚙, Base properties… on a Colour input: a change shows at once, and Cancel
   puts the old colour back.
7. A listener input nobody calls can be deleted at once, and the program
   closes within five seconds with one in the collection.
8. After closing with one input of each kind in the collection, the log has
   `Number of memory leaks: 0`, no line containing `remaining`, and none
   containing `must be a top level window`.

```

- [ ] **Step 3: Run both automated tests once more**

```powershell
.\breadstudio\build.ps1
build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-desk-test.exe; $LASTEXITCODE
build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-stream-test.exe; $LASTEXITCODE
```
Expected: `LayerGraph: all checks passed`, `0`, `StreamAddress: all checks passed`, `0`.

- [ ] **Step 4: Run section E and record it**

Run section E on the build folder. Add an entry under `## Runs` in `DESK-SMOKE.md`, in the form of the entry that is there: the date, the commit, "build folder", section E's result, and anything run with a stand-in or not run. A real camera on the Camera row (Task 2 Step 8) needs a PC that has one: say so in the entry if this PC has none.

- [ ] **Step 5: Commit**

```powershell
git add breadstudio/docs/DESK-SMOKE.md
powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add Input Select and streams to the smoke checklist"
```

---

## Left for Plans B and C

| Item | Where |
|---|---|
| The NDI and OMT tabs, which wrap this plan's Desktop Capture page (§4.1 to §4.3) | Plan B (OMT), Plan C (NDI) |
| The on-air rule for tally (§8) | Plan B |
| A real camera on the Camera row, and a phone as an SRT caller (§1.1) | The owner's run of `DESK-SMOKE.md` |
