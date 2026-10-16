# OBS files BreadStudio changes

Every OBS file that is edited, replaced, or added outside `breadstudio/`.
`breadstudio/check-edits.ps1` fails the build when this list and
`git diff <tag in UPSTREAM-TAG>` disagree. During a merge, a conflict should
only ever be in a file on this list.

One file per line, in this exact form: a dash, the path in backticks, the reason.

## Edited

- `.gitignore` - OBS ignores every top-level folder it does not name; this un-ignores breadstudio/
- `plugins/CMakeLists.txt` - four lines that add breadstudio/desk, breadstudio/omt-hx, breadstudio/ndi and breadstudio/srt to the build
- `frontend/OBSApp.cpp` - Studio Mode's scene duplication is off by default, so an edit to an input shows wherever the input is
- `frontend/CMakeLists.txt` - program file is BreadStudio.exe; includes breadstudio/cmake/frontend.cmake
- `frontend/widgets/OBSBasic.cpp` - calls ApplyMainWindowBranding; window title shows BreadStudio and its version
- `frontend/widgets/OBSBasic_SysTray.cpp` - tray tooltip and notification title
- `frontend/dialogs/OBSAbout.cpp` - name, "based on OBS Studio" version line, About text; no request to obsproject.com

## Replaced

- `frontend/forms/images/obs.png` - placeholder BreadStudio mark; written by breadstudio/icons/make-icons.ps1
- `frontend/forms/images/obs_paused.png` - placeholder mark with the paused dot; same script
- `frontend/forms/images/tray_active.png` - placeholder mark with the live dot; same script

## Added outside breadstudio/
