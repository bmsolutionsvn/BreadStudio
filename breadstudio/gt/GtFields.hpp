#pragma once

#include <string>
#include <vector>

#include "GtDocument.hpp"

namespace gt {

/* What a field holds (§4.3). */
enum class GtFieldType { Text, Image, Colour };

/* One editable field, named as vMix names it. The object stays valid for the document's life. */
struct GtField {
	std::string name;      /* NAME.Text, NAME.Source or NAME.Fill.Color */
	GtFieldType type;
	std::string defaultValue;
	const GtObject *object;
};

/* The fields the operator may change, in document order (§4.3). */
std::vector<GtField> gtFields(const GtDocument &doc);

} // namespace gt
