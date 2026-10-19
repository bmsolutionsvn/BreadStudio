#include "TitlePage.hpp"
#include "Desk.hpp"
#include "GtPackage.hpp"
#include "GtSettings.hpp"
#include "InputModel.hpp"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <string>

namespace desk {

TitlePage::TitlePage(QWidget *parent) : InputPage(parent)
{
	build();
	showFile();
}

TitlePage::TitlePage(obs_source_t *base_, QWidget *parent) : InputPage(parent), base(base_)
{
	build();

	OBSDataAutoRelease now = obs_source_get_settings(base);
	before = QString::fromUtf8(obs_data_get_string(now, gt::KeyFile));
	path->setText(before);
	showFile();
}

void TitlePage::build()
{
	auto *layout = new QVBoxLayout(this);

	auto *row = new QHBoxLayout();
	row->addWidget(new QLabel(text("Title.File"), this));
	path = new QLineEdit(this);
	path->setReadOnly(true);
	row->addWidget(path, 1);
	browseButton = new QPushButton(text("Title.Browse"), this);
	row->addWidget(browseButton);
	layout->addLayout(row);
	connect(browseButton, &QPushButton::clicked, this, &TitlePage::browse);

	/* the thumbnail, or the read error in its place; Ignored so neither forces the window
	 * to the picture's size (design §4.1) */
	preview = new QLabel(this);
	preview->setAlignment(Qt::AlignCenter);
	preview->setWordWrap(true);
	preview->setMinimumHeight(200);
	preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	layout->addWidget(preview, 1);
}

void TitlePage::browse()
{
	const QString chosen =
		QFileDialog::getOpenFileName(this, text("Title.File"), path->text(), text("Title.Filter"));
	if (chosen.isEmpty()) {
		return;
	}
	path->setText(chosen);
	showFile();
}

/* reads the chosen path: the thumbnail on success, the reason in place of it on failure */
void TitlePage::showFile()
{
	const std::string file = path->text().toUtf8().constData();

	gt::GtPackage package;
	std::string error;
	if (!file.empty() && gt::readGtFile(file, package, error)) {
		thumbnail = QPixmap();
		if (!package.thumbnail.empty()) {
			thumbnail.loadFromData(package.thumbnail.data(), (int)package.thumbnail.size());
		}
		preview->setText(QString());
		scalePreview();
		readable = true;
	} else {
		thumbnail = QPixmap();
		preview->setPixmap(QPixmap());
		preview->setText(file.empty() ? QString() : text("Title.NotTitle").arg(QString::fromUtf8(error.c_str())));
		readable = false;
	}
	emit readyChanged();
}

/* the thumbnail kept inside the page, whatever its size */
void TitlePage::scalePreview()
{
	if (thumbnail.isNull()) {
		return;
	}
	preview->setPixmap(thumbnail.scaled(preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void TitlePage::resizeEvent(QResizeEvent *event)
{
	InputPage::resizeEvent(event);
	scalePreview();
}

bool TitlePage::ready() const
{
	return readable;
}

OBSSource TitlePage::create()
{
	if (base || !readable) {
		return OBSSource();
	}
	const QByteArray file = path->text().toUtf8();
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, gt::KeyFile, file.constData());
	const std::string name = QFileInfo(path->text()).completeBaseName().toUtf8().constData();
	return createInput(gt::SourceId, name.c_str(), settings);
}

void TitlePage::apply()
{
	if (!base) {
		return;
	}
	const std::string wanted = path->text().toUtf8().constData();
	OBSDataAutoRelease now = obs_source_get_settings(base);
	if (std::string(obs_data_get_string(now, gt::KeyFile)) == wanted) {
		return;
	}
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, gt::KeyFile, wanted.c_str());
	obs_source_update(base, settings);
}

void TitlePage::revert()
{
	if (!base) {
		return;
	}
	const std::string old = before.toUtf8().constData();
	OBSDataAutoRelease now = obs_source_get_settings(base);
	if (std::string(obs_data_get_string(now, gt::KeyFile)) == old) {
		return;
	}
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, gt::KeyFile, old.c_str());
	obs_source_update(base, settings);
}

} // namespace desk
