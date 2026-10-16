#include "Branding.hpp"

#include <obs.h>

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

} // namespace BreadStudio
