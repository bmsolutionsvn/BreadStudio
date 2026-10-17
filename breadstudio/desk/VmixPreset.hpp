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
	double cropLeft = 0, cropTop = 0, cropRight = 0, cropBottom = 0;

	/* vMix's own matrix, kept raw and relative: what the preset writes for the
	 * input's placement on an output. Pan and zoom are fractions of the frame and
	 * rotation is degrees, so they are not the geometry above yet — the desk turns
	 * them into it, and into the base's obs_transform_info, in Task 2, where the
	 * source's own pixel size is known; the reader cannot know it, so it carries
	 * the matrix and nothing more. `matrixCount` is how many outputs the preset
	 * holds a matrix for: above one, no single matrix is the desk's geometry, so
	 * Task 2 takes Program and reports the rest. `matrixMoved` is false when that
	 * matrix is the identity, so a full-frame input is not mistaken for a placed
	 * one. What the desk cannot hold — a mirror, a nested MultiplyPosition, a
	 * border, a rotate origin, crop — is what makes an applied matrix an
	 * approximation, which Task 2 puts on the report (V4). */
	int matrixCount = 0;
	bool matrixMoved = false;
	double panX = 0, panY = 0;
	double zoomX = 1, zoomY = 1;
	double rotateX = 0, rotateY = 0, rotateZ = 0;

	double volume = 1.0; /* the strip's fader */
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
};

/* Reads one .vmix. False, with `error` set, only when the bytes are not a
 * preset at all; a preset with nothing the desk can back still returns true,
 * with the reasons in `result.report`. */
bool readVmixPreset(const std::string &xml, VmixPresetResult &result, std::string &error);

/* A preset can hold more inputs than the desk's bar. Keeps the first `limit` and
 * moves the rest onto the report (V5); a limit of 0 or less means no bar at all.
 * The desk passes its own limit in, so the rule is the desk's, not the preset's. */
void limitVmixInputs(VmixPresetResult &result, int limit);

} // namespace desk
