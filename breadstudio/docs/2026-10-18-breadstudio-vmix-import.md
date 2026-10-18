# BreadStudio — the vMix import, plan 3

The plan for spec §5: `Project → Import vMix Preset…` reads one `.vmix` file and
builds a new, unsaved Project from it. It is the last piece of the cleanup
design (`2026-10-16-breadstudio-cleanup-design.md`), after the Project itself
(`2026-10-17-breadstudio-project.md`, done).

**What it is not:** BreadStudio never writes a `.vmix`, never opens one as a
Project, and never becomes a vMix editor (V1). The import is one way, in.

**Done when:** an operator's own preset produces the same show on screen — the
same inputs, in the same order, the same shots, the same audio — with every
difference on the report (V6), and the report is never empty when something was
dropped or approximated (V4).

## The shape

One unit, `VmixPreset`, in two halves on purpose (spec §5.4):

- **Reading** turns a preset's XML into a plain list — no OBS types, no Qt
  widgets, one entry per input carrying its kind, name, number, geometry, audio
  and file or device — plus a list of report rows for what it could not read.
  This half is automated-tested (§9).
- **Building** walks that list and creates the OBS sources through the desk's
  own `InputModel`, so an imported input is made exactly as an Add-menu input
  is: the same fixed shape, the same loop refusal, the same numbering (spec
  §5.4, S1 §3).

## The mapping (spec §5.2 — the rule, not a suggestion)

| vMix | Becomes | Notes |
|---|---|---|
| Colour | a Colour base ("Blank") | the colour value is carried |
| Camera / Video Capture | a video capture device | matched by name; absent ⇒ still made, empty, reported |
| Video, Audio (file) | a media source | the file path is carried |
| Image | an image source | the file path is carried |
| NDI | the desk's NDI input | the NDI source name is carried |
| Browser, Web Browser | a browser source | only if OBS's browser plugin is built in; else reported |
| Desktop Capture, Screen | a display or window capture | a window that has gone is reported |
| Title, GT Title | the imported title, drawn as it was (S7) | text, colours, images, static layout drawn; animation, data binding and anything that changes on air reported |
| Audio input | an audio capture device | matched by name, as a camera is |
| Video Delay, Virtual Set, PowerPoint, Stream, Video Call, Mix, Photos, Replay | nothing | no desk equivalent; reported |
| input number | the input's order in the bar | number is position |
| input title | the input's name | |
| position, size, crop, rotation | the base's geometry | where vMix has one and the OBS source supports it |
| volume, mute | the strip's fader and ♪ | |
| Overlays 1–4 | nothing | no overlay bus; the inputs are still imported, the routing reported |
| active and preview numbers | the desk's initial Program and Preview | applied once the inputs exist |
| transitions | nothing | the desk uses OBS's; reported |
| stream, output, recording settings | the Project's settings half | carried where OBS has the same setting — **not implemented; see "Not carried" below** |

**Not carried.** The table's last row — stream, output and recording settings — is
not implemented, and no honest mapping exists. The reader reads no service, key,
encoder or recording path, and the builder applies none, so the new Project keeps
OBS's own defaults. vMix's settings are vMix's own shape — `StreamingSettings`
holds up to three destinations with embedded URLs and keys, `RecordingSettings` a
path and a format, `OutputFormat` the external and NDI outputs — and OBS's stream
service, encoders and output modes are not that; a fabricated map would put a
wrong setting on air, so none is written. The import raises one report row naming
the settings as not imported, so the operator is told rather than left to find
out on air (V4).

**Unknown types.** An input `Type` this spec cannot name (15, 22, 100 are the
known unknowns) is read from the input's `OriginalTitle` and **put on the
report rather than guessed** (V7). A vMix release that changes the file is
caught by the report and the smoke checklist, not by a crash.

---

### Task 1: `VmixPreset` reads a preset into a list

**Files:**
- Create: `breadstudio/desk/VmixPreset.hpp`, `breadstudio/desk/VmixPreset.cpp`
- Create: `breadstudio/desk/test/VmixPresetTest.cpp`
- Create: `breadstudio/desk/test/fixtures/*.vmix` (cut-down real presets)
- Modify: `breadstudio/desk/CMakeLists.txt`

**Step 1: the interface.** One entry per input, and one row per thing to report
— both plain data, no OBS, no Qt widgets. The reader takes the preset's bytes
and returns false with a message only when the file is not XML at all; a preset
that parses but holds nothing the desk can back still *succeeds* and produces
report rows.

```cpp
namespace desk {

/* What a vMix input is, as far as the desk needs to know. */
enum class VmixKind {
	Colour,
	Video,
	Image,
	Capture,   /* Camera, Video Capture */
	AudioIn,   /* an audio capture device */
	AudioFile,
	Media,     /* Video or Audio file */
	NDI,
	Browser,
	DesktopCapture,
	Title,     /* Title, GT Title */
	Unknown,   /* a Type this spec cannot name: reported, never guessed */
	Skipped,   /* a kind with no desk equivalent: reported */
};

/* One vMix input, read straight from the preset. */
struct VmixInput {
	int number = 0;          /* the input's order in the bar: number is position */
	std::string title;       /* the input's name */
	std::string originalTitle; /* what OriginalType said, for the report */
	int type = -1;           /* vMix's Type as written, for the report */
	VmixKind kind = VmixKind::Unknown;
	std::string file;        /* a path, for Video/Image/Audio/Media */
	std::string device;      /* a device or NDI name, for Capture/AudioIn/NDI */
	std::string colour;      /* the colour value, for Colour */

	bool hasGeometry = false; /* where vMix has one and the source supports it */
	double x = 0, y = 0, width = 0, height = 0, rotation = 0;
	double cropLeft = 0, cropTop = 0, cropRight = 0, cropBottom = 0;

	double volume = 1.0;      /* the strip's fader */
	bool muted = false;       /* the strip's ♪ */
};

/* One line of the report (§5.3): what vMix had, and why it is not here. */
struct VmixReportRow {
	std::string input;   /* the vMix input's title, or "" for a preset-wide row */
	std::string had;     /* what vMix had */
	std::string why;     /* why it was skipped or approximated */
};

/* What a preset produced. `inputs` is in vMix's number order. */
struct VmixPresetResult {
	std::vector<VmixInput> inputs;
	std::vector<VmixReportRow> report;
	int active = 0;    /* the active number, for the desk's Program */
	int preview = 0;   /* the preview number, for the desk's Preview */
	std::string name;  /* the preset's own name, for the new Project */
};

/* Reads one .vmix. False, with `error` set, only when the bytes are not a
 * preset at all; a preset with nothing the desk can back still returns true,
 * with the reasons in `result.report`. */
bool readVmixPreset(const std::string &xml, VmixPresetResult &result, std::string &error);

} // namespace desk
```

**Step 2: the test, written first.** `breadstudio/desk/test/VmixPresetTest.cpp`,
following the desk's other tests: plain C++, a small assert helper, a `main`
that runs each case, prints `VmixPreset: all checks passed`, and **ends with an
explicit exit**. It links `Qt6::Xml` for the reader (the spec asks for no **OBS**
types here, not no Qt — `§9` is explicit) and reads the fixtures from beside
itself (pass the fixture directory by argument with a sensible default, so it
also runs from the build tree).

Cases:
- a cut-down real preset produces the inputs it holds, **in number order**, with
  each one's kind, title, file or device, geometry and audio read from it;
- a preset of unknown types (15, 22, 100) produces **no** inputs of those kinds
  and a report row each, naming the input from `OriginalTitle` — the guarantee
  the spec makes (V7);
- a kind with no desk equivalent (Video Delay, Virtual Set, PowerPoint, Stream,
  Video Call, Mix, Photos, Replay) is reported, not silently dropped;
- a preset with more inputs than the bar holds imports the first N and reports
  the rest (V5) — N is the desk's own limit, passed in, so the test does not
  hard-code it;
- bytes that are not XML at all are refused with a message;
- an empty preset succeeds with an empty input list (no silent failure);
- the report is **empty** for a preset where everything mapped — "an empty
  report says the whole preset came across" (§5.3).

**Fixtures.** Cut down from the owner's real presets in `F:\Stream` (they are
private; **do not commit them whole** — take the smallest real one and cut it to
a handful of inputs, or hand-write fixtures in vMix's shape if cutting is
awkward). Every fixture must be a real preset's shape — `<XML>` with
`<Input Type="N" ...>` attributes — not an invented one, or the test proves
nothing about real files. Say which fixture is cut and which is written.

**Step 3: implement the reading half.** Qt's `QDomDocument` (already linked, and
`Qt6::Xml` added to the test target). Read the `<Input>` elements' attributes,
map `Type` to `VmixKind` by the table, and push a report row for every
`Unknown`/`Skipped` and for every input whose file or device a later task cannot
resolve. **Do not** read children — the presets carry inputs as attributes only.

**Step 4:** build and run. The test must pass.

**Step 5: commit.**
```
breadstudio: read a vMix preset into a plain list, and report what it cannot back
```

**As built — the plan corrected by the file.** Written against the real presets,
three things above turned out wrong; the code follows the file:

- **Geometry is not on `<Input>` as flat numbers.** vMix keeps an input's
  placement in the embedded `Positions` matrix — a relative Pan/Zoom/Rotate plus
  Clipping, one `MatrixPosition` per placement, index 0 (the input's own Main
  placement) first — not as flat
  `x`/`y`/`width`/`height`/`rotation`/`crop` attributes. It **is** readable:
  the reader carries the Main placement's matrix raw (`panX/panY`, `zoomX/zoomY`,
  `rotateX/Y/Z`, the crop fractions from `Clipping`, `hidden`, `postZoomX/Y`,
  `sourceRotation`, `matrixMoved`, and `matrixCount` — how many placements the
  preset holds for this input), and **Task 2 converts it** into the base's
  `obs_transform_info`/crop, given the output resolution and the source's own pixel
  size, which the reader cannot know. Crop the desk *does* hold
  (`obs_sceneitem_set_crop`) and is ported; what it cannot put on a base — a mirror,
  a nested `MultiplyPosition`, a border, a rotate origin, and the post-zoom and
  source rotation vMix applies around them — makes an applied matrix an
  approximation, so Task 2 reports the shot (V4). **The plan above read `Positions`
  as an array of outputs; it is not.** Index 0 is the input's own Main placement and
  the rest are its overlay **layers**: the Position tab reads "Layers / Multi View"
  and vMix serializes each extra placement as an `<overlay>` element. The desk
  builds no layers, so Task 2 takes Main (index 0) and reports the overlay layers.
- **The bar's limit is not on the reader.** `readVmixPreset` carries no bar, so
  the over-limit split (V5) is a second pure function, `limitVmixInputs(result,
  limit)`, which the desk calls with its own limit; the test passes N in.
- **The Type table is what the archive names** (the workspace roadmap §10):
  0 Video, 1 Image, 2 `.xaml` Title, 5 Camera, 7 Audio Input, 12 Colour,
  13 Audio File, 4000 NDI, 9000 GT Title; 14 List, 21 Image Sequence and 6000
  Video Call are named but have no desk equivalent and are reported; 15, 22 and
  100 cannot be named — read from `OriginalTitle` and reported, and they make no
  entry of their own (V7). Browser and Desktop Capture are in §5.2's table but in
  none of the presets, so no `Type` number is evidenced for them: they fall to
  Unknown and are reported rather than guessed. An input's `Position` is a
  playback position, not its order; the order is document order.

---

### Task 2: the building half, through the desk's own `InputModel`

**Files:**
- Modify: `breadstudio/desk/VmixPreset.hpp`, `breadstudio/desk/VmixPreset.cpp`
- Modify: `breadstudio/desk/test/VmixPresetTest.cpp`
- Modify: `breadstudio/desk/CMakeLists.txt`

**Step 1:** the second half walks a `VmixPresetResult` and makes the inputs
**through the desk's own `InputModel`/`createInput` path** — read
`InputModel.cpp`'s `createInput` and the Add-menu's own caller and use the same
route, so an imported input is a desk input, not a hand-built source (spec
§5.4). It returns the report the window shows, extended with a row for every
input the model refused (a loop, V5) and every source that could not be made
(an absent capture device still gets its input, empty, and a report row).

Keep the **decision** logic (which kind needs which base, what to report) pure
and testable; keep the OBS calls in thin wrappers. If a decision can only be
made against a live OBS source, say so rather than inventing a table.

**Step 2:** the geometry — position, size, crop and rotation map onto the base's
`obs_transform_info`/crop, the same fields the desk's own transform page edits;
volume and mute map onto the strip's fader and ♪, the same fields the mixer
edits. **Mark the Project changed** as the desk's own edits do
(`markProjectChanged()`), because an import is a change the operator will save.

**Step 3:** the active and preview numbers become the desk's initial Program and
Preview **after** the inputs exist. Transitions and Overlays 1–4 are reported,
not applied.

**Step 4:** extend the test with the pure parts (which kind maps to which base;
what gets a report row; the ordering; the over-limit split from Task 1 now
driven end to end through the builder's pure part). Anything that needs a live
OBS source is covered by the smoke row, not the test — say which.

**Step 5: commit.**
```
breadstudio: build a Project's inputs from an imported vMix preset
```

---

### Task 3: the import report window

**Files:**
- Create: `breadstudio/desk/VmixReport.cpp`, `breadstudio/desk/VmixReport.hpp`
- Modify: `breadstudio/desk/CMakeLists.txt`
- Modify: `breadstudio/desk/data/locale/en-US.ini`

**Step 1:** the desk's own widget (§5.3): a plain, read-only list of the report
rows, each naming the vMix input, what vMix had and why it was left out, with a
**Copy to clipboard** button (as text, so an operator can keep it with the
project). It is the same window whether one input was dropped or fifty, and
**an empty report says the whole preset came across** — so the window opens even
when there is nothing to report, with a line saying so. Style it like the desk's
other dialogs.

**Step 2:** locale keys beside the desk's others: `VmixReport.Title`,
`VmixReport.Invite`, `VmixReport.None`, `VmixReport.Copy`, `VmixReport.Columns`
(the three column headings).

**Step 3:** build; `check-edits.ps1` exit 0; all tests exit 0; clang-format
clean; every touched file ends with a newline.

**Step 4: commit.**
```
breadstudio: show what a vMix import could not bring across
```

---

### Task 4: the menu entry, and the smoke rows

**Files:**
- Modify: `breadstudio/desk/DeskLayout.cpp`, `breadstudio/desk/DeskLayout.hpp`
- Modify: `breadstudio/desk/data/locale/en-US.ini`
- Modify: `breadstudio/docs/DESK-SMOKE.md`
- Modify: `breadstudio/docs/2026-10-18-breadstudio-vmix-import.md` (only if the
  plan turned out wrong somewhere — correct it in the commit that finds out)

**Step 1:** add `Import vMix Preset…` to the ☰'s Project menu, in the place
plan 2 left for it (between `Save As…` and the separator). It asks for a `.vmix`
with `QFileDialog::getOpenFileName`. Because it builds a **new, unsaved
Project** (V3), it goes through the same unsaved-changes prompt the other
show-replacing actions use (`confirmDiscard`), and then: read the preset, build
into a **fresh** Project, and raise the report window once (V4). A read failure
is surfaced verbatim, as `open`'s errors are.

**Step 2:** locale key `Project.ImportVmix`, beside the others.

**Step 3:** append the smoke row to `DESK-SMOKE.md`, in the Project section, **appended, never inserted, never renumbered**:

```
The owner's own .vmix is imported: the inputs, their order, their shots and
their audio match what vMix showed, and the report names every difference (V6).
```

That is spec §9's row 10, and it is the acceptance test: **the owner's own
preset**, not a fixture. Note in the row which preset it is, so the check is
reproducible.

**Step 4:** build, `check-edits.ps1` exit 0, all tests exit 0, clang-format
clean, every touched file ends with a newline.

**Step 5: commit.**
```
breadstudio: import a vMix preset from the Project menu
```

**As built — the plan corrected by the tree.** Two things Task 4 left unsaid, and
one file it names that needed no change:

- **The desk's bar limit is not named, and this desk has none.** Task 4 says
  "build into a fresh Project" without saying what `buildVmixPreset`'s `limit`
  should be, and there is no bar cap to take it from: the bar scrolls and holds
  every input (S1 design §3, "Inputs are numbered 1…N"; the smoke row that makes
  26 inputs scrolls rather than trims, and only OBS's multiview is capped, at 25).
  So the import passes **0** — no bar — and V5's over-limit split has nothing to
  trim here. The rule still exists and is proven by Task 1's test, which passes N
  in directly.
- **Nothing names the new Project after the preset.** V3 says the new Project is
  "named after the preset", but `VmixPresetResult.name` is never set by the
  reader, `Project` has no API to take a name (`createNew()` makes "Untitled"),
  and Task 4's file list holds neither `Project` nor `VmixPreset`. The import
  therefore makes the fresh, unsaved Project `createNew()` already makes, and the
  operator names it when they first save. Carrying the preset's name needs a task
  that owns `Project`.
- **`DeskLayout.hpp` needed no change.** The entry and its whole flow are static
  to `DeskLayout.cpp` (a new `importVmixPreset` and one action in
  `buildProjectMenu`), so no declaration is added to the header.
- **A fresh Project that could not be made refuses the import.** `Project::createNew()`
  now reports failure (`bool createNew(std::string &error)`) instead of returning
  void, and the import checks it before building. Before this, an OBS refusal to
  create the profile or the collection left the open show live and the build would
  append the preset's inputs to it — which a later Save could write over the
  operator's own file. Now the open Project is untouched and the failure is
  surfaced verbatim, through the same message path a read failure uses. The other
  caller, the New action, surfaces it the same way: the open show is left untouched
  either way and there is nothing else the caller can do.
- **The stream, output and recording settings are not imported, and the report says
  so.** The reader and builder carry none (see "Not carried" above), so the import
  appends one preset-wide report row naming them; the operator is told rather than
  left to find out on air (V4).

---

## Self-review

**Spec coverage.** V1 (one way) — the reader never writes and Task 4 never
offers a save-back. V2 (its own entry, not Open…) — Task 4. V3 (a new unsaved
Project, the open one left alone) — Tasks 2 and 4. V4 (everything that maps is
created, everything else reported, nothing dropped silently) — Tasks 1 and 2,
shown by Task 3. V5 (the desk's own rules still hold: the loop refusal, more
inputs than the bar, both reported) — Tasks 1 and 2. V6 (the same show on
screen) — the smoke row. V7 (unknown types read from `OriginalTitle` and
reported, never guessed; a changed format caught by the report and the smoke
row, not a crash) — Task 1's unknown-type case and Task 4's row. §5.2's table
**except its stream/output/recording row** — the mapping in Task 1 and the
building in Task 2 cover one row per line of the table bar the last, which is not
implemented; it is stated plainly under "Not carried" and named by a report row.
§5.3 — Task 3. §5.4's two halves, `QDomDocument`, and "nothing here
edits an OBS file, `UPSTREAM-EDITS.md` gains no row" — Tasks 1 and 2, checked
in every task.

**Not in this plan:** vMix's `.xaml`/GT titles beyond their text (§10); vMix's
shortcuts, activators, scripting and data sources (§10); writing a `.vmix`
(§10); the installer step.

**Placeholder scan:** no TBD. Every task carries its interface, its test cases
and its locale keys.

**Type consistency:** `VmixKind`, `VmixInput`, `VmixReportRow`,
`VmixPresetResult`, `readVmixPreset` are defined once, in Task 1, and used by
name thereafter. The build half's entry point is added to the same header, so
Task 2 and Task 3 reference one type, not three.

**Two risks worth stating up front.**

1. **The presets are private and large** (0.1 MB to 2.3 MB, in `F:\Stream`).
   The fixtures must be **cut down**, and the cut-down ones committed, or the
   test proves the shape of an invented file rather than a real one. Task 1 says
   which is which.
2. **The reading half needs `Qt6::Xml` the desk's tests have never linked.**
   The existing six test exes are "no OBS, no Qt"; spec §9 asks this one for no
   **OBS** types, so Qt is allowed — but the CMake must add `Qt6::Xml` to that
   one target and the plan must not pretend otherwise. If linking Qt into a
   test executable turns out to be awkward in this tree, say so in the commit
   that finds out and move the reader's pure mapping behind a small seam the
   test can reach without Qt, rather than weakening the test.

## What is left after this

The installer step (registering `.bstd`, from the cleanup design's §7 list),
and the owner's hand-checks: the smoke rows K1–K6 from plan 2 and row 10 here.
