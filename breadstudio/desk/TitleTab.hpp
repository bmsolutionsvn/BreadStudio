#pragma once

#include <obs.hpp>

#include <QWidget>

namespace desk {

class InputSettings;

/* The ⚙ Title tab for a GT title input (gt-title design §4.4): the .gtzip path with Change…, then
 * each field in document order — its text or picture or colour, its style, and its countdown — and
 * the source's warnings. Null when the input's base is not a GT title, so a page is absent for every
 * other kind of input, as the desk's other pages are. */
QWidget *makeTitleTab(obs_source_t *input, InputSettings *window);

} // namespace desk
