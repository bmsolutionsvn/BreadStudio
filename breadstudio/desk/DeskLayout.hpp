#pragma once

namespace desk {

/* Studio Mode on, OBS's Scenes, Sources and Audio Mixer docks hidden, the desk's docks
 * placed (spec §4.5). Does nothing but log when a dock is not found (spec §6.2). */
/* returns false when a dock the layout needs was not found (nothing is changed then) */
bool applyLayout();

/* applyLayout, the first time the program runs with the desk */
void applyLayoutOnFirstRun();

} // namespace desk
