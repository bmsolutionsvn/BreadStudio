#include "GtFields.hpp"

#include <cstdio>

namespace gt {

namespace {

/* "#AARRGGBB" of a brush's colour (§4.3). */
std::string colourText(uint32_t argb)
{
	char buffer[11];
	std::snprintf(buffer, sizeof(buffer), "#%08X", argb);
	return buffer;
}

/* Hidden=1; a field's DataFlags may carry other flags too (§3.2). */
bool hidden(const GtObject &object)
{
	return (object.dataFlags & 1u) != 0;
}

} // namespace

std::vector<GtField> gtFields(const GtDocument &doc)
{
	std::vector<GtField> fields;
	/* Only a top-level Layer's own children are fields; deeper objects are not (§4.3). */
	for (const GtObject &object : doc.objects) {
		if (object.kind != "Layer") {
			continue;
		}
		for (const GtObject &child : object.children) {
			if (child.kind == "TextBlock" || child.kind == "Text3D") {
				if (!hidden(child)) {
					fields.push_back({child.name + ".Text", GtFieldType::Text, child.text, &child});
				}
			} else if (child.kind == "Image") {
				if (!hidden(child)) {
					fields.push_back({child.name + ".Source", GtFieldType::Image, child.bitmapSource, &child});
				}
			} else if (child.kind == "Rectangle" || child.kind == "Ellipse") {
				/* Shapes default to Hidden, so a field needs the flags set and Hidden clear (§4.3). */
				if (child.dataFlagsSet && !hidden(child)) {
					fields.push_back({child.name + ".Fill.Color", GtFieldType::Colour,
							  colourText(child.fill.argb), &child});
				}
			}
		}
	}
	return fields;
}

} // namespace gt
