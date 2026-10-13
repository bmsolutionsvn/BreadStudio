#pragma once

#include "InputPage.hpp"

#include <string>

namespace desk {

/* A page that is OBS's own settings panel for one kind of source. */
class PropertiesPage : public InputPage {
	Q_OBJECT

public:
	/* for a new input of that type; `nameKey` is the locale key of the input's name */
	PropertiesPage(const char *unversionedId, const char *nameKey, QWidget *parent);

	OBSSource create() override;

private:
	void build();

	std::string typeId;
	std::string nameKey;
	OBSSource source; /* a private stand-in of the real type; never saved, never on air */
};

} // namespace desk
