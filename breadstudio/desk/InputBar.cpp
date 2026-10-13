#include "InputBar.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "TileDisplay.hpp"

#include <obs-frontend-api.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace desk {

/* U+266A, the eighth note */
static const char *AudioGlyph = "\xE2\x99\xAA";

/* runs on whichever thread changed the mute state */
static void onMute(void *param, calldata_t *)
{
	auto *self = static_cast<InputBar *>(param);
	QMetaObject::invokeMethod(self, [self] { self->syncAudioButtons(); }, Qt::QueuedConnection);
}

InputBar::InputBar(QWidget *parent) : QWidget(parent)
{
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(4, 4, 4, 4);

	/* shown only while Studio Mode is off (spec §8) */
	warning = new QFrame(this);
	auto *warningRow = new QHBoxLayout(warning);
	warningRow->setContentsMargins(0, 0, 0, 0);
	warningRow->addWidget(new QLabel(text("StudioModeOff"), warning), 1);
	auto *studioOn = new QPushButton(text("StudioModeOn"), warning);
	connect(studioOn, &QPushButton::clicked, this, [] { obs_frontend_set_preview_program_mode(true); });
	warningRow->addWidget(studioOn);
	warning->setVisible(false);
	outer->addWidget(warning);

	row = new QHBoxLayout();
	outer->addLayout(row, 1);

	auto *scroll = new QScrollArea(this);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	row->addWidget(scroll, 1);

	/* the strip: one wide display on top, a row of footers under it, column by column */
	auto *strip = new QWidget(scroll);
	auto *stripLayout = new QVBoxLayout(strip);
	stripLayout->setContentsMargins(0, 0, 0, 0);
	stripLayout->setSpacing(0);
	stripLayout->setSizeConstraint(QLayout::SetFixedSize);

	display = new TileDisplay(strip);
	stripLayout->addWidget(display, 0, Qt::AlignLeft);

	auto *footers = new QWidget(strip);
	footerRow = new QHBoxLayout(footers);
	footerRow->setContentsMargins(0, 0, 0, 0);
	footerRow->setSpacing(0);
	stripLayout->addWidget(footers, 0, Qt::AlignLeft);

	scroll->setWidget(strip);

	connect(display, &TileDisplay::clicked, this, [this](int index) {
		if ((size_t)index < sources.size()) {
			toPreview(sources[(size_t)index]);
		}
	});

	/* + Add: one entry per base type that exists on this machine (spec §3) */
	struct InputType {
		const char *textKey;
		const char *id;
		bool blank;
	};
	static const InputType types[] = {
		{"Type.Camera", "dshow_input", false},     {"Type.Media", "ffmpeg_source", false},
		{"Type.Screen", "monitor_capture", false}, {"Type.Window", "window_capture", false},
		{"Type.Browser", "browser_source", false}, {"Type.Image", "image_source", false},
		{"Type.Colour", "color_source", false},    {"Type.Blank", "color_source", true},
		{"Type.Audio", "wasapi_input_capture", false},
	};

	auto *add = new QPushButton(text("Add"), this);
	auto *addMenu = new QMenu(add);
	for (const InputType &type : types) {
		if (!obs_get_latest_input_type_id(type.id)) {
			continue;
		}
		QAction *action = addMenu->addAction(text(type.textKey));
		connect(action, &QAction::triggered, this,
			[this, type] { addInput(type.textKey, type.id, type.blank); });
	}
	add->setMenu(addMenu);
	row->addWidget(add, 0, Qt::AlignTop);

	connect(display, &TileDisplay::menuRequested, this, &InputBar::showMenu);
	connect(display, &TileDisplay::moved, this, [](int from, int to) {
		if (moveInput(from, to)) {
			inputsChanged();
		}
	});
}

QWidget *InputBar::makeFooter(int index)
{
	obs_source_t *input = sources[(size_t)index];

	auto *footer = new QWidget();
	footer->setFixedWidth(TileDisplay::TileW);
	auto *column = new QVBoxLayout(footer);
	column->setContentsMargins(2, 2, 2, 2);
	column->setSpacing(2);

	auto *name = new QLabel(QString("%1  %2").arg(index + 1).arg(QString::fromUtf8(obs_source_get_name(input))),
				footer);
	name->setToolTip(name->text());
	column->addWidget(name);

	auto *buttons = new QHBoxLayout();
	buttons->setSpacing(2);
	column->addLayout(buttons);

	auto *cut = new QToolButton(footer);
	cut->setText(text("Cut"));
	connect(cut, &QToolButton::clicked, this, [this, index] {
		if ((size_t)index < sources.size()) {
			cutToProgram(sources[(size_t)index]);
		}
	});
	buttons->addWidget(cut);

	/* the same switch as the mixer strip's: the base's mute, inverted (spec §5.1) */
	OBSSource base = baseSource(input);
	auto *audio = new QToolButton(footer);
	audio->setObjectName("audio");
	audio->setText(QString::fromUtf8(AudioGlyph));
	audio->setToolTip(text("Audio.Tip"));
	audio->setCheckable(true);
	audio->setEnabled(hasAudio(base));
	audio->setChecked(hasAudio(base) && !obs_source_muted(base));
	connect(audio, &QToolButton::clicked, this, [this, index](bool on) {
		if ((size_t)index >= sources.size()) {
			return;
		}
		OBSSource target = baseSource(sources[(size_t)index]);
		if (target) {
			obs_source_set_muted(target, !on);
		}
	});
	buttons->addWidget(audio);
	buttons->addStretch(1);

	if (hasAudio(base)) {
		muteSignals.emplace_back(obs_source_get_signal_handler(base), "mute", onMute, this);
	}

	return footer;
}

void InputBar::refresh()
{
	muteSignals.clear();
	sources = inputs();

	while (QLayoutItem *item = footerRow->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	for (int i = 0; i < (int)sources.size(); i++) {
		footerRow->addWidget(makeFooter(i));
	}

	display->setInputs(sources);
	updateTally();
}

void InputBar::updateTally()
{
	const bool studio = obs_frontend_preview_program_mode_active();
	warning->setVisible(!studio);

	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	OBSSourceAutoRelease preview;
	if (studio) {
		preview = obs_frontend_get_current_preview_scene();
	}

	std::vector<uint32_t> frames;
	for (OBSSource &input : sources) {
		if (input.Get() == program.Get()) {
			frames.push_back(ProgramColor);
		} else if (studio && input.Get() == preview.Get()) {
			frames.push_back(PreviewColor);
		} else {
			frames.push_back(IdleColor);
		}
	}
	display->setFrames(frames);
}

void InputBar::clear()
{
	muteSignals.clear();
	sources.clear();
	while (QLayoutItem *item = footerRow->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	display->setInputs({});
}

void InputBar::syncAudioButtons()
{
	for (int i = 0; i < footerRow->count() && (size_t)i < sources.size(); i++) {
		QWidget *footer = footerRow->itemAt(i)->widget();
		auto *audio = footer ? footer->findChild<QToolButton *>("audio") : nullptr;
		OBSSource base = baseSource(sources[(size_t)i]);
		if (audio && base) {
			audio->setChecked(!obs_source_muted(base));
		}
	}
}

void InputBar::addInput(const char *textKey, const char *typeId, bool blank)
{
	OBSDataAutoRelease settings = obs_data_create();
	if (blank) {
		obs_data_set_int(settings, "color", 0xFF000000);
	}

	OBSSource input = createInput(typeId, obs_module_text(textKey), settings);
	if (!input) {
		return;
	}

	inputsChanged();
	toPreview(input);

	/* a Blank needs no setting up; everything else opens OBS's own properties window */
	OBSSource base = baseSource(input);
	if (!blank && base) {
		obs_frontend_open_source_properties(base);
	}
}

void InputBar::showMenu(int index, const QPoint &globalPos)
{
	if ((size_t)index >= sources.size()) {
		return;
	}
	OBSSource input = sources[(size_t)index];

	QMenu menu(this);
	QAction *rename = menu.addAction(text("Rename"));
	QAction *remove = menu.addAction(text("Delete"));

	QAction *chosen = menu.exec(globalPos);
	if (chosen == rename) {
		renameDialog(input);
	} else if (chosen == remove) {
		deleteDialog(input);
	}
}

void InputBar::renameDialog(obs_source_t *input)
{
	const QString current = QString::fromUtf8(obs_source_get_name(input));

	bool accepted = false;
	const QString name = QInputDialog::getText(this, text("Rename"), text("Rename.Prompt"), QLineEdit::Normal,
						   current, &accepted)
				     .trimmed();
	if (!accepted || name == current) {
		return;
	}

	if (!renameInput(input, name.toUtf8().constData())) {
		QMessageBox::warning(this, text("Rename"), text("Rename.Taken"));
		return;
	}
	inputsChanged();
}

void InputBar::deleteDialog(obs_source_t *input)
{
	switch (canDelete(input)) {
	case DeleteCheck::OnProgram:
		QMessageBox::warning(this, text("Delete"), text("Delete.OnProgram"));
		return;
	case DeleteCheck::LastInput:
		QMessageBox::warning(this, text("Delete"), text("Delete.Last"));
		return;
	case DeleteCheck::Ok:
		break;
	}

	const QString name = QString::fromUtf8(obs_source_get_name(input));
	const int hosts = layerUseCount(input);
	const QString question = hosts > 0 ? text("Delete.ConfirmLayers").arg(name).arg(hosts)
					   : text("Delete.Confirm").arg(name);
	if (QMessageBox::question(this, text("Delete"), question) != QMessageBox::Yes) {
		return;
	}

	/* OBS raises "scene list changed" when the scene goes, which refreshes the bar */
	deleteInput(input);
}

} // namespace desk
