# BreadStudio — GT title input — Design

Date: 2026-10-19
Status: design agreed with the owner in conversation (2026-10-19); this written
spec awaits the owner's review.
Parent: workspace `docs/superpowers/specs/2026-10-12-broadcast-roadmap-design.md`.
§11 lists what this spec changes there.
Builds on: `2026-10-18-breadstudio-vmix-import.md` (the .vmix import, whose
"Title, GT Title" row this spec fulfils for GT titles).
Base: OBS Studio 32.2.2, as pinned by S0.

## 1. Purpose

The owner runs esports broadcasts (EAFC24, CACAS24, FVPL Summer 2024) whose
graphics are vMix GT titles: `.gtzip` files made in vMix GT Designer. BreadStudio
must put those titles on air **natively** — drawn by BreadStudio itself, not by a
browser source — and let the operator change their text and pictures, as vMix's
Title Editor does.

This is the first of three projects the owner agreed, in this order:

| # | Project | Spec |
|---|---|---|
| 1 | **GT title input**: open a `.gtzip`, draw it as vMix does, clocks, a field panel, the .vmix import | **This document** |
| 2 | Overlay channels 1–4 and the title animations: in/out, hotkeys, obs-websocket requests for Stream Deck / Companion | Not written |
| 3 | BreadOverlay data link: a self-hosted Grist sheet and a relay at the venue, pushed live to title fields; rows, Auto Next | Not written |

Project 1 is useful on its own: it replaces vMix for titles typed by hand.

### 1.1 Why native

The owner's two constraints (2026-10-19):

- **Graphics must keep working when the venue's internet drops.**
- **No browser sources for titles.** Each browser source is a Chromium instance;
  several of them are too heavy for a PC that is also mixing and encoding.

So the title is drawn inside BreadStudio, from the file on disk, with nothing
on the network.

### 1.2 Done when

On the production PC (vMix and BreadStudio on the same PC, so a font that is not
installed falls back the same way in both — §7):

1. Each of the 25 `.gtzip` files under `F:\Stream` opens as an input and, with
   the values saved in the file, matches vMix's own rendering of the same file
   (§9) — layout, fonts, colours, masks, shadows — with differences no larger
   than anti-aliasing.
2. An input's ⚙ → **Title** tab lists every editable field (§4.3) — and no
   others. Across the 25 files that is 158 text fields (156 TextBlocks and the
   two Timer files' Text3D) and 55 picture fields.
   Typing in a text field or choosing a picture shows on Program on the next
   frame.
3. `Timer.gtzip` added by itself shows the current time and ticks every
   second. Imported from CACAS24, the same title is a 30:00 countdown in
   `mm:ss` that Start, Pause, Stop and Reset in the Title tab control, as vMix's
   do (§4.5).
4. A field's own font, size, colour and style can be changed in the Title tab,
   and the ones a preset saved are drawn: EAFC24's `TimeMHC` is cyan, italic,
   bold Helvetica Neue at 192.
5. Importing `F:\Stream\EAFC24\EAFC24.vmix` makes every GT input a GT title
   input showing the values the preset saved: the input `TagWinLeft` reads
   "Player Left", `TagWinRight` reads "Player Right" — though the preset names
   the file `C:\EAFC24\GT\TagWin\TagWin.gtzip`, which is now under `F:\Stream`.
6. Saving the Project and restarting BreadStudio brings every title back with
   its values.
7. A missing font, a missing `.gtzip` and a missing picture are each named to
   the operator (§7); none crashes or leaves a blank input without saying why.
8. With 8 GT title inputs in the Project and 4 of them shown on Program at
   1080p60, OBS's "frames missed due to rendering lag" stays at 0.

## 2. Goals, non-goals, anti-goals

**Goals:** §1.2.

**Not in this project:**

- Animations. A title is drawn in its final, on-air state; its `<Storyboard>`
  is read and kept, and played by project 2.
- Overlay channels and anything about how a title goes on or off air beyond
  being an ordinary input (project 2). Hotkeys and obs-websocket requests for
  the countdown's Start / Pause / Stop / Reset also come with project 2; here
  they are buttons in the Title tab.
- Data sources (project 3). Links the .vmix preset carries are kept, not run
  (§6).
- Designing or editing a title's layout. GT Designer stays the tool for making
  titles; BreadStudio never writes a `.gtzip`.
- vMix `.xaml` titles (input type 2). They stay as the import makes them today.
- What none of the owner's files use (§3.4): drawn, reported.

**Anti-goals — failures even if everything else works:**

- **A title that looks almost right on air without warning.** A substituted
  font, text that wraps where vMix's does not, a picture stretched where vMix
  centres it. Where BreadStudio cannot draw something as vMix does, it says so
  (§7); it never quietly approximates.
- A browser source, or any network dependency, in the title path.
- Copying vMix's code. §3 is what the owner's files and vMix's engine were
  found to do; BreadStudio's renderer is written from that description and
  checked against vMix's output (§9). The decompiled engine is not committed
  or quoted.

## 3. The `.gtzip` format

Established by reading the owner's 25 files and vMix's `GraphicsEngine.dll`
(2026-10-19). Where this section says *verify*, the behaviour was inferred and
§9's comparison decides.

### 3.1 The package

An OPC zip (`[Content_Types].xml` present). Its parts:

| Part | Contents |
|---|---|
| `/document.xml` | The title (§3.2). vMix reads it as **UTF-8**, though its declaration says `utf-16`; the owner's files are UTF-8. |
| `/resources.xml` | `<resources><resource filename="NAME"><source guid="GUID">NAME</source>…</resource></resources>`: each picture the document names (`Bitmap Source="NAME"`) is stored as the part `/GUID`. A resource may list several sources (an image sequence); the owner's files have one each. |
| `/thumbnail.png` | vMix's own 640×360 render of the title with its saved values. |
| `/GUID` | The picture bytes: PNG, and one JPEG with EXIF data (LINEUP 5). |

Six of the owner's files name a picture (a layout-guide JPEG) that is not in the
package. Most are `Hidden` or `Visible="False"`; PRE INGAME's is visible and
TEAM vs TEAM's is a picture field. Such a picture draws nothing, and the file
gets one warning listing them all, not one per picture (§7).

### 3.2 The document

A tree of XAML-style elements. The root is
`<Composition Width="1920" Height="1080">`, holding `Layer`s and, optionally,
`Storyboard`s. A property is either an attribute (`Opacity="0"`) or a
child element `<Type.Property>` (`<TextBlock.Fill><Brush …/></TextBlock.Fill>`).

Common to every object (attribute, default):

| Property | Default | Meaning |
|---|---|---|
| `Name` | — | Unique within the title; the field name's stem (§4.3) |
| `Location` | `0,0,0` | Top-left in the parent composition, px (x,y,z) |
| `Dimensions` | `100,100,0` | Width, height, depth, px |
| `Visible` | `True` | |
| `Opacity` | `1` | 0–1 |
| `DataFlags` | `None` (`Hidden` for shapes, §4.3) | Flags: `Hidden`=1, `NoEvents`=2, `ShowVisible`=4 |
| `Mask` | none | `<X.Mask><Mask Object="NAME"/></X.Mask>` (§3.3) |
| `Effects` | none | `<X.Effects><Effect Type="…"/>…` |
| `Locked` | — | Designer-only; ignored |

Objects:

- **`Layer`** — a nested `<Layer.Composition><Composition W H>` placed at its
  `Location`. Nesting may go deeper than one level.
- **`TextBlock`** — `Text` (default "Text"), `FontFamily` ("Arial"), `FontSize`
  (36), `FontWeight` (Regular; the owner uses Medium, DemiBold, Bold,
  ExtraBold, Black), `FontStyle`, `FontStretch` (Normal), `TextAlign` (Left;
  Center, Right), `VerticalAlign` (Top; Center), `TextWordWrapping` (Wrap),
  `LineSpacing` (0), `TextEffect` (flags: `Uppercase`, `SingleLine`),
  `AutoSize` (Fixed), `Fill` brush, `Stroke` brush, `StrokeThickness`
  (**0 — a `Stroke` brush with no thickness draws nothing**; most of the
  owner's text has a stroke brush and no thickness).
- **`Text3D`** — a TextBlock with extrusion depth (`Dimensions` z). Used only by
  the two Timer files.
- **`Image`** — `<Image.Bitmap><Bitmap Source="NAME"/>`, `SizeMode` (default
  `Centered`; also Normal, Stretch, TopRight). *Verify* what Centered does to a
  picture whose size differs from `Dimensions` — the owner swaps logos and
  player photos of other sizes into these fields.
- **`Rectangle`** (`Radius` x,y; corner `Style` Rounded), **`Ellipse`** —
  `Fill`, `Stroke`, `StrokeThickness`.
- **`Brush`** — `Color="#AARRGGBB"` for a solid colour; with `StartPoint`,
  `EndPoint` (0–1, relative to the object) and `<Brush.Stops><GradientStop
  Position Color/>` a linear gradient; `Type="Transparent"`; `WrapX`/`WrapY`
  (Clamp).
- **`Effect`** — `Type`: `Shadow` (with `Offset`, `Color`, `BlurAmount`,
  `Mode`), `Skew` (with `Angle`), and, unused by the owner, `GaussianBlur`,
  `FlipX`, `FlipY`, `Reflection`, `Composite`.
- **`Storyboard`** — `<Storyboard.Animations>` of `Fly`, `Fade`, `Reveal`, …
  Kept for project 2; not drawn here.

### 3.3 Masks

`Mask Object="NAME"` clips the object to the shape of the named object in the
**same composition** — in the owner's files always a `Rectangle` (112) or an
`Ellipse` (1) with `Opacity="0"`, so the shape itself is never seen. The clip
follows the shape's geometry, rounded corners included. 113 objects across the
files are masked, so titles are wrong without this.

### 3.4 What the owner's files use

Counted over all 25 files: 156 TextBlocks, 2 Text3D, 129 Images, 32
Rectangles, 1 Ellipse, 113 Masks, 2 gradient brushes, 4 Shadows, 2 Skews, 5
files with a Storyboard. Fonts: CruyffSansVN, CruyffSansVNCondensed, SCHABO,
Helvetica Neue, Bangers.

**Everything in §3.2 must be drawn**, except Text3D's depth: a Text3D is drawn as
flat text with its fill and stroke, and the input says its depth is not drawn
(builder's call, §10 — no owner title is known to depend on the depth). Object
types §3.2 does not name (vMix also has Ticker, QR, Line, Triangle, Grid,
image sequences) are not drawn; the input names them (§7).

## 4. The GT title input

### 4.1 Adding one

The Input Select window gains a **Title** page: choose a `.gtzip` file, see its
thumbnail, OK. The input is named after the file, as the desk names other
inputs. Like every desk input, it is a scene whose base is the new source
(`desk::createInput`), so layers, tiles, MultiView, tally and triggers work on
it unchanged.

### 4.2 What it shows

The title's composition, scaled to the canvas when the canvas is not the
title's size, with transparency kept (straight alpha into OBS). Fields show
their current values (§4.3); clocks run (§4.5). A title with nothing changing
costs no drawing per frame — it is redrawn only when a value, a clock or a
picture changes.

### 4.3 Fields

A field is a value the operator may change. vMix's rule, which BreadStudio keeps:

- Only objects that sit **directly in a top-level `Layer`'s composition** are
  fields; anything nested deeper is not.
- TextBlock / Text3D → `NAME.Text`; Image → `NAME.Source`; Rectangle / Ellipse →
  `NAME.Fill.Color`.
- Text and Image are fields unless their `DataFlags` include `Hidden`. Shapes
  are `Hidden` by default and are fields only when the designer cleared it —
  none of the owner's are.

The names are vMix's, so a .vmix preset's values (§6) and, in project 3, a data
source's columns, apply to them without translation.

### 4.4 The Title tab

The input's ⚙ gains a **Title** tab, listing the fields in document order:

- **Text:** a text box. Each edit shows on Program on the next frame. Beside it,
  the field's **style**, as vMix's Title Editor has it: font, size, fill
  colour, bold/weight, italic, stretch, line spacing — each starting at what
  the file says, with a Reset to the file's own. A text field with countdown
  settings (§4.5) also has **Start, Pause, Stop, Reset** and its duration and
  format.
- **Picture:** the current file, a Browse… button, and a Reset to the file's
  own picture. A field value is a file path; a picture that cannot be read is
  named (§7) and the field draws nothing.
- **Colour:** a colour button.

Also on the tab: the `.gtzip` path with a Change… button (the field values are
kept where the new file has the same field names), and the list of warnings
(§7).

### 4.5 Clocks and countdowns

**Clock.** A text value containing `{0:FORMAT}` is formatted with the PC's
current local time, as .NET's custom date-and-time format strings do (the
owner's files use `HH:mm:ss` and `hh:mm:ss tt`). The builder supports the .NET
specifiers an operator would put in a title (H, HH, h, hh, m, mm, s, ss, f…, tt,
d, dd, ddd, dddd, M, MM, MMM, MMMM, yy, yyyy, literal text and quotes), and shows
a text it cannot format as written, as vMix does.

**Countdown.** vMix's per-field countdown, which the owner's presets use
(CACAS24 Timer 30:00 and Timer2 59 s, EAFC24 TimeMHC 20:00, FVPL COUNTDOWN
30:00 and GIFTCODE IN 45 s). Its settings, as a preset stores them:

```xml
<CountdownSettings>
  <StartString>00:00:00</StartString>
  <DurationString>00:30:00</DurationString>
  <Format>mm:ss</Format>
  <Reverse>false</Reverse>
  <ReverseDisplay>false</ReverseDisplay>
</CountdownSettings>
```

While it runs, the field shows the time in `Format` and replaces the text
value; Start, Pause, Stop and Reset behave as vMix's. A countdown's state is not
saved: a restarted BreadStudio shows it stopped and reset. *Verify* in vMix
(§9), before building it: the direction it counts (from Duration down to zero,
or from Start up to Duration), what `Reverse` and `ReverseDisplay` change, what
it shows when it ends, what Stop shows against Reset, and which field each
`<CountdownSettings>` belongs to (the array has no field names; the owner's
titles have one countdown each).

A clocked or counting title redraws once a second, or every frame when its
format shows fractions.

## 5. Saving

The input's source settings hold the `.gtzip` path, every field value and
style that differs from the file's own, and each field's countdown settings. The `.gtzip` is referenced, not copied, as vMix
does; the Project file therefore saves titles with no new mechanism.

## 6. The .vmix import

Today a GT input (type 9000) becomes an empty text source and a report row
(`VmixBuild.cpp`). It becomes a GT title input:

- **File:** the `<Input>` element's text is the `.gtzip`'s full path. When no
  file is there, the importer tries the path's tail under the preset's folder,
  dropping leading folders one at a time: `C:\EAFC24\GT\TagWin\TagWin.gtzip`
  with a preset at `F:\Stream\EAFC24\EAFC24.vmix` finds
  `F:\Stream\EAFC24\GT\TagWin\TagWin.gtzip`. A file found that way is a report
  row naming both paths; a file not found at all is still made, empty, and
  reported — as the import treats a missing video file.
- **Values:** the `XML` attribute holds
  `<items><item name="PlayerName.Text" version="2"><value>…</value></item>…</items>`;
  each value is applied to its field. A name the file does not have is
  reported. An item may carry the field's style:
  `<item name="Time.Text"><Font Name="Helvetica Neue" Size="192"
  FillColor="#FF00FFFF" Style="Italic" Weight="Bold" Stretch="Normal"
  LineSpacing="0"/><value>08:49</value></item>`; it is applied as the field's
  style (§4.4).
- **Countdowns:** the `CountdownXML` attribute (§4.5). A `<CountdownSettings>`
  whose `DurationString` is `00:00:00` is no countdown — most of the owner's
  inputs carry one — and is ignored. A real one makes the field a countdown,
  stopped and reset: the text value the preset saved beside it (CACAS24's
  "27:21") is a frozen reading, not the field's text, and is not shown.
- **Picture values** are paths or `file:///` URLs
  (`file:///C:/FVPL SUMMER 2024/LOGO/T4M.PNG`), and are found with the same
  rule as the `.gtzip`, case-insensitively, as Windows paths are. Each picture
  found by the rule is a report row naming both paths, so a wrong match is
  never silent. A value that is the package's own resource name (TagCamIG's
  `BG.Source`) is that resource. An empty value (MAN OF THE MATCH's `IMG PLY`)
  means whatever vMix draws for it — *verify* in vMix (§9) whether that is
  nothing or the file's own picture.
- **Data source links:** the `DataSourcesXML` attribute
  (`<dataSources><dataSource name="FIELD"><mapper instanceId table column
  format row/>…`) is stored on the input unchanged, for project 3, and each
  `<mapper>` is one report row: "linked to a data source — not live until the
  data link is built". A `<dataSource>` with no `<mapper>` (CACAS24 has one)
  is not a link and is not reported.
- The same `.gtzip` used by several vMix inputs (TagWinLeft, TagWinRight)
  becomes several inputs, each with its own values.
- The spec table's row "Title, GT Title" becomes, for GT titles, "a GT title
  input; its values carried; its animation, overlay channel and data links
  reported". `.xaml` titles are unchanged.

## 7. Warnings

Each is shown in the input's Title tab and, on an import, as a report row. None
stops the input from being made.

| Situation | What the operator is told | What is drawn |
|---|---|---|
| A font the title names is not installed | the font's name and the fields that use it | the text in the fallback font vMix uses on the same PC (owner, 2026-10-19: fonts are the owner's to install; with one missing, vMix and BreadStudio must look the same) |
| The `.gtzip` is missing or not a GT title | the path | nothing |
| A picture field's file cannot be read | the field and the path | nothing in that field |
| An object type or effect §3.4 does not draw | its name and the object | the rest of the title |
| A Text3D | that its depth is not drawn | flat text |
| The title has animations | that they play once overlay channels exist (project 2) | the final state |

## 8. Constraints

- Windows, as BreadStudio's CI builds it. The renderer may use Direct2D and
  DirectWrite (builder's call, §10) — vMix itself draws GT titles with
  Direct2D.
- Drawn on the GPU into the frame OBS composites: no copy of each frame through
  the CPU.
- The new code is a module under `breadstudio/` (a new source plugin, beside
  `breadstudio/ndi`), plus the desk's Title page and tab. The one OBS file
  touched is `plugins/CMakeLists.txt`, whose `UPSTREAM-EDITS.md` row grows from
  four lines to five.
- Locale keys beside the desk's others; no user-facing text hard-coded.
- No new runtime dependency beyond what Windows provides. Reading the zip may
  use what the tree already carries (Qt, or the zip library OBS vendors).

## 9. Checking against vMix

The 25 files are the test set; vMix is installed on the owner's PC.

- **The reference picture** for each file is made on the same PC as the render
  test, whatever fonts it has, so both programs fall back alike. It is vMix's
  own render at 1920×1080
  with the file's saved values: the title added as a vMix input and captured
  with vMix's HTTP API (`/api/?Function=SnapshotInput&Input=N&Value=path.png`).
  `thumbnail.png` (640×360, also vMix's) is a coarse check for files where
  vMix is not at hand.
- **Where the tests' data lives (builder's call):** the owner's titles,
  pictures, presets and fonts are not committed. They are large (one picture is
  16 MB), the fonts are licensed, and the presets carry the owner's events. Two
  kinds of test:
  - **Committed:** small `.gtzip` and `.vmix` fixtures written for the tests,
    covering each element, the field rule, masks, the values, the relocation
    rule and the warnings. These run anywhere.
  - **Local:** a render test pointed at `F:\Stream` and a folder of vMix
    reference pictures, skipped when they are absent, run by the owner on the
    production PC.
- The render test draws each file and compares it with its reference; the
  differences allowed are anti-aliasing, reported as a per-file score so a
  regression shows.
- The parser and the field rule (§4.3) are checked against the counts in §3.4
  and §1.2 (158 text, 55 picture fields) in the local test.
- §6 is checked on the real presets, `F:\Stream\EAFC24\EAFC24.vmix`,
  `F:\Stream\CACAS24\CACAS24.vmix` and the FVPL ones, in the local test, beside
  the existing `VmixPresetTest` cases.

## 10. Left to the builder

- The drawing technology (Direct2D/DirectWrite is the suggestion, and how its
  output reaches OBS's D3D11 texture), the caching, and the module's layout.
- Text3D drawn flat (the owner can override once titles are seen side by side).
- The layout of the Title page and tab, following the desk's existing pages.
- The exact behaviours marked *verify* in §3, §4.5 and §6, settled by §9.
- How DirectWrite's fallback is made to match vMix's for a missing font.

## 11. Changes to the roadmap

Decided by the owner on 2026-10-19:

- **R6 (BreadOverlay's form):** BreadOverlay is local-first. Its sheet (a
  self-hosted Grist) and its relay run on a PC at the venue; the internet is
  needed only to edit from outside the venue.
- **R7 (data-driven titles):** titles are drawn natively by BreadStudio, not as
  a BreadOverlay web page in a browser source. BreadOverlay owns the data and
  pushes it to BreadStudio's title fields (project 3).
- A web renderer of `.gtzip` for other switchers is not planned; it is built
  only when someone needs titles outside BreadStudio.
