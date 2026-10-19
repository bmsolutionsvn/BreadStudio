# BreadStudio — owner's backlog — 2026-10-16

Captured from the owner in conversation, with the screenshots they supplied.
Each row is a change to make. "Where" names the unit the change lands in; each
group becomes its own plan when it is worked.

Status: **every item has landed on `main`** (report below, 2026-10-19). Plan 1 (hiding OBS's surface)
is done and shipped. Nothing here has been run on the desk yet; each row names its DESK-SMOKE check.

## Report — 2026-10-19

| Item | Commit(s) | What landed | Check, and what is open |
|---|---|---|---|
| **B1** Remove a layer, with Undo | `b508474b1` | No confirmation; an **Undo** button beside Remove / Up / Down puts the layer back exactly (place, tick, geometry); Up, Down or another remove clears it. Remove only, not Add or the tick. | DESK-SMOKE I.9 |
| **B2** Close an input that is on air | `a41ae9121` | On Program → it goes to Preview and Program moves to the first other input; Preview follows. Layers of it elsewhere go with it. The last input still cannot be closed. | DESK-SMOKE (delete rows) |
| **B3** Scene Transitions as a button | `130312782` | The dock is shown by a footer button and can be pinned into the layout; the pin persists. Other desk windows were not given the same button. | — |
| **B4** Source properties; changing an input's source | `7cc842d65`, `405a0dc4e` | **Change…** opens `Change source: <name>`: **This source** first (edited in place, Cancel reverts), then every kind; any other kind puts a new source in place of the old one, keeping name, bar place, layers, Position, filters, fader, mute, monitoring, sends, balance, sync offset and AFV (vMix: "preserving all settings"). **Source properties…** (it opened OBS's own window) is gone; a new **Audio** page has balance, sync offset and downmix to mono — what OBS's Advanced Audio has and the mixer strip lacks. | DESK-SMOKE I.1, I.2a. Open: replacing a camera with the *same* device may find it busy until the old one closes — use This source for that. |
| **B5** MultiView after vMix | `051af35de`, `74d9114e0`, `6964ff751` | Review against the vMix User Guide appended to `2026-10-15-breadstudio-multiview.md` (the output MultiView, the input's Layers / MultiView page — the owner's screenshot — and each output's shape). Landed: letterbox, two-by-four, green/red Preview and Program frames, titles in a strip under the picture, **tally wins over a category tag** (a tagged input on air was not red), frame and font scaled to the canvas, and **Edit…** for a layer's X / Y / scale / rotation / crop. | DESK-SMOKE H.9, I.5. Six owner decisions are listed at the end of the review (layouts beyond + 8, Program/Output heading, audio meters, layer templates, Import / Export format, layer borders). A MultiView output at another aspect than the canvas is stretched, as Program is. |
| **B6** README | `834bfc8f0` | Rewritten for BreadStudio. | — |
| **B7** Rebuild with the new OMT-HX | `674314546`, `79172d523` | omthx is built from the `OMT-HX` checkout. At OMT-HX `ae96aa1` (plain OMT takes limited-range NV12): `omt-hx`, `bread-omt-test` and `bread-onair-test` build (`build_vs2022`, RelWithDebInfo) and both tests pass (2026-10-19). | Open: the parallel session's `omt-hx.cpp` / `omt-output.cpp` changes (fast premultiply; NV12/UYVY to omthx instead of BGRA) are built and tested above but **not committed** — the owner's call. `bread-onair-test` needs `libobs` and `frontend\api` on PATH to start. |
| **B8** Title and logo | `f7e8e38b1`, `19fce91aa` | BreadStudio's own title, icon and About. | — |

---

## B1. Removing a layer needs no confirmation, but needs an undo

**Owner:** "Close layer no need confirm, but should have button to undo."

Today the Layers / MultiView page removes a layer after a confirmation. Remove
the confirmation and give the removal an **Undo** — the layer is put back where
it was, on its own, with no dialog.

**Where:** `breadstudio/desk/InputPages.cpp` (the Layers / MultiView page),
`breadstudio/desk/InputModel.{hpp,cpp}` (the layer list).

**Decisions to take when it is planned:** whether Undo is a button beside the
list, a toast, or a `Ctrl+Z` scoped to the desk; how long it lives; whether it
also covers Add and the on/off tick, or only Remove.

## B2. Closing an input that is on air

**Owner:** "When input in program but close, just switch to Preview and close.
If Preview and Program same in close input → Switch to Input 0 at program."

Today deleting an input that is on Program (directly or as a layer of what is on
Program) is **refused** (S1 §8). The owner wants it allowed, with the show kept
safe:

- the input is on **Program** only → put it on **Preview** instead, then delete;
- the input is on **Program and Preview** → put **input 0** (the first input) on
  Program, then delete;
- **layers** of the closing input elsewhere are removed with it, as they are now.

**Where:** `breadstudio/desk/InputModel.{hpp,cpp}` (delete), `desk.cpp` (the
scene-change events), `breadstudio/desk/SourceTiles.cpp` (the delete action).

**Decisions:** what happens when it is the only input (nothing left to put on
Program); what "on Program as a layer" does — the input on Program stays, only
the layers naming the closing input go.

## B3. Scene Transitions as a pin-able button, as vMix's Overlay

**Owner:** "Screen Transition should be a button to show windows because we not
use it much (Like Overlay button in vmix) to pin it on main layout."

The supplied screenshot shows vMix's strip of buttons — **Overlay**, the
transition list, the audio mixer, the multiview, capture and lock — each of
which shows or pins its window.

Our Scene Transitions dock is always on screen (`DeskLayout::applyLayout` places
it beside the mixer). It should instead be **shown by a button** on the desk's
footer, and that button should be able to **pin** the dock into the layout.

**Where:** `breadstudio/desk/InputBar.cpp` (the footer, next to the ☰),
`breadstudio/desk/DeskLayout.cpp` (whether the dock is placed at all).

**Decisions:** does the button show a floating window, dock it, or toggle
between the two; does the pinned state persist (it should — a `LayoutVersion`-
style flag or the plugin's own setting); whether other desk windows (Outputs,
Stream, Input Settings) get the same treatment in one row of buttons.

## B4. Source properties do not work, and an input's source cannot be changed

**Owner:** "Sources Properties not work correctly, and I cannot change source of
input after add."

Two bugs behind one report:

1. **Source properties are wrong.** The button that opens the base's OBS
   properties does not do what it says. (The Input Settings window's General
   page has "Source properties…"; the `Copy From…` row and the pages around it
   were reworked in the input-settings plan, and something there is broken.)
2. **An input's source cannot be changed after it is added.** OBS can replace a
   source inside a scene item; the desk does not offer it.

**Where:** `breadstudio/desk/InputPages.cpp` (General page),
`breadstudio/desk/InputSettings.cpp`, `breadstudio/desk/InputModel.{hpp,cpp}`,
`breadstudio/desk/PropertiesPage.cpp`.

**This is a bug, not a feature.** Start by reproducing both against the running
build and finding the real cause before changing anything.

## B5. The MultiView should follow vMix — review position, size and crop per output

**Owner:** "Multiview should follow vmix, and need a review to see what position,
size, crop, ... for each output."

The supplied screenshot is the desk's **Layers / MultiView** page: ten slot
previews drawn as coloured diagrams (1 over 2, side by side, diamond, and so
on), with Import / Export / Add / Remove, and the input's own preview below.

The work is a **review first**: take vMix's Multiview layout (its fixed boxes,
where Preview and Program sit, how many inputs, what the labels say, how a
missing input is drawn) and write down where BreadStudio's own
`bread_multiview` differs — for **each output** that can carry it (a window, a
monitor, an NDI or SRT output), since the box a view gets depends on the canvas
it is drawn into. Then change `MultiviewLayout` and `MultiviewSource` to match.

**Where:** `breadstudio/desk/MultiviewLayout.{hpp,cpp}`,
`breadstudio/desk/MultiviewSource.cpp`, `breadstudio/docs/2026-10-15-breadstudio-multiview.md`.

## B6. Rewrite the README

**Owner:** "Rewrite readme."

`BreadStudio/README.md` is still OBS's. It must describe BreadStudio: what it is,
what it is for, how to build it here, and where the desk's own docs live.

**Where:** `BreadStudio/README.md`, and check `breadstudio/docs/` for a doc the
README should point at.

**Note:** `README.md` sits outside `breadstudio/`, so editing it is an OBS-file
edit — add it to `UPSTREAM-EDITS.md` or replace it from the packaging step, and
make sure `check-edits.ps1` still passes.

## B7. Rebuild with the new OMT-HX fix

**Owner:** "Rebuild with new fix OMT-HX."

Take the newest `OMT-HX` (the sibling repo at `F:\Coding\BreadMedia\OMT-HX`,
which has uncommitted work on its h264 codec plan), rebuild `breadstudio`'s
OMT input against it, and run the desk's OMT tests
(`bread-omt-test`, `bread-onair-test`).

**Where:** `breadstudio/omt-hx/`, the OMT-HX submodule/sibling checkout, and the
build (`breadstudio\build.ps1`).

## B8. Take OBS out of the title and the logo

**Owner:** "Change title and logo BreadStudio out of OBS."

The window title and the application icon still read OBS. They must read
BreadStudio. This is S0's unfinished branding work (S0 §3.1, "its Task 3"),
still open.

**Where:** `breadstudio/icons/` (a real icon replacing the placeholder),
`frontend/`, the installer's metadata, `breadstudio/packaging/`.

**Note:** the icon and title touch OBS files, so each goes into
`UPSTREAM-EDITS.md`; `check-edits.ps1` must pass.

---

## Order

B4 is a bug and goes first. B8 and B6 are small and visible. B3 and B5 are desk
changes with real decisions in them. B1 and B2 are the input-model changes and
sit together. B7 is a build task that can run whenever OMT-HX is ready.

Nothing here changes the three plans already written: plan 2 (the Project) and
plan 3 (the vMix import) still stand, and B1–B5 will touch some of the same
files, so whichever is planned next should take them into account.