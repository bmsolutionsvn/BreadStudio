# BreadStudio — GT title rendering: what still differs from vMix

Date: 2026-10-19
For: the planning session, to turn into a plan.
Status: the GT title input plan (Tasks 1–12) is complete and its task-level tests pass. This
report is about the **first real comparison against vMix**, which now runs and measures.

## 1. How to reproduce, exactly

vMix must be **open** with its API enabled (default `http://127.0.0.1:8088/api/`). Its output
must be 1920×1080.

```powershell
# 25 references from vMix (non-destructive: AddInput -> SnapshotInput -> RemoveInput)
powershell -File breadstudio\gt\test\make-references.ps1 -Source F:\Stream -Out <refs>

# compare: per-file mean / bad, exit = number of failed files
$env:PATH="<repo>\.deps\obs-deps-qt6-2026-07-15-x64\bin;$env:PATH"
build_vs2022\breadstudio\gt\test-bin\RelWithDebInfo\bread-gt-render-test.exe F:/Stream <refs> <out>
```

A file fails when `mean > 2.0 || bad > 0.5%` (mean = mean absolute channel difference 0–255;
bad = % of pixels with any channel off by more than 32). `<out>` also gets a `-diff.png` per file.

Facts established about the references (vMix Pro 26.0.0.45):

- A snapshot is **1920×1080, 32bppArgb, transparent corner** — vMix writes alpha, which is what the
  render test's own readback produces, so the two are compared like for like.
- `SnapshotInput` (on the input) and `Snapshot` (on the output) produce the **same** image.
- vMix's on-screen Program/Preview agrees with its snapshot, so the references are faithful.

## 2. State: 25 compared, 12 ok, 13 failed

Fixed and pushed (see the commits named):

- **Clocks were not expanded in the render test** (`0ac4e8ab5`). The test built its frame state from
  field defaults only, so a clock field drew as its raw format string — `Timer.gtzip` rendered the
  literal `{0:hh:mm:ss tt}`. The source expands clocks when it draws; the test now does too, with a
  fixed time (21:05:09), so a render is deterministic and comparable. 23 of 25 renders were
  byte-identical afterwards and exactly the two clock titles changed.
- **The reference script could not reach vMix at all** (`eb20ba624`). Two bugs: the AddInput value was
  not percent-encoded, and a new input was picked out by its *number* — but vMix renumbers (it inserts
  at 1 and shifts the rest), so `RemoveInput` deleted the wrong input. It now percent-encodes values
  and keys off the unique `key`.
- **Pictures were drawn at their own pixel size** (`a17575ae6`). The object's rectangle **is** the box;
  vMix scales the picture into it. `Centered` and `TopRight` previously only *aligned* the native size.

Measured effect of the sizing fix (25 vs the same references):

| title | before | after |
|---|---|---|
| MAN OF THE MATCH | 92.382 / bad 63.807% | **3.435 / 8.223%** |
| PRE INGAME | 26.173 / 22.501% | **1.925 / 3.047%** |
| TAB INGAME | 8.335 / 5.227% | **0.172 / 0.288% — passes** |
| NEXT MATCH | 2.487 / 3.265% | **0.022 / 0.031% — passes** |
| LINEUP 5 | 8.666 / 13.638% | 4.442 / 6.855% |
| TEAM vs TEAM | 7.204 / 11.230% | 4.437 / 7.735% |

The ten text-only titles did not move (mean ≤ 0.3), so that change is not a regression.

## 3. Disproved — do not repeat this

**`DataFlags="Hidden"` does NOT mean "do not draw".** Skipping those objects took the failures from
13 to **23** and broke titles that had been near-perfect (Code, GIFTCODE IN, WIN RATE, Cam2, Caster,
Ingame, TagWin). It is about the *field/data* sense only. The change was reverted and re-measured;
nothing of it is committed.

## 4. The 13 failures, in three groups

### 4a. Not renderer bugs — clock and countdown fields (3 files)

| title | mean | bad |
|---|---|---|
| CACAS24/GTs/MHC/Timer | 7.606 | 4.946% |
| CACAS24/GTs/MHC/Timer2 | 3.377 | 2.440% |
| FVPL…/COUNT DOWN MHC/COUNTDOWN | 7.074 | 5.695% |

vMix's snapshot shows **its own live time**; the test draws a fixed 21:05:09. These can never match by
pixel, and the comparison method — not the renderer — is wrong for them.

**Do:** pin them with a unit test (the expansion is already unit-tested in `GtClock`) and exclude them
from the pixel comparison, or make the comparison clock-aware. Do not "fix" the renderer for these.

### 4b. Small differences that fail only the `bad > 0.5%` bound (4 files)

| title | mean | bad |
|---|---|---|
| CAM LINE UP/CAM LINEUP | 0.328 | 0.525% |
| EAFC24/GT/MHC | 0.365 | 0.656% |
| EAFC24/GT/Matchup | 0.447 | 0.841% |
| CAM PEN/CAM PEN | 0.785 | 0.891% |

The mean is well inside the bound; a small number of localised pixels differ by more than 32. This is
where to look first — a `-diff.png` shows exactly which element.

### 4c. The CruyffSansVN titles (the real remaining bug)

| title | mean | bad |
|---|---|---|
| TAG INGAME | 0.639 | 0.738% |
| PRE INGAME | 1.925 | 3.047% |
| WINNER | 2.575 | 6.592% |
| MAN OF THE MATCH | 3.435 | 8.223% |
| LINEUP 3 / LINEUP 5 | 3.806 / 4.442 | 4.669% / 6.855% |
| TEAM vs TEAM | 4.437 | 7.735% |

## 5. The evidence for 4c, and the leading hypothesis

The `-diff.png` for MAN OF THE MATCH is almost entirely **glyph and silhouette outlines** — the same
shapes in the same places, differing at their edges — plus solid blocks over the player name. That is
the signature of **different font metrics**, not of a misplaced object.

The titles confirm it. Fonts used, against result:

| title | fonts | result |
|---|---|---|
| CODE | (none declared) | 0.003 — ok |
| WIN RATE | SCHABO | 0.002 — ok |
| TagCamIG | — | 0.004 — ok |
| TagWin | Helvetica Neue | 0.015 — ok |
| Caster / Cam2 / Ingame | Helvetica Neue | 0.18 / 0.08 / 0.09 — ok |
| CASTER OFF LINE / ONL | Helvetica Neue | 0.12 / 0.28 — ok |
| Matchup | Helvetica Neue | 0.447 — fails on `bad` |
| **TEAM vs TEAM, WINNER, MAN OF THE MATCH, LINEUP 3/5, PRE INGAME, TAG INGAME** | **CruyffSansVN / CruyffSansVNCondensed** | 0.6 – 4.4 — fail |

**Every badly-failing title asks for `CruyffSansVN` or `CruyffSansVNCondensed`. Nothing else does.**

And those families **do not exist on this PC**. What is installed:

- GDI+ families: `Cruyff Sans`, `Cruyff Sans Condensed`, `Cruyff Sans Condensed Medium`,
  `Cruyff Sans Expanded Heavy`, `Cruyff Sans Light`, `Cruyff Sans Medium`, `Cruyff Sans Mono`, …
- The file `CruyffSans-Regular.ttf` declares, in its name table: family **`Cruyff Sans`**,
  full name `Cruyff Sans Regular`, PostScript name **`CruyffSans`**.
- **No name table entry anywhere says `CruyffSansVN`** (checked the installed files and the vMix
  install folder, which carries **no** fonts of its own — `C:\Program Files (x86)\vMix` has none).

So **both vMix and BreadStudio fall back for these titles, and their fallbacks differ.** vMix's
fallback is what the references show; ours is what our render shows. `Helvetica Neue` and `SCHABO`
resolve the same in both, which is why those titles pass.

The "VN" is almost certainly the author's intent to mean the Vietnamese-capable Cruyff Sans, and the
designer's machine had it under a name this PC does not use.

### What to do about it

1. **Find out what vMix resolves `CruyffSansVN` to**, before changing anything. A decisive probe:
   build a one-word `.gtzip` whose only field asks for `CruyffSansVN`, snapshot it from vMix, and then
   compare that reference against our own renders of the same word in each candidate
   (`Cruyff Sans`, `Cruyff Sans Condensed`, `Segoe UI`, `Arial`). Whichever is near-zero tells us the
   mapping. The same probe for `CruyffSansVNCondensed`.
2. **Make the renderer's family lookup match vMix's.** Note this is a *fallback* behaviour, so it is
   not just "look harder for the exact name": it is "when the exact name is absent, fall back the way
   vMix does". Our code should carry the finding from step 1 in one place, with the evidence in a
   comment.
3. Only if step 1 shows vMix *did* find the right face: then the fault is on our side (wrong font
   collection or lookup) and the fix is in font resolution, not a name table.
4. A likely-shaped answer, to be confirmed not assumed: map `CruyffSansVN` → `Cruyff Sans` and
   `CruyffSansVNCondensed` → `Cruyff Sans Condensed`. If step 1 confirms it, implement it as an
   explicit, commented alias table rather than a fuzzy match.

Do the same for 4b after 4c — the diff maps there are small and localised, and may well be the same
cause (Matchup uses `Helvetica Neue`, which resolves, so 4b is probably something else: opacity,
masking, or a shadow).

## 6. Not yet done, and worth knowing

- **The comparison has never been an acceptance gate.** Nothing here has been signed off; the numbers
  above are the first measurement. `DESK-SMOKE.md`'s "L. GT titles" section is still marked
  "Not yet run — the owner runs this section".
- **The render test still builds its frame from defaults.** It expands clocks now, but it does not
  apply saved `values`, `styles` or `countdowns` from a preset, because those live in the `.vmix`
  preset, not the `.gtzip`. Comparing a title *as the owner's show uses it* would need the preset's
  values fed in — a real gap if the goal is "matches what goes on air".
- **`make-references.ps1` mutates the owner's vMix session** for the length of the run (it adds and
  removes 25 inputs). It leaves the input list as it found it, but it should not be run while the
  owner is working live.