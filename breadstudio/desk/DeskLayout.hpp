#pragma once

class QMenu;

namespace desk {

/* Studio Mode on, OBS's Scenes, Sources and Audio Mixer docks hidden, the desk's docks
 * placed (spec §4.5). Does nothing but log when a dock is not found (spec §6.2). */
/* returns false when a dock the layout needs was not found (nothing is changed then) */
bool applyLayout();

/* applyLayout when the desk's layout version is newer than the one stored in the config */
void applyLayoutOnFirstRun();

/* The desk cannot work without its own two docks, and they are only placed by applyLayout, which
 * runs once per LayoutVersion - so a crash after that version is stored, or a dock the operator
 * lost, leaves the desk with no input bar and no way to get it back (the Docks menu is hidden).
 * This puts them back at every load, and does nothing when they are already docked and visible. */
void ensureDeskDocks();

/* The desk is OBS's Studio Mode: it is switched on at every load, not only when the layout is
 * applied, because the whole desk - Preview, Program, Cut, the tally - needs it (S1 §4.1). Turns
 * it on when it is off; does nothing when it is already on. */
void ensureStudioMode();

/* Whether OBS's Scene Transitions dock is pinned into the desk layout (backlog B3): the desk's
 * footer button reads and sets it, and applyLayout places the dock only while it is set. Stored
 * in the user config's BreadDesk section; the default is off, so the dock starts hidden. */
bool transitionsPinned();
void setTransitionsPinned(bool pinned);

/* Put the Scene Transitions dock where the stored pin says it goes, without saving the setting
 * again. Called at every load, after OBS has restored its own dock state, so the flag always
 * wins. */
void applyTransitionsPin();

/* OBS's own Start Streaming button: the desk's Stream button runs the RTMP destinations instead
 * (outputs spec §4.5). Found by its name; when it is not found, the log says so and it stays. */
void hideObsStreamButton();

/* OBS's own top-level menus (File, Edit, View, Docks, Profile, Scene Collection, Tools, Help)
 * handed to a popup, so the desk can offer them from its footer now the menu bar is hidden.
 * Built once and cached. */
QMenu *obsMainMenu();

} // namespace desk
