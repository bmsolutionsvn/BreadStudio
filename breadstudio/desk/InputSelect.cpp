#include "InputSelect.hpp"
#include "Desk.hpp"
#include "GtSettings.hpp"
#include "InputModel.hpp"
#include "InputPage.hpp"
#include "NdiTab.hpp"
#include "OmtTab.hpp"
#include "PropertiesPage.hpp"
#include "StreamPage.hpp"
#include "TitlePage.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include <cstring>
#include <iterator>

namespace desk {

namespace {

/* a black input, to put layers on */
class BlankPage : public InputPage {
public:
	explicit BlankPage(QWidget *parent) : InputPage(parent)
	{
		auto *layout = new QVBoxLayout(this);
		layout->addWidget(new QLabel(text("InputSelect.BlankHint"), this), 0, Qt::AlignTop);
	}

	OBSSource create() override
	{
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_int(settings, "color", 0xFF000000);
		return createInput("color_source", obs_module_text("Type.Blank"), settings);
	}
};

/* a screen or a window of this PC (spec §4.4) */
class DesktopPage : public InputPage {
public:
	explicit DesktopPage(QWidget *parent) : InputPage(parent)
	{
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);

		auto *choice = new QHBoxLayout();
		auto *screen = new QRadioButton(text("Type.Screen"), this);
		auto *window = new QRadioButton(text("Type.Window"), this);
		choice->addWidget(screen);
		choice->addWidget(window);
		choice->addStretch(1);
		layout->addLayout(choice);

		stack = new QStackedWidget(this);
		stack->addWidget(new PropertiesPage("monitor_capture", "Type.Screen", stack));
		stack->addWidget(new PropertiesPage("window_capture", "Type.Window", stack));
		layout->addWidget(stack, 1);

		screen->setChecked(true);
		connect(window, &QRadioButton::toggled, this, [this](bool on) { stack->setCurrentIndex(on ? 1 : 0); });
	}

	OBSSource create() override { return static_cast<InputPage *>(stack->currentWidget())->create(); }

private:
	QStackedWidget *stack;
};

/* The OMT / Desktop Capture row: one tab per kind, as vMix groups them (spec §4). A tab whose plugin
 * is not in the build is left out. NDI, then OMT, then Desktop Capture, as vMix's tabs. */
class CapturePage : public InputPage {
public:
	explicit CapturePage(QWidget *parent) : InputPage(parent)
	{
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);
		tabs = new QTabWidget(this);
		layout->addWidget(tabs);

		if (obs_get_latest_input_type_id(NdiTab::TypeId)) {
			addTab(new NdiTab(tabs), "Tab.Ndi");
		}
		if (obs_get_latest_input_type_id(OmtTab::TypeId)) {
			addTab(new OmtTab(tabs), "Tab.Omt");
		}
		addTab(new DesktopPage(tabs), "Tab.Desktop");

		connect(tabs, &QTabWidget::currentChanged, this, &InputPage::readyChanged);
	}

	bool ready() const override { return current()->ready(); }
	OBSSource create() override { return current()->create(); }

private:
	void addTab(InputPage *page, const char *key)
	{
		tabs->addTab(page, text(key));
		connect(page, &InputPage::readyChanged, this, &InputPage::readyChanged);
	}

	InputPage *current() const { return static_cast<InputPage *>(tabs->currentWidget()); }

	QTabWidget *tabs;
};

/* One row of the list. `typeId` is the OBS source type the row needs: without it the row
 * is left out. */
struct Kind {
	const char *textKey;
	const char *typeId;
	InputPage *(*make)(QWidget *parent);
};

const Kind kinds[] = {
	{"Kind.Video", "ffmpeg_source",
	 [](QWidget *parent) -> InputPage * {
		 return new PropertiesPage("ffmpeg_source", "Type.Media", parent);
	 }},
	{"Kind.Camera", "dshow_input",
	 [](QWidget *parent) -> InputPage * {
		 return new PropertiesPage("dshow_input", "Type.Camera", parent);
	 }},
	{"Kind.Desktop", "monitor_capture",
	 [](QWidget *parent) -> InputPage * {
		 return new CapturePage(parent);
	 }},
	{"Kind.Stream", "ffmpeg_source",
	 [](QWidget *parent) -> InputPage * {
		 return new StreamPage(parent);
	 }},
	{"Kind.Image", "image_source",
	 [](QWidget *parent) -> InputPage * {
		 return new PropertiesPage("image_source", "Type.Image", parent);
	 }},
	{"Kind.Title", gt::SourceId,
	 [](QWidget *parent) -> InputPage * {
		 return new TitlePage(parent);
	 }},
	{"Kind.Colour", "color_source",
	 [](QWidget *parent) -> InputPage * {
		 return new PropertiesPage("color_source", "Type.Colour", parent);
	 }},
	{"Kind.Blank", "color_source",
	 [](QWidget *parent) -> InputPage * {
		 return new BlankPage(parent);
	 }},
	{"Kind.Audio", "wasapi_input_capture",
	 [](QWidget *parent) -> InputPage * {
		 return new PropertiesPage("wasapi_input_capture", "Type.Audio", parent);
	 }},
	{"Kind.Browser", "browser_source",
	 [](QWidget *parent) -> InputPage * {
		 return new PropertiesPage("browser_source", "Type.Browser", parent);
	 }},
};

} // namespace

InputSelect::InputSelect(QWidget *parent) : InputSelect(nullptr, parent) {}

InputSelect::InputSelect(obs_source_t *input, QWidget *parent)
	: QDialog(parent),
	  existing(input),
	  pageOf(std::size(kinds), nullptr)
{
	setWindowTitle(existing ? text("InputSelect.ChangeTitle").arg(QString::fromUtf8(obs_source_get_name(existing)))
				: text("InputSelect.Title"));
	resize(1000, 620);

	auto *outer = new QVBoxLayout(this);
	auto *body = new QHBoxLayout();
	outer->addLayout(body, 1);

	list = new QListWidget(this);
	list->setFixedWidth(200);
	body->addWidget(list);

	pages = new QStackedWidget(this);
	body->addWidget(pages, 1);

	/* An input that exists has its own source first, edited in place: OK keeps the change and
	 * Cancel puts it back. Every other row makes a new source in place of it (vMix's Change,
	 * owner's backlog B4). */
	if (existing) {
		OBSSource base = baseSource(existing);
		if (StreamPage::isStream(base)) {
			own = new StreamPage(base.Get(), pages);
		} else if (base && std::strcmp(obs_source_get_unversioned_id(base), NdiTab::TypeId) == 0) {
			own = new NdiTab(base.Get(), pages);
		} else if (base && std::strcmp(obs_source_get_unversioned_id(base), OmtTab::TypeId) == 0) {
			own = new OmtTab(base.Get(), pages);
		} else if (base && std::strcmp(obs_source_get_unversioned_id(base), gt::SourceId) == 0) {
			own = new TitlePage(base.Get(), pages);
		} else {
			own = new PropertiesPage(base.Get(), pages);
		}
		pages->addWidget(own);
		connect(own, &InputPage::readyChanged, this, &InputSelect::syncOk);
		auto *item = new QListWidgetItem(text("InputSelect.ThisSource"), list);
		item->setData(Qt::UserRole, -1);
	}

	/* The list is filled here, not when the plugin loads: by now every plugin has
	 * registered its source types. */
	for (size_t i = 0; i < std::size(kinds); i++) {
		if (!obs_get_latest_input_type_id(kinds[i].typeId)) {
			continue;
		}
		auto *item = new QListWidgetItem(text(kinds[i].textKey), list);
		item->setData(Qt::UserRole, (int)i);
	}

	addButtons(outer);

	connect(list, &QListWidget::currentRowChanged, this, [this](int) { showKind(); });
	list->setCurrentRow(0);
}

void InputSelect::addButtons(QVBoxLayout *outer)
{
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	ok = buttons->button(QDialogButtonBox::Ok);
	connect(buttons, &QDialogButtonBox::accepted, this, &InputSelect::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &InputSelect::reject);
	outer->addWidget(buttons);
}

/* A page is made when its row is first shown, so a camera or a browser is not started for
 * a row the operator never opens. */
void InputSelect::showKind()
{
	QListWidgetItem *item = list->currentItem();
	if (!item) {
		return;
	}

	const int row = item->data(Qt::UserRole).toInt();
	if (row < 0) {
		pages->setCurrentWidget(own);
		syncOk();
		return;
	}

	const size_t kind = (size_t)row;
	if (!pageOf[kind]) {
		pageOf[kind] = kinds[kind].make(pages);
		pages->addWidget(pageOf[kind]);
		connect(pageOf[kind], &InputPage::readyChanged, this, &InputSelect::syncOk);
	}
	pages->setCurrentWidget(pageOf[kind]);
	syncOk();
}

InputPage *InputSelect::page() const
{
	return static_cast<InputPage *>(pages->currentWidget());
}

void InputSelect::syncOk()
{
	InputPage *current = page();
	ok->setEnabled(current && current->ready());
}

void InputSelect::accept()
{
	InputPage *current = page();
	if (!current || !current->ready()) {
		return;
	}

	if (current == own) {
		own->apply();
		markProjectChanged();
		QDialog::accept();
		return;
	}

	/* another kind for an input that exists: the page makes only a base, which replaces the old one */
	OBSSource made;
	if (existing) {
		BaseReplacement replacing(existing);
		made = current->create();
	} else {
		made = current->create();
	}
	if (!made) {
		QMessageBox::warning(this, windowTitle(), text("InputSelect.Failed"));
		return;
	}

	if (!existing) {
		emit inputMade(made);
	}
	QDialog::accept();
}

void InputSelect::reject()
{
	if (own) {
		own->revert();
	}
	QDialog::reject();
}

} // namespace desk
