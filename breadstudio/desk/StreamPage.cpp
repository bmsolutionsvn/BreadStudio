#include "StreamPage.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <string>

namespace desk {

static const char *FormKey = "bread_stream";

/* in vMix's order */
static const StreamType typeOrder[] = {
	StreamType::RtspUdp,   StreamType::RtspTcp,   StreamType::TsUdp,       StreamType::TsTcp,
	StreamType::TsTcpPull, StreamType::SrtCaller, StreamType::SrtListener, StreamType::SrtRendezvous,
};

StreamPage::StreamPage(QWidget *parent) : InputPage(parent)
{
	buildForm();
	setForm(StreamForm());
}

void StreamPage::buildForm()
{
	auto *layout = new QVBoxLayout(this);
	fields = new QFormLayout();
	layout->addLayout(fields);

	type = new QComboBox(this);
	for (StreamType each : typeOrder) {
		const std::string key = std::string("Stream.Type.") + streamTypeId(each);
		type->addItem(text(key.c_str()), QString::fromUtf8(streamTypeId(each)));
	}
	fields->addRow(text("Stream.Type"), type);

	hostLabel = new QLabel(text("Stream.Host"), this);
	host = new QLineEdit(this);
	fields->addRow(hostLabel, host);

	port = new QSpinBox(this);
	port->setRange(1, 65535);
	fields->addRow(text("Stream.Port"), port);

	latency = new QSpinBox(this);
	latency->setRange(0, 60000);
	fields->addRow(text("Stream.Latency"), latency);

	passphrase = new QLineEdit(this);
	passphrase->setEchoMode(QLineEdit::PasswordEchoOnEdit);
	passphrase->setMaxLength(79);
	fields->addRow(text("Stream.Passphrase"), passphrase);

	keyLength = new QComboBox(this);
	for (int length : {16, 24, 32}) {
		keyLength->addItem(QString::number(length), length);
	}
	fields->addRow(text("Stream.KeyLength"), keyLength);

	streamId = new QLineEdit(this);
	fields->addRow(text("Stream.StreamId"), streamId);

	hardware = new QCheckBox(text("Stream.Hardware"), this);
	fields->addRow(QString(), hardware);

	/* why OK is greyed out, when it is */
	problem = new QLabel(this);
	problem->setWordWrap(true);
	layout->addWidget(problem);
	layout->addStretch(1);

	connect(type, &QComboBox::currentIndexChanged, this, [this](int) { typeChanged(); });
	connect(host, &QLineEdit::textChanged, this, [this] { formChanged(); });
	connect(port, &QSpinBox::valueChanged, this, [this](int) { formChanged(); });
	connect(latency, &QSpinBox::valueChanged, this, [this](int) { formChanged(); });
	connect(passphrase, &QLineEdit::textChanged, this, [this] { formChanged(); });
	connect(streamId, &QLineEdit::textChanged, this, [this] { formChanged(); });
}

StreamForm StreamPage::form() const
{
	StreamForm result;
	result.type = streamTypeFromId(type->currentData().toString().toStdString());
	result.host = host->text().toStdString();
	result.port = port->value();
	result.latencyMs = latency->value();
	result.passphrase = passphrase->text().toStdString();
	result.keyLength = keyLength->currentData().toInt();
	result.streamId = streamId->text().toStdString();
	result.hardwareDecoder = hardware->isChecked();
	return result;
}

void StreamPage::setForm(const StreamForm &value)
{
	type->setCurrentIndex(std::max(0, type->findData(QString::fromUtf8(streamTypeId(value.type)))));
	host->setText(QString::fromStdString(value.host));
	port->setValue(value.port);
	latency->setValue(value.latencyMs);
	passphrase->setText(QString::fromStdString(value.passphrase));
	keyLength->setCurrentIndex(std::max(0, keyLength->findData(value.keyLength)));
	streamId->setText(QString::fromStdString(value.streamId));
	hardware->setChecked(value.hardwareDecoder);
	typeChanged();
}

/* each type shows the fields it uses (spec §5.1) */
void StreamPage::typeChanged()
{
	const StreamType now = streamTypeFromId(type->currentData().toString().toStdString());
	const bool srt = isSrt(now);
	const bool rtsp = isRtsp(now);

	hostLabel->setText(text(rtsp ? "Stream.Url" : "Stream.Host"));
	fields->setRowVisible(port, !rtsp);
	fields->setRowVisible(latency, srt);
	fields->setRowVisible(passphrase, srt);
	fields->setRowVisible(keyLength, srt);
	fields->setRowVisible(streamId, srt);

	formChanged();
}

void StreamPage::formChanged()
{
	const char *why = streamProblem(form());
	/* an empty form is not a mistake yet: say nothing until something is typed */
	problem->setText(why && !host->text().isEmpty() ? text(why) : QString());
	emit readyChanged();
}

bool StreamPage::ready() const
{
	return streamProblem(form()) == nullptr;
}

OBSDataAutoRelease StreamPage::mediaSettings(const StreamForm &value)
{
	const StreamTarget target = streamTarget(value);

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", false);
	obs_data_set_string(settings, "input", target.input.c_str());
	obs_data_set_string(settings, "input_format", target.format.c_str());
	obs_data_set_string(settings, "ffmpeg_options", target.options.c_str());
	obs_data_set_int(settings, "reconnect_delay_sec", 2);
	obs_data_set_bool(settings, "hw_decode", value.hardwareDecoder);
	/* keep running while not on Program, so a cut to it has no start-up delay */
	obs_data_set_bool(settings, "restart_on_activate", false);
	obs_data_set_bool(settings, "close_when_inactive", false);
	obs_data_set_bool(settings, "clear_on_media_end", false);
	return settings;
}

void StreamPage::saveForm(obs_source_t *base, const StreamForm &value)
{
	OBSDataAutoRelease saved = obs_data_create();
	obs_data_set_string(saved, "type", streamTypeId(value.type));
	obs_data_set_string(saved, "host", value.host.c_str());
	obs_data_set_int(saved, "port", value.port);
	obs_data_set_int(saved, "latency_ms", value.latencyMs);
	obs_data_set_string(saved, "passphrase", value.passphrase.c_str());
	obs_data_set_int(saved, "key_length", value.keyLength);
	obs_data_set_string(saved, "stream_id", value.streamId.c_str());
	obs_data_set_bool(saved, "hardware_decoder", value.hardwareDecoder);

	OBSDataAutoRelease keep = obs_source_get_private_settings(base);
	obs_data_set_obj(keep, FormKey, saved);
}

bool StreamPage::loadForm(obs_source_t *base, StreamForm &value)
{
	if (!base) {
		return false;
	}

	OBSDataAutoRelease keep = obs_source_get_private_settings(base);
	OBSDataAutoRelease saved = obs_data_get_obj(keep, FormKey);
	if (!saved) {
		return false;
	}

	value.type = streamTypeFromId(obs_data_get_string(saved, "type"));
	value.host = obs_data_get_string(saved, "host");
	value.port = (int)obs_data_get_int(saved, "port");
	value.latencyMs = (int)obs_data_get_int(saved, "latency_ms");
	value.passphrase = obs_data_get_string(saved, "passphrase");
	value.keyLength = (int)obs_data_get_int(saved, "key_length");
	value.streamId = obs_data_get_string(saved, "stream_id");
	value.hardwareDecoder = obs_data_get_bool(saved, "hardware_decoder");
	return true;
}

bool StreamPage::isStream(obs_source_t *base)
{
	StreamForm unused;
	return loadForm(base, unused);
}

OBSSource StreamPage::create()
{
	const StreamForm value = form();
	if (streamProblem(value)) {
		return OBSSource();
	}

	OBSDataAutoRelease settings = mediaSettings(value);
	OBSSource input = createInput("ffmpeg_source", obs_module_text("Type.Stream"), settings);
	if (input) {
		OBSSource base = baseSource(input);
		if (base) {
			saveForm(base, value);
		}
	}
	return input;
}

} // namespace desk
