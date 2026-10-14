#include "SrtDialog.hpp"
#include "Desk.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <string>

namespace desk {

SrtDialog::SrtDialog(const StreamForm &form, QWidget *parent) : QDialog(parent)
{
	setWindowTitle(text("Outputs.Srt.Title"));
	auto *layout = new QVBoxLayout(this);
	auto *fields = new QFormLayout();
	layout->addLayout(fields);

	mode = new QComboBox(this);
	for (StreamType type : {StreamType::SrtCaller, StreamType::SrtListener, StreamType::SrtRendezvous}) {
		const std::string key = std::string("Stream.Type.") + streamTypeId(type);
		mode->addItem(text(key.c_str()), QString::fromUtf8(streamTypeId(type)));
	}
	mode->setCurrentIndex(std::max(0, mode->findData(QString::fromUtf8(streamTypeId(form.type)))));
	fields->addRow(text("Stream.Type"), mode);

	host = new QLineEdit(QString::fromStdString(form.host), this);
	fields->addRow(text("Stream.Host"), host);

	port = new QSpinBox(this);
	port->setRange(1, 65535);
	port->setValue(form.port);
	fields->addRow(text("Stream.Port"), port);

	latency = new QSpinBox(this);
	latency->setRange(0, 60000);
	latency->setValue(form.latencyMs);
	fields->addRow(text("Stream.Latency"), latency);

	passphrase = new QLineEdit(QString::fromStdString(form.passphrase), this);
	passphrase->setEchoMode(QLineEdit::PasswordEchoOnEdit);
	passphrase->setMaxLength(79);
	fields->addRow(text("Stream.Passphrase"), passphrase);

	keyLength = new QComboBox(this);
	for (int length : {16, 24, 32}) {
		keyLength->addItem(QString::number(length), length);
	}
	keyLength->setCurrentIndex(std::max(0, keyLength->findData(form.keyLength)));
	fields->addRow(text("Stream.KeyLength"), keyLength);

	streamId = new QLineEdit(QString::fromStdString(form.streamId), this);
	fields->addRow(text("Stream.StreamId"), streamId);

	problem = new QLabel(this);
	problem->setWordWrap(true);
	layout->addWidget(problem);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	ok = buttons->button(QDialogButtonBox::Ok);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

	connect(mode, &QComboBox::currentIndexChanged, this, [this](int) { check(); });
	connect(host, &QLineEdit::textChanged, this, [this] { check(); });
	connect(port, &QSpinBox::valueChanged, this, [this](int) { check(); });
	connect(latency, &QSpinBox::valueChanged, this, [this](int) { check(); });
	connect(passphrase, &QLineEdit::textChanged, this, [this] { check(); });
	connect(streamId, &QLineEdit::textChanged, this, [this] { check(); });
	check();
}

StreamForm SrtDialog::form() const
{
	StreamForm result;
	result.type = streamTypeFromId(mode->currentData().toString().toStdString());
	result.host = host->text().toStdString();
	result.port = port->value();
	result.latencyMs = latency->value();
	result.passphrase = passphrase->text().toStdString();
	result.keyLength = keyLength->currentData().toInt();
	result.streamId = streamId->text().toStdString();
	return result;
}

void SrtDialog::check()
{
	const char *why = streamProblem(form());
	problem->setText(why ? text(why) : QString());
	ok->setEnabled(why == nullptr);
}

} // namespace desk
