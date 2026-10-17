#pragma once

class QMenu;
class QWidget;

namespace desk {

/* Studio Mode on, OBS's Scenes, Sources and Audio Mixer docks hidden, the desk's docks
 * placed (spec §4.5). Does nothing but log when a dock is not found (spec §6.2). */
/* returns false when a dock the layout needs was not found (nothing is changed then) */
bool applyLayout();

/* applyLayout when the desk's layout version is newer than the one stored in the config */
void applyLayoutOnFirstRun();

/* The desk cannot work without its own two docks, and applyLayout runs only once per
 * LayoutVersion. The desk's arrangement is the desk's, not the config's, so this places both
 * docks at every load and deliberately overrides whatever dock state OBS stored: a crash after
 * that version is stored, or a dock the operator lost, cannot leave the desk with no input bar
 * and no way to get it back (the Docks menu is hidden). */
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

/* The window title, from OBS's own: its base text with a dot appended while the open Project has
 * changes that are not written (P9). Reads the current title, strips a trailing dot, and re-adds
 * it when Project::changed(). Called at load, whenever the changed state flips, and after the
 * desk's own New, Open, Save and Save As. */
void applyProjectTitle();

/* React to the window's own title changing, so the Project's dot survives every re-title OBS makes
 * (launch, scene-collection switch, profile switch, Studio Mode toggle): OBS sets the title itself
 * and would otherwise wipe the dot after the desk has added it. Doing this from the window's
 * windowTitleChanged signal, rather than after each event, does not depend on event ordering.
 * Connects once; call at module load. */
void followTitleBar();

/* The close prompt: the same Save / Discard / Cancel set the New, Open and Recent actions use,
 * for the app being closed with unsaved work (P9/§4.4). True when the close may go on; false
 * when the operator cancelled, so the caller must keep the app open. */
bool confirmProjectClose(QWidget *parent);

} // namespace desk
