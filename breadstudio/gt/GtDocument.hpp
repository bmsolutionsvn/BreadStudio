#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace gt {

/* How a brush paints: nothing, one colour, a linear gradient, or fully transparent (design §3.2). */
enum class GtBrushType { None, Solid, Linear, Transparent };

/* A fill or a stroke; a linear gradient's points are relative to the object's bounds (0–1). */
struct GtBrush {
	GtBrushType type = GtBrushType::None;
	uint32_t argb = 0;
	float startX = 0, startY = 0, endX = 0, endY = 1;
	std::vector<std::pair<float, uint32_t>> stops;
};

/* Shadow, Skew and the rest; only Shadow and Skew are drawn (§3.4). */
struct GtEffect {
	std::string type;
	float offsetX = 0, offsetY = 0, blur = 0, angle = 0;
	uint32_t argb = 0xFF000000;
	std::string mode;
};

/* A field's style override (§4.4); an unset member is the file's own value. */
struct GtStyle {
	std::optional<std::string> font;
	std::optional<float> size;
	std::optional<uint32_t> fill;
	std::optional<int> weight;
	std::optional<bool> italic;
	std::optional<std::string> stretch;
	std::optional<float> lineSpacing;
};

/* One element of the composition; members a kind does not use keep their defaults. */
struct GtObject {
	std::string kind;
	std::string name;
	float x = 0, y = 0, width = 100, height = 100, depth = 0;
	bool visible = true;
	float opacity = 1;
	unsigned dataFlags = 0;
	bool dataFlagsSet = false; /* Hidden=1 NoEvents=2 ShowVisible=4 */
	std::string mask;          /* Mask Object, "" for none */
	std::vector<GtEffect> effects;
	/* text */
	std::string text = "Text", fontFamily = "Arial";
	float fontSize = 36;
	int fontWeight = 400;
	bool italic = false;
	std::string fontStretch = "Normal", textAlign = "Left", verticalAlign = "Top", wrapping = "Wrap",
		    autoSize = "Fixed";
	float lineSpacing = 0;
	bool uppercase = false, singleLine = false;
	GtBrush fill, stroke;
	float strokeThickness = 0;
	/* image */
	std::string bitmapSource, sizeMode = "Centered";
	/* rectangle */
	float radiusX = 0, radiusY = 0;
	/* layer: its composition */
	float compWidth = 0, compHeight = 0;
	std::vector<GtObject> children;
};

/* A parsed document.xml (§3.2). */
struct GtDocument {
	float width = 1920, height = 1080;
	std::vector<GtObject> objects;
	bool hasStoryboards = false;
	std::vector<std::string> unknownKinds;
};

/* Parses document.xml, kept as UTF-8; the root composition's size and objects come back. */
bool parseGtDocument(const std::string &utf8Xml, GtDocument &out, std::string &error);

/* vMix's weight name -> 100..950; 400 when unknown (§3.2). */
int gtFontWeight(const std::string &name);

/* "#AARRGGBB" or "#RRGGBB" -> ARGB; false when it is neither. */
bool gtParseColour(const std::string &text, uint32_t &argb);

} // namespace gt
