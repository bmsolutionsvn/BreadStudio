#include "VmixPreset.hpp"

#include <QByteArray>
#include <QDomDocument>
#include <QDomElement>
#include <QString>
#include <QtGlobal>

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
}

/* vMix keeps an input's placement in the `Positions` attribute as a whole embedded
 * XML document — one MatrixPosition per output. Read it into the input raw: the
 * desk converts it in Task 2, where the source's own pixel size is known. A preset
 * with more than one output leaves matrixCount above one, so Task 2 knows no single
 * matrix is the desk's geometry. The matrix carried is the first output that
 * actually places the input, or the identity when none does. */
static void readMatrix(const QDomElement &element, VmixInput &input)
{
	QDomDocument positions;
	if (!positions.setContent(element.attribute(QStringLiteral("Positions")))) {
		return;
	}

	QDomElement first;
	QDomElement placed;
	for (QDomElement position = positions.documentElement().firstChildElement(QStringLiteral("MatrixPosition"));
	     !position.isNull(); position = position.nextSiblingElement(QStringLiteral("MatrixPosition"))) {
		input.matrixCount++;
		if (first.isNull()) {
			first = position;
		}
		if (placed.isNull() && !isIdentity(position)) {
			placed = position;
		}
	}

	input.hasGeometry = input.matrixCount > 0;
	if (!placed.isNull()) {
		takeMatrix(placed, input);
		input.matrixMoved = true;
	} else if (!first.isNull()) {
		takeMatrix(first, input);
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

} // namespace desk
