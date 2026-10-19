#include "VmixPreset.hpp"

#include "Desk.hpp"
#include "GtSettings.hpp"
#include "InputModel.hpp"

#include <obs.hpp>

#include <callback/calldata.h>
#include <callback/proc.h>
#include <util/platform.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <set>
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

/* A path's existence, the probe findVmixFile walks a moved GT title's tail against (§6).
 * os_file_exists takes the UTF-8 path findVmixFile works in. */
bool fileExists(const std::string &path)
{
	return !path.empty() && os_file_exists(path.c_str());
}

/* Whether a title field is a picture's `.Source` (vMix's own name, §4.3), the only
 * field whose value can be a relocated file path. */
bool gtSourceField(const std::string &name)
{
	return name.size() > 7 && name.compare(name.size() - 7, 7, ".Source") == 0;
}

void addRow(std::vector<VmixReportRow> &rows, const std::string &input, const std::string &had, const std::string &why)
{
	VmixReportRow row;
	row.input = input;
	row.had = had;
	row.why = why;
	rows.push_back(row);
}

/* The settings each kind's base is made with (spec §5.2's table), the same shape
 * an Add-menu page hands createInput. A value the reader did not carry leaves the
 * base empty — an absent device, a title with no text — which is a report row,
 * never a guess. A GT title's file, values and styles are built here; its countdowns
 * need the source's own gt_info, so they follow in applyGtInfo. */
OBSDataAutoRelease baseSettings(const VmixInput &input, const std::string &presetDir, std::vector<VmixReportRow> &rows)
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
	case VmixKind::GtTitle: {
		/* the .gtzip's path, found under the preset's folder when it has moved
		 * (§6). A file not found is still made empty and reported, as a missing
		 * video file is; the stored path is carried so the source says what it
		 * could not read. */
		const std::string name = rowName(input);
		const std::string found = findVmixFile(input.titleFile, presetDir, fileExists);
		obs_data_set_string(settings, gt::KeyFile, (found.empty() ? input.titleFile : found).c_str());
		if (found.empty()) {
			addRow(rows, name, "file '" + input.titleFile + "'",
			       "the title file was not found; the input is empty");
		} else if (found != input.titleFile) {
			addRow(rows, name, "file '" + input.titleFile + "'", "found at '" + found + "'");
		}

		OBSDataAutoRelease values = obs_data_create();
		OBSDataAutoRelease styles = obs_data_create();
		for (const VmixTitleItem &item : input.titleItems) {
			if (item.name.empty()) {
				continue;
			}
			std::string value = item.value;
			/* a picture's path or file:/// URL is relocated like the title file,
			 * so a wrong match is never silent (§6); a package resource name is
			 * left as it is, because it names the file's own picture */
			if (gtSourceField(item.name) && vmixAbsolutePath(value)) {
				const std::string relocated = findVmixFile(value, presetDir, fileExists);
				if (!relocated.empty()) {
					if (relocated != value) {
						addRow(rows, name, "file '" + value + "'",
						       "found at '" + relocated + "'");
					}
					value = relocated;
				}
			}
			obs_data_set_string(values, item.name.c_str(), value.c_str());

			/* the item's style, in vMix's own vocabulary (§4.4); a key vMix did
			 * not write is left out, so the field keeps the file's own value */
			if (item.hasStyle) {
				OBSDataAutoRelease style = obs_data_create();
				if (!item.font.empty()) {
					obs_data_set_string(style, "font", item.font.c_str());
				}
				if (item.size > 0) {
					obs_data_set_double(style, "size", item.size);
				}
				if (!item.fill.empty()) {
					obs_data_set_string(style, "fill", item.fill.c_str());
				}
				if (!item.weight.empty()) {
					obs_data_set_string(style, "weight", item.weight.c_str());
				}
				if (!item.style.empty()) {
					obs_data_set_bool(style, "italic", item.style == "Italic");
				}
				if (!item.stretch.empty()) {
					obs_data_set_string(style, "stretch", item.stretch.c_str());
				}
				if (item.lineSpacing != 0) {
					obs_data_set_double(style, "line_spacing", item.lineSpacing);
				}
				obs_data_set_obj(styles, item.name.c_str(), style);
			}
		}
		obs_data_set_obj(settings, gt::KeyValues, values);
		obs_data_set_obj(settings, gt::KeyStyles, styles);

		/* the data-source links are kept unchanged for project 3 (§6) */
		if (!input.dataSourcesXml.empty()) {
			obs_data_set_string(settings, gt::KeyDataSources, input.dataSourcesXml.c_str());
		}
		break;
	}
	default:
		break; /* Title (no text carried), Browser and Desktop Capture: no carried settings */
	}
	return settings;
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

/* A GT title's fields, warnings and countdowns, read from the source's own gt_info
 * once it exists (§6). The preset's countdown array holds one entry per editable
 * **text** field, in field order (Task 5 Step 1, established from the owner's
 * presets), so countdown index i is the i-th text field gt_info lists. A real
 * countdown replaces that field's text value — the reading the preset saved beside
 * it is frozen, not the field's text — and is written through obs_source_update. */
void applyGtInfo(obs_source_t *base, const VmixInput &vmix, obs_data_t *settings, std::vector<VmixReportRow> &rows)
{
	calldata_t data;
	calldata_init(&data);
	std::string json = "{}";
	if (proc_handler_call(obs_source_get_proc_handler(base), gt::ProcInfo, &data)) {
		const char *value = calldata_string(&data, "json");
		if (value) {
			json = value;
		}
	}
	calldata_free(&data);

	const QJsonObject root = QJsonDocument::fromJson(QByteArray::fromStdString(json)).object();
	const QJsonArray fields = root.value(QStringLiteral("fields")).toArray();
	const std::string name = rowName(vmix);

	std::set<std::string> names;
	std::vector<std::string> textFields;
	for (const QJsonValue &value : fields) {
		const QJsonObject object = value.toObject();
		const std::string field = object.value(QStringLiteral("name")).toString().toStdString();
		names.insert(field);
		if (object.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
			textFields.push_back(field);
		}
	}

	/* an item the title has no field of is reported, never applied blindly (§6) */
	for (const VmixTitleItem &item : vmix.titleItems) {
		if (!item.name.empty() && !names.count(item.name)) {
			addRow(rows, name, "value for '" + item.name + "'", "the title has no field of this name");
		}
	}

	/* one row per warning the source reports (§7) */
	for (const QJsonValue &value : root.value(QStringLiteral("warnings")).toArray()) {
		const QJsonObject object = value.toObject();
		addRow(rows, name, object.value(QStringLiteral("kind")).toString().toStdString(),
		       object.value(QStringLiteral("detail")).toString().toStdString());
	}

	/* each <mapper> is one link (§6) */
	for (int i = 0; i < vmix.dataLinks; i++) {
		addRow(rows, name, "linked to a data source", "not live until the data link is built");
	}

	OBSDataAutoRelease values = obs_data_get_obj(settings, gt::KeyValues);
	OBSDataAutoRelease countdowns = obs_data_create();
	bool any = false;
	for (std::size_t i = 0; i < vmix.countdowns.size(); i++) {
		const VmixCountdown &countdown = vmix.countdowns[i];
		/* a 00:00:00 duration is no countdown: most of the owner's inputs carry one */
		if (countdown.duration.empty() || countdown.duration == "00:00:00") {
			continue;
		}
		if (i >= textFields.size()) {
			continue; /* the array is longer than the title's text fields */
		}
		const std::string &field = textFields[i];
		OBSDataAutoRelease entry = obs_data_create();
		obs_data_set_string(entry, "start", countdown.start.c_str());
		obs_data_set_string(entry, "duration", countdown.duration.c_str());
		obs_data_set_string(entry, "format", countdown.format.c_str());
		obs_data_set_bool(entry, "reverse", countdown.reverse);
		obs_data_set_bool(entry, "reverse_display", countdown.reverseDisplay);
		obs_data_set_obj(countdowns, field.c_str(), entry);
		if (values) {
			obs_data_erase(values, field.c_str());
		}
		any = true;
	}
	if (any) {
		obs_data_set_obj(settings, gt::KeyCountdowns, countdowns);
		if (values) {
			obs_data_set_obj(settings, gt::KeyValues, values);
		}
		obs_source_update(base, settings);
	}
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
		OBSDataAutoRelease settings = baseSettings(vmix, result.presetDir, result.report);
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
			 * Colour is a deliberate blank: empty is what it should be. A GT title
			 * is a real GT title input; its countdowns, absent fields and warnings
			 * follow from the source's own gt_info (§6), and its file row is its
			 * own, so it is not judged as an empty base. */
			if (vmix.kind == VmixKind::Title) {
				addRow(result.report, name, "vMix title '" + vmix.originalTitle + "'",
				       "imported as a plain text source; its text and layout are not carried");
			} else if (vmix.kind == VmixKind::GtTitle) {
				applyGtInfo(base.Get(), vmix, settings, result.report);
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
