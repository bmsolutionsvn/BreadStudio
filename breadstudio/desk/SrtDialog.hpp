#pragma once

#include "StreamAddress.hpp"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace desk {

/* An output's SRT settings: the Stream / SRT page's SRT fields (outputs spec §3.4). */
class SrtDialog : public QDialog {
	Q_OBJECT

public:
	SrtDialog(const StreamForm &form, QWidget *parent);

	StreamForm form() const;

private:
	void check();

	QComboBox *mode = nullptr;
	QLineEdit *host = nullptr;
	QSpinBox *port = nullptr;
	QSpinBox *latency = nullptr;
	QLineEdit *passphrase = nullptr;
	QComboBox *keyLength = nullptr;
	QLineEdit *streamId = nullptr;
	QLabel *problem = nullptr;
	QPushButton *ok = nullptr;
};

} // namespace desk
