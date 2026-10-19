#include "GtSettings.hpp"
#include "VmixPreset.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace desk;

/* The fixtures sit beside this file; the build passes their directory in, and a
 * run with no argument falls back to the one CMake sets at configure time. */
#ifndef VMIX_FIXTURE_DIR
#define VMIX_FIXTURE_DIR "test/fixtures"
#endif

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

/* a shot's numbers are computed from doubles, so an exact compare would be brittle */
static bool near(double a, double b)
{
	return std::fabs(a - b) < 1e-9;
}

static std::string readFile(const std::string &path)
{
	std::FILE *file = std::fopen(path.c_str(), "rb");
	if (!file) {
		std::printf("cannot open fixture %s\n", path.c_str());
		return std::string();
	}
	std::fseek(file, 0, SEEK_END);
	const long size = std::ftell(file);
	std::fseek(file, 0, SEEK_SET);
	std::string bytes(size > 0 ? static_cast<std::size_t>(size) : 0, '\0');
	const std::size_t got = bytes.empty() ? 0 : std::fread(&bytes[0], 1, bytes.size(), file);
	std::fclose(file);
	if (got != bytes.size()) {
		std::printf("short read on fixture %s\n", path.c_str());
	}
	return bytes;
}

static std::string fixture(const std::string &dir, const char *name)
{
	return readFile(dir + "/" + name);
}

int main(int argc, char **argv)
{
	const std::string dir = argc > 1 ? argv[1] : VMIX_FIXTURE_DIR;

	/* A cut-down real preset: every input it holds, in number order, with its
	 * kind, title, file or device and audio read straight from the file. The two
	 * kinds with no desk equivalent are on the report, not silently dropped. */
	{
		const std::string bytes = fixture(dir, "cut-cacas24.vmix");
		CHECK(!bytes.empty());
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset(bytes, result, error));
		CHECK(error.empty());
		CHECK(result.inputs.size() == 6);
		CHECK(result.active == 3);
		CHECK(result.preview == 2);
		if (result.inputs.size() == 6) {
			const VmixInput &image = result.inputs[0];
			CHECK(image.number == 1);
			CHECK(image.kind == VmixKind::Image);
			CHECK(image.type == 1);
			CHECK(image.title.empty());
			CHECK(image.originalTitle == "logo.png");
			CHECK(image.file == "logo.png");
			CHECK(image.volume == 1.0);
			CHECK(image.muted);
			CHECK(image.hasGeometry);
			CHECK(image.matrixCount == 1);
			CHECK(!image.matrixMoved);

			const VmixInput &list = result.inputs[1];
			CHECK(list.number == 2);
			CHECK(list.kind == VmixKind::Skipped);
			CHECK(list.type == 14);
			CHECK(list.title == "Videos");
			CHECK(list.originalTitle == "List");
			CHECK(list.volume == 0.1296);

			const VmixInput &colour = result.inputs[2];
			CHECK(colour.number == 3);
			CHECK(colour.kind == VmixKind::Colour);
			CHECK(colour.title == "Main");
			CHECK(colour.colour == "0");
			/* a real picture-in-picture, but on overlay layer 1: the list is index 0
			 * (the input's own Main/Program placement) followed by one MatrixPosition
			 * per overlay layer, so the matrix carried is Main's — the identity — and
			 * the preset holds two placements. The PiP is reported, not taken (the
			 * desk builds no layers). */
			CHECK(colour.hasGeometry);
			CHECK(colour.matrixCount == 2);
			CHECK(!colour.matrixMoved);
			CHECK(colour.zoomX == 1.0);
			CHECK(colour.zoomY == 1.0);
			CHECK(colour.panX == 0.0);
			CHECK(colour.panY == 0.0);
			CHECK(colour.rotateX == 0.0 && colour.rotateY == 0.0 && colour.rotateZ == 0.0);
			/* vMix's serialized fields the desk reports, at their defaults here */
			CHECK(colour.postZoomX == 1.0 && colour.postZoomY == 1.0);
			CHECK(colour.sourceRotation == 0);
			CHECK(!colour.hidden);
			CHECK(colour.cropLeft == 0.0 && colour.cropRight == 0.0);

			const VmixInput &title = result.inputs[3];
			CHECK(title.number == 4);
			/* clock.gtzip is Type 9000: a GT title, not a plain .xaml Title */
			CHECK(title.kind == VmixKind::GtTitle);
			CHECK(title.originalTitle == "clock.gtzip");

			const VmixInput &video = result.inputs[4];
			CHECK(video.number == 5);
			CHECK(video.kind == VmixKind::Video);
			CHECK(video.file == "background.mp4");

			const VmixInput &frames = result.inputs[5];
			CHECK(frames.number == 6);
			CHECK(frames.kind == VmixKind::Skipped);
			CHECK(frames.type == 21);
			CHECK(frames.title == "Transitions");
		}

		/* an empty report would be wrong: two inputs have no desk equivalent */
		CHECK(result.report.size() == 2);
		if (result.report.size() == 2) {
			CHECK(result.report[0].input == "Videos");
			CHECK(result.report[0].had == "List");
			CHECK(result.report[0].why == "no desk equivalent");
			CHECK(result.report[1].input == "Transitions");
			CHECK(result.report[1].had == "Image Sequence");
			CHECK(result.report[1].why == "no desk equivalent");
		}
	}

	/* Unknown types (15, 22, 100) make no input and get a report row each, named
	 * from OriginalTitle — never guessed (V7). The Colour maps, and Video Call is
	 * a kind the desk can name but has no equivalent for: both stay, reported. */
	{
		const std::string bytes = fixture(dir, "written-unknowns.vmix");
		CHECK(!bytes.empty());
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset(bytes, result, error));

		/* two entries: the Colour, and the Video Call with no desk equivalent */
		CHECK(result.inputs.size() == 2);
		if (result.inputs.size() == 2) {
			CHECK(result.inputs[0].kind == VmixKind::Colour);
			CHECK(result.inputs[0].title == "Lower Third Colour");
			CHECK(result.inputs[0].colour == "-16777216");
			CHECK(result.inputs[0].volume == 0.8);
			CHECK(!result.inputs[0].muted);
			/* this one holds no matrix at all: no geometry, and none claimed */
			CHECK(!result.inputs[0].hasGeometry);
			CHECK(result.inputs[0].matrixCount == 0);
			CHECK(!result.inputs[0].matrixMoved);

			CHECK(result.inputs[1].kind == VmixKind::Skipped);
			CHECK(result.inputs[1].type == 6000);
			CHECK(result.inputs[1].title == "Game Call");

			/* not one entry came from a Type the spec cannot name */
			for (const VmixInput &input : result.inputs) {
				CHECK(input.kind != VmixKind::Unknown);
			}
		}

		CHECK(result.report.size() == 4);
		if (result.report.size() == 4) {
			CHECK(result.report[0].input == "Blank");
			CHECK(result.report[0].had == "vMix input type 15");
			CHECK(result.report[1].input == "Camera 1");
			CHECK(result.report[1].had == "vMix input type 22");
			CHECK(result.report[2].input == "Blank");
			CHECK(result.report[2].had == "vMix input type 100");
			CHECK(result.report[3].input == "Game Call");
			CHECK(result.report[3].had == "Video Call");
			CHECK(result.report[3].why == "no desk equivalent");
		}
	}

	/* Bytes that are not XML at all, and XML that is not a preset, are refused. */
	{
		VmixPresetResult result;
		std::string error;
		CHECK(!readVmixPreset("this is not xml at all", result, error));
		CHECK(!error.empty());
		CHECK(result.inputs.empty());

		error.clear();
		CHECK(!readVmixPreset("<settings><value>1</value></settings>", result, error));
		CHECK(!error.empty());
		CHECK(result.inputs.empty());
	}

	/* A Type that is not a number is not Type 0 (Video) by accident: it lands on
	 * the report as an unnamed type, never as a video (V7). */
	{
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset("<XML><Input Type=\"abc\" OriginalTitle=\"Mystery\" /></XML>", result, error));
		CHECK(result.inputs.empty());
		CHECK(result.report.size() == 1);
		if (result.report.size() == 1) {
			CHECK(result.report[0].input == "Mystery");
			CHECK(result.report[0].had == "vMix input type -1");
		}
	}

	/* An empty preset succeeds: no inputs and nothing to report, but not a failure. */
	{
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset("<XML><Version>9</Version></XML>", result, error));
		CHECK(error.empty());
		CHECK(result.inputs.empty());
		CHECK(result.report.empty());
	}

	/* Everything mapped: an empty report says the whole preset came across (§5.3).
	 * More inputs than the bar holds: the first N stay, the rest are reported (V5). */
	{
		const std::string bytes = fixture(dir, "written-many.vmix");
		CHECK(!bytes.empty());
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset(bytes, result, error));
		CHECK(result.inputs.size() == 4);
		CHECK(result.report.empty());

		/* N is the desk's own limit, passed in; here the bar is two. */
		limitVmixInputs(result, 2);
		CHECK(result.inputs.size() == 2);
		CHECK(result.report.size() == 2);
		if (result.inputs.size() == 2 && result.report.size() == 2) {
			CHECK(result.inputs[0].title == "One");
			CHECK(result.inputs[1].title == "Two");
			CHECK(result.report[0].input == "Three");
			CHECK(result.report[0].had == "input 3");
			CHECK(result.report[1].input == "Four");
			CHECK(result.report[1].had == "input 4");
		}

		/* a bar that holds them all, or no bar at all, changes nothing */
		VmixPresetResult roomy;
		CHECK(readVmixPreset(bytes, roomy, error));
		limitVmixInputs(roomy, 4);
		CHECK(roomy.inputs.size() == 4);
		CHECK(roomy.report.empty());

		VmixPresetResult unlimited;
		CHECK(readVmixPreset(bytes, unlimited, error));
		limitVmixInputs(unlimited, 0);
		CHECK(unlimited.inputs.size() == 4);
		CHECK(unlimited.report.empty());
	}

	/* A preset that both drops a kind and holds more inputs than the bar: the trim
	 * appends the over-limit rows after the rows already on the report (V5). */
	{
		const std::string bytes = fixture(dir, "cut-cacas24.vmix");
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset(bytes, result, error));
		CHECK(result.report.size() == 2); /* the List and the Image Sequence */

		limitVmixInputs(result, 4);
		CHECK(result.inputs.size() == 4);
		CHECK(result.report.size() == 4);
		if (result.report.size() == 4) {
			/* the two from the read are untouched, then the two the trim added */
			CHECK(result.report[0].input == "Videos");
			CHECK(result.report[1].input == "Transitions");
			CHECK(result.report[2].input == "background.mp4");
			CHECK(result.report[2].had == "input 5");
			CHECK(result.report[2].why == "the desk's bar holds 4");
			CHECK(result.report[3].input == "Transitions");
			CHECK(result.report[3].had == "input 6");
		}
	}

	/* Which kind needs which OBS base, and which kind makes no input at all
	 * (§5.2/§5.4): the pure decision the build half turns into a source. */
	{
		CHECK(std::string(vmixBaseId(VmixKind::Colour)) == "color_source");
		CHECK(std::string(vmixBaseId(VmixKind::Image)) == "image_source");
		CHECK(std::string(vmixBaseId(VmixKind::Video)) == "ffmpeg_source");
		CHECK(std::string(vmixBaseId(VmixKind::AudioFile)) == "ffmpeg_source");
		CHECK(std::string(vmixBaseId(VmixKind::Media)) == "ffmpeg_source");
		CHECK(std::string(vmixBaseId(VmixKind::Capture)) == "dshow_input");
		CHECK(std::string(vmixBaseId(VmixKind::AudioIn)) == "wasapi_input_capture");
		CHECK(std::string(vmixBaseId(VmixKind::NDI)) == "ndi_source");
		CHECK(std::string(vmixBaseId(VmixKind::Browser)) == "browser_source");
		CHECK(std::string(vmixBaseId(VmixKind::DesktopCapture)) == "monitor_capture");
		CHECK(std::string(vmixBaseId(VmixKind::Title)) == "text_ft2_source");
		/* a Type 9000 GT title is a real GT title input, not a plain text source */
		CHECK(std::string(vmixBaseId(VmixKind::GtTitle)) == gt::SourceId);
		/* the two that make no input: a kind with no desk equivalent, and one the
		 * spec cannot name (which the read does not even carry) */
		CHECK(vmixBaseId(VmixKind::Skipped) == nullptr);
		CHECK(vmixBaseId(VmixKind::Unknown) == nullptr);
	}

	/* The pure build plan over a real cut-down preset: the inputs to make, in
	 * number order, with the two kinds that have no equivalent left out but their
	 * rows kept (V4/V5). */
	{
		const std::string bytes = fixture(dir, "cut-cacas24.vmix");
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset(bytes, result, error));
		CHECK(result.report.size() == 2); /* the List and the Image Sequence */

		const std::vector<VmixBuildStep> steps = planVmixBuild(result, 0);
		CHECK(steps.size() == 4); /* the image, colour, title and video; not the two skipped */
		if (steps.size() == 4) {
			CHECK(steps[0].input->kind == VmixKind::Image);
			CHECK(steps[1].input->kind == VmixKind::Colour);
			CHECK(steps[2].input->kind == VmixKind::GtTitle);
			CHECK(steps[3].input->kind == VmixKind::Video);
			CHECK(std::string(steps[0].baseId) == "image_source");
			CHECK(std::string(steps[1].baseId) == "color_source");
			/* the Type 9000 clock base is the GT title source, not text_ft2_source */
			CHECK(std::string(steps[2].baseId) == gt::SourceId);
			CHECK(std::string(steps[3].baseId) == "ffmpeg_source");
		}
		/* no desk base for the two skipped kinds, so no step named one */
		for (const VmixBuildStep &step : steps) {
			CHECK(step.input->kind != VmixKind::Skipped);
			CHECK(step.input->kind != VmixKind::Unknown);
		}
		/* the read's rows are untouched; this preset has no overlays or transitions */
		CHECK(result.report.size() == 2);
	}

	/* The over-limit split driven end to end through the build's pure part (V5):
	 * the bar holds two, the rest are reported, and the steps are what is left. */
	{
		const std::string bytes = fixture(dir, "written-many.vmix");
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset(bytes, result, error));
		CHECK(result.report.empty());

		const std::vector<VmixBuildStep> steps = planVmixBuild(result, 2);
		CHECK(steps.size() == 2);
		CHECK(result.inputs.size() == 2);
		CHECK(result.report.size() == 2);
		if (steps.size() == 2) {
			CHECK(steps[0].input->title == "One");
			CHECK(steps[1].input->title == "Two");
		}
		if (result.report.size() == 2) {
			CHECK(result.report[0].input == "Three");
			CHECK(result.report[0].why == "the desk's bar holds 2");
			CHECK(result.report[1].input == "Four");
		}
	}

	/* The matrix turned into a shot (V4). The source is **square** against a 16:9
	 * frame, so contain (fit the whole source, 1.08) and cover (fill and crop, 1.92)
	 * give different numbers: the case can fail. */
	{
		VmixInput shot;
		shot.kind = VmixKind::Video;
		shot.zoomX = shot.zoomY = 1;
		shot.panX = shot.panY = 0;

		VmixPlacement place;
		CHECK(vmixPlacement(shot, 1920, 1080, 1000, 1000, place));
		/* fit = min(1920/1000, 1080/1000) = 1.08, centred at 1080x1080 */
		CHECK(near(place.scaleX, 1.08));
		CHECK(near(place.scaleY, 1.08));
		CHECK(near(place.x, (1920 - 1080) / 2.0)); /* 420 */
		CHECK(near(place.y, 0.0));
		CHECK(place.rot == 0.0);

		/* zoom 0.5: half of the fitted size, still centred */
		shot.zoomX = shot.zoomY = 0.5;
		CHECK(vmixPlacement(shot, 1920, 1080, 1000, 1000, place));
		CHECK(near(place.scaleX, 0.54));
		CHECK(near(place.x, (1920 - 540) / 2.0)); /* 690 */
		CHECK(near(place.y, (1080 - 540) / 2.0)); /* 270 */

		/* a stored Zoom of 0 is vMix's unset: read as 1, or the source would vanish */
		shot.zoomX = shot.zoomY = 0;
		CHECK(vmixPlacement(shot, 1920, 1080, 1000, 1000, place));
		CHECK(near(place.scaleX, 1.08));

		/* pan is a fraction of the frame, on top of the centred shot */
		shot.zoomX = shot.zoomY = 1;
		shot.panX = 0.25;
		shot.panY = -0.25;
		CHECK(vmixPlacement(shot, 1920, 1080, 1000, 1000, place));
		CHECK(near(place.x, (1920 - 1080) / 2.0 + 0.25 * 1920));
		CHECK(near(place.y, 0.0 - 0.25 * 1080));

		/* rotation is carried through as degrees */
		shot.panX = shot.panY = 0;
		shot.rotateZ = 30;
		CHECK(vmixPlacement(shot, 1920, 1080, 1000, 1000, place));
		CHECK(place.rot == 30.0);

		/* a shot that cannot be computed is refused, not guessed: no frame, or a
		 * source with no pixel size yet */
		CHECK(!vmixPlacement(shot, 0, 0, 1000, 1000, place));
		CHECK(!vmixPlacement(shot, 1920, 1080, 0, 0, place));
	}

	/* The output frame, the overlays and the transitions the reader carries: a
	 * preset that names none leaves them at rest, one that names them sets them. */
	{
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset("<XML><Version>9</Version>"
				     "<OutputFormat OutputSize=\"1920x1080\" />"
				     "<Input Type=\"12\" Title=\"C\" OriginalTitle=\"Colour\" Colour=\"0\" />"
				     "<Overlay1>1</Overlay1></XML>",
				     result, error));
		CHECK(result.outputWidth == 1920);
		CHECK(result.outputHeight == 1080);
		CHECK(result.overlays);
		CHECK(!result.transitions);

		/* a preset that names no frame: nothing to compute a shot against */
		VmixPresetResult bare;
		CHECK(readVmixPreset("<XML><Version>9</Version></XML>", bare, error));
		CHECK(bare.outputWidth == 0 && bare.outputHeight == 0);
		CHECK(!bare.overlays && !bare.transitions);

		/* a transition lies under a Transitions directory, vMix's own shape */
		VmixPresetResult trans;
		CHECK(readVmixPreset("<XML><Version>9</Version>"
				     "<Input Type=\"0\" Title=\"T\" OriginalTitle=\"a.mp4\" />"
				     "<AdditionalFiles>C:\\Show\\Transitions\\Trans00001.png</AdditionalFiles></XML>",
				     trans, error));
		CHECK(trans.transitions);

		/* the cut-down real preset names neither, so a clean import stays clean */
		const std::string bytes = fixture(dir, "cut-cacas24.vmix");
		VmixPresetResult real;
		CHECK(readVmixPreset(bytes, real, error));
		CHECK(!real.overlays && !real.transitions);
	}

	/* Program's matrix carries more than pan and zoom (V4): the crop Clipping
	 * names, the hidden flag, post-zoom and the source rotation are all serialized
	 * (no [XmlIgnore]) and read, so the build half applies crop and hidden and
	 * reports the two it cannot place. */
	{
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset(
			"<XML><Version>9</Version>"
			"<Input Type=\"5\" Title=\"Cam\" OriginalTitle=\"Camera 1\" Positions=\""
			"&lt;ArrayOfMatrixPosition&gt;&lt;MatrixPosition&gt;"
			"&lt;Hidden&gt;true&lt;/Hidden&gt;"
			"&lt;ZoomX&gt;0.5&lt;/ZoomX&gt;&lt;ZoomY&gt;0.5&lt;/ZoomY&gt;"
			"&lt;PanX&gt;0.1&lt;/PanX&gt;&lt;PanY&gt;0.2&lt;/PanY&gt;"
			"&lt;PostZoomX&gt;2&lt;/PostZoomX&gt;&lt;PostZoomY&gt;2&lt;/PostZoomY&gt;"
			"&lt;SourceRotation&gt;R90&lt;/SourceRotation&gt;"
			"&lt;Rotate&gt;&lt;X&gt;0&lt;/X&gt;&lt;Y&gt;0&lt;/Y&gt;&lt;Z&gt;0&lt;/Z&gt;&lt;/Rotate&gt;"
			"&lt;Clipping&gt;&lt;float&gt;0.1&lt;/float&gt;&lt;float&gt;0.2&lt;/float&gt;"
			"&lt;float&gt;0.9&lt;/float&gt;&lt;float&gt;0.8&lt;/float&gt;&lt;/Clipping&gt;"
			"&lt;/MatrixPosition&gt;&lt;/ArrayOfMatrixPosition&gt;\" /></XML>",
			result, error));
		CHECK(result.inputs.size() == 1);
		if (result.inputs.size() == 1) {
			const VmixInput &cam = result.inputs[0];
			CHECK(cam.kind == VmixKind::Capture);
			CHECK(cam.matrixCount == 1);
			CHECK(cam.matrixMoved);
			CHECK(near(cam.zoomX, 0.5));
			CHECK(near(cam.panX, 0.1));
			CHECK(near(cam.postZoomX, 2.0));
			CHECK(near(cam.postZoomY, 2.0));
			CHECK(cam.sourceRotation == 90);
			CHECK(cam.hidden);
			/* Clipping is boundary positions, so crop is what is cut off each edge */
			CHECK(near(cam.cropLeft, 0.1));
			CHECK(near(cam.cropTop, 0.2));
			CHECK(near(cam.cropRight, 1.0 - 0.9));
			CHECK(near(cam.cropBottom, 1.0 - 0.8));
		}
	}

	/* The report as text (§5.3): one line per row, the three fields tab-separated —
	 * the body the report window shows and copies to the clipboard. Every row is on
	 * its own line, in order, so nothing is dropped or summarised away (V4). */
	{
		const std::string bytes = fixture(dir, "cut-cacas24.vmix");
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset(bytes, result, error));
		CHECK(result.report.size() == 2);
		/* the two rows this preset carries, verbatim: two lines, nothing merged */
		CHECK(vmixReportText(result.report) ==
		      "Videos\tList\tno desk equivalent\nTransitions\tImage Sequence\tno desk equivalent");

		/* every row the reader raised is here, the unknown-type ones included: all
		 * four, in order, named as the preset named them (V4/V7) */
		const std::string unknownBytes = fixture(dir, "written-unknowns.vmix");
		VmixPresetResult unknowns;
		CHECK(readVmixPreset(unknownBytes, unknowns, error));
		CHECK(unknowns.report.size() == 4);
		CHECK(vmixReportText(unknowns.report) ==
		      "Blank\tvMix input type 15\tthe desk cannot name this input type\n"
		      "Camera 1\tvMix input type 22\tthe desk cannot name this input type\n"
		      "Blank\tvMix input type 100\tthe desk cannot name this input type\n"
		      "Game Call\tVideo Call\tno desk equivalent");

		/* everything mapped: the text is empty — "the whole preset came across" (§5.3) */
		const std::string clean = fixture(dir, "written-many.vmix");
		VmixPresetResult mapped;
		CHECK(readVmixPreset(clean, mapped, error));
		CHECK(mapped.report.empty());
		CHECK(vmixReportText(mapped.report).empty());

		/* a "not applied" row the build raises is carried into the text, not left
		 * behind by the display (V4) */
		VmixPresetResult overlays;
		CHECK(readVmixPreset("<XML><Version>9</Version>"
				     "<Input Type=\"12\" Title=\"C\" OriginalTitle=\"Colour\" Colour=\"0\" />"
				     "<Overlay1>1</Overlay1></XML>",
				     overlays, error));
		planVmixBuild(overlays, 0);
		CHECK(vmixReportText(overlays.report) ==
		      "\tvMix overlays 1-4\tnot applied: the desk has no overlay bus; the inputs are imported, the routing "
		      "is not carried");
	}

	/* A GT title input (Type 9000) reads as a GT title, not a plain text source
	 * (§6): the .gtzip's path from the element's text, its values and styles from
	 * the `XML` attribute, every countdown from `CountdownXML`, and the data links
	 * counted from `DataSourcesXML`. Three inputs, in the shape of the owner's
	 * EAFC24 preset: two share one .gtzip and carry different values. */
	{
		const std::string bytes = fixture(dir, "gt-titles.vmix");
		CHECK(!bytes.empty());
		VmixPresetResult result;
		std::string error;
		CHECK(readVmixPreset(bytes, result, error));
		CHECK(error.empty());
		CHECK(result.inputs.size() == 3);
		if (result.inputs.size() == 3) {
			const VmixInput &left = result.inputs[0];
			CHECK(left.kind == VmixKind::GtTitle);
			CHECK(left.type == 9000);
			CHECK(left.titleFile == "C:\\EAFC24\\GT\\TagWin\\TagWin.gtzip");
			CHECK(left.titleItems.size() == 1);
			if (left.titleItems.size() == 1) {
				CHECK(left.titleItems[0].name == "PlayerName.Text");
				CHECK(left.titleItems[0].value == "Player Left");
				CHECK(!left.titleItems[0].hasStyle); /* no <Font>: the file's own style */
			}
			CHECK(left.dataLinks == 1);
			CHECK(!left.dataSourcesXml.empty());
			CHECK(left.countdowns.size() == 1);
			if (left.countdowns.size() == 1) {
				CHECK(left.countdowns[0].start == "00:00:00");
				CHECK(left.countdowns[0].duration == "00:00:00");
				CHECK(left.countdowns[0].format == "HH:mm:ss");
				CHECK(!left.countdowns[0].reverse);
				CHECK(!left.countdowns[0].reverseDisplay);
			}

			/* the same .gtzip as a second input, with its own value (§6) */
			const VmixInput &right = result.inputs[1];
			CHECK(right.kind == VmixKind::GtTitle);
			CHECK(right.titleFile == left.titleFile);
			CHECK(right.titleItems.size() == 1);
			if (right.titleItems.size() == 1) {
				CHECK(right.titleItems[0].name == "PlayerName.Text");
				CHECK(right.titleItems[0].value == "Player Right");
			}
			CHECK(right.dataLinks == 1);

			const VmixInput &time = result.inputs[2];
			CHECK(time.kind == VmixKind::GtTitle);
			CHECK(time.titleItems.size() == 1);
			if (time.titleItems.size() == 1) {
				CHECK(time.titleItems[0].name == "Time.Text");
				CHECK(time.titleItems[0].hasStyle);
				CHECK(time.titleItems[0].font == "Helvetica Neue");
				CHECK(time.titleItems[0].size == 192);
				CHECK(time.titleItems[0].fill == "#FF00FFFF");
				CHECK(time.titleItems[0].style == "Italic");
				CHECK(time.titleItems[0].weight == "Bold");
				CHECK(time.titleItems[0].stretch == "Normal");
				CHECK(time.titleItems[0].lineSpacing == 0);
			}
			CHECK(time.countdowns.size() == 1);
			if (time.countdowns.size() == 1) {
				CHECK(time.countdowns[0].duration == "00:20:00");
				CHECK(time.countdowns[0].format == "mm:ss");
			}
			CHECK(time.dataLinks == 1);
		}
		/* a GT title is a kind the desk can back: no report row for it */
		CHECK(result.report.empty());
	}

	/* The three small XML documents the GT input carries, read on their own (§6):
	 * a <dataSource> with no mapper is not a link, a value can be any UTF-8 text
	 * (Vietnamese must survive — Review Focus 2), and the two path helpers. */
	{
		CHECK(countVmixDataLinks("<dataSources><dataSource name=\"X\" /></dataSources>") == 0);
		CHECK(countVmixDataLinks(
			      "<dataSources><dataSource name=\"X\"><mapper column=\"A\" /></dataSource></dataSources>") == 1);
		CHECK(countVmixDataLinks("") == 0);

		/* the value is UTF-8, byte for byte, through the preset's own XML */
		const std::vector<VmixTitleItem> items = readVmixTitleItems(
			"<items><item name=\"N.Text\" version=\"2\"><value>Nguyễn Văn Á</value></item></items>");
		CHECK(items.size() == 1);
		if (items.size() == 1) {
			CHECK(items[0].name == "N.Text");
			CHECK(items[0].value == "Nguyễn Văn Á");
			CHECK(!items[0].hasStyle);
		}
		CHECK(readVmixTitleItems("<items />").empty());
		CHECK(readVmixTitleItems("not xml").empty());

		/* every countdown, in order; an empty or malformed document reads none */
		CHECK(readVmixCountdowns("").empty());
		CHECK(readVmixCountdowns("not xml").empty());

		/* relocation with a fake filesystem (§6): the stored path if it exists,
		 * else the path's tail under the preset's folder, dropped one folder at a
		 * time; %XX is decoded and / turned into \ first. Nothing found is "" */
		auto fs = [](const std::string &p) {
			return p == "F:\\Stream\\EAFC24\\GT\\TagWin\\TagWin.gtzip" ||
			       p == "F:\\Stream\\FVPL SUMMER 2024\\LOGO\\T4M.PNG";
		};
		CHECK(findVmixFile("C:\\EAFC24\\GT\\TagWin\\TagWin.gtzip", "F:\\Stream\\EAFC24", fs) ==
		      "F:\\Stream\\EAFC24\\GT\\TagWin\\TagWin.gtzip");
		CHECK(findVmixFile("file:///C:/FVPL%20SUMMER%202024/LOGO/T4M.PNG", "F:\\Stream\\FVPL SUMMER 2024", fs) ==
		      "F:\\Stream\\FVPL SUMMER 2024\\LOGO\\T4M.PNG");
		CHECK(findVmixFile("C:\\nowhere\\x.gtzip", "F:\\Stream\\EAFC24", fs) == "");

		/* a value is absolute when it is a drive path or a file:/// URL */
		CHECK(vmixAbsolutePath("file:///C:/a.png"));
		CHECK(vmixAbsolutePath("C:\\a.png"));
		CHECK(!vmixAbsolutePath("422d1b65\\BG.png"));
		CHECK(!vmixAbsolutePath(""));
	}

	if (failures == 0) {
		std::printf("VmixPreset: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
