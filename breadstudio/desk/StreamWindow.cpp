#include "StreamWindow.hpp"
#include "Desk.hpp"
#include "OutputEngine.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace desk {

StreamWindow::StreamWindow(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(text("Stream.Title"));
	auto *layout = new QVBoxLayout(this);
	auto *grid = new QGridLayout();
	layout->addLayout(grid);

	const char *headings[] = {"Stream.On", "Outputs.Output", "Stream.Server", "Stream.Key", "Outputs.Status"};
	for (int column = 0; column < 5; column++) {
		grid->addWidget(new QLabel(text(headings[column]), this), 0, column);
	}

	for (int d = 0; d < DestinationCount; d++) {
		const Destination &destination = OutputEngine::instance().config().destinations[(size_t)d];
		Row &row = rows[(size_t)d];
		row.on = new QCheckBox(this);
		row.on->setChecked(destination.on);
		row.output = new QComboBox(this);
		for (int i = 0; i < OutputCount; i++) {
			row.output->addItem(text("Outputs.Number").arg(i + 1));
		}
		row.output->setCurrentIndex(destination.output);
		row.server = new QLineEdit(QString::fromStdString(destination.server), this);
		row.server->setPlaceholderText("rtmp://server/app");
		row.server->setMinimumWidth(280);
		row.key = new QLineEdit(QString::fromStdString(destination.key), this);
		row.key->setEchoMode(QLineEdit::PasswordEchoOnEdit);
		row.status = new QLabel(this);
		row.status->setMinimumWidth(200);

		grid->addWidget(row.on, d + 1, 0);
		grid->addWidget(row.output, d + 1, 1);
		grid->addWidget(row.server, d + 1, 2);
		grid->addWidget(row.key, d + 1, 3);
		grid->addWidget(row.status, d + 1, 4);
	}

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &StreamWindow::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

	connect(&timer, &QTimer::timeout, this, &StreamWindow::updateStatus);
	timer.start(500);
	for (int d = 0; d < DestinationCount; d++) {
		rows[(size_t)d].lastBytes = OutputEngine::instance().destinationStatus(d).bytes;
	}
	updateStatus();
}

void StreamWindow::accept()
{
	const OutputsConfig before = OutputEngine::instance().config();
	OutputsConfig config = before;
	for (int d = 0; d < DestinationCount; d++) {
		const Row &row = rows[(size_t)d];
		Destination &destination = config.destinations[(size_t)d];
		const QString server = row.server->text().trimmed();
		/* a destination without a server cannot be on */
		destination.on = row.on->isChecked() && !server.isEmpty();
		destination.output = row.output->currentIndex();
		destination.server = server.toStdString();
		destination.key = row.key->text().trimmed().toStdString();
	}
	OutputEngine::instance().apply(config);
	/* only a real change marks the Project; opening the window and pressing OK unchanged does
	 * not (P9) */
	if (!(config.outputs == before.outputs && config.destinations == before.destinations)) {
		markProjectChanged();
	}
	QDialog::accept();
}

void StreamWindow::updateStatus()
{
	for (int d = 0; d < DestinationCount; d++) {
		Row &row = rows[(size_t)d];
		const SenderStatus status = OutputEngine::instance().destinationStatus(d);
		QString line;
		switch (status.state) {
		case SenderState::Live: {
			/* the window polls twice a second */
			const uint64_t kbps = (status.bytes - row.lastBytes) * 8 * 2 / 1000;
			line = text("Stream.State.Live").arg(kbps).arg(status.dropped);
			break;
		}
		case SenderState::Retrying:
			line = text("Outputs.State.Retrying");
			break;
		case SenderState::Failed:
			line = text("Outputs.State.Failed");
			break;
		case SenderState::Waiting:
			line = text("Stream.State.Starting");
			break;
		default:
			line = text("Outputs.State.Off");
			break;
		}
		row.lastBytes = status.bytes;
		row.status->setText(line);
		row.status->setStyleSheet(status.state == SenderState::Retrying || status.state == SenderState::Failed
						  ? "color: #d00000;"
						  : "");
	}
}

} // namespace desk
