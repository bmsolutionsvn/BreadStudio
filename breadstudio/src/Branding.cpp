#include "Branding.hpp"

#include <obs.h>

#include <QAction>
#include <QDesktopServices>
#include <QMainWindow>
#include <QMenu>
#include <QUrl>

namespace BreadStudio {

std::string ConfigSubPath(const char *name)
{
	std::string path = kName;

	if (name && *name) {
		path += "/";
		path += name;
	}

	return path;
}

QString DisplayVersion()
{
	return QStringLiteral("%1 %2, based on OBS Studio %3")
		.arg(QString::fromUtf8(kName), QString::fromUtf8(kVersion),
		     QString::fromUtf8(obs_get_version_string()));
}

QString AboutHtml()
{
	return QStringLiteral("<p>%1 is built on <a href=\"https://obsproject.com\">OBS Studio</a> %2, the work of "
			      "the OBS Project and its contributors (see Authors).</p>"
			      "<p>%1 is not affiliated with or endorsed by the OBS Project.</p>"
			      "<p>Licensed under the GNU General Public License, version 2 or later (see License). "
			      "Source code: <a href=\"%3\">%3</a></p>")
		.arg(QString::fromUtf8(kName), QString::fromUtf8(obs_get_version_string()),
		     QString::fromUtf8(kSourceUrl));
}

void ApplyMainWindowBranding(QMainWindow *window)
{
	/* Object names from frontend/forms/OBSBasic.ui. */
	static const char *const hidden[] = {
		"actionHelpPortal",
		"actionWebsite",
		"actionDiscord",
		"actionReleaseNotes",
		"actionUploadCurrentLog",
		"actionUploadLastLog",
		"actionUploadLastCrashLog",
		"actionCheckForUpdates",
		"actionRepair",
	};

	for (const char *name : hidden) {
		if (QAction *action = window->findChild<QAction *>(QString::fromUtf8(name))) {
			action->setVisible(false);
		}
	}

	QMenu *help = window->findChild<QMenu *>(QStringLiteral("menuBasic_MainMenu_Help"));
	if (!help) {
		return;
	}

	QAction *website = new QAction(QStringLiteral("bmsolutions.vn"), help);
	QObject::connect(website, &QAction::triggered, help,
			 [] { QDesktopServices::openUrl(QUrl(QString::fromUtf8(kWebsite))); });
	help->insertAction(help->actions().value(0), website);
}

} // namespace BreadStudio
