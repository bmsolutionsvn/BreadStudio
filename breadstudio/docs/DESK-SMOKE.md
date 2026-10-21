# Desk smoke checklist

Run on an installed build before a release and after every upstream merge.
Needs two cameras, a media file, a microphone and a second monitor.
Spec: `2026-10-13-breadstudio-s1-desk-design.md`.

A new check is appended to the end of its section, never inserted, so the
numbers cited in the Runs log below stay true.

## A. The show (spec §1.1)

Start from a first run (no settings folder).

1. With Add Input alone, make: camera 1, camera 2, the media file, a Blank with
   both cameras as layers placed side by side, and the microphone.
2. Open Projector → Multiview on the second monitor. Cut and fade between the four
   picture inputs for two minutes. On the bar and on the multiview, the red
   frame is always what is on Program and the green one what is on Preview.
3. The microphone is heard whatever is on Program. A camera's audio is heard
   only while that camera is on Program, alone or inside the two-box.
4. Test tone (`breadstudio/desk/test/make-tone.ps1`, added as a Media file
   input, looping): bus M reads -12 ± 0.5 with the tone on Program and AFV
   lit; with something else on Program and AFV unlit; and with the tone on
   Program and AFV unlit.
5. Switch the microphone's send A off and every other input's send A on.
   Settings → Output → Recording: record tracks 1 and 2. Record 20 seconds
   while speaking. The file's first audio track has the voice; its second
   does not.
6. Close and restart. Inputs, order, layers, levels, sends, ♪ and AFV are as
   they were.
7. OBS's Scenes and Sources docks were not opened at any point above.
8. Pause Inputs (`Pause Inputs` in the footer) with a looping media input
   playing: it pauses; the button reads `Resume Inputs`; pressing it carries
   on from where it stopped. With nothing playing the button is disabled.

## B. Failures (spec §8)

1. Studio Mode off: the warning row shows, and its button switches Studio
   Mode back on.
2. Delete the input on Program, and an input that is a layer of what is on
   Program: both allowed.
3. Add an input as a layer of one of its own layers: refused.
4. Unplug a camera: its tile goes black; nothing else changes. Plug it back:
   the picture returns.
5. Scene Collection → New, then switch back, with a ⚙ window open: the bar
   and mixer follow each time. After closing, the log has
   `Number of memory leaks: 0` and no line containing `remaining`.
6. Make 26 inputs (Blank): the bar scrolls, and the buttons under the tiles
   stay whole; the multiview shows the first 25 (Settings → General →
   Multiview Layout: Scenes only, 25 Scenes).
7. Bind "Input 2 to Preview": it follows whatever is in place 2.
8. Press Cut on a tile during a three-second fade: ignored; the selected
   transition is unchanged afterwards.
9. Closing an input that is on Program puts the first other input on Program
   and Preview and removes the input, and the show never goes black.

## C. Things found by name (spec §6.2): check after every upstream merge

1. Drag a tile to a new place: it moves, the numbers follow, and the
   multiview follows. The log has no line
   `scene list 'scenes' was not found` and none about `scenesReordered`.
2. The desk layout is applied automatically once per `LayoutVersion` (stored
   under `BreadDesk` in the user config); it is not reachable by hand. The log
   has no line `dock '…' was not found`.
3. No Start Streaming button is visible anywhere in the window (the desk's own
   Stream button is the one that should be there), and the log has no line
   `'streamButton' was not found`. An output set to Program shows a fade as a
   fade (section H, item 2): OBS still draws the main mix first.

## D. A collection from stock OBS

1. Load a scene collection made in stock OBS that has: a scene with several
   sources, a scene with a group, a scene holding another scene, and an
   empty scene. Every scene is a tile and switches. The empty one is black,
   its ♪ is greyed out, and its ⚙ window has "Base properties…" greyed out.

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
9. Output 2 set to MultiView, at 1280×720: its receiver shows Preview and
   Program across the top and the inputs below, each with its number, name
   and a frame that matches the input bar's — red on Program, green on
   Preview, idle otherwise; with Studio Mode off the Preview box shows
   Program and nothing is green. The Program box is framed red and the
   Preview box green (idle with Studio Mode off). Each title sits in a strip
   under its picture, never over it, and an input with a category tag on
   Program or Preview is still red or green (the tag shows only while it is
   idle). At 3840×2160 and at 1280×720 the frames are the same share of the
   box. Changing Program, Preview or the input list
   is reflected within a second; changing the size restarts only its own
   receivers.
10. Output 2 set to NDI, and a second PC running NDI Studio Monitor or vMix:
    the output appears by name within five seconds, plays with sound, and the
    Outputs window reads one receiver; closing the receiver returns it to 0.
    With the DLL renamed away, the NDI column is greyed with the reason and
    every other output still works.
11. Output 2 set to OMT, received by another BreadStudio's OMT input, in all
    three modes (OMT, OMT-HX HEVC, OMT-HX H.264): picture and sound, the
    receiver count in the Outputs window, and a mode change followed without
    touching the input.

## I. The input settings window (spec §6)

Spec: `2026-10-16-breadstudio-input-settings-design.md`. Aspect Ratio is never
shown — OBS has no such source property.

1. In an input's settings, the General page's Source row shows the input's source, and
   its **Change…** opens `Change source: <name>` with **This source** first and selected,
   then every kind. On This source, a change takes effect live, OK keeps it and Cancel puts
   it back. On any other kind — a camera input changed to a media file, an NDI input changed
   to an SRT stream — OK puts that new source in place of the old one: the input keeps its
   name, its place in the bar, its layers (and every layer of it elsewhere), its Position,
   its filters, its fader, mute, monitoring, sends, balance, sync offset and AFV, and the old
   source is gone from the Audio Mixer.
2. The gear opens a window titled `Input: N <name>` (N the input's place in the
   bar) with the General page shown. The page list is exactly the pages that
   input's type backs: General and Position for every input, each colour/effect
   page whose filter OBS has registered, Layers / MultiView and Triggers for the
   desk, and Advanced; and Audio for a source with sound. A page with nothing behind
   it is not listed. No button on any page opens one of OBS's own windows.
2a. Audio: Balance (-100 to 100), Sync offset (ms) and Downmix to mono each change what is
   heard live and survive a restart; the fader, mute, monitoring and sends are not on the
   page, they stay on the mixer strip.
3. Rename, Category, AFV and Mouse Click Action each take effect — the name on
   the tile, the category tint on the tile and the MultiView slot, AFV on the
   mixer strip and the tile, and the click action on what a tile click does — and
   each survives closing and reopening the window.
4. Each colour/effect page (Colour Adjust, Colour Key, Colour Correction,
   Effects) adds, configures and removes its filter on the base, and the picture
   changes live.
5. Position moves the base within its own input (X/Y/scale/rotation/crop); Layers
   / MultiView lists the layers with hidden/shown, up/down and remove, and its
   small live view follows what the MultiView draws (red or green while the input
   is on air, whatever its category). **Edit…** on a selected layer opens
   `Layer: <name>` with the same X/Y/scale/rotation/crop as Position, for that
   layer only; each change shows live in Preview, and dragging the layer in Preview
   then reopening Edit shows the new numbers.
6. A trigger (on Program → Start recording) fires when the input becomes Program,
   and does not fire on starting OBS or loading a collection.
7. `Copy From…` copies the desk settings (AFV, category, mouse-click action, the
   automatic behaviours) from another input.
8. A base whose type has no deinterlacing control shows none.
9. Removing a layer asks for no confirmation, and the **Undo** button beside Remove / Up /
   Down puts it back exactly as it was — the same place in the list, the same on/off tick and
   the same picture geometry — and then greys out again. Up, Down or another remove clears it,
   so Undo never puts a layer into a slot that has since changed.
10. A fresh profile has no Desktop Audio or Mic in the mixer, Settings → Audio
    shows no Global Audio Devices group, and an existing profile's global
    devices are gone after a restart.

## J. OBS's surface is hidden

Run after every upstream merge: these are object-name lookups into OBS's window,
and an upstream rename breaks them silently.

1. The ☰ menu holds **Project**, **File** (Settings…, About, Exit) and **Projector**,
   and nothing else. No Profile or Scene Collection, and no Edit, View, Docks,
   Tools or Help.
2. OBS's own menu bar is not visible anywhere, and its Scenes and Sources
   toolbars are gone.
3. File holds Settings…, About and Exit, and no Show Recordings, Remux, Show
   Settings Folder or Show Profile Folder.
4. Projector → Multiview opens a Multiview projector window, and the monitor
   list it offers is the one from launch — a monitor plugged in after
   BreadStudio started is not offered until it restarts (the View menu it used
   to refresh from is gone).
5. No OBS dock or plug-in dock is visible — Scenes, Sources, Audio Mixer,
   Controls and Stats included — and the Docks menu does not exist to bring
   them back.
6. The desk's input bar and Mixer docks are visible; the Scene Transitions dock is
   not shown unless the footer's Transitions button pins it.
7. A desk dock can be dragged out to float on a second monitor, and has no close
   button; Scene Transitions has none either.
8. Settings… opens and every page of it works.
9. An input's properties and its filters open from the desk.
10. A search for `was not found` in the newest log under
    `%APPDATA%\obs-studio\logs` finds nothing.
11. The footer's **Transitions** button is unchecked and OBS's Scene Transitions
    dock is not shown; pressing it pins the dock beside the mixer, and either
    choice survives a restart.
12. The input bar and Mixer are at the bottom on every launch, whatever the saved
    dock state.

## K. The Project

Spec: `2026-10-16-breadstudio-cleanup-design.md` §2.2 and §4.

1. The ☰ has a **Project** menu with New, Open…, Save, Save As…, Import vMix
   Preset…, Recent, Reveal in Explorer, and no Profile or Scene Collection menu.
2. A show is built, **Save As…** writes a `.bstd`, the app is closed, and
   **Open…** brings back the inputs, order, layers, levels, bus sends and AFV.
3. **Save** after a change writes without asking for a path, and the title bar
   shows ` •` until it does.
4. A `.bstd` opened by hand in Windows Explorer, or by any zip tool, shows
   `project.json`, `profile/` and `scenes/`.
5. **Save As…** with the media tick packs the media, and the file opens on
   another machine with every file in place.
6. **Save As…** with **Leave out the stream key** ticked writes a copy whose
   profile carries no stream details (service, server, key).
7. The owner's own `.vmix` (`F:\Stream\CACAS24\CACAS24.vmix`, the 27-input
   show) is imported: the inputs, their order, their shots and their audio
   match what vMix showed, and the report names every difference (V6).

## L. GT titles

Spec: `2026-10-19-breadstudio-gt-title-design.md`; §1.2 and Tasks 7 to 11.
Needs `F:\Stream` (the owner's 25 `.gtzip` files and the EAFC24, CACAS24 and
FVPL `.vmix` presets); without it, skip this section.

1. Each of the 25 `.gtzip` files under `F:\Stream` opens as an input and, with
   the values saved in the file, matches vMix's own rendering of the same file —
   layout, fonts, colours, masks and shadows — with differences no larger than
   anti-aliasing.
2. An input's ⚙ → **Title** tab lists every editable field and no others — across
   the 25 files that is 158 text fields (156 TextBlocks and the two Timer files'
   Text3D) and 55 picture fields; typing in a text field or choosing a picture
   shows on Program on the next frame.
3. `Timer.gtzip` added by itself shows the current time and ticks every second;
   imported from CACAS24 the same title is a 30:00 countdown in `mm:ss` that
   Start, Pause, Stop and Reset control.
4. A field's own font, size, colour and style can be changed in the Title tab,
   and the ones a preset saved are drawn: EAFC24's `TimeMHC` is cyan, italic,
   bold Helvetica Neue at 192.
5. Importing `F:\Stream\EAFC24\EAFC24.vmix` makes every GT input a GT title
   input showing the values the preset saved: `TagWinLeft` reads "Player Left"
   and `TagWinRight` reads "Player Right", though the preset names the file
   `C:\EAFC24\GT\TagWin\TagWin.gtzip`, which is now under `F:\Stream`.
6. Saving the Project and restarting BreadStudio brings every title back with
   its values and its styles.
7. A missing font, a missing `.gtzip` and a missing picture are each named to
   the operator; none crashes or leaves a blank input without saying why.
8. With 8 GT title inputs in the Project and 4 of them shown on Program as
   layers at 1080p60 for ten minutes, OBS's "frames missed due to rendering lag"
   stays 0.
9. It draws over Program with soft edges free of dark fringes.
10. Setting `NameL1.Text` through the source's settings JSON changes it on the
    next frame.
11. Re-saving the file in GT Designer updates the input within two seconds.
12. Setting MAN OF THE MATCH's picture field to its 16 MB PNG leaves OBS's
    "frames missed due to rendering lag" unchanged.
13. **Title** is listed after Image in Input Select.
14. Choosing `Matchup.gtzip` shows its thumbnail and enables OK; OK makes an
    input named `Matchup` whose tile shows the title.
15. Choosing `not-a-zip.gtzip` keeps OK greyed and shows the error.
16. With `bread-gt` removed from the plugins folder, the Title row is absent.
17. With `Matchup.gtzip`, the Title tab lists 20 text fields and no shapes.
18. Typing `Nguyễn Văn Á` shows correctly on Program.
19. Changing a field's size and colour applies them, and Reset restores them.
20. With `Timer.gtzip`, a countdown set to 00:00:10 counts down and its Start,
    Pause, Stop and Reset buttons work.
21. Saving the Project and restarting brings the values and styles back.
22. In the EAFC24 import, `TagWinLeft` reads "Player Left" and `TagWinRight`
    reads "Player Right", each with a "found at" row.
23. In the EAFC24 import, `TimeMHC` is cyan, italic and bold at 192, showing
    20:00 stopped.
24. In the EAFC24 import, the report lists the data links and the overlays row.
25. Importing the CACAS24 and FVPL presets, the FVPL logos are drawn, each with
    a "found at" row, and CACAS24's Timer shows 30:00, not "27:21".
26. On a 1280×720 canvas the titles scale to fit, not crop.

Not yet run — the owner runs this section on the production PC and records the
date, the results and the render test's per-file scores.

## Before a run

- The build folder must hold no copies of plugin files. OBS loads every DLL in
  `obs-plugins\64bit` as a plugin, so a stray copy (a file-sync
  `*.sync-conflict-*.dll`, a backup) loads each plugin twice: scripts run
  twice, and the log ends with one or two memory leaks.
- The session that shows OBS's first-run wizard logs
  `Number of memory leaks: 1` when the wizard is cancelled. Stock OBS 32.2.2
  does the same without the desk. Read B5's leak count from a later session.

## Runs

One entry per run: the date, the build, installed or build folder, and what
failed.

- **2026-10-14, `f92157f57`, build folder.** A PC with no camera, one monitor
  and a silent microphone, so this run does not sign S1 off.
  **Every item that could be run passed.**
  - Run with stand-ins, to be repeated with the real hardware: A1, A2, A3 and
    A5. Three looping media files stood in for the two cameras and the media
    file, a looping tone with AFV unlit for a voice at the microphone, and a
    windowed multiview for the second monitor. The program was driven by
    posted clicks and its state read back through OBS's websocket; each media
    file's path was set through the websocket, not picked in the properties
    window.
  - Not run: B4 (unplug a camera). B7 was run by triggering the hotkey through
    OBS, not by pressing a bound key.
  - Run as written: A4, A6, A7, B1 to B3, B5, B6, B8, C1, C2 and D.
  - Measured: A2, 85 cuts, fades and clicks in two minutes with no wrong frame
    on the bar or on the multiview; A4, bus M at -12.0 on all three readings;
    A5, the voice tone at -11.8 dB on track 1 and absent from track 2.
  - Still owed: the run with two cameras, a microphone and a second monitor,
    and the run on an installed build (waits for S0).
