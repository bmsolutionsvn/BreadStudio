# BreadStudio — Network inputs and the Input Select window — Design

Date: 2026-10-14
Status: design approved by the owner in conversation (2026-10-14); this written
spec awaits the owner's review.
Parent: workspace `docs/superpowers/specs/2026-10-12-broadcast-roadmap-design.md`
(R12, §4 rows 3 and 5). §14 lists what this spec changes there.
Builds on: `2026-10-13-breadstudio-s1-desk-design.md` (the desk), and replaces
the scope of `2026-10-13-s2-omt-hx-input.md` (§12).
Base: OBS Studio 32.2.2, as pinned by S0.

## 1. Purpose

The owner wants BreadStudio to have SRT, NDI and OMT built in, as vMix 29 has,
for inputs and outputs, with vMix's External output. That is three pieces of
work, in this order:

| Part | What | Spec |
|---|---|---|
| 1 | Network inputs: SRT, NDI and OMT as inputs on the desk | **This document** |
| 2 | The outputs engine: numbered outputs, each with its own source, sent over NDI, OMT and SRT | Not written |
| 3 | External: capture card, virtual camera and full-screen outputs fed from an output's source | Not written |

Part 1 gives the desk a vMix-style **Input Select** window in place of its Add
menu, and three kinds of network input behind it. An operator adds an NDI or
OMT source by clicking its name in a list, and an SRT or RTSP stream by filling
in the form vMix users know.

### 1.1 Done when

On a production PC with BreadCamera on a second PC, vMix or NDI Tools on a
third, and a phone with an SRT app:

1. One BreadCamera input over OMT, one NDI input from the third PC and one SRT
   input from the phone are added from the Input Select window. No address is
   typed for the NDI or the OMT input.
2. With the test sender running three times (plain OMT, OMT-HX HEVC, OMT-HX
   H.264), each tile on the OMT tab names the right format, size and frame
   rate within two seconds, before anything is added (§4.2).
3. Each of the three inputs comes back by itself after its sender is
   restarted.
4. The NDI and the OMT sender each show Program tally only while their input
   is drawn on Program, and Preview tally only while it is drawn on Preview
   (§8).
5. A tile's ⚙ reopens the page the input was made on, filled in.
6. A PC with no NDI DLL starts, runs every other kind of input, and says on
   the NDI tab why NDI is off.

## 2. Decisions

| # | Topic | Decision |
|---|---|---|
| N1 | Order of the three parts | Inputs, then the outputs engine, then External. The owner left the order to the author. |
| N2 | The interface | vMix's Input Select window: kinds down the left, one page per kind. It replaces the desk's Add menu for every kind, old and new. Owner's instruction, from vMix screenshots. |
| N3 | Page grouping | NDI, OMT and Desktop Capture share one page with a tab each. Stream / SRT is its own page. Owner's instruction. |
| N4 | How each protocol is built | Three small parts that reuse what exists: OMT on the `omthx` library, NDI from DistroAV's input code, streams on OBS's Media Source. Chosen by the author; the owner gave the interface and did not choose between the approaches offered. |
| N5 | NDI's code | DistroAV's input code is taken into `breadstudio/ndi` and renamed. BreadStudio does not show DistroAV's name, menu, settings window or update checker. Owner: "Reference DistroAV but make builtin". |
| N6 | NDI's runtime DLL | **Ships inside BreadStudio's folder**, so the user installs nothing. Owner's decision, taken after §7.3's licence risk was put to them. |
| N7 | OMT formats | One OMT input reads plain OMT (VMX), OMT-HX HEVC and OMT-HX H.264, and tells them apart by itself. No codec setting exists in BreadStudio. |
| N8 | Knowing what a source is | The OMT tab names each source's format, size and frame rate **before it is added**, from a brief check of the source's first frame header. Owner's instruction. |
| N9 | Tally | Program and Preview tally are worked out from what Program and Preview draw, by one rule shared by NDI and OMT (§8). OBS's "active" and "showing" are not used. |

Rejected:

- **One "network input" plugin with a protocol dropdown.** One settings window
  for all three, but it ties three libraries into one plugin, so a fault in
  one takes out every network input, and it rewrites both the OMT plan and
  DistroAV's code to fit.
- **Third-party plugins as they are.** DistroAV unmodified keeps its own name,
  menus and update prompts inside BreadStudio, and its tally is wrong on the
  desk (§8).
- **A dedicated SRT receiver.** OBS's Media Source already plays every stream
  type on vMix's list. Its ceiling is in §5.3.

## 3. The Input Select window

```
┌ Input Select ─────────────────────────────────────────────────┐
│ Video               │ [ NDI ] [ OMT ] [ Desktop Capture ]     │
│ Camera              │ ─ CAMERA-PC ──────────────────────────  │
│▌NDI / OMT / Desktop │  ┌──────────┐ ┌──────────┐              │
│ Stream / SRT        │  │ OMT-HX   │ │ OMT      │              │
│ Image               │  │ HEVC     │ │ VMX      │              │
│ Colour              │  └──────────┘ └──────────┘              │
│ Audio Input         │   Cam 1        vMix - Output 1          │
│ Web Browser         │                                         │
├─────────────────────┴─────────────────────────────────────────┤
│                                              [ OK ] [ Cancel ]│
└───────────────────────────────────────────────────────────────┘
```

### 3.1 The rows

One row per kind of input the desk can make, in vMix's order. A row whose
plugin is not in the build is left out, as the Add menu leaves a type out now.

| Row | Makes an input whose base is | Page |
|---|---|---|
| Video | Media Source, local file | OBS's settings for that source, embedded |
| Camera | Video Capture Device | OBS's settings, embedded |
| NDI / OMT / Desktop Capture | NDI source, OMT source, or Display or Window Capture | §4 |
| Stream / SRT | Media Source, network | §5 |
| Image | Image | OBS's settings, embedded |
| Colour | Colour Source | OBS's settings, embedded |
| Blank | A black Colour Source, to put layers on | Nothing to set |
| Audio Input | Audio Input Capture | OBS's settings, embedded |
| Web Browser | Browser | OBS's settings, embedded |

"Embedded" means OBS's own settings panel for that source type, shown inside
the page. OBS ships that panel as a reusable part (`shared/properties-view`),
and it can show a type's settings before any source exists. No second window
opens.

### 3.2 OK, Cancel and ⚙

- **OK** makes the input with the page's settings, places it last, and sends
  it to Preview, as the Add menu does now. OK is greyed out until the page has
  what it needs (a source picked, a host typed).
- **Cancel** makes nothing.
- A tile's **⚙** opens the desk's input settings as now (layers). Its "Base
  properties…" button opens this window on the page the input was made on,
  filled in, with the left list hidden. OK then changes the input; it does not
  make another.

### 3.3 Left out of vMix's window

| vMix has | Why not here |
|---|---|
| Live thumbnails on the NDI tab | Each one is a real connection to a source for as long as the window is open. The tiles here are named, with no picture. May follow. |
| PsF and FEIM checkboxes | Nothing in what BreadStudio receives with corresponds to them. |
| Decoder Delay | OBS already has a sync offset per source in Advanced Audio Properties. |
| The VLC stream type | Needs VLC installed on the PC. |
| Number (add several at once) | Meaningless for a network source. |
| DVD, List, Instant Replay, Image Sequence, Video Delay, Photos, PowerPoint, Audio (file), Title, Virtual Set, Video Call | Not kinds of input the desk has. |

## 4. The NDI / OMT / Desktop Capture page

### 4.1 NDI and OMT tabs

- Each tab lists the sources announced on the network, grouped under the
  sender PC's name, and refreshes once a second while the window is open.
- Clicking a tile selects it. OK adds it.
- For a network where announcements do not arrive: under the OMT list, **Add
  by address** takes a sender's address typed by hand; under the NDI list,
  **Also search** takes the IP addresses of sender PCs, which NDI's finder
  then asks directly.
- A tab whose plugin is not in the build is not shown. A tab whose library
  failed to load shows one line saying why (§10).

NDI tab checkboxes:

| Checkbox | Does |
|---|---|
| Low Bandwidth Mode | Asks the sender for its low-bandwidth stream |
| Audio Only | Receives the sound and no picture |
| Increase Buffer Size | Trades delay for smoothness on a poor network |

The OMT tab has no checkboxes: the receiver has no settings.

### 4.2 What each OMT source is

OMT's announcement carries a name only. Every video frame carries its format
in its header. So when a source appears on the OMT tab, BreadStudio connects
to it for a moment, reads the header of the first frame, and disconnects. The
picture is not decoded.

The tile then reads one of:

| Tile | Meaning |
|---|---|
| `OMT · VMX · 1920×1080 · 50` | Plain OMT, as vMix 29 and OMT's own tools send |
| `OMT-HX · HEVC · 1920×1080 · 60` | BreadCamera's compressed format, HEVC |
| `OMT-HX · H.264 · 1280×720 · 60` | The same, H.264 |
| `… · alpha` added | The source carries transparency |
| `checking…` | No answer yet |
| `no signal` | Connected; no picture within two seconds |
| `unknown format` | A compressed format BreadStudio does not know |

- The check runs again each time the window opens, because a camera's codec
  can be changed between shows.
- For plain OMT it asks for the low-resolution preview stream. For OMT-HX the
  sender's stream goes to one more receiver for that moment. It never asks for
  a keyframe, so it causes no bitrate spike for receivers already watching.
- The sender's connection count rises by one for a fraction of a second. Its
  tally is not touched.

OMT-HX is the owner's own extension of OMT. A third party's different
compressed format inside OMT is `unknown format`: it can be added, and stays
black.

### 4.3 After an OMT input is added

The input reads all three formats and decides per frame which one it has
(N7). The page, opened from ⚙, shows what it is receiving now, including the
decoder: `OMT-HX · HEVC · GPU decode`, `OMT-HX · H.264 · software decode`, or
`OMT · VMX`. A source that comes back in another format is followed without
the input being touched.

### 4.4 Desktop Capture tab

Two choices, **Screen** and **Window**, each showing OBS's settings for
Display Capture or Window Capture, embedded. They are today's "Screen" and
"Window" kinds, moved here as vMix groups them.

## 5. The Stream / SRT page

### 5.1 The form

As vMix's, field for field:

| Field | Shown for | Notes |
|---|---|---|
| Stream Type | always | §5.2 |
| Hostname | all but RTSP | For a listener: the local address to listen on; empty means every network card |
| URL | RTSP only | The whole `rtsp://…` address, in place of Hostname and Port |
| Port | all but RTSP | Default 5000 |
| Latency (ms) | SRT | Default 200 |
| Passphrase | SRT | Empty means no encryption |
| Key Length | SRT | 16, 24 or 32; default 32 |
| Stream ID | SRT | Optional |
| Use Hardware Decoder | always | Default on |
| Status box, Clear | always | §5.3 |

### 5.2 Stream types

Every type plays through OBS's Media Source. The page turns the form into
that source's address and options:

| Stream Type | Address | FFmpeg options |
|---|---|---|
| SRT (Caller) | `srt://host:port` | `mode=caller latency=… passphrase='…' pbkeylen=… streamid='…'` |
| SRT (Listener) | `srt://host:port`; host empty: `0.0.0.0` | `mode=listener …` |
| SRT (Rendezvous) | `srt://host:port` | `mode=rendezvous …` |
| RTSP over UDP | the URL as typed | `rtsp_transport=udp` |
| RTSP over TCP | the URL as typed | `rtsp_transport=tcp` |
| Transport Stream over UDP | `udp://host:port`; host empty: `udp://@:port` | format `mpegts` |
| Transport Stream over TCP | `tcp://host:port`; host empty: `0.0.0.0` | `listen=1`, format `mpegts` |
| Transport Stream over TCP Pull | `tcp://host:port` | format `mpegts` |

- The address carries the host and the port and nothing else. Every other
  value goes in the Media Source's FFmpeg options, in quotes, because
  FFmpeg does not decode every parameter of an SRT address: a passphrase
  with `&` in it would be cut short there. Quoted, a space, `&` or `=` in a
  passphrase or stream ID cannot change another option.
- Latency is typed in milliseconds and written in the unit the library wants.
- An option whose field is empty is left out.
- A host is a name or an address. One that holds a `/`, `?`, `@` or a space
  greys OK out, as does an SRT passphrase outside SRT's 10 to 79 characters.
- The source is set to reconnect after two seconds and to keep running while
  it is not on Program, so a cut to it has no start-up delay.

Turning the form into an address and options is one plain function with no
OBS and no Qt in it (§11). The form's values are saved beside the source
(§9), so nothing has to be read back out of an address.

### 5.3 Status, and the ceiling

The status box lists this input's connection state as it changes, with the
time: connecting, playing, lost and trying again. It is shown when the page is
opened for an input that exists. OBS's Media Source reports a state and no
reason; FFmpeg's reason is in OBS's log.

This is OBS's general media player. It reconnects by itself, but it has no SRT
statistics (packet loss, round-trip time, bitrate). A dedicated SRT receiver
is the upgrade if those are needed.

## 6. The parts

| Part | Where | Does | Depends on |
|---|---|---|---|
| Input Select window | `breadstudio/desk` | §3. Replaces the Add menu; opened by "Base properties…" | The desk's input model; `shared/properties-view` |
| Stream page | `breadstudio/desk` | §5: the form, the address function, the status box | OBS's Media Source |
| OMT input | `breadstudio/omt-hx` | The OMT source; the list of sources with their formats (§4.2) | The `omthx` library, with a new `probe()` |
| NDI input | `breadstudio/ndi` | The NDI source; the list of sources | NDI's DLL (§7) |
| On-air rule | `breadstudio/shared/OnAir.hpp` | §8, one header | libobs, and the frontend API for the Preview scene |

### 6.1 How the desk reaches the plugins

The desk links neither `omthx` nor NDI. It asks each source type for its
settings, as OBS's own settings panel does, and reads two lists from them:

| List | Each entry |
|---|---|
| `address` | Shown name; value: the address to connect to |
| `detail` (OMT only) | Same order as `address`; shown text: the tile's format line (§4.2) |

The window reads them again once a second while a tab is open. A build
without a plugin has no such type, and so no tab.

The OMT plugin keeps a table of address to detected format, filled by a
background check. The check runs only while something has asked for the list
within the last few seconds, so nothing is probed while the window is closed.

### 6.2 Edits to OBS files

`plugins/CMakeLists.txt` gains two lines, under the desk's, adding
`breadstudio/omt-hx` and `breadstudio/ndi`. The file is already on
`UPSTREAM-EDITS.md`; its reason is reworded to name all three. No other OBS
file is edited, and none is added outside `breadstudio/`.

### 6.3 A change in the OMT-HX repo

`omthx` gains one function, about 30 lines, beside the receiver it shares its
header-reading with:

- `probe(address, timeout)`: connects, asks for the preview stream, reads the
  first video frame's header and returns the mode, codec, size, frame rate
  and alpha flag; or "no signal"; or "unknown format".

It has a test beside the existing receiver tests, against the repo's test
sender in all three formats.

## 7. NDI

### 7.1 The code

DistroAV (the former obs-ndi) is an OBS plugin under the GPL. BreadStudio
takes its input and the code that finds and loads NDI's library into
`breadstudio/ndi`:

- Each file keeps its copyright header, and the About box credits DistroAV.
- The source is named "NDI source". DistroAV's Tools menu entry, output
  settings window, update checker and its own configuration file are not
  taken.
- Its output code is not taken in this part; part 2 will need it.
- Its tally, which follows OBS's "active" and "showing", is replaced by §8.
- DistroAV's later fixes are merged in by hand. The files are few and change
  rarely.

The plan's first step confirms DistroAV's exact licence text and the release
the code is taken from.

### 7.2 The DLL

By N6, `Processing.NDI.Lib.x64.dll` ships in BreadStudio's folder.

- It is copied from the NDI SDK on the build machine at build time. It is
  never committed to git.
- The build machine, and later CI, therefore needs the NDI SDK installed, as
  BreadCamera's build already expects. Without it the configure prints a
  warning, the NDI plugin is still built (NDI's headers are MIT-licensed and
  are in the tree), and the DLL is absent, which §10 covers. The script that
  makes the installer refuses a build that has no DLL.
- The plugin loads the DLL at run time, looking in its own folder first and
  then in NDI's install location. So the DLL can be taken out of the package
  later without a code change.
- The package carries NDI's licence text, and About carries NDI's trademark
  line.

### 7.3 The licence risk, as put to the owner

NDI's distribution terms allow the DLL inside an application "if your EULA
terms cover the specific requirements of the NDI SDK EULA". BreadStudio is
GPL, as a fork of OBS must be, and the GPL does not let a distributor attach
NDI's restrictions to the program. NDI's own stated route for open-source
programs is its MIT-licensed headers with the runtime installed separately,
optionally by the program's installer.

So shipping the DLL in the folder conflicts with the GPL as the author reads
it, and the exposure is a complaint from the OBS project or from NDI. The
author is not a lawyer. The owner chose this route knowing that. The two
alternatives offered were BreadStudio's installer running NDI's own runtime
installer silently, and a one-click download at first use; §7.2's run-time
loading keeps both open.

## 8. Tally

Both NDI and OMT send tally back to the sender. OBS's own signals cannot
drive it on the desk:

- The input bar and the multiview show every input all the time, so every
  source is "showing". A tally taken from that reads Preview for every camera.
- The audio bed keeps an input with AFV unlit "active" whatever is on Program
  (desk spec §5.3). A tally taken from that reads Program for that camera all
  the time. Measured on the desk on 2026-10-14.

One rule replaces them, in `OnAir.hpp`, used by both plugins:

| Tally | True when |
|---|---|
| Program | The source is drawn by output channel 0, the transition that feeds Program. During a fade that is both sides, so a camera stays red until it has left the picture. |
| Preview | Studio Mode is on and the source is drawn by the Preview scene. |

"Drawn by" means a visible item at any depth: in the scene, in an input that
is a layer of it, or in a group. The audio bed is on another channel and does
not count. Each source works its tally out ten times a second and sends it
when it changes, and once to every new connection.

## 9. What is saved

Everything is in the scene collection, as the desk's inputs are now.

| Saved | Where |
|---|---|
| An NDI or OMT input's source (sender PC and name) and its checkboxes | The base source's settings |
| A stream input's form values | The base source's private settings, so the form can be shown again |
| The detected format, the connection state, the status box | Not saved; worked out live |

A scene collection made before this change loads as it does now. Its inputs
open in the new window from "Base properties…".

## 10. Failures

| Situation | What happens |
|---|---|
| A sender is absent at startup, or disappears | The tile goes black. The input keeps trying and comes back by itself. Nothing else on the desk changes. |
| A sender returns under its name in another format | The input follows it. |
| Two senders have the same source name | They are two tiles, under two PC names. An input stays bound to the PC and name it was made from. |
| SRT: the listener port is in use, or the passphrase is wrong | The tile stays black and the status box shows the input trying again. The reason is in OBS's log. |
| NDI's DLL is missing or does not load | The NDI tab shows one line saying so. NDI inputs already in the collection are black tiles. Nothing crashes; every other kind works. |
| No GPU decoder for OMT-HX | Software decoding, and the page says so. |
| An OMT source's format is unknown | `unknown format` on the tile. It can be added, and stays black. |
| Announcements do not arrive | Add by address for OMT, Also search for NDI (§4.1). |
| The window is open while a source is probed and the source goes away | The tile goes to `no signal`, then leaves the list. |

## 11. Testing

Automated, on every build:

- **The stream address function**: every stream type; with and without
  passphrase and stream ID; a quote, a space, `&` and `=` in a value; an IPv6
  host; and every reason OK is greyed out.
- **The on-air rule**: a small scene tree with the source alone, as a layer,
  hidden, and absent.
- In the OMT-HX repo: **`probe()`** against the test sender in all three
  formats, and against an address with nothing there.

By hand, added to `DESK-SMOKE.md`:

- **Input Select**: every row makes its kind of input; "Base properties…"
  reopens the page filled in; OK there changes the input and makes no second
  one. This replaces the Add rows of the desk's checks.
- **OMT**: §1.1 item 2 with the test sender; then BreadCamera and vMix 29's
  OMT output as real senders; then a sender restarted in the other codec.
- **NDI**: vMix or NDI Tools on another PC; Low Bandwidth Mode and Audio Only
  each; the DLL renamed away, for §10.
- **SRT**: a phone as caller into a listener input; BreadStudio as caller into
  a listener; a wrong passphrase; the network cable out for ten seconds.
- **Tally**, for NDI and for OMT: Program alone; Program as a layer; Preview;
  only a tile in the bar reads neither; AFV unlit with something else on
  Program reads neither; a three-second fade holds Program to its end; a layer
  unticked drops Program; Studio Mode off.
- **Leaks**: close with one input of each kind open; the log ends with
  `Number of memory leaks: 0`.

## 12. Build order

One spec, three plans. Each leaves something usable.

| Plan | Delivers | Needs |
|---|---|---|
| A | The Input Select window with today's kinds, and the Stream / SRT page | Nothing new |
| B | The OMT plugin, its tab, the format check, the on-air rule | The OMT-HX checkout, with `probe()` added |
| C | The NDI plugin, its tab, the DLL | The NDI SDK on the build machine |

Plan B takes over `2026-10-13-s2-omt-hx-input.md`. That plan's Tasks 1 to 5
stand as checked on 2026-10-14; its Task 6 (an Add menu entry) is replaced by
the OMT tab, and the format check is added.

## 13. Out of scope

- Sending anything: NDI, OMT and SRT outputs, and External (parts 2 and 3).
- NDI cameras' PTZ control, and NDI|HX as a tested sender.
- Live thumbnails in the lists.
- SRT statistics.
- RTMP as a stream type.
- The format check on the NDI tab.

## 14. What this changes in the roadmap

| Roadmap | Was | Becomes |
|---|---|---|
| R12, NDI | "NDI is BreadCamera's door for outside programs; its runtime is closed and installed by the user." | BreadStudio receives NDI too, and ships NDI's DLL (N6). |
| R12, OMT | "Nothing but the BreadCamera output and the BreadStudio input may depend on OMT." | Unchanged by part 1: this is still the BreadStudio input. Part 2's OMT output will change it. |
| §4 row 3, S2 | Includes "BreadCamera outputs as inputs over OMT, with tally sent back". | That item is delivered here. The rest of S2 (Kinde, BreadCall, BreadStream) is untouched. |
| §4 row 5, S3 | "Several independent outputs over SDI, NDI and SRT; ISO recording." | Brought forward as parts 2 and 3, with OMT added. |

The roadmap file is not edited by this spec's commit; the owner's review
decides.

## 15. Risks

| Risk | Handling |
|---|---|
| The NDI DLL in a GPL package (§7.3). | The owner's decision. Run-time loading keeps the installer and download routes open. |
| DistroAV's code assumes its own configuration and menus. | Plan C's first task reads the release it takes from and lists what each kept file needs. |
| The desk's input bar was reworked on 2026-10-14 (`f78a6d607`), after its checks were run. | Plan A reads the bar as it is then, and its hand-checks cover the Add path again. |
| The format check opens connections the operator did not ask for. | Preview stream for plain OMT; a fraction of a second; only while the window is open (§4.2, §6.1). |
| The build then needs two things outside the repo: the OMT-HX checkout and the NDI SDK. | Without the OMT-HX checkout the configure stops and names it. Without the NDI SDK it warns, and the installer script refuses to package (§7.2). S0's CI task must fetch both. |
| OBS's Media Source adds delay on SRT that a dedicated receiver would not. | Measured in Plan A's hand-check against vMix on the same stream; §5.3's upgrade if it matters. |
