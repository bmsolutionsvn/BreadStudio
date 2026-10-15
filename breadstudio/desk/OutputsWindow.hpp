#pragma once

#include "OutputPlan.hpp"

#include <QDialog>
#include <QTimer>

#include <array>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace desk {

/* The Outputs window: one row per output (outputs spec §3.3). OK applies; only what changed
 * restarts. */
class OutputsWindow : public QDialog {
	Q_OBJECT

public:
	explicit OutputsWindow(QWidget *parent);

	void accept() override;

private:
	struct Row {
		QComboBox *source = nullptr;
		QComboBox *size = nullptr;
		QCheckBox *alpha = nullptr;
		QComboBox *bus = nullptr;
		QCheckBox *ndi = nullptr;
		QComboBox *omt = nullptr;
		QCheckBox *srt = nullptr;
		QPushButton *srtSettings = nullptr;
		QComboBox *codec = nullptr;
		QSpinBox *bitrate = nullptr;
		QLabel *status = nullptr;
		StreamForm srtForm;
		bool red = false;
	};

	void addRow(int index);
	void syncRow(int index);
	OutputsConfig read() const;
	void updateStatus();

	std::array<Row, OutputCount> rows;
	QTimer timer;
};

} // namespace desk
