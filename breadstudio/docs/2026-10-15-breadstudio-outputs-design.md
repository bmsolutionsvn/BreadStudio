# BreadStudio — Outputs and Mix inputs — Design

Date: 2026-10-15
Status: design approved by the owner in conversation (2026-10-15); this written
spec awaits the owner's review.
Parent: `2026-10-14-breadstudio-network-inputs-design.md`, §1 part 2 (the
outputs engine). Part 3 (External) builds on this and has no spec yet.
Builds on: the desk (`2026-10-13-breadstudio-s1-desk-design.md`), the OMT and
NDI plugins (network inputs Plans B and C), and the on-air rule
(`breadstudio/shared/OnAir.hpp`).
Base: OBS Studio 32.2.2, as pinned by S0.

## 1. Purpose

BreadStudio sends pictures out the way vMix 29 does: four numbered outputs,
each with its own source, size, alpha and audio bus, each sent over NDI, OMT
and SRT at once, and up to three RTMP destinations that each pick an output.
Alongside them, **Mix inputs** as in vMix 28 and later: extra small switchers,
each with its own Preview, Program and transition, that are inputs on the desk
and can be sent on any output.

What the outputs feed, as the owner listed them:

| Use | Output |
|---|---|
| Program to a streaming or recording PC, or to vMix | Output 1 over NDI or OMT |
| A return feed to camera operators' monitors | An output over NDI or OMT, watched in any NDI or OMT viewer |
| A return feed to remote guests in BreadCall | An output over SRT or RTMP; how BreadCall takes it in is decided in S2 |
| An encoder or a CDN | An output over SRT, or an RTMP destination |
| A monitor PC, or another switcher | An output carrying the MultiView, or one input, with alpha when it is a key |

### 1.1 Done when

On the production PC, with a second PC running vMix 29 or NDI Tools:

1. Output 1 (Program) is received on the second PC over NDI and over OMT at
   the same time, with sound from bus M.
2. Output 2 set to the MultiView, at 720p, is received on the second PC.
3. Output 3 set to one input with alpha is received over NDI and keys
   cleanly over another picture in vMix.
4. Output 2 is sent over SRT to a listener (`ffplay` or vMix's SRT input)
   with a passphrase, and Output 1 to two RTMP destinations at once
   (a local RTMP server and a second one), started by the desk's Stream
   button.
5. A Mix input switches between two cameras by itself, is put on Program as
   an input, and is sent alone on Output 4. A camera on the Mix's Program
   shows Program tally while the Mix is on Program.
6. Stopping the second PC's receivers, pulling the network cable for ten
   seconds, or killing the RTMP server affects only those senders, and each
   recovers by itself. Program and the other outputs never stop.
7. Closing and opening the program brings every output, destination and Mix
   back as it was.

## 2. Decisions

| # | Topic | Decision |
|---|---|---|
| O1 | Number of outputs | Four, as vMix 29. Output 1 is always Program. |
| O2 | An output's source | Outputs 2 to 4: Program, Preview, MultiView, or one input. A Mix is an input. |
| O3 | Senders per output | NDI, OMT and SRT, each on or off, all at once. |
| O4 | RTMP | Three RTMP destinations, each picking an output. They replace OBS's Stream button and its one service. Owner's choice. |
| O5 | Encoding | One video and one audio encoder per output, made only while that output's SRT or one of its RTMP destinations is running, shared by all of them. One bitrate per output. |
| O6 | The engine | One OBS canvas per output for outputs 2 to 4; Output 1 is OBS's main mix. Owner approved, from three approaches (§11). |
| O7 | How the desk reaches the senders | Each protocol plugin registers an OBS output type; the desk makes outputs by type name and links no protocol library, as it does for inputs. |
| O7a | Program on outputs 2 to 4 | The Program picture itself, scaled: one picture, drawn once, as vMix's outputs set to "Output" are. Owner's question. |
| O7b | SRT output | BreadStudio's own output on FFmpeg's `srt` protocol, the one the SRT inputs use, with its settings passed apart from the address. Owner's instruction. |
| O7c | An output that loses its picture | Keeps sending, black, with its bus's sound. No sender stops because of what is or is not on it. Owner's instruction. |
| O8 | MultiView | vMix's default layout, one layout, nothing to set. Owner's choice. |
| O9 | Mix | A kind of input, as in vMix 28 and later. Owner's choice. |
| O10 | A Mix's sound | None of its own. Inputs' sound stays in the main mixer, and Audio Follows Video follows the main Program. |
| O11 | Tally | Outputs 2 to 4 do not light tally. An input on a Mix's Program is on air while that Mix is on the main Program. |
| O12 | Saved | In the scene collection, with the inputs, so a show file carries its outputs, destinations and Mixes. |

## 3. Outputs

### 3.1 What an operator sets

| | Output 1 | Outputs 2 to 4 |
|---|---|---|
| Source | Program | Program, Preview, MultiView, or one input |
| Size | OBS's output resolution (Settings → Video) | Same as Program, 3840×2160, 1920×1080 or 1280×720 |
| Alpha | No | When the source is an input |
| Audio | Bus M, A to E, or none | Bus M, A to E, or none |
| NDI | On or off | On or off |
| OMT | Off, OMT (VMX), OMT-HX HEVC, or OMT-HX H.264 | The same |
| SRT | Off, or a caller, listener or rendezvous with its settings (§3.4) | The same |
| Encoder | H.264 or HEVC, bitrate | The same |

- An output whose source is Preview shows what is on Preview. With Studio
  Mode off, that is Program.
- An output's audio is one of OBS's six tracks, which are the desk's buses
  M and A to E. An output carrying one input carries that bus, not only that
  input's sound.
- Alpha is offered only where it can be carried: NDI and OMT send it; SRT and
  RTMP drop it.
- Senders are named `<PC> (BreadStudio Output N)` on the network, as vMix
  names its own `<PC> (vMix - Output N)`.

### 3.2 RTMP destinations

Three, in a window opened from the ⚙ beside the desk's Stream button:

| Field | Notes |
|---|---|
| On | Whether the Stream button starts it |
| Output | 1 to 4 |
| Server | `rtmp://` or `rtmps://` address |
| Stream key | Shown as dots |

The Stream button starts every destination that is on, and stops them all.
It is red while any of them is live.

### 3.3 The Outputs window

Opened from a new **Outputs** button in the desk's footer. One row per output,
with every setting of §3.1, and beside each sender its state, updated live:

| Sender | State shown |
|---|---|
| NDI, OMT | Off, or the number of receivers |
| SRT | Off, listening, connected, trying again, failed |
| RTMP (per destination) | Off, live with its bitrate and dropped frames, trying again, failed |
| The output itself | Its size, and frames it dropped because the PC fell behind |

Changes take effect on OK. Then a new source for an output interrupts
nothing; a new size or alpha restarts that output's senders (§4.2).

### 3.4 SRT

The same fields as the Stream / SRT input page (network inputs spec §5.1):
mode, host, port, latency, passphrase, key length, stream ID. A caller calls
the host; a listener waits on the port, for one receiver at a time.

The values reach FFmpeg as separate options, never inside the address, so a
passphrase or stream ID may hold any character SRT allows (§4.3).

## 4. The engine

### 4.1 Canvases

OBS 32 has canvases: extra video mixes, each with its own channels, size and
pixel format, rendered in the same frame as the main mix and at its frame
rate.

- Output 1 is the main mix.
- Outputs 2 to 4 are each a canvas, made when the output has something to
  send. Channel 0 of the canvas holds the output's source:

| Source | Channel 0 |
|---|---|
| Program | The Program picture source, `bread_program`: it draws the main mix's finished picture |
| Preview | The current Preview scene, followed as it changes |
| MultiView | The MultiView source (§5) |
| An input | That input's scene |
| None (its input was deleted) | Nothing: the canvas is black, and its senders keep running |

- A canvas with alpha is BGRA; one without is NV12, as the main mix.
- **Program is one picture.** OBS draws its mixes in the order they were
  made, and the main mix always first, so in every frame the main mix's
  picture is finished before any output's canvas is drawn. `bread_program`
  draws that picture (`obs_render_main_texture`), scaled to the output's
  size. An output set to Program shows the same frame as Program, during a
  fade too, and the transition is drawn once, by the main mix. This is how
  vMix treats an output set to "Output": one Program picture, sent at
  several sizes.

### 4.2 Changing size or alpha

OBS refuses to reset a canvas while any output is live. So a change of an
output's size or alpha makes a new canvas for it, moves its senders to the new
canvas, and removes the old one. That output's receivers see a gap of a
fraction of a second; nothing else does.

### 4.3 Senders

| Sender | OBS output type | Registered by | Takes |
|---|---|---|---|
| NDI | `bread_ndi_output` | The NDI plugin, from DistroAV 6.2.2's `ndi-output.cpp`, cut down as its input was | Raw frames from the output's canvas, raw audio from its bus |
| OMT | `bread_omt_output` | The OMT plugin, on omthx's `Sender` | The same |
| SRT | `bread_srt_output` | A new small plugin, `breadstudio/srt` (§4.4) | The output's encoders |
| RTMP | OBS's `rtmp_output` | OBS | The output's encoders |

- The desk makes each by its type name. A build without a protocol plugin
  greys that column out, saying why, as a missing plugin removes a tab on
  Input Select.
- NDI's sender is greyed out, with the reason, while NDI's DLL is not loaded.
- The video encoder is the best hardware one the PC has (AMD AMF, NVIDIA
  NVENC, Intel QSV), else x264; the audio encoder is AAC at 160 kbit/s from
  the output's bus. Keyframes every two seconds.

### 4.4 SRT on FFmpeg's own protocol

OBS's built-in SRT output (`ffmpeg_mpegts_muxer` with OBS's "native SRT")
carries its own copy of FFmpeg's SRT code and reads every SRT setting from the
address, where a passphrase with `&` in it is cut short. BreadStudio does not
use it.

`bread_srt_output` instead:

- takes the output's encoded video and audio from OBS's encoders, as OBS's
  outputs do, so it shares them with the RTMP destinations;
- muxes them to MPEG-TS with OBS's FFmpeg (libavformat);
- opens `srt://host:port` through FFmpeg's own `srt` protocol, the one the
  Stream / SRT inputs play through, passing mode, latency, passphrase, key
  length and stream ID as an options dictionary, never in the address;
- opens and writes on its own thread, so a listener waiting for a caller, or
  a caller that cannot connect, never holds up the program; stopping it
  interrupts that wait at once;
- as a caller, tries again every two seconds after a failure or a dropped
  connection; as a listener, goes back to waiting when its receiver leaves.

It links OBS's own FFmpeg (`FFmpeg::avformat`, `FFmpeg::avcodec`,
`FFmpeg::avutil`), as the OMT plugin does, so the process has one FFmpeg.

### 4.5 What the engine does not change

- Recording stays OBS's: the Record button records Program.
- Tally stays the on-air rule's (network inputs spec §8), extended for Mixes
  in §6.4. Outputs 2 to 4 do not light a camera's tally.
- OBS's Start Streaming button, in OBS's Controls dock, is hidden. It is found
  by its object name, as the desk finds OBS's docks; when it is not found, the
  log says so and the button stays.

## 5. MultiView

A source of the desk plugin, `bread_multiview`, that draws vMix's default
layout at the size of the output that shows it:

```
┌───────────────────┬───────────────────┐
│                   │                   │
│     Preview       │     Program       │
│                   │                   │
├────┬────┬────┬────┼────┬────┬────┬────┤
│ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │ 7  │ 8  │
└────┴────┴────┴────┴────┴────┴────┴────┘
```

- Preview and Program on top, inputs 1 to 8 below, each with its number and
  name, and a red or green frame by the same rule as the input bar's.
- With Studio Mode off, the Preview box shows Program.
- An input slot with no input is dark.
- It has no settings.

## 6. Mix inputs

### 6.1 What a Mix is

Add Input gains a **Mix** row. It makes an input named `Mix 2`, then `Mix 3`,
and so on, whose base is a Mix source, `bread_mix`. The input is a tile on the
desk like any other: it can be put on Preview and Program, used as a layer,
and sent alone on an output.

The Mix source holds its own private transition, of any type OBS has. What
that transition shows is the Mix's Program; the Mix's Preview is the input
that will go to its Program next. The source draws the transition where it is
shown, as a scene draws its items, so a Mix on Program shows the same frame
as everything else on Program.

A Mix needs no canvas: it is drawn wherever it is shown, as every input
already is (Program, the input bar, the MultiView, an output).

### 6.2 The Mix window

A tile's ⚙ on a Mix opens the Mix window instead of the layers window:

```
┌ Mix 2 ──────────────────────────────────────────────┐
│ ┌───────────────┐            ┌───────────────┐       │
│ │   Preview     │  [ Cut ]   │   Program     │       │
│ │               │  [ Fade ]  │               │       │
│ └───────────────┘  500 ms    └───────────────┘       │
│ [1 Cam A] [2 Cam B] [3 Slides] [4 Blank] …           │
└──────────────────────────────────────────────────────┘
```

- The Program monitor shows the Mix source; the Preview monitor shows the
  Mix's Preview input.
- A click on an input button puts that input on the Mix's Preview.
- Cut and Fade send the Mix's Preview to the Mix's Program, and the old
  Program to the Mix's Preview, as the desk does.
- An input that would make the Mix show itself, directly or through layers or
  another Mix, is greyed out, by the desk's existing loop check.
- The window can stay open beside the desk; several Mix windows can be open.

### 6.3 Sound

A Mix has no sound of its own. Every input's sound stays in the main mixer,
and Audio Follows Video follows the main Program. An input whose AFV is lit
is heard when it is on the main Program, alone or as a layer, and is not heard
because it is on a Mix's Program.

### 6.4 Tally

An input on a Mix's Program is on air while that Mix is drawn by the main
Program. The Mix source declares its transition as its active child, and the
on-air rule's walk (`draws()`) gains one step: for a source that is neither a
scene nor a transition, it walks the children the source declares. Preview
tally stays the main Preview's only.

### 6.5 Limits

None fixed. Each place that shows a Mix draws what is on its Program, as for
any input; during a Mix's fade, both sides.

## 7. Failures

| Situation | What happens |
|---|---|
| A sender fails | Only it stops. Its state turns red in the Outputs window. Nothing else changes. |
| An RTMP server drops the connection | OBS's reconnect: it tries again, and the state says so. |
| An SRT caller cannot reach its listener | It tries again every two seconds. |
| An SRT listener has nobody calling | It waits, and says "listening". |
| NDI or OMT has no receivers | It keeps sending; the count reads 0. |
| No hardware encoder | x264, and the row says so. |
| NDI's DLL is not loaded | The NDI column is greyed out, with the reason (network inputs spec §10). |
| The input an output shows is deleted | That output keeps sending, black, with its bus's sound, so its receivers and streams stay connected. Its source reads "none" until another is chosen. |
| A Mix's Program or Preview input is deleted | That side of the Mix is black; the Mix goes on. |
| The scene collection is switched | The new collection's output settings are applied as OK in the Outputs window would apply them: a sender whose settings are the same keeps running, black for the moment the scenes reload; one whose settings changed restarts; one the new collection does not have stops. A live RTMP destination with the same settings stays live. |
| The PC cannot keep up | OBS drops frames in the output that falls behind; the Outputs window shows them per output. |

## 8. What is saved

All in the scene collection, as the inputs are:

| Saved | Where |
|---|---|
| The four outputs and their senders' settings | The desk's part of the scene collection |
| The three RTMP destinations, including stream keys | The same, in plain text, as OBS saves stream keys |
| SRT passphrases | The same, in plain text |
| A Mix | Its source's settings: its Program and Preview inputs, its transition and duration |
| Senders' states, receiver counts | Not saved; worked out live |

A scene collection made before this change loads with every output off and no
RTMP destination.

## 9. Testing

Automated, on every build:

- **The output plan**: a plain function that turns the four outputs' settings
  into what must run: which canvases at which size and format, which senders,
  which encoders, and which of them a change restarts. Tested for every
  source, a size change, an alpha change, SRT and RTMP sharing one encoder,
  and RTMP destinations on an output with nothing else on.
- **The SRT options**: a plain function that turns the SRT fields into
  FFmpeg's options, tested for every mode, an empty field, and a passphrase
  and stream ID with a space, `&`, `=`, `?` and a quote, which must arrive
  unchanged.
- **The on-air rule** with a Mix: an input on the Mix's Program, with the Mix
  on Program, alone and as a layer; the Mix on Preview only; the Mix's
  Preview side.
- **The MultiView layout**: the box of each slot for a few output sizes.

By hand, added to `DESK-SMOKE.md`: every row of §1.1, then each row of §7.

## 10. Build order

| Plan | Delivers | Needs |
|---|---|---|
| D | The output plan, canvases, the Program picture source, the Outputs window, the SRT plugin, the RTMP destinations, the Stream button | Nothing new |
| E | The MultiView source | Plan D |
| F | The OMT and NDI outputs | Plans B and C merged |
| G | Mix inputs, their window, the on-air rule's extra step | Nothing new; it can come before D |

## 11. Approaches considered

| Approach | Why not chosen, or why chosen |
|---|---|
| **One canvas per output** | Chosen. OBS 32's own mechanism for extra mixes: the GPU scales, every output type OBS has can take a canvas's video, and a canvas is saved and holds its scenes. |
| One OBS "view" per output | The older mechanism behind OBS's virtual camera. OBS has moved to canvases, and a view does not hold its scenes. |
| Copying frames back from the GPU ourselves, as DistroAV's main output does | Serves only NDI and OMT; SRT and RTMP would need hand-built encoder plumbing. |
| Each output is a Mix | Rejected by the owner: an output could no longer simply carry Preview, the MultiView or one camera. |
| Program on an output drawn by its own copy of the main transition | Draws every Program input twice per output, and a transition drawn from two mixes was untried. The main mix's finished picture is the same frame for free (§4.1). |
| SRT through OBS's built-in MPEG-TS/SRT output | Reads SRT settings from the address, cutting a passphrase with `&`; its SRT code is OBS's own copy rather than FFmpeg's. Owner's instruction to use FFmpeg's (§4.4). |
| A Mix as its own canvas, drawn on the main mix as a finished picture | Canvases made after the main mix are drawn after it, so the main mix would show a Mix one frame late. A Mix drawn where it is shown has no such lag (§6.1). |

## 12. Out of scope

- External: a capture card, the virtual camera and full-screen outputs fed
  from these outputs (part 3).
- Recording an output or an input on its own (ISO recording, S3).
- How BreadCall takes in a return feed (S2).
- A MultiView layout editor.
- Different bitrates for two RTMP destinations on the same output.
- A Mix with its own sound; a Preview tally per Mix.
- More than four outputs.

## 13. Risks

| Risk | Handling |
|---|---|
| The Program picture relies on OBS drawing the main mix before every other mix in a frame (`output_frames` in `libobs/obs-video.c` walks the mixes in the order they were made, and the main mix is made at start). | Plan D's first task shows a frame counter on Program and on an output set to Program and checks they match, during a fade too. The order is on the list of things checked after every upstream merge. |
| An SRT listener that nobody calls, or a caller whose listener is gone, waits inside FFmpeg. | It waits on its own thread, and FFmpeg's interrupt callback ends the wait when the output is stopped or the program closes (Plan D checks both). |
| CPU load: NDI and plain OMT encode on the CPU, about one core per 1080p60 sender; each output's canvas is one more render on the GPU, and each place that shows a Mix draws its Program. | The Outputs window shows each output's dropped frames. OMT-HX moves OMT's encoding to the GPU. |
| Hiding OBS's Start Streaming button relies on its object name. | Found by name, logged when absent, and added to the desk's checks after every upstream merge (desk smoke checklist, section C). |
| Stream keys in the scene collection travel with the show file. | Approved with the design (O12). OBS's own `service.json` holds keys in plain text too. |
