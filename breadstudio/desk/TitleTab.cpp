#include "TitleTab.hpp"

#include "Desk.hpp"
#include "GtSettings.hpp"
#include "InputModel.hpp"
#include "InputSettings.hpp"

#include <callback/calldata.h>
#include <callback/proc.h>

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <cstring>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace desk {

namespace {

/* vMix's weight and stretch names, as the style object and the source's parser use them (§4.4). */
const char *const WeightNames[] = {"Thin",	 "ExtraLight", "Light",	    "Normal", "Medium",
				   "DemiBold", "Bold",	   "ExtraBold", "Black",  "ExtraBlack"};
const char *const StretchNames[] = {"UltraCondensed", "ExtraCondensed", "Condensed", "SemiCondensed",
				    "Normal",	      "SemiExpanded",	"Expanded",  "ExtraExpanded",
				    "UltraExpanded"};

/* the picture types a field's Browse… offers; a bare pattern list needs no translation */
const char *const PictureFilter = "*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff";

/* one field gt_info lists, in the source's order (design §4.4) */
struct FieldInfo {
	QString name;
	QString type; /* "text", "image" or "colour" */
	QString def;  /* the file's own value; a Reset returns here */
	bool countdown = false;
};

struct WarningInfo {
	QString kind;
	QString detail;
};

struct Info {
	std::vector<FieldInfo> fields;
	std::vector<WarningInfo> warnings;
};

/* gt_info is a proc on the source that writes its JSON out through `json` (GtSettings.hpp). */
std::string infoJson(obs_source_t *base)
{
	calldata_t data;
	calldata_init(&data);
	std::string json = "{}";
	if (proc_handler_call(obs_source_get_proc_handler(base), gt::ProcInfo, &data)) {
		const char *value = calldata_string(&data, "json");
		if (value) {
			json = value;
		}
	}
	calldata_free(&data);
	return json;
}

/* Read gt_info once: its fields in order, and its warnings (design §4.4). */
Info readInfo(obs_source_t *base)
{
	Info info;
	const QJsonObject root = QJsonDocument::fromJson(QByteArray::fromStdString(infoJson(base))).object();

	for (const QJsonValue &value : root.value(QStringLiteral("fields")).toArray()) {
		const QJsonObject object = value.toObject();
		FieldInfo field;
		field.name = object.value(QStringLiteral("name")).toString();
		field.type = object.value(QStringLiteral("type")).toString();
		field.def = object.value(QStringLiteral("default")).toString();
		field.countdown = object.value(QStringLiteral("countdown")).toBool();
		info.fields.push_back(field);
	}

	for (const QJsonValue &value : root.value(QStringLiteral("warnings")).toArray()) {
		const QJsonObject object = value.toObject();
		info.warnings.push_back({object.value(QStringLiteral("kind")).toString(),
					 object.value(QStringLiteral("detail")).toString()});
	}
	return info;
}

/* Start, Pause, Stop and Reset are the source's gt_countdown proc (§4.4). */
void callCountdown(obs_source_t *base, const QString &field, const char *action)
{
	calldata_t data;
	calldata_init(&data);
	calldata_set_string(&data, "field", field.toUtf8().constData());
	calldata_set_string(&data, "action", action);
	proc_handler_call(obs_source_get_proc_handler(base), gt::ProcCountdown, &data);
	calldata_free(&data);
}

/* A field's value or its file default: values[name] when set, else the default (design §5). */
QString fieldValue(obs_source_t *base, const QString &name, const QString &fallback)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	OBSDataAutoRelease values = obs_data_get_obj(settings, gt::KeyValues);
	const QByteArray key = name.toUtf8();
	if (!values || !obs_data_has_user_value(values, key.constData())) {
		return fallback;
	}
	return QString::fromUtf8(obs_data_get_string(values, key.constData()));
}

/* One key of the field's style override, or `fallback` when the file carries none (§4.4). */
QString styleString(obs_source_t *base, const QString &name, const char *key, const QString &fallback)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	OBSDataAutoRelease styles = obs_data_get_obj(settings, gt::KeyStyles);
	const QByteArray field = name.toUtf8();
	OBSDataAutoRelease entry;
	if (styles) {
		entry = obs_data_get_obj(styles, field.constData());
	}
	if (!entry || !obs_data_has_user_value(entry, key)) {
		return fallback;
	}
	return QString::fromUtf8(obs_data_get_string(entry, key));
}

double styleDouble(obs_source_t *base, const QString &name, const char *key, double fallback)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	OBSDataAutoRelease styles = obs_data_get_obj(settings, gt::KeyStyles);
	const QByteArray field = name.toUtf8();
	OBSDataAutoRelease entry;
	if (styles) {
		entry = obs_data_get_obj(styles, field.constData());
	}
	if (!entry || !obs_data_has_user_value(entry, key)) {
		return fallback;
	}
	return obs_data_get_double(entry, key);
}

bool styleBool(obs_source_t *base, const QString &name, const char *key, bool fallback)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	OBSDataAutoRelease styles = obs_data_get_obj(settings, gt::KeyStyles);
	const QByteArray field = name.toUtf8();
	OBSDataAutoRelease entry;
	if (styles) {
		entry = obs_data_get_obj(styles, field.constData());
	}
	if (!entry || !obs_data_has_user_value(entry, key)) {
		return fallback;
	}
	return obs_data_get_bool(entry, key);
}

QString countdownString(obs_source_t *base, const QString &name, const char *key, const QString &fallback)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	OBSDataAutoRelease countdowns = obs_data_get_obj(settings, gt::KeyCountdowns);
	const QByteArray field = name.toUtf8();
	OBSDataAutoRelease entry;
	if (countdowns) {
		entry = obs_data_get_obj(countdowns, field.constData());
	}
	if (!entry || !obs_data_has_user_value(entry, key)) {
		return fallback;
	}
	return QString::fromUtf8(obs_data_get_string(entry, key));
}

/* Write values[name] and update the source; the key is dropped when the value is the file's own, so
 * the input saves only what differs from the file (design §4.4, §5). */
void writeValue(obs_source_t *base, const QString &name, const QString &value, const QString &def)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	OBSDataAutoRelease values = obs_data_get_obj(settings, gt::KeyValues);
	if (!values) {
		values = obs_data_create();
	}
	const QByteArray key = name.toUtf8();
	if (value == def) {
		obs_data_erase(values, key.constData());
	} else {
		obs_data_set_string(values, key.constData(), value.toUtf8().constData());
	}
	obs_data_set_obj(settings, gt::KeyValues, values);
	obs_source_update(base, settings);
	markProjectChanged();
}

/* Write one key of styles[name], keeping the field's other overrides (§4.4). */
void writeStyle(obs_source_t *base, const QString &name, const std::function<void(obs_data_t *)> &edit)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	OBSDataAutoRelease styles = obs_data_get_obj(settings, gt::KeyStyles);
	if (!styles) {
		styles = obs_data_create();
	}
	const QByteArray key = name.toUtf8();
	OBSDataAutoRelease style = obs_data_get_obj(styles, key.constData());
	if (!style) {
		style = obs_data_create();
	}
	edit(style.Get());
	obs_data_set_obj(styles, key.constData(), style);
	obs_data_set_obj(settings, gt::KeyStyles, styles);
	obs_source_update(base, settings);
	markProjectChanged();
}

/* Reset drops the field's whole style override, back to the file's own (§4.4). */
void resetStyle(obs_source_t *base, const QString &name)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	OBSDataAutoRelease styles = obs_data_get_obj(settings, gt::KeyStyles);
	if (!styles) {
		return;
	}
	obs_data_erase(styles, name.toUtf8().constData());
	obs_data_set_obj(settings, gt::KeyStyles, styles);
	obs_source_update(base, settings);
	markProjectChanged();
}

void writeCountdown(obs_source_t *base, const QString &name, const std::function<void(obs_data_t *)> &edit)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	OBSDataAutoRelease countdowns = obs_data_get_obj(settings, gt::KeyCountdowns);
	if (!countdowns) {
		countdowns = obs_data_create();
	}
	const QByteArray key = name.toUtf8();
	OBSDataAutoRelease countdown = obs_data_get_obj(countdowns, key.constData());
	if (!countdown) {
		countdown = obs_data_create();
	}
	edit(countdown.Get());
	obs_data_set_obj(countdowns, key.constData(), countdown);
	obs_data_set_obj(settings, gt::KeyCountdowns, countdowns);
	obs_source_update(base, settings);
	markProjectChanged();
}

/* Drop a nested object's keys whose field names the new gt_info lacks (the file changed, §4.4). */
void prune(obs_data_t *settings, const char *key, const std::set<std::string> &names)
{
	OBSDataAutoRelease object = obs_data_get_obj(settings, key);
	if (!object) {
		return;
	}
	std::vector<std::string> stale;
	for (obs_data_item_t *item = obs_data_first(object); item; obs_data_item_next(&item)) {
		const char *name = obs_data_item_get_name(item);
		if (name && !names.count(name)) {
			stale.push_back(name);
		}
	}
	for (const std::string &name : stale) {
		obs_data_erase(object, name.c_str());
	}
	obs_data_set_obj(settings, key, object);
}

/* The Title tab's contents, in a scroll area because a title can carry twenty fields (§10). */
class TitleTab : public QWidget {
public:
	TitleTab(obs_source_t *input, InputSettings *window_, QWidget *parent)
		: QWidget(parent), base(baseSource(input)), window(window_)
	{
		build();
	}

private:
	void build();
	void changeFile(const QString &path);

	QWidget *makeTextField(const FieldInfo &field);
	QWidget *makePictureField(const FieldInfo &field);
	QWidget *makeColourField(const FieldInfo &field);

	OBSSource base;
	InputSettings *window = nullptr;
	QLineEdit *filePath = nullptr;
};

void TitleTab::build()
{
	auto *outer = new QVBoxLayout(this);

	/* the .gtzip path and Change… (design §4.4) */
	auto *fileRow = new QHBoxLayout();
	fileRow->addWidget(new QLabel(text("Title.File"), this));
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	filePath = new QLineEdit(QString::fromUtf8(obs_data_get_string(settings, gt::KeyFile)), this);
	filePath->setReadOnly(true);
	fileRow->addWidget(filePath, 1);
	auto *change = new QPushButton(text("Title.Change"), this);
	fileRow->addWidget(change);
	outer->addLayout(fileRow);

	QObject::connect(change, &QPushButton::clicked, this, [this] {
		const QString chosen =
			QFileDialog::getOpenFileName(this, text("Title.File"), filePath->text(), text("Title.Filter"));
		if (!chosen.isEmpty()) {
			changeFile(chosen);
		}
	});

	/* the fields, in the order gt_info lists them (design §4.4) */
	const Info info = readInfo(base);
	for (const FieldInfo &field : info.fields) {
		if (field.type == QStringLiteral("image")) {
			outer->addWidget(makePictureField(field));
		} else if (field.type == QStringLiteral("colour")) {
			outer->addWidget(makeColourField(field));
		} else {
			outer->addWidget(makeTextField(field));
		}
	}

	/* the source's warnings (§7) */
	outer->addWidget(new QLabel(text("Title.Warnings"), this));
	if (info.warnings.empty()) {
		outer->addWidget(new QLabel(text("Title.NoWarnings"), this));
	} else {
		for (const WarningInfo &warning : info.warnings) {
			auto *line = new QLabel(warning.detail, this);
			line->setWordWrap(true);
			outer->addWidget(line);
		}
	}

	outer->addStretch(1);
}

/* A text field: its value, its style, and its countdown (design §4.4). */
QWidget *TitleTab::makeTextField(const FieldInfo &field)
{
	auto *box = new QWidget(this);
	auto *outer = new QVBoxLayout(box);
	outer->setContentsMargins(0, 0, 0, 0);

	auto *name = new QLabel(field.name, box);
	name->setStyleSheet(QStringLiteral("font-weight: bold"));
	outer->addWidget(name);

	/* a two-line box; each change writes values[name] and shows on Program on the next frame */
	auto *value = new QPlainTextEdit(box);
	value->setPlainText(fieldValue(base, field.name, field.def));
	value->setFixedHeight(2 * value->fontMetrics().lineSpacing() + 12);
	outer->addWidget(value);
	QObject::connect(value, &QPlainTextEdit::textChanged, box,
			 [this, field, value] { writeValue(base, field.name, value->toPlainText(), field.def); });

	/* the style: font, size, fill, weight, italic, stretch, line spacing, and Reset (§4.4) */
	auto *style = new QFormLayout();
	outer->addLayout(style);
	style->addRow(new QLabel(text("Title.Style"), box));

	auto *font = new QFontComboBox(box);
	font->setCurrentFont(QFont(styleString(base, field.name, "font", QStringLiteral("Arial"))));
	style->addRow(text("Title.Font"), font);
	QObject::connect(font, &QFontComboBox::currentFontChanged, box, [this, field](const QFont &chosen) {
		writeStyle(base, field.name, [&](obs_data_t *s) {
			obs_data_set_string(s, "font", chosen.family().toUtf8().constData());
		});
	});

	auto *size = new QDoubleSpinBox(box);
	size->setRange(1.0, 2000.0);
	size->setDecimals(2);
	size->setValue(styleDouble(base, field.name, "size", 36.0));
	style->addRow(text("Title.Size"), size);
	QObject::connect(size, &QDoubleSpinBox::valueChanged, box, [this, field](double chosen) {
		writeStyle(base, field.name, [&](obs_data_t *s) { obs_data_set_double(s, "size", chosen); });
	});

	auto *fill = new QPushButton(box);
	const QString fillNow = styleString(base, field.name, "fill", QStringLiteral("#FF000000"));
	fill->setStyleSheet(QString("background-color: %1;").arg(QColor::fromString(fillNow).name(QColor::HexArgb)));
	style->addRow(text("Title.Fill"), fill);
	QObject::connect(fill, &QPushButton::clicked, box, [this, field, fill] {
		const QString current = styleString(base, field.name, "fill", QStringLiteral("#FF000000"));
		const QColor chosen = QColorDialog::getColor(QColor::fromString(current), this, text("Title.Fill"),
							     QColorDialog::ShowAlphaChannel);
		if (!chosen.isValid()) {
			return;
		}
		const QString hex = chosen.name(QColor::HexArgb);
		writeStyle(base, field.name,
			   [&](obs_data_t *s) { obs_data_set_string(s, "fill", hex.toUtf8().constData()); });
		fill->setStyleSheet(QString("background-color: %1;").arg(hex));
	});

	auto *weight = new QComboBox(box);
	for (const char *entry : WeightNames) {
		weight->addItem(entry);
	}
	const QString weightNow = styleString(base, field.name, "weight", QStringLiteral("Normal"));
	const int weightRow = weight->findText(weightNow);
	weight->setCurrentIndex(weightRow >= 0 ? weightRow : weight->findText(QStringLiteral("Normal")));
	style->addRow(text("Title.Weight"), weight);
	QObject::connect(weight, &QComboBox::currentTextChanged, box, [this, field](const QString &chosen) {
		writeStyle(base, field.name, [&](obs_data_t *s) {
			obs_data_set_string(s, "weight", chosen.toUtf8().constData());
		});
	});

	auto *italic = new QCheckBox(text("Title.Italic"), box);
	italic->setChecked(styleBool(base, field.name, "italic", false));
	style->addRow(italic);
	QObject::connect(italic, &QCheckBox::toggled, box, [this, field](bool on) {
		writeStyle(base, field.name, [&](obs_data_t *s) { obs_data_set_bool(s, "italic", on); });
	});

	auto *stretch = new QComboBox(box);
	for (const char *entry : StretchNames) {
		stretch->addItem(entry);
	}
	const QString stretchNow = styleString(base, field.name, "stretch", QStringLiteral("Normal"));
	const int stretchRow = stretch->findText(stretchNow);
	stretch->setCurrentIndex(stretchRow >= 0 ? stretchRow : stretch->findText(QStringLiteral("Normal")));
	style->addRow(text("Title.Stretch"), stretch);
	QObject::connect(stretch, &QComboBox::currentTextChanged, box, [this, field](const QString &chosen) {
		writeStyle(base, field.name, [&](obs_data_t *s) {
			obs_data_set_string(s, "stretch", chosen.toUtf8().constData());
		});
	});

	auto *spacing = new QDoubleSpinBox(box);
	spacing->setRange(0.0, 1000.0);
	spacing->setDecimals(2);
	spacing->setValue(styleDouble(base, field.name, "line_spacing", 0.0));
	style->addRow(text("Title.LineSpacing"), spacing);
	QObject::connect(spacing, &QDoubleSpinBox::valueChanged, box, [this, field](double chosen) {
		writeStyle(base, field.name, [&](obs_data_t *s) { obs_data_set_double(s, "line_spacing", chosen); });
	});

	auto *resetStyleButton = new QPushButton(text("Title.Reset"), box);
	style->addRow(resetStyleButton);
	QObject::connect(resetStyleButton, &QPushButton::clicked, box,
			 [this, field] { resetStyle(base, field.name); });

	/* the countdown: its duration and format, and Start, Pause, Stop and Reset (§4.4, §4.5) */
	auto *countdown = new QFormLayout();
	outer->addLayout(countdown);

	auto *duration = new QLineEdit(countdownString(base, field.name, "duration", QStringLiteral("00:00:00")), box);
	countdown->addRow(text("Title.Duration"), duration);
	QObject::connect(duration, &QLineEdit::editingFinished, box, [this, field, duration] {
		writeCountdown(base, field.name, [&](obs_data_t *c) {
			obs_data_set_string(c, "duration", duration->text().toUtf8().constData());
		});
	});

	auto *format = new QLineEdit(countdownString(base, field.name, "format", QStringLiteral("mm:ss")), box);
	countdown->addRow(text("Title.Format"), format);
	QObject::connect(format, &QLineEdit::editingFinished, box, [this, field, format] {
		writeCountdown(base, field.name, [&](obs_data_t *c) {
			obs_data_set_string(c, "format", format->text().toUtf8().constData());
		});
	});

	const std::pair<const char *, const char *> CountdownButtons[] = {
		{"Title.Start", "start"}, {"Title.Pause", "pause"}, {"Title.Stop", "stop"}, {"Title.Reset", "reset"}};
	auto *buttons = new QHBoxLayout();
	for (const auto &entry : CountdownButtons) {
		auto *button = new QPushButton(text(entry.first), box);
		const char *action = entry.second;
		QObject::connect(button, &QPushButton::clicked, box,
				 [this, field, action] { callCountdown(base, field.name, action); });
		buttons->addWidget(button);
	}
	countdown->addRow(buttons);

	return box;
}

/* A picture field: the path, Browse… and Reset (design §4.4). */
QWidget *TitleTab::makePictureField(const FieldInfo &field)
{
	auto *box = new QWidget(this);
	auto *outer = new QVBoxLayout(box);
	outer->setContentsMargins(0, 0, 0, 0);

	auto *name = new QLabel(field.name, box);
	name->setStyleSheet(QStringLiteral("font-weight: bold"));
	outer->addWidget(name);

	auto *row = new QHBoxLayout();
	auto *path = new QLineEdit(fieldValue(base, field.name, field.def), box);
	path->setReadOnly(true);
	row->addWidget(path, 1);
	auto *browse = new QPushButton(text("Title.Browse"), box);
	row->addWidget(browse);
	auto *reset = new QPushButton(text("Title.Reset"), box);
	row->addWidget(reset);
	outer->addLayout(row);

	QObject::connect(browse, &QPushButton::clicked, box, [this, field, path] {
		const QString chosen = QFileDialog::getOpenFileName(this, text("Title.Browse"), path->text(), PictureFilter);
		if (chosen.isEmpty()) {
			return;
		}
		writeValue(base, field.name, chosen, field.def);
		path->setText(chosen);
	});
	QObject::connect(reset, &QPushButton::clicked, box, [this, field, path] {
		writeValue(base, field.name, field.def, field.def); /* equal to the default: the key is dropped */
		path->setText(field.def);
	});

	return box;
}

/* A colour field: a button with alpha, writing #AARRGGBB (design §4.4). */
QWidget *TitleTab::makeColourField(const FieldInfo &field)
{
	auto *box = new QWidget(this);
	auto *outer = new QVBoxLayout(box);
	outer->setContentsMargins(0, 0, 0, 0);

	auto *name = new QLabel(field.name, box);
	name->setStyleSheet(QStringLiteral("font-weight: bold"));
	outer->addWidget(name);

	auto *button = new QPushButton(box);
	const QString now = fieldValue(base, field.name, field.def);
	button->setStyleSheet(QString("background-color: %1;").arg(QColor::fromString(now).name(QColor::HexArgb)));
	outer->addWidget(button);

	QObject::connect(button, &QPushButton::clicked, box, [this, field, button] {
		const QString current = fieldValue(base, field.name, field.def);
		const QColor chosen = QColorDialog::getColor(QColor::fromString(current), this, text("Title.Fill"),
							     QColorDialog::ShowAlphaChannel);
		if (!chosen.isValid()) {
			return;
		}
		const QString hex = chosen.name(QColor::HexArgb);
		writeValue(base, field.name, hex, field.def);
		button->setStyleSheet(QString("background-color: %1;").arg(hex));
	});

	return box;
}

/* The file changed: keep the fields the new file has, drop the rest, then read the pages again. */
void TitleTab::changeFile(const QString &path)
{
	OBSDataAutoRelease settings = obs_source_get_settings(base);
	obs_data_set_string(settings, gt::KeyFile, path.toUtf8().constData());
	obs_source_update(base, settings);

	std::set<std::string> names;
	for (const FieldInfo &field : readInfo(base).fields) {
		names.insert(field.name.toStdString());
	}
	OBSDataAutoRelease now = obs_source_get_settings(base);
	prune(now, gt::KeyValues, names);
	prune(now, gt::KeyStyles, names);
	prune(now, gt::KeyCountdowns, names);
	obs_source_update(base, now);

	markProjectChanged();
	filePath->setText(path);
	if (window) {
		window->reload();
	}
}

} // namespace

QWidget *makeTitleTab(obs_source_t *input, InputSettings *window)
{
	if (!input) {
		return nullptr;
	}
	OBSSource base = baseSource(input);
	const char *id = base ? obs_source_get_id(base) : nullptr;
	if (!id || std::strcmp(id, gt::SourceId) != 0) {
		return nullptr; /* not a GT title: the tab is absent, as every other kind's pages are */
	}

	auto *scroll = new QScrollArea();
	scroll->setWidgetResizable(true);
	auto *content = new TitleTab(base.Get(), window, scroll);
	scroll->setWidget(content);
	return scroll;
}

} // namespace desk
