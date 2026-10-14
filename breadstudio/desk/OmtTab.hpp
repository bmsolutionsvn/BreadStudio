#pragma once

#include "InputPage.hpp"

#include <QTimer>

#include <map>
#include <string>
#include <vector>

class QButtonGroup;
class QLabel;
class QLineEdit;
class QScrollArea;
class QToolButton;

namespace desk {

struct SenderGroup;

/* The OMT tab (spec §4.1 to §4.3). The sources and their formats come from the OMT plugin's
 * settings lists (§6.1), so the desk does not link omthx. */
class OmtTab : public InputPage {
	Q_OBJECT

public:
	static constexpr const char *TypeId = "omt_hx_source";

	/* for a new OMT input */
	explicit OmtTab(QWidget *parent);

	/* for the base of an OMT input that exists */
	OmtTab(obs_source_t *base, QWidget *parent);

	bool ready() const override;
	OBSSource create() override;
	void apply() override;

private:
	void build();
	void refresh();
	void rebuild(const std::vector<SenderGroup> &groups);
	std::string selected() const;

	OBSSource base;                 /* null for a new input */
	std::string chosen;             /* the address of the tile picked */
	std::vector<std::string> shown; /* the addresses the tiles were made for, in order */
	std::map<std::string, QToolButton *> tiles;

	QScrollArea *scroll = nullptr;
	QButtonGroup *buttons = nullptr;
	QLineEdit *byAddress = nullptr;
	QLabel *receiving = nullptr; /* for an input that exists */
	QTimer timer;
};

} // namespace desk
