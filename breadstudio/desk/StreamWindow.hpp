#pragma once

#include "OutputPlan.hpp"

#include <QDialog>
#include <QTimer>

#include <array>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

namespace desk {

/* The three RTMP destinations (outputs spec §3.2). OK applies; a live destination whose settings
 * did not change stays live. */
class StreamWindow : public QDialog {
	Q_OBJECT

public:
	explicit StreamWindow(QWidget *parent);

	void accept() override;

private:
	struct Row {
		QCheckBox *on = nullptr;
		QComboBox *output = nullptr;
		QLineEdit *server = nullptr;
		QLineEdit *key = nullptr;
		QLabel *status = nullptr;
		uint64_t lastBytes = 0;
	};

	void updateStatus();

	std::array<Row, DestinationCount> rows;
	QTimer timer;
};

} // namespace desk
