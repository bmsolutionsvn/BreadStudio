# BreadStudio — the app's surface, the Project, and the vMix import — Design

Date: 2026-10-16
Status: draft for the owner's review.
Builds on: `2026-10-13-breadstudio-s1-desk-design.md` (the desk) and
`2026-10-12-breadstudio-s0-fork-baseline-design.md` (the fork and its merge
routine).
Base: OBS Studio 32.2.2.

## 1. Purpose

Two things make BreadStudio read as OBS-with-a-plugin rather than a product:

1. The operator sits in front of OBS's whole interface — Edit/View/Docks/Tools/
   Help, the Scenes, Sources, Audio Mixer, Scene Transitions, Controls and Stats
   docks, the scene editor — and the desk is the one part of it they use.
2. Saving a show is OBS's two-part idea: a **Profile** (settings) and a **Scene
   Collection** (inputs) that the operator has to manage separately and cannot
   hand to anyone.

A third stands in the way of anyone actually moving: an operator with a vMix
show has no way to bring it across, so the first thing BreadStudio asks of them
is to rebuild the show by hand.

This spec fixes all three. Everything of OBS the desk does not use is hidden;
the Profile and Scene Collection become one thing with one file, a **Project**,
the way vMix has a preset; and an existing vMix preset can be imported into a
Project (§5).

Nothing is deleted. The point is a small, honest app in front and a stock OBS
underneath that stays mergeable.

## 2. Decisions

### 2.1 Hiding OBS's surface

| # | Topic | Decision |
|---|---|---|
| K1 | Delete or hide | **Hide only.** No OBS source file is deleted, and none is edited for this spec (§6). The whole change lives in `breadstudio/desk/`, so upstream merges see nothing new. |
| K2 | The menu | BreadStudio's only menu is the desk's ☰ (`obsMainMenu()`, already built). It keeps **File** (Settings…, Exit), **Project** (§2.2) and **Projector**. Every other top-level menu is dropped from it. |
| K3 | Settings | OBS's **Settings** dialog is kept whole. It is the only place to set encoders, audio devices, video and hotkeys; rebuilding it in the desk is a separate, later job. |
| K4 | Source and filter properties | Kept. The desk's Add menu and a tile's ⚙ open OBS's own properties and filter windows; they are the desk's editing surface. |
| K5 | Projector | OBS's **Projector** submenus (Preview/Program, Scenes, Sources, Multiview) are kept, so a view can be put on a second monitor. The desk's outputs carry stream, record, NDI and SRT; a local monitor is OBS's projector. |
| K6 | Hidden docks | Every OBS dock is hidden: Scenes, Sources, OBS's Audio Mixer, Scene Transitions, Controls, Stats, and any dock a loaded plugin adds. The desk's own docks are named in one keep-list and are never hidden. |
| K7 | How hiding is done | One table of `{object name, what it is}` in a new desk unit. Applied once at `FINISHED_LOADING`, after `DeskLayout` and the ☰ menu, and again after a scene collection loads (a collection loads plugin docks). |
| K8 | A miss is not fatal | A name that is not found is logged and skipped; the rest still hides. This is S1 §6.2's rule, for the same reason: the names are OBS internals. |
| K9 | No new settings | There is no option to show the hidden surface again. BreadStudio is the desk; the OBS surface is not a mode. |
| K10 | The desk's docks | The desk's docks — the input bar and the Mixer — and OBS's **Scene Transitions** dock, which is the desk's transition control, are **movable and floatable but not closable**: the operator can detach one and park it on a second monitor, as in vMix, but cannot close it and lose it. With OBS's other docks gone (K6) there is nothing else to close and no way to end up with an empty dock area. |

### 2.2 The Project

| # | Topic | Decision |
|---|---|---|
| P1 | What a Project is | **One file** holding everything a show is: the inputs (OBS's scenes and scene items), their levels, ♪, bus sends, monitoring and AFV, and every setting — video, audio, stream, outputs, hotkeys. Like vMix's `.vmix` preset. The file is `My Show.bstd`. |
| P2 | One at a time | One Project is open at a time. New, Open, Save, Save As, and a Recent list. Opening or starting a new Project when the current one has unsaved changes asks first. |
| P3 | The menu | The ☰ has **Project** (New, Open…, Save, Save As…, Recent, Reveal in Explorer). OBS's separate **Profile** and **Scene Collection** menus are hidden and not reachable (K2). Their jobs map onto Project: OBS's two "new"s are **New**, opening a received collection or profile is **Open…**, and sending one is **Save As…**. Renaming or deleting a Project is done to its file in Explorer; there is no in-app rename or delete. |
| P4 | Under the hood | A Project **is** OBS's profile + scene collection, kept where OBS keeps them, so none of OBS's save/load is reimplemented. The desk binds the pair under one name, and opening a Project unpacks its profile and its scenes into the app's own folders and switches to both with the frontend API. |
| P5 | The file | A Project file is a container holding the profile folder and the scene collection, plus a small manifest (Project name, BreadStudio version, the OBS version it was made on). It is not OBS's own format and OBS cannot open it; OBS's own scene collection still imports through the desk, as S1 §3 says. |
| P6 | Sharing it | Sharing a Project is sending its file. **Save As…** writes the copy to send; **Open…** opens one that was sent. There is no server and no link. The installer registers `.bstd` so a double-clicked Project opens in BreadStudio. A Project **can carry its media**, as vMix's *Save Preset Bundle* does: Save As… offers to pack the files the show uses, so a show is one file to send. With the media left out and a file missing, Open… names each one and lets the operator locate it, as vMix's *Open Preset* does. |
| P7 | The stream key | A Project holds the stream key, because the profile it packs does. So **Save As…** offers "Leave out the stream key", unticked by default; with it ticked the saved copy carries no stream details, and Open warns when a Project was received with a key in it. |
| P8 | Where the recent list lives | In the plugin's own settings, next to the desk's "layout applied" flag. One Project = one path. |
| P9 | The unsaved marker | The title bar shows the Project name and a dot while it has unsaved changes, so Save and Save As are not done blind. |

Rejected:

- **Deleting the frontend, the dialogs and the unused plugins.** Smallest
  binary, but every upstream merge then deletes-versus-modifies the same files,
  which is exactly the merge pain S0's routine exists to avoid.
- **Hiding by editing OBS's `.ui` and its setup code.** Fewer lookups at
  runtime, but it edits OBS's most-changed files on every future merge.
- **Rebuilding Settings inside the desk now.** A large job, and nothing in the
  desk needs a setting that OBS's own pages do not already hold.
- **A Project as only a scene collection** (settings stay global). Then a show
  sent to someone else arrives without the video, audio and output settings it
  was built for, which is the thing vMix's preset exists to avoid.
- **A Project as a folder rather than a file.** Easier to diff, but a show is
  then a directory to zip before it can be sent.
- **Storing the Project in a database or the cloud.** P6's file is enough; a
  service is a later, separate decision.

### 2.3 Importing a vMix preset

| # | Topic | Decision |
|---|---|---|
| V1 | Direction | **One way, in.** BreadStudio reads a vMix preset (`.vmix`) and builds a Project from it. BreadStudio never writes a `.vmix` and never opens one as a Project to save back: a `.bstd` is ours, a `.vmix` is theirs. |
| V2 | Where it lives | The ☰ Project menu gains **Import vMix Preset…**. It is a separate entry from Open…, because a preset is converted, not opened: what a preset holds that the desk cannot back is dropped (V4), and what survives becomes a new `.bstd` the operator then saves. |
| V3 | What it produces | A **new, unsaved Project**, named after the preset, holding the inputs and settings that mapped. The Project open before the import is left alone; the operator Saves the new one where they want (P3). |
| V4 | What maps, and what is reported | §5's table is the rule. Everything that maps is created; everything that does not is listed in an **import report** shown once when the import finishes, naming each vMix input and why it was skipped or approximated. Nothing is dropped silently. |
| V5 | The desk's own rules still hold | An imported layer that would make an input contain itself is refused and reported (S1 §3), exactly as it would be if the operator had added it by hand. So is a preset with more inputs than the desk's bar holds. |
| V6 | It is a conversion, not a clone | The report is the record of that. The done-when is that an operator's own preset produces the same show on screen — the same inputs in the same order, the same shots, the same audio — with every difference on the report. |
| V7 | The format is not documented | vMix's preset format is private and has changed between versions. It has been read: the four presets in the owner's `F:\Stream` archive, and the installed vMix 26, are written up in the workspace roadmap's §10, with every field BreadStudio has no home for already assigned to a stage. An input `Type` this spec cannot name (15, 22, 100) is read from the input's `OriginalTitle` and put on the report rather than guessed. A vMix release that changes the file is caught by the report and by the smoke checklist, not by a crash. |

## 3. The app's surface

What the operator can reach once this is applied:

- **The desk:** the input bar (tiles, Add), Studio Mode's Preview/Program and
  transition column with its T-bar, the desk Mixer dock, the desk MultiView
  (as an output source), the Stream window, the Inputs/Settings windows, and
  the footer transport (`● Record`, Stream, Outputs, ☰).
- **The ☰ menu:**
  - **File** — Settings…, Exit. (Show Recordings, Remux, the settings and
    profile folders and the file-log entries are dropped.)
  - **Project** — New, Open…, Save, Save As…, Import vMix Preset…, Recent,
    Reveal in Explorer (§4).
  - **Projector** — OBS's projector submenus, unchanged.
- **OBS's Settings dialog**, whole, from File → Settings….
- **Its docks** — the input bar and the Mixer — detach and move to another
  monitor, but have no close button (K10).
- **OBS's source properties** and **filter properties** windows, from the desk.
- **The title bar** — the Project's name and its unsaved dot (§4.4).

## 4. The Project

### 4.1 What it holds

Everything the show is:

| Held | Where it already lives |
|---|---|
| Inputs: scenes, their bases, layers, order | OBS's scene collection |
| Levels, ♪, bus sends, 🎧, AFV | OBS's scene collection (source volume, mute, track switches, monitoring, the desk's AFV flag) |
| Video, audio devices, stream, outputs, hotkeys, accessibility | OBS's profile |
| Which inputs are on which output (§the outputs engine) | OBS's profile, as the outputs engine already stores them |

Nothing new is stored beyond the Project file itself.

### 4.2 The menu

```
Project ▸ New
          Open…               Ctrl+O
          Save                Ctrl+S
          Save As…
          Import vMix Preset…
          ─────
          Recent            ▸ My Show
                              Sunday Service
                              ─────
                              Clear
          ─────
          Reveal in Explorer
```

- **New** — a fresh, empty Project (no inputs, default settings), with the desk
  layout applied.
- **Open…** — a file dialog for `*.bstd`. The received Project is unpacked
  and switched to (P4).
- **Save** — writes over the open Project's file. With no path yet, it is Save
  As….
- **Save As…** — writes one `.bstd` file anywhere, with the P7 stream-key tick
  and the P6 media tick.
- **Import vMix Preset…** — converts a `.vmix` into a new Project (§5).
- **Recent** — the last ten, by path (P8).
- **Reveal in Explorer** — opens the folder holding the open Project.

### 4.3 What "share" is

Save As… to a file, then send it. Opening a Project someone sent is Open….
There is no account, no upload and no link (P6). The one thing that needs care
is P7: a Project carries the stream key, so Save As… can leave it out.

### 4.4 The unsaved marker

The title bar reads `My Show — BreadStudio`, and `My Show • — BreadStudio`
while the Project has changes not yet written. Closing the app, or opening
another Project, asks before losing them.

## 5. Importing a vMix preset

### 5.1 What it is

`Project → Import vMix Preset…` reads a vMix preset — one `.vmix` file; the
presets in the owner's archive have no companion file — and builds a new
Project from it. It is one-way (V1): nothing is written back, and BreadStudio
does not become a vMix preset editor.

The result is what an operator sees on screen: the same inputs, in the same
order, with the same shots and the same audio (V6). What vMix has and the desk
cannot back is not faked; it goes on the report (§5.3).

### 5.2 The mapping

Every vMix input becomes a desk input by the desk's own rules (S1 §3): its type
decides the base, its number decides the order, its title decides the name. A
vMix input always becomes one desk input; there is no path that produces a
half-built shot.

| vMix | Becomes | Notes |
|---|---|---|
| Colour | a Colour base ("Blank") | the vMix colour value is carried |
| Camera / Video Capture | a video capture device | the device is matched by name; if it is gone, the input is still made, empty, and reported |
| Video, Audio (file) | a media source | the file path is carried |
| Image | an image source | the file path is carried |
| NDI | the desk's NDI input | the NDI source name is carried (see the NDI input spec) |
| Browser, Web Browser | a browser source | only where OBS's browser plugin is built in; otherwise reported |
| Desktop Capture, Screen | a display or window capture | a named window that has gone is reported |
| Title, GT Title | the imported title, drawn as it was (S7) | its text, colours, images and static layout are drawn; its animation, its data binding and anything that changes while on air are reported, because they are BreadOverlay's (O2) |
| Audio input | an audio capture device | matched by name, as a camera is |
| Video Delay, Virtual Set, PowerPoint, Stream, Video Call, Mix, Photos, Replay | nothing | no desk equivalent; reported |
| input number | the input's order in the bar | S1 §3: number is position |
| input title | the input's name | |
| position, size, crop, rotation | the base's geometry | where vMix has one and the OBS source supports it |
| volume, mute | the strip's fader and ♪ | |
| Overlays 1–4 | nothing | the desk has no overlay bus (S1 §10); the inputs they held are still imported, the routing is reported |
| the active and preview numbers | the desk's initial Program and Preview | applied once the inputs exist |
| transitions | nothing | the desk uses OBS's transitions; reported |
| stream, output and recording settings | the Project's settings half | carried where OBS has the same setting — service, key, encoders, recording path |

### 5.3 The report

When the import finishes, a report window lists every row of the table that was
**skipped** or **approximated**, each naming the vMix input, what vMix had, and
why it was left out. It is the same window whether one input was dropped or
fifty. The window can be copied to the clipboard as text, so an operator can
keep it with the project.

No import is silent: an empty report says the whole preset came across.

### 5.4 How it is done

- One new unit in the desk plugin, `VmixPreset`, in two halves on purpose:
  - **Reading** turns the preset's XML into a plain list — one entry per input,
    carrying its kind, name, number, geometry, audio and file or device — with
    no OBS types and no Qt widgets. This half is testable on its own (§9).
  - **Building** walks that list and creates the OBS sources through the desk's
    own `InputModel`, so an imported input is made exactly as an Add-menu input
    is, with the same fixed shape, the same loop refusal and the same numbering.
- The XML is read with Qt's `QDomDocument`, which the desk already links.
- The report window is the desk's own widget; the desk raises it once, after the
  build half finishes.
- A preset holding more inputs than the bar supports imports the first N and
  reports the rest (V5).
- Nothing here edits an OBS file. `UPSTREAM-EDITS.md` gains no row.

## 6. What is hidden

Named as OBS names them, so `check-edits.ps1` and the smoke checklist can hold
the list:

| Kind | Object name | What it is |
|---|---|---|
| Menu | `menuBasic_MainMenu_Edit` | Edit — copy, undo, transform, order, scale, Advanced Audio Properties |
| Menu | `viewMenu` | View — fullscreen interface, toolbars, status bar, stats, always-on-top, scene-list mode |
| Menu | `menuDocks` | Docks — lock, side, reset |
| Menu | `menuTools` | Tools — auto-configuration wizard, plugin manager, script playground, websocket |
| Menu | `menuBasic_MainMenu_Help` | Help — portal, logs, crash logs, updates, About |
| Menu | `profileMenu`, `sceneCollectionMenu` | OBS's two menus, replaced by Project (P3) |
| Dock | `scenesDock` | OBS's Scenes list |
| Dock | `sourcesDock` | OBS's Sources list |
| Dock | `mixerDock` | OBS's Audio Mixer (the desk Mixer replaces it) |
| Dock | `controlsDock` | OBS's Controls — Studio Mode, recording, virtual cam, settings |
| Dock | `statsDock` | OBS's Stats |

`transitionsDock` is **not** in this list: OBS's Scene Transitions dock is the
desk's own transition control — the layout places it beside the mixer and the
transition and its duration are chosen there (S1 §4.1). It stays.
| Menu item | `actionShow_Recordings`, `actionRemux`, `actionShowSettingsFolder`, `actionShowProfileFolder` | the File entries that stay outside the desk's job |

Anything a **plugin** adds (a dock, a Tools entry) is hidden too, unless it is
one of the desk's own: the keep-list is the desk's dock object names, and the
table hides every other dock.

## 7. How it is done

**Hiding**

- One new unit in the desk plugin, `DeskChrome`, holding the table of §6 and
  the keep-list of the desk's own docks.
- It reaches the widgets the way `DeskLayout` already does: from
  `obs_frontend_get_main_window()`, by object name, on the UI thread. Docks are
  hidden with `QDockWidget::hide()` and also removed from the **Docks menu**
  OBS builds — but that menu is hidden too (K2), so hiding the widget is
  enough.
- The menu is trimmed in the one place OBS's menus are already handled: the
  `obsMainMenu()` of the ☰ (built from the menu bar's actions) keeps only the
  File and Projector menus, adds the desk's own **Project** menu, and drops the
  File entries of §6.
- The desk's own docks are made movable and floatable and **not** closable
  (K10), at the point the desk creates them.
- Applied at `FINISHED_LOADING`, after `DeskLayout` and the ☰, and again on
  `OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED`, because loading a collection
  re-adds plugin docks.

**The Project**

- One new unit in the desk plugin, `Project`, next to `DeskLayout`:
  - It knows the open Project's path and its "changed" flag, and it is the only
    place that writes the file.
  - **Open** unpacks the container into the app's own profile and scenes
    folders under a name taken from the manifest, then calls
    `obs_frontend_set_current_profile` and
    `obs_frontend_set_current_scene_collection` (P4). OBS does the load.
  - **Save As…** copies the current profile folder and the current scene
    collection into the container, writes the manifest, and remembers the path.
  - **Save** writes to the same path, or falls through to Save As… the first
    time.
  - Changedness comes from OBS's own save callbacks
    (`obs_frontend_add_save_callback`), which fire when the profile or the
    collection is written, and from the desk's own edits.
- The container is a zip: the profile folder, the scene collection, and
  `project.json` (name, BreadStudio version, the OBS version). OBS's own files
  are stored as they sit, so a Project round-trips without loss.
- Nothing here edits an OBS file. `UPSTREAM-EDITS.md` gains no row.

**The import**

- `VmixPreset` (§5.4) does the whole job: it reads the preset into a plain
  list, builds the sources through `InputModel`, and hands the result to
  `Project` as a new, unsaved Project (V3). It shares `InputModel` with the Add
  menu, so an imported input and a hand-made one are the same thing.

## 8. Failures

| Case | Behaviour |
|---|---|
| A dock or menu name is not found | Log it and keep going (K8). Hiding is best-effort; a miss is a warning in the log, never a crash and never a reason to leave the app in OBS's shape. |
| A plugin adds a dock after the collection loads | Hidden on the next collection load (K7). A dock that appears and is not the desk's is a bug for the smoke checklist, not for the code. |
| OBS renames an object on an upstream merge | The lookup fails, it is logged, and the smoke checklist's chrome rows catch it. Same handling as S1 §6.2 — an upstream rename is a checklist failure, not a build failure. |
| A Project file is not a Project, or is from a newer BreadStudio | Refused with a plain reason; the open Project is left untouched and nothing is unpacked. |
| A Project holds a stream key (P7) | Open warns once that the Project carries stream details, naming the service, so a received show is not streamed blind. |
| A Project's media is missing | Open names each file it cannot find and lets the operator locate it (P6): a received show comes up with its media in place, or with that input empty and named. |
| A Project names a profile or a collection that already exists | The unpack uses the manifest's name; if it collides, a numbered name is used and the log says so. The copied-in Project never overwrites an existing profile or collection. |
| A Project is opened while the current one has unsaved changes | Ask: Save, Discard, or Cancel. Cancel leaves everything as it was. |
| A `.vmix` is not XML, or is not a vMix preset | Refused with a plain reason; the open Project is left untouched and nothing is created. |
| An imported input names a device or a file that is gone | The input is still made, empty, and reported — the shot exists and the operator can point it at something else. |
| The preset has more inputs than the bar supports | The first N are imported, the rest are reported (V5). |
| An imported layer would make an input contain itself | Refused and reported (V5), exactly as a hand-added layer would be. |

## 9. Testing

- **Automated:** the Project's manifest and its path handling (name, extension,
  collision numbering, the "changed" rule) as a plain function over strings and
  paths, with no OBS types, run from the desk's existing test program. The
  round-trip of a real profile and collection is manual.
- **Automated:** `VmixPreset`'s reading half (§5.4) over recorded `.vmix` XML
  fixtures — the input list it produces from a preset, and the report it
  raises for a preset full of inputs the desk cannot back. No OBS types, so it
  runs in the same test program. The fixtures are real presets, cut down, and
  kept beside the test.
- **Manual:** rows added to `breadstudio/docs/DESK-SMOKE.md`, run after every
  upstream merge:
  1. The ☰ holds File (Settings…, Exit), Project and Projector, and nothing
     else.
  2. None of the docks of §6 is visible or reachable; the desk's docks are.
  3. A desk dock detaches to another monitor and has no close button (K10).
  4. Settings… opens, and every page of it works.
  5. A source's properties and its filters open from the desk.
  6. A projector opens on a second monitor.
  7. A show is built, saved as a Project, the app is closed, and the Project is
     opened again: inputs, order, layers, levels, bus sends and AFV come back
     (S1 §1.1 item 6).
  8. That Project is opened on another machine and every input, setting and
     output is there.
  9. **Save As…** with "Leave out the stream key" ticked writes a copy that
     carries no stream details.
  10. The owner's own `.vmix` is imported: the inputs, their order, their shots
      and their audio match what vMix showed, and the report names every
      difference (V6).
  11. A Project saved **with** its media opens on another machine with every
      file in place; one saved without it names the missing files and lets each
      be found (P6).

## 10. Out of scope

- Rebuilding OBS's Settings pages inside the desk (K3).
- Deleting any OBS code, dialog, plugin or workflow (K1).
- Hiding OBS's settings **pages** (Stream, Output, Audio, Video, Hotkeys,
  Accessibility, Advanced); they all stay.
- A cloud Project, an account, or a share link (P6).
- Opening OBS's own scene collection format as a Project; that stays S1's
  import path.
- Writing a `.vmix`, or opening one as a Project and saving it back as one
  (V1). The import is one way.
- vMix's title files — `.xaml` titles and GT titles — beyond their text: those
  import as a plain text source and the rest is reported (V4).
- vMix's shortcuts, activators, scripting and data sources.
- The first-run experience beyond hiding the auto-configuration wizard; a new
  install still opens the desk's layout (S1).
- Branding, the installer and the updater (S0's work, unchanged).