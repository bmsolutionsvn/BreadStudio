# BreadStudio S0 — Fork Baseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A public fork of OBS Studio 32.2.2 that builds on this machine, carries the BreadStudio name and look, installs from its own installer beside OBS, and has a proven routine for merging the next OBS release.

**Architecture:** OBS's code is kept as it is and merged forward release by release. Our own code lives in a new top-level `breadstudio/` folder; OBS files are edited only where nothing else works, and every such file is listed in `breadstudio/UPSTREAM-EDITS.md`, which a script enforces. Branding that can be applied to the built files (interface text, the updater switch) is applied by the packaging script, not by editing OBS sources.

**Tech Stack:** C++17/Qt 6 (OBS's frontend), CMake presets from OBS, Visual Studio 2026, Windows PowerShell 5.1 scripts, Inno Setup 6, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-12-breadstudio-s0-fork-baseline-design.md` (workspace root; Task 2 copies it into the repo). Parent roadmap: `docs/superpowers/specs/2026-10-12-broadcast-roadmap-design.md`.

## Global Constraints

- Base: OBS Studio tag `32.2.2` (commit `ba2f32bdf791005443988a4955e963663e16b1ed`). All line numbers below are at that tag.
- Repo: `bmsolutionsvn/BreadStudio`, a public fork of `obsproject/obs-studio`. Local path `F:\Coding\BreadMedia\BreadStudio`. Branch `main`. Remote `upstream` = `https://github.com/obsproject/obs-studio.git`.
- **Never rebase or force-push `main`.** Upstream releases are merged.
- **Anything that publishes is an owner gate:** creating the GitHub repo, every `git push`, changing repo settings. A worker stops at a step marked **OWNER GATE** and waits.
- Every OBS file that is edited, added outside `breadstudio/`, or replaced is listed in `breadstudio/UPSTREAM-EDITS.md` in the same commit. `breadstudio/check-edits.ps1` must pass before every commit from Task 2 on.
- Do not rename `obs.dll`, `obs-frontend-api.dll`, any plugin, or change `LIBOBS_API_VER`.
- Do not edit any file under `frontend/data/locale/` or `plugins/*/data/locale/`.
- Do not delete or edit files under `.github/` that OBS owns. Only add `breadstudio-windows.yaml`.
- Product name in user-visible text is exactly `BreadStudio`. The names "OBS", "OBS Studio" and the OBS logo may appear only in: the "based on OBS Studio" line, the About text, the Authors and License tabs, the phrases "OBS Classic" and "OBS Project", and log files.
- C++ follows OBS style: tabs, 120 columns, braces on every `if`.
- PowerShell scripts must run on Windows PowerShell 5.1 (`powershell.exe`): no `&&`, `||`, `?:`, `??`. Files are written as UTF-8 without BOM using `System.IO.File`, never `Set-Content`/`Out-File`, wherever the content is a file OBS reads.
- All builds run from a Visual Studio 2026 developer shell: `& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64`.
- Commit messages: `breadstudio: <what changed>`, imperative, lowercase after the prefix.
- Windows x64 only. macOS and Linux code is not built, not tested and not deleted.

## Review Focus

1. **OBS is installed and running on the same PC.** BreadStudio must start without an "already running" warning and must not write into `%APPDATA%\obs-studio`. Pinned by Task 4 Step 6 and SMOKE items 5 and 6.
2. **A translation file with a BOM, CRLF line endings or non-ASCII text** (Vietnamese) goes through the product rename. Only the product name may change; every other byte stays. Pinned by Task 7 Step 1.
3. **A translation key that contains "OBS"** (for example `Remux.OBSRecording`) must not be renamed, or OBS can no longer find the string. Pinned by Task 7 Step 1.
4. **The edits list goes stale in either direction:** an OBS file edited but not listed, or listed but no longer edited (after a merge makes an edit unnecessary), or an OBS file deleted. Pinned by Task 2 Step 1.
5. **A BreadStudio release tag changes what `git describe` returns**, which OBS's build uses for the library version that plugins check. The build must always pass the pinned OBS version explicitly. Pinned by the version assertion in Task 7 Step 5.

---

## File Structure

New files (all ours):

| Path | Responsibility |
|---|---|
| `breadstudio/VERSION` | BreadStudio's own version, one line. Read by CMake and the packaging script. |
| `breadstudio/UPSTREAM-TAG` | The OBS tag `main` is based on, one line. Read by the build, the check and the packaging script. |
| `breadstudio/UPSTREAM-EDITS.md` | Every OBS file edited, added or replaced, with the reason. |
| `breadstudio/check-edits.ps1` | Fails when the list and `git diff <tag>` disagree. |
| `breadstudio/check-edits.test.ps1` | Tests for the above, on a throwaway git repo. |
| `breadstudio/build.ps1` | Configure and build with the pinned OBS version. |
| `breadstudio/cmake/frontend.cmake` | Adds our sources, definitions and Windows resource to OBS's frontend target. |
| `breadstudio/src/Branding.hpp`, `Branding.cpp` | The name, version, settings sub-path, About text, and the main-window menu changes. |
| `breadstudio/windows/breadstudio.rc.in` | Windows version resource and icon. |
| `breadstudio/icons/make-icons.ps1` | Draws the placeholder mark and writes the `.ico` and the three PNGs. |
| `breadstudio/icons/breadstudio.ico` | Generated. |
| `breadstudio/packaging/Rename-Product.ps1` | Replaces the OBS product name in installed translation files. |
| `breadstudio/packaging/Rename-Product.test.ps1` | Tests for the above. |
| `breadstudio/packaging/package.ps1` | Stages the install tree, applies branding to it, builds the installer. |
| `breadstudio/packaging/installer.iss` | Inno Setup script. |
| `breadstudio/docs/SMOKE.md`, `MERGING.md` | Manual checklist and merge routine. |
| `breadstudio/docs/…-design.md`, `…-fork-baseline.md` | Copies of the spec and this plan. |
| `frontend/data/themes/Yami_Bread.ovt` | The Bread theme (new file in an OBS folder, so OBS's own data copy installs it). |
| `.github/workflows/breadstudio-windows.yaml` | Our CI. |

OBS files edited (each is listed in `UPSTREAM-EDITS.md` when edited):

| Path | Edit |
|---|---|
| `frontend/CMakeLists.txt` | Program file name; one `include()` line. |
| `frontend/OBSApp.cpp` | Settings folder prefix; crash-log upload off. |
| `frontend/widgets/OBSBasic.cpp` | Menu branding call; window title; plugin folder. |
| `frontend/widgets/OBSBasic_SysTray.cpp` | Tray tooltip and notification title. |
| `frontend/dialogs/OBSAbout.cpp` | Name, version and About text; no call to OBS's server. |
| `frontend/utility/platform-windows.cpp` | Single-instance mutex name. |
| `frontend/cmake/templates/ui-config.h.in` | Default theme id. |
| `frontend/forms/images/obs.png`, `obs_paused.png`, `tray_active.png` | Replaced with the placeholder mark. |

---

### Task 1: Create the fork and prove the unmodified build

**Files:**
- Create: the repo at `F:\Coding\BreadMedia\BreadStudio`

**Interfaces:**
- Produces: a local clone on branch `main` at tag `32.2.2`, remote `upstream`, and a working `build_x64/` from an unmodified build.

- [ ] **Step 1: Check prerequisites**

Run in PowerShell:

```powershell
gh api orgs/bmsolutionsvn --jq .login
Test-Path "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1"
Test-Path "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0"
(Get-PSDrive F).Free / 1GB
```

Expected: `bmsolutionsvn`, `True`, `True`, and at least 40 (GB free). If the first command fails, stop: the GitHub account cannot see the organisation.

- [ ] **Step 2: Install Inno Setup**

```powershell
winget install -e --id JRSoftware.InnoSetup --accept-source-agreements --accept-package-agreements
Test-Path "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
```

Expected: `True`.

- [ ] **Step 3: OWNER GATE — create the public fork**

This creates a public repository. Do not run it without the owner's go-ahead in this session.

```powershell
gh repo fork obsproject/obs-studio --org bmsolutionsvn --fork-name BreadStudio --clone=false
gh repo view bmsolutionsvn/BreadStudio --json isFork,visibility,parent --jq '"\(.isFork) \(.visibility) \(.parent.owner.login)/\(.parent.name)"'
```

Expected: `true PUBLIC obsproject/obs-studio`.

- [ ] **Step 4: Clone and set up the branch**

```powershell
git clone https://github.com/bmsolutionsvn/BreadStudio.git F:\Coding\BreadMedia\BreadStudio
cd F:\Coding\BreadMedia\BreadStudio
git config core.longpaths true
git remote add upstream https://github.com/obsproject/obs-studio.git
git fetch upstream --tags
git checkout -b main 32.2.2
git submodule update --init --recursive
git log -1 --format=%H
```

Expected last line: `ba2f32bdf791005443988a4955e963663e16b1ed`.

- [ ] **Step 5: Build OBS unmodified**

In a Visual Studio 2026 developer shell (see Global Constraints), from the repo root:

```powershell
cmake --preset windows-x64 -DOBS_VERSION_OVERRIDE=32.2.2 -DENABLE_WHATSNEW=OFF
cmake --build --preset windows-x64 --config RelWithDebInfo --parallel
```

The first configure downloads OBS's prebuilt dependencies (several GB). Expected: both commands exit 0 and `build_x64\rundir\RelWithDebInfo\bin\64bit\obs64.exe` exists.

If configure fails on the generator or SDK version, stop and report the error text. Do not change the preset.

- [ ] **Step 6: Run it once**

```powershell
Start-Process -WorkingDirectory build_x64\rundir\RelWithDebInfo\bin\64bit build_x64\rundir\RelWithDebInfo\bin\64bit\obs64.exe
```

Expected: OBS Studio 32.2.2 opens. Close it. This is the baseline every later task is compared against.

- [ ] **Step 7: OWNER GATE — publish the branch and make it the default**

```powershell
git push -u origin main
gh repo edit bmsolutionsvn/BreadStudio --default-branch main --description "BreadStudio: a live production switcher, based on OBS Studio. Not affiliated with the OBS Project."
```

Nothing is committed in this task; `main` equals the `32.2.2` tag.

---

### Task 2: The edits list and its check

**Files:**
- Create: `breadstudio/VERSION`, `breadstudio/UPSTREAM-TAG`, `breadstudio/UPSTREAM-EDITS.md`, `breadstudio/check-edits.ps1`, `breadstudio/check-edits.test.ps1`, `breadstudio/build.ps1`
- Create: `breadstudio/docs/2026-10-12-breadstudio-s0-fork-baseline-design.md`, `breadstudio/docs/2026-10-12-breadstudio-s0-fork-baseline.md` (copies)

**Interfaces:**
- Produces: `breadstudio/check-edits.ps1 [-RepoRoot <path>]`, exit 0 when the list matches, exit 1 otherwise. List line format: a line starting with ``- `path/from/repo/root` `` followed by the reason. `breadstudio/build.ps1 [-Configuration RelWithDebInfo]`. `breadstudio/VERSION` holds `0.1.0`; `breadstudio/UPSTREAM-TAG` holds `32.2.2`.

- [ ] **Step 1: Write the failing test**

Create `breadstudio/check-edits.test.ps1`:

```powershell
$ErrorActionPreference = 'Stop'

$script = Join-Path $PSScriptRoot 'check-edits.ps1'
$repo = Join-Path ([System.IO.Path]::GetTempPath()) ('bs-check-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $repo | Out-Null

function G {
    git -C $repo @args | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "git $args failed" }
}

function Expect([int] $Code, [string] $Name) {
    powershell -NoProfile -ExecutionPolicy Bypass -File $script -RepoRoot $repo | Out-Null
    if ($LASTEXITCODE -ne $Code) { throw "${Name}: expected exit $Code, got $LASTEXITCODE" }
    Write-Host "ok - $Name"
}

$list = Join-Path $repo 'breadstudio/UPSTREAM-EDITS.md'

try {
    G init -q
    G config user.email test@example.invalid
    G config user.name test
    Set-Content (Join-Path $repo 'a.c') 'a'
    Set-Content (Join-Path $repo 'b.c') 'b'
    G add -A
    G commit -qm base
    G tag 1.0.0

    New-Item -ItemType Directory (Join-Path $repo 'breadstudio') | Out-Null
    Set-Content (Join-Path $repo 'breadstudio/UPSTREAM-TAG') '1.0.0'
    Set-Content $list '# Edits'
    G add -A
    G commit -qm ours
    Expect 0 'only breadstudio/ changed'

    Set-Content (Join-Path $repo 'a.c') 'a2'
    G commit -qam edit
    Expect 1 'edited upstream file is not listed'

    Add-Content $list '- `a.c` - test edit'
    G commit -qam list
    Expect 0 'edited upstream file is listed'

    Add-Content $list '- `b.c` - never edited'
    G commit -qam stale
    Expect 1 'listed file is not changed'

    G rm -q b.c
    G commit -qm delete
    Expect 0 'deleted upstream file is listed'

    Set-Content (Join-Path $repo 'c.c') 'c'
    G add -A
    G commit -qm add
    Expect 1 'file added outside breadstudio/ is not listed'

    Add-Content $list '- `c.c` - our new file in an upstream folder'
    G commit -qam list-add
    Expect 0 'added file is listed'

    Set-Content (Join-Path $repo 'breadstudio/UPSTREAM-TAG') '9.9.9'
    G commit -qam badtag
    Expect 1 'pinned tag does not exist'
} finally {
    Remove-Item -Recurse -Force $repo
}

Write-Host 'check-edits: all tests passed'
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.test.ps1`
Expected: FAIL. PowerShell reports that `check-edits.ps1` does not exist, and the first `Expect` throws.

- [ ] **Step 3: Write the script**

Create `breadstudio/check-edits.ps1`:

```powershell
# Fails when breadstudio/UPSTREAM-EDITS.md and the files changed since the pinned OBS tag disagree.
param([string] $RepoRoot = (Resolve-Path "$PSScriptRoot/..").Path)

$tag = (Get-Content (Join-Path $RepoRoot 'breadstudio/UPSTREAM-TAG') -TotalCount 1).Trim()

git -C $RepoRoot rev-parse --verify --quiet "refs/tags/$tag" | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "Tag '$tag' from breadstudio/UPSTREAM-TAG is not in this repo. Run: git fetch upstream --tags"
    exit 1
}

# --no-renames: a moved OBS file must show up as one deletion and one addition.
$changed = @(git -C $RepoRoot diff --name-only --no-renames $tag HEAD -- . ':(exclude)breadstudio')
if ($LASTEXITCODE -ne 0) {
    Write-Host 'git diff failed'
    exit 1
}

$listed = @(Get-Content (Join-Path $RepoRoot 'breadstudio/UPSTREAM-EDITS.md') |
        ForEach-Object { if ($_ -match '^- `([^`]+)`') { $Matches[1] } })

$unlisted = @($changed | Where-Object { $listed -notcontains $_ })
$stale = @($listed | Where-Object { $changed -notcontains $_ })

if ($unlisted.Count -gt 0) {
    Write-Host 'Changed since the pinned tag but not in breadstudio/UPSTREAM-EDITS.md:'
    $unlisted | ForEach-Object { Write-Host "  $_" }
}
if ($stale.Count -gt 0) {
    Write-Host 'In breadstudio/UPSTREAM-EDITS.md but not changed since the pinned tag:'
    $stale | ForEach-Object { Write-Host "  $_" }
}
if ($unlisted.Count -gt 0 -or $stale.Count -gt 0) { exit 1 }

Write-Host "UPSTREAM-EDITS.md matches the $($changed.Count) file(s) changed since $tag."
exit 0
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.test.ps1`
Expected: eight `ok - …` lines, then `check-edits: all tests passed`.

- [ ] **Step 5: Add the version files, the list, the build script and the docs**

`breadstudio/VERSION` (one line, no trailing text):

```
0.1.0
```

`breadstudio/UPSTREAM-TAG`:

```
32.2.2
```

`breadstudio/UPSTREAM-EDITS.md`:

```markdown
# OBS files BreadStudio changes

Every OBS file that is edited, replaced, or added outside `breadstudio/`.
`breadstudio/check-edits.ps1` fails the build when this list and
`git diff <tag in UPSTREAM-TAG>` disagree. During a merge, a conflict should
only ever be in a file on this list.

One file per line, in this exact form: a dash, the path in backticks, the reason.

## Edited

## Replaced

## Added outside breadstudio/
```

`breadstudio/build.ps1`:

```powershell
# Configure and build. Run from a Visual Studio 2026 developer shell.
param([string] $Configuration = 'RelWithDebInfo')

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/..").Path
$tag = (Get-Content (Join-Path $root 'breadstudio/UPSTREAM-TAG') -TotalCount 1).Trim()

Push-Location $root
try {
    # OBS_VERSION_OVERRIDE: without it OBS reads its version from `git describe`,
    # which a BreadStudio release tag would change. Plugins check this version.
    cmake --preset windows-x64 "-DOBS_VERSION_OVERRIDE=$tag" -DENABLE_WHATSNEW=OFF
    if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }

    cmake --build --preset windows-x64 --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) { throw 'cmake build failed' }
} finally {
    Pop-Location
}
```

Copy the two documents:

```powershell
New-Item -ItemType Directory -Force breadstudio\docs | Out-Null
Copy-Item F:\Coding\BreadMedia\docs\superpowers\specs\2026-10-12-breadstudio-s0-fork-baseline-design.md breadstudio\docs\
Copy-Item F:\Coding\BreadMedia\docs\superpowers\plans\2026-10-12-breadstudio-s0-fork-baseline.md breadstudio\docs\
```

- [ ] **Step 6: Run the real check and commit**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git add breadstudio
git commit -m "breadstudio: add the upstream edits list, its check and the build script"
```

Expected from the check: `UPSTREAM-EDITS.md matches the 0 file(s) changed since 32.2.2.`

---

### Task 3: Name, version, About and menus

OBS's frontend has no unit-test harness, so this task and Task 4 are verified by building and by looking at the running program. Each check below names exactly what to look at.

**Files:**
- Create: `breadstudio/src/Branding.hpp`, `breadstudio/src/Branding.cpp`, `breadstudio/cmake/frontend.cmake`, `breadstudio/windows/breadstudio.rc.in`
- Modify: `frontend/CMakeLists.txt` (last line, 115)
- Modify: `frontend/widgets/OBSBasic.cpp:256` and `:2130-2135`
- Modify: `frontend/widgets/OBSBasic_SysTray.cpp:33` and `:105`
- Modify: `frontend/dialogs/OBSAbout.cpp:30-32`, `:65-78`, `:80-128`
- Modify: `breadstudio/UPSTREAM-EDITS.md`

**Interfaces:**
- Consumes: `breadstudio/VERSION` (Task 2). `breadstudio/icons/breadstudio.ico` does not exist yet; this task points the resource at OBS's icon and Task 5 switches it.
- Produces, in namespace `BreadStudio` (`#include <Branding.hpp>`):
  - `const char *kName` = `"BreadStudio"`, `const char *kVersion` = contents of `VERSION`
  - `std::string ConfigSubPath(const char *name)` — `"BreadStudio"` or `"BreadStudio/<name>"`
  - `QString DisplayVersion()` — `"BreadStudio 0.1.0, based on OBS Studio 32.2.2"`
  - `QString AboutHtml()`
  - `void ApplyMainWindowBranding(QMainWindow *window)`
- Produces: the program file is `BreadStudio.exe`.

- [ ] **Step 1: Write `breadstudio/src/Branding.hpp`**

```cpp
#pragma once

#include <QString>

#include <string>

class QMainWindow;

namespace BreadStudio {

inline constexpr const char *kName = "BreadStudio";
inline constexpr const char *kVersion = BREADSTUDIO_VERSION;
inline constexpr const char *kWebsite = "https://bmsolutions.vn";
inline constexpr const char *kSourceUrl = "https://github.com/bmsolutionsvn/BreadStudio";

/* "BreadStudio" or "BreadStudio/<name>". OBS asks for paths such as
 * "obs-studio/global.ini" under %APPDATA%; prefixing them keeps every
 * BreadStudio setting under %APPDATA%\BreadStudio and out of OBS's folder. */
std::string ConfigSubPath(const char *name);

/* "BreadStudio 0.1.0, based on OBS Studio 32.2.2" */
QString DisplayVersion();

/* Text of the About tab. */
QString AboutHtml();

/* Hides the menu entries that lead to OBS's servers and volunteer support,
 * and adds a link to the Bread website at the top of the Help menu. */
void ApplyMainWindowBranding(QMainWindow *window);

} // namespace BreadStudio
```

- [ ] **Step 2: Write `breadstudio/src/Branding.cpp`**

```cpp
#include "Branding.hpp"

#include <obs.h>

#include <QAction>
#include <QDesktopServices>
#include <QMainWindow>
#include <QMenu>
#include <QUrl>

namespace BreadStudio {

std::string ConfigSubPath(const char *name)
{
	std::string path = kName;

	if (name && *name) {
		path += "/";
		path += name;
	}

	return path;
}

QString DisplayVersion()
{
	return QStringLiteral("%1 %2, based on OBS Studio %3")
		.arg(QString::fromUtf8(kName), QString::fromUtf8(kVersion),
		     QString::fromUtf8(obs_get_version_string()));
}

QString AboutHtml()
{
	return QStringLiteral("<p>%1 is built on <a href=\"https://obsproject.com\">OBS Studio</a> %2, the work of "
			      "the OBS Project and its contributors (see Authors).</p>"
			      "<p>%1 is not affiliated with or endorsed by the OBS Project.</p>"
			      "<p>Licensed under the GNU General Public License, version 2 or later (see License). "
			      "Source code: <a href=\"%3\">%3</a></p>")
		.arg(QString::fromUtf8(kName), QString::fromUtf8(obs_get_version_string()),
		     QString::fromUtf8(kSourceUrl));
}

void ApplyMainWindowBranding(QMainWindow *window)
{
	/* Object names from frontend/forms/OBSBasic.ui. */
	static const char *const hidden[] = {
		"actionHelpPortal",
		"actionWebsite",
		"actionDiscord",
		"actionReleaseNotes",
		"actionUploadCurrentLog",
		"actionUploadLastLog",
		"actionUploadLastCrashLog",
		"actionCheckForUpdates",
		"actionRepair",
	};

	for (const char *name : hidden) {
		if (QAction *action = window->findChild<QAction *>(QString::fromUtf8(name))) {
			action->setVisible(false);
		}
	}

	QMenu *help = window->findChild<QMenu *>(QStringLiteral("menuBasic_MainMenu_Help"));
	if (!help) {
		return;
	}

	QAction *website = new QAction(QStringLiteral("bmsolutions.vn"), help);
	QObject::connect(website, &QAction::triggered, help,
			 [] { QDesktopServices::openUrl(QUrl(QString::fromUtf8(kWebsite))); });
	help->insertAction(help->actions().value(0), website);
}

} // namespace BreadStudio
```

- [ ] **Step 3: Write `breadstudio/windows/breadstudio.rc.in`**

The icon line points at OBS's icon for now; Task 5 changes it.

```
IDI_ICON1 ICON DISCARDABLE "${CMAKE_SOURCE_DIR}/frontend/cmake/windows/obs-studio.ico"

1 VERSIONINFO
FILEVERSION ${OBS_VERSION_MAJOR},${OBS_VERSION_MINOR},${OBS_VERSION_PATCH},0
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904B0"
    BEGIN
      VALUE "CompanyName", "BM Solutions"
      VALUE "FileDescription", "BreadStudio"
      VALUE "FileVersion", "${BREADSTUDIO_VERSION}"
      VALUE "InternalName", "BreadStudio"
      VALUE "OriginalFilename", "BreadStudio.exe"
      VALUE "ProductName", "BreadStudio"
      VALUE "ProductVersion", "${BREADSTUDIO_VERSION}"
      VALUE "Comments", "Based on OBS Studio ${OBS_VERSION_CANONICAL}"
      VALUE "LegalCopyright", "${OBS_LEGAL_COPYRIGHT}"
    END
  END

  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x0409, 0x04B0
  END
END
```

- [ ] **Step 4: Write `breadstudio/cmake/frontend.cmake`**

```cmake
# Included from the last line of frontend/CMakeLists.txt.

file(STRINGS "${CMAKE_SOURCE_DIR}/breadstudio/VERSION" BREADSTUDIO_VERSION LIMIT_COUNT 1)

target_sources(
  obs-studio
  PRIVATE "${CMAKE_SOURCE_DIR}/breadstudio/src/Branding.cpp" "${CMAKE_SOURCE_DIR}/breadstudio/src/Branding.hpp"
)
target_include_directories(obs-studio PRIVATE "${CMAKE_SOURCE_DIR}/breadstudio/src")
target_compile_definitions(obs-studio PRIVATE BREADSTUDIO_VERSION="${BREADSTUDIO_VERSION}")

if(OS_WINDOWS)
  # frontend/cmake/os-windows.cmake has already generated obs.rc from OBS's
  # template. Generating ours to the same path replaces it, so OBS's
  # obs.rc.in stays unedited.
  configure_file("${CMAKE_SOURCE_DIR}/breadstudio/windows/breadstudio.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/obs.rc")
endif()
```

- [ ] **Step 5: Edit `frontend/CMakeLists.txt`**

Replace the last line (115):

```cmake
set_target_properties_obs(obs-studio PROPERTIES FOLDER frontend OUTPUT_NAME "$<IF:$<PLATFORM_ID:Windows>,obs64,obs>")
```

with:

```cmake
set_target_properties_obs(obs-studio PROPERTIES FOLDER frontend OUTPUT_NAME "$<IF:$<PLATFORM_ID:Windows>,BreadStudio,obs>")

include("${CMAKE_SOURCE_DIR}/breadstudio/cmake/frontend.cmake")
```

- [ ] **Step 6: Edit `frontend/widgets/OBSBasic.cpp`**

Add `#include <Branding.hpp>` on a new line directly below the file's first `#include` line.

In `OBSBasic::OBSBasic` (starts at line 232), directly below `ui->setupUi(this);` (line 256), add:

```cpp
	BreadStudio::ApplyMainWindowBranding(this);
```

In the window-title function (around line 2130), replace:

```cpp
	name << "OBS ";
	if (previewProgramMode) {
		name << "Studio ";
	}

	name << App()->GetVersionString(false);
```

with:

```cpp
	name << BreadStudio::kName << " " << BreadStudio::kVersion;
	if (previewProgramMode) {
		name << " (Studio Mode)";
	}
```

Leave line 457 (`name << "OBS " << App()->GetVersionString();`) alone: it writes the OBS version into the log file, which is what a log should say.

- [ ] **Step 7: Edit `frontend/widgets/OBSBasic_SysTray.cpp`**

Add `#include <Branding.hpp>` directly below the file's first `#include` line.

Line 33, replace `trayIcon->setToolTip("OBS Studio");` with:

```cpp
	trayIcon->setToolTip(BreadStudio::kName);
```

Line 105, replace `trayIcon->showMessage("OBS Studio", text, icon, 10000);` with:

```cpp
		trayIcon->showMessage(BreadStudio::kName, text, icon, 10000);
```

- [ ] **Step 8: Edit `frontend/dialogs/OBSAbout.cpp`**

Add `#include <Branding.hpp>` directly below `#include "OBSAbout.hpp"`.

Replace (lines 30-32):

```cpp
	QString ver = obs_get_version_string();

	ui->version->setText(ver + bitness);
```

with:

```cpp
	ui->name->setText(BreadStudio::kName);
	ui->version->setText(BreadStudio::DisplayVersion() + bitness);
```

Replace the block that starts `QPointer<OBSAbout> about(this);` and ends with the closing brace of the `else { ShowAbout(); }` (lines 65-78) with:

```cpp
	ShowAbout();
```

Replace the whole body of `void OBSAbout::ShowAbout()` (lines 80-128) with:

```cpp
void OBSAbout::ShowAbout()
{
	ui->textBrowser->setHtml(BreadStudio::AboutHtml());
}
```

The donate and "get involved" links to obsproject.com higher in the constructor stay: they credit and support the project BreadStudio is built on.

If the compiler then reports `json11`, `RemoteTextThread` or `using namespace json11` as unused, leave the includes in place; removing them only widens the diff.

- [ ] **Step 9: Record the edits**

In `breadstudio/UPSTREAM-EDITS.md`, under `## Edited`, add:

```markdown
- `frontend/CMakeLists.txt` - program file is BreadStudio.exe; includes breadstudio/cmake/frontend.cmake
- `frontend/widgets/OBSBasic.cpp` - calls ApplyMainWindowBranding; window title shows BreadStudio and its version
- `frontend/widgets/OBSBasic_SysTray.cpp` - tray tooltip and notification title
- `frontend/dialogs/OBSAbout.cpp` - name, "based on OBS Studio" version line, About text; no request to obsproject.com
```

- [ ] **Step 10: Build and verify**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\build.ps1
Test-Path build_x64\rundir\RelWithDebInfo\bin\64bit\BreadStudio.exe
(Get-Item build_x64\rundir\RelWithDebInfo\bin\64bit\BreadStudio.exe).VersionInfo | Format-List ProductName, ProductVersion, Comments
```

Expected: `True`; `ProductName : BreadStudio`, `ProductVersion : 0.1.0`, `Comments : Based on OBS Studio 32.2.2`.

Start `BreadStudio.exe` from that folder and check, in order:

1. The title bar starts with `BreadStudio 0.1.0`.
2. Help menu: the first entry is `bmsolutions.vn` and it opens that site. There is no Help Portal, Website, Discord, Release Notes, Check For Updates or Check File Integrity entry. Help → Log Files has no Upload entries.
3. Help → About: the heading is `BreadStudio`, the version line is `BreadStudio 0.1.0, based on OBS Studio 32.2.2 (64 bit)`, the About tab shows the three paragraphs, and the Authors and License tabs still show OBS's files.
4. The tray icon tooltip reads `BreadStudio`.

- [ ] **Step 11: Commit**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git add -A
git commit -m "breadstudio: brand the program name, title, tray, About and Help menu"
```

Expected from the check: `UPSTREAM-EDITS.md matches the 4 file(s) changed since 32.2.2.`

---

### Task 4: Keep BreadStudio out of OBS's folders

**Files:**
- Modify: `frontend/OBSApp.cpp:1142` and `:1725-1758`
- Modify: `frontend/widgets/OBSBasic.cpp:149`
- Modify: `frontend/utility/platform-windows.cpp:308`
- Modify: `breadstudio/UPSTREAM-EDITS.md`

**Interfaces:**
- Consumes: `BreadStudio::ConfigSubPath(const char *)` from Task 3.
- Produces: settings live under `%APPDATA%\BreadStudio\obs-studio\`; third-party plugins load from `%ProgramData%\BreadStudio\plugins\<name>\bin\64bit` and from the install folder's `obs-plugins\64bit`; the single-instance mutex is `BreadStudioCore`.

Why a prefix and not a rename: OBS names its settings folder as the literal `"obs-studio/…"` in about 45 places across 12 files. Every one of them goes through `GetAppConfigPath` or `GetAppConfigPathPtr`, so prefixing there moves everything with two small edits.

- [ ] **Step 1: Edit `GetAppConfigPath` and `GetAppConfigPathPtr` in `frontend/OBSApp.cpp`**

Add `#include <Branding.hpp>` directly below the file's first `#include` line.

In `GetAppConfigPath` (line 1725), both occurrences of:

```cpp
		return os_get_config_path(path, size, name);
```

(one inside the `else` of the portable-mode branch, one after `#else`) become:

```cpp
		return os_get_config_path(path, size, BreadStudio::ConfigSubPath(name).c_str());
```

Keep each line's existing indentation.

In `GetAppConfigPathPtr` (line 1742), both occurrences of:

```cpp
		return os_get_config_path_ptr(name);
```

become:

```cpp
		return os_get_config_path_ptr(BreadStudio::ConfigSubPath(name).c_str());
```

Do not touch the `if (portable_mode)` branches.

- [ ] **Step 2: Switch crash-log upload off in `frontend/OBSApp.cpp`**

In `OBSApp::checkForUncleanShutdown` (line 1139), replace line 1142:

```cpp
	bool hasNewCrashLog = crashHandler_->hasNewCrashLog();
```

with:

```cpp
	/* BreadStudio: crash logs are never offered for upload to OBS's servers. */
	bool hasNewCrashLog = false;
```

With `false`, the crash dialog (`handleUncleanShutdown`, line 91) shows no upload checkbox and `uploadLastCrashLog` is never called.

- [ ] **Step 3: Edit the plugin folder in `frontend/widgets/OBSBasic.cpp`**

In `AddExtraModulePaths` (line 115), replace line 149:

```cpp
	int ret = GetProgramDataPath(base_module_dir, sizeof(base_module_dir), "obs-studio/plugins/%module%");
```

with:

```cpp
	int ret = GetProgramDataPath(base_module_dir, sizeof(base_module_dir), "BreadStudio/plugins/%module%");
```

Only the `#if defined(_WIN32)` line. Leave the macOS and Linux lines.

- [ ] **Step 4: Edit the mutex name in `frontend/utility/platform-windows.cpp`**

In `CheckIfAlreadyRunning`, replace line 308:

```cpp
		name = "OBSStudioCore";
```

with:

```cpp
		name = "BreadStudioCore";
```

- [ ] **Step 5: Look for code that bypasses the two functions**

```powershell
git grep -nE "os_get_config_path(_ptr)?\(" -- frontend plugins libobs shared deps | Select-String "obs-studio"
```

Expected: no output. If a line appears, that code writes into OBS's own settings folder. Stop and report the file and line; do not guess at a fix.

- [ ] **Step 6: Build and verify isolation**

Note the state of OBS's folder first, then build and run:

```powershell
$before = Get-ChildItem "$env:APPDATA\obs-studio" -Recurse -File -ErrorAction SilentlyContinue | Measure-Object LastWriteTimeUtc -Maximum
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\build.ps1
Start-Process -Wait -WorkingDirectory build_x64\rundir\RelWithDebInfo\bin\64bit build_x64\rundir\RelWithDebInfo\bin\64bit\BreadStudio.exe
```

In the program: add a Color Source to the scene, then close BreadStudio. Then:

```powershell
Test-Path "$env:APPDATA\BreadStudio\obs-studio\global.ini"
Test-Path "$env:APPDATA\BreadStudio\obs-studio\basic\scenes"
$after = Get-ChildItem "$env:APPDATA\obs-studio" -Recurse -File -ErrorAction SilentlyContinue | Measure-Object LastWriteTimeUtc -Maximum
"$($before.Count) $($before.Maximum) -> $($after.Count) $($after.Maximum)"
```

Expected: `True`, `True`, and the last line shows the same count and the same time on both sides of the arrow (or `0  -> 0 ` when OBS was never installed).

If OBS is installed on this machine, also start OBS, leave it running, and start BreadStudio: BreadStudio must open with no "already running" warning.

- [ ] **Step 7: Record the edits and commit**

In `breadstudio/UPSTREAM-EDITS.md`, under `## Edited`, add:

```markdown
- `frontend/OBSApp.cpp` - settings paths are prefixed with BreadStudio/; crash-log upload offer is off
- `frontend/utility/platform-windows.cpp` - single-instance mutex is BreadStudioCore, so OBS and BreadStudio can run together
```

and change the existing `frontend/widgets/OBSBasic.cpp` line to:

```markdown
- `frontend/widgets/OBSBasic.cpp` - calls ApplyMainWindowBranding; window title shows BreadStudio and its version; third-party plugins load from ProgramData\BreadStudio\plugins
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git add -A
git commit -m "breadstudio: keep settings, plugins and the instance lock separate from OBS"
```

Expected from the check: `UPSTREAM-EDITS.md matches the 6 file(s) changed since 32.2.2.`

---

### Task 5: Placeholder icon

**Files:**
- Create: `breadstudio/icons/make-icons.ps1`, `breadstudio/icons/breadstudio.ico`
- Modify: `breadstudio/windows/breadstudio.rc.in` (line 1)
- Replace: `frontend/forms/images/obs.png`, `obs_paused.png`, `tray_active.png` (each 256×256)
- Modify: `breadstudio/UPSTREAM-EDITS.md`

**Interfaces:**
- Produces: `breadstudio/icons/breadstudio.ico` with sizes 16, 24, 32, 48, 64, 128, 256.

The mark is the one in `bm-landing/public/favicon.svg`: a near-black rounded square with "BM" in cyan monospace. It is a placeholder; the owner supplies the real icon later and only this script's output changes.

- [ ] **Step 1: Write `breadstudio/icons/make-icons.ps1`**

```powershell
# Draws the placeholder BreadStudio mark and writes the .ico and the three PNGs OBS's interface uses.
param([string] $RepoRoot = (Resolve-Path "$PSScriptRoot/../..").Path)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function New-MarkBitmap([int] $Size, [string] $Badge) {
    $bmp = New-Object System.Drawing.Bitmap $Size, $Size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $g.Clear([System.Drawing.Color]::Transparent)

    # Same proportions as the favicon: corner radius 6 and font size 15 on a 32 unit square.
    $d = [single]($Size * 12 / 32)
    $plate = New-Object System.Drawing.Drawing2D.GraphicsPath
    $plate.AddArc(0, 0, $d, $d, 180, 90)
    $plate.AddArc($Size - $d, 0, $d, $d, 270, 90)
    $plate.AddArc($Size - $d, $Size - $d, $d, $d, 0, 90)
    $plate.AddArc(0, $Size - $d, $d, $d, 90, 90)
    $plate.CloseFigure()
    $g.FillPath((New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#08090B'))), $plate)

    $font = New-Object System.Drawing.Font 'Consolas', ([single]($Size * 15 / 32)), ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
    $format = New-Object System.Drawing.StringFormat
    $format.Alignment = 'Center'
    $format.LineAlignment = 'Center'
    $cyan = New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#22D3EE'))
    $g.DrawString('BM', $font, $cyan, (New-Object System.Drawing.RectangleF 0, 0, $Size, $Size), $format)

    if ($Badge) {
        # State dot, bottom right: amber when paused, red when live.
        $dot = [single]($Size * 0.34)
        $g.FillEllipse((New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml($Badge))), $Size - $dot - 2, $Size - $dot - 2, $dot, $dot)
    }

    $g.Dispose()
    return $bmp
}

function Save-Png([string] $Path, [string] $Badge) {
    $bmp = New-MarkBitmap 256 $Badge
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Save-Ico([string] $Path, [int[]] $Sizes) {
    $pngs = foreach ($size in $Sizes) {
        $bmp = New-MarkBitmap $size $null
        $stream = New-Object System.IO.MemoryStream
        $bmp.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        , $stream.ToArray()
    }

    $out = New-Object System.IO.MemoryStream
    $w = New-Object System.IO.BinaryWriter $out
    $w.Write([uint16]0)
    $w.Write([uint16]1)
    $w.Write([uint16]$Sizes.Count)

    $offset = 6 + 16 * $Sizes.Count
    for ($i = 0; $i -lt $Sizes.Count; $i++) {
        $dim = $Sizes[$i]
        if ($dim -ge 256) { $dim = 0 }   # 0 means 256 in the ICO directory
        $w.Write([byte]$dim)
        $w.Write([byte]$dim)
        $w.Write([byte]0)
        $w.Write([byte]0)
        $w.Write([uint16]1)
        $w.Write([uint16]32)
        $w.Write([uint32]$pngs[$i].Length)
        $w.Write([uint32]$offset)
        $offset += $pngs[$i].Length
    }
    foreach ($png in $pngs) { $w.Write([byte[]]$png) }
    $w.Flush()

    [System.IO.File]::WriteAllBytes($Path, $out.ToArray())
}

$images = Join-Path $RepoRoot 'frontend/forms/images'
Save-Png (Join-Path $images 'obs.png') $null
Save-Png (Join-Path $images 'obs_paused.png') '#F59E0B'
Save-Png (Join-Path $images 'tray_active.png') '#EF4444'

$ico = Join-Path $RepoRoot 'breadstudio/icons/breadstudio.ico'
Save-Ico $ico @(16, 24, 32, 48, 64, 128, 256)

# Self-check: the files are what the build expects.
foreach ($name in 'obs.png', 'obs_paused.png', 'tray_active.png') {
    $image = [System.Drawing.Image]::FromFile((Join-Path $images $name))
    $ok = ($image.Width -eq 256 -and $image.Height -eq 256)
    $image.Dispose()
    if (-not $ok) { throw "$name is not 256x256" }
}
$header = [System.IO.File]::ReadAllBytes($ico)[0..5]
if (($header -join ',') -ne '0,0,1,0,7,0') { throw "breadstudio.ico has a bad header: $($header -join ',')" }

Write-Host 'Wrote breadstudio.ico and 3 PNGs.'
```

- [ ] **Step 2: Run it**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\icons\make-icons.ps1`
Expected: `Wrote breadstudio.ico and 3 PNGs.`

Open `frontend\forms\images\obs.png` and confirm it shows a dark rounded square with a cyan "BM", with nothing cut off.

- [ ] **Step 3: Point the resource at the new icon**

In `breadstudio/windows/breadstudio.rc.in`, replace line 1 with:

```
IDI_ICON1 ICON DISCARDABLE "${CMAKE_SOURCE_DIR}/breadstudio/icons/breadstudio.ico"
```

- [ ] **Step 4: Record, build and verify**

In `breadstudio/UPSTREAM-EDITS.md`, under `## Replaced`, add:

```markdown
- `frontend/forms/images/obs.png` - placeholder BreadStudio mark; written by breadstudio/icons/make-icons.ps1
- `frontend/forms/images/obs_paused.png` - placeholder mark with the paused dot; same script
- `frontend/forms/images/tray_active.png` - placeholder mark with the live dot; same script
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\build.ps1
```

Expected: exit 0. If the resource compiler stops with `RC2176 old DIB`, it has rejected PNG-compressed small icon sizes; stop and report it rather than changing the script.

Start `BreadStudio.exe` and confirm: the "BM" mark is the window icon, the taskbar icon, the tray icon, and the picture in Help → About. The OBS logo appears nowhere.

- [ ] **Step 5: Commit**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git add -A
git commit -m "breadstudio: replace the OBS logo with a placeholder mark"
```

Expected from the check: `… matches the 9 file(s) changed since 32.2.2.`

---

### Task 6: The Bread theme

**Files:**
- Create: `frontend/data/themes/Yami_Bread.ovt`
- Modify: `frontend/cmake/templates/ui-config.h.in:16`
- Modify: `breadstudio/UPSTREAM-EDITS.md`

**Interfaces:**
- Produces: theme id `vn.bmsolutions.Yami.Bread`, the default theme.

Scope: colours only. OBS's themes set their own font (`Open Sans`); changing type means loading fonts in code and is left to S1, where the desk panels are designed.

- [ ] **Step 1: Write `frontend/data/themes/Yami_Bread.ovt`**

```css
@OBSThemeMeta {
    name: 'Bread';
    id: 'vn.bmsolutions.Yami.Bread';
    extends: 'com.obsproject.Yami';
    author: 'BM Solutions';
    dark: 'true';
}

@OBSThemeVars {
    /* Cyan Blueprint, from the workspace DESIGN.md: a black plate and one cyan accent. */
    --grey1: #3A3F48;
    --grey2: #2E323A;
    --grey3: #23272E;
    --grey4: #1B1E24;
    --grey5: #16191F; /* surface-raised */
    --grey6: #0E1013; /* surface */
    --grey7: #08090B; /* plate-black */
    --grey8: #060709; /* rail-black */

    /* Yami draws white text on --primary (selected rows, checked buttons).
       Blueprint cyan #22D3EE cannot carry white text, so fills use a darker
       cyan and the brand cyan is kept for highlights and focus. */
    --primary: #0E7490;
    --primary_light: #0891B2;
    --primary_lighter: #22D3EE; /* blueprint-cyan */
    --primary_dark: #155E75;
    --primary_darker: #164E63;

    --text: #F5F7FA; /* paper-white */
    --text_light: #F5F7FA;
    --text_muted: #9B9D9F; /* text-muted, 62% paper-white on the plate */
}
```

- [ ] **Step 2: Make it the default**

In `frontend/cmake/templates/ui-config.h.in`, replace line 16:

```c
#define DEFAULT_THEME "com.obsproject.Yami.Original"
```

with:

```c
#define DEFAULT_THEME "vn.bmsolutions.Yami.Bread"
```

- [ ] **Step 3: Record, build and verify**

In `breadstudio/UPSTREAM-EDITS.md`, under `## Edited`, add:

```markdown
- `frontend/cmake/templates/ui-config.h.in` - the default theme is the Bread theme
```

and under `## Added outside breadstudio/`:

```markdown
- `frontend/data/themes/Yami_Bread.ovt` - the Bread theme; it sits here so OBS's own data copy installs it
```

Remove the settings from earlier runs so the default applies (at this point the folder holds only test settings from Tasks 3 and 4), then build and run:

```powershell
Remove-Item -Recurse -Force "$env:APPDATA\BreadStudio" -ErrorAction SilentlyContinue
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\build.ps1
Start-Process -WorkingDirectory build_x64\rundir\RelWithDebInfo\bin\64bit build_x64\rundir\RelWithDebInfo\bin\64bit\BreadStudio.exe
```

Check:

1. The window is near-black with cyan highlights, not OBS's blue-grey.
2. Settings → Appearance lists `Bread` and it is selected. `Yami` and the other OBS themes are still listed and still apply when chosen.
3. Text on selected list rows and on checked buttons (for example Start Recording while recording) is readable.

- [ ] **Step 4: Commit**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git add -A
git commit -m "breadstudio: add the Bread theme and make it the default"
```

Expected from the check: `… matches the 11 file(s) changed since 32.2.2.`

---

### Task 7: Product name in interface text, and the installer

**Files:**
- Create: `breadstudio/packaging/Rename-Product.ps1`, `breadstudio/packaging/Rename-Product.test.ps1`, `breadstudio/packaging/package.ps1`, `breadstudio/packaging/installer.iss`

**Interfaces:**
- Consumes: `breadstudio/VERSION`, `breadstudio/UPSTREAM-TAG`, `breadstudio/icons/breadstudio.ico`, a finished build in `build_x64/`.
- Produces: `Rename-ProductInText([string]) -> [string]` and `Rename-ProductInFile([string]) -> [bool]` (dot-source the script); `Rename-Product.ps1 -Path <files>`; `package.ps1 [-Configuration]`, which writes `build_x64\BreadStudio-<version>-windows-x64.exe`.

Why text is renamed here and not in the source: "OBS" appears in 77 translation files of the interface and in plugin translations. Editing them would conflict on every merge. The packaging step rewrites the installed copies instead, so a build run from `build_x64\rundir` still shows OBS's wording in some dialogs. Only an installed build is fully branded, and only an installed build is smoke-tested.

- [ ] **Step 1: Write the failing test**

Create `breadstudio/packaging/Rename-Product.test.ps1`:

```powershell
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Rename-Product.ps1')

function Assert-Equal($Expected, $Actual, [string] $Name) {
    if ($Expected -cne $Actual) { throw "${Name}`n  expected: $Expected`n  actual:   $Actual" }
    Write-Host "ok - $Name"
}

Assert-Equal 'A="BreadStudio is still currently active."' (Rename-ProductInText 'A="OBS is still currently active."') 'bare OBS'
Assert-Equal 'A="BreadStudio Crash Detected"' (Rename-ProductInText 'A="OBS Studio Crash Detected"') 'OBS Studio becomes one name'
Assert-Equal 'A="Select BreadStudio Recording..."' (Rename-ProductInText 'A="Select OBS Recording..."') 'OBS before another word'
Assert-Equal 'Remux.OBSRecording="BreadStudio Recording"' (Rename-ProductInText 'Remux.OBSRecording="OBS Recording"') 'the key is never changed'
Assert-Equal 'A="Import from OBS Classic"' (Rename-ProductInText 'A="Import from OBS Classic"') 'OBS Classic is kept'
Assert-Equal 'A="the OBS Project"' (Rename-ProductInText 'A="the OBS Project"') 'OBS Project is kept'
Assert-Equal 'A="report to the OBSProject."' (Rename-ProductInText 'A="report to the OBSProject."') 'OBSProject is kept'
Assert-Equal 'A="uses obs-websocket"' (Rename-ProductInText 'A="uses obs-websocket"') 'lower-case obs is kept'
Assert-Equal 'no equals sign OBS' (Rename-ProductInText 'no equals sign OBS') 'a line without = is kept'
Assert-Equal "A=`"BreadStudio`"`r`nB=`"x`"`r`n" (Rename-ProductInText "A=`"OBS`"`r`nB=`"x`"`r`n") 'CRLF line endings are kept'
Assert-Equal "A=`"BreadStudio`"`nB=`"BreadStudio`"`n" (Rename-ProductInText "A=`"OBS Studio`"`nB=`"OBS`"`n") 'every line is handled'

# Files: a BOM, CRLF and Vietnamese text survive; an untouched file is not rewritten.
$utf8 = New-Object System.Text.UTF8Encoding $false
$dir = Join-Path ([System.IO.Path]::GetTempPath()) ('bs-rename-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $dir | Out-Null
try {
    $d = [string][char]0x0111   # Vietnamese d with stroke
    $bom = [byte[]](0xEF, 0xBB, 0xBF)

    $withBom = Join-Path $dir 'vi-VN.ini'
    [System.IO.File]::WriteAllBytes($withBom, $bom + $utf8.GetBytes("A=`"OBS ${d}ang ch`"`r`n"))
    Assert-Equal $true (Rename-ProductInFile $withBom) 'a changed file reports true'
    $expected = ($bom + $utf8.GetBytes("A=`"BreadStudio ${d}ang ch`"`r`n")) -join ','
    Assert-Equal $expected ([System.IO.File]::ReadAllBytes($withBom) -join ',') 'BOM, CRLF and non-ASCII bytes are kept'

    $clean = Join-Path $dir 'clean.ini'
    [System.IO.File]::WriteAllBytes($clean, $utf8.GetBytes("A=`"${d}`"`n"))
    $stamp = (Get-Item $clean).LastWriteTimeUtc
    Start-Sleep -Milliseconds 50
    Assert-Equal $false (Rename-ProductInFile $clean) 'an unchanged file reports false'
    Assert-Equal $stamp (Get-Item $clean).LastWriteTimeUtc 'an unchanged file is not rewritten'
} finally {
    Remove-Item -Recurse -Force $dir
}

Write-Host 'Rename-Product: all tests passed'
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\packaging\Rename-Product.test.ps1`
Expected: FAIL, because `Rename-Product.ps1` does not exist.

- [ ] **Step 3: Write `breadstudio/packaging/Rename-Product.ps1`**

```powershell
# Replaces the OBS product name with BreadStudio in the values of installed translation (.ini) files.
# Keys are never changed. "OBS Classic", "OBS Project" and "OBSProject" name other things and are kept.
# Dot-source the file to get the two functions; pass -Path to rewrite files.
param([string[]] $Path)

$script:ProductPattern = [regex]'\bOBS(?: Studio)?\b(?! (?:Classic|Project)\b)'
$script:ValuePattern = [regex]'(?m)^([^=\r\n]*=)([^\r\n]*)'
$script:Utf8NoBom = New-Object System.Text.UTF8Encoding $false

function Rename-ProductInText([string] $Text) {
    return $script:ValuePattern.Replace($Text, [System.Text.RegularExpressions.MatchEvaluator] {
            param($match)
            $match.Groups[1].Value + $script:ProductPattern.Replace($match.Groups[2].Value, 'BreadStudio')
        })
}

function Rename-ProductInFile([string] $File) {
    $bytes = [System.IO.File]::ReadAllBytes($File)
    $skip = 0
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { $skip = 3 }

    $old = $script:Utf8NoBom.GetString($bytes, $skip, $bytes.Length - $skip)
    $new = Rename-ProductInText $old
    if ($new -ceq $old) { return $false }

    $out = New-Object System.IO.MemoryStream
    $out.Write($bytes, 0, $skip)
    $body = $script:Utf8NoBom.GetBytes($new)
    $out.Write($body, 0, $body.Length)
    [System.IO.File]::WriteAllBytes($File, $out.ToArray())
    return $true
}

if ($Path) {
    $count = 0
    foreach ($file in $Path) {
        if (Rename-ProductInFile $file) { $count++ }
    }
    Write-Host "Renamed the product in $count of $($Path.Count) file(s)."
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\packaging\Rename-Product.test.ps1`
Expected: fifteen `ok - …` lines, then `Rename-Product: all tests passed`.

- [ ] **Step 5: Write `breadstudio/packaging/package.ps1`**

```powershell
# Stages the install tree, applies the branding that is done on built files, and builds the installer.
# Run from a Visual Studio 2026 developer shell after breadstudio\build.ps1.
param([string] $Configuration = 'RelWithDebInfo')

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$build = Join-Path $root 'build_x64'
$stage = Join-Path $build 'install'
$version = (Get-Content (Join-Path $root 'breadstudio/VERSION') -TotalCount 1).Trim()
$tag = (Get-Content (Join-Path $root 'breadstudio/UPSTREAM-TAG') -TotalCount 1).Trim()

if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
cmake --install $build --prefix $stage --config $Configuration
if ($LASTEXITCODE -ne 0) { throw 'cmake --install failed' }

$exe = Join-Path $stage 'bin/64bit/BreadStudio.exe'
if (-not (Test-Path $exe)) { throw "Missing $exe" }

# The library version plugins check must be the pinned OBS version, whatever tags this repo has.
$comments = (Get-Item $exe).VersionInfo.Comments
if ($comments -ne "Based on OBS Studio $tag") {
    throw "BreadStudio.exe says '$comments'; expected 'Based on OBS Studio $tag'. Build with breadstudio\build.ps1."
}

# OBS switches its updater off when this file sits two levels above the program (frontend/obs-main.cpp).
New-Item -ItemType File -Force (Join-Path $stage 'disable_updater') | Out-Null
Get-ChildItem $stage -Recurse -Filter 'updater.exe' | Remove-Item -Force

$locales = @(Get-ChildItem (Join-Path $stage 'data') -Recurse -Filter '*.ini' |
        Where-Object { $_.Directory.Name -eq 'locale' } | ForEach-Object { $_.FullName })
if ($locales.Count -eq 0) { throw 'No translation files found in the staged tree' }
& (Join-Path $PSScriptRoot 'Rename-Product.ps1') -Path $locales

$english = Join-Path $stage 'data/obs-studio/locale/en-US.ini'
$left = @(Select-String -Path $english -CaseSensitive -Pattern '=.*\bOBS(?: Studio)?\b(?! (?:Classic|Project)\b)')
if ($left.Count -gt 0) { throw "The OBS name is still in en-US.ini:`n$($left -join "`n")" }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$redist = & $vswhere -latest -products * -find 'VC\Redist\MSVC\**\vc_redist.x64.exe' | Select-Object -Last 1
if (-not $redist) { throw 'vc_redist.x64.exe not found in the Visual Studio install' }

$iscc = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
if (-not (Test-Path $iscc)) { throw "Inno Setup not found at $iscc" }

& $iscc "/DAppVersion=$version" "/DStageDir=$stage" "/DRedist=$redist" "/DOutputDir=$build" (Join-Path $PSScriptRoot 'installer.iss')
if ($LASTEXITCODE -ne 0) { throw 'ISCC failed' }

Write-Host "Installer: $(Join-Path $build "BreadStudio-$version-windows-x64.exe")"
```

- [ ] **Step 6: Write `breadstudio/packaging/installer.iss`**

```ini
; BreadStudio installer. Built by package.ps1, which passes AppVersion, StageDir, Redist and OutputDir.
; It does not register a virtual camera or a Vulkan capture layer: OBS owns those system-wide
; registrations on a PC that has OBS, and BreadStudio must not take them over.

[Setup]
AppId={{8B1F6C2E-4D3A-4F7B-9C55-2A6E7D1B0F43}
AppName=BreadStudio
AppVersion={#AppVersion}
AppPublisher=BM Solutions
AppPublisherURL=https://bmsolutions.vn
DefaultDirName={autopf}\BreadStudio
DefaultGroupName=BreadStudio
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
LicenseFile={#StageDir}\data\obs-studio\license\gplv2.txt
OutputDir={#OutputDir}
OutputBaseFilename=BreadStudio-{#AppVersion}-windows-x64
SetupIconFile=..\icons\breadstudio.ico
UninstallDisplayIcon={app}\bin\64bit\BreadStudio.exe
AppMutex=BreadStudioCore
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#Redist}"; DestDir: "{tmp}"; DestName: "vc_redist.x64.exe"; Flags: deleteafterinstall

[Icons]
Name: "{group}\BreadStudio"; Filename: "{app}\bin\64bit\BreadStudio.exe"; WorkingDir: "{app}\bin\64bit"
Name: "{autodesktop}\BreadStudio"; Filename: "{app}\bin\64bit\BreadStudio.exe"; WorkingDir: "{app}\bin\64bit"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing the Microsoft Visual C++ runtime..."; Flags: waituntilterminated
Filename: "{app}\bin\64bit\BreadStudio.exe"; WorkingDir: "{app}\bin\64bit"; Description: "Start BreadStudio"; Flags: postinstall nowait skipifsilent
```

Settings in `%APPDATA%\BreadStudio` are deliberately not removed on uninstall (there is no `[UninstallDelete]` section).

- [ ] **Step 7: Build the installer**

In a Visual Studio 2026 developer shell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\packaging\package.ps1
```

Expected: `Renamed the product in N of M file(s).` with N greater than 0, then `Installer: …\build_x64\BreadStudio-0.1.0-windows-x64.exe`.

If it stops with `The OBS name is still in en-US.ini`, the printed lines show wording the pattern does not cover. Add a failing case for that wording to `Rename-Product.test.ps1`, fix the pattern in both `Rename-Product.ps1` and the `Select-String` line of `package.ps1`, and rerun the tests before rerunning this step.

- [ ] **Step 8: Install and verify**

Run the installer (accept the SmartScreen warning: the build is unsigned). Then:

```powershell
Test-Path "C:\Program Files\BreadStudio\bin\64bit\BreadStudio.exe"
Test-Path "C:\Program Files\BreadStudio\disable_updater"
Test-Path "C:\Program Files\BreadStudio\bin\64bit\updater.exe"
```

Expected: `True`, `True`, `False`.

Start BreadStudio from the Start menu and, with the interface in English, open File → Exit while a recording is running: the confirmation must say `BreadStudio is still currently active`, not `OBS`.

- [ ] **Step 9: Commit**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git add -A
git commit -m "breadstudio: add the packaging script, product rename and installer"
```

---

### Task 8: CI

**Files:**
- Create: `.github/workflows/breadstudio-windows.yaml`
- Modify: `breadstudio/UPSTREAM-EDITS.md`

**Interfaces:**
- Consumes: `breadstudio/check-edits.ps1`, both test scripts, `breadstudio/build.ps1`, `breadstudio/packaging/package.ps1`.
- Produces: an installer artifact named `BreadStudio-windows-x64` on every push and pull request to `main`.

- [ ] **Step 1: Write `.github/workflows/breadstudio-windows.yaml`**

The runner image and the pinned action commits are the ones OBS's own `build-project.yaml` uses at 32.2.2.

```yaml
name: BreadStudio Windows
on:
  push:
    branches: [main]
  pull_request:
    branches: [main]
permissions:
  contents: read
concurrency:
  group: breadstudio-windows-${{ github.ref }}
  cancel-in-progress: true
jobs:
  windows:
    name: Windows x64
    runs-on: windows-2025-vs2026
    defaults:
      run:
        shell: powershell
    steps:
      - uses: actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd # v6.0.2
        with:
          submodules: recursive
          fetch-depth: 0
      - name: Fetch OBS tags
        run: git fetch --tags https://github.com/obsproject/obs-studio.git
      - name: Script tests
        run: |
          ./breadstudio/check-edits.test.ps1
          ./breadstudio/packaging/Rename-Product.test.ps1
      - name: Check the upstream edits list
        run: ./breadstudio/check-edits.ps1
      - name: Build
        run: ./breadstudio/build.ps1
      - name: Install Inno Setup
        run: choco install innosetup --no-progress -y
      - name: Package
        run: ./breadstudio/packaging/package.ps1
      - uses: actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a # v7.0.1
        with:
          name: BreadStudio-windows-x64
          path: build_x64/BreadStudio-*-windows-x64.exe
          if-no-files-found: error
```

- [ ] **Step 2: Record and commit**

In `breadstudio/UPSTREAM-EDITS.md`, under `## Added outside breadstudio/`, add:

```markdown
- `.github/workflows/breadstudio-windows.yaml` - our CI; GitHub only reads workflows from this folder
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git add -A
git commit -m "breadstudio: add the Windows CI workflow"
```

- [ ] **Step 3: OWNER GATE — switch OBS's workflows off, then push**

OBS's workflows build macOS and Linux, sign, and publish OBS releases. They are disabled in the repo's settings, not deleted, so they never conflict in a merge. Disable them before the first push that could trigger them.

```powershell
gh api -X PUT repos/bmsolutionsvn/BreadStudio/actions/permissions -F enabled=true -f allowed_actions=all
foreach ($w in 'analyze-project.yaml', 'build-project.yaml', 'check-format.yaml', 'dispatch.yaml', 'pr-pull.yaml', 'publish.yaml', 'push.yaml', 'scheduled.yaml', 'sign-windows.yaml') {
    gh workflow disable $w -R bmsolutionsvn/BreadStudio
}
gh workflow list -R bmsolutionsvn/BreadStudio --all
git push origin main
```

`gh workflow disable` reports an error for a workflow GitHub does not list as runnable (the reusable ones); that is fine. Expected from the list: every OBS workflow shows `disabled_manually` or is absent, and `BreadStudio Windows` shows `active` after the push.

- [ ] **Step 4: Watch the run**

```powershell
gh run watch -R bmsolutionsvn/BreadStudio (gh run list -R bmsolutionsvn/BreadStudio --workflow breadstudio-windows.yaml --limit 1 --json databaseId --jq '.[0].databaseId')
```

Expected: the run succeeds and has the `BreadStudio-windows-x64` artifact.

If the job never starts because the runner label is unknown to the fork, report it; do not change the label by guessing. If the build fails on the runner but passes locally, report the first error.

---

### Task 9: Smoke checklist, merge routine, first release

**Files:**
- Create: `breadstudio/docs/SMOKE.md`, `breadstudio/docs/MERGING.md`

**Interfaces:**
- Consumes: an installer from Task 7 or from CI.
- Produces: tag `breadstudio-0.1.0`.

- [ ] **Step 1: Write `breadstudio/docs/SMOKE.md`**

```markdown
# BreadStudio smoke checklist

Run on a build installed from the installer, never from the build folder.
Record the date, the installer file name and the result of each item in the
table at the end.

Before starting: note whether OBS Studio is installed on this PC. If it is,
write down the newest modified time under `%APPDATA%\obs-studio`.

1. **Look.** BreadStudio starts with the Bread theme (near-black, cyan
   highlights). The window, taskbar and tray icons are the BreadStudio mark.
   The title bar starts with `BreadStudio` and its version.
2. **Name.** With the interface in English, "OBS" appears nowhere on the main
   window, in Settings, or in the Help menu. In Help → About it appears only
   in the "based on OBS Studio" line, the About text, and the Authors and
   License tabs. Repeat the main-window check with the interface in
   Vietnamese (Settings → General → Language, then restart).
3. **Sources and recording.** Add a Video Capture Device and a Media Source.
   Record 30 seconds. The file plays back with picture and sound.
4. **Streaming.** Settings → Stream → Service: Custom. Paste a BreadStream
   Channel's ingest URL and key. Start Streaming. The Channel shows Live in
   BreadStream within 30 seconds and stays Live for 10 minutes with no
   reconnect in the BreadStudio log.
5. **A third-party plugin.** Take one plugin built for the same OBS version
   (the tag in `breadstudio/UPSTREAM-TAG`). Copy it to
   `%ProgramData%\BreadStudio\plugins\<name>\bin\64bit\` and its data to
   `%ProgramData%\BreadStudio\plugins\<name>\data\`. Restart. The plugin's
   source or filter is offered, and Help → Log Files → View Current Log shows
   it loaded.
6. **Beside OBS.** (Skip if OBS is not installed.) The newest modified time
   under `%APPDATA%\obs-studio` is unchanged. Start OBS and BreadStudio at the
   same time: both open, neither warns that it is already running. OBS still
   has its own scenes and settings.
7. **What must be gone.** No Check For Updates, no What's New, no service
   sign-in ("Connect Account") in Settings → Stream, no log upload in
   Help → Log Files. After forcing a crash (end `BreadStudio.exe` in Task
   Manager while it runs) the next start offers Safe Mode or Normal, with no
   option to send a report.
8. **Upgrade and uninstall.** Run the same installer again over the existing
   install: scenes and settings are still there. Uninstall BreadStudio: its
   Start menu entry is gone, `%APPDATA%\BreadStudio` is still there, and OBS
   (if installed) still starts.

| Date | Installer | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
```

- [ ] **Step 2: Write `breadstudio/docs/MERGING.md`**

```markdown
# Merging a new OBS release

BreadStudio's `main` is an OBS release tag plus our commits. A new OBS release
is merged in; `main` is never rebased or force-pushed.

Run everything from a Visual Studio 2026 developer shell at the repo root.
`NEW` below is the OBS tag being merged, for example `33.0.0`.

1. **Fetch and branch.**

       git fetch upstream --tags
       git checkout main
       git pull --ff-only
       git checkout -b merge-NEW
       git merge --no-ff NEW

2. **Resolve conflicts** with `breadstudio/UPSTREAM-EDITS.md` open.
   - Every conflicted file should be on the list. Keep OBS's new code and
     re-apply the edit the list describes.
   - A conflict in a file that is not on the list means either the list or
     the fork is wrong. Stop and find out which before going on.
   - `frontend/forms/images/*.png`: keep ours.

3. **Update the submodules** to the commits the new tag pins.

       git submodule update --init --recursive

4. **Point the fork at the new tag.** Put `NEW` in `breadstudio/UPSTREAM-TAG`.

5. **Re-check each edit, even where git reported no conflict.** For every
   line in `UPSTREAM-EDITS.md`, open the file and confirm the edit is still
   there and still does what the list says. OBS moving or renaming code can
   leave an edit in place but useless. Then search for new places that need
   the same treatment:

       git diff OLD NEW --stat -- frontend/forms/OBSBasic.ui frontend/OBSApp.cpp frontend/widgets/OBSBasic.cpp
       $old = git grep -hoE "https://obsproject\.com[A-Za-z0-9/._-]*" OLD -- frontend | Sort-Object -Unique
       $new = git grep -hoE "https://obsproject\.com[A-Za-z0-9/._-]*" NEW -- frontend | Sort-Object -Unique
       Compare-Object $old $new

   The first command shows how much the files we depend on moved. The last
   one lists obsproject.com addresses that exist in only one of the two tags
   (`OLD` is the tag `main` was on before this merge). An address marked `=>`
   is new in `NEW`: a new place where BreadStudio would contact OBS's servers.
   Find the code that uses it and switch it off or hide it, listing the file.
   Also check `frontend/forms/OBSBasic.ui` for new Help-menu actions and add
   any that lead to OBS's servers or support to the `hidden` list in
   `breadstudio/src/Branding.cpp`.

6. **Check, build, package.**

       powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
       powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.test.ps1
       powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\packaging\Rename-Product.test.ps1
       powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\build.ps1
       powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\packaging\package.ps1

   If packaging stops with "The OBS name is still in en-US.ini", OBS added
   wording the rename pattern does not cover. Add a test case, fix the
   pattern, rerun.

7. **Smoke-test** the installed build with `breadstudio/docs/SMOKE.md`.

8. **Finish.** Raise `breadstudio/VERSION`, commit, merge the branch into
   `main` with a merge commit, tag `breadstudio-<version>`, push.

9. **Record the merge** in the table below.

| Date | From | To | Hours | Conflicted files | Edits that needed rework | Notes |
|---|---|---|---|---|---|---|
```

- [ ] **Step 3: Commit the documents**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git add -A
git commit -m "breadstudio: add the smoke checklist and the merge routine"
```

- [ ] **Step 4: OWNER GATE — run the smoke checklist**

The owner runs `breadstudio/docs/SMOKE.md` on the installer from Task 7 and fills in a row of its table. Items 4 and 6 need a BreadStream Channel and a PC with OBS installed. A worker cannot pass this step; it hands over the installer path and stops.

Every item must pass. A failed item is fixed in the task that owns it (name and menus: Task 3; folders and the instance lock: Task 4; icons: Task 5; theme: Task 6; interface text, updater, installer: Task 7), then the checklist is run again from item 1.

- [ ] **Step 5: OWNER GATE — tag and push**

```powershell
git add breadstudio/docs/SMOKE.md
git commit -m "breadstudio: record the 0.1.0 smoke run"
git tag breadstudio-0.1.0
git push origin main breadstudio-0.1.0
```

---

### Task 10: The merge drill (OBS 33.0.0)

This task starts when OBS 33.0.0 is released. If Tasks 1–9 finish first, run the drill on the newest `33.0.0-rcN` tag, then run it again on the final release; both runs get a row in `MERGING.md`.

**Files:**
- Modify: `breadstudio/UPSTREAM-TAG`, `breadstudio/VERSION`, `breadstudio/docs/MERGING.md`, `breadstudio/docs/SMOKE.md`, and whichever listed files conflict

**Interfaces:**
- Consumes: everything above.
- Produces: `main` based on the new OBS tag; a filled row in each of the two tables.

- [ ] **Step 1: Find the tag and note the start time**

```powershell
git fetch upstream --tags
git tag --list "33.0.0*" --sort=-creatordate | Select-Object -First 3
Get-Date -Format o
```

Use `33.0.0` if it is listed, otherwise the first (newest) `33.0.0-rcN`.

- [ ] **Step 2: Follow `breadstudio/docs/MERGING.md` steps 1 to 6 exactly**

Use the tag from Step 1 as `NEW` and `32.2.2` as `OLD`. Do not skip step 5 of the routine when git reports no conflicts.

Where the routine itself is wrong or unclear, fix `MERGING.md` in the same branch: improving the routine is the point of the drill.

- [ ] **Step 3: Raise the version**

Put `0.2.0` in `breadstudio/VERSION`. Rebuild and repackage (routine step 6) so the installer and the About line carry it.

- [ ] **Step 4: OWNER GATE — smoke-test the merged build**

The owner runs `SMOKE.md` on the new installer and adds a row. Every item must pass.

- [ ] **Step 5: Record the drill**

Add a row to the table in `MERGING.md`: today's date, `32.2.2`, the new tag, the hours from Step 1 to now, the conflicted files, the edits that needed rework, and anything that surprised you.

- [ ] **Step 6: OWNER GATE — merge, tag, push**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\check-edits.ps1
git add -A
git commit -m "breadstudio: merge OBS NEW and record the drill"
git checkout main
git merge --no-ff merge-NEW
git tag breadstudio-0.2.0
git push origin main breadstudio-0.2.0
```

Replace `NEW` with the tag from Step 1. S0 is done when this push is made from a final `33.0.0` merge with a passing smoke row.
