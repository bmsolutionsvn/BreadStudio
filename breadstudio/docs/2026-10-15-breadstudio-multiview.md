# BreadStudio — MultiView source Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An output set to MultiView carries a picture of the desk's own monitoring view — Preview and Program across the top, the desk's inputs along the bottom, each with its number, name and a red or green frame by the input bar's rule — at the size of the output that shows it.

**Architecture:** A new source of the desk plugin, `bread_multiview`, drawn with the graphics API (no Qt in the render), laid out by a plain function `multiviewSlots(width, height)` so the box of each slot is testable without OBS. What it draws — the current Program and Preview scenes, the desk's inputs, and the studio-mode flag — is gathered on the UI thread by `refreshMultiview()` (which the engine calls from `refreshSources()`), so the render reads only a cache and calls nothing of the frontend. One instance is held by the engine, and its `sourceFor()` returns it for an output set to MultiView.

**Tech Stack:** C++17, libobs and obs-frontend-api from OBS Studio 32.2.2, the graphics API (`gs_*`), a private `text_gdiplus` source per slot label (Windows, as OBS's own multiview uses), CMake (OBS's `windows-x64` preset), Visual Studio 2026, Windows PowerShell 5.1.

**Spec:** `breadstudio/docs/2026-10-15-breadstudio-outputs-design.md` §5 (and §3.1's Source row, §9's MultiView-layout test). Read it before any task. Section numbers below (§) refer to it.

**Needs first:** Plan D merged (`2026-10-15-breadstudio-outputs-engine.md`). Task 2 fills in Plan D's `OutputEngine::sourceFor()` MultiView case (which today returns an empty `OBSSource()` with the comment "Plan E fills this in"). Read `OutputEngine.hpp/.cpp`, `ProgramSource.hpp/.cpp`, `Desk.hpp`, `InputModel.hpp` and `frontend/components/Multiview.cpp` as they are on `main` before those tasks; where they differ from this plan's text, keep their differences.

## What reading the code changed

Checked on 2026-10-15 against OBS 32.2.2 on `main` at `a0b5d77c`, and OBS's own `frontend/components/Multiview.cpp`:

| Found | Consequence |
|---|---|
| OBS's frontend multiview draws each box with a solid-effect background, `gs_matrix_translate3f`/`scale3f`, a per-box viewport+ortho (`startRegion`/`endRegion`), and `obs_source_video_render(src)` inside it; labels are private `text_gdiplus` sources (`h/9.81` font), drawn the same way. | `bread_multiview` mirrors that idiom: one solid box per slot for the frame, one for the black inside, `obs_source_video_render` for the picture, and a private `text_gdiplus` source per label. |
| The frontend builds its scene list and labels in `Update()`, on the UI thread, and only draws in `Render()`; nothing of the frontend API is called from `Render()` except the already-gathered sources. | `bread_multiview` gathers Program, Preview, the inputs and the studio-mode flag in `refreshMultiview()`, called on the UI thread by the engine's `refreshSources()`; the render reads only that cache. |
| The desk already calls `OutputEngine::refreshSources()` on every input, Preview, Program and Studio-Mode change (Plan D). | Those events already reach the multiview; no new desk wiring is needed beyond calling `refreshMultiview()` from `refreshSources()`. |
| `Desk.hpp` already holds the input bar's colours (`ProgramColor 0xFFD00000`, `PreviewColor 0xFF00D000`, `IdleColor 0xFF303030`), and `InputBar::updateTally()` tints a tile by: the input equals OBS's current scene → Program, equals the current Preview scene (Studio Mode on) → Preview, else Idle. | The multiview's input slots use the same three colours and the same three-way rule, so its frames match the bar's. |
| The desk's inputs are the scenes, in OBS's scene order (`inputs()` in `InputModel.hpp`, via `obs_frontend_get_scenes`), and each is base size. | The multiview's bottom slots are the first eight of `inputs()`, gathered once per `refreshMultiview()`. |
| `bread_program` renders the main mix by `obs_render_main_texture()`; `OutputEngine` creates one such source and returns it from `sourceFor()` for Program. | `bread_multiview` is created and held exactly as `bread_program` is, and returned from `sourceFor()` for MultiView. |
| OBS's text source on Windows is `text_gdiplus`; it takes a `font` object (`face`, `flags=1` bold, `size`) and a `text` string, and updates in place with `obs_source_update`. | Each slot label is one `text_gdiplus` source, made or updated when its text changes. |

## Global Constraints

- Base: OBS Studio 32.2.2. New code goes under `breadstudio/desk`. No OBS file is edited by this plan.
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1` passes before every commit.
- Commit messages: `breadstudio: <what changed>`. Never rebase or force-push `main`. Nothing is pushed: every `git push` is an owner gate.
- C++17 (OBS's setting): no designated initialisers; fill OBS's info structs field by field. Warnings are errors (`/W3 /WX /permissive-`): fix a warning with the smallest change, do not switch it off.
- OBS's code style: tabs, 120 columns, braces on every `if`.
- Every string the operator sees is in `breadstudio/desk/data/locale/en-US.ini` (`desk::text` or `obs_module_text`).
- The render runs on OBS's graphics thread: it must use no Qt and must call nothing of `obs_frontend_*`. Everything the frontend supplies is gathered on the UI thread by `refreshMultiview()`, which the engine calls from its own UI-thread `refreshSources()`.
- Windows x64 only. Build from a Visual Studio 2026 developer shell with `cmake --build --preset windows-x64 --config RelWithDebInfo --target <target>`; the program must be closed first. Run it from `build_x64\rundir\RelWithDebInfo\bin\64bit\obs64.exe --portable`; before a run, `build_x64\rundir` holds no `*sync-conflict*` file; after a normal close, the newest log ends with `Number of memory leaks: 0`.

## Review Focus

1. **The layout, for any size.** At 16:9, and at an odd size where the halves and eighths do not divide evenly, the ten slots tile the frame exactly — no gap, no overlap, no slot off the edge. Owned by Task 1's test.
2. **The frames follow the bar.** An input on Program is red, on Preview green, otherwise idle; with Studio Mode off nothing is green. Owned by Task 1's rule being the bar's, and Task 2's hand-check.
3. **Two outputs, one view, no second one.** Output 2 set to MultiView shows the view; a size change restarts only its receivers. Owned by Task 2's hand-check.
4. **The view is live.** Changing Program, Preview, the input list and Studio Mode is reflected within a second. Owned by Task 2's hand-check.
5. **Closing clean.** Opening an output on MultiView and closing leaves no leaked source, canvas or label. Owned by Task 2's hand-check (the log's `Number of memory leaks: 0`).

## File Structure

| Path | Responsibility |
|---|---|
| `breadstudio/desk/MultiviewLayout.hpp`, `.cpp` | The ten slot rectangles, one plain function. No OBS, no Qt. |
| `breadstudio/desk/test/MultiviewLayoutTest.cpp` | Its test, built as `bread-multiview-test` |
| `breadstudio/desk/MultiviewSource.hpp`, `.cpp` | The `bread_multiview` source: what it draws, the gather, the render |
| `breadstudio/desk/OutputEngine.hpp`, `.cpp` | One multiview source; `sourceFor()`'s MultiView case; the gather on refresh |
| `breadstudio/desk/desk.cpp` | Registering the source |
| `breadstudio/desk/CMakeLists.txt`, `data/locale/en-US.ini` | New files, the test, strings |
| `breadstudio/docs/DESK-SMOKE.md` | Section H, one item |

---

### Task 1: The layout, as ten rectangles

**Files:**
- Create: `breadstudio/desk/MultiviewLayout.hpp`, `breadstudio/desk/MultiviewLayout.cpp`, `breadstudio/desk/test/MultiviewLayoutTest.cpp`
- Modify: `breadstudio/desk/CMakeLists.txt`

**Interfaces:**
- Produces, in namespace `desk`:
  - `inline constexpr int MultiviewSlots = 10;` — Preview, Program, then eight inputs.
  - `struct SlotRect { int x; int y; int w; int h; };`
  - `std::array<SlotRect, MultiviewSlots> multiviewSlots(int width, int height);`

- [ ] **Step 1: Write the header**

Create `breadstudio/desk/MultiviewLayout.hpp`:
```cpp
#pragma once

#include <array>

namespace desk {

/* vMix's default layout (outputs spec §5): Preview and Program across the top half, eight input
 * slots along the bottom half. Slot 0 is Preview, slot 1 is Program, slots 2 to 9 are the inputs. */
inline constexpr int MultiviewSlots = 10;

struct SlotRect {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

/* The ten boxes for a frame that is `width` by `height`. They tile it exactly, whatever the size. */
std::array<SlotRect, MultiviewSlots> multiviewSlots(int width, int height);

} // namespace desk
```

- [ ] **Step 2: Write the failing test**

Create `breadstudio/desk/test/MultiviewLayoutTest.cpp`:
```cpp
#include "MultiviewLayout.hpp"

#include <cstdio>

using namespace desk;

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

/* every slot is inside the frame, they do not overlap, and together they cover every pixel */
static void covers(int width, int height)
{
	const std::array<SlotRect, MultiviewSlots> slots = multiviewSlots(width, height);

	long long area = 0;
	for (const SlotRect &s : slots) {
		CHECK(s.x >= 0 && s.y >= 0);
		CHECK(s.x + s.w <= width);
		CHECK(s.y + s.h <= height);
		/* no empty slot; below width 8 an eight-across row cannot give every input a pixel */
		if (width >= 8 && height >= 2) {
			CHECK(s.w > 0 && s.h > 0);
		}
		area += (long long)s.w * (long long)s.h;
	}
	CHECK(area == (long long)width * (long long)height);

	for (int i = 0; i < MultiviewSlots; i++) {
		for (int j = i + 1; j < MultiviewSlots; j++) {
			const SlotRect &a = slots[(size_t)i];
			const SlotRect &b = slots[(size_t)j];
			const bool apart = a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
			CHECK(apart);
		}
	}
}

int main()
{
	/* Preview is the top-left half, Program the top-right half */
	{
		const std::array<SlotRect, MultiviewSlots> slots = multiviewSlots(1920, 1080);
		CHECK(slots[0].x == 0 && slots[0].y == 0 && slots[0].w == 960 && slots[0].h == 540);
		CHECK(slots[1].x == 960 && slots[1].y == 0 && slots[1].w == 960 && slots[1].h == 540);
	}

	/* the eight input slots run along the bottom, left to right */
	{
		const std::array<SlotRect, MultiviewSlots> slots = multiviewSlots(1920, 1080);
		for (int i = 0; i < 8; i++) {
			CHECK(slots[(size_t)(2 + i)].y == 540);
			CHECK(slots[(size_t)(2 + i)].h == 540);
			CHECK(slots[(size_t)(2 + i)].w == 240);
			CHECK(slots[(size_t)(2 + i)].x == i * 240);
		}
	}

	/* they tile exactly at a few sizes, including odd ones that do not divide evenly */
	covers(1920, 1080);
	covers(1280, 720);
	covers(3840, 2160);
	covers(1000, 575);
	covers(7, 3);

	if (failures == 0) {
		std::printf("MultiviewLayout: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
```

Create `breadstudio/desk/MultiviewLayout.cpp` with a stub so the test builds and fails:
```cpp
#include "MultiviewLayout.hpp"

namespace desk {

std::array<SlotRect, MultiviewSlots> multiviewSlots(int, int)
{
	return {};
}

} // namespace desk
```

At the end of `breadstudio/desk/CMakeLists.txt`, add:
```cmake

# The ten MultiView boxes, for any frame size: no OBS, no Qt.
add_executable(bread-multiview-test test/MultiviewLayoutTest.cpp MultiviewLayout.cpp)
target_include_directories(bread-multiview-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(
  bread-multiview-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/desk/test-bin" FOLDER plugins/bread-desk
)
```

- [ ] **Step 3: Run it and see it fail**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-multiview-test
& build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-multiview-test.exe; $LASTEXITCODE
```
Expected: `FAIL line …` lines and `1`.

- [ ] **Step 4: Implement it**

Replace `breadstudio/desk/MultiviewLayout.cpp` with:
```cpp
#include "MultiviewLayout.hpp"

namespace desk {

std::array<SlotRect, MultiviewSlots> multiviewSlots(int width, int height)
{
	std::array<SlotRect, MultiviewSlots> slots{};
	const int topH = height / 2;
	const int bottomY = topH;
	const int bottomH = height - topH;
	const int leftW = width / 2;

	/* Preview and Program: the top half, left and right */
	slots[0] = {0, 0, leftW, topH};
	slots[1] = {leftW, 0, width - leftW, topH};

	/* the eight inputs: the bottom half, eight across, so no pixel is lost to rounding */
	for (int i = 0; i < 8; i++) {
		const int x0 = width * i / 8;
		const int x1 = width * (i + 1) / 8;
		slots[(size_t)(2 + i)] = {x0, bottomY, x1 - x0, bottomH};
	}
	return slots;
}

} // namespace desk
```

- [ ] **Step 5: Run it and see it pass**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-multiview-test
& build_x64\breadstudio\desk\test-bin\RelWithDebInfo\bread-multiview-test.exe; $LASTEXITCODE
```
Expected: `MultiviewLayout: all checks passed` and `0`.

- [ ] **Step 6: Commit**

```powershell
git add breadstudio/desk/MultiviewLayout.hpp breadstudio/desk/MultiviewLayout.cpp breadstudio/desk/test/MultiviewLayoutTest.cpp breadstudio/desk/CMakeLists.txt
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: work out the MultiView's ten boxes for any frame size"
```

---

### Task 2: The MultiView source, and the engine's MultiView case

**Files:**
- Create: `breadstudio/desk/MultiviewSource.hpp`, `breadstudio/desk/MultiviewSource.cpp`
- Modify: `breadstudio/desk/OutputEngine.hpp`, `breadstudio/desk/OutputEngine.cpp`, `breadstudio/desk/desk.cpp`, `breadstudio/desk/CMakeLists.txt`, `breadstudio/desk/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `multiviewSlots`, `MultiviewSlots`, `SlotRect` (Task 1); `ProgramColor`/`PreviewColor`/`IdleColor` (`Desk.hpp`); `inputs()` (`InputModel.hpp`); the engine's `Running`, `program`, `multiview` members and `sourceFor()`.
- Produces:
  - `inline constexpr const char *MultiviewSourceId = "bread_multiview";`, `void registerMultiviewSource();` and `void refreshMultiview(obs_source_t *source);` in `MultiviewSource.hpp`.
  - The OBS source type `bread_multiview`, drawn at the main mix's base size.
  - The engine's `sourceFor()` returns one `bread_multiview` for an output whose source is MultiView.

- [ ] **Step 1: Write the source**

Create `breadstudio/desk/MultiviewSource.hpp`:
```cpp
#pragma once

#include <obs-module.h>

namespace desk {

inline constexpr const char *MultiviewSourceId = "bread_multiview";

/* bread_multiview: the desk's own monitoring view (outputs spec §5) — Preview and Program across
 * the top, the inputs below, each with its number, name and a red or green frame by the input
 * bar's rule. Drawn at the main mix's size; an output's canvas scales it. Hidden from every list
 * of sources. */
void registerMultiviewSource();

/* Gather Program, Preview, the inputs and the studio-mode flag, and lay out the labels, for the
 * source the engine holds. UI thread only; the engine calls it from refreshSources(). */
void refreshMultiview(obs_source_t *source);

/* Empty the view: the scene collection it showed is going (the engine's detachSources). */
void clearMultiview(obs_source_t *source);

} // namespace desk
```

Create `breadstudio/desk/MultiviewSource.cpp`:
```cpp
#include "MultiviewSource.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "MultiviewLayout.hpp"

#include <obs.hpp>
#include <obs-frontend-api.h>

#include <array>
#include <mutex>
#include <string>
#include <vector>

namespace desk {

/* the frame round each box, in the box's own pixels */
static const float Border = 6.0f;

struct Multiview {
	obs_source_t *source = nullptr;

	/* gathered on the UI thread by publishMultiview, read on the graphics thread by the render */
	OBSSourceAutoRelease program;
	OBSSourceAutoRelease preview;
	std::vector<OBSSource> inputs;
	bool studio = false;

	/* one private text source per slot */
	std::array<OBSSourceAutoRelease, MultiviewSlots> labels;

	std::mutex mutex; /* the render and publishMultiview run on different threads */
};

/* a solid box of one colour, its top-left at (x, y) */
static void solidBox(float x, float y, float w, float h, uint32_t color)
{
	if (w <= 0.0f || h <= 0.0f) {
		return;
	}
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *param = gs_effect_get_param_by_name(solid, "color");
	gs_effect_set_color(param, color);
	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);
	while (gs_effect_loop(solid, "Solid")) {
		gs_draw_sprite(nullptr, 0, (uint32_t)w, (uint32_t)h);
	}
	gs_matrix_pop();
}

/* draws `picture` scaled to fill the box at (x, y) by (w, h), in the source's own coordinates */
static void drawPicture(float x, float y, float w, float h, obs_source_t *picture)
{
	if (!picture) {
		return;
	}
	const uint32_t pw = obs_source_get_width(picture);
	const uint32_t ph = obs_source_get_height(picture);
	if (pw == 0 || ph == 0) {
		return;
	}
	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);
	gs_matrix_scale3f(w / (float)pw, h / (float)ph, 1.0f);
	obs_source_video_render(picture);
	gs_matrix_pop();
}

/* one private text source for a slot label (UI thread; may enter the graphics context) */
static OBSSourceAutoRelease makeLabel(const std::string &text, uint32_t height)
{
	OBSDataAutoRelease settings = obs_data_create();
	OBSDataAutoRelease font = obs_data_create();
	obs_data_set_string(font, "face", "Arial");
	obs_data_set_int(font, "flags", 1); /* bold */
	obs_data_set_int(font, "size", (int)(height / 18));
	obs_data_set_obj(settings, "font", font);
	obs_data_set_string(settings, "text", text.c_str());
	obs_data_set_bool(settings, "outline", false);
	return obs_source_create_private("text_gdiplus", "BreadStudio MultiView label", settings);
}

static uint32_t multiviewWidth(void *)
{
	obs_video_info ovi = {};
	return obs_get_video_info(&ovi) ? ovi.base_width : 0;
}

static uint32_t multiviewHeight(void *)
{
	obs_video_info ovi = {};
	return obs_get_video_info(&ovi) ? ovi.base_height : 0;
}

/* Builds everything the render needs, OFF mv->mutex (creating and destroying sources enters the
 * graphics context, and holding the lock across that would deadlock the graphics thread), then
 * publishes it under the lock; the old set is released after the lock, back on this thread. With
 * `live` false the view is emptied, for a scene collection that is going. */
static void publishMultiview(Multiview *mv, bool live)
{
	const uint32_t height = multiviewHeight(nullptr);

	OBSSourceAutoRelease program;
	OBSSourceAutoRelease preview;
	std::vector<OBSSource> nextInputs;
	std::array<OBSSourceAutoRelease, MultiviewSlots> nextLabels;
	bool studio = false;
	if (live) {
		program = obs_frontend_get_current_scene();
		preview = obs_frontend_get_current_preview_scene();
		nextInputs = inputs();
		studio = obs_frontend_preview_program_mode_active();
		nextLabels[0] = makeLabel(obs_module_text("Multiview.Preview"), height);
		nextLabels[1] = makeLabel(obs_module_text("Multiview.Program"), height);
		for (int i = 0; i < 8; i++) {
			std::string text;
			if (i < (int)nextInputs.size()) {
				text = std::to_string(i + 1) + "  " + obs_source_get_name(nextInputs[(size_t)i].Get());
			}
			nextLabels[(size_t)(2 + i)] = makeLabel(text, height);
		}
	}

	OBSSourceAutoRelease oldProgram;
	OBSSourceAutoRelease oldPreview;
	std::vector<OBSSource> oldInputs;
	std::array<OBSSourceAutoRelease, MultiviewSlots> oldLabels;
	{
		std::lock_guard lock(mv->mutex);
		oldProgram = std::move(mv->program);
		oldPreview = std::move(mv->preview);
		oldInputs.swap(mv->inputs);
		oldLabels.swap(mv->labels);
		mv->program = std::move(program);
		mv->preview = std::move(preview);
		mv->inputs.swap(nextInputs);
		mv->studio = studio;
		mv->labels.swap(nextLabels);
	}
	/* oldProgram, oldPreview, oldInputs and oldLabels die here, off the lock */
}

void refreshMultiview(obs_source_t *source)
{
	auto *mv = static_cast<Multiview *>(obs_obj_get_data(source));
	if (mv) {
		publishMultiview(mv, true);
	}
}

void clearMultiview(obs_source_t *source)
{
	auto *mv = static_cast<Multiview *>(obs_obj_get_data(source));
	if (mv) {
		publishMultiview(mv, false);
	}
}

static const char *multiviewName(void *)
{
	return obs_module_text("Multiview");
}

static void *multiviewCreate(obs_data_t *, obs_source_t *source)
{
	auto *mv = new Multiview();
	mv->source = source;
	return mv;
}

static void multiviewDestroy(void *data)
{
	delete static_cast<Multiview *>(data);
}

/* draws one box: its frame, its picture and its label. Called with mv->mutex held. */
static void drawSlot(Multiview *mv, int slot, const SlotRect &box, obs_source_t *picture, uint32_t frame)
{
	const float innerX = (float)box.x + Border;
	const float innerY = (float)box.y + Border;
	const float innerW = (float)box.w - 2 * Border;
	const float innerH = (float)box.h - 2 * Border;

	solidBox((float)box.x, (float)box.y, (float)box.w, (float)box.h, frame);
	solidBox(innerX, innerY, innerW, innerH, 0xFF000000u);
	drawPicture(innerX, innerY, innerW, innerH, picture);

	obs_source_t *label = mv->labels[(size_t)slot];
	if (!label) {
		return;
	}
	const float lw = (float)obs_source_get_width(label);
	const float lh = (float)obs_source_get_height(label);
	if (lw == 0.0f || lh == 0.0f) {
		return;
	}
	const float lx = (float)box.x + Border;
	const float ly = (float)(box.y + box.h) - lh - Border;
	solidBox(lx, ly, lw, lh, 0x88000000u);
	gs_matrix_push();
	gs_matrix_translate3f(lx, ly, 0.0f);
	obs_source_video_render(label);
	gs_matrix_pop();
}

static void multiviewRender(void *data, gs_effect_t *)
{
	auto *mv = static_cast<Multiview *>(data);
	const uint32_t width = multiviewWidth(nullptr);
	const uint32_t height = multiviewHeight(nullptr);
	if (width == 0 || height == 0) {
		return;
	}

	const std::array<SlotRect, MultiviewSlots> boxes = multiviewSlots((int)width, (int)height);

	std::lock_guard lock(mv->mutex);
	obs_source_t *program = mv->program.Get();
	obs_source_t *preview = mv->preview.Get();

	/* Preview and Program: the top two boxes, no tally frame (spec §5) */
	obs_source_t *previewPicture = mv->studio ? preview : program;
	drawSlot(mv, 0, boxes[0], previewPicture, 0xFF000000u);
	drawSlot(mv, 1, boxes[1], program, 0xFF000000u);

	/* the inputs: the bottom eight boxes, framed by the input bar's rule */
	for (int i = 0; i < 8; i++) {
		obs_source_t *input = i < (int)mv->inputs.size() ? mv->inputs[(size_t)i].Get() : nullptr;

		uint32_t frame = IdleColor;
		if (input && input == program) {
			frame = ProgramColor;
		} else if (mv->studio && input && input == preview) {
			frame = PreviewColor;
		}
		drawSlot(mv, 2 + i, boxes[(size_t)(2 + i)], input, frame);
	}
}

void registerMultiviewSource()
{
	obs_source_info info = {};
	info.id = MultiviewSourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_CAP_DISABLED;
	info.get_name = multiviewName;
	info.create = multiviewCreate;
	info.destroy = multiviewDestroy;
	info.get_width = multiviewWidth;
	info.get_height = multiviewHeight;
	info.video_render = multiviewRender;
	obs_register_source(&info);
}

} // namespace desk
```

- [ ] **Step 2: Wire it into the engine**

In `breadstudio/desk/OutputEngine.hpp`, add `#include "MultiviewSource.hpp"` beside the other includes, and add to the private members, after the `program` line:
```cpp
	OBSSourceAutoRelease multiview; /* one bread_multiview for every output that shows MultiView */
```

In `breadstudio/desk/OutputEngine.cpp`, in `sourceFor()`, replace the MultiView case:
```cpp
	case OutputSource::MultiView:
		return OBSSource(); /* Plan E fills this in */
```
with:
```cpp
	case OutputSource::MultiView:
		return OBSSource(multiview.Get());
```

In `reconcile()`, after the block that creates `program`, add its twin:
```cpp
	if (!multiview) {
		multiview = obs_source_create_private(MultiviewSourceId, "BreadStudio MultiView", nullptr);
		refreshMultiview(multiview); /* gather now: apply() does not call refreshSources() */
	}
```

In `refreshSources()`, after the loop that calls `show(i)`, add:
```cpp
	/* the multiview reads what the desk shows; gather it now, on this thread */
	if (multiview) {
		refreshMultiview(multiview);
	}
```

In `detachSources()`, after the loop that blanks the outputs, add:
```cpp
	/* let go of the scenes of the collection that is going */
	if (multiview) {
		clearMultiview(multiview);
	}
```

In `shutdown()`, after `program = nullptr;`, add:
```cpp
	multiview = nullptr;
```

- [ ] **Step 3: Register it in the desk**

In `breadstudio/desk/desk.cpp`, add `#include "MultiviewSource.hpp"` to the includes, and in `obs_module_load`, immediately after `registerProgramSource();`, add:
```cpp
	registerMultiviewSource();
```

- [ ] **Step 4: Build files and strings**

In `breadstudio/desk/CMakeLists.txt`, add to the plugin's sources, in alphabetical places:
```cmake
    MultiviewLayout.cpp
    MultiviewLayout.hpp
    MultiviewSource.cpp
    MultiviewSource.hpp
```

Append to `breadstudio/desk/data/locale/en-US.ini`:
```ini
Multiview="MultiView"
Multiview.Preview="Preview"
Multiview.Program="Program"
```

- [ ] **Step 5: Build, then check by hand**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-desk
```
Expected: builds with no warning.

Start the program, add three inputs, then in the Outputs window set Output 2's source to MultiView and give it an SRT listener on 9002.

| Do | Check |
|---|---|
| `ffplay srt://127.0.0.1:9002` | Preview and Program across the top, the inputs below, each with its number and name. |
| Click an input's tile (Preview) | That slot's frame turns green within a second. |
| Put an input on Program | Its slot is red. |
| Studio Mode off | No slot is green; the Preview box shows Program. |
| Add and delete an input | The bottom slots follow within a second. |
| Change Output 2's size to 1280×720; OK | Only the 9002 receiver restarts; the view is the same layout, smaller. |
| Close the program | It closes within five seconds; the log ends with `Number of memory leaks: 0`. |

- [ ] **Step 6: Commit**

```powershell
git add breadstudio/desk
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the MultiView source, and let an output show it"
```

---

### Task 3: The smoke checklist, and the run

**Files:**
- Modify: `breadstudio/docs/DESK-SMOKE.md`

- [ ] **Step 1: Add a MultiView item to section H**

In `breadstudio/docs/DESK-SMOKE.md`, in section H, after its last item (item 8), add:
```markdown
9. Output 2 set to MultiView, at 1280×720: its receiver shows Preview and
   Program across the top and the inputs below, each with its number, name
   and a frame that matches the input bar's — red on Program, green on
   Preview, idle otherwise; with Studio Mode off the Preview box shows
   Program and nothing is green. Changing Program, Preview or the input list
   is reflected within a second; changing the size restarts only its own
   receivers.
```

- [ ] **Step 2: Run the automated tests**

```powershell
cmake --build --preset windows-x64 --config RelWithDebInfo --target bread-multiview-test bread-outputs-test bread-stream-test bread-sources-test bread-desk-test
foreach ($t in 'bread-multiview-test', 'bread-outputs-test', 'bread-stream-test', 'bread-sources-test', 'bread-desk-test') { & "build_x64\breadstudio\desk\test-bin\RelWithDebInfo\$t.exe" | Select-Object -Last 1; "exit $LASTEXITCODE" }
```
Expected: each ends `exit 0` (`MultiviewLayout`, `OutputPlan`, `StreamAddress`, `SourceList`, `LayerGraph`).

- [ ] **Step 3: Run section H, and record**

Build everything (`breadstudio\build.ps1`), run section H on this PC, and add an entry under `## Runs` in the existing form.

- [ ] **Step 4: Commit**

```powershell
git add breadstudio/docs
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git commit -m "breadstudio: add the MultiView to the smoke checklist, and record the run"
```

---

## Left for later plans

| Item | Where |
|---|---|
| `bread_ndi_output` and `bread_omt_output` to the engine's raw-output contract | Plan F |
| A second PC receiving over NDI and OMT | Plan F's run |
| Mix inputs, their window, the on-air rule's extra step | Plan G |