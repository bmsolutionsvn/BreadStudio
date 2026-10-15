#pragma once

namespace desk {

/* Studio Mode on, OBS's Scenes, Sources and Audio Mixer docks hidden, the desk's docks
 * placed (spec §4.5). Does nothing but log when a dock is not found (spec §6.2). */
/* returns false when a dock the layout needs was not found (nothing is changed then) */
bool applyLayout();

/* applyLayout when the desk's layout version is newer than the one stored in the config */
void applyLayoutOnFirstRun();

/* OBS's own Start Streaming button: the desk's Stream button runs the RTMP destinations instead
 * (outputs spec §4.5). Found by its name; when it is not found, the log says so and it stays. */
void hideObsStreamButton();

/* The menu bar is hidden, so OBS's top-level menus (File with Settings… and Exit, View, Profile,
 * Scene Collection and the rest) are moved into one popup behind a compact ☰ button in a slim
 * top toolbar. Does nothing but log when the main window or its menu bar is not found. */
void addTopMenu();

} // namespace desk
