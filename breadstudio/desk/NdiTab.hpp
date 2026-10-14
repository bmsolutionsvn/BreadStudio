#pragma once

#include "InputPage.hpp"

#include <QTimer>

class QCheckBox;
class QLabel;
class QLineEdit;

namespace desk {

class SourceTiles;

/* The NDI tab (spec §4.1): the senders found on the network, vMix's three checkboxes, Also search, and
 * why NDI is off when it is. Everything comes from the NDI plugin's settings and procedures (§6.1). */
class NdiTab : public InputPage {
	Q_OBJECT

public:
	static constexpr const char *TypeId = "ndi_source";

	/* for a new NDI input */
	explicit NdiTab(QWidget *parent);

	/* for the base of an NDI input that exists */
	NdiTab(obs_source_t *base, QWidget *parent);

	bool ready() const override;
	OBSSource create() override;
	void apply() override;

private:
	void build();
	void refresh();
	OBSDataAutoRelease settings() const;

	OBSSource base; /* null for a new input */
	SourceTiles *tiles = nullptr;
	QLabel *status = nullptr;
	QCheckBox *lowBandwidth = nullptr;
	QCheckBox *audioOnly = nullptr;
	QCheckBox *bigBuffer = nullptr;
	QLineEdit *alsoSearch = nullptr;
	QTimer timer;
};

} // namespace desk
