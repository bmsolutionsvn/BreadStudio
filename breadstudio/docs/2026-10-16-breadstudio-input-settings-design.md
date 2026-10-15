# BreadStudio — Input Settings window (vMix-shaped) — Design

Date: 2026-10-16
Status: draft for the owner's review.
Builds on: `2026-10-13-breadstudio-s1-desk-design.md` (the desk). Replaces the
plain gear window (`breadstudio/desk/InputSettings.*`) with a vMix-shaped one.
Base: OBS Studio 32.2.2.

## 1. Purpose

The owner wants the window behind a tile's ⚙ to look and behave like vMix's
**Input Settings**: a left list of pages, the chosen page on the right, and a
`Copy From…` and `OK`/`Cancel` row along the bottom. Today the gear opens a
plain dialog with the base's OBS properties and a layers list.

This is a shell change plus a set of pages. Many vMix pages have no OBS or desk
equivalent; this spec says which pages exist and what backs each, and leaves the
rest out (as the desk leaves out an input kind whose plugin is absent).

## 2. Decisions

| # | Topic | Decision |
|---|---|---|
| I1 | The shape | vMix's: title `Input: N <name>`, a left page list, one page on the right, and a bottom row. The window is **not** modal (drags into Preview must still work), as today. |
| I2 | Which pages | Only the pages something can back. A page with nothing behind it is **not shown** (the desk shows no tab for an absent plugin). |
| I3 | OK / Cancel | vMix applies as you go; OBS's pages apply on change. So the window has a single **Close** button, not OK/Cancel, except the pages that already open OBS's modal panels. |
| I4 | Categories | A tile's own colour tag, kept in the input's private settings, shown as the swatch row and used to tint the tile. New. |
| I5 | MultiView / Layers | One page, "Layers / MultiView": the desk's existing layers list, and a small live view of the input as the MultiView draws it. |
| I6 | `Copy From…` | Copies another input's desk settings (AFV, category, mouse-click action, the automatic behaviours) — not its source. |

Rejected: **OBS's own properties window embedded as the whole page** (the desk
already opens it from "Base properties…"; making it the shell would not be
vMix-shaped).

## 3. The window

```
┌ Input: 3  Cam A ──────────────────────────────────────────────┐
│ General            │  Name      [ Cam A                     ] │
│ Position           │  Aspect Ratio  [ Source            ▾ ]    │
│ Colour Adjust      │  Category  ● ● ● ● ● ● ● ● ●              │
│ Colour Key         │  ────────────────────────────────────    │
│ Colour Correction  │  ☑ Automatically mix audio (AFV)          │
│ Effects            │  ☑ Play with transition                  │
│ Layers / MultiView │  ☐ Restart with transition               │
│ Triggers           │  ☐ Pause after transition                │
│ Advanced           │  Mouse Click Action [ Preview       ▾ ]  │
│                    │  Resolution   1920×1080 (read only)      │
│                    │  Frame Rate   50 (read only)             │
│                    │  [ Audio Settings… ]                     │
├────────────────────┴───────────────────────────────────────────┤
│ [ Copy From… ]                                       [ Close ] │
└────────────────────────────────────────────────────────────────┘
```

### 3.1 The pages

| Page | Backed by | Contains |
|---|---|---|
| **General** | the input's settings | Name (rename), Aspect Ratio and Deinterlacing (the base's OBS settings), Category (colours), the automatic behaviours, Mouse Click Action, a read-only Resolution and Frame Rate, and `Audio Settings…` (OBS's Advanced Audio for the base) |
| **Position** | the base's scene item in its own input | X/Y/scale/rotation/crop, the same controls OBS's transform has |
| **Colour Adjust** | an OBS `color_filter_v2` filter on the base | Gamma, contrast, brightness, saturation, hue, opacity |
| **Colour Key** | an OBS `chroma_key_filter_v2` filter | the key colour and its thresholds |
| **Colour Correction** | an OBS `color_correction_filter`/LUT filter | the correction and the LUT |
| **Effects** | OBS's effect filters | the list and each one's settings |
| **Layers / MultiView** | the desk | the layers list (hidden, shown, up/down, remove), and a small view of the input as the MultiView draws it |
| **Triggers** | the desk | the behaviours vMix puts here, but the subset the desk can back: on the input becoming Program/Preview, or on click — start/stop a recording or a stream, or move to Preview/Program |
| **Advanced** | the base's OBS settings | the base's Advanced properties (Sync Offset, hotkeys), and the desk's own hidden values |

Pages vMix has with **no backing** and therefore not shown: Colour Correction's
LUT beyond OBS's, PTZ, Tally Lights, Virtual Input. (A note in the Advanced page
says what is absent, so the operator knows it was not an oversight.)

### 3.2 What each control changes

- **Name** — `renameInput()` (the desk's rename, which refuses a taken name).
- **Aspect Ratio / Deinterlacing** — the base's own settings through
  `obs_source_update`; anything the source type does not have is not shown.
- **Category** — the input's private settings key `bread_category`; the tile
  in the input bar and the MultiView slot take the tint.
- **Automatically mix audio** — the desk's AFV (`AudioBed::afv`/`setAfv`).
- **Play / Restart with transition, Pause after transition** — where OBS has the
  matching per-source flag (the base's "restart on activate" and OBS's show/hide
  transition), else not shown.
- **Mouse Click Action** — none, Preview, or Program: what a click on the tile
  does (today always Preview). The input bar reads the private setting.
- **Audio Settings…** — opens OBS's Advanced Audio for the base (or the mixer
  strip), so nothing is duplicated.
- **Copy From…** — a list of the other inputs; choosing one copies the private
  settings §2/I6 names.

## 4. What is saved

| Saved | Where |
|---|---|
| Category, mouse-click action, the automatic behaviours | The input's private settings |
| Name | OBS, as today |
| Aspect Ratio, Deinterlacing, filters, transform | OBS, as today |

## 5. Failures

| Situation | What happens |
|---|---|
| The base's type has no aspect-ratio or deinterlacing setting | The control is not shown |
| A page's filter plugin is absent | That page is not shown, with a line saying why |
| The input is deleted while the window is open | The window closes (as today) |
| A layer drag is in progress when the window is closed | Nothing; the drag is OBS's |

## 6. Testing

By hand, added to `DESK-SMOKE.md`:

- The gear opens the vMix-shaped window on the input's General page; the page
  list has exactly the pages §3.1 lists for that input's type.
- Rename, Category, AFV, Mouse Click Action each take effect on the tile and the
  mixer and survive a reload.
- Each filter page adds, configures and removes its filter on the base.
- `Copy From…` copies the desk settings from another input.
- A source type with no aspect-ratio setting shows no such control.

No automated test: the window is Qt over OBS, as the desk's other windows are.

## 7. Out of scope

- A top menu bar replacing OBS's (the owner and this spec both want one; it is
  its own piece of work, since hiding OBS's menus leaves Settings unreachable).
- PTZ, Tally Lights, Virtual Input, vMix's LUT beyond OBS's — no backing.
- Rebuilding OBS's own properties panels.

## 8. Risks

| Risk | Handling |
|---|---|
| Hiding OBS's menus removed reach to OBS's Settings | Build the replacement top bar (or re-show the menu) before this ships; the spec for it is separate. |
| vMix's page list is long; drift from the owner's wish | The page set is §3.1; the owner reviews it here before the plan. |
| The tile tint and MultiView slot draw with the category colour | The tint is one more input of the tile and slot draw, not a new mechanism. |