# BreadStudio — owner's backlog — 2026-10-16

Captured from the owner in conversation, with the screenshots they supplied.
Each row is a change to make. "Where" names the unit the change lands in; each
group becomes its own plan when it is worked.

Status: **not started.** Plan 1 (hiding OBS's surface) is done and shipped.

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