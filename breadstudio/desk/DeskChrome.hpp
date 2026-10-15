#pragma once

namespace desk {

/* Hides the OBS surface the desk does not use (cleanup spec §6/§7): OBS's extra docks, and any
 * dock a loaded plugin adds. Nothing is deleted, and a name that is not found is logged and
 * skipped, never fatal (K8). Applied at load, and again after a scene collection loads, because
 * loading one re-adds plugin docks (K7). UI thread only. */
void applyChrome();

} // namespace desk
