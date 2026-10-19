# BreadStudio — GT render fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the GT render test measure the right thing, then fix the two renderer bugs it then shows, so the 25 owner titles match vMix except the three clock titles.

**Why this plan exists:** `breadstudio/docs/2026-10-19-breadstudio-gt-render-gaps.md` blamed the 13 failures mostly on fonts (§4c, §5). A check on 2026-10-20 against that report's own run (`%TEMP%\opencode\gtr-ref` vs `%TEMP%\opencode\gtr6`, which reproduce its numbers exactly) found otherwise. The findings below are binding for this plan.

**Spec:** `breadstudio/docs/2026-10-19-breadstudio-gt-title-design.md` (binding). The parent plan is `breadstudio/docs/2026-10-19-breadstudio-gt-title.md` (Task 6 is the renderer and render test).

## Findings (2026-10-20)

1. **The comparison double-applies alpha.** vMix's snapshot PNG holds *premultiplied* colour in a file labelled straight. Evidence, MAN OF THE MATCH pixel (960,1060), alpha 227: source picture `238,123,10`; our render `238,124,10`; vMix `212,110,9` = 238×227/255, 123×227/255. `compareImages` (`GtRenderTest.cpp`) does `r[c] * a / 255` on the reference, so every semi-transparent pixel of the reference is darkened twice. That is the "outlines at every edge" signature in the old `-diff.png` files, and it shows even on a 1920×1080 picture drawn 1:1 with no text.
   Re-scoring the same 25 renders with the reference read as premultiplied: **19 ok, 6 fail** (was 12/13). Still failing: Timer, Timer2, COUNTDOWN (clocks), LINEUP 3 (2.452 / 1.041%), LINEUP 5 (2.369 / 1.050%), TEAM vs TEAM (0.986 / 0.798%).
2. **Fonts are not the cause. Do not add a font alias.** `CruyffSansVN` / `CruyffSansVNCondensed` exist nowhere on this PC; vMix and BreadStudio both fall back to the same face (Segoe UI) with identical glyphs and wrapping. Tested: rewriting the 13 titles that use CruyffSansVN to `Cruyff Sans` / `Cruyff Sans Condensed` made **every** one of them worse against vMix (e.g. MAN OF THE MATCH 0.785 → 2.860, WINNER 0.063 → 1.067, LINEUP 5 2.369 → 10.106, corrected metric). `SCHABO` is installed (`SCHABO-Condensed.otf`, family `SCHABO`) and resolves in DirectWrite/WPF. `CruyffSans` (no space) is only a PostScript name and resolves nowhere.
3. **A shadow is translated twice.** `recordShadows` records the object's drawing with `SetTransform(matrix)` already applied (`GtRenderer.cpp`, `recordShadows`), and `drawShadow` then draws that command list with `SetTransform(Translation(offset) * matrix)`. The shadow lands one extra object-location away. TEAM vs TEAM `ROUND` ("3 : 1", Location 754,255, `<Effect Type="Shadow" Offset="5,5">`): the vMix reference has a black offset shadow under the white text; ours has none there and a blurred black "3" near x≈1600, y≈410–560.
4. **Text is not clipped to its box.** When a TextBlock's text overflows its `Dimensions` (LINEUP 5 `MATCH`: "MATCH 1", font 150 in 435×174, wraps to two lines; TEAM vs TEAM `ROUND`: font 230 in 410×227), vMix cuts the text at the box's top and bottom edges; ours paints the overflow over neighbouring objects.

## Global Constraints

- Change only `breadstudio/gt/` files and the two docs named in Task 5.
- **No font alias, no fuzzy family match** (Finding 2). DirectWrite's own fallback stays.
- Never re-run `make-references.ps1`: it adds and removes inputs in the owner's live vMix session. Use the existing references in `C:\Users\bachtam2001\AppData\Local\Temp\opencode\gtr-ref` (25 PNGs).
- The owner's titles (`F:\Stream`), references and render outputs are never committed.
- Repo rules (`BreadStudio/CLAUDE.md`): run GitNexus `impact` before editing an existing symbol (`compareImages`, `drawShadow`, `recordShadows`, `drawText`/`gtDrawText`); `detect_changes({scope: "all"})` before every commit. Commit subjects start `breadstudio: `; every message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Build: `cmake --build build_vs2022 --config RelWithDebInfo --target <target>`. Tests: `bread-gt-package-test`, `bread-gt-document-test`, `bread-gt-fields-test`, `bread-gt-clock-test`, `bread-gt-render-test`, in `build_vs2022/breadstudio/gt/test-bin/RelWithDebInfo/`. Before running: `$env:PATH="<repo>\.deps\obs-deps-qt6-2026-07-15-x64\bin;$env:PATH"`.
- The local comparison (call it **the 25-run**):
  ```powershell
  build_vs2022\breadstudio\gt\test-bin\RelWithDebInfo\bread-gt-render-test.exe F:/Stream C:/Users/bachtam2001/AppData/Local/Temp/opencode/gtr-ref <out>
  ```
  Use a fresh `<out>` folder under `%TEMP%\opencode\` per run (`gtr7`, `gtr8`, …). Record each run's per-file `mean`/`bad` in the ledger.

## Review Focus

1. **The premultiplied reading must not hide real alpha bugs.** Our side is already premultiplied over black; after Task 1 both sides are compared as premultiplied RGB. A title whose *alpha* differs (something drawn semi-transparent that vMix draws opaque) must still fail. Check the diff of a transparent-corner region stays 0 and that the Task 1 unit check covers both an equal and an unequal pixel.
2. **The shadow fix must hold inside a Layer and under Skew**, not only for a top-level object at a plain translation. Task 2's transform must be derived from the object's full matrix, not from its Location alone.
3. **Clipping must not cut glyphs that fit.** Ascenders/descenders and the stroke of text that fits its box must be unchanged: the ten text-only titles that pass today must not move (mean change ≤ 0.05).

---

### Task 1: Read vMix references as premultiplied

**Files:** Modify `breadstudio/gt/test/GtRenderTest.cpp` (`compareImages`, its comment), `breadstudio/gt/test/make-references.ps1` (header note only).

- [ ] **Step 1: Failing check.** Add a small pure check, run from `main` before `selfCheck`, that builds a 1×1 `ours` pixel `{9,110,212,227}` (BGRA, premultiplied, as our render is) and a 1×1 reference pixel with the same bytes (as vMix writes it), calls `compareImages(..., refHasAlpha=true, ...)`, and REQUIREs `mean == 0`. Add a second pair that differs by 40 in one channel and REQUIRE `bad == 100`. Build, run with no arguments, watch the first REQUIRE fail.
- [ ] **Step 2: Fix.** In `compareImages`, use the reference channel as-is when it has alpha (`refChannel = r[c]`); keep `a == 255` handling for references without alpha unchanged (equivalent). Same in the diff-image loop. Rewrite the comment above `compareImages`: vMix's snapshot stores premultiplied colour (cite the 960,1060 pixel from Finding 1), so both sides are premultiplied-over-black already.
- [ ] **Step 3: Header note.** In `make-references.ps1`'s header, where it says the snapshot carries alpha, add that its colour is premultiplied.
- [ ] **Step 4: Verify.** Unit run passes. The 25-run: expected **19 ok, 6 fail** — the six in Finding 1 with numbers matching it to ±0.01.
- [ ] **Step 5: Commit:** `breadstudio: read vMix GT references as premultiplied in the render test`.

### Task 2: Draw a shadow once, at its object

**Files:** Modify `breadstudio/gt/GtRenderer.cpp` (`drawShadow`), `breadstudio/gt/test/GtRenderTest.cpp` (new check).

- [ ] **Step 1: Failing check.** Add `shadowCheck(device, context)` beside `selfCheck`: a 320×120 transparent document with one Rectangle at Location (80,56), 40×20, opaque white fill, effect `Shadow` with offset (10,10), blur 0, colour `0xFF000000`. Read back. REQUIRE alpha > 128 at (125,80) (inside the shadow, outside the object). Then, in a second document drawn on its own, put the same object inside a Layer at Location (20,0) and REQUIRE the shadow at (145,80) (Review Focus 2). Two documents, so one shadow cannot satisfy the other's check. Watch it fail (today the shadow lands at ≈(170,122), off the target).
- [ ] **Step 2: Fix.** The command list already carries `matrix` (world space). In `drawShadow`, the extra world transform that moves it by the offset in the object's own space is `inverse(matrix) * Translation(offset) * matrix` (D2D row-vector order: apply left first). Use `D2D1::Matrix3x2F` and `Invert()`; if `matrix` is not invertible, fall back to `Translation(offset)`. Restore the transform to `matrix` afterwards as today. Comment the reason in one line.
- [ ] **Step 3: Verify.** Unit run passes. The 25-run: TEAM vs TEAM improves; the black "3" near (1600,480) is gone from its `-diff.png`; no other title's mean rises by more than 0.05. Record numbers.
- [ ] **Step 4: Commit:** `breadstudio: draw a GT shadow once, under its object`.

### Task 3: Clip text to its box

**Files:** Modify `breadstudio/gt/GtRenderer.cpp` (`drawText`) or `breadstudio/gt/GtText.cpp` (`gtDrawText`) — choose one, one place only; `breadstudio/gt/test/GtRenderTest.cpp` (new check).

- [ ] **Step 1: Failing check.** In `shadowCheck` or a new `clipCheck`: a TextBlock at (10,10), 200×30, Arial 28, white, text long enough to wrap to three lines. REQUIRE every pixel with y ≥ 41 (below the box) has alpha 0, and that some pixel inside the box is white. Watch it fail.
- [ ] **Step 2: Fix.** Around the layout draw, `PushAxisAlignedClip(D2D1::RectF(0, 0, width, height), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE)` in the object's space, `PopAxisAlignedClip()` after. Note in a comment that under Skew an axis-aligned clip uses the transformed box's bounds (`ponytail:` comment naming that ceiling; a geometry layer is the upgrade if a skewed overflowing title appears).
- [ ] **Step 3: Verify.** Unit run passes. The 25-run: LINEUP 3, LINEUP 5 and TEAM vs TEAM improve. The ten text-only titles that passed before move by ≤ 0.05 (Review Focus 3). If any of these gets worse, or the clip visibly cuts glyphs that vMix shows, revert this task and write a `Ruling:` with the numbers and diff crops — do not tune it blind.
- [ ] **Step 4: Commit:** `breadstudio: clip GT text to its box, as vMix does`.

### Task 4: Name the clock titles as known differences

**Files:** Modify `breadstudio/gt/test/GtRenderTest.cpp`.

- [ ] **Step 1:** A `known` list of relative paths with a reason: `CACAS24/GTs/MHC/Timer`, `CACAS24/GTs/MHC/Timer2`, `FVPL SUMMER 2024/COUNT DOWN MHC/COUNTDOWN` — "vMix's snapshot shows its live clock; the test draws a fixed 21:05:09". A known file still prints its `mean`/`bad`, then `known: <reason>` instead of `FAIL`, and does not count toward the exit code. The clock expansion itself stays covered by `bread-gt-clock-test`.
- [ ] **Step 2: Verify.** The 25-run's exit code equals the number of non-known failures. Record it.
- [ ] **Step 3: Commit:** `breadstudio: name the GT clock titles as known render differences`.

### Task 5: Correct the report

**Files:** Modify `breadstudio/docs/2026-10-19-breadstudio-gt-render-gaps.md`; add this plan, `breadstudio/docs/2026-10-20-breadstudio-gt-render-fixes.md`.

- [ ] **Step 1:** At the top of the gaps report, under the status, add a dated "2026-10-20 correction" section: Findings 1–4 in short, a pointer to this plan, and the final 25-run table (title, mean, bad, ok/known/FAIL). Mark §4b, §4c and §5 as superseded (keep the text, prefix each heading with "Superseded:"). Add to §3 "Disproved": the font-fallback hypothesis, with the Cruyff Sans swap numbers from Finding 2. Fix the "TAG INGAME" row (no such title; TAB INGAME passes). Fix "SCHABO resolves the same in both": it is installed and resolves.
- [ ] **Step 2: Commit:** `breadstudio: correct the GT render-gaps report`.

---

## Final report

Commits; unit test results; the 25-run before (Task 1) and after (Task 4) as tables; every `Ruling:` line; anything still failing with its `-diff.png` path and one line on what the diff shows.
