#include "InputBar.hpp"
#include "AudioBed.hpp"
#include "Desk.hpp"
#include "DeskLayout.hpp"
#include "InputModel.hpp"
#include "InputSelect.hpp"
#include "InputSettings.hpp"
#include "OutputEngine.hpp"
#include "OutputsWindow.hpp"
#include "StreamWindow.hpp"
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
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace desk {

/* U+2699, the gear */
static const char *GearGlyph = "\xE2\x9A\x99";

/* the gap between two rows of tiles */
static const int RowGap = 6;

/* A tile's title bar carries the tally, as vMix's does: the colour, with text that can be
 * read on it. */
static QString headerStyle(uint32_t argb)
{
	const QColor back = QColor::fromRgba(argb);
	return QString("background-color: %1; color: %2; padding: 2px 4px;")
		.arg(back.name(), qGray(back.rgb()) > 90 ? "black" : "white");
}

/* The settings windows belong to OBS's main window, not to this dock: Qt wants a top-level
 * window behind a dialog, and a floating dock is a different one from a docked one. */
static QWidget *settingsParent()
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

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

	/* the tiles wrap into rows, as vMix's do, and the rows scroll when there are too many */
	scroll = new QScrollArea(this);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setWidgetResizable(true);
	outer->addWidget(scroll, 1);

	auto *holder = new QWidget(scroll);
	rows = new QVBoxLayout(holder);
	rows->setContentsMargins(0, 0, 0, 0);
	rows->setSpacing(RowGap);
	rows->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	scroll->setWidget(holder);

	/* the footer, as vMix's: Add Input on the left, then Record and Stream */
	auto *footer = new QHBoxLayout();
	footer->setSpacing(6);
	outer->addLayout(footer);

	/* Add Input opens the Input Select window (network inputs spec §3). It blocks the
	 * main window, as vMix's does, but is not run with exec(): clear() must be able to
	 * delete it when the scene collection goes. */
	auto *add = new QPushButton(text("Add"), this);
	connect(add, &QPushButton::clicked, this, [this] {
		auto *window = new InputSelect(settingsParent());
		window->setAttribute(Qt::WA_DeleteOnClose);
		connect(window, &InputSelect::inputMade, this, [](obs_source_t *input) {
			inputsChanged();
			toPreview(input);
		});
		window->open();
	});
	footer->addWidget(add);

	/* the four outputs (outputs spec §3.3); one window at a time */
	auto *outputs = new QPushButton(text("Outputs"), this);
	connect(outputs, &QPushButton::clicked, this, [] {
		for (OutputsWindow *open : settingsParent()->findChildren<OutputsWindow *>()) {
			open->raise();
			open->activateWindow();
			return;
		}
		auto *window = new OutputsWindow(settingsParent());
		window->setAttribute(Qt::WA_DeleteOnClose);
		window->show();
	});
	footer->addWidget(outputs);

	record = new QPushButton(text("Record"), this);
	connect(record, &QPushButton::clicked, this, [] {
		if (obs_frontend_recording_active()) {
			obs_frontend_recording_stop();
		} else {
			obs_frontend_recording_start();
		}
	});
	footer->addWidget(record);

	/* the RTMP destinations (outputs spec §3.2), not OBS's own stream */
	stream = new QPushButton(text("Stream"), this);
	connect(stream, &QPushButton::clicked, this, [this] {
		OutputEngine &engine = OutputEngine::instance();
		if (engine.streaming()) {
			engine.setStreaming(false);
		} else {
			bool any = false;
			for (const Destination &destination : engine.config().destinations) {
				any = any || destination.on;
			}
			if (!any) {
				QMessageBox::information(window(), text("Stream"), text("Stream.NoneOn"));
				openStreamWindow();
				return;
			}
			engine.setStreaming(true);
		}
		updateOutputs();
	});
	footer->addWidget(stream);

	auto *streamSettings = new QPushButton(QString::fromUtf8(GearGlyph), this);
	streamSettings->setToolTip(text("Stream.Title"));
	connect(streamSettings, &QPushButton::clicked, this, [this] { openStreamWindow(); });
	footer->addWidget(streamSettings);

	/* OBS's own menus, which the desk hides the menu bar for (DeskLayout) */
	auto *menu = new QToolButton(this);
	menu->setText(QString::fromUtf8("\xE2\x98\xB0")); /* U+2630 */
	menu->setToolTip(text("Menu"));
	menu->setPopupMode(QToolButton::InstantPopup);
	menu->setMenu(obsMainMenu());
	footer->addWidget(menu);

	/* the button is red while any destination is live, and says so */
	connect(&streamTimer, &QTimer::timeout, this, &InputBar::updateOutputs);
	streamTimer.start(1000);
	footer->addStretch(1);
}

void InputBar::updateOutputs()
{
	record->setText(text(obs_frontend_recording_active() ? "Record.Stop" : "Record"));

	OutputEngine &engine = OutputEngine::instance();
	bool anyLive = false;
	for (int d = 0; d < DestinationCount; d++) {
		anyLive = anyLive || engine.destinationStatus(d).state == SenderState::Live;
	}
	stream->setText(text(engine.streaming() ? "Stream.Stop" : "Stream"));
	stream->setStyleSheet(anyLive ? "background-color: #d00000; color: white;" : "");
}

void InputBar::openStreamWindow()
{
	for (StreamWindow *open : settingsParent()->findChildren<StreamWindow *>()) {
		open->raise();
		open->activateWindow();
		return;
	}
	auto *window = new StreamWindow(settingsParent());
	window->setAttribute(Qt::WA_DeleteOnClose);
	window->show();
}

/* the scroll bar's width is always left free, so the count does not change when it appears */
int InputBar::tilesPerRow() const
{
	const QMargins margins = layout()->contentsMargins();
	const int room = width() - margins.left() - margins.right() - style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	return std::max(1, room / TileDisplay::TileW);
}

void InputBar::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);

	if (!sources.empty() && tilesPerRow() != perRow) {
		layoutTiles();
		updateTally();
	}
}

void InputBar::layoutTiles()
{
	clearTiles();
	perRow = tilesPerRow();

	const int total = (int)sources.size();
	for (int first = 0; first < total; first += perRow) {
		const int last = std::min(first + perRow, total);

		/* a row: the title bars, one display for the pictures, the buttons */
		auto *row = new QWidget(scroll->widget());
		auto *column = new QVBoxLayout(row);
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);
		column->setSizeConstraint(QLayout::SetFixedSize);

		auto *titles = new QHBoxLayout();
		titles->setSpacing(0);
		auto *display = new TileDisplay(row);
		auto *footers = new QHBoxLayout();
		footers->setSpacing(0);
		column->addLayout(titles);
		column->addWidget(display, 0, Qt::AlignLeft);
		column->addLayout(footers);

		for (int i = first; i < last; i++) {
			obs_source_t *input = sources[(size_t)i];
			auto *title = new QLabel(
				QString("%1  %2").arg(i + 1).arg(QString::fromUtf8(obs_source_get_name(input))), row);
			title->setFixedWidth(TileDisplay::TileW);
			title->setToolTip(title->text());
			titles->addWidget(title);
			headers.push_back(title);

			footers->addWidget(makeFooter(i));
		}

		display->setInputs({sources.begin() + first, sources.begin() + last}, first);
		connect(display, &TileDisplay::clicked, this, [this](int index) {
			if ((size_t)index >= sources.size()) {
				return;
			}
			obs_source_t *input = sources[(size_t)index];
			switch (clickAction(input)) {
			case ClickAction::None:
				break;
			case ClickAction::Preview:
				toPreview(input);
				break;
			case ClickAction::Program:
				cutToProgram(input);
				break;
			}
			/* the input's own on-click trigger, whatever the click action did (spec §3.2) */
			runTriggers(input, TriggerWhen::Click);
		});
		connect(display, &TileDisplay::menuRequested, this, &InputBar::showMenu);
		/* the target of a drag may be in another row */
		connect(display, &TileDisplay::dragged, this, [this](int from, const QPoint &globalPos) {
			for (TileDisplay *target : displays) {
				const int to = target->indexAtGlobal(globalPos);
				if (to >= 0 && to != from && canReorder() && moveInput(from, to)) {
					inputsChanged();
					return;
				}
			}
		});
		displays.push_back(display);

		rows->addWidget(row, 0, Qt::AlignLeft);
	}

	/* the dock must not get shorter than one row */
	if (QLayoutItem *item = rows->itemAt(0)) {
		scroll->setMinimumHeight(item->widget()->sizeHint().height());
	}
}

void InputBar::clearTiles()
{
	muteSignals.clear();
	headers.clear();
	audioButtons.clear();

	/* A row is deleted later, because this can run inside a signal from one of its own
	 * widgets. Its display drops its references now, so OBS can unload the collection. */
	for (TileDisplay *display : displays) {
		display->setInputs({});
	}
	displays.clear();

	while (QLayoutItem *item = rows->takeAt(0)) {
		if (QWidget *row = item->widget()) {
			row->hide();
			row->deleteLater();
		}
		delete item;
	}
}

/* the buttons under a tile, as vMix's: Close, Cut, audio, settings */
QWidget *InputBar::makeFooter(int index)
{
	obs_source_t *input = sources[(size_t)index];

	auto *footer = new QWidget();
	footer->setFixedWidth(TileDisplay::TileW);
	auto *buttons = new QHBoxLayout(footer);
	buttons->setContentsMargins(0, 2, 0, 2);
	buttons->setSpacing(2);

	/* Close asks first, and is refused for an input on Program (spec §8) */
	auto *close = new QToolButton(footer);
	close->setText(text("Close"));
	connect(close, &QToolButton::clicked, this, [this, index] {
		if ((size_t)index < sources.size()) {
			OBSSource target = sources[(size_t)index];
			deleteDialog(target);
		}
	});
	buttons->addWidget(close);

	auto *cut = new QToolButton(footer);
	cut->setText(text("Cut"));
	connect(cut, &QToolButton::clicked, this, [this, index] {
		if ((size_t)index < sources.size()) {
			cutToProgram(sources[(size_t)index]);
		}
	});
	buttons->addWidget(cut);

	/* the same switch as the mixer strip's: the input's own on/off intent (spec §5.1);
	 * AFV and Program are applied as an effective mute by AudioBed */
	OBSSource base = baseSource(input);
	auto *audio = new QToolButton(footer);
	audio->setText(text("AudioButton"));
	audio->setToolTip(text("Audio.Tip"));
	audio->setCheckable(true);
	audio->setEnabled(hasAudio(base));
	audio->setChecked(hasAudio(base) && AudioBed::audioOn(input));
	connect(audio, &QToolButton::clicked, this, [this, index](bool on) {
		if ((size_t)index >= sources.size()) {
			return;
		}
		bed().setAudioOn(sources[(size_t)index], on);
	});
	buttons->addWidget(audio);
	audioButtons.push_back(audio);
	buttons->addStretch(1);

	auto *gear = new QToolButton(footer);
	gear->setText(QString::fromUtf8(GearGlyph));
	connect(gear, &QToolButton::clicked, this, [this, index] { openSettings(index); });
	buttons->addWidget(gear);

	if (hasAudio(base)) {
		muteSignals.emplace_back(obs_source_get_signal_handler(base), "mute", onMute, this);
	}

	return footer;
}

void InputBar::refresh()
{
	sources = inputs();

	/* close the settings window of an input that no longer exists */
	for (InputSettings *open : settingsParent()->findChildren<InputSettings *>()) {
		const bool alive = std::any_of(sources.begin(), sources.end(),
					       [open](const OBSSource &input) { return input.Get() == open->input(); });
		if (!alive) {
			delete open;
		} else {
			open->reload(); /* a layer's input may have been renamed or deleted */
		}
	}

	/* the same for a base's page */
	for (InputSelect *open : settingsParent()->findChildren<InputSelect *>()) {
		const bool alive = !open->input() ||
				   std::any_of(sources.begin(), sources.end(),
					       [open](const OBSSource &input) { return input.Get() == open->input(); });
		if (!alive) {
			delete open;
		}
	}

	layoutTiles();
	updateTally();
	updateOutputs();
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
		/* a category tints the tile in place of the tally (spec I4) */
		const uint32_t category = categoryColour(input);
		if (category) {
			frames.push_back(category);
		} else if (input.Get() == program.Get()) {
			frames.push_back(ProgramColor);
		} else if (studio && input.Get() == preview.Get()) {
			frames.push_back(PreviewColor);
		} else {
			frames.push_back(IdleColor);
		}
	}

	for (size_t i = 0; i < headers.size() && i < frames.size(); i++) {
		headers[i]->setStyleSheet(headerStyle(frames[i]));
	}
	for (size_t row = 0; row < displays.size(); row++) {
		const size_t first = std::min(row * (size_t)perRow, frames.size());
		const size_t last = std::min(first + (size_t)perRow, frames.size());
		displays[row]->setFrames({frames.begin() + (long)first, frames.begin() + (long)last});
	}
}

void InputBar::clear()
{
	qDeleteAll(settingsParent()->findChildren<OutputsWindow *>());
	qDeleteAll(settingsParent()->findChildren<StreamWindow *>());
	qDeleteAll(settingsParent()->findChildren<InputSelect *>());
	qDeleteAll(settingsParent()->findChildren<InputSettings *>());
	sources.clear();
	clearTiles();
}

void InputBar::syncAudioButtons()
{
	for (size_t i = 0; i < audioButtons.size() && i < sources.size(); i++) {
		OBSSource base = baseSource(sources[i]);
		if (base) {
			audioButtons[i]->setChecked(AudioBed::audioOn(sources[i]));
		}
	}
}

void InputBar::showMenu(int index, const QPoint &globalPos)
{
	if ((size_t)index >= sources.size()) {
		return;
	}
	OBSSource input = sources[(size_t)index];

	QMenu menu(window()); /* a top-level parent: the dock's content is not one, and Qt says so in the log */
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
	const QString name = QInputDialog::getText(window(), text("Rename"), text("Rename.Prompt"), QLineEdit::Normal,
						   current, &accepted)
				     .trimmed();
	if (!accepted || name == current) {
		return;
	}

	if (!renameInput(input, name.toUtf8().constData())) {
		QMessageBox::warning(window(), text("Rename"), text("Rename.Taken"));
		return;
	}
	inputsChanged();
}

void InputBar::deleteDialog(obs_source_t *input)
{
	switch (canDelete(input)) {
	case DeleteCheck::OnProgram:
		QMessageBox::warning(window(), text("Delete"), text("Delete.OnProgram"));
		return;
	case DeleteCheck::LastInput:
		QMessageBox::warning(window(), text("Delete"), text("Delete.Last"));
		return;
	case DeleteCheck::Ok:
		break;
	}

	const QString name = QString::fromUtf8(obs_source_get_name(input));
	const int hosts = layerUseCount(input);
	const QString question = hosts > 0 ? text("Delete.ConfirmLayers").arg(name).arg(hosts)
					   : text("Delete.Confirm").arg(name);
	if (QMessageBox::question(window(), text("Delete"), question) != QMessageBox::Yes) {
		return;
	}

	/* OBS raises "scene list changed" when the scene goes, which refreshes the bar */
	deleteInput(input);
}

void InputBar::openSettings(int index)
{
	if ((size_t)index >= sources.size()) {
		return;
	}
	obs_source_t *input = sources[(size_t)index];

	for (InputSettings *open : settingsParent()->findChildren<InputSettings *>()) {
		if (open->input() == input) {
			open->raise();
			open->activateWindow();
			return;
		}
	}

	(new InputSettings(input, settingsParent()))->show();
}

} // namespace desk
