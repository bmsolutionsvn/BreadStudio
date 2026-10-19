#pragma once

#include "InputPage.hpp"

#include <QPixmap>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;

namespace desk {

/* The Input Select Title page (gt-title design §4.1): choose a .gtzip, see its
 * thumbnail, OK. For a GT title that exists the page edits its file in place. */
class TitlePage : public InputPage {
	Q_OBJECT

public:
	/* for a new GT title input */
	explicit TitlePage(QWidget *parent);

	/* for the base of a GT title input that exists */
	TitlePage(obs_source_t *base, QWidget *parent);

	bool ready() const override;      /* a readable .gtzip is chosen */
	OBSSource create() override;
	void apply() override;
	void revert() override;

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	void build();
	void browse();
	void showFile();
	void scalePreview();

	OBSSource base;            /* null for a new input */
	QLineEdit *path = nullptr; /* the .gtzip path, read-only */
	QPushButton *browseButton = nullptr;
	QLabel *preview = nullptr; /* the thumbnail, or the read error in its place */
	QPixmap thumbnail;         /* the title's thumbnail, scaled to fit the page */
	bool readable = false;     /* true once the chosen path reads as a title */
	QString before;            /* the path when an existing input's page opened */
};

} // namespace desk
