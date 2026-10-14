#include "OutputsWindow.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "OutputEngine.hpp"
#include "SrtDialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>
#include <string>

namespace desk {

static const char *NdiType = "bread_ndi_output";
static const char *OmtType = "bread_omt_output";
static const char *SrtType = "bread_srt_output";
static const char *MultiViewId = "bread_multiview";

/* "program", "preview", "multiview", or "input:<uuid>" */
static QString sourceKey(const OutputConfig &config)
{
	switch (config.source) {
	case OutputSource::Program:
		return "program";
	case OutputSource::Preview:
		return "preview";
	case OutputSource::MultiView:
		return "multiview";
	case OutputSource::Input:
		break;
	}
	return "input:" + QString::fromStdString(config.input);
}

static QString stateText(const SenderStatus &status, bool listener)
{
	switch (status.state) {
	case SenderState::Off:
		return text("Outputs.State.Off");
	case SenderState::Waiting:
		return text(listener ? "Outputs.State.Listening" : "Outputs.State.Calling");
	case SenderState::Connected:
		return text("Outputs.State.Connected");
	case SenderState::Retrying:
		return text("Outputs.State.Retrying");
	case SenderState::Live:
		return status.receivers >= 0 ? text("Outputs.State.Receivers").arg(status.receivers)
					     : text("Outputs.State.Live");
	case SenderState::Failed:
		return text("Outputs.State.Failed");
	}
	return QString();
}

OutputsWindow::OutputsWindow(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(text("Outputs.Title"));
	auto *layout = new QVBoxLayout(this);
	auto *grid = new QGridLayout();
	layout->addLayout(grid);

	const char *headings[] = {"Outputs.Output", "Outputs.Source",  "Outputs.Size",
				  "Outputs.Alpha",  "Outputs.Audio",   "Outputs.Ndi",
				  "Outputs.Omt",    "Outputs.Srt",     "",
				  "Outputs.Codec",  "Outputs.Bitrate", "Outputs.Status"};
	for (int column = 0; column < (int)std::size(headings); column++) {
		if (*headings[column]) {
			grid->addWidget(new QLabel(text(headings[column]), this), 0, column);
		}
	}

	for (int i = 0; i < OutputCount; i++) {
		addRow(i);
		Row &row = rows[(size_t)i];
		grid->addWidget(new QLabel(text("Outputs.Number").arg(i + 1), this), i + 1, 0);
		grid->addWidget(row.source, i + 1, 1);
		grid->addWidget(row.size, i + 1, 2);
		grid->addWidget(row.alpha, i + 1, 3);
		grid->addWidget(row.bus, i + 1, 4);
		grid->addWidget(row.ndi, i + 1, 5);
		grid->addWidget(row.omt, i + 1, 6);
		grid->addWidget(row.srt, i + 1, 7);
		grid->addWidget(row.srtSettings, i + 1, 8);
		grid->addWidget(row.codec, i + 1, 9);
		grid->addWidget(row.bitrate, i + 1, 10);
		grid->addWidget(row.status, i + 1, 11);
	}

	/* a column whose plugin is not in this build says why */
	auto missing = [this](const char *type, int column, QGridLayout *g) {
		if (!OutputEngine::hasOutputType(type)) {
			auto *note = new QLabel(text("Outputs.Missing"), this);
			g->addWidget(note, OutputCount + 1, column);
		}
	};
	missing(NdiType, 5, grid);
	missing(OmtType, 6, grid);
	missing(SrtType, 7, grid);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &OutputsWindow::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

	connect(&timer, &QTimer::timeout, this, &OutputsWindow::updateStatus);
	timer.start(500);
	updateStatus();
}

void OutputsWindow::addRow(int index)
{
	Row &row = rows[(size_t)index];
	const OutputConfig &config = OutputEngine::instance().config().outputs[(size_t)index];

	row.source = new QComboBox(this);
	row.source->addItem(text("Outputs.Program"), "program");
	if (index > 0) {
		row.source->addItem(text("Outputs.Preview"), "preview");
		if (obs_source_get_display_name(MultiViewId)) {
			row.source->addItem(text("Outputs.MultiView"), "multiview");
		}
		for (const OBSSource &input : inputs()) {
			row.source->addItem(QString::fromUtf8(obs_source_get_name(input)),
					    "input:" + QString::fromUtf8(obs_source_get_uuid(input)));
		}
	}
	int found = row.source->findData(sourceKey(config));
	if (found < 0) {
		/* the input it showed is gone: "none" until another is chosen (outputs spec §7) */
		row.source->addItem(text("Outputs.SourceNone"), sourceKey(config));
		found = row.source->count() - 1;
	}
	row.source->setCurrentIndex(found);
	row.source->setEnabled(index > 0);

	row.size = new QComboBox(this);
	row.size->addItem(text("Outputs.SizeProgram"), QSize(0, 0));
	if (index > 0) {
		for (QSize size : {QSize(3840, 2160), QSize(1920, 1080), QSize(1280, 720)}) {
			row.size->addItem(QString("%1×%2").arg(size.width()).arg(size.height()), size);
		}
	}
	row.size->setCurrentIndex(std::max(0, row.size->findData(QSize(config.width, config.height))));

	row.alpha = new QCheckBox(this);
	row.alpha->setChecked(config.alpha);

	row.bus = new QComboBox(this);
	for (const char *bus : {"M", "A", "B", "C", "D", "E"}) {
		row.bus->addItem(bus);
	}
	row.bus->setCurrentIndex(config.bus);

	row.ndi = new QCheckBox(this);
	row.ndi->setChecked(config.ndi);
	row.ndi->setEnabled(OutputEngine::hasOutputType(NdiType));

	row.omt = new QComboBox(this);
	for (const char *key : {"Outputs.Omt.Off", "Outputs.Omt.Vmx", "Outputs.Omt.Hevc", "Outputs.Omt.H264"}) {
		row.omt->addItem(text(key));
	}
	row.omt->setCurrentIndex((int)config.omt);
	row.omt->setEnabled(OutputEngine::hasOutputType(OmtType));

	row.srt = new QCheckBox(this);
	row.srt->setChecked(config.srt);
	row.srt->setEnabled(OutputEngine::hasOutputType(SrtType));
	row.srtForm = config.srtForm;
	row.srtSettings = new QPushButton(QString::fromUtf8("\xE2\x9A\x99"), this);
	connect(row.srtSettings, &QPushButton::clicked, this, [this, index] {
		Row &r = rows[(size_t)index];
		SrtDialog dialog(r.srtForm, this);
		if (dialog.exec() == QDialog::Accepted) {
			r.srtForm = dialog.form();
			r.srt->setChecked(true);
		}
	});

	row.codec = new QComboBox(this);
	row.codec->addItem("H.264");
	row.codec->addItem("HEVC");
	row.codec->setCurrentIndex(config.codec == VideoCodec::Hevc ? 1 : 0);

	row.bitrate = new QSpinBox(this);
	row.bitrate->setRange(500, 100000);
	row.bitrate->setSingleStep(500);
	row.bitrate->setSuffix(" kbps");
	row.bitrate->setValue(config.bitrateKbps);

	row.status = new QLabel(this);
	row.status->setMinimumWidth(260);

	connect(row.source, &QComboBox::currentIndexChanged, this, [this, index](int) { syncRow(index); });
	connect(row.srt, &QCheckBox::toggled, this, [this, index](bool) { syncRow(index); });
	syncRow(index);
}

/* alpha only for an input; SRT needs settings that can be used */
void OutputsWindow::syncRow(int index)
{
	Row &row = rows[(size_t)index];
	const bool input = row.source->currentData().toString().startsWith("input:");
	row.alpha->setEnabled(index > 0 && input);
	row.srtSettings->setEnabled(row.srt->isEnabled());
	if (row.srt->isChecked() && streamProblem(row.srtForm)) {
		row.srt->setChecked(false);
		row.srtSettings->click();
	}
}

OutputsConfig OutputsWindow::read() const
{
	OutputsConfig config = OutputEngine::instance().config(); /* keeps the destinations */
	for (int i = 0; i < OutputCount; i++) {
		const Row &row = rows[(size_t)i];
		OutputConfig &o = config.outputs[(size_t)i];
		const QString key = row.source->currentData().toString();
		o.source = key == "preview"           ? OutputSource::Preview
			   : key == "multiview"       ? OutputSource::MultiView
			   : key.startsWith("input:") ? OutputSource::Input
						      : OutputSource::Program;
		o.input = o.source == OutputSource::Input ? key.mid(6).toStdString() : std::string();
		const QSize size = row.size->currentData().toSize();
		o.width = size.width();
		o.height = size.height();
		o.alpha = row.alpha->isEnabled() && row.alpha->isChecked();
		o.bus = row.bus->currentIndex();
		o.ndi = row.ndi->isChecked();
		o.omt = (OmtMode)row.omt->currentIndex();
		o.srt = row.srt->isChecked() && !streamProblem(row.srtForm);
		o.srtForm = row.srtForm;
		o.codec = row.codec->currentIndex() == 1 ? VideoCodec::Hevc : VideoCodec::H264;
		o.bitrateKbps = row.bitrate->value();
	}
	return config;
}

void OutputsWindow::accept()
{
	OutputEngine::instance().apply(read());
	QDialog::accept();
}

void OutputsWindow::updateStatus()
{
	for (int i = 0; i < OutputCount; i++) {
		const OutputStatus status = OutputEngine::instance().status(i);
		const OutputConfig &config = OutputEngine::instance().config().outputs[(size_t)i];
		QStringList lines;
		if (status.width > 0) {
			lines << text("Outputs.Picture").arg(status.width).arg(status.height).arg(status.skipped);
		}
		if (!status.encoder.empty()) {
			lines << QString::fromStdString(status.encoder);
		}
		if (config.ndi) {
			lines << "NDI: " + stateText(status.ndi, false);
		}
		if (config.omt != OmtMode::Off) {
			lines << "OMT: " + stateText(status.omt, false);
		}
		if (config.srt) {
			lines << "SRT: " + stateText(status.srt, config.srtForm.type == StreamType::SrtListener);
		}
		QLabel *label = rows[(size_t)i].status;
		label->setText(lines.join("\n"));
		const bool red = status.srt.state == SenderState::Retrying || status.ndi.state == SenderState::Failed ||
				 status.omt.state == SenderState::Failed;
		label->setStyleSheet(red ? "color: #d00000;" : "");
	}
}

} // namespace desk
