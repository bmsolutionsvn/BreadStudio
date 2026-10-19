#include "MultiviewSource.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "MultiviewLayout.hpp"

#include <obs.hpp>
#include <obs-frontend-api.h>

#include <algorithm>
#include <array>
#include <mutex>
#include <string>
#include <vector>

namespace desk {

/* the frame round each box: 6 px on a 1080-line canvas and the same share of any other, so a
 * 4K output does not get a hairline nor a 720p one a slab (owner's backlog B5) */
static float borderFor(uint32_t height)
{
	return std::max(2.0f, (float)height / 180.0f);
}

struct Multiview {
	/* gathered on the UI thread by publishMultiview, read on the graphics thread by the render */
	OBSSourceAutoRelease program;
	OBSSourceAutoRelease preview;
	std::vector<OBSSource> inputs;
	std::vector<uint32_t> categories; /* one per input: the category tint, 0 for none (spec I4) */
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

/* draws `picture` whole and centred in `box`, in the source's own coordinates: the fitted
 * rectangle keeps the picture's own aspect, so it is letterboxed or pillarboxed, never stretched */
static void drawPicture(const SlotRect &box, obs_source_t *picture)
{
	if (!picture) {
		return;
	}
	const uint32_t pw = obs_source_get_width(picture);
	const uint32_t ph = obs_source_get_height(picture);
	if (pw == 0 || ph == 0) {
		return;
	}
	const SlotRect fit = fitInside(box, (int)pw, (int)ph);
	if (fit.w == 0 || fit.h == 0) {
		return;
	}
	gs_matrix_push();
	gs_matrix_translate3f((float)fit.x, (float)fit.y, 0.0f);
	gs_matrix_scale3f((float)fit.w / (float)pw, (float)fit.h / (float)ph, 1.0f);
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
	obs_data_set_int(font, "size", (int)(height / 36));
	obs_data_set_obj(settings, "font", font);
	obs_data_set_string(settings, "text", text.c_str());
	obs_data_set_bool(settings, "outline", false);
	return obs_source_create_private("text_gdiplus", "BreadStudio MultiView label", settings);
}

/* the category tint of each input, gathered on the UI thread with the inputs themselves */
static std::vector<uint32_t> categoryTints(const std::vector<OBSSource> &inputs)
{
	std::vector<uint32_t> tints;
	tints.reserve(inputs.size());
	for (const OBSSource &input : inputs) {
		tints.push_back(categoryColour(input));
	}
	return tints;
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
	std::vector<uint32_t> nextCategories = categoryTints(nextInputs);

	OBSSourceAutoRelease oldProgram;
	OBSSourceAutoRelease oldPreview;
	std::vector<OBSSource> oldInputs;
	std::vector<uint32_t> oldCategories;
	std::array<OBSSourceAutoRelease, MultiviewSlots> oldLabels;
	{
		std::lock_guard lock(mv->mutex);
		oldProgram = std::move(mv->program);
		oldPreview = std::move(mv->preview);
		oldInputs.swap(mv->inputs);
		oldCategories.swap(mv->categories);
		oldLabels.swap(mv->labels);
		mv->program = std::move(program);
		mv->preview = std::move(preview);
		mv->inputs.swap(nextInputs);
		mv->categories.swap(nextCategories);
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

static void *multiviewCreate(obs_data_t *, obs_source_t *)
{
	return new Multiview();
}

static void multiviewDestroy(void *data)
{
	delete static_cast<Multiview *>(data);
}

/* draws one box: its frame, its picture and its title. As vMix places its headings, the title
 * has its own strip under the picture rather than covering it (owner's backlog B5). Called with
 * mv->mutex held. */
static void drawSlot(Multiview *mv, int slot, const SlotRect &box, obs_source_t *picture, uint32_t frame,
		     float border, float titleHeight)
{
	const float innerX = (float)box.x + border;
	const float innerY = (float)box.y + border;
	const float innerW = (float)box.w - 2 * border;
	const float innerH = (float)box.h - 2 * border;

	solidBox((float)box.x, (float)box.y, (float)box.w, (float)box.h, frame);
	solidBox(innerX, innerY, innerW, innerH, 0xFF000000u);

	/* every slot has the same strip, so an empty slot's picture area lines up with its
	 * neighbours'; a title taller than half the box is left off rather than squeezing the picture
	 * away */
	const float strip = titleHeight * 2.0f < innerH ? titleHeight : 0.0f;
	drawPicture({(int)innerX, (int)innerY, (int)innerW, (int)(innerH - strip)}, picture);

	obs_source_t *label = mv->labels[(size_t)slot];
	if (strip == 0.0f || !label || obs_source_get_width(label) == 0) {
		return;
	}
	gs_matrix_push();
	gs_matrix_translate3f(innerX + border, innerY + innerH - strip, 0.0f);
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
	const float border = borderFor(height);

	std::lock_guard lock(mv->mutex);
	/* the strip's height is the one title that always has text: every label has the same font */
	const float titleHeight = mv->labels[0] ? (float)obs_source_get_height(mv->labels[0]) : 0.0f;
	obs_source_t *program = mv->program.Get();
	obs_source_t *preview = mv->preview.Get();

	/* Preview and Program: the top two boxes, framed green and red as vMix marks them (owner's
	 * backlog B5); with Studio Mode off the Preview box shows Program and is not green */
	obs_source_t *previewPicture = mv->studio ? preview : program;
	drawSlot(mv, 0, boxes[0], previewPicture, mv->studio ? PreviewColor : IdleColor, border, titleHeight);
	drawSlot(mv, 1, boxes[1], program, ProgramColor, border, titleHeight);

	/* the inputs: the bottom eight boxes, framed by the input bar's rule */
	for (int i = 0; i < 8; i++) {
		obs_source_t *input = i < (int)mv->inputs.size() ? mv->inputs[(size_t)i].Get() : nullptr;

		/* the tally wins: an input on air is red whatever its category, and a category tints only
		 * an idle slot (spec I4; owner's backlog B5) */
		uint32_t frame = IdleColor;
		if (input && input == program) {
			frame = ProgramColor;
		} else if (mv->studio && input && input == preview) {
			frame = PreviewColor;
		} else if (i < (int)mv->categories.size() && mv->categories[(size_t)i]) {
			frame = mv->categories[(size_t)i];
		}
		drawSlot(mv, 2 + i, boxes[(size_t)(2 + i)], input, frame, border, titleHeight);
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
