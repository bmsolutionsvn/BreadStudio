#include "InputSettings.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "InputPages.hpp"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace desk {

InputSettings::InputSettings(obs_source_t *input, QWidget *parent) : QDialog(parent), source(input)
{
	setAttribute(Qt::WA_DeleteOnClose);
	resize(760, 620);

	auto *outer = new QVBoxLayout(this);

	auto *body = new QHBoxLayout();
	outer->addLayout(body, 1);

	list = new QListWidget(this);
	list->setFixedWidth(200);
	body->addWidget(list);

	pages = new QStackedWidget(this);
	body->addWidget(pages, 1);

	/* the list and the stack are built in the same order, so a row is its page */
	connect(list, &QListWidget::currentRowChanged, this, [this](int row) {
		if (row >= 0) {
			pages->setCurrentIndex(row);
		}
	});

	auto *buttons = new QHBoxLayout();
	outer->addLayout(buttons);

	/* copies another input's desk settings (spec §2/I6); wired in the next task */
	copyFrom = new QPushButton(text("Settings.CopyFrom"), this);
	buttons->addWidget(copyFrom);
	buttons->addStretch(1);

	auto *close = new QPushButton(text("Settings.Close"), this);
	connect(close, &QPushButton::clicked, this, &QDialog::close);
	buttons->addWidget(close);

	rebuild();
}

void InputSettings::rebuild()
{
	const int keep = list->currentRow();

	/* the title names the input's place in the bar, as vMix's does (design I1) */
	const QString name = QString::fromUtf8(obs_source_get_name(source));
	int index = 0;
	const std::vector<OBSSource> all = inputs();
	for (size_t i = 0; i < all.size(); i++) {
		if (all[i].Get() == source.Get()) {
			index = (int)i + 1;
			break;
		}
	}
	setWindowTitle(text("Settings.Title").arg(QString("%1 %2").arg(index).arg(name)));

	/* a page reads its backing again, so the stack is thrown away and built afresh */
	list->clear();
	while (pages->count() > 0) {
		QWidget *page = pages->widget(0);
		pages->removeWidget(page);
		delete page;
	}
	for (const InputSettingPage &page : pagesFor(source, this)) {
		list->addItem(page.title);
		pages->addWidget(page.widget);
	}
	if (list->count() > 0) {
		list->setCurrentRow(keep >= 0 && keep < list->count() ? keep : 0);
	}
}

void InputSettings::reload()
{
	rebuild();
}

} // namespace desk
