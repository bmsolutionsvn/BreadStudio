#include "GtDocument.hpp"

#include <algorithm>
#include <cctype>

#include <QDomDocument>
#include <QDomElement>
#include <QString>
#include <QStringList>

namespace gt {

namespace {

/* what the walk collects across a composition (design §3.2) */
struct ParseContext {
	bool hasStoryboards = false;
	std::vector<std::string> unknownKinds;
};

float toFloat(const QString &text, float fallback)
{
	bool ok = false;
	const float value = text.trimmed().toFloat(&ok);
	return ok ? value : fallback;
}

/* "x,y,z" into three floats; an empty string leaves every default in place. */
void parseVec3(const QString &text, float dx, float dy, float dz, float &x, float &y, float &z)
{
	x = dx;
	y = dy;
	z = dz;
	const QString trimmed = text.trimmed();
	if (trimmed.isEmpty()) {
		return;
	}
	const QStringList parts = trimmed.split(',');
	if (!parts[0].trimmed().isEmpty()) {
		x = parts[0].toFloat();
	}
	if (parts.size() > 1 && !parts[1].trimmed().isEmpty()) {
		y = parts[1].toFloat();
	}
	if (parts.size() > 2 && !parts[2].trimmed().isEmpty()) {
		z = parts[2].toFloat();
	}
}

/* "x,y" into two floats; an absent or empty string leaves both as they are. */
void parseVec2(const QString &text, float &x, float &y)
{
	const QString trimmed = text.trimmed();
	if (trimmed.isEmpty()) {
		return;
	}
	const QStringList parts = trimmed.split(',');
	if (!parts[0].trimmed().isEmpty()) {
		x = parts[0].toFloat();
	}
	if (parts.size() > 1 && !parts[1].trimmed().isEmpty()) {
		y = parts[1].toFloat();
	}
}

/* a comma or space separated list, as DataFlags and TextEffect are written */
QStringList tokens(const QString &text)
{
	QString spaced = text;
	spaced.replace(',', ' ');
	return spaced.split(' ', Qt::SkipEmptyParts);
}

/* A property is an attribute, or a <Kind.Property> child; both carry the same name (§3.2). */
bool hasProp(const QDomElement &element, const QString &kind, const QString &name)
{
	return element.hasAttribute(name) || !element.firstChildElement(kind + "." + name).isNull();
}

QString prop(const QDomElement &element, const QString &kind, const QString &name)
{
	if (element.hasAttribute(name)) {
		return element.attribute(name);
	}
	const QDomElement child = element.firstChildElement(kind + "." + name);
	return child.isNull() ? QString() : child.text().trimmed();
}

/* "#AARRGGBB" or "#RRGGBB"; the leading # is required. */
bool parseColourText(const QString &text, uint32_t &argb)
{
	QString hex = text.trimmed();
	if (!hex.startsWith('#')) {
		return false;
	}
	hex = hex.mid(1);
	if (hex.size() != 6 && hex.size() != 8) {
		return false;
	}
	bool ok = false;
	const uint value = hex.toUInt(&ok, 16);
	if (!ok) {
		return false;
	}
	argb = hex.size() == 6 ? (0xFF000000u | value) : value;
	return true;
}

/* A Brush element: a colour, or a linear gradient with StartPoint, EndPoint and stops (§3.2). */
GtBrush parseBrush(const QDomElement &element)
{
	GtBrush brush;
	if (element.isNull()) {
		return brush;
	}

	const QString color = element.attribute("Color");
	if (!color.isEmpty()) {
		parseColourText(color, brush.argb);
	}

	const QDomElement stops = element.firstChildElement("Brush.Stops");
	if (!stops.isNull()) {
		for (QDomElement stop = stops.firstChildElement("GradientStop"); !stop.isNull();
		     stop = stop.nextSiblingElement("GradientStop")) {
			const float position = toFloat(stop.attribute("Position"), 0.0f);
			uint32_t argb = 0;
			parseColourText(stop.attribute("Color"), argb);
			brush.stops.emplace_back(position, argb);
		}
	}

	/* Type="Transparent" wins; points or stops make it a gradient; a lone colour is solid */
	const QString type = element.attribute("Type");
	if (type.compare("Transparent", Qt::CaseInsensitive) == 0) {
		brush.type = GtBrushType::Transparent;
	} else if (element.hasAttribute("StartPoint") || element.hasAttribute("EndPoint") || !brush.stops.empty()) {
		brush.type = GtBrushType::Linear;
	} else if (!color.isEmpty()) {
		brush.type = GtBrushType::Solid;
	}

	if (element.hasAttribute("StartPoint")) {
		parseVec2(element.attribute("StartPoint"), brush.startX, brush.startY);
	}
	if (element.hasAttribute("EndPoint")) {
		parseVec2(element.attribute("EndPoint"), brush.endX, brush.endY);
	}
	return brush;
}

GtObject parseObject(const QDomElement &element, ParseContext &context);
void parseComposition(const QDomElement &composition, ParseContext &context, std::vector<GtObject> &out);

/* The elements directly in a composition; a Storyboard is noted, any other element is pushed by
 * its kind and its kind is remembered once as one the renderer does not know (§3.4). */
void parseComposition(const QDomElement &composition, ParseContext &context, std::vector<GtObject> &out)
{
	for (QDomElement child = composition.firstChildElement(); !child.isNull(); child = child.nextSiblingElement()) {
		const QString tag = child.tagName();
		if (tag == "Storyboard") {
			context.hasStoryboards = true;
			continue;
		}
		GtObject object = parseObject(child, context);
		const bool known = tag == "Layer" || tag == "TextBlock" || tag == "Text3D" || tag == "Image" ||
				   tag == "Rectangle" || tag == "Ellipse";
		if (!known && std::find(context.unknownKinds.begin(), context.unknownKinds.end(), object.kind) ==
				      context.unknownKinds.end()) {
			context.unknownKinds.push_back(object.kind);
		}
		out.push_back(std::move(object));
	}
}

GtObject parseObject(const QDomElement &element, ParseContext &context)
{
	GtObject object;
	const QString kind = element.tagName();
	object.kind = kind.toStdString();
	object.name = element.attribute("Name").toStdString();

	float locationZ = 0;
	parseVec3(element.attribute("Location"), 0, 0, 0, object.x, object.y, locationZ);
	parseVec3(element.attribute("Dimensions"), 100, 100, 0, object.width, object.height, object.depth);

	if (hasProp(element, kind, "Visible")) {
		object.visible = prop(element, kind, "Visible").compare("false", Qt::CaseInsensitive) != 0;
	}
	if (hasProp(element, kind, "Opacity")) {
		object.opacity = toFloat(prop(element, kind, "Opacity"), 1.0f);
	}
	if (hasProp(element, kind, "DataFlags")) {
		object.dataFlagsSet = true;
		for (const QString &token : tokens(prop(element, kind, "DataFlags"))) {
			if (token.compare("Hidden", Qt::CaseInsensitive) == 0) {
				object.dataFlags |= 1u;
			} else if (token.compare("NoEvents", Qt::CaseInsensitive) == 0) {
				object.dataFlags |= 2u;
			} else if (token.compare("ShowVisible", Qt::CaseInsensitive) == 0) {
				object.dataFlags |= 4u;
			}
		}
	}

	const QDomElement maskProp = element.firstChildElement(kind + ".Mask");
	if (!maskProp.isNull()) {
		const QDomElement mask = maskProp.firstChildElement("Mask");
		if (!mask.isNull()) {
			object.mask = mask.attribute("Object").toStdString();
		}
	}

	const QDomElement effectsProp = element.firstChildElement(kind + ".Effects");
	if (!effectsProp.isNull()) {
		for (QDomElement effectElement = effectsProp.firstChildElement("Effect"); !effectElement.isNull();
		     effectElement = effectElement.nextSiblingElement("Effect")) {
			GtEffect effect;
			effect.type = effectElement.attribute("Type").toStdString();
			parseVec2(effectElement.attribute("Offset"), effect.offsetX, effect.offsetY);
			effect.blur = toFloat(effectElement.attribute("BlurAmount"), 0.0f);
			effect.angle = toFloat(effectElement.attribute("Angle"), 0.0f);
			uint32_t argb = effect.argb;
			if (parseColourText(effectElement.attribute("Color"), argb)) {
				effect.argb = argb;
			}
			effect.mode = effectElement.attribute("Mode").toStdString();
			object.effects.push_back(std::move(effect));
		}
	}

	/* text */
	if (hasProp(element, kind, "Text")) {
		object.text = prop(element, kind, "Text").toStdString();
	}
	if (hasProp(element, kind, "FontFamily")) {
		object.fontFamily = prop(element, kind, "FontFamily").toStdString();
	}
	if (hasProp(element, kind, "FontSize")) {
		object.fontSize = toFloat(prop(element, kind, "FontSize"), 36.0f);
	}
	if (hasProp(element, kind, "FontWeight")) {
		object.fontWeight = gtFontWeight(prop(element, kind, "FontWeight").toStdString());
	}
	if (hasProp(element, kind, "FontStyle")) {
		object.italic = prop(element, kind, "FontStyle").compare("italic", Qt::CaseInsensitive) == 0;
	}
	if (hasProp(element, kind, "FontStretch")) {
		object.fontStretch = prop(element, kind, "FontStretch").toStdString();
	}
	if (hasProp(element, kind, "TextAlign")) {
		object.textAlign = prop(element, kind, "TextAlign").toStdString();
	}
	if (hasProp(element, kind, "VerticalAlign")) {
		object.verticalAlign = prop(element, kind, "VerticalAlign").toStdString();
	}
	if (hasProp(element, kind, "TextWordWrapping")) {
		object.wrapping = prop(element, kind, "TextWordWrapping").toStdString();
	}
	if (hasProp(element, kind, "LineSpacing")) {
		object.lineSpacing = toFloat(prop(element, kind, "LineSpacing"), 0.0f);
	}
	if (hasProp(element, kind, "AutoSize")) {
		object.autoSize = prop(element, kind, "AutoSize").toStdString();
	}
	if (hasProp(element, kind, "StrokeThickness")) {
		object.strokeThickness = toFloat(prop(element, kind, "StrokeThickness"), 0.0f);
	}
	if (hasProp(element, kind, "TextEffect")) {
		for (const QString &token : tokens(prop(element, kind, "TextEffect"))) {
			if (token.compare("Uppercase", Qt::CaseInsensitive) == 0) {
				object.uppercase = true;
			} else if (token.compare("SingleLine", Qt::CaseInsensitive) == 0) {
				object.singleLine = true;
			}
		}
	}

	const QDomElement fillProp = element.firstChildElement(kind + ".Fill");
	if (!fillProp.isNull()) {
		object.fill = parseBrush(fillProp.firstChildElement("Brush"));
	}
	const QDomElement strokeProp = element.firstChildElement(kind + ".Stroke");
	if (!strokeProp.isNull()) {
		object.stroke = parseBrush(strokeProp.firstChildElement("Brush"));
	}

	/* image */
	const QDomElement bitmapProp = element.firstChildElement(kind + ".Bitmap");
	if (!bitmapProp.isNull()) {
		const QDomElement bitmap = bitmapProp.firstChildElement("Bitmap");
		if (!bitmap.isNull()) {
			object.bitmapSource = bitmap.attribute("Source").toStdString();
		}
	}
	if (hasProp(element, kind, "SizeMode")) {
		object.sizeMode = prop(element, kind, "SizeMode").toStdString();
	}

	/* rectangle */
	if (hasProp(element, kind, "Radius")) {
		parseVec2(prop(element, kind, "Radius"), object.radiusX, object.radiusY);
	}

	/* layer: its own composition is read recursively into the children */
	const QDomElement compositionProp = element.firstChildElement(kind + ".Composition");
	if (!compositionProp.isNull()) {
		const QDomElement composition = compositionProp.firstChildElement("Composition");
		if (!composition.isNull()) {
			object.compWidth = toFloat(composition.attribute("Width"), 0.0f);
			object.compHeight = toFloat(composition.attribute("Height"), 0.0f);
			parseComposition(composition, context, object.children);
		}
	}
	return object;
}

} // namespace

int gtFontWeight(const std::string &name)
{
	std::string lower;
	lower.reserve(name.size());
	for (char c : name) {
		lower.push_back((char)std::tolower((unsigned char)c));
	}
	if (lower == "thin") {
		return 100;
	}
	if (lower == "extralight" || lower == "ultralight") {
		return 200;
	}
	if (lower == "light") {
		return 300;
	}
	if (lower == "normal" || lower == "regular") {
		return 400;
	}
	if (lower == "medium") {
		return 500;
	}
	if (lower == "demibold" || lower == "semibold") {
		return 600;
	}
	if (lower == "bold") {
		return 700;
	}
	if (lower == "extrabold" || lower == "ultrabold") {
		return 800;
	}
	if (lower == "black" || lower == "heavy") {
		return 900;
	}
	if (lower == "extrablack") {
		return 950;
	}
	return 400;
}

bool gtParseColour(const std::string &text, uint32_t &argb)
{
	return parseColourText(QString::fromStdString(text), argb);
}

bool parseGtDocument(const std::string &utf8Xml, GtDocument &out, std::string &error)
{
	out = GtDocument();
	error.clear();

	QDomDocument document;
	/* Passing a QString makes Qt ignore the utf-16 declaration the owner's files carry, so the
	 * UTF-8 bytes they actually hold are read as written (§3.1). Passing the bytes would misdecode. */
	const QDomDocument::ParseResult result =
		document.setContent(QString::fromUtf8(utf8Xml.c_str(), (int)utf8Xml.size()),
				    QDomDocument::ParseOption::Default);
	if (!result) {
		error = result.errorMessage.isEmpty() ? "document.xml is not well-formed"
						     : result.errorMessage.toStdString();
		return false;
	}

	const QDomElement root = document.documentElement();
	if (root.isNull() || root.tagName() != "Composition") {
		error = "document.xml has no Composition root";
		return false;
	}

	out.width = root.hasAttribute("Width") ? toFloat(root.attribute("Width"), 1920.0f) : 1920.0f;
	out.height = root.hasAttribute("Height") ? toFloat(root.attribute("Height"), 1080.0f) : 1080.0f;

	ParseContext context;
	parseComposition(root, context, out.objects);
	out.hasStoryboards = context.hasStoryboards;
	out.unknownKinds = context.unknownKinds;
	return true;
}

} // namespace gt
