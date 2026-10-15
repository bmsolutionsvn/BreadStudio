#pragma once

#include <obs.hpp>

#include <QString>
#include <QWidget>

#include <vector>

namespace desk {

class InputSettings;

/* one page of the Input Settings window */
struct InputSettingPage {
	QString title;
	QWidget *widget = nullptr; /* owned by the window's stack */
};

/* The pages an input gets, in order (inputs spec §3.1). A page whose backing is absent is
 * left out. `window` is the shell, for the pages that must close or reload it. */
std::vector<InputSettingPage> pagesFor(obs_source_t *input, InputSettings *window);

} // namespace desk
