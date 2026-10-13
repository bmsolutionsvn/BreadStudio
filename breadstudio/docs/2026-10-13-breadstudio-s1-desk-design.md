# BreadStudio S1 — The desk — Design

Date: 2026-10-13
Status: design approved by the owner in conversation (2026-10-13); this written
spec awaits the owner's review.
Parent: workspace `docs/superpowers/specs/2026-10-12-broadcast-roadmap-design.md`
(§4 row 2, R2, §7 "Inputs-first on a scene-based core", §8 row S1).
Base: OBS Studio 32.2.2, as pinned by S0.

## 1. Purpose

An operator who knows vMix runs a show in BreadStudio the way they would in
vMix: a row of numbered inputs, Preview and Program, Cut and Auto, a multiview
with tally, and an audio mixer with buses. They never open OBS's scene list.

S1 reshapes more than it writes. OBS already has Preview and Program (Studio
Mode), transitions, a T-bar, a multiview and six audio tracks. S1 adds the two
things OBS lacks, an input bar and a bus mixer, and makes inputs the thing the
operator handles instead of scenes.

### 1.1 Done when

On one PC with two cameras, a media file, a microphone and a second monitor:

1. Four inputs with a picture (camera 1, camera 2, the media file, and a
   two-box of the two cameras) and one microphone input are built from the
   input bar alone.
2. The show is cut and faded between them, and the multiview's red and green
   frames match Program and Preview throughout.
3. The microphone is heard all the time. Camera audio is heard only while its
   input is on Program.
4. A test tone reads the same level when its input is in the audio bed only,
   on Program only, and both (§5.3).
5. A recording carries Master on track 1 and, on track 2, a mix-minus without
   the microphone.
6. After a restart, inputs, order, layers, levels, bus sends and AFV come back
   as they were.
7. OBS's Scenes and Sources docks were never opened.

## 2. Decisions

| # | Topic | Decision |
|---|---|---|
| D1 | What an input is | One OBS scene with a fixed shape: a base at the bottom, layers above (§3). |
| D2 | What a layer is | A reference to another input, and nothing else. Underneath, a scene placed on a scene. |
| D3 | Preview, Program, Cut, Auto, T-bar | OBS's Studio Mode, switched on. Nothing new is written. |
| D4 | Where the desk's code lives | One plugin, `breadstudio/desk/`, using the frontend API. Two one-line edits to OBS files (§6.1). |
| D5 | Multiview and tally | OBS's own multiview. Nothing new is written. |
| D6 | Tally colours | Red for Program, green for Preview: the broadcast convention and what OBS's multiview draws. vMix's own colours (green and orange) are not used. |
| D7 | Buses | OBS's six audio tracks: Master is track 1, buses A to E are tracks 2 to 6. Per-input send switches and a meter per bus. No bus fader. |
| D8 | Audio independent of Program | One hidden scene, the audio bed, kept on a spare output channel (§5.3). No libobs edit. |
| D9 | Input numbers | Number is position in the input bar. Dragging a tile renumbers. |
| D10 | Shortcuts | Nine new hotkeys, "Input 1 to Preview" to "Input 9 to Preview", which follow the number, not the scene. Unassigned by default. *Taken from the owner's "approve" to a message that offered them; strike this row if that was not meant.* |
| D11 | Scene duplication in Studio Mode | Off by default in BreadStudio, so an edit to an input shows at once wherever that input is, as in vMix. |

Rejected:

- **An input is a bare source on a desk-owned Program pipeline.** vMix's true
  model, but OBS's frontend, multiview, hotkeys and websocket all assume
  scenes, and each would have to be patched or replaced.
- **The desk written into OBS's main window (`OBSBasic`).** Full control of the
  layout, at the price of a conflict in OBS's most-changed files on every
  upstream merge. Against roadmap R2.
- **A bus fader in S1.** OBS has no gain per track, so it needs an edit to
  libobs's mixer. Left until a show needs it.
- **Overlay channels in S1.** vMix's Overlay 1–4 are a separate bus over
  Program. Not needed for the done-when.

## 3. The input model

An input is one OBS scene.

| Position in the scene | What it is | Rules |
|---|---|---|
| Bottom item | **The base.** One camera, media file, screen or window capture, browser page, image, colour, or audio capture device. | Fitted to the canvas and locked. Exactly one. Not moved or removed while the input exists. |
| Every item above | **A layer.** A reference to another input, with its own position, size, crop and on/off. | Added, ordered and removed in the desk's layers list. No limit on count. |

- **A layer shows the other input as it is, its own layers included.**
- **One source lives in one input.** To show camera 1 inside another shot, add
  input 1 as a layer. A source is never opened twice.
- **A shot made only of layers** (a two-box) has a colour base. The add menu
  calls it "Blank".
- **An audio-only input** has an audio capture device as its base. Its tile
  shows the name instead of a picture.
- **No loops.** A layer is refused if it would make an input contain itself,
  directly or through other inputs.
- **Deleting an input** that is used as a layer elsewhere removes those
  layers. The desk states how many before it deletes.
- **Number is position.** Inputs are numbered 1…N in OBS's scene order.
  Dragging a tile changes the scene order and so the numbers. A layer points
  at the input, not at its number, so renumbering never rewires a shot.

**Scenes made outside the desk** (in OBS's scene editor, or an imported OBS
scene collection) appear as inputs and switch normally. The desk keeps no
hidden marker on a scene. It treats the bottom item as the base and lists the
rest as they are. Only inputs created by the desk are guaranteed the fixed
shape.

## 4. The screen

```
┌────────────────────┬────────┬────────────────────┐
│                    │  Cut   │                    │
│      PREVIEW       │  Fade  │      PROGRAM       │   OBS Studio Mode,
│                    │  …     │                    │   unchanged
│                    │ T-bar  │                    │
├────────────────────┴────────┴────────────────────┤
│ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐              │
│ │ live │ │ live │ │ live │ │ live │   [+ Add]    │   Input bar (new dock)
│ │1 Cam1│ │2 Cam2│ │3 Deck│ │4 2box│              │
│ │Cut ♪⚙│ │Cut ♪⚙│ │Cut ♪⚙│ │Cut ♪⚙│              │
│ └──────┘ └──────┘ └──────┘ └──────┘              │
├───────────────────────┬─────────────┬────────────┤
│ Mixer (new dock, §5)  │ OBS Scene   │ OBS        │
│                       │ Transitions │ Controls   │
└───────────────────────┴─────────────┴────────────┘
```

### 4.1 Top half

OBS's Studio Mode: Preview, Program, the transition column with its quick
transition buttons (Cut, Fade, and any the operator adds), and the T-bar. Auto
is OBS's Transition button, with the transition and duration chosen in OBS's
transition controls. S1 changes nothing here.

### 4.2 Input bar

- One tile per input: live picture, number, name. A red frame when the input
  is on Program, green when on Preview.
- **Click** sends the input to Preview.
- **Cut** on the tile sends it straight to Program as a cut, whatever
  transition is selected.
- **♪** switches the input's audio on or off (§5.1).
- **⚙** opens the input's settings (§4.3).
- **Drag** reorders. **Right-click** offers rename and delete.
- **+ Add** offers the base types in §3, creates the input, and opens OBS's
  own properties window for the new base.
- All tiles are drawn by one display, the way OBS's multiview draws its
  pictures, so every input stays live and a cut has no start-up delay.

### 4.3 Input settings

- A button that opens the base's properties (OBS's own window).
- The layers list: add a layer by choosing an input, remove, reorder, switch
  on or off.
- Position and size of a layer are set by dragging it in the Preview pane,
  which OBS already supports. The base is locked, so it cannot be dragged.
  Opening the settings sends the input to Preview.

### 4.4 Multiview

OBS's multiview, opened from OBS's View menu in a window or full-screen on a
second monitor. It shows Preview, Program and up to 25 inputs in input-bar
order, with red and green frames. It labels a picture with the name only; the
number is on the tile.

### 4.5 Layout

On first run the desk switches Studio Mode on, arranges the window as drawn
above and hides OBS's Scenes, Sources and Audio Mixer docks. Those docks stay
in OBS's Docks menu. A "Desk layout" item in the Tools menu reapplies the
arrangement.

### 4.6 Shortcuts

Nine hotkeys in OBS's Settings → Hotkeys: "Input 1 to Preview" to "Input 9 to
Preview". Each sends whichever input currently holds that number. OBS's
existing per-scene hotkeys and its transition hotkey keep working.

## 5. Audio

### 5.1 The mixer dock

It takes the place of OBS's Audio Mixer dock in the desk layout.

```
┌─ 1 Cam1 ─┬─ 2 Cam2 ─┬─ 5 Mic ──┐  ┌─ M ─┬─ A ─┬─ B ─┬ … E ┐
│  meter   │  meter   │  meter   │  │meter│meter│meter│     │
│  fader   │  fader   │  fader   │  └─────┴─────┴─────┴─────┘
│ ♪ AFV 🎧 │ ♪ AFV 🎧 │ ♪ AFV 🎧 │     bus meters only
│ M A B …  │ M A B …  │ M A B …  │
└──────────┴──────────┴──────────┘
```

One strip per input whose base has audio, in input order, then one per device
set in OBS's Settings → Audio.

| Control | Does | Built on |
|---|---|---|
| Fader, meter | Level of the input | OBS's fader and volume meter |
| ♪ | Audio on or off. The same switch as ♪ on the tile. | Source mute |
| AFV | Audio follows video (§5.3). An icon button, placed and behaving as vMix's, with a tooltip. The icon is drawn for BreadStudio, not copied from vMix. | The audio bed |
| 🎧 | Hear this input in the headphones, on air or not | OBS's per-source monitoring, on the device set in Settings → Audio |
| M A B C D E | Which buses the input feeds | OBS's per-source track switches |

### 5.2 Buses

- Master is OBS track 1. Buses A to E are tracks 2 to 6.
- Each bus has a meter, read from that track's mixed audio. No fader, no mute.
- A **mix-minus** is a bus with the unwanted inputs' send switched off.
- A bus is used where OBS already uses a track: the stream picks one, a
  recording can carry several. S2's return to BreadCall will pick one.

### 5.3 AFV and the audio bed

OBS plays a source only while it is part of what is on Program. vMix plays an
input's audio all the time unless told otherwise. AFV chooses between the two,
per input:

| AFV | The input is heard |
|---|---|
| On | Only while it is on Program, directly or as a visible layer, crossfading with the transition. OBS's normal behaviour. |
| Off | All the time, whatever is on Program. |

Defaults: an input with a picture starts with AFV on. An audio-only input
starts with AFV off, so a microphone is live when it is added.

**Layers carry their audio.** When a two-box is on Program, both of its inputs
are heard, each subject to its own ♪.

**The audio bed.** The desk owns one hidden scene, not saved and not listed
anywhere, and keeps it on output channel 63. The base of every input with AFV
off is an item in it, placed outside the canvas so nothing is drawn. OBS mixes
the bed along with Program.

An input with AFV off that is also on Program reaches the mix by two paths.
libobs 32 detects a source that appears more than once in the audio tree,
drops it from the scenes that hold it, and mixes it once
(`libobs/obs-audio.c`, `push_audio_tree2`; `libobs/obs-scene.c`,
`scene_audio_render`). The level is therefore the same on either path or both.

This was read in the source, then proven with a test tone (§1.1 item 4): bus
M reads -12.0 dB with the tone on Program alone, in the bed alone, and on
both paths at once.

OBS clears every output channel when a scene collection is unloaded, so the
desk puts the bed back after each load.

## 6. The plugin

`breadstudio/desk/`, one module, built like OBS's `plugins/frontend-tools`:
Qt widgets, the frontend API and libobs.

| Unit | Job | Depends on |
|---|---|---|
| `InputModel` | Create and delete inputs, keep the fixed shape, layers, the loop check, order. No widgets. | libobs, frontend API |
| `AudioBed` | The hidden scene, its channel, which bases are in it, the AFV flag. | libobs |
| `InputBar` | The tile dock, its one display, the add menu, the nine hotkeys. | `InputModel`, `AudioBed` |
| `InputSettings` | The ⚙ window. | `InputModel` |
| `Mixer` | Strips and bus meters. | `AudioBed`, libobs |
| `DeskLayout` | First-run arrangement, Studio Mode on, the "Desk layout" menu item. | Frontend API, the main window |

The plugin follows OBS's frontend events to stay in step: scene list changed,
Preview changed, Program changed, scene collection changed and cleaned up,
Studio Mode switched off.

### 6.1 OBS files edited

Both are added to `breadstudio/UPSTREAM-EDITS.md` in the commit that makes
them.

| File | Edit | Why |
|---|---|---|
| `plugins/CMakeLists.txt` | One line that adds `breadstudio/desk` to the build. | A plugin is built only if it is named there. |
| `frontend/OBSApp.cpp` | The default of `SceneDuplicationMode` changes from true to false (D11). | OBS reads the setting once at start; a plugin cannot change it. The operator can still switch it in Studio Mode's menu. S0 already edits this file. |

### 6.2 Reaching into the main window

Two things have no frontend API, so the desk finds OBS's widgets by name in
the main window:

| Need | Looked up | Used for |
|---|---|---|
| Reordering inputs | The scene list, `scenes` | Moving a row, then raising its `scenesReordered` signal so the multiview follows. |
| The desk layout | The docks `scenesDock`, `sourcesDock`, `mixerDock`, `transitionsDock`, `controlsDock` | Hiding the first three and placing the last two beside the desk's docks. |

An upstream rename would break these without a build error. So: if a lookup
fails, that feature is switched off (drag is disabled, the layout is left as
it is) and the failure is logged, and both lookups are items on the smoke
checklist, which is run after every upstream merge.

## 7. State

Almost nothing new is stored.

| What | Where it is kept |
|---|---|
| Inputs, their order, bases, layers | OBS's scene collection: scenes and scene items. |
| Levels, ♪, bus sends, 🎧 | OBS's scene collection: source volume, mute, track switches, monitoring. |
| AFV, per input | A flag in that scene's own saved settings. |
| "Desk layout has been applied" | One flag in the plugin's settings. |

The audio bed is not saved. It is rebuilt from the AFV flags after each load.

## 8. Failures

| Case | Behaviour |
|---|---|
| Studio Mode is switched off by hand | The input bar shows a warning that a click now cuts straight to Program, with a button that switches Studio Mode back on. |
| Deleting an input that is on Program, directly or as a layer of what is on Program | Refused. |
| A layer that would make a loop | Refused, with the reason. |
| A camera is unplugged | Its tile goes black, as in OBS. Nothing else changes. |
| The scene collection is switched | The bar, the mixer and the bed are rebuilt. Every reference the desk holds is released first, so OBS can unload the collection and close cleanly. |
| More than 25 inputs | The bar scrolls. OBS's multiview shows the first 25. |
| More than 9 inputs | Inputs 10 and up have no number hotkey. |
| Channel 63 is already taken by another plugin at start | The bed takes the next free channel below it, and the choice is logged. |
| A lookup in §6.2 fails | That feature is off and the failure is logged. |

## 9. Testing

OBS has no test harness to build on, so S1 has one automated test and one
manual checklist.

- **Automated:** the loop check. It is written as a function over a plain
  list of "input → the inputs it has as layers", with no OBS types, and run
  from a small test program in CI. Cases: a layer on itself, a loop through
  two inputs, a loop through three, and a deep chain that is not a loop.
- **Manual:** `breadstudio/docs/DESK-SMOKE.md`, run on an installed build. It
  holds the seven items of §1.1, each row of §8, the two lookups of §6.2, and
  a check that an OBS scene collection made in stock OBS loads and switches.

## 10. Out of scope

- Overlay channels (vMix's Overlay 1–4).
- A fader or mute per bus; headphone monitoring of a bus; routing a bus into
  Master; delay, gain or EQ in the strip.
- Tally to anything outside BreadStudio. That is S2.
- Layer presets, borders and animation.
- The input number on the multiview. It needs an edit to OBS's multiview.
- Relabelling OBS's Transition button as "Auto".
- The brand typeface. S0 left it to S1; it is a theme change unrelated to the
  desk and is left out here.
- Any change to OBS's scene editor, sources dock or transitions.

## 11. Risks

| Risk | Handling |
|---|---|
| The audio bed relies on libobs's duplicate check, which was read and not run. | Proven first, with a test tone, before the mixer is built (§5.3). |
| Widgets found by name change upstream. | Fail soft, log, and check after every merge (§6.2). |
| Every input is drawn every frame for the tiles, and again when the multiview is open. | Measured in the plan on the done-when show. OBS's multiview already does this for 25 scenes. |
| A base kept in the audio bed counts as active for OBS all the time, so "is this source active" no longer means "is on Program". | The desk's tally comes from the Program and Preview scenes, not from source activity. S2's tally to BreadCamera must do the same. |
| S0 is unfinished: branding (its Task 3) and the installer are not built. | S1's plugin does not depend on them. The done-when asks for an installed build, so S0 must be finished before S1 is signed off, not before it is started. |
