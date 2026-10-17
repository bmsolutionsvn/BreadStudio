#include "VmixPreset.hpp"

#include <cstdio>
#include <string>

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
			/* a real picture-in-picture: output 1 full frame, output 2 placing it
			 * small and low-left. The reader carries that placement raw, for Task 2
			 * to convert once the source's pixel size is known; matrixCount says the
			 * preset holds more than one output, so no single matrix is the desk's. */
			CHECK(colour.hasGeometry);
			CHECK(colour.matrixCount == 2);
			CHECK(colour.matrixMoved);
			CHECK(colour.zoomX == 0.83525);
			CHECK(colour.zoomY == 0.83525);
			CHECK(colour.panX == 0.214);
			CHECK(colour.panY == 0.189);
			CHECK(colour.rotateX == 0.0 && colour.rotateY == 0.0 && colour.rotateZ == 0.0);

			const VmixInput &title = result.inputs[3];
			CHECK(title.number == 4);
			CHECK(title.kind == VmixKind::Title);
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

	if (failures == 0) {
		std::printf("VmixPreset: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
