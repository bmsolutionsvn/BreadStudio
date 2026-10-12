# BreadStudio S0 — Fork baseline — Design

Date: 2026-10-12
Status: approved by the owner (2026-10-12).
Parent: `docs/superpowers/specs/2026-10-12-broadcast-roadmap-design.md` (§4 row 1, R1, R2, R8).
Location: this file sits at the workspace root because the BreadStudio repo
does not exist yet. The plan's first task moves it into
`BreadStudio/breadstudio/docs/`.

## 1. Purpose

BreadStudio is a vMix alternative built on the OBS Studio source code. S0
creates the fork and proves it can be kept: it builds here, it carries the
Bread name and look instead of OBS's, it installs, and a new OBS release can be
merged in by a written routine.

S0 adds no feature that OBS lacks. The desk is S1.

### 1.1 Done when

- A branded build, installed from the installer, streams to BreadStream from a
  pasted URL for 10 minutes.
- OBS 33.0.0 has been merged in by the routine in §6, and the time taken and
  conflicts met are recorded.
- The smoke checklist (§7) passes before and after that merge.

## 2. Decisions

| # | Topic | Decision |
|---|---|---|
| F1 | Base | OBS Studio 32.2.2, the stable release on 2026-10-12. |
| F2 | Merge drill | OBS 33.0.0 when it ships (release candidate 1 was published 2026-10-09). If the rest of S0 is finished first, drill on the newest 33 release candidate and repeat on the final. |
| F3 | Carrying our changes | Upstream releases are merged into our branch. No rebasing and no force-push, because the repo is public. |
| F4 | Repo | `bmsolutionsvn/BreadStudio` on GitHub, a public fork of `obsproject/obs-studio`, public from the start. |
| F5 | What is rebranded | The application, not the libraries (§3). |
| F6 | Plugins | Loaded from BreadStudio's own folders only, never from OBS's. |
| F7 | Platform | Windows x64 only is built and tested. |
| F8 | Installer | Inno Setup, unsigned in S0. |
| F9 | Licence | GPL-2.0-or-later, unchanged from OBS. |

Rejected:

- **Rebasing a patch stack onto each release.** A cleaner stack, but it
  rewrites public history on every release.
- **Branding only through build options and added files.** It cannot cover the
  name, the icon and the settings folder. Its discipline is kept as a rule:
  add a new file where possible, edit an OBS file only where there is no other
  way.
- **Sharing OBS's plugin folder.** A plugin installed for another OBS version
  on the same PC could then crash BreadStudio.

## 3. What the fork changes

"OBS", "OBS Studio", "Open Broadcaster Software" and the OBS Studio logo are
registered trademarks of Wizards of OBS LLC. The licence allows the fork; the
fork must not carry the name or the logo.

### 3.1 Changed

| What | How |
|---|---|
| Name shown to the user: window title, About, Start menu, Task Manager | The program is `BreadStudio.exe`. About reads "BreadStudio 0.1.0, based on OBS Studio 32.2.2" and keeps OBS's licence and author notices. |
| OBS logo and icons | Replaced. No BreadStudio icon exists, so S0 ships a placeholder drawn from the mark in `bm-landing/public/favicon.svg`. The owner supplies the real icon later. |
| The name "OBS Studio" inside interface text | Replaced where the interface looks text up, in one place. The translation files are not edited. |
| Look | A Bread theme added to OBS's theme system and made the default: the Cyan Blueprint colours and type from the workspace `DESIGN.md`. OBS's own themes stay selectable. |
| Settings folder | `%APPDATA%\BreadStudio`. BreadStudio installs beside OBS and never reads or writes OBS's settings. |
| Help menu links | Pointed at bmsolutions.vn, or removed where there is no Bread equivalent. |
| Log upload | Removed. Logs must not go to OBS's servers. |

### 3.2 Switched off

| What | Why |
|---|---|
| Updater and "What's New" | They fetch OBS's builds and news from OBS's servers. |
| Built-in Twitch, YouTube and Restream sign-in | They need OBS's private API keys. Streaming to any service by pasted URL and key still works. |

### 3.3 Left as OBS has it

| What | Why |
|---|---|
| `obs.dll`, the frontend API library, the plugin API, the library version number | A plugin built for OBS 32.2.2 loads unchanged. |
| All translations, Vietnamese included | They come free. |
| The streaming service list | It is data, and pasted-key streaming uses it. |
| The plugin manager | Kept if it works without OBS's servers (§9). |
| macOS and Linux code | Not built or tested, and not deleted, so merges stay clean. |

### 3.4 Version

BreadStudio has its own version, starting at 0.1.0. The library version stays
OBS's, because plugins check it.

## 4. Repo layout

- GitHub: `bmsolutionsvn/BreadStudio`. Local: `F:\Coding\BreadMedia\BreadStudio`.
- Branch `main` starts at the `32.2.2` tag. Remote `upstream` is
  `obsproject/obs-studio`.
- OBS's submodules (`plugins/obs-browser`, `plugins/obs-websocket`,
  `deps/libdshowcapture/src`) keep pointing at OBS's repos. One is forked only
  if it has to be changed.
- Everything of ours lives in one new top-level folder:

  ```
  breadstudio/
    theme/              the Bread theme file
    icons/              placeholder icon sources and the .ico
    installer/          the Inno Setup script
    docs/               this spec, the plan, MERGING.md, SMOKE.md
    UPSTREAM-EDITS.md   every OBS file we edited, and why
    check-edits.ps1     fails if the list is out of date
  ```

- **`UPSTREAM-EDITS.md` is the merge checklist.** `check-edits.ps1` compares
  the files changed since the pinned OBS tag, outside `breadstudio/`, with the
  files named in the list, and fails on any difference. It runs in CI.

## 5. Build, CI, installer

**Build**

- OBS's `windows-x64` CMake preset, unchanged. At 32.2.2 it uses the
  `Visual Studio 18 2026` generator and Windows SDK 10.0.26100.0, which the
  build machine has.
- OBS's build downloads its dependencies prebuilt, so nothing but OBS is
  compiled.

**CI**

- OBS's workflows are disabled in the repo's GitHub Actions settings. Their
  files are not deleted or edited, so they never conflict.
- One new workflow, `breadstudio-windows.yaml`: on every push and pull request
  it runs `check-edits.ps1`, builds Windows x64, builds the installer and
  uploads it as an artifact.

**Installer**

- Inno Setup. Installs to `Program Files\BreadStudio`, adds a Start menu entry
  and an uninstaller. Uninstalling leaves `%APPDATA%\BreadStudio` in place.
- Unsigned, so Windows shows a SmartScreen warning. A signing certificate is
  needed before a build goes to anyone outside. That purchase is not part of
  S0.

## 6. Merge routine

Written as `breadstudio/docs/MERGING.md` and followed step by step:

1. `git fetch upstream --tags`, then merge the new tag into `main` on a branch.
2. Resolve conflicts with `UPSTREAM-EDITS.md` open: each conflict should be in
   a file on the list. A conflict in a file not on the list means the list or
   the fork is wrong; stop and find out which.
3. Update submodules to the commits the new tag pins.
4. Run `check-edits.ps1` against the new tag, build, and run the smoke
   checklist.
5. Update the "based on OBS Studio" version, merge the branch, tag the
   BreadStudio release.
6. Record in `MERGING.md`: the date, the OBS tag, the time taken, and each
   conflict with how it was resolved.

## 7. Testing

OBS has almost no automated tests to build on, so S0 has one script and one
manual checklist.

- **Automated:** `check-edits.ps1` (§4), in CI.
- **Manual:** `breadstudio/docs/SMOKE.md`, run on an installed build:
  1. It starts with the Bread theme. No OBS name or logo shows on the main
     window, in Settings or in About, other than the "based on OBS Studio"
     line and the licence notices.
  2. A camera and a media file can be added. A recording is made and plays
     back.
  3. It streams to BreadStream from a pasted URL and key for 10 minutes.
  4. One third-party plugin built for the same OBS version loads from
     BreadStudio's plugin folder.
  5. With OBS installed on the same PC, OBS's settings are unchanged after
     BreadStudio has run, and uninstalling BreadStudio leaves OBS working.
  6. The updater, "What's New", service sign-in and log upload are not
     reachable from any menu.

## 8. Out of scope

- The desk (S1) and every Bread integration (S2).
- Code signing and an updater of our own.
- macOS and Linux builds.
- Renaming OBS's libraries or changing the plugin API.
- The final BreadStudio icon.

## 9. Findings from the 32.2.2 source (amendment, 2026-10-12)

The six assumptions this section first listed were checked against the source
while the plan was written. The results below override §3 to §5 where they
differ.

| Assumption | Result | What the plan does |
|---|---|---|
| Interface text is looked up through one function where the name can be replaced. | False. The lookup returns pointers into loaded tables. | The packaging step rewrites the product name in the installed translation files. No source or translation file is edited. A build run from the build folder therefore still shows OBS's wording in some dialogs; only an installed build is fully branded. |
| Service sign-in is compiled only when API keys are supplied. | True. | Nothing to do. |
| The updater and "What's New" can be switched off by build options. | Half true. "What's New" has a build option. The updater does not, but OBS switches it off when a file named `disable_updater` sits in the install folder. | The build passes `ENABLE_WHATSNEW=OFF`. The installer ships `disable_updater` and leaves out `updater.exe`. |
| The plugin manager works without OBS's servers. | True; it contains no web address. | Kept. |
| The settings folder name is set in one place. | False. The literal `obs-studio/…` appears in about 45 places in 12 files. All of them pass through two functions. | Those two functions add a `BreadStudio/` prefix. Settings live in `%APPDATA%\BreadStudio\obs-studio\`. |
| The plugin search path is set in one place. | True: one line. | Third-party plugins load from `%ProgramData%\BreadStudio\plugins\`. |
| The GitHub Windows runner can build with Visual Studio 2026. | OBS's own workflow uses the image `windows-2025-vs2026`. | CI uses the same image. Confirmed only when the first CI run passes. |

Further changes found while planning:

- **Theme location.** The theme file is `frontend/data/themes/Yami_Bread.ovt`,
  not `breadstudio/theme/`, so OBS's own data copy installs it. It is a new
  file in an OBS folder and is listed in `UPSTREAM-EDITS.md`.
- **Theme scope.** Colours only. OBS's themes set their own font; the brand
  type is left to S1.
- **Accent colour.** OBS's theme draws white text on its accent colour, and
  the brand cyan cannot carry white text. Filled controls use a darker cyan
  (`#0E7490`); the brand cyan (`#22D3EE`) is the highlight and focus colour.
- **Running beside OBS.** OBS's single-instance lock is renamed, so OBS and
  BreadStudio can run at the same time.
- **Crash reports.** The offer to send a crash log to OBS's servers after a
  crash is switched off.
- **About dialog.** OBS's About fetches a supporter list from its server each
  time it opens. BreadStudio's About shows a fixed text instead. The links to
  donate to and contribute to the OBS Project stay.
- **Virtual camera.** The installer does not register one. The registration
  is system-wide and belongs to OBS on a PC that has OBS; a BreadStudio
  virtual camera needs its own identity and is left to a later stage.
- **Version pin.** OBS's build takes its version from `git describe`, which a
  BreadStudio release tag would change. Every build passes the pinned OBS
  version explicitly, and packaging refuses a build where it differs.
- **Layout.** `breadstudio/` also holds `VERSION`, `UPSTREAM-TAG`, `build.ps1`,
  `src/`, `cmake/`, `windows/` and `packaging/`. The full list is in the plan.
