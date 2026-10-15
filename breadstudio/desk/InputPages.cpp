#include "InputPages.hpp"

#include "AudioBed.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"

#include <obs-frontend-api.h>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace desk {

namespace {

/* the colours a tile's category can be tagged with (spec I4); the values are the model's
 * bread_category key, a "#rrggbb" string */
const char *const CategoryPalette[] = {
	"#d00000", "#e08000", "#e8c800", "#40b000", "#00a090", "#0080d0", "#7040c0", "#c04090",
};

/* the deinterlacing modes OBS offers, named as OBS's own menu names them (spec §3.2) */
struct DeinterlaceMode {
	const char *label;
	enum obs_deinterlace_mode mode;
};

const DeinterlaceMode DeinterlaceModes[] = {
	{"Settings.Deinterlacing.Disable", OBS_DEINTERLACE_MODE_DISABLE},
	{"Settings.Deinterlacing.Discard", OBS_DEINTERLACE_MODE_DISCARD},
	{"Settings.Deinterlacing.Retro", OBS_DEINTERLACE_MODE_RETRO},
	{"Settings.Deinterlacing.Blend", OBS_DEINTERLACE_MODE_BLEND},
	{"Settings.Deinterlacing.Blend2x", OBS_DEINTERLACE_MODE_BLEND_2X},
	{"Settings.Deinterlacing.Linear", OBS_DEINTERLACE_MODE_LINEAR},
	{"Settings.Deinterlacing.Linear2x", OBS_DEINTERLACE_MODE_LINEAR_2X},
	{"Settings.Deinterlacing.Yadif", OBS_DEINTERLACE_MODE_YADIF},
	{"Settings.Deinterlacing.Yadif2x", OBS_DEINTERLACE_MODE_YADIF_2X},
};

/* Deinterlacing is a source-level setting, not a property: OBS's own UI offers it only for an
 * async-video source, and the mode is read and written through the source API (spec §3.2). */
void addDeinterlacing(QFormLayout *form, obs_source_t *base)
{
	if ((obs_source_get_output_flags(base) & OBS_SOURCE_ASYNC_VIDEO) != OBS_SOURCE_ASYNC_VIDEO) {
		return;
	}

	auto *combo = new QComboBox();
	for (const DeinterlaceMode &entry : DeinterlaceModes) {
		combo->addItem(text(entry.label), (int)entry.mode);
	}
	combo->setCurrentIndex(combo->findData((int)obs_source_get_deinterlace_mode(base)));

	QObject::connect(combo, &QComboBox::currentIndexChanged, combo, [base, combo](int) {
		obs_source_set_deinterlace_mode(base, (enum obs_deinterlace_mode)combo->currentData().toInt());
	});

	form->addRow(text("Settings.Deinterlacing"), combo);
}

/* Restart with transition is the base's own "restart on activate", where its type has one: the
 * media and slideshow sources do (spec §3.2). Play and Pause with transition are the scene
 * item's behaviour, not a source setting, so they are not offered here. */
void addRestart(QFormLayout *form, obs_source_t *base, QWidget *parent)
{
	obs_properties_t *props = obs_source_properties(base);
	obs_property_t *property = obs_properties_get(props, "restart_on_activate");
	const bool backed = property && obs_property_get_type(property) == OBS_PROPERTY_BOOL;
	obs_properties_destroy(props);
	if (!backed) {
		return;
	}

	auto *check = new QCheckBox(text("Settings.Restart"), parent);
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	check->setChecked(obs_data_get_bool(settings, "restart_on_activate"));
	QObject::connect(check, &QCheckBox::toggled, parent, [base](bool on) {
		OBSDataAutoRelease now = obs_source_get_settings(base);
		obs_data_set_bool(now, "restart_on_activate", on);
		obs_source_update(base, now);
	});
	form->addRow(check);
}

QString resolutionText(obs_source_t *base)
{
	const uint32_t width = obs_source_get_width(base);
	const uint32_t height = obs_source_get_height(base);
	return width && height ? QString("%1×%2").arg(width).arg(height) : QStringLiteral("—");
}

/* the rate the input is drawn at: the canvas's, as the desk has one canvas */
QString frameRateText()
{
	struct obs_video_info info;
	if (!obs_get_video_info(&info) || !info.fps_den) {
		return QStringLiteral("—");
	}
	return QString::number((double)info.fps_num / (double)info.fps_den, 'g', 6);
}

QWidget *makeGeneral(obs_source_t *input)
{
	obs_source_t *base = baseSource(input);
	if (!base) {
		return nullptr; /* an empty scene made outside the desk has no base */
	}

	auto *page = new QWidget();
	auto *outer = new QVBoxLayout(page);
	auto *form = new QFormLayout();
	outer->addLayout(form);

	/* the desk's rename, which refuses a taken name (spec §3.2) */
	auto *name = new QLineEdit(QString::fromUtf8(obs_source_get_name(input)), page);
	QObject::connect(name, &QLineEdit::editingFinished, name, [input, name] {
		const QString current = QString::fromUtf8(obs_source_get_name(input));
		const QString wanted = name->text().trimmed();
		if (wanted.isEmpty() || wanted == current) {
			name->setText(current);
			return;
		}
		if (!renameInput(input, wanted.toUtf8().constData())) {
			QMessageBox::warning(name, text("Rename"), text("Rename.Taken"));
			name->setText(current);
		}
	});
	form->addRow(text("Settings.Name"), name);

	/* Aspect Ratio is not an OBS source property, so it is not shown; Deinterlacing is a
	 * source-level setting, shown for the types OBS offers it on (spec §3.2) */
	addDeinterlacing(form, base);

	/* the tile's colour tag (spec I4): one swatch per palette colour, the input's current one
	 * ringed; ticking the ringed swatch again clears the tag */
	const std::string current = category(input);
	auto *swatches = new QWidget(page);
	auto *row = new QHBoxLayout(swatches);
	row->setContentsMargins(0, 0, 0, 0);
	auto *group = new QButtonGroup(page);
	group->setExclusive(false); /* so the ringed swatch can be unticked, to clear the tag */
	for (const char *colour : CategoryPalette) {
		auto *swatch = new QPushButton(swatches);
		swatch->setCheckable(true);
		swatch->setFixedSize(22, 22);
		swatch->setChecked(current == colour);
		group->addButton(swatch);
		row->addWidget(swatch);
		QObject::connect(swatch, &QPushButton::toggled, page, [input, group, swatch, colour](bool on) {
			swatch->setStyleSheet(QString("background-color: %1; border: 2px solid %2;")
						      .arg(QString::fromUtf8(colour), on ? "#ffffff" : "transparent"));
			if (!on) {
				/* unticking the only ringed swatch clears the tag */
				if (!group->checkedButton()) {
					setCategory(input, "");
					inputsChanged();
				}
				return;
			}
			for (QAbstractButton *other : group->buttons()) {
				if (other != swatch && other->isChecked()) {
					other->setChecked(false);
				}
			}
			setCategory(input, colour);
			inputsChanged();
		});
		swatch->setStyleSheet(
			QString("background-color: %1; border: 2px solid %2;")
				.arg(QString::fromUtf8(colour), current == colour ? "#ffffff" : "transparent"));
	}
	row->addStretch(1);
	form->addRow(text("Settings.Category"), swatches);

	/* the desk's AFV, and the base's own automatic behaviour where its type has one (spec §3.2);
	 * both sit in the form so the page reads in vMix's order */
	auto *afv = new QCheckBox(text("Settings.Afv"), page);
	afv->setChecked(AudioBed::afv(input));
	QObject::connect(afv, &QCheckBox::toggled, page, [input](bool on) { bed().setAfv(input, on); });
	form->addRow(afv);

	addRestart(form, base, page);

	/* what a click on the tile does (spec §3.2); the bar reads the model at the click */
	auto *click = new QComboBox(page);
	click->addItem(text("Settings.Click.None"), (int)ClickAction::None);
	click->addItem(text("Settings.Click.Preview"), (int)ClickAction::Preview);
	click->addItem(text("Settings.Click.Program"), (int)ClickAction::Program);
	click->setCurrentIndex(click->findData((int)clickAction(input)));
	QObject::connect(click, &QComboBox::currentIndexChanged, page,
			 [input, click](int) { setClickAction(input, (ClickAction)click->currentData().toInt()); });
	form->addRow(text("Settings.Click"), click);

	form->addRow(text("Settings.Resolution"), new QLabel(resolutionText(base), page));
	form->addRow(text("Settings.FrameRate"), new QLabel(frameRateText(), page));

	/* OBS's own panel for the base: its Advanced Audio is not reachable from a plugin */
	auto *audio = new QPushButton(text("Settings.AudioSettings"), page);
	QObject::connect(audio, &QPushButton::clicked, page, [base] { obs_frontend_open_source_properties(base); });
	outer->addWidget(audio);

	outer->addStretch(1);
	return page;
}

} // namespace

std::vector<InputSettingPage> pagesFor(obs_source_t *input, InputSettings *)
{
	std::vector<InputSettingPage> pages;

	if (QWidget *general = makeGeneral(input)) {
		pages.push_back({text("Settings.Page.General"), general});
	}

	return pages;
}

} // namespace desk
