#include "InputPages.hpp"

#include "AudioBed.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "InputSelect.hpp"
#include "TileDisplay.hpp"

#include <obs-frontend-api.h>
#include <properties-view.hpp>

#include <graphics/vec2.h>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

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
		markProjectChanged();
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
		markProjectChanged();
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

	/* the base this input shows, and vMix's Change: the desk's picker with the base itself first,
	 * edited in place, and every kind, any of which puts a new source in place of the base while
	 * the input's name, layers, levels and AFV stay as they are (owner's backlog B4) */
	auto *source = new QWidget(page);
	auto *sourceRow = new QHBoxLayout(source);
	sourceRow->setContentsMargins(0, 0, 0, 0);
	auto *sourceName = new QLabel(QString::fromUtf8(obs_source_get_name(base)), source);
	sourceName->setToolTip(sourceName->text());
	sourceRow->addWidget(sourceName, 1);
	auto *change = new QPushButton(text("Settings.ChangeSource"), source);
	QObject::connect(change, &QPushButton::clicked, page, [input, page] {
		for (InputSelect *open : page->window()->findChildren<InputSelect *>()) {
			open->raise();
			open->activateWindow();
			return;
		}
		auto *window = new InputSelect(input, page->window());
		window->setAttribute(Qt::WA_DeleteOnClose);
		QObject::connect(window, &QDialog::accepted, page, [] { inputsChanged(); });
		window->open();
	});
	sourceRow->addWidget(change);
	form->addRow(text("Settings.Source"), source);

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

	outer->addStretch(1);
	return page;
}

/* drop a layout's widgets; each is deleted later, because this can run inside a signal from one */
void clearLayout(QLayout *layout)
{
	while (QLayoutItem *item = layout->takeAt(0)) {
		if (QWidget *widget = item->widget()) {
			widget->deleteLater();
		}
		delete item;
	}
}

/* OBS's own panel for a filter's properties, changing the filter as the operator changes a
 * setting — the same panel the Input Select page hosts for a source (spec §3.1) */
QWidget *makeFilterView(obs_source_t *filter)
{
	OBSDataAutoRelease settings = obs_source_get_settings(filter);
	auto *view = new OBSPropertiesView(settings.Get(), filter, (PropertiesReloadCallback)obs_source_properties,
					   (PropertiesUpdateCallback) nullptr,
					   (PropertiesVisualUpdateCb)obs_source_update);
	/* the panel changes the filter as the operator edits it, which is a change to the show (P9) */
	QObject::connect(view, &OBSPropertiesView::Changed, view, [] { markProjectChanged(); });
	return view;
}

/* a number box in a form: shows `value`, and writes each change through `set` */
void addSpin(QFormLayout *form, const QString &label, double value, double min, double max, double step, int decimals,
	     QWidget *parent, std::function<void(double)> set)
{
	auto *spin = new QDoubleSpinBox(parent);
	spin->setRange(min, max);
	spin->setDecimals(decimals);
	spin->setSingleStep(step);
	spin->setValue(value);
	QObject::connect(spin, &QDoubleSpinBox::valueChanged, parent, [set = std::move(set)](double v) { set(v); });
	/* the transform is applied as it is typed; the Project is marked changed when the edit
	 * finishes, so the title does not flicker on every keystroke (P9) */
	QObject::connect(spin, &QDoubleSpinBox::editingFinished, parent, [] { markProjectChanged(); });
	form->addRow(label, spin);
}

/* a crop box: whole pixels only, never negative */
void addCrop(QFormLayout *form, const QString &label, int value, QWidget *parent, std::function<void(int)> set)
{
	auto *spin = new QSpinBox(parent);
	spin->setRange(0, 100000);
	spin->setValue(value);
	QObject::connect(spin, &QSpinBox::valueChanged, parent, [set = std::move(set)](int v) { set(v); });
	QObject::connect(spin, &QSpinBox::editingFinished, parent, [] { markProjectChanged(); });
	form->addRow(label, spin);
}

/* The base is created bounded (OBS_BOUNDS_SCALE_INNER), and OBS renders from the bounds and
 * ignores the scale while a bound is set, so a scale change does nothing until the bound is
 * cleared; clear it only when there is one, so nothing is disturbed needlessly. */
void scalePastBounds(OBSSceneItem item, const struct vec2 &scale)
{
	if (obs_sceneitem_get_bounds_type(item) != OBS_BOUNDS_NONE) {
		obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_NONE);
	}
	obs_sceneitem_set_scale(item, &scale);
}

/* one scene item's transform — position, scale, rotation and crop, the controls OBS's own transform
 * window has — changed live as the operator edits it */
QWidget *makeTransform(OBSSceneItem item)
{
	auto *page = new QWidget();
	auto *form = new QFormLayout(page);

	struct vec2 pos;
	obs_sceneitem_get_pos(item, &pos);
	struct vec2 scale;
	obs_sceneitem_get_scale(item, &scale);
	struct obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(item, &crop);

	addSpin(form, text("Settings.Position.X"), pos.x, -32768.0, 32768.0, 1.0, 1, page, [item](double v) {
		struct vec2 now;
		obs_sceneitem_get_pos(item, &now);
		now.x = (float)v;
		obs_sceneitem_set_pos(item, &now);
	});
	addSpin(form, text("Settings.Position.Y"), pos.y, -32768.0, 32768.0, 1.0, 1, page, [item](double v) {
		struct vec2 now;
		obs_sceneitem_get_pos(item, &now);
		now.y = (float)v;
		obs_sceneitem_set_pos(item, &now);
	});
	addSpin(form, text("Settings.Position.ScaleX"), scale.x, -10000.0, 10000.0, 0.1, 3, page, [item](double v) {
		struct vec2 now;
		obs_sceneitem_get_scale(item, &now);
		now.x = (float)v;
		scalePastBounds(item, now);
	});
	addSpin(form, text("Settings.Position.ScaleY"), scale.y, -10000.0, 10000.0, 0.1, 3, page, [item](double v) {
		struct vec2 now;
		obs_sceneitem_get_scale(item, &now);
		now.y = (float)v;
		scalePastBounds(item, now);
	});
	addSpin(form, text("Settings.Position.Rotation"), obs_sceneitem_get_rot(item), -3600.0, 3600.0, 1.0, 2, page,
		[item](double v) { obs_sceneitem_set_rot(item, (float)v); });
	addCrop(form, text("Settings.Position.CropLeft"), crop.left, page, [item](int v) {
		struct obs_sceneitem_crop now;
		obs_sceneitem_get_crop(item, &now);
		now.left = v;
		obs_sceneitem_set_crop(item, &now);
	});
	addCrop(form, text("Settings.Position.CropTop"), crop.top, page, [item](int v) {
		struct obs_sceneitem_crop now;
		obs_sceneitem_get_crop(item, &now);
		now.top = v;
		obs_sceneitem_set_crop(item, &now);
	});
	addCrop(form, text("Settings.Position.CropRight"), crop.right, page, [item](int v) {
		struct obs_sceneitem_crop now;
		obs_sceneitem_get_crop(item, &now);
		now.right = v;
		obs_sceneitem_set_crop(item, &now);
	});
	addCrop(form, text("Settings.Position.CropBottom"), crop.bottom, page, [item](int v) {
		struct obs_sceneitem_crop now;
		obs_sceneitem_get_crop(item, &now);
		now.bottom = v;
		obs_sceneitem_set_crop(item, &now);
	});

	return page;
}

/* Position is the base's own scene item (spec §3.1). Only the bottom item, the base, is read and
 * written, so the input's layers are left alone. */
QWidget *makePosition(obs_source_t *input)
{
	OBSSceneItem item = baseItem(input);
	return item ? makeTransform(item) : nullptr;
}

/* A colour page (spec §3.1): one OBS filter on the base, added when the operator applies it and
 * removed when they clear the tick, with OBS's own panel for its properties. The filter keeps the
 * type's display name, so re-opening the page finds the one it made and does not add a second. */
QWidget *makeFilterPage(obs_source_t *input, const char *filterId)
{
	OBSSource base = baseSource(input);
	const char *display = obs_source_get_display_name(filterId);
	if (!base || !display || (obs_source_get_output_flags(base) & OBS_SOURCE_VIDEO) == 0) {
		return nullptr; /* no base, no such filter type, or a base a video filter cannot sit on */
	}
	const std::string name = display;

	auto *page = new QWidget();
	auto *outer = new QVBoxLayout(page);

	auto *apply = new QCheckBox(text("Settings.Filter.Apply"), page);
	outer->addWidget(apply);

	auto *panel = new QVBoxLayout();
	outer->addLayout(panel, 1);

	/* show OBS's own panel for the filter, once it is on the base */
	auto show = [base, name, panel] {
		clearLayout(panel);
		OBSSourceAutoRelease filter = obs_source_get_filter_by_name(base, name.c_str());
		if (filter) {
			panel->addWidget(makeFilterView(filter));
		}
	};

	OBSSourceAutoRelease existing = obs_source_get_filter_by_name(base, name.c_str());
	apply->setChecked(existing != nullptr);
	if (existing) {
		show();
	}

	QObject::connect(apply, &QCheckBox::toggled, page, [base, name, filterId, panel, show](bool on) {
		markProjectChanged();
		if (!on) {
			clearLayout(panel);
			OBSSourceAutoRelease filter = obs_source_get_filter_by_name(base, name.c_str());
			if (filter) {
				obs_source_filter_remove(base, filter);
			}
			return;
		}
		OBSSourceAutoRelease present = obs_source_get_filter_by_name(base, name.c_str());
		if (!present) {
			OBSSourceAutoRelease filter = obs_source_create(filterId, name.c_str(), nullptr, nullptr);
			if (filter) {
				obs_source_filter_add(base, filter);
			}
		}
		show();
	});

	return page;
}

/* The Effects page (spec §3.1): the base's filters, OBS's own add and remove, and the chosen
 * filter's panel. The list is read back with OBS's filter API, so it is the same set OBS's own
 * Filters window shows. */
class EffectsPage : public QWidget {
public:
	explicit EffectsPage(obs_source_t *base_, QWidget *parent) : QWidget(parent), base(base_)
	{
		auto *outer = new QVBoxLayout(this);

		auto *body = new QHBoxLayout();
		outer->addLayout(body);

		list = new QListWidget(this);
		body->addWidget(list, 1);

		auto *column = new QVBoxLayout();
		body->addLayout(column);

		auto *add = new QPushButton(text("Settings.Filters.Add"), this);
		add->setMenu(makeAddMenu(add));
		column->addWidget(add);

		remove = new QPushButton(text("Settings.Filters.Remove"), this);
		column->addWidget(remove);
		column->addStretch(1);

		panel = new QVBoxLayout();
		outer->addLayout(panel, 1);

		QObject::connect(remove, &QPushButton::clicked, this, [this] { removeCurrent(); });
		QObject::connect(list, &QListWidget::currentRowChanged, this, [this](int) { showCurrent(); });

		refresh();
	}

private:
	/* OBS's own list of filter types, by displayed name, without the ones it will not let an
	 * operator add (OBS orders its own Add menu the same way) */
	QMenu *makeAddMenu(QWidget *parent)
	{
		auto *menu = new QMenu(parent);

		std::vector<std::pair<std::string, std::string>> types; /* displayed name, type id */
		size_t index = 0;
		const char *type = nullptr;
		while (obs_enum_filter_types(index++, &type)) {
			const char *display = obs_source_get_display_name(type);
			const uint32_t caps = obs_get_source_output_flags(type);
			if (!display ||
			    (caps & (OBS_SOURCE_DEPRECATED | OBS_SOURCE_CAP_DISABLED | OBS_SOURCE_CAP_OBSOLETE)) != 0) {
				continue;
			}
			types.emplace_back(display, type);
		}
		std::sort(types.begin(), types.end());

		for (const auto &entry : types) {
			menu->addAction(QString::fromUtf8(entry.first.c_str()), this,
					[this, id = entry.second] { addFilter(id); });
		}
		return menu;
	}

	void addFilter(const std::string &id)
	{
		const char *display = obs_source_get_display_name(id.c_str());
		if (!display) {
			return;
		}

		/* a name no filter on the base holds yet, as OBS's own Add does */
		std::string name = display;
		for (int suffix = 2;; suffix++) {
			OBSSourceAutoRelease clash = obs_source_get_filter_by_name(base, name.c_str());
			if (!clash) {
				break;
			}
			name = std::string(display) + " " + std::to_string(suffix);
		}

		OBSSourceAutoRelease filter = obs_source_create(id.c_str(), name.c_str(), nullptr, nullptr);
		if (!filter) {
			return;
		}
		obs_source_filter_add(base, filter);
		markProjectChanged();

		refresh();
		selectName(QString::fromUtf8(name.c_str()));
	}

	void removeCurrent()
	{
		QListWidgetItem *item = list->currentItem();
		if (!item) {
			return;
		}
		OBSSourceAutoRelease filter = obs_source_get_filter_by_name(base, item->text().toUtf8().constData());
		if (filter) {
			obs_source_filter_remove(base, filter);
			markProjectChanged();
		}
		refresh();
	}

	/* OBS's panel for the filter `item` names, or none for no item */
	void showCurrent()
	{
		clearLayout(panel);

		QListWidgetItem *item = list->currentItem();
		remove->setEnabled(item != nullptr);
		if (!item) {
			return;
		}

		OBSSourceAutoRelease filter = obs_source_get_filter_by_name(base, item->text().toUtf8().constData());
		if (filter) {
			panel->addWidget(makeFilterView(filter));
		}
	}

	void refresh()
	{
		const QString keep = list->currentItem() ? list->currentItem()->text() : QString();

		list->blockSignals(true);
		list->clear();
		obs_source_enum_filters(
			base,
			[](obs_source_t *, obs_source_t *filter, void *param) {
				static_cast<QListWidget *>(param)->addItem(
					QString::fromUtf8(obs_source_get_name(filter)));
			},
			list);
		list->blockSignals(false);

		if (list->count() == 0) {
			showCurrent();
			return;
		}

		int row = 0;
		for (int i = 0; i < list->count(); i++) {
			if (list->item(i)->text() == keep) {
				row = i;
				break;
			}
		}
		list->setCurrentRow(row);
	}

	void selectName(const QString &name)
	{
		for (int i = 0; i < list->count(); i++) {
			if (list->item(i)->text() == name) {
				list->setCurrentRow(i);
				return;
			}
		}
	}

	OBSSource base;
	QListWidget *list = nullptr;
	QPushButton *remove = nullptr;
	QVBoxLayout *panel = nullptr;
};

/* Layers / MultiView (spec §3.1): the desk's layers over the input's base — top first, a tick
 * for shown, up/down and remove — and a small live view of the input as the MultiView draws
 * it. A layer is added by dragging a source in OBS's Preview pane, so the window leaves the
 * input on Preview (design I5) and stays non-modal, so that drag still works. */
class LayersPage : public QWidget {
public:
	explicit LayersPage(obs_source_t *source, QWidget *parent) : QWidget(parent), input(source)
	{
		auto *outer = new QVBoxLayout(this);
		outer->addWidget(new QLabel(text("Settings.Layers"), this));

		list = new QListWidget(this);
		outer->addWidget(list, 1);

		auto *buttons = new QHBoxLayout();
		outer->addLayout(buttons);
		auto *remove = new QPushButton(text("Settings.Remove"), this);
		buttons->addWidget(remove);
		auto *up = new QPushButton(text("Settings.Up"), this);
		buttons->addWidget(up);
		auto *down = new QPushButton(text("Settings.Down"), this);
		buttons->addWidget(down);
		undo = new QPushButton(text("Settings.Undo"), this);
		buttons->addWidget(undo);
		/* vMix's Edit beside a layer: the layer's position, size and crop as numbers (owner's
		 * backlog B5), in its own window so the Preview pane stays free for dragging */
		auto *edit = new QPushButton(text("Settings.EditLayer"), this);
		buttons->addWidget(edit);
		buttons->addStretch(1);
		undo->setEnabled(false);

		outer->addWidget(new QLabel(text("Settings.Hint"), this));

		view = new TileDisplay(this);
		outer->addWidget(view, 0, Qt::AlignLeft);
		view->setInputs({input});
		view->setFrames({frameColour()});

		QObject::connect(list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
			if (!loading) {
				setLayerVisible(input, item->data(Qt::UserRole).toLongLong(),
						item->checkState() == Qt::Checked);
			}
		});
		QObject::connect(remove, &QPushButton::clicked, this, [this] {
			const RemovedLayer taken = removeLayer(input, selected());
			refresh(); /* clears any earlier removal and redraws */
			removed = taken;
			undo->setEnabled(removed.source != nullptr);
		});
		QObject::connect(undo, &QPushButton::clicked, this, [this] {
			restoreLayer(input, removed);
			refresh();
		});
		QObject::connect(edit, &QPushButton::clicked, this, [this] {
			obs_sceneitem_t *item = obs_scene_find_sceneitem_by_id(obs_scene_from_source(input), selected());
			if (!item) {
				return;
			}
			auto *window = new QDialog(this);
			window->setAttribute(Qt::WA_DeleteOnClose);
			window->setWindowTitle(
				text("Settings.EditLayer.Title")
					.arg(QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item)))));
			auto *layout = new QVBoxLayout(window);
			layout->addWidget(makeTransform(OBSSceneItem(item)));
			/* the item may be removed while the window is open: it is held, so nothing dangles,
			 * and closing it is enough */
			QObject::connect(window, &QDialog::finished, this, [this] { refresh(); });
			window->show();
		});
		QObject::connect(up, &QPushButton::clicked, this, [this] {
			moveLayer(input, selected(), true);
			refresh();
		});
		QObject::connect(down, &QPushButton::clicked, this, [this] {
			moveLayer(input, selected(), false);
			refresh();
		});

		refresh();
	}

	/* The colour and the layer list are read before toPreview moved the input onto Preview, and a
	 * layer dragged into Preview later raises no event, so read both again each time shown. */
	void showEvent(QShowEvent *) override
	{
		view->setFrames({frameColour()});
		refresh();
	}

private:
	int64_t selected() const
	{
		QListWidgetItem *item = list->currentItem();
		return item ? item->data(Qt::UserRole).toLongLong() : (int64_t)-1;
	}

	/* the frame the view draws round the picture: the input's tally, else its category, as the
	 * MultiView draws a slot (spec §3.1; owner's backlog B5) */
	uint32_t frameColour() const
	{
		OBSSourceAutoRelease program = obs_frontend_get_current_scene();
		if (program.Get() == input.Get()) {
			return ProgramColor;
		}
		if (obs_frontend_preview_program_mode_active()) {
			OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
			if (preview.Get() == input.Get()) {
				return PreviewColor;
			}
		}
		const uint32_t category = categoryColour(input);
		return category ? category : IdleColor;
	}

	void refresh()
	{
		const int64_t keep = selected();

		loading = true;
		list->clear();
		for (const Layer &layer : layers(input)) {
			auto *item = new QListWidgetItem(QString::fromUtf8(layer.name.c_str()), list);
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
			item->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(layer.id));
			if (layer.id == keep) {
				list->setCurrentItem(item);
			}
		}
		loading = false;

		/* the list is rebuilt from the scene, so a removal captured before now no longer lines
		 * up with it: drop it, and Undo stays grey until the next removal */
		removed = RemovedLayer();
		undo->setEnabled(false);
	}

	OBSSource input;
	QListWidget *list = nullptr;
	TileDisplay *view = nullptr;
	QPushButton *undo = nullptr;
	RemovedLayer removed; /* the most recent removal, so Undo can put it back */
	bool loading = false;
};

/* Triggers (spec §3.1): the three moments the desk can act on, each a choice of the actions
 * the desk can run. The choices live in the input's private settings, key bread_triggers
 * (spec §3.2); the desk runs them when the input becomes Program or Preview, or on a click. */
QWidget *makeTriggers(obs_source_t *input)
{
	auto *page = new QWidget();
	auto *form = new QFormLayout(page);

	const struct {
		TriggerWhen when;
		const char *labelKey;
	} Moments[] = {
		{TriggerWhen::Program, "Settings.Trigger.Program"},
		{TriggerWhen::Preview, "Settings.Trigger.Preview"},
		{TriggerWhen::Click, "Settings.Trigger.Click"},
	};
	const struct {
		TriggerAction action;
		const char *labelKey;
	} Actions[] = {
		{TriggerAction::Nothing, "Settings.TriggerAction.Nothing"},
		{TriggerAction::StartRecording, "Settings.TriggerAction.StartRecording"},
		{TriggerAction::StopRecording, "Settings.TriggerAction.StopRecording"},
		{TriggerAction::StartStream, "Settings.TriggerAction.StartStream"},
		{TriggerAction::StopStream, "Settings.TriggerAction.StopStream"},
		{TriggerAction::SendToPreview, "Settings.TriggerAction.SendToPreview"},
		{TriggerAction::SendToProgram, "Settings.TriggerAction.SendToProgram"},
	};

	for (const auto &moment : Moments) {
		auto *combo = new QComboBox(page);
		for (const auto &action : Actions) {
			combo->addItem(text(action.labelKey), (int)action.action);
		}
		combo->setCurrentIndex(combo->findData((int)trigger(input, moment.when)));
		QObject::connect(combo, &QComboBox::currentIndexChanged, page, [input, when = moment.when, combo](int) {
			setTrigger(input, when, (TriggerAction)combo->currentData().toInt());
		});
		form->addRow(text(moment.labelKey), combo);
	}

	return page;
}

/* Audio: what OBS's Advanced Audio Properties has and the mixer strip does not — balance, sync
 * offset and mono. The fader, mute, monitoring and sends stay on the strip, so nothing is
 * offered twice. OBS's own window is not reachable from a plugin (owner's backlog B4), so these
 * are the base's own audio settings through libobs. Null for a base with no audio. */
QWidget *makeAudio(obs_source_t *input)
{
	OBSSource base = baseSource(input);
	if (!hasAudio(base)) {
		return nullptr;
	}

	auto *page = new QWidget();
	auto *outer = new QVBoxLayout(page);
	auto *form = new QFormLayout();
	outer->addLayout(form);

	/* OBS's balance runs 0 (left) to 1 (right), 0.5 centred; shown as -100 to 100 */
	auto *balance = new QSpinBox(page);
	balance->setRange(-100, 100);
	balance->setValue((int)std::lround(obs_source_get_balance_value(base) * 200.0f - 100.0f));
	QObject::connect(balance, &QSpinBox::valueChanged, page, [base](int value) {
		obs_source_set_balance_value(base, (float)(value + 100) / 200.0f);
		markProjectChanged();
	});
	form->addRow(text("Settings.Audio.Balance"), balance);

	/* the same range OBS's window offers, in milliseconds; libobs keeps nanoseconds */
	auto *sync = new QSpinBox(page);
	sync->setRange(-950, 20000);
	sync->setSuffix(QStringLiteral(" ms"));
	sync->setValue((int)(obs_source_get_sync_offset(base) / 1000000));
	QObject::connect(sync, &QSpinBox::valueChanged, page, [base](int ms) {
		obs_source_set_sync_offset(base, (int64_t)ms * 1000000);
		markProjectChanged();
	});
	form->addRow(text("Settings.Audio.Sync"), sync);

	auto *mono = new QCheckBox(text("Settings.Audio.Mono"), page);
	mono->setChecked((obs_source_get_flags(base) & OBS_SOURCE_FLAG_FORCE_MONO) != 0);
	QObject::connect(mono, &QCheckBox::toggled, page, [base](bool on) {
		const uint32_t flags = obs_source_get_flags(base);
		obs_source_set_flags(base, on ? flags | OBS_SOURCE_FLAG_FORCE_MONO : flags & ~OBS_SOURCE_FLAG_FORCE_MONO);
		markProjectChanged();
	});
	form->addRow(mono);

	outer->addStretch(1);
	return page;
}

/* Advanced (spec §3.1): the base's own OBS properties — the same panel OBS's source properties
 * shows — and a line naming the vMix features BreadStudio
 * does not offer, so the absence is explained (design §3.1). */
QWidget *makeAdvanced(obs_source_t *input)
{
	OBSSource base = baseSource(input);
	if (!base) {
		return nullptr;
	}

	auto *page = new QWidget();
	auto *outer = new QVBoxLayout(page);

	OBSDataAutoRelease settings = obs_source_get_settings(base);
	auto *panel = new OBSPropertiesView(settings.Get(), base.Get(), (PropertiesReloadCallback)obs_source_properties,
					    (PropertiesUpdateCallback) nullptr,
					    (PropertiesVisualUpdateCb)obs_source_update);
	/* the panel changes the base's own settings as the operator edits them, a change to the show (P9) */
	QObject::connect(panel, &OBSPropertiesView::Changed, panel, [] { markProjectChanged(); });
	outer->addWidget(panel, 1);

	auto *absent = new QLabel(text("Settings.Advanced.Absent"), page);
	absent->setWordWrap(true);
	outer->addWidget(absent);

	return page;
}

} // namespace

std::vector<InputSettingPage> pagesFor(obs_source_t *input, InputSettings *)
{
	std::vector<InputSettingPage> pages;

	if (QWidget *general = makeGeneral(input)) {
		pages.push_back({text("Settings.Page.General"), general});
	}

	if (QWidget *audio = makeAudio(input)) {
		pages.push_back({text("Settings.Page.Audio"), audio});
	}

	/* the base's own scene item, where the input has one (spec §3.1) */
	if (QWidget *position = makePosition(input)) {
		pages.push_back({text("Settings.Page.Position"), position});
	}

	/* the colour pages, each backed by one OBS filter on the base (spec §3.1); a page whose
	 * filter type OBS does not have is left out (spec I2). The type id is resolved to the
	 * latest version the way the rest of the desk does, so an OBS major bump that renames the
	 * versioned id does not silently drop the page. */
	static const struct {
		const char *typeId;
		const char *pageKey;
	} Colours[] = {
		{"color_filter", "Settings.Page.ColourAdjust"},
		{"chroma_key_filter", "Settings.Page.ColourKey"},
		{"clut_filter", "Settings.Page.ColourCorrection"},
	};
	for (const auto &colour : Colours) {
		const char *filterId = obs_get_latest_input_type_id(colour.typeId);
		if (!filterId) {
			continue;
		}
		if (QWidget *page = makeFilterPage(input, filterId)) {
			pages.push_back({text(colour.pageKey), page});
		}
	}

	/* every filter on the base, with OBS's own add and remove (spec §3.1) */
	if (OBSSource base = baseSource(input)) {
		pages.push_back({text("Settings.Page.Effects"), new EffectsPage(base, nullptr)});
	}

	/* the desk's layer list and its triggers, and the base's own OBS properties (spec §3.1) */
	pages.push_back({text("Settings.Page.Layers"), new LayersPage(input, nullptr)});
	pages.push_back({text("Settings.Page.Triggers"), makeTriggers(input)});
	if (QWidget *advanced = makeAdvanced(input)) {
		pages.push_back({text("Settings.Page.Advanced"), advanced});
	}

	return pages;
}

} // namespace desk
