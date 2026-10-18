#include "VmixPreset.hpp"

#include <QByteArray>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>

namespace desk {

/* vMix writes an input's Type as a bare number and documents none of it (the
 * format is private — spec V7). These are the numbers the owner's own presets
 * name; they were read out of the four shows in their archive and confirmed
 * against the installed vMix 26. A number this spec cannot name is Unknown: read
 * from OriginalTitle and put on the report, never guessed. */
static VmixKind kindOfType(int type)
{
	switch (type) {
	case 0: /* a video file, *.mp4 */
		return VmixKind::Video;
	case 1: /* an image, *.png */
		return VmixKind::Image;
	case 2:    /* vMix's Title, a *.xaml */
	case 9000: /* a *.gtzip GT title */
		return VmixKind::Title;
	case 5: /* Camera / Video Capture */
		return VmixKind::Capture;
	case 7: /* an audio capture device */
		return VmixKind::AudioIn;
	case 12: /* Colour */
		return VmixKind::Colour;
	case 13: /* an audio file, *.mp3 or *.wav */
		return VmixKind::AudioFile;
	case 14:   /* List: a playlist */
	case 21:   /* Image Sequence / Stinger */
	case 6000: /* Video Call */
		/* no desk equivalent: still imported as a row on the report */
		return VmixKind::Skipped;
	case 4000: /* NDI */
		return VmixKind::NDI;
	default: /* 15, 22, 100, and anything a later vMix adds */
		return VmixKind::Unknown;
	}
}

/* vMix's own name for a Type, for the report. "" when the spec cannot name it. */
static const char *typeName(int type)
{
	switch (type) {
	case 0:
		return "Video";
	case 1:
		return "Image";
	case 2:
		return "Title";
	case 5:
		return "Camera";
	case 7:
		return "Audio Input";
	case 12:
		return "Colour";
	case 13:
		return "Audio File";
	case 14:
		return "List";
	case 21:
		return "Image Sequence";
	case 4000:
		return "NDI";
	case 6000:
		return "Video Call";
	case 9000:
		return "GT Title";
	default:
		return "";
	}
}

/* A number under `parent`, or `fallback` when the element is not there or holds
 * no number — vMix leaving one out must not be read as zero. */
static double childNumber(const QDomElement &parent, const char *name, double fallback)
{
	const QDomElement child = parent.firstChildElement(QString::fromLatin1(name));
	if (child.isNull()) {
		return fallback;
	}
	bool ok = false;
	const double value = child.text().toDouble(&ok);
	return ok ? value : fallback;
}

/* The identity matrix is a full-frame input: nothing to place, nothing to report. */
static bool isIdentity(const QDomElement &position)
{
	const QDomElement rotate = position.firstChildElement(QStringLiteral("Rotate"));
	return childNumber(position, "ZoomX", 1) == 1 && childNumber(position, "ZoomY", 1) == 1 &&
	       childNumber(position, "PanX", 0) == 0 && childNumber(position, "PanY", 0) == 0 &&
	       childNumber(rotate, "X", 0) == 0 && childNumber(rotate, "Y", 0) == 0 && childNumber(rotate, "Z", 0) == 0;
}

/* vMix's source rotation as degrees: R0/R90/R180/R270. */
static int sourceRotationDegrees(const QDomElement &position)
{
	const QString value = position.firstChildElement(QStringLiteral("SourceRotation")).text();
	if (value == QStringLiteral("R90")) {
		return 90;
	}
	if (value == QStringLiteral("R180")) {
		return 180;
	}
	if (value == QStringLiteral("R270")) {
		return 270;
	}
	return 0;
}

/* vMix's Clipping, a float[4], is serialized (no [XmlIgnore]) as four numbers under
 * <Clipping>: the left, top, right and bottom **boundary positions as a fraction of
 * the source** (vMix's own default is 0,0,1,1 = whole). The desk crops in source
 * pixels, so each edge's crop is the boundary minus its own edge: left=a[0],
 * top=a[1], right=1-a[2], bottom=1-a[3] (read off hj.o() in vMix, which moves the
 * quad's four corners by those fractions). Absent means no crop. */
static void readClipping(const QDomElement &position, VmixInput &input)
{
	const QDomElement clipping = position.firstChildElement(QStringLiteral("Clipping"));
	if (clipping.isNull()) {
		return;
	}
	double edge[4] = {0, 0, 1, 1};
	int seen = 0;
	for (QDomElement value = clipping.firstChildElement(); !value.isNull() && seen < 4;
	     value = value.nextSiblingElement()) {
		bool ok = false;
		const double number = value.text().toDouble(&ok);
		if (ok) {
			edge[seen++] = number;
		}
	}
	if (seen < 4) {
		return; /* a shape the spec cannot read: leave the crop alone rather than guess */
	}
	input.cropLeft = edge[0];
	input.cropTop = edge[1];
	input.cropRight = 1.0 - edge[2];
	input.cropBottom = 1.0 - edge[3];
}

/* One MatrixPosition's transform, copied raw. */
static void takeMatrix(const QDomElement &position, VmixInput &input)
{
	input.zoomX = childNumber(position, "ZoomX", 1);
	input.zoomY = childNumber(position, "ZoomY", 1);
	input.panX = childNumber(position, "PanX", 0);
	input.panY = childNumber(position, "PanY", 0);
	const QDomElement rotate = position.firstChildElement(QStringLiteral("Rotate"));
	input.rotateX = childNumber(rotate, "X", 0);
	input.rotateY = childNumber(rotate, "Y", 0);
	input.rotateZ = childNumber(rotate, "Z", 0);
	input.postZoomX = childNumber(position, "PostZoomX", 1);
	input.postZoomY = childNumber(position, "PostZoomY", 1);
	input.sourceRotation = sourceRotationDegrees(position);
	input.hidden = position.firstChildElement(QStringLiteral("Hidden")).text() == QStringLiteral("true");
	readClipping(position, input);
}

/* vMix keeps an input's placement in the `Positions` attribute as a whole embedded
 * XML document — one MatrixPosition per placement, **index 0 first**. Index 0 is
 * the input's own Main/Program placement; the rest are its overlay **layers** (the
 * Position tab reads "Layers / Multi View", and vMix serializes them as `<overlay>`
 * elements). The desk builds no layers, so it takes index 0, Program's, and reports
 * the rest: that is what the desk's Program shows. A preset with more than one
 * placement leaves matrixCount above one. `matrixMoved` is false when the Main
 * matrix is the identity — a full-frame input, not a placed one. */
static void readMatrix(const QDomElement &element, VmixInput &input)
{
	QDomDocument positions;
	if (!positions.setContent(element.attribute(QStringLiteral("Positions")))) {
		return;
	}

	QDomElement first;
	for (QDomElement position = positions.documentElement().firstChildElement(QStringLiteral("MatrixPosition"));
	     !position.isNull(); position = position.nextSiblingElement(QStringLiteral("MatrixPosition"))) {
		input.matrixCount++;
		if (first.isNull()) {
			first = position;
		}
	}

	input.hasGeometry = input.matrixCount > 0;
	if (!first.isNull()) {
		takeMatrix(first, input);
		input.matrixMoved = !isIdentity(first);
	}
}

bool readVmixPreset(const std::string &xml, VmixPresetResult &result, std::string &error)
{
	result = VmixPresetResult();
	error.clear();

	QDomDocument document;
	const QDomDocument::ParseResult parsed = document.setContent(QByteArray::fromStdString(xml));
	if (!parsed) {
		error = QStringLiteral("not XML: %1 (line %2, column %3)")
				.arg(parsed.errorMessage)
				.arg(parsed.errorLine)
				.arg(parsed.errorColumn)
				.toStdString();
		return false;
	}

	const QDomElement root = document.documentElement();
	if (root.isNull() || root.tagName() != QStringLiteral("XML")) {
		error = "not a vMix preset: the root element is not <XML>";
		return false;
	}

	int number = 0;
	for (QDomElement element = root.firstChildElement(QStringLiteral("Input")); !element.isNull();
	     element = element.nextSiblingElement(QStringLiteral("Input"))) {
		number++;

		VmixInput input;
		input.number = number; /* the order it sits in the file is its number in the bar */
		input.title = element.attribute(QStringLiteral("Title")).toStdString();
		input.originalTitle = element.attribute(QStringLiteral("OriginalTitle")).toStdString();
		/* A Type that is not a number is not Type 0 (Video) by accident (V7): it
		 * falls to -1, which kindOfType() reports as Unknown. */
		bool typed = false;
		const int type = element.attribute(QStringLiteral("Type")).toInt(&typed);
		input.type = typed ? type : -1;
		input.kind = kindOfType(input.type);
		if (element.hasAttribute(QStringLiteral("VolumeF"))) {
			input.volume = element.attribute(QStringLiteral("VolumeF")).toDouble();
		}
		input.muted = element.attribute(QStringLiteral("Muted"))
				      .compare(QStringLiteral("True"), Qt::CaseInsensitive) == 0;
		/* The input's placement, read raw; Task 2 turns it into the base's geometry. */
		readMatrix(element, input);

		switch (input.kind) {
		case VmixKind::Video:
		case VmixKind::Image:
		case VmixKind::AudioFile:
		case VmixKind::Media:
			/* A single file's name is in OriginalTitle; the preset records no path for it. */
			input.file = input.originalTitle;
			break;
		case VmixKind::Capture:
			input.device = input.originalTitle;
			break;
		case VmixKind::AudioIn: {
			const QString path = element.attribute(QStringLiteral("AudioPath"));
			input.device = path.isEmpty() ? input.originalTitle : path.toStdString();
			break;
		}
		case VmixKind::NDI: {
			const QString name = element.attribute(QStringLiteral("NDISourceName"));
			input.device = name.isEmpty() ? input.originalTitle : name.toStdString();
			break;
		}
		case VmixKind::Colour:
			input.colour = element.attribute(QStringLiteral("Colour")).toStdString();
			break;
		default:
			break;
		}

		/* V7: a Type the spec cannot name makes no entry at all — it is read from
		 * OriginalTitle and put on the report, never guessed. */
		if (input.kind == VmixKind::Unknown) {
			VmixReportRow row;
			row.input = input.originalTitle.empty() ? input.title : input.originalTitle;
			row.had = "vMix input type " + std::to_string(input.type);
			row.why = "the desk cannot name this input type";
			result.report.push_back(row);
			continue;
		}

		/* Everything the desk cannot back is reported, never dropped silently (V4). */
		if (input.kind == VmixKind::Skipped) {
			VmixReportRow row;
			row.input = input.title.empty() ? input.originalTitle : input.title;
			row.had = typeName(input.type);
			row.why = "no desk equivalent";
			result.report.push_back(row);
		}

		result.inputs.push_back(input);
	}

	/* The show's on-air state: the shot it was left on, so an import can come up on it. */
	const QDomElement state = root.firstChildElement(QStringLiteral("State"));
	if (!state.isNull()) {
		result.active = state.attribute(QStringLiteral("Main")).toInt();
		result.preview = state.attribute(QStringLiteral("Preview")).toInt();
	}

	/* The output frame the matrices are relative to, so Task 2 can turn a matrix
	 * into a shot. A preset that names none leaves both at 0 (a shot it cannot
	 * compute is reported, not guessed). */
	const QDomElement format = root.firstChildElement(QStringLiteral("OutputFormat"));
	if (!format.isNull()) {
		const QString size = format.attribute(QStringLiteral("OutputSize"));
		const int x = size.indexOf(QLatin1Char('x'));
		bool wide = false;
		bool tall = false;
		if (x > 0) {
			result.outputWidth = size.left(x).toInt(&wide);
			result.outputHeight = size.mid(x + 1).toInt(&tall);
		}
		if (!wide) {
			result.outputWidth = 0;
		}
		if (!tall) {
			result.outputHeight = 0;
		}
	}

	/* The overlays 1-4 and the transitions the desk cannot carry (§5.2). Presence
	 * only: vMix writes an <OverlayN> slot value (nonzero when the slot is used) per
	 * output, and names its transition files under a Transitions directory. The
	 * routing itself is not read, so the report names the section, not which input
	 * sat on it — the shape is private (V7), and a guess would be a wrong shot. */
	for (int slot = 1; slot <= 4 && !result.overlays; slot++) {
		const QDomNodeList nodes = document.elementsByTagName(QStringLiteral("Overlay%1").arg(slot));
		for (int i = 0; i < nodes.count(); i++) {
			bool used = false;
			if (nodes.at(i).toElement().text().toInt(&used) != 0 && used) {
				result.overlays = true;
				break;
			}
		}
	}
	result.transitions = xml.find("Transitions\\") != std::string::npos ||
			     xml.find("Transitions/") != std::string::npos;

	return true;
}

void limitVmixInputs(VmixPresetResult &result, int limit)
{
	if (limit <= 0 || static_cast<int>(result.inputs.size()) <= limit) {
		return;
	}

	const std::size_t keep = static_cast<std::size_t>(limit);
	for (std::size_t i = keep; i < result.inputs.size(); i++) {
		const VmixInput &input = result.inputs[i];
		VmixReportRow row;
		row.input = input.title.empty() ? input.originalTitle : input.title;
		row.had = "input " + std::to_string(input.number);
		row.why = "the desk's bar holds " + std::to_string(limit);
		result.report.push_back(row);
	}

	result.inputs.resize(keep);
}

/* The building half's pure part (V4/V5). A kind the desk can back gets its OBS
 * type; a kind with no desk input — a Skipped entry (Task 1 already reported it),
 * or an Unknown one (Task 1 carries none) — returns null and makes no input. None
 * of the owner's presets holds a Browser or a Desktop Capture, so those two have
 * no evidenced Type number and fall to Unknown at read; they are mapped here
 * anyway, for the day a preset carries one. */
const char *vmixBaseId(VmixKind kind)
{
	switch (kind) {
	case VmixKind::Colour:
		return "color_source";
	case VmixKind::Image:
		return "image_source";
	case VmixKind::Video:
	case VmixKind::AudioFile:
	case VmixKind::Media:
		return "ffmpeg_source";
	case VmixKind::Capture:
		return "dshow_input";
	case VmixKind::AudioIn:
		return "wasapi_input_capture";
	case VmixKind::NDI:
		return "ndi_source";
	case VmixKind::Browser:
		return "browser_source";
	case VmixKind::DesktopCapture:
		return "monitor_capture";
	case VmixKind::Title:
		/* §10: a vMix title imports as a plain text source, the rest reported. */
		return "text_ft2_source";
	case VmixKind::Unknown:
	case VmixKind::Skipped:
		return nullptr;
	}
	return nullptr;
}

std::vector<VmixBuildStep> planVmixBuild(VmixPresetResult &result, int limit)
{
	/* The bar's own rule first (V5), so the over-limit rows precede the build's
	 * and the steps are exactly what is left to make. */
	limitVmixInputs(result, limit);

	std::vector<VmixBuildStep> steps;
	steps.reserve(result.inputs.size());
	for (const VmixInput &input : result.inputs) {
		const char *baseId = vmixBaseId(input.kind);
		if (!baseId) {
			continue; /* no desk base: no input, and the read's row still stands */
		}
		steps.push_back({&input, baseId});
	}

	/* The preset-wide rows for the two sections the desk has no home for (§5.2):
	 * reported, not applied. Only when the preset holds them, so a clean import
	 * still says the whole preset came across (§5.3). Added once: planVmixBuild is
	 * pure and may be called twice, and the rows must not double. */
	auto hasRow = [&result](const char *had) {
		for (const VmixReportRow &row : result.report) {
			if (row.input.empty() && row.had == had) {
				return true;
			}
		}
		return false;
	};
	if (!result.inputs.empty() && result.overlays && !hasRow("vMix overlays 1-4")) {
		result.report.push_back(
			{"", "vMix overlays 1-4",
			 "not applied: the desk has no overlay bus; the inputs are imported, the routing is not carried"});
	}
	if (!result.inputs.empty() && result.transitions && !hasRow("vMix transitions")) {
		result.report.push_back({"", "vMix transitions", "not applied: the desk uses OBS's transitions"});
	}

	return steps;
}

bool vmixPlacement(const VmixInput &input, double frameW, double frameH, double srcW, double srcH, VmixPlacement &out)
{
	if (frameW <= 0 || frameH <= 0 || srcW <= 0 || srcH <= 0) {
		return false;
	}

	/* vMix fits the whole source into the frame at Zoom 1, then scales by Zoom: the
	 * fit is whichever of the two axes is tighter, so nothing is cropped. A stored
	 * Zoom of 0 is vMix's "unset", which it coerces to 1; match that, or the source
	 * would vanish. */
	const double zoomX = input.zoomX == 0 ? 1.0 : input.zoomX;
	const double zoomY = input.zoomY == 0 ? 1.0 : input.zoomY;
	const double fit = std::min(frameW / srcW, frameH / srcH);
	const double onFrameW = srcW * fit * zoomX;
	const double onFrameH = srcH * fit * zoomY;

	VmixPlacement placed;
	placed.scaleX = fit * zoomX;
	placed.scaleY = fit * zoomY;
	/* centred at zoom, then panned: vMix's Pan is a fraction of the frame (X of its
	 * width, Y of its height) and, like the desk, reads Y downwards. */
	placed.x = (frameW - onFrameW) / 2.0 + input.panX * frameW;
	placed.y = (frameH - onFrameH) / 2.0 + input.panY * frameH;
	placed.rot = input.rotateZ;

	out = placed;
	return true;
}

} // namespace desk
