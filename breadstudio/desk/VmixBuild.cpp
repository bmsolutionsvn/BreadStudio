#include "VmixPreset.hpp"

#include "Desk.hpp"
#include "InputModel.hpp"

#include <obs.hpp>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

namespace desk {

namespace {

/* The name the desk's own Add route gives an input: the vMix title, or what
 * OriginalTitle said when there is none, so a made input always has a name. */
std::string wantedName(const VmixInput &input)
{
	if (!input.title.empty()) {
		return input.title;
	}
	if (!input.originalTitle.empty()) {
		return input.originalTitle;
	}
	return "vMix input";
}

/* The name a report row carries: the vMix title, or OriginalTitle when there is none. */
std::string rowName(const VmixInput &input)
{
	return input.title.empty() ? input.originalTitle : input.title;
}

/* The settings each kind's base is made with (spec §5.2's table), the same shape
 * an Add-menu page hands createInput. A value the reader did not carry leaves the
 * base empty — an absent device, a title with no text — which is a report row,
 * never a guess. */
OBSDataAutoRelease baseSettings(const VmixInput &input)
{
	OBSDataAutoRelease settings = obs_data_create();
	switch (input.kind) {
	case VmixKind::Colour:
		obs_data_set_int(settings, "color",
				 static_cast<long long>(std::strtoll(input.colour.c_str(), nullptr, 10)));
		break;
	case VmixKind::Image:
		obs_data_set_string(settings, "file", input.file.c_str());
		break;
	case VmixKind::Video:
	case VmixKind::AudioFile:
	case VmixKind::Media:
		obs_data_set_bool(settings, "is_local_file", true);
		obs_data_set_string(settings, "local_file", input.file.c_str());
		break;
	case VmixKind::Capture:
		obs_data_set_string(settings, "video_device_id", input.device.c_str());
		break;
	case VmixKind::AudioIn:
		obs_data_set_string(settings, "device_id", input.device.c_str());
		break;
	case VmixKind::NDI:
		obs_data_set_string(settings, "ndi_source_name", input.device.c_str());
		break;
	default:
		break; /* Title (no text carried), Browser and Desktop Capture: no carried settings */
	}
	return settings;
}

void addRow(std::vector<VmixReportRow> &rows, const std::string &input, const std::string &had, const std::string &why)
{
	VmixReportRow row;
	row.input = input;
	row.had = had;
	row.why = why;
	rows.push_back(row);
}

/* Volume and mute (spec §5.2): vMix's VolumeF is a **linear gain**, so it is set as
 * the base's own volume — the value the mixer's fader shows — and mute as its ♪, on
 * the base the mixer's strip is attached to. Only where the source has audio, so a
 * colour or an image is not given a meaningless level. */
void applyAudio(obs_source_t *base, const VmixInput &input)
{
	if (!hasAudio(base)) {
		return;
	}
	obs_source_set_volume(base, static_cast<float>(input.volume));
	obs_source_set_muted(base, input.muted);
}

/* A base that has nothing to show or hear right after creation: the file or device
 * is missing, or the source has not started. Reported, not dropped (V4). An audio
 * file is judged by its audio, not its (absent) picture; a capture device opens
 * only when its scene goes live, so a working camera can read as empty here, which
 * is why the row says "yet". */
bool emptyBase(obs_source_t *base, VmixKind kind)
{
	if (kind == VmixKind::AudioIn || kind == VmixKind::AudioFile) {
		return !obs_source_audio_active(base);
	}
	if (obs_source_get_output_flags(base) & OBS_SOURCE_VIDEO) {
		return obs_source_get_width(base) <= 0 || obs_source_get_height(base) <= 0;
	}
	if (obs_source_get_output_flags(base) & OBS_SOURCE_AUDIO) {
		return !obs_source_audio_active(base);
	}
	return false;
}

/* What the report names the input's content by: its device, its file, or its Type. */
std::string hadSubject(const VmixInput &input)
{
	if (!input.device.empty()) {
		return "device '" + input.device + "'";
	}
	if (!input.file.empty()) {
		return "file '" + input.file + "'";
	}
	return "vMix input type " + std::to_string(input.type);
}

std::string matrixHad(const VmixInput &input)
{
	return "vMix matrix: zoom " + std::to_string(input.zoomX) + "x" + std::to_string(input.zoomY) + ", pan " +
	       std::to_string(input.panX) + "," + std::to_string(input.panY) + ", rotate " +
	       std::to_string(input.rotateZ);
}

/* The geometry (spec §5.4): the base's own transform and crop, the fields the desk's
 * transform page edits, from Program's matrix. Crop and hidden the desk holds
 * exactly; the placement vmixPlacement turns into the base's pixels; the serialized
 * fields it cannot apply exactly are reported (V4). */
void applyGeometry(obs_source_t *input, obs_source_t *base, const VmixInput &vmix, const VmixPresetResult &result,
		   std::vector<VmixReportRow> &rows)
{
	if (!vmix.hasGeometry) {
		return;
	}
	const std::string name = rowName(vmix);

	/* the matrix list is the input's own Main/Program placement (index 0, taken)
	 * followed by one per overlay layer; the desk builds no layers, so the overlays
	 * are not applied */
	if (vmix.matrixCount > 1) {
		const int layers = vmix.matrixCount - 1;
		addRow(rows, name,
		       std::to_string(vmix.matrixCount) + " placements: Program plus " + std::to_string(layers) +
			       " overlay layers",
		       "the desk builds no layers; it took Program and the overlay layers are not applied");
	}

	OBSSceneItem item = baseItem(input);
	const double srcW = static_cast<double>(obs_source_get_width(base));
	const double srcH = static_cast<double>(obs_source_get_height(base));

	/* vMix's Clipping is a fraction of the source per edge; the desk's crop is the
	 * same thing in the base's own pixels, which it holds */
	if (item && (vmix.cropLeft > 0 || vmix.cropTop > 0 || vmix.cropRight > 0 || vmix.cropBottom > 0)) {
		struct obs_sceneitem_crop crop = {};
		crop.left = static_cast<int>(std::lround(vmix.cropLeft * srcW));
		crop.top = static_cast<int>(std::lround(vmix.cropTop * srcH));
		crop.right = static_cast<int>(std::lround(vmix.cropRight * srcW));
		crop.bottom = static_cast<int>(std::lround(vmix.cropBottom * srcH));
		obs_sceneitem_set_crop(item, &crop);
	}

	/* vMix's Hidden is Program's own visibility, which the desk's item holds too */
	if (item && vmix.hidden) {
		obs_sceneitem_set_visible(item, false);
		addRow(rows, name, "hidden on this output", "the input is hidden, as vMix showed it");
	}

	/* serialized vMix fields the desk cannot apply exactly, so it reports them */
	if (vmix.postZoomX != 1 || vmix.postZoomY != 1) {
		addRow(rows, name, "post-zoom " + std::to_string(vmix.postZoomX) + "x" + std::to_string(vmix.postZoomY),
		       "not applied: vMix scales after panning, which the desk's base scale cannot express");
	}
	if (vmix.sourceRotation != 0) {
		addRow(rows, name, "source rotation R" + std::to_string(vmix.sourceRotation),
		       "not applied: the desk rotates the base, not the source, and vMix's rotation also rescales");
	}

	if (!vmix.matrixMoved || !item) {
		return; /* Program's identity: the base already fills the frame */
	}

	VmixPlacement place;
	if (!vmixPlacement(vmix, result.outputWidth, result.outputHeight, srcW, srcH, place)) {
		/* a shot that cannot be computed is reported, not applied wrong (V4) */
		addRow(rows, name, matrixHad(vmix),
		       "not applied: the shot cannot be computed (no output resolution, or the source has no size)");
		return;
	}

	/* the preset's frame maps onto the desk's canvas, so a preset cut at another
	 * resolution lands as the same fraction of the frame; the base's scale is of the
	 * base's own pixels */
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	const double frameW = result.outputWidth;
	const double frameH = result.outputHeight;
	const double kx = frameW > 0 ? static_cast<double>(ovi.base_width) / frameW : 1.0;
	const double ky = frameH > 0 ? static_cast<double>(ovi.base_height) / frameH : 1.0;

	/* the base is created bounded and OBS renders from the bound, ignoring the
	 * scale, so the bound goes before the shot is set (as the transform page does) */
	if (obs_sceneitem_get_bounds_type(item) != OBS_BOUNDS_NONE) {
		obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_NONE);
	}
	struct vec2 scale;
	vec2_set(&scale, static_cast<float>(place.scaleX * kx), static_cast<float>(place.scaleY * ky));
	obs_sceneitem_set_scale(item, &scale);
	obs_sceneitem_set_rot(item, static_cast<float>(place.rot));
	struct vec2 pos;
	vec2_set(&pos, static_cast<float>(place.x * kx), static_cast<float>(place.y * ky));
	obs_sceneitem_set_pos(item, &pos);

	addRow(rows, name, matrixHad(vmix),
	       "approximate: the desk places zoom, pan and rotation on the base, but vMix's fit, and a mirror, a "
	       "border and a rotate origin, are not carried");
}

} // namespace

void buildVmixPreset(VmixPresetResult &result, int limit)
{
	/* the over-limit split and the preset-wide rows are decided here, without OBS;
	 * the steps are what is left to make, in number order */
	const std::vector<VmixBuildStep> steps = planVmixBuild(result, limit);

	/* Every input becomes its own input, never a layer over another, so the desk's
	 * loop refusal (V5) has nothing to refuse: a vMix preset carries no layers the
	 * desk could close back on themselves. The bar's limit is the rule that applies,
	 * and planVmixBuild has already reported the inputs past it. */
	std::vector<OBSSource> made;
	made.reserve(steps.size());
	for (const VmixBuildStep &step : steps) {
		const VmixInput &vmix = *step.input;
		OBSDataAutoRelease settings = baseSettings(vmix);
		OBSSource input = createInput(step.baseId, wantedName(vmix).c_str(), settings);
		if (!input) {
			/* the desk has no such source type, or the source failed: report it
			 * and keep going, so one bad input never aborts the import (V4) */
			addRow(result.report, rowName(vmix), std::string("vMix ") + step.baseId,
			       "not made: the desk could not create this input's source");
			made.push_back(OBSSource());
			continue;
		}

		OBSSource base = baseSource(input);
		if (base) {
			const std::string name = rowName(vmix);
			applyAudio(base.Get(), vmix);

			/* a title is made as a plain text source (§10); its text and layout
			 * are not carried, so it is reported here and not asked to be empty. A
			 * Colour is a deliberate blank: empty is what it should be. */
			if (vmix.kind == VmixKind::Title) {
				addRow(result.report, name, "vMix title '" + vmix.originalTitle + "'",
				       "imported as a plain text source; its text and layout are not carried");
			} else if (vmix.kind != VmixKind::Colour && emptyBase(base.Get(), vmix.kind)) {
				addRow(result.report, name, hadSubject(vmix),
				       "empty: no picture or audio yet — the file or device is missing, or not started");
			}

			applyGeometry(input.Get(), base.Get(), vmix, result, result.report);
		}
		made.push_back(input);
	}

	/* Program and Preview, after the inputs exist (spec §5.2). Preview goes first so
	 * the cut to Program can leave it; with Studio Mode off both land on Program. */
	for (std::size_t i = 0; i < steps.size(); i++) {
		if (made[i] && steps[i].input->number == result.preview) {
			toPreview(made[i].Get());
		}
	}
	for (std::size_t i = 0; i < steps.size(); i++) {
		if (made[i] && steps[i].input->number == result.active) {
			cutToProgram(made[i].Get());
		}
	}

	/* an import is a change the operator will save (P9) */
	markProjectChanged();
	inputsChanged();
}

} // namespace desk
