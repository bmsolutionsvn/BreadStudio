# Desk smoke checklist

Run on an installed build before a release and after every upstream merge.
Needs two cameras, a media file, a microphone and a second monitor.
Spec: `2026-10-13-breadstudio-s1-desk-design.md`.

## A. The show (spec §1.1)

Start from a first run (no settings folder).

1. With + Add alone, make: camera 1, camera 2, the media file, a Blank with
   both cameras as layers placed side by side, and the microphone.
2. Open View → Multiview on the second monitor. Cut and fade between the four
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

## B. Failures (spec §8)

1. Studio Mode off: the warning row shows, and its button switches Studio
   Mode back on.
2. Delete the input on Program, and an input that is a layer of what is on
   Program: both refused.
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

## C. Things found by name (spec §6.2): check after every upstream merge

1. Drag a tile to a new place: it moves, the numbers follow, and the
   multiview follows. The log has no line
   `scene list 'scenes' was not found` and none about `scenesReordered`.
2. Tools → Desk layout: the arrangement is applied. The log has no line
   `dock '…' was not found`.

## D. A collection from stock OBS

1. Load a scene collection made in stock OBS that has: a scene with several
   sources, a scene with a group, a scene holding another scene, and an
   empty scene. Every scene is a tile and switches. The empty one is black,
   its ♪ is greyed out, and its ⚙ window has "Base properties…" greyed out.

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
