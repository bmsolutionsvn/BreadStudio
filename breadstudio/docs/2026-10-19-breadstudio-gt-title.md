# BreadStudio — GT title input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A native BreadStudio input that opens a vMix `.gtzip`, draws it as vMix does on the GPU, runs its clocks and countdowns, lets the operator edit its fields and their style, and replaces the empty text source the .vmix import makes for GT titles today.

**Architecture:** A new Windows-only OBS module, `bread-gt` (`breadstudio/gt/`), holds the pure parts — the package reader, the document parser, the field rule, the clock and countdown — and a Direct2D/DirectWrite renderer. The source `bread_gt_title` draws on its own D3D11 device into a keyed-mutex shared texture that OBS opens, so Direct2D never touches OBS's device state. The desk (`breadstudio/desk/`) gains the Input Select **Title** page, the ⚙ **Title** tab, and the import changes. It talks to the source only through its settings and two procs, with the keys in a header both sides include.

**Tech Stack:** C++17, libobs 32.2.2, Qt 6 (Widgets, Xml), zlib, Direct3D 11, Direct2D 1.1, DirectWrite, WIC. Tests are plain executables with the desk's `CHECK` macro.

**Spec:** `breadstudio/docs/2026-10-19-breadstudio-gt-title-design.md`. Executors read both. Section numbers (§) below are the spec's.

## Global Constraints

- Windows only. `breadstudio/gt/CMakeLists.txt` returns early when `NOT OS_WINDOWS`.
- New code lives under `breadstudio/`. The one OBS file touched is `plugins/CMakeLists.txt`, which gets one `add_subdirectory` line. Its `UPSTREAM-EDITS.md` row changes from "four lines" to "five lines" and names `breadstudio/gt`. `breadstudio/check-edits.ps1` must pass.
- No browser source and no network anywhere in the title path (§1.1).
- Drawn on the GPU into the frame OBS composites, with no copy of each frame through the CPU (§8).
- No new runtime dependency beyond Windows (D3D11, Direct2D, DirectWrite, WIC) and what the tree carries (Qt 6, zlib) (§8).
- Every user-facing string is a locale key in `breadstudio/desk/data/locale/en-US.ini`. Warning text the source produces is plain English data that the desk shows inside a localized frame.
- Field names are vMix's: `NAME.Text`, `NAME.Source`, `NAME.Fill.Color` (§4.3).
- The owner's titles, pictures, presets, fonts and vMix reference pictures are never committed. Local tests read `F:\Stream` and skip with a `SKIP` line and exit 0 when it is absent (§9).
- vMix's decompiled code is not committed, quoted or transcribed. Behaviour comes from the spec, and from vMix's output where a step says *verify* (§2 anti-goals).
- Repo rules (`BreadStudio/CLAUDE.md`): run GitNexus `impact` before editing an existing symbol, and `detect_changes` before every commit. Commit subjects start `breadstudio: `, and every message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Build: `cmake --build build_vs2022 --config RelWithDebInfo --target <target>`. Test binaries land in `build_vs2022/breadstudio/<module>/test-bin/RelWithDebInfo/`. A unit test prints `<Unit>: all checks passed` and exits 0, as `VmixPresetTest` does.

## The contract between source and desk

`breadstudio/gt/GtSettings.hpp`, header-only, is included by both modules:

```cpp
namespace gt {
inline constexpr const char *SourceId = "bread_gt_title";
inline constexpr const char *KeyFile = "file";              /* string: the .gtzip path */
inline constexpr const char *KeyValues = "values";          /* object: field -> string, only values differing from the file's */
inline constexpr const char *KeyStyles = "styles";          /* object: field -> GtStyle object (below) */
inline constexpr const char *KeyCountdowns = "countdowns";  /* object: field -> {start,duration,format,reverse,reverse_display} */
inline constexpr const char *KeyDataSources = "data_sources"; /* string: vMix's DataSourcesXML, kept for project 3 */
inline constexpr const char *ProcInfo = "void gt_info(out string json)";
inline constexpr const char *ProcCountdown = "void gt_countdown(in string field, in string action)"; /* start|pause|stop|reset */
}
```

A style object uses vMix's vocabulary, as strings and numbers: `font`, `size`, `fill` (`#AARRGGBB`), `weight` (`Bold`, …), `italic` (bool), `stretch` (`Condensed`, …), `line_spacing`. A missing key means the file's own value.

`gt_info` returns:

```json
{"fields":[{"name":"NameL1.Text","type":"text","default":"player name","countdown":false}],
 "warnings":[{"kind":"font","detail":"CruyffSansVN: NameL1.Text, NameR1.Text"}]}
```

`type` is `text`, `image` or `colour`. Warning `kind` is one of `font`, `file`, `picture`, `missing-resources`, `unsupported`, `text3d`, `animations` (§7).

## Review Focus

1. **A `.gtzip` saved deflate-compressed** (another vMix version, or a re-zip): all 171 entries in the owner's files are stored, but a reader that refuses deflate would make a valid title "not a GT title". Test: Task 1, `deflated.gtzip`.
2. **Vietnamese text** in values and fonts (CruyffSansVN): diacritics must survive the preset's XML, the settings JSON and DirectWrite shaping. Tests: Task 2 (document value), Task 10 (preset item value), Task 6 (render line in the local test), Task 9 (typed on Program, smoke).
3. **Braces in ordinary text** (`{TEAM}`, `{{`, an unmatched `{`): must show as written and never throw or blank the field. Test: Task 4.
4. **A large picture** (MAN OF THE MATCH has a 16 MB PNG) set into a field while on air: decoding it on the graphics thread drops frames. Requirement and smoke check: Task 7.
5. **The `.gtzip` re-saved in GT Designer while BreadStudio is open:** an operator expects the title to update. Requirement and smoke check: Task 7 (reload when the file's modified time changes).

---

### Task 1: The `bread-gt` module and the package reader

**Files:**
- Create: `breadstudio/gt/CMakeLists.txt`, `breadstudio/gt/GtSettings.hpp`, `breadstudio/gt/GtPackage.hpp`, `breadstudio/gt/GtPackage.cpp`, `breadstudio/gt/gt.cpp` (module entry: `OBS_DECLARE_MODULE()`, `obs_module_load` that only logs for now)
- Create: `breadstudio/gt/test/GtPackageTest.cpp`, `breadstudio/gt/test/fixtures/make-fixtures.py`, and the fixtures it writes: `minimal.gtzip`, `deflated.gtzip`, `missing-part.gtzip`, `not-a-zip.gtzip`
- Modify: `plugins/CMakeLists.txt` (one line after the `breadstudio/srt` line), `breadstudio/UPSTREAM-EDITS.md` (the `plugins/CMakeLists.txt` row)

**Interfaces:**
- Produces:
  ```cpp
  namespace gt {
  struct GtPackage {
      std::string documentXml;                                     /* /document.xml decoded as UTF-8 (§3.1) */
      std::map<std::string, std::vector<uint8_t>> resources;       /* resource filename -> first source's bytes */
      std::vector<uint8_t> thumbnail;                              /* empty when absent */
  };
  bool readGtPackage(const std::vector<uint8_t> &zip, GtPackage &out, std::string &error);
  bool readGtFile(const std::string &path, GtPackage &out, std::string &error); /* UTF-8 path */
  }
  ```
- Target `bread-gt-package-test` (no OBS, no Qt; links zlib the way `bread-project-test` does).

- [ ] **Step 1: Write the fixture generator and fixtures.** `make-fixtures.py` uses Python's `zipfile` to write:
  - `minimal.gtzip`: stored entries `[Content_Types].xml`, `document.xml`, `resources.xml`, `thumbnail.png`, and one picture part `/d3b07384-…` named `abc\BG.png` in `resources.xml`. The `document.xml` starts `<?xml version="1.0" encoding="utf-16"?>` but is UTF-8 bytes containing `Text="Nguyễn"`.
  - `deflated.gtzip`: the same entries, all `ZIP_DEFLATED`.
  - `missing-part.gtzip`: `resources.xml` names a guid that has no part.
  - `not-a-zip.gtzip`: the bytes `hello`.

  Run `python -I breadstudio/gt/test/fixtures/make-fixtures.py` and commit the outputs.
- [ ] **Step 2: Write the failing test** `GtPackageTest.cpp`:
  ```cpp
  GtPackage p; std::string e;
  CHECK(readGtFile(dir + "/minimal.gtzip", p, e));
  CHECK(p.documentXml.find("Nguyễn") != std::string::npos);   /* UTF-8 kept despite the utf-16 declaration */
  CHECK(p.resources.count("abc\\BG.png") == 1);
  CHECK(!p.thumbnail.empty());
  GtPackage d; CHECK(readGtFile(dir + "/deflated.gtzip", d, e));
  CHECK(d.documentXml == p.documentXml && d.resources == p.resources);
  GtPackage m; CHECK(readGtFile(dir + "/missing-part.gtzip", m, e));   /* a missing picture is not fatal */
  CHECK(m.resources.empty());
  GtPackage n; CHECK(!readGtFile(dir + "/not-a-zip.gtzip", n, e)); CHECK(!e.empty());
  GtPackage x; CHECK(!readGtFile(dir + "/absent.gtzip", x, e));
  ```
- [ ] **Step 3: Run it to see it fail.** Build `bread-gt-package-test`. Expected: a compile failure (`readGtFile` undefined).
- [ ] **Step 4: Implement `readGtPackage`/`readGtFile`.** Walk the zip central directory as `ProjectFile.cpp:280-330` does, but accept method 0 (stored) and method 8 (raw inflate, zlib `inflateInit2` with windowBits `-15`). A part name in `resources.xml` is a guid; strip a leading `/`. Parse `resources.xml` with a minimal scan for `<resource filename="…">` and `<source guid="…">` (no Qt in this unit), decoding the five XML entities in attribute values. Missing `/document.xml` is an error; a missing picture part is not.
- [ ] **Step 5: Run the test.** Expected: `GtPackage: all checks passed`.
- [ ] **Step 6: Wire the module.** Add `add_subdirectory("${CMAKE_SOURCE_DIR}/breadstudio/gt" "${CMAKE_BINARY_DIR}/breadstudio/gt")` to `plugins/CMakeLists.txt`. `breadstudio/gt/CMakeLists.txt` defines MODULE `bread-gt` (sources so far: `gt.cpp GtPackage.cpp`, linking `OBS::libobs ZLIB::ZLIB`), using `set_target_properties_obs(bread-gt PROPERTIES FOLDER plugins/bread-gt PREFIX "")`, with test output in `${CMAKE_BINARY_DIR}/breadstudio/gt/test-bin`. Update the `UPSTREAM-EDITS.md` row to "five lines that add breadstudio/desk, breadstudio/omt-hx, breadstudio/ndi, breadstudio/srt and breadstudio/gt to the build". Run `./breadstudio/check-edits.ps1`. Expected: passes.
- [ ] **Step 7: Commit:** `breadstudio: read a vMix GT title package`.

### Task 2: The document parser

**Files:**
- Create: `breadstudio/gt/GtDocument.hpp`, `breadstudio/gt/GtDocument.cpp`, `breadstudio/gt/test/GtDocumentTest.cpp`
- Modify: `breadstudio/gt/CMakeLists.txt` (add sources, link `Qt6::Xml`, target `bread-gt-document-test`)

**Interfaces:**
- Produces:
  ```cpp
  namespace gt {
  enum class GtBrushType { None, Solid, Linear, Transparent };
  struct GtBrush { GtBrushType type = GtBrushType::None; uint32_t argb = 0;
                   float startX = 0, startY = 0, endX = 0, endY = 1;
                   std::vector<std::pair<float, uint32_t>> stops; };
  struct GtEffect { std::string type; float offsetX = 0, offsetY = 0, blur = 0, angle = 0;
                    uint32_t argb = 0xFF000000; std::string mode; };
  struct GtStyle {                       /* a field's style override (§4.4); unset = the file's */
      std::optional<std::string> font; std::optional<float> size; std::optional<uint32_t> fill;
      std::optional<int> weight; std::optional<bool> italic; std::optional<std::string> stretch;
      std::optional<float> lineSpacing; };
  struct GtObject {
      std::string kind;                  /* element name: Layer, TextBlock, Text3D, Image, Rectangle, Ellipse, or other */
      std::string name;
      float x = 0, y = 0, width = 100, height = 100, depth = 0;
      bool visible = true; float opacity = 1;
      unsigned dataFlags = 0; bool dataFlagsSet = false;   /* Hidden=1 NoEvents=2 ShowVisible=4 */
      std::string mask;                  /* Mask Object, "" for none */
      std::vector<GtEffect> effects;
      /* text */
      std::string text = "Text", fontFamily = "Arial"; float fontSize = 36; int fontWeight = 400;
      bool italic = false; std::string fontStretch = "Normal", textAlign = "Left", verticalAlign = "Top",
      wrapping = "Wrap", autoSize = "Fixed"; float lineSpacing = 0; bool uppercase = false, singleLine = false;
      GtBrush fill, stroke; float strokeThickness = 0;
      /* image */
      std::string bitmapSource, sizeMode = "Centered";
      /* rectangle */
      float radiusX = 0, radiusY = 0;
      /* layer: its composition */
      float compWidth = 0, compHeight = 0; std::vector<GtObject> children;
  };
  struct GtDocument { float width = 1920, height = 1080; std::vector<GtObject> objects;
                      bool hasStoryboards = false; std::vector<std::string> unknownKinds; };
  bool parseGtDocument(const std::string &utf8Xml, GtDocument &out, std::string &error);
  int gtFontWeight(const std::string &name);        /* vMix weight name -> 100..950; 400 when unknown */
  bool gtParseColour(const std::string &text, uint32_t &argb); /* #AARRGGBB or #RRGGBB */
  }
  ```

- [ ] **Step 1: Write the failing test.** One inline XML string per case (§3.2 defaults):
  ```cpp
  /* defaults when attributes are absent */
  parse(R"(<?xml version="1.0" encoding="utf-16"?><Composition Width="1920" Height="1080"><Layer Name="L" Dimensions="1920,1080,0"><Layer.Composition><Composition Width="1920" Height="1080"><TextBlock Name="T" /></Composition></Layer.Composition></Layer></Composition>)");
  const GtObject &t = doc.objects[0].children[0];
  CHECK(t.kind == "TextBlock" && t.text == "Text" && t.fontFamily == "Arial" && t.fontSize == 36);
  CHECK(t.strokeThickness == 0 && t.textAlign == "Left" && t.wrapping == "Wrap" && t.autoSize == "Fixed");
  CHECK(t.width == 100 && t.height == 100 && t.opacity == 1 && t.visible);
  /* attributes and child properties, from Matchup.gtzip's NameL1 */
  CHECK(n.x == 80 && n.y == 90 && n.width == 248 && n.fontWeight == 700 && n.uppercase && n.textAlign == "Center");
  CHECK(n.fill.type == GtBrushType::Solid && n.fill.argb == 0xFF000000 && n.stroke.argb == 0xFFFFFFFF);
  /* the Timer gradient */
  CHECK(g.fill.type == GtBrushType::Linear && g.fill.stops.size() == 2 && g.fill.stops[1] == std::make_pair(1.0f, 0xFFFFFFFFu));
  CHECK(g.fill.stops[0].first == 0.0f);                               /* Position absent = 0 */
  /* mask, data flags, opacity, effects, Text3D, storyboard, unknown element */
  CHECK(img.mask == "Rectangle1" && img.dataFlags == 1 && img.dataFlagsSet);
  CHECK(rect.opacity == 0);
  CHECK(shadowed.effects[0].type == "Shadow");
  CHECK(doc.hasStoryboards);
  CHECK(doc.unknownKinds == std::vector<std::string>{"Ticker"});
  CHECK(gtFontWeight("DemiBold") == 600 && gtFontWeight("Black") == 900 && gtFontWeight("Medium") == 500);
  /* Vietnamese survives (Review Focus 2) */
  CHECK(vn.text == "Nguyễn Văn Á");
  ```
  (Write the exact XML for each case into the test, copying shapes from §3.2 and the owner's Matchup and PRE INGAME snippets.)
- [ ] **Step 2: Run it to see it fail.** Expected: a compile failure.
- [ ] **Step 3: Implement `parseGtDocument`** with `QDomDocument::setContent(QString::fromUtf8(...))`. Passing a QString makes Qt ignore the `utf-16` declaration; passing bytes would make it misdecode. Attributes and `<Kind.Property>` children both set a property. Parse `Location` and `Dimensions` as `x,y,z`. `Visible`, `Locked` and `DataFlags` take vMix's names, and a flags value is a comma or space list (`Hidden`, `NoEvents`, `ShowVisible`). `TextEffect` sets `uppercase` / `singleLine`. A `Layer` reads `Layer.Composition/Composition` recursively into `children`. Any other element directly in a composition is pushed with its `kind` and its name added to `unknownKinds`, once per kind.
- [ ] **Step 4: Run the test.** Expected: `GtDocument: all checks passed`.
- [ ] **Step 5: Commit:** `breadstudio: parse a GT title's document`.

### Task 3: The field rule

**Files:**
- Create: `breadstudio/gt/GtFields.hpp`, `breadstudio/gt/GtFields.cpp`, `breadstudio/gt/test/GtFieldsTest.cpp`
- Modify: `breadstudio/gt/CMakeLists.txt` (target `bread-gt-fields-test`, compiled with `GtPackage.cpp GtDocument.cpp`)

**Interfaces:**
- Consumes: `GtDocument`, `GtObject` (Task 2); `readGtFile` (Task 1).
- Produces:
  ```cpp
  namespace gt {
  enum class GtFieldType { Text, Image, Colour };
  struct GtField { std::string name; GtFieldType type; std::string defaultValue; const GtObject *object; };
  std::vector<GtField> gtFields(const GtDocument &doc);  /* document order */
  }
  ```

- [ ] **Step 1: Write the failing test.**
  - Inline cases (§4.3):
    - a TextBlock directly in a top-level Layer → `T.Text`, default its `Text`;
    - an Image with `DataFlags="Hidden"` → no field;
    - an Image without flags → `I.Source`, default its `bitmapSource`;
    - a Rectangle without flags → no field;
    - a Rectangle with `DataFlags="None"` → `R.Fill.Color`, default `#AARRGGBB` of its fill;
    - a Text3D → `Time.Text`;
    - a TextBlock inside a Layer inside a Layer → no field;
    - an object directly in the root composition (not in a Layer) → no field.
  - **Local** (skipped when `F:/Stream` is absent; folder is `argv[1]`): over every `.gtzip` found recursively, `CHECK(text == 158 && image == 55 && colour == 0)`, and `CHECK(files == 25)`.
- [ ] **Step 2: Run it to see it fail.**
- [ ] **Step 3: Implement `gtFields`.** Objects of each top-level `Layer`'s `children` only. Text and Image are fields unless `Hidden`. Rectangle and Ellipse are fields only when `dataFlagsSet` and not `Hidden`, because shapes default to `Hidden` (§4.3).
- [ ] **Step 4: Run the test** with and without the folder argument. Expected: `GtFields: all checks passed`; without `F:\Stream`, a `SKIP local counts` line too.
- [ ] **Step 5: Commit:** `breadstudio: name a GT title's editable fields as vMix does`.

### Task 4: Clocks

**Files:**
- Create: `breadstudio/gt/GtClock.hpp`, `breadstudio/gt/GtClock.cpp`, `breadstudio/gt/test/GtClockTest.cpp`
- Modify: `breadstudio/gt/CMakeLists.txt` (target `bread-gt-clock-test`)

**Interfaces:**
- Produces:
  ```cpp
  namespace gt {
  struct GtClockTime { int year, month, day, hour, minute, second, millis, weekday; }; /* weekday 0 = Sunday */
  GtClockTime gtNow();                                                  /* local time */
  /* .NET custom date-time format; false when it cannot be formatted (the caller shows the text as written) */
  bool gtFormatDate(const std::string &format, const GtClockTime &t, std::string &out);
  struct GtClockResult { std::string text; bool ticking = false; bool fractions = false; };
  GtClockResult gtExpandClock(const std::string &text, const GtClockTime &now); /* §4.5 Clock */
  }
  ```

- [ ] **Step 1: Write the failing test.** With `t = {2026,10,19,21,5,9,42,1}`:
  ```cpp
  CHECK(gtExpandClock("{0:HH:mm:ss}", t).text == "21:05:09" && gtExpandClock("{0:HH:mm:ss}", t).ticking);
  CHECK(gtExpandClock("{0:hh:mm:ss tt}", t).text == "09:05:09 PM");
  CHECK(gtExpandClock("{0:H:m:s}", t).text == "21:5:9");
  CHECK(gtExpandClock("{0:dddd d MMMM yyyy}", t).text == "Monday 19 October 2026");
  CHECK(gtExpandClock("{0:ss.fff}", t).text == "09.042" && gtExpandClock("{0:ss.fff}", t).fractions);
  CHECK(gtExpandClock("Kick-off {0:HH:mm}", t).text == "Kick-off 21:05");
  CHECK(gtExpandClock("{0:'at' HH}", t).text == "at 21");
  CHECK(gtExpandClock("{0:HH\\h}", t).text == "21h");
  /* Review Focus 3: braces in ordinary text are shown as written */
  CHECK(gtExpandClock("{TEAM}", t).text == "{TEAM}" && !gtExpandClock("{TEAM}", t).ticking);
  CHECK(gtExpandClock("a { b", t).text == "a { b");
  CHECK(gtExpandClock("{{0}}", t).text == "{0}");
  CHECK(gtExpandClock("{0:H}", t).text == "{0:H}");  /* one-letter = .NET standard format: unsupported, as written */
  CHECK(gtExpandClock("player name", t).text == "player name");
  ```
- [ ] **Step 2: Run it to see it fail.**
- [ ] **Step 3: Implement `gtFormatDate` and `gtExpandClock`.**
  - Specifiers (§4.5): d dd ddd dddd, f…fffffff, F…, h hh H HH, m mm, M MM MMM MMMM, s ss, t tt, y yy yyyy. English invariant names.
  - Literals: `'…'`, `"…"` and `\x` are literal; `:` and `/` are literal.
  - Text: `{{` and `}}` are escapes. Only `{0:FORMAT}` placeholders are expanded. Any other `{…}` or an unmatched brace makes the whole text come back as written, as .NET's `string.Format` throwing makes vMix's.
- [ ] **Step 4: Run the test.** Expected: `GtClock: all checks passed`.
- [ ] **Step 5: Commit:** `breadstudio: run clocks in GT title text`.

### Task 5: Countdowns

**Files:**
- Modify: `breadstudio/gt/GtClock.hpp`, `breadstudio/gt/GtClock.cpp`, `breadstudio/gt/test/GtClockTest.cpp`
- Modify: `breadstudio/docs/2026-10-19-breadstudio-gt-title-design.md` §4.5 (record what Step 1 found)

**Interfaces:**
- Produces:
  ```cpp
  namespace gt {
  struct GtCountdownSettings { std::string start = "00:00:00", duration = "00:00:00", format = "mm:ss";
                               bool reverse = false, reverseDisplay = false; };
  bool gtTimeSpanMs(const std::string &text, int64_t &ms);   /* "hh:mm:ss", "hh:mm:ss.fff", "mm:ss" */
  class GtCountdown {
  public:
      explicit GtCountdown(const GtCountdownSettings &settings);
      bool active() const;                 /* duration > 0; a 00:00:00 countdown is none (§6) */
      void start(int64_t nowMs); void pause(int64_t nowMs); void stop(int64_t nowMs); void reset();
      bool running() const;
      std::string text(int64_t nowMs) const;   /* what the field shows, always (it replaces the text value) */
  };
  }
  ```

- [ ] **Step 1: Verify in vMix (spec §4.5 *verify*) before writing the test.** On the owner's PC:
  1. Add `F:\Stream\CACAS24\GTs\MHC\Timer.gtzip`.
  2. In its Title Editor, set the countdown to Duration 00:00:10, Format `mm:ss`.
  3. Record what it shows before Start; while running; after 10 s; after Pause then Start; after Stop; and after Reset.
  4. Repeat with Reverse, then with ReverseDisplay.
  5. In a title with two text fields, set a countdown on the second only, save the preset, and see which `<CountdownSettings>` index it occupies.

  Write the findings into §4.5 in place of the *verify* sentence.
- [ ] **Step 2: Write the failing test** from the findings. The hypothesis to confirm or replace (counting down from Duration, holding at zero):
  ```cpp
  GtCountdown c({"00:00:00", "00:30:00", "mm:ss", false, false});
  CHECK(c.active() && !c.running() && c.text(0) == "30:00");
  c.start(1000); CHECK(c.text(1000 + 159000) == "27:21");       /* CACAS24's frozen reading after 2:39 */
  c.pause(200000); CHECK(c.text(999999) == c.text(200000));
  c.start(300000); CHECK(c.text(300000 + 3600000) == "00:00");   /* holds at zero */
  c.reset(); CHECK(!c.running() && c.text(0) == "30:00");
  CHECK(!GtCountdown({"00:00:00", "00:00:00", "mm:ss", false, false}).active());
  int64_t ms; CHECK(gtTimeSpanMs("00:00:59", ms) && ms == 59000); CHECK(gtTimeSpanMs("30:00", ms) && ms == 1800000);
  CHECK(!gtTimeSpanMs("soon", ms));
  ```
  Add one case each for Reverse, ReverseDisplay and Stop, as Step 1 found them.
- [ ] **Step 3: Run it to see it fail.**
- [ ] **Step 4: Implement `GtCountdown`.** Format the remaining (or elapsed) time with `gtFormatDate`, on a `GtClockTime` whose hour/minute/second/millis are the span's, as vMix formats a span as a time of day.
- [ ] **Step 5: Run the test.** Expected: `GtClock: all checks passed`.
- [ ] **Step 6: Commit:** `breadstudio: count down in a GT title field as vMix does`.

### Task 6: The renderer and the local render test

**Files:**
- Create: `breadstudio/gt/GtRenderer.hpp`, `breadstudio/gt/GtRenderer.cpp`, `breadstudio/gt/GtText.cpp` (DirectWrite layout, outline, fallback), `breadstudio/gt/test/GtRenderTest.cpp`, `breadstudio/gt/test/make-references.ps1`
- Modify: `breadstudio/gt/CMakeLists.txt` (link `d3d11 dxgi d2d1 dwrite windowscodecs`; target `bread-gt-render-test`)

**Interfaces:**
- Consumes: `GtPackage` (1), `GtDocument`, `GtStyle`, `gtFontWeight`, `gtParseColour` (2).
- Produces:
  ```cpp
  namespace gt {
  struct GtWarning { std::string kind; std::string detail; };       /* kinds as in the contract */
  struct GtFrameState {                                             /* values after clocks/countdowns */
      std::map<std::string, std::string> text;      /* "T.Text" -> shown text */
      std::map<std::string, std::string> picture;   /* "I.Source" -> resource name or file path; "" = nothing */
      std::map<std::string, uint32_t> colour;       /* "R.Fill.Color" -> argb */
      std::map<std::string, GtStyle> style;         /* "T.Text" -> override */
  };
  class GtRenderer {
  public:
      GtRenderer(); ~GtRenderer();
      bool init(ID3D11Device *device, std::string &error);            /* the device must have BGRA support */
      void load(const GtPackage &package, const GtDocument &document); /* decodes the package's pictures */
      void setPicture(const std::string &path, std::vector<uint8_t> bgraPremul, int w, int h); /* pre-decoded off-thread */
      static bool decodePicture(const std::string &path, std::vector<uint8_t> &bgraPremul, int &w, int &h); /* WIC, any thread */
      bool draw(ID3D11Texture2D *target, const GtFrameState &state);   /* BGRA, document size, premultiplied, cleared to 0 */
      std::vector<GtWarning> warnings() const;                        /* from load and the last draw, deduplicated */
  };
  }
  ```

- [ ] **Step 1: Write the reference script.** `make-references.ps1 -Source F:\Stream -Out <folder>` drives vMix's HTTP API at `http://127.0.0.1:8088/api/`. vMix's output must be 1920×1080. For each `.gtzip` it:
  1. Calls `Function=AddInput&Value=GT|<path>`. If the input count in `/api/` does not rise, it uses `Title|<path>` and says which type string worked.
  2. Calls `Function=SnapshotInput&Input=<n>&Value=<out>\<relative path>.png`.
  3. Calls `Function=RemoveInput&Input=<n>`.

  Run it on the owner's PC. Expected: 25 PNGs. Note in the script header whether vMix's snapshot carries alpha.
- [ ] **Step 2: Verify picture sizing in vMix (spec §3.2 *verify*).**
  1. With vMix's API `SetImage`, put a 300×100 PNG into a Centered 200×200 picture field; snapshot.
  2. Do the same for an empty value into MAN OF THE MATCH's `IMG PLY.Source` (spec §6 *verify*).

  Record both in the spec (§3.2 Image, §6 Picture values) in place of the *verify*. These two snapshots join the reference folder as `sizing-centered.png` and `empty-value.png`, with their inputs described in the test.
- [ ] **Step 3: Write the failing render test** `GtRenderTest.cpp <gtzip folder> <reference folder> <out folder>`. It:
  1. Creates a D3D11 device with BGRA support and a target per file.
  2. Draws each file with its saved values: each field's default as its state.
  3. Reads the target back through a staging texture (the test's own readback, not the source's) and writes `<out>/<name>.png` and `<name>-diff.png`.
  4. Compares with the reference, composited over black when the reference has no alpha.
  5. Prints, per file, `mean` (mean absolute channel difference, 0–255) and `bad` (% pixels with any channel off by more than 32).
  6. Fails a file when `mean > 2.0 || bad > 0.5`, and exits with the number of failed files. When a folder is absent it prints `SKIP` and exits 0.

  It also renders one line `Nguyễn Văn Á` in a TextBlock and checks the glyph run has no `.notdef` (Review Focus 2).
- [ ] **Step 4: Run it to see it fail.** Expected: compile failure, then (after a stub) 25 failures.
- [ ] **Step 5: Implement shapes, pictures, layers, opacity and masks.**
  - Draw order is document order, first at the bottom.
  - A Layer translates to its Location and draws its children in its composition's space.
  - `Visible="False"` draws nothing. Opacity is a D2D layer's opacity.
  - Brushes are solid colour, or a linear gradient with StartPoint and EndPoint relative to the object's bounds.
  - A Rectangle is rounded by Radius; an Ellipse fills its bounds.
  - An Image draws its resource or field picture per `SizeMode` as Step 2 found.
  - A mask is `PushLayer` with the named sibling's geometry (§3.3).
  - A missing resource adds one `missing-resources` warning per file, listing the names. An unknown kind adds `unsupported`.

  Run the test. Expected: the non-text parts of every file's diff image are clean.
- [ ] **Step 6: Implement text in `GtText.cpp`.**
  - One `IDWriteTextLayout` per text object per distinct (text, style).
  - Font: family, weight (`gtFontWeight`), italic and stretch, with `GtStyle` overriding the object's own.
  - Layout: size, alignment, vertical alignment, wrapping (`singleLine` = no wrap), line spacing, uppercase applied to the string.
  - Fill and stroke: glyph runs become outlines through a custom `IDWriteTextRenderer` (`GetGlyphRunOutline`), filled with the fill brush and, when `strokeThickness > 0`, stroked with the stroke brush.
  - A Text3D is drawn flat and adds a `text3d` warning.
  - A family not in the system collection adds a `font` warning naming the family and its fields. The text falls back to the font vMix falls back to on this PC: start with DirectWrite's system font fallback, and make the render test the judge, since this PC lacks the owner's fonts.

  Run the test. Expected: every file passes, or a file's remaining difference is named in a `known` list printed with the reason, and the owner agrees to it.
- [ ] **Step 7: Implement effects.** `Shadow` is a D2D shadow effect of the object's own drawing, offset by `Offset`, with `Color` and `BlurAmount`, drawn under it. `Skew` is a skew transform by `Angle` about the object's centre. Any other type adds `unsupported`. Run the test. Expected: the 4 shadowed and 2 skewed objects match.
- [ ] **Step 8: Commit:** `breadstudio: draw GT titles with Direct2D`. The PNG outputs and references are not committed.

### Task 7: The `bread_gt_title` source

**Files:**
- Create: `breadstudio/gt/gt-source.cpp`
- Modify: `breadstudio/gt/gt.cpp` (register the source), `breadstudio/gt/CMakeLists.txt`

**Interfaces:**
- Consumes: everything in Tasks 1–6, and `GtSettings.hpp`.
- Produces: OBS source `gt::SourceId` (`OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW`; colour handled as OBS's image source handles a BGRA texture, so a title's colours match vMix's in the render test), the settings keys, and the procs `gt::ProcInfo` and `gt::ProcCountdown` on the source's own proc handler.

- [ ] **Step 1: Device and texture.** In `create`, inside `obs_enter_graphics`:
  1. Take OBS's adapter from `gs_get_device_obj()` → `IDXGIDevice::GetAdapter`.
  2. Create the source's own D3D11 device on it with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`.
  3. Make a BGRA texture of the document size with `D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX`, bound as render target and shader resource.
  4. Open it in OBS with `gs_texture_open_nt_shared`.

  Both sides acquire and release key 0, because all drawing happens on the graphics thread in turn. Direct2D never runs on OBS's device, so OBS's cached pipeline state is never disturbed.
- [ ] **Step 2: Settings to frame state.** Map `update(settings)` onto `GtFrameState`:
  - A field's state is its default, overridden by `values`.
  - A `.Source` value that names a package resource is that resource; otherwise it is a file path.
  - Styles come from `styles`, parsed with `gtFontWeight` and `gtParseColour`.
  - A field in `countdowns` gets a `GtCountdown`, replaced only when its settings change.

  `update` reloads the package when `file` changed. It decodes changed picture files with `GtRenderer::decodePicture` on the calling thread, never the graphics thread (Review Focus 4), and hands the pixels over with `setPicture`.
- [ ] **Step 3: Redraw only on change.** In `video_render`:
  1. Expand clocks (`gtExpandClock(…, gtNow())`) and countdown texts into the frame state.
  2. Redraw through the keyed mutex when anything differs from the last drawn state, or a picture or the file changed. A ticking title therefore redraws once a second; a `fractions` clock or a running fractional countdown redraws every frame.
  3. Draw the texture with `gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA)` inside a pushed blend state, because the texture is premultiplied.

  `get_width` and `get_height` return the document's size, or 0 with no document.
- [ ] **Step 4: Watch the file.** In `video_tick`, once a second, compare the `.gtzip`'s modified time with the loaded one. When it changed, re-read the package off the graphics thread (`obs_queue_task` with `OBS_TASK_UI`), then reload, keeping values (Review Focus 5).
- [ ] **Step 5: Procs.**
  - `gt_info` returns the contract's JSON: `gtFields` with `countdown` set, and `warnings()` plus `file` (missing or unreadable `.gtzip`), `picture` (a field file that cannot be read) and `animations` (`hasStoryboards`).
  - `gt_countdown` calls start/pause/stop/reset on the field's countdown with `os_gettime_ns()/1000000`.
- [ ] **Step 6: Smoke check in BreadStudio.** Build and run BreadStudio. Add a GT input through the OBS source list for now (the desk page comes in Task 8), with `file` set to `Matchup.gtzip`. Expected:
  - It draws over Program with soft edges free of dark fringes.
  - `Timer.gtzip` ticks.
  - Setting `NameL1.Text` through the source's settings JSON changes it on the next frame.
  - Re-saving the file in GT Designer updates it within 2 s.
  - Setting MAN OF THE MATCH's picture field to its 16 MB PNG shows OBS's "frames missed due to rendering lag" unchanged.
- [ ] **Step 7: Commit:** `breadstudio: a GT title source that draws without a browser`.

### Task 8: The Input Select Title page

**Files:**
- Create: `breadstudio/desk/TitlePage.hpp`, `breadstudio/desk/TitlePage.cpp`
- Modify: `breadstudio/desk/InputSelect.cpp` (`kinds[]`: insert after `Kind.Image`; the existing-input constructor: a `gt::SourceId` base gets `TitlePage(base, pages)`), `breadstudio/desk/CMakeLists.txt` (add the files, plus `../gt/GtPackage.cpp` for the thumbnail, include `../gt`), `breadstudio/desk/data/locale/en-US.ini`

**Interfaces:**
- Consumes: `readGtFile`, `GtPackage::thumbnail` (1); `gt::SourceId`, `gt::KeyFile`; `desk::createInput` (`InputModel.hpp`); `InputPage` (`InputPage.hpp`).
- Produces:
  ```cpp
  namespace desk {
  class TitlePage : public InputPage {
  public:
      explicit TitlePage(QWidget *parent);                    /* a new input */
      TitlePage(obs_source_t *base, QWidget *parent);         /* an existing one: apply()/revert() */
      bool ready() const override;                            /* a readable .gtzip is chosen */
      OBSSource create() override;                            /* createInput(gt::SourceId, file's base name, {file}) */
      void apply() override; void revert() override;
  };
  }
  ```
- Locale: `Kind.Title="Title"`, `Type.Title="GT Title"`, `Title.File="Title file"`, `Title.Browse="Browse..."`, `Title.Filter="vMix GT titles (*.gtzip)"`, `Title.NotTitle="This file is not a vMix GT title: %1"`.

- [ ] **Step 1: Implement `TitlePage`.** It shows the path, a Browse… button with the filter, the thumbnail scaled to fit, and the read error in place of the thumbnail when `readGtFile` fails. `ready()` is false until a file reads. `apply` for an existing base writes `file` through `obs_source_update`; `revert` puts back the old path.
- [ ] **Step 2: Wire it** into `kinds[]` (`{"Kind.Title", gt::SourceId, …}`) and the existing-input constructor.
- [ ] **Step 3: Smoke check.** Run BreadStudio and open Input Select. Expected:
  - **Title** is listed after Image.
  - Choosing `Matchup.gtzip` shows its thumbnail and enables OK; OK makes an input named `Matchup` whose tile shows the title.
  - Choosing `not-a-zip.gtzip` keeps OK greyed and shows the error.
  - With `bread-gt` removed from the plugins folder, the row is absent.
- [ ] **Step 4: Commit:** `breadstudio: add a GT title from Input Select`.

### Task 9: The ⚙ Title tab

**Files:**
- Create: `breadstudio/desk/TitleTab.hpp`, `breadstudio/desk/TitleTab.cpp`
- Modify: `breadstudio/desk/InputPages.cpp` (`pagesFor`: after General, when the base is `gt::SourceId`), `breadstudio/desk/CMakeLists.txt`, `breadstudio/desk/data/locale/en-US.ini`

**Interfaces:**
- Consumes: the contract (`gt_info`, `gt_countdown`, keys); `InputSettings::reload()`; `markProjectChanged()` (`Desk.hpp`).
- Produces: `QWidget *makeTitleTab(obs_source_t *input, InputSettings *window);` — null when the input's base is not a GT title.
- Locale: `Settings.Page.Title="Title"`, `Title.Change="Change..."`, `Title.Reset="Reset"`, `Title.Style="Style"`, `Title.Font="Font"`, `Title.Size="Size"`, `Title.Fill="Colour"`, `Title.Weight="Weight"`, `Title.Italic="Italic"`, `Title.Stretch="Stretch"`, `Title.LineSpacing="Line spacing"`, `Title.Start="Start"`, `Title.Pause="Pause"`, `Title.Stop="Stop"`, `Title.Duration="Duration"`, `Title.Format="Format"`, `Title.Warnings="Warnings"`, `Title.NoWarnings="Nothing to report"`.

- [ ] **Step 1: Implement `makeTitleTab`** (§4.4). It reads `gt_info` once, and lists the fields in its order:
  - **Text:** a two-line text box. Each change writes `values[name]` (removed when equal to `default`), calls `obs_source_update` and `markProjectChanged()`.
  - **Style** for each text field: font (`QFontComboBox`), size, colour (with alpha), weight (vMix's names, Thin…ExtraBlack), italic, stretch (UltraCondensed…UltraExpanded), line spacing, and Reset, which removes `styles[name]`. Each control writes its one key of `styles[name]`.
  - **Countdown fields:** Start, Pause, Stop and Reset call `gt_countdown`. Duration and format write `countdowns[name]`.
  - **Picture:** the path, Browse… (png, jpg, jpeg, bmp, gif, tif, tiff) and Reset.
  - **Colour:** a colour button with alpha, writing `#AARRGGBB`.
  - **The file:** the path and Change…. After a change, drop the `values`, `styles` and `countdowns` keys whose names the new `gt_info` lacks, then `window->reload()`.
  - **Warnings:** every warning's `detail` under `Title.Warnings`, or `Title.NoWarnings`.

  The layout is the builder's, following `InputPages.cpp`'s pages (§10).
- [ ] **Step 2: Wire it into `pagesFor`.**
- [ ] **Step 3: Smoke check** (done-when 2 and 4). Run BreadStudio with `Matchup.gtzip`. Expected:
  - 20 text fields and no shapes are listed.
  - Typing in `NameL1.Text` changes Program on the next frame.
  - Typing `Nguyễn Văn Á` shows correctly on Program (Review Focus 2).
  - Changing size and colour applies them, and Reset restores them.
  - With `Timer.gtzip`, a countdown set to 00:00:10 counts and its buttons work.
  - Saving the Project and restarting brings the values and styles back (done-when 6).
- [ ] **Step 4: Commit:** `breadstudio: edit a GT title's fields and style`.

### Task 10: Reading GT inputs from a .vmix

**Files:**
- Modify: `breadstudio/desk/VmixPreset.hpp`, `breadstudio/desk/VmixPreset.cpp`, `breadstudio/desk/test/VmixPresetTest.cpp`
- Create: `breadstudio/desk/test/fixtures/gt-titles.vmix` (hand-written, below)

**Interfaces:**
- Produces, in `VmixPreset.hpp`:
  ```cpp
  enum class VmixKind { …, Title, GtTitle, … };   /* Title = .xaml (type 2); GtTitle = .gtzip (type 9000) */
  struct VmixTitleItem { std::string name, value; bool hasStyle = false;
                         std::string font, fill, style, weight, stretch; double size = 0, lineSpacing = 0; };
  struct VmixCountdown { std::string start, duration, format; bool reverse = false, reverseDisplay = false; };
  /* VmixInput gains: */ std::string titleFile; std::vector<VmixTitleItem> titleItems;
                         std::vector<VmixCountdown> countdowns; std::string dataSourcesXml; int dataLinks = 0;
  /* VmixPresetResult gains: */ std::string presetDir;   /* set by the caller before building */
  std::vector<VmixTitleItem> readVmixTitleItems(const std::string &itemsXml);
  std::vector<VmixCountdown> readVmixCountdowns(const std::string &countdownXml); /* every entry, in order */
  int countVmixDataLinks(const std::string &dataSourcesXml);                      /* <mapper> elements */
  /* the stored path if it exists; else the path's tail under presetDir, dropping leading folders one at a time; "" if none */
  std::string findVmixFile(const std::string &stored, const std::string &presetDir,
                           const std::function<bool(const std::string &)> &exists);
  bool vmixAbsolutePath(const std::string &value);  /* a drive path or a file:/// URL */
  ```

- [ ] **Step 1: Run GitNexus `impact` on `kindOfType`, `vmixBaseId`, `VmixInput` and `readVmixPreset`,** and report the callers.
- [ ] **Step 2: Write the fixture and the failing tests.** `gt-titles.vmix` has three Type 9000 inputs, copied in shape from `F:\Stream\EAFC24\EAFC24.vmix` (TagWinLeft, TagWinRight, TimeMHC) with the attribute values below:
  ```cpp
  CHECK(left.kind == VmixKind::GtTitle && left.titleFile == "C:\\EAFC24\\GT\\TagWin\\TagWin.gtzip");
  CHECK(left.titleItems.size() == 1 && left.titleItems[0].name == "PlayerName.Text" && left.titleItems[0].value == "Player Left");
  CHECK(left.dataLinks == 1);
  CHECK(time.titleItems[0].hasStyle && time.titleItems[0].font == "Helvetica Neue" && time.titleItems[0].size == 192
        && time.titleItems[0].fill == "#FF00FFFF" && time.titleItems[0].style == "Italic" && time.titleItems[0].weight == "Bold");
  CHECK(time.countdowns.size() == 1 && time.countdowns[0].duration == "00:20:00" && time.countdowns[0].format == "mm:ss");
  CHECK(countVmixDataLinks("<dataSources><dataSource name=\"X\" /></dataSources>") == 0);   /* no mapper: not a link */
  CHECK(readVmixTitleItems("<items><item name=\"N.Text\" version=\"2\"><value>Nguyễn Văn Á</value></item></items>")[0].value == "Nguyễn Văn Á");
  /* relocation, with a fake filesystem */
  auto fs = [](const std::string &p) { return p == "F:\\Stream\\EAFC24\\GT\\TagWin\\TagWin.gtzip"
                                        || p == "F:\\Stream\\FVPL SUMMER 2024\\LOGO\\T4M.PNG"; };
  CHECK(findVmixFile("C:\\EAFC24\\GT\\TagWin\\TagWin.gtzip", "F:\\Stream\\EAFC24", fs) == "F:\\Stream\\EAFC24\\GT\\TagWin\\TagWin.gtzip");
  CHECK(findVmixFile("file:///C:/FVPL%20SUMMER%202024/LOGO/T4M.PNG", "F:\\Stream\\FVPL SUMMER 2024", fs) == "F:\\Stream\\FVPL SUMMER 2024\\LOGO\\T4M.PNG");
  CHECK(findVmixFile("C:\\nowhere\\x.gtzip", "F:\\Stream\\EAFC24", fs) == "");
  CHECK(vmixAbsolutePath("file:///C:/a.png") && vmixAbsolutePath("C:\\a.png") && !vmixAbsolutePath("422d1b65\\BG.png") && !vmixAbsolutePath(""));
  ```
  Update the existing expectations: `cut-cacas24.vmix`'s `clock.gtzip` is now `VmixKind::GtTitle` (test lines 123, 332). `vmixBaseId(VmixKind::Title)` stays `"text_ft2_source"` (line 310). The plan step at line 336 expects `gt::SourceId` for the GT input.
- [ ] **Step 3: Run `bread-vmix-test` to see it fail.**
- [ ] **Step 4: Implement.**
  - `kindOfType(9000)` returns `GtTitle`.
  - For `GtTitle`, the reader sets `titleFile = element.text().trimmed()`, the items from the `XML` attribute, the countdowns from `CountdownXML`, `dataSourcesXml` from `DataSourcesXML`, and `dataLinks`.
  - The small XML documents are parsed with `QDomDocument` from a QString, as in Task 2. `findVmixFile` decodes `%XX` and turns `/` into `\`.
  - `vmixBaseId(GtTitle)` returns `gt::SourceId`. Include `../gt/GtSettings.hpp`, adding the include path to `bread-vmix-test` too.
- [ ] **Step 5: Run the test.** Expected: `VmixPreset: all checks passed`.
- [ ] **Step 6: Commit:** `breadstudio: read a vMix preset's GT titles, values and countdowns`.

### Task 11: Building GT inputs from a .vmix

**Files:**
- Modify: `breadstudio/desk/VmixBuild.cpp`, `breadstudio/desk/DeskLayout.cpp:383-404` (set `result.presetDir` from the chosen path), `breadstudio/docs/2026-10-18-breadstudio-vmix-import.md` (the "Title, GT Title" row, as §6's last bullet words it)

**Interfaces:**
- Consumes: Task 10's reader output and `findVmixFile`; the contract's keys and `gt_info`.

- [ ] **Step 1: Run GitNexus `impact` on `buildVmixPreset` and `baseSettings`.**
- [ ] **Step 2: Implement the GT branch.**
  - **File:** `baseSettings` sets `file` to `findVmixFile(titleFile, presetDir, fileExists)`, or `titleFile` when nothing is found.
  - **Values:** `values[name] = value` per item; a `.Source` value that `vmixAbsolutePath` accepts is relocated the same way. `styles[name]` comes from an item with `hasStyle`. `data_sources` is the raw XML.
  - **Countdowns:** after `createInput`, read `gt_info`. Each countdown with a duration other than `00:00:00` goes to the field Task 5 Step 1 found for its index, written to `countdowns` with `obs_source_update`, and that field's `values` entry is removed, because it was a frozen reading (§6).
  - **Report rows** (`addRow`, all under the input's name):
    - a file or picture found by the rule: had "file '<stored>'", why "found at '<found>'";
    - a `.gtzip` not found: why "the title file was not found; the input is empty";
    - an item name `gt_info` lacks: why "the title has no field of this name";
    - one row per `gt_info` warning: had its kind, why its detail;
    - `dataLinks` rows: had "linked to a data source", why "not live until the data link is built";
    - the xaml `Title` row stays as it is.
- [ ] **Step 3: Smoke check** (done-when 5). Import `F:\Stream\EAFC24\EAFC24.vmix`. Expected:
  - `TagWinLeft` reads "Player Left" and `TagWinRight` "Player Right", each with a "found at" row.
  - `TimeMHC` is cyan, italic and bold at 192, showing 20:00 stopped.
  - The report lists the data links and the overlays row.

  Import the CACAS24 and FVPL presets too. Expected: the FVPL logos are drawn, each with a "found at" row, and CACAS24's Timer shows 30:00, not "27:21".
- [ ] **Step 4: Commit:** `breadstudio: import vMix GT titles as GT title inputs`.

### Task 12: Smoke checklist and the performance check

**Files:**
- Modify: `breadstudio/docs/DESK-SMOKE.md` (a "GT titles" section)

- [ ] **Step 1: Add the section,** with one line per §1.2 done-when item and the steps from Tasks 7–11's smoke checks. Add the performance line: 8 GT inputs, 4 on Program as layers at 1080p60, 10 minutes, "frames missed due to rendering lag" stays 0 (done-when 8). Add a 1280×720 canvas line: titles scale to fit, not crop.
- [ ] **Step 2: Run the whole section on the owner's PC.** Record the date and results in the file, and the render test's per-file scores.
- [ ] **Step 3: Update the roadmap.** In the workspace's `docs/superpowers/specs/2026-10-12-broadcast-roadmap-design.md` (outside this repo, not committed here), reword R6 and R7 and §3's BreadOverlay row as spec §11 states, citing the owner's decision of 2026-10-19.
- [ ] **Step 4: Commit:** `breadstudio: smoke-check GT titles`.
