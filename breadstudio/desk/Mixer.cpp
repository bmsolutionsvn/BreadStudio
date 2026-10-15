#include "Mixer.hpp"
#include "AudioBed.hpp"
#include "BusMeters.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "Meter.hpp"

#include <obs-audio-controls.h>

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QScrollArea>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <limits>

namespace desk {

static const float Silent = -std::numeric_limits<float>::infinity();
static const char *BusLetters = "MABCDE";

/* U+266A, the eighth note; U+1F3A7, headphones */
static const char *AudioGlyph = "\xE2\x99\xAA";
static const char *HeadphonesGlyph = "\xF0\x9F\x8E\xA7";

/* The AFV icon is drawn here, not copied from vMix: a screen with sound beside it. */
static QIcon afvIcon(const QPalette &palette)
{
	QPixmap pixmap(20, 20);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(palette.color(QPalette::ButtonText), 1.5));
	painter.drawRoundedRect(QRectF(2, 4, 11, 9), 1.5, 1.5);   /* the screen */
	painter.drawLine(QPointF(5, 16), QPointF(10, 16));        /* its stand */
	painter.drawArc(QRectF(12, 5, 5, 7), -60 * 16, 120 * 16); /* sound */
	painter.drawArc(QRectF(13, 3, 7, 11), -60 * 16, 120 * 16);

	return QIcon(pixmap);
}

/* runs on whichever thread changed the mute state */
static void onMute(void *param, calldata_t *)
{
	auto *self = static_cast<Mixer *>(param);
	QMetaObject::invokeMethod(self, [self] { self->syncMute(); }, Qt::QueuedConnection);
}

struct Mixer::Strip {
	OBSSource source; /* the base of an input, or an OBS audio device */
	OBSSource input;  /* the input this strip is for; null for an OBS audio device */
	obs_fader_t *fader = nullptr;
	obs_volmeter_t *volmeter = nullptr;
	std::atomic<float> peakDb{Silent};
	OBSSignal muteSignal;
	QWidget *widget = nullptr;
	Meter *meter = nullptr;
	QToolButton *audio = nullptr;

	~Strip()
	{
		muteSignal.Disconnect();
		obs_volmeter_remove_callback(volmeter, onLevel, this);
		obs_volmeter_destroy(volmeter);
		obs_fader_destroy(fader);
		delete widget;
	}

	/* runs on OBS's audio thread; levels arrive in dB */
	static void onLevel(void *param, const float[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS],
			    const float[MAX_AUDIO_CHANNELS])
	{
		auto *self = static_cast<Strip *>(param);

		float highest = Silent;
		for (int ch = 0; ch < MAX_AUDIO_CHANNELS; ch++) {
			highest = std::max(highest, peak[ch]);
		}

		float seen = self->peakDb.load();
		while (highest > seen && !self->peakDb.compare_exchange_weak(seen, highest)) {
		}
	}
};

Mixer::Mixer(QWidget *parent) : QWidget(parent)
{
	auto *outer = new QHBoxLayout(this);
	outer->setContentsMargins(4, 4, 4, 4);

	auto *scroll = new QScrollArea(this);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidgetResizable(true);
	auto *holder = new QWidget(scroll);
	stripRow = new QHBoxLayout(holder);
	stripRow->setContentsMargins(0, 0, 0, 0);
	stripRow->setSpacing(4);
	stripRow->setAlignment(Qt::AlignLeft);
	scroll->setWidget(holder);
	outer->addWidget(scroll, 1);

	/* the buses: a level and a number each, no fader (spec D7) */
	auto *buses = new QHBoxLayout();
	buses->setSpacing(4);
	outer->addLayout(buses);
	for (int bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		auto *column = new QVBoxLayout();
		busMeter[bus] = new Meter(this);
		busNumber[bus] = new QLabel("-", this);
		busNumber[bus]->setFixedWidth(40);
		busNumber[bus]->setAlignment(Qt::AlignHCenter);
		busHold[bus] = Silent;

		column->addWidget(new QLabel(QString(QChar(BusLetters[bus])), this), 0, Qt::AlignHCenter);
		column->addWidget(busMeter[bus], 1, Qt::AlignHCenter);
		column->addWidget(busNumber[bus], 0, Qt::AlignHCenter);
		buses->addLayout(column);
	}

	connect(&timer, &QTimer::timeout, this, &Mixer::tick);
	timer.start(33);
}

Mixer::~Mixer() = default;

void Mixer::addStrip(const QString &title, obs_source_t *input, obs_source_t *source)
{
	auto owned = std::make_unique<Strip>();
	Strip *strip = owned.get();
	strip->source = source;
	strip->input = input;

	strip->widget = new QWidget(this);
	strip->widget->setFixedWidth(108);
	auto *column = new QVBoxLayout(strip->widget);
	column->setContentsMargins(2, 2, 2, 2);
	column->setSpacing(2);

	auto *name = new QLabel(title, strip->widget);
	name->setToolTip(title);
	column->addWidget(name);

	/* level and fader */
	auto *levels = new QHBoxLayout();
	column->addLayout(levels, 1);
	strip->meter = new Meter(strip->widget);
	levels->addWidget(strip->meter);

	strip->fader = obs_fader_create(OBS_FADER_LOG);
	obs_fader_attach_source(strip->fader, source);
	auto *slider = new QSlider(Qt::Vertical, strip->widget);
	slider->setRange(0, 1000);
	slider->setValue((int)(obs_fader_get_deflection(strip->fader) * 1000.0f));
	connect(slider, &QSlider::valueChanged, this,
		[strip](int value) { obs_fader_set_deflection(strip->fader, (float)value / 1000.0f); });
	levels->addWidget(slider);

	strip->volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(strip->volmeter, source);
	obs_volmeter_add_callback(strip->volmeter, Strip::onLevel, strip);

	/* audio on/off, AFV, headphones */
	auto *switches = new QHBoxLayout();
	switches->setSpacing(2);
	column->addLayout(switches);

	strip->audio = new QToolButton(strip->widget);
	strip->audio->setText(QString::fromUtf8(AudioGlyph));
	strip->audio->setToolTip(text("Audio.Tip"));
	strip->audio->setCheckable(true);
	strip->audio->setChecked(input ? AudioBed::audioOn(input) : !obs_source_muted(source));
	connect(strip->audio, &QToolButton::clicked, this, [strip](bool on) {
		/* an input's button is its own on/off intent; an OBS audio device has no AFV */
		if (strip->input) {
			bed().setAudioOn(strip->input, on);
		} else {
			obs_source_set_muted(strip->source, !on);
		}
	});
	switches->addWidget(strip->audio);
	strip->muteSignal.Connect(obs_source_get_signal_handler(source), "mute", onMute, this);

	/* an OBS audio device is not an input: it is always on and has no AFV */
	if (input) {
		auto *afv = new QToolButton(strip->widget);
		afv->setIcon(afvIcon(palette()));
		afv->setToolTip(text("AFV.Tip"));
		afv->setCheckable(true);
		afv->setChecked(AudioBed::afv(input));
		OBSSource keep = input;
		connect(afv, &QToolButton::clicked, this, [keep](bool on) { bed().setAfv(keep, on); });
		switches->addWidget(afv);
	}

	auto *phones = new QToolButton(strip->widget);
	phones->setText(QString::fromUtf8(HeadphonesGlyph));
	phones->setToolTip(text("Headphones.Tip"));
	phones->setCheckable(true);
	phones->setChecked(obs_source_get_monitoring_type(source) != OBS_MONITORING_TYPE_NONE);
	connect(phones, &QToolButton::clicked, this, [strip](bool on) {
		obs_source_set_monitoring_type(strip->source,
					       on ? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT : OBS_MONITORING_TYPE_NONE);
	});
	switches->addWidget(phones);
	switches->addStretch(1);

	/* bus sends: OBS's per-source track switches */
	auto *sends = new QHBoxLayout();
	sends->setSpacing(0);
	column->addLayout(sends);
	const uint32_t mixers = obs_source_get_audio_mixers(source);
	for (int bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		const QString letter = QString(QChar(BusLetters[bus]));
		auto *send = new QToolButton(strip->widget);
		send->setText(letter);
		send->setToolTip(text("Bus.Tip").arg(letter));
		send->setFixedWidth(17);
		send->setCheckable(true);
		send->setChecked((mixers & (1u << bus)) != 0);
		connect(send, &QToolButton::clicked, this, [strip, bus](bool on) {
			uint32_t mask = obs_source_get_audio_mixers(strip->source);
			mask = on ? (mask | (1u << bus)) : (mask & ~(1u << bus));
			obs_source_set_audio_mixers(strip->source, mask);
		});
		sends->addWidget(send);
	}

	stripRow->addWidget(strip->widget);
	strips.push_back(std::move(owned));
}

/* ponytail: every strip is made again on any change to the inputs, and a volume or bus
 * change made in OBS's own Advanced Audio Properties shows only after the next rebuild.
 * Follow the sources' "volume" and "audio_mixers" signals if that becomes a nuisance. */
void Mixer::rebuild()
{
	clear();

	std::vector<OBSSource> all = inputs();
	for (size_t i = 0; i < all.size(); i++) {
		OBSSource base = baseSource(all[i]);
		if (hasAudio(base)) {
			addStrip(QString("%1  %2").arg(i + 1).arg(QString::fromUtf8(obs_source_get_name(all[i]))),
				 all[i], base);
		}
	}

	/* the devices set in OBS's Settings -> Audio sit on output channels 1 to 6 */
	for (uint32_t channel = 1; channel <= 6; channel++) {
		OBSSourceAutoRelease device = obs_get_output_source(channel);
		if (hasAudio(device)) {
			addStrip(QString::fromUtf8(obs_source_get_name(device)), nullptr, device);
		}
	}
}

void Mixer::clear()
{
	strips.clear();
}

void Mixer::syncMute()
{
	for (auto &strip : strips) {
		strip->audio->setChecked(strip->input ? AudioBed::audioOn(strip->input)
						      : !obs_source_muted(strip->source));
	}
}

void Mixer::tick()
{
	for (auto &strip : strips) {
		strip->meter->setDb(strip->peakDb.exchange(Silent));
	}

	for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		const float db = busMeters().takePeakDb(bus);
		busMeter[bus]->setDb(db);
		busHold[bus] = std::max(busHold[bus], db);
	}

	/* the numbers change three times a second, so they can be read */
	if (++ticks < 10) {
		return;
	}
	ticks = 0;
	for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		busNumber[bus]->setText(busHold[bus] > -100.0f ? QString::number((double)busHold[bus], 'f', 1)
							       : QString("-"));
		busHold[bus] = Silent;
	}
}

} // namespace desk
