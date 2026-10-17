# OBS files BreadStudio changes

Every OBS file that is edited, replaced, or added outside `breadstudio/`.
`breadstudio/check-edits.ps1` fails the build when this list and
`git diff <tag in UPSTREAM-TAG>` disagree. During a merge, a conflict should
only ever be in a file on this list.

One file per line, in this exact form: a dash, the path in backticks, the reason.
A path may name a whole tree: it then covers itself and everything under it,
which is how a dependency the fork carries as ordinary files is listed once.

## Edited

- `.gitignore` - OBS ignores every top-level folder it does not name; this un-ignores breadstudio/
- `CMakePresets.json` - adds a `windows-x64-vs2022` preset, for a machine with Visual Studio 2022; OBS's own `windows-x64` preset, and the generator it names, are left as upstream has them
- `plugins/CMakeLists.txt` - four lines that add breadstudio/desk, breadstudio/omt-hx, breadstudio/ndi and breadstudio/srt to the build
- `frontend/OBSApp.cpp` - Studio Mode's scene duplication is off by default, so an edit to an input shows wherever the input is
- `frontend/CMakeLists.txt` - program file is BreadStudio.exe; includes breadstudio/cmake/frontend.cmake
- `frontend/widgets/OBSBasic.cpp` - window title shows BreadStudio and its version
- `frontend/widgets/OBSBasic_SysTray.cpp` - tray tooltip and notification title
- `frontend/dialogs/OBSAbout.cpp` - name, "based on OBS Studio" version line, About text; no request to obsproject.com
- `frontend/widgets/OBSBasic_SceneItems.cpp` - CreateFirstRunSources makes no global audio devices, because the desk takes its microphones and system audio as inputs
- `frontend/forms/OBSBasicSettings.ui` - Settings → Audio's Global Audio Devices group is hidden, because the desk has no global audio devices; its control objects stay for OBSBasicSettings.cpp

## Replaced

- `frontend/forms/images/obs.png` - placeholder BreadStudio mark; written by breadstudio/icons/make-icons.ps1
- `frontend/forms/images/obs_paused.png` - placeholder mark with the paused dot; same script
- `frontend/forms/images/tray_active.png` - placeholder mark with the live dot; same script
- `README.rst` - rewritten to describe BreadStudio instead of OBS Studio

## Added outside breadstudio/

- `.github/workflows/breadstudio-windows.yaml` - the fork's own CI: the repository's checks on every push and pull request

## Deleted

- `.gitmodules` - the fork carries OBS's three dependencies as ordinary files instead of submodules, so a clone builds as it stands and the CI checkout needs no submodule step. `plugins/obs-browser`, `plugins/obs-websocket` and `deps/libdshowcapture/src` (with its own `external/capture-device-support`) are vendored in place.

## Vendored

OBS's three submodules, committed as ordinary files so the repository builds from
a plain clone. The sources are OBS's, unedited, at the commits the pinned tag
pointed at.

- `plugins/obs-browser` - the browser source, at 2.23.4-96-g3f0a2cd
- `plugins/obs-websocket` - the websocket server, at 5.7.4
- `deps/libdshowcapture/src` - the capture library `plugins/win-dshow` builds against, at no-atl-121-g8878638, including its own `external/capture-device-support` submodule at fe963097
