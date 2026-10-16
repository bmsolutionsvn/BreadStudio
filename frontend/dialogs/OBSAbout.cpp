#include "OBSAbout.hpp"
#include <Branding.hpp>

#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>

#include "moc_OBSAbout.cpp"

extern bool steam;

OBSAbout::OBSAbout(QWidget *parent) : QDialog(parent), ui(new Ui::OBSAbout)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->setupUi(this);

	QString bitness;

	if (sizeof(void *) == 4) {
		bitness = " (32 bit)";
	} else if (sizeof(void *) == 8) {
		bitness = " (64 bit)";
	}

	ui->name->setText(BreadStudio::kName);
	ui->version->setText(BreadStudio::DisplayVersion() + bitness);

	ui->contribute->setText(QTStr("About.Contribute"));

	if (steam) {
		delete ui->donate;
	} else {
		ui->donate->setText("&nbsp;&nbsp;<a href='https://obsproject.com/contribute'>" + QTStr("About.Donate") +
				    "</a>");
		ui->donate->setTextInteractionFlags(Qt::TextBrowserInteraction);
		ui->donate->setOpenExternalLinks(true);
	}

	ui->getInvolved->setText("&nbsp;&nbsp;<a href='https://obsproject.com/developer-contributing'>" +
				 QTStr("About.GetInvolved") + "</a>");
	ui->getInvolved->setTextInteractionFlags(Qt::TextBrowserInteraction);
	ui->getInvolved->setOpenExternalLinks(true);

	ui->about->setText("<a href='#'>" + QTStr("About") + "</a>");
	ui->authors->setText("<a href='#'>" + QTStr("About.Authors") + "</a>");
	ui->license->setText("<a href='#'>" + QTStr("About.License") + "</a>");

	ui->name->setProperty("class", "text-heading");
	ui->version->setProperty("class", "text-large");
	ui->about->setProperty("class", "bg-base");
	ui->authors->setProperty("class", "bg-base");
	ui->license->setProperty("class", "bg-base");
	ui->info->setProperty("class", "");

	connect(ui->about, &ClickableLabel::clicked, this, &OBSAbout::ShowAbout);
	connect(ui->authors, &ClickableLabel::clicked, this, &OBSAbout::ShowAuthors);
	connect(ui->license, &ClickableLabel::clicked, this, &OBSAbout::ShowLicense);

	ShowAbout();
}

void OBSAbout::ShowAbout()
{
	ui->textBrowser->setHtml(BreadStudio::AboutHtml());
}

void OBSAbout::ShowAuthors()
{
	std::string path;
	QString error = QTStr("About.Error").arg("https://github.com/obsproject/obs-studio/blob/master/AUTHORS");

#ifdef __APPLE__
	if (!GetDataFilePath("AUTHORS", path)) {
#else
	if (!GetDataFilePath("authors/AUTHORS", path)) {
#endif
		ui->textBrowser->setPlainText(error);
		return;
	}

	ui->textBrowser->setPlainText(QString::fromStdString(path));

	BPtr<char> text = os_quick_read_utf8_file(path.c_str());

	if (!text || !*text) {
		ui->textBrowser->setPlainText(error);
		return;
	}

	ui->textBrowser->setPlainText(QT_UTF8(text));
}

void OBSAbout::ShowLicense()
{
	std::string path;
	QString error = QTStr("About.Error").arg("https://github.com/obsproject/obs-studio/blob/master/COPYING");

	if (!GetDataFilePath("license/gplv2.txt", path)) {
		ui->textBrowser->setPlainText(error);
		return;
	}

	BPtr<char> text = os_quick_read_utf8_file(path.c_str());

	if (!text || !*text) {
		ui->textBrowser->setPlainText(error);
		return;
	}

	ui->textBrowser->setPlainText(QT_UTF8(text));
}
