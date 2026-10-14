# OBS files BreadStudio changes

Every OBS file that is edited, replaced, or added outside `breadstudio/`.
`breadstudio/check-edits.ps1` fails the build when this list and
`git diff <tag in UPSTREAM-TAG>` disagree. During a merge, a conflict should
only ever be in a file on this list.

One file per line, in this exact form: a dash, the path in backticks, the reason.

## Edited

- `.gitignore` - OBS ignores every top-level folder it does not name; this un-ignores breadstudio/
- `plugins/CMakeLists.txt` - two lines that add breadstudio/desk and breadstudio/omt-hx to the build
- `frontend/OBSApp.cpp` - Studio Mode's scene duplication is off by default, so an edit to an input shows wherever the input is

## Replaced

## Added outside breadstudio/
