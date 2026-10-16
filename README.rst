BreadStudio
===========

BreadStudio is a live production switcher in the vMix mould, built as a fork
of OBS Studio 32.2.2. An operator who knows vMix runs a show here the way they
would in vMix: a row of numbered inputs, Preview and Program, Cut and OBS's
automatic transition, a bus mixer, and a MultiView. They never open OBS's
scene list.

BreadStudio is a fork in progress. Several of the desk's features are still to
come -- the single Project file and the vMix preset import among them;
``breadstudio/docs/2026-10-16-breadstudio-cleanup-design.md`` designs them, and
``breadstudio/docs/2026-10-16-breadstudio-backlog.md`` lists what remains.

Licence
-------

BreadStudio is distributed under the GNU General Public License v2 (or any
later version), the same licence as OBS Studio -- see the accompanying COPYING
file for details.

BreadStudio is based on OBS Studio 32.2.2 and is *not* affiliated with or
endorsed by the OBS Project. Credit and thanks to the OBS Studio authors and
contributors. "OBS", "OBS Studio" and "Open Broadcaster Software", and the OBS
Studio logo, are registered trademarks of Wizards of OBS LLC.

Building on Windows
-------------------

Windows x64 is the only platform built and tested.

The ``windows-x64`` CMake preset configures the build;
``CMakePresets.json`` uses the ``Visual Studio 18 2026`` generator at OBS
32.2.2, and the build machine's edit to ``Visual Studio 17 2022`` is pending.

``breadstudio\build.ps1`` configures and builds it in one step:

::

    powershell -NoProfile -ExecutionPolicy Bypass -File breadstudio\build.ps1

When it finishes, the program is
``build_x64\rundir\RelWithDebInfo\bin\64bit\BreadStudio.exe``.

It configures the ``windows-x64`` preset with the pinned OBS version and
``ENABLE_WHATSNEW=OFF``, then builds the ``windows-x64`` preset with the
``RelWithDebInfo`` configuration by default (pass ``-Configuration`` for
another). Run it from a developer shell where ``cmake`` and the Visual Studio
toolchain are on the path.

Documentation
-------------

The desk's own documentation lives in ``breadstudio/docs/``:

- the S0 fork-baseline and S1 desk design documents and their plans;
- the design and plan documents for the desk's stages -- the outputs engine,
  the MultiView, the input settings window and the cleanup -- alongside this
  fork's own;
- ``DESK-SMOKE.md``, the manual checklist run on an installed build and after
  every upstream merge;
- the owner's backlog, ``2026-10-16-breadstudio-backlog.md``.

Keeping the fork mergeable
--------------------------

Every OBS file BreadStudio changes outside ``breadstudio/`` is listed, with its
reason, in ``breadstudio/UPSTREAM-EDITS.md``. ``breadstudio/check-edits.ps1``
compares that list with the files changed since the pinned OBS tag and fails
when the two disagree. During an upstream merge, a conflict should only ever
be in a file on that list.

Project
-------

Source and issues: https://github.com/bmsolutionsvn/BreadStudio
