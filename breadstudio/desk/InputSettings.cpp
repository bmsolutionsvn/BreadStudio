#include "InputSettings.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "InputPages.hpp"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QListWidget>
#include <QMessageBox>
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

	/* copies another input's desk settings (spec §2/I6) */
	copyFrom = new QPushButton(text("Settings.CopyFrom"), this);
	connect(copyFrom, &QPushButton::clicked, this, [this] {
		QStringList names;
		std::vector<OBSSource> others;
		for (OBSSource &input : inputs()) {
			if (input.Get() == source.Get()) {
				continue;
			}
			names << QString::fromUtf8(obs_source_get_name(input));
			others.emplace_back(input);
		}
		if (names.isEmpty()) {
			QMessageBox::information(this, text("Settings.CopyFrom"), text("Settings.CopyFrom.None"));
			return;
		}

		bool accepted = false;
		const QString chosen = QInputDialog::getItem(
			this, text("Settings.CopyFrom"), text("Settings.CopyFrom.Prompt"), names, 0, false, &accepted);
		const int index = names.indexOf(chosen);
		if (!accepted || index < 0) {
			return;
		}

		copyDeskSettings(source, others[(size_t)index].Get());
		inputsChanged();
	});
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
		/* A page is deleted later, because this can run inside a signal from one of its
		 * own widgets. The page owns nothing, so deferring the delete releases nothing late. */
		page->hide();
		page->deleteLater();
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
