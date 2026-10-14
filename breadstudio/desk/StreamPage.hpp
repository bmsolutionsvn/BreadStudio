#pragma once

#include "InputPage.hpp"
#include "StreamAddress.hpp"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace desk {

/* The Stream / SRT page: a network stream played by OBS's Media Source (spec §5). */
class StreamPage : public InputPage {
	Q_OBJECT

public:
	/* for a new stream input */
	explicit StreamPage(QWidget *parent);

	bool ready() const override;
	OBSSource create() override;

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
