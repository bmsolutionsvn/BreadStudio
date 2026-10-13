#include "InputBar.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "TileDisplay.hpp"

#include <obs-frontend-api.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace desk {

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
	buttons->addStretch(1);

	return footer;
}

void InputBar::refresh()
{
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
	sources.clear();
	while (QLayoutItem *item = footerRow->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	display->setInputs({});
}

} // namespace desk
