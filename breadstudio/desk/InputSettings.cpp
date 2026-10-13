#include "InputSettings.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"

#include <obs-frontend-api.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace desk {

InputSettings::InputSettings(obs_source_t *input, QWidget *parent) : QDialog(parent), source(input)
{
	setWindowTitle(text("Settings.Title").arg(QString::fromUtf8(obs_source_get_name(source))));
	setAttribute(Qt::WA_DeleteOnClose);

	auto *layout = new QVBoxLayout(this);

	/* an empty scene made outside the desk has no base */
	auto *baseButton = new QPushButton(text("Settings.Base"), this);
	baseButton->setEnabled(baseSource(source).Get() != nullptr);
	connect(baseButton, &QPushButton::clicked, this, [this] {
		OBSSource base = baseSource(source);
		if (base) {
			obs_frontend_open_source_properties(base);
		}
	});
	layout->addWidget(baseButton);

	layout->addWidget(new QLabel(text("Settings.Layers"), this));
	list = new QListWidget(this);
	layout->addWidget(list, 1);
	connect(list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
		if (!loading) {
			setLayerVisible(source, item->data(Qt::UserRole).toLongLong(),
					item->checkState() == Qt::Checked);
		}
	});

	auto *buttons = new QHBoxLayout();
	layout->addLayout(buttons);

	/* a layer can only be another input (spec D2) */
	auto *add = new QPushButton(text("Settings.AddLayer"), this);
	auto *addMenu = new QMenu(add);
	add->setMenu(addMenu);
	connect(addMenu, &QMenu::aboutToShow, this, [this, addMenu] {
		addMenu->clear();
		for (OBSSource &other : inputs()) {
			if (other.Get() == source.Get()) {
				continue;
			}
			QAction *action = addMenu->addAction(QString::fromUtf8(obs_source_get_name(other)));
			connect(action, &QAction::triggered, this, [this, other] {
				if (!addLayer(source, other)) {
					QMessageBox::warning(this, windowTitle(), text("Settings.Loop"));
				}
				reload();
			});
		}
	});
	connect(addMenu, &QMenu::aboutToHide, addMenu, [addMenu] {
		/* the actions hold references to inputs; let them go once a choice has run */
		QMetaObject::invokeMethod(addMenu, [addMenu] { addMenu->clear(); }, Qt::QueuedConnection);
	});
	buttons->addWidget(add);

	auto *remove = new QPushButton(text("Settings.Remove"), this);
	connect(remove, &QPushButton::clicked, this, [this] {
		removeLayer(source, selected());
		reload();
	});
	buttons->addWidget(remove);

	auto *up = new QPushButton(text("Settings.Up"), this);
	connect(up, &QPushButton::clicked, this, [this] {
		moveLayer(source, selected(), true);
		reload();
	});
	buttons->addWidget(up);

	auto *down = new QPushButton(text("Settings.Down"), this);
	connect(down, &QPushButton::clicked, this, [this] {
		moveLayer(source, selected(), false);
		reload();
	});
	buttons->addWidget(down);

	layout->addWidget(new QLabel(text("Settings.Hint"), this));

	/* so the layers can be dragged in the Preview pane (spec §4.3) */
	toPreview(source);
	reload();
}

int64_t InputSettings::selected() const
{
	QListWidgetItem *item = list->currentItem();
	return item ? item->data(Qt::UserRole).toLongLong() : -1;
}

void InputSettings::reload()
{
	const int64_t keep = selected();

	loading = true;
	list->clear();
	for (const Layer &layer : layers(source)) {
		auto *item = new QListWidgetItem(QString::fromUtf8(layer.name.c_str()), list);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
		item->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(layer.id));
		if (layer.id == keep) {
			list->setCurrentItem(item);
		}
	}
	loading = false;
}

} // namespace desk
