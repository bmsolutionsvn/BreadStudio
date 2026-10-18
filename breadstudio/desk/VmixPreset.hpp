#pragma once

#include <string>
#include <vector>

namespace desk {

/* What a vMix input is, as far as the desk needs to know. */
enum class VmixKind {
	Colour,
	Video,
	Image,
	Capture, /* Camera, Video Capture */
	AudioIn, /* an audio capture device */
	AudioFile,
	Media, /* Video or Audio file */
	NDI,
	Browser,
	DesktopCapture,
	Title,   /* Title, GT Title */
	Unknown, /* a Type this spec cannot name: reported, never guessed */
	Skipped, /* a kind with no desk equivalent: reported */
};

/* One vMix input, read straight from the preset. */
struct VmixInput {
	int number = 0;            /* the input's order in the bar: number is position */
	std::string title;         /* the input's name */
	std::string originalTitle; /* what OriginalTitle said, for the report */
	int type = -1;             /* vMix's Type as written, for the report */
	VmixKind kind = VmixKind::Unknown;
	std::string file;   /* a path, for Video/Image/Audio/Media */
	std::string device; /* a device or NDI name, for Capture/AudioIn/NDI */
	std::string colour; /* the colour value, for Colour */

	bool hasGeometry = false; /* vMix carries a matrix for this input; Task 2 converts it */
	double x = 0, y = 0, width = 0, height = 0, rotation = 0;
	/* vMix's Clipping, as the fraction of the source cropped from each edge (0 is
	 * no crop). vMix stores the two boundary positions per axis; the reader turns
	 * them into the desk's crop, which the base holds (obs_sceneitem_set_crop). */
	double cropLeft = 0, cropTop = 0, cropRight = 0, cropBottom = 0;

	/* vMix's own matrix for the **Program** placement, kept raw and relative: what
	 * the preset writes for the input's own Main placement. The list is index 0 —
	 * the Main/Program placement — followed by one MatrixPosition per overlay
	 * **layer**, which is what the Position tab ("Layers / Multi View") and the
	 * serialized `<overlay>` elements are; the desk builds no layers, so only index
	 * 0 is the desk's geometry. Pan and zoom are fractions of the frame and
	 * rotation is degrees, so they are not the geometry above yet — the desk turns
	 * them into it, and into the base's obs_transform_info/crop, in Task 2, where
	 * the source's own pixel size is known; the reader cannot know it, so it carries
	 * the matrix and nothing more. `matrixCount` is how many placements the preset
	 * holds for this input: above one, the overlay layers' placements are not the
	 * desk's, so Task 2 reports them. `matrixMoved` is false when the Main matrix is
	 * the identity, so a full-frame input is not mistaken for a placed one. What the
	 * desk cannot hold — a mirror, a nested MultiplyPosition, a border, a rotate
	 * origin — is what makes an applied matrix an approximation, which Task 2 puts
	 * on the report (V4). `postZoomX/Y` and `sourceRotation` and `hidden` are
	 * serialized vMix fields the desk reports (it cannot apply them exactly); each
	 * is its vMix default when the preset carries nothing. */
	int matrixCount = 0;
	bool matrixMoved = false;
	double panX = 0, panY = 0;
	double zoomX = 1, zoomY = 1;
	double rotateX = 0, rotateY = 0, rotateZ = 0;
	double postZoomX = 1, postZoomY = 1; /* applied after Pan in vMix */
	int sourceRotation = 0;              /* R0/R90/R180/R270, in degrees */
	bool hidden = false;                 /* hidden on Program */

	double volume = 1.0; /* the strip's gain, vMix's VolumeF */
	bool muted = false;  /* the strip's ♪ */
};

/* One line of the report (§5.3): what vMix had, and why it is not here. */
struct VmixReportRow {
	std::string input; /* the vMix input's title, or "" for a preset-wide row */
	std::string had;   /* what vMix had */
	std::string why;   /* why it was skipped or approximated */
};

/* What a preset produced. `inputs` is in vMix's number order. */
struct VmixPresetResult {
	std::vector<VmixInput> inputs;
	std::vector<VmixReportRow> report;
	int active = 0;   /* the active number, for the desk's Program */
	int preview = 0;  /* the preview number, for the desk's Preview */
	std::string name; /* the preset's own name, for the new Project */

	/* The output frame the preset's matrices are relative to: the
	 * `<OutputFormat OutputSize="WxH">` the file carries, 0 when it names none.
	 * Task 2 needs it to turn a matrix into a shot; with no frame a shot cannot be
	 * computed, so it is reported rather than guessed. */
	int outputWidth = 0;
	int outputHeight = 0;

	/* The two vMix sections the desk has no home for (spec §5.2): overlays 1-4 and
	 * transitions. The reader carries whether the preset has them (presence, not the
	 * routing) so the build half can report them rather than pass them over (V4).
	 * The one file, or none, is what the reader can tell cheaply; the shape is not
	 * documented (V7). */
	bool overlays = false;
	bool transitions = false;
};

/* Reads one .vmix. False, with `error` set, only when the bytes are not a
 * preset at all; a preset with nothing the desk can back still returns true,
 * with the reasons in `result.report`. */
bool readVmixPreset(const std::string &xml, VmixPresetResult &result, std::string &error);

/* A preset can hold more inputs than the desk's bar. Keeps the first `limit` and
 * moves the rest onto the report (V5); a limit of 0 or less means no bar at all.
 * The desk passes its own limit in, so the rule is the desk's, not the preset's. */
void limitVmixInputs(VmixPresetResult &result, int limit);

/* The report rows as plain text, one row per line, the three fields tab-separated
 * (spec §5.3): the body the import report window shows and copies to the clipboard,
 * so what is on screen and what is copied cannot drift. Empty when there is nothing
 * to report — "the whole preset came across". Pure, so the format is proven without
 * a widget while the window's own decoration (the localized headings and the empty
 * line) stays out of it. */
std::string vmixReportText(const std::vector<VmixReportRow> &rows);

/* The building half (spec §5.4). Walks `result` and makes each input through the
 * desk's own InputModel, so an imported input is a desk input, not a hand-built
 * source: the same fixed shape, the same loop refusal, the same numbering. Every
 * input it cannot make — a source the desk has no type for, one whose creation
 * failed, an absent device — is appended to `result.report`, never dropped
 * (V4), and one failure never aborts the rest. `limit` is the desk's bar limit
 * (V5); 0 or less means no bar. The OBS calls live in VmixBuild.cpp; the
 * decisions below are pure and tested without OBS. */
void buildVmixPreset(VmixPresetResult &result, int limit);

/* Pure: the OBS source type a kind is made with, or null when the desk has no
 * input for it at all — a Skipped entry, or an Unknown one, which Task 1 does not
 * even carry. That null is the "Makes no input" decision the plan asks for; a
 * Skipped entry keeps the row Task 1 already put on the report. */
const char *vmixBaseId(VmixKind kind);

/* One input the builder will make, and the OBS type it needs. `input` points into
 * the result's own list, which the caller must not resize while a plan is in use. */
struct VmixBuildStep {
	const VmixInput *input;
	const char *baseId; /* vmixBaseId(input->kind), never null */
};

/* Pure: the over-limit split (V5, Task 1's `limitVmixInputs`) applied to `result`,
 * then the inputs the builder will make, in number order. A kind with no desk
 * base is skipped here, its report row already written by the read. No OBS. */
std::vector<VmixBuildStep> planVmixBuild(VmixPresetResult &result, int limit);

/* The base's geometry, as the desk's own transform page would hold it: the base's
 * top-left in pixels, its scale of its own pixels, and its rotation in degrees. */
struct VmixPlacement {
	double x = 0, y = 0;           /* the base's top-left */
	double scaleX = 1, scaleY = 1; /* what the base's own pixels are multiplied by */
	double rot = 0;                /* degrees */
};

/* Pure: turns a carried vMix matrix into a shot, from the two sizes the reader
 * cannot know — the preset's output frame and the source's own pixel size (known
 * only once the source exists, via obs_source_get_width/height). vMix fits the
 * whole source into the frame, then scales by Zoom, pans (a fraction of the frame)
 * and rotates by degrees — a relative placement this fork approximates, so an
 * applied matrix is reported (V4). A stored Zoom of 0 is vMix's "unset", which vMix
 * coerces to 1, and is read as 1 here. Returns false when the shot cannot be
 * computed (no frame or no source size): the caller reports the shot instead of
 * applying a wrong one, because a wrong geometry that looks right is worse than a
 * reported one. `out` is unchanged when false. */
bool vmixPlacement(const VmixInput &input, double frameW, double frameH, double srcW, double srcH, VmixPlacement &out);

} // namespace desk
