#pragma once

#include "InputPage.hpp"
#include "StreamAddress.hpp"

#include <QTimer>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

namespace desk {

/* The Stream / SRT page: a network stream played by OBS's Media Source (spec §5). */
class StreamPage : public InputPage {
	Q_OBJECT

public:
	/* for a new stream input */
	explicit StreamPage(QWidget *parent);

	/* for a stream input that exists */
	StreamPage(obs_source_t *base, QWidget *parent);

	bool ready() const override;
	OBSSource create() override;
	void apply() override;

	/* true when this source is the base of a stream input made on this page */
	static bool isStream(obs_source_t *base);

protected:
	void buildForm();
	StreamForm form() const;
	void setForm(const StreamForm &form);

	/* the Media Source settings for a form, and the form kept with the source */
	static OBSDataAutoRelease mediaSettings(const StreamForm &form);
	static void saveForm(obs_source_t *base, const StreamForm &form);
	static bool loadForm(obs_source_t *base, StreamForm &form);

	QFormLayout *fields = nullptr;

private:
	void typeChanged();
	void formChanged();
	void pollState();

	OBSSource base;    /* null for a new input */
	StreamForm before; /* the form when the page opened */
	QPlainTextEdit *status = nullptr;
	int lastState = -1;
	QTimer timer;

	QComboBox *type = nullptr;
	QLabel *hostLabel = nullptr;
	QLineEdit *host = nullptr;
	QSpinBox *port = nullptr;
	QSpinBox *latency = nullptr;
	QLineEdit *passphrase = nullptr;
	QComboBox *keyLength = nullptr;
	QLineEdit *streamId = nullptr;
	QCheckBox *hardware = nullptr;
	QLabel *problem = nullptr;
};

} // namespace desk
