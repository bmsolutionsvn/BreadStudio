# BreadStudio — MultiView against vMix — Review

Date: 2026-10-16
Scope: the desk's MultiView (`breadstudio/desk/MultiviewSource.cpp`,
`MultiviewLayout.{hpp,cpp}`) measured against vMix's, per output. This is the
review the owner asked for in backlog item B5; the changes that follow from it
are at the end.

## 1. What vMix does

Read from the installed vMix 26 (`C:\Program Files (x86)\vMix`) and its
language file, the API command table inside `vMix64.exe`, the owner's presets in
`F:\Stream`, and vMix's own documentation of Fullscreen and MultiView.

- **Each output chooses what it shows.** A Fullscreen, Fullscreen2 or External
  output is set to **Output, Preview, MultiView, or any input** — the API's
  command is literally `Output, Preview, MultiView, Input` ("Change what is
  displayed on the Fullscreen output"). The MultiView is therefore one of the
  things an output *can* be, not a separate kind of output.
- **The MultiView has a layout, and every window can be re-pointed.** Settings →
  Outputs/NDI/SRT holds **MultiView Layout** (a general layout) and a
  **Customize Layout** step that picks what appears in each window. The preset
  carries it: `MultiViewLayout` is one number, and `MultiViewSettings` holds
  `InputMapping0`…`InputMapping13` — fourteen windows, each a source — plus
  `TitleLocation` and `TitleHeadings`. In the owner's shows the mapping is the
  identity: window *i* shows input *i*, titles off (`TitleLocation = 0`).
- **The default layout is what BreadStudio already draws**: vMix's own
  documentation describes "the preview window, the output window, and eight
  commonly used inputs" — Preview and Program, then eight inputs. Other
  layouts go up to sixteen inputs.
- **Windows are letterboxed, not stretched.** A vMix MultiView window shows its
  input whole, inside a frame, whatever the window's shape.
- **Titles.** `TitleLocation` places the window's name (off, top or bottom) and
  `TitleHeadings` switches a heading row on. The owner's shows have both off.
- **An input has its own "MultiView" too**, which is a different thing: the
  input's layers. Input Settings has MultiView `Add/Remove/Import/Export`, and
  the API calls it "change Layer in Input MultiView". BreadStudio's
  Layers / MultiView page already mirrors this (input-settings spec I5), so it
  is not part of this review.

## 2. What BreadStudio does

- One source, `bread_multiview`, drawn by `MultiviewSource.cpp`, with the ten
  boxes computed by `multiviewSlots(width, height)`.
- Layout: Preview (top left), Program (top right), eight inputs along the
  bottom — vMix's default, and it tiles the canvas exactly.
- It is offered as one of the things an output can show: the outputs engine's
  `sourceFor()` returns `bread_multiview` for an output whose source is
  MultiView.
- Each box is framed in a category tint or the tally colour, with a name label
  at its bottom left.

## 3. The differences that matter

| # | What | vMix | BreadStudio | Verdict |
|---|---|---|---|---|
| M1 | **Aspect inside a window** | letterboxed — the picture is whole | **stretched** — `drawPicture` scales x by `w/pw` and y by `h/ph`, so a picture is squashed to whatever shape the box is | **A defect.** On a 1920×1080 canvas the bottom eight boxes are 240×540 (4:9); a 16:9 camera drawn into one is squeezed to about a quarter of its width. The two top boxes happen to be 16:9 on a 16:9 canvas, which is why Preview and Program look right and the inputs do not. |
| M2 | **Choosing the layout** | a layout list, and a window-to-source mapping stored in the preset | fixed: Preview, Program, then the first eight inputs in bar order | A gap, not a fault. Worth a later stage; the fixed layout is vMix's default. |
| M3 | **Pointing a window at something else** | any window, any source | none — the mapping is implied by input order | A gap. |
| M4 | **Window titles** | `TitleLocation` (off/top/bottom) and `TitleHeadings` | always a name label at the bottom left of each box | Close enough; the owner's own shows have vMix's titles off. |
| M5 | **More than eight inputs** | up to sixteen in a layout | the bottom row shows the first eight; the rest are not drawn | The S1 design already accepts this ("OBS's multiview shows the first 25"; ours shows eight). A gap. |
| M6 | **The canvas the MultiView is composed at** | generated for the output that shows it | composed at the **main mix** size (`multiviewWidth`/`Height` return the base canvas), then scaled by whatever output shows it | **A defect in the same family as M1.** An output whose canvas is not the main mix's shape scales the whole composition, so every box changes shape and the labels change size. With M1 fixed the *pictures* stay true to their own aspect only if the composition is not also distorted — an output 16:9 showing a 16:9 composition is fine, an output 4:3 is not. |
| M7 | **The shape of the input boxes** | a layout list, so a wider grid than eight-across is available | eight across a 16:9 canvas, so each bottom box is **240×540 (4:9)**; a 16:9 picture contained in one is 240×135, a quarter of the box, and no 16:9 input can ever fill a 4:9 box | **A defect.** The bottom half in **two rows of four** makes each box **480×270 — exactly 16:9**, which a 16:9 input fills exactly: **four times the picture**, no dead letterbox. Fixed here. |

## 4. What this changes

**Now (M1, and M6 with it):** draw each picture **letterboxed** inside its box —
fit it by the smaller of the two scale factors and centre it — instead of
stretching it. This is a pure geometry function, so it is testable without OBS:

```cpp
/* the rectangle a `pw` by `ph` picture fills inside `box`, keeping its aspect and centred */
SlotRect fitInside(const SlotRect &box, int pw, int ph);
```

It goes in `MultiviewLayout.{hpp,cpp}` beside `multiviewSlots`, is covered by the
existing `bread-multiview-test`, and `drawPicture` uses it.

**Also now (M7):** the bottom half is **two rows of four**, not one row of
eight. On a 1920×1080 canvas an eight-across row makes each box **240×540
(4:9)**; a 16:9 picture contained in one occupies 240×135, a quarter of the box
— and since no 16:9 picture can fill a 4:9 box, the row can never show a 16:9
input usefully. Two by four in the same half makes each box **480×270, exactly
16:9**, which a 16:9 input fills exactly: **four times the picture area**, with
no dead letterbox. It is the same `multiviewSlots`, still tiling the frame
exactly, and the eight boxes read left to right, top row first.

**Later (M2, M3, M5):** a layout choice and a per-window mapping, stored with the
outputs as vMix stores it. That is a stage of its own — it is the same shape of
work as vMix's `MultiViewLayout` + `InputMapping`, and it belongs with the
outputs engine, not in this fix. M4 follows it.

## 5. Outputs, one by one

What a MultiView output shows, as the desk stands and after M1:

| Output | Canvas | Before M1 | After M1 |
|---|---|---|---|
| A window or monitor (OBS's projector) | the main mix | correct | correct |
| An output at the main mix's size and shape | the main mix | top boxes correct, bottom row squashed | all boxes correct |
| An output of a different shape (a 4:3 canvas) | its own | everything squashed twice over | boxes keep the composition's shape, which is still the main mix's — M6 |
| NDI, SRT | the output's canvas | as above | as above |
