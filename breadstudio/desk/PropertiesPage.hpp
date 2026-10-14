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

	/* for a source that exists: the base of an input */
	PropertiesPage(obs_source_t *existing, QWidget *parent);

	OBSSource create() override;
	void revert() override;

private:
	void build();

	std::string typeId;
	std::string nameKey;
	/* for a new input: a private stand-in of the real type, never saved, never on air.
	 * For an existing one: the source itself. */
	OBSSource source;
	OBSData before; /* an existing source's settings when the page opened; null for a new input */
};

} // namespace desk
