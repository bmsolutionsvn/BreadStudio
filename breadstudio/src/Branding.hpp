#pragma once

#include <QString>

#include <string>

class QMainWindow;

namespace BreadStudio {

inline constexpr const char *kName = "BreadStudio";
inline constexpr const char *kVersion = BREADSTUDIO_VERSION;
inline constexpr const char *kWebsite = "https://bmsolutions.vn";
inline constexpr const char *kSourceUrl = "https://github.com/bmsolutionsvn/BreadStudio";

/* "BreadStudio" or "BreadStudio/<name>". OBS asks for paths such as
 * "obs-studio/global.ini" under %APPDATA%; prefixing them keeps every
 * BreadStudio setting under %APPDATA%\BreadStudio and out of OBS's folder. */
std::string ConfigSubPath(const char *name);

/* "BreadStudio 0.1.0, based on OBS Studio 32.2.2" */
QString DisplayVersion();

/* Text of the About tab. */
QString AboutHtml();

/* Hides the menu entries that lead to OBS's servers and volunteer support,
 * and adds a link to the Bread website at the top of the Help menu. */
void ApplyMainWindowBranding(QMainWindow *window);

} // namespace BreadStudio
