# breadstudio/

BreadStudio's own code, inside an OBS Studio fork. Upstream's `README.rst` describes OBS itself.

| Folder | What |
|---|---|
| `desk/` | The desk UI: inputs, mixer, bus meters, multiview |
| `omt-hx/` | OMT-HX sender/receiver |
| `ndi/`, `srt/` | NDI and SRT sources/outputs |
| `gt/` | Graphics and titles |
| `cmake/`, `packaging/`, `shared/`, `windows/`, `icons/` | Build, installer, shared code, platform, assets |

Rules: edit OBS files only when unavoidable, and list each in `UPSTREAM-EDITS.md`
(`check-edits.ps1` fails the build if the list and `git diff` disagree). See `PRODUCT.md`.
