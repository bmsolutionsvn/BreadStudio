#include "GtDocument.hpp"
#include "GtFields.hpp"
#include "GtPackage.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace gt;

/* The owner's titles live outside the repo (§9). The folder is argv[1]; this is the default. */
#ifndef GT_LOCAL_DIR
#define GT_LOCAL_DIR "F:/Stream"
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

static GtDocument parse(const std::string &xml)
{
	GtDocument doc;
	std::string error;
	if (!parseGtDocument(xml, doc, error)) {
		std::printf("FAIL: could not parse a test document: %s\n", error.c_str());
		failures++;
	}
	return doc;
}

int main(int argc, char **argv)
{
	/* Only objects directly in a top-level Layer are fields (§4.3). */
	{
		GtDocument doc = parse(
			R"(<Composition Width="1920" Height="1080"><Layer Name="L" Dimensions="1920,1080,0"><Layer.Composition><Composition Width="1920" Height="1080"><TextBlock Name="T" Text="hello" /></Composition></Layer.Composition></Layer></Composition>)");
		const std::vector<GtField> fields = gtFields(doc);
		CHECK(fields.size() == 1);
		if (fields.size() == 1) {
			CHECK(fields[0].name == "T.Text" && fields[0].type == GtFieldType::Text);
			CHECK(fields[0].defaultValue == "hello" && fields[0].object == &doc.objects[0].children[0]);
		}
	}

	/* The kinds, their flags and their document order. */
	{
		GtDocument doc = parse(
			R"(<Composition Width="1920" Height="1080"><Layer Name="L" Dimensions="1920,1080,0"><Layer.Composition><Composition Width="1920" Height="1080"><Image Name="HiddenImg" DataFlags="Hidden" /><Image Name="I" Dimensions="200,200,0"><Image.Bitmap><Bitmap Source="abc\BG.png" /></Image.Bitmap></Image><Rectangle Name="Plain" /><Rectangle Name="R" DataFlags="None"><Rectangle.Fill><Brush Color="#FF00FFFF" /></Rectangle.Fill></Rectangle><Ellipse Name="E" DataFlags="None"><Ellipse.Fill><Brush Color="#8000FF00" /></Ellipse.Fill></Ellipse><Text3D Name="Time" Text="{0:HH:mm:ss}" /></Composition></Layer.Composition></Layer></Composition>)");
		const std::vector<GtField> fields = gtFields(doc);
		CHECK(fields.size() == 4);
		if (fields.size() == 4) {
			CHECK(fields[0].name == "I.Source" && fields[0].type == GtFieldType::Image &&
			      fields[0].defaultValue == "abc\\BG.png");
			CHECK(fields[1].name == "R.Fill.Color" && fields[1].type == GtFieldType::Colour &&
			      fields[1].defaultValue == "#FF00FFFF");
			CHECK(fields[2].name == "E.Fill.Color" && fields[2].type == GtFieldType::Colour &&
			      fields[2].defaultValue == "#8000FF00");
			CHECK(fields[3].name == "Time.Text" && fields[3].type == GtFieldType::Text &&
			      fields[3].defaultValue == "{0:HH:mm:ss}");
		}
	}

	/* Deeper than a top-level Layer, and outside a Layer: neither is a field. */
	{
		GtDocument doc = parse(
			R"(<Composition Width="1920" Height="1080"><Layer Name="Outer" Dimensions="1920,1080,0"><Layer.Composition><Composition Width="1920" Height="1080"><Layer Name="Inner"><Layer.Composition><Composition Width="1920" Height="1080"><TextBlock Name="Deep" /></Composition></Layer.Composition></Layer></Composition></Layer.Composition></Layer><TextBlock Name="Root" /></Composition>)");
		CHECK(gtFields(doc).empty());
	}

	/* A Hidden TextBlock is not a field either. */
	{
		GtDocument doc = parse(
			R"(<Composition Width="1920" Height="1080"><Layer Name="L"><Layer.Composition><Composition Width="1920" Height="1080"><TextBlock Name="H" DataFlags="Hidden" /></Composition></Layer.Composition></Layer></Composition>)");
		CHECK(gtFields(doc).empty());
	}

	/* Local counts over the owner's titles (design §1.2, §3.4, §9): 158 text, 55 picture, 0 colour. */
	const std::string folder = argc > 1 ? argv[1] : GT_LOCAL_DIR;
	std::error_code ec;
	if (!std::filesystem::is_directory(folder, ec)) {
		std::printf("GtFields: SKIP local counts (%s not found)\n", folder.c_str());
	} else {
		int files = 0, text = 0, image = 0, colour = 0;
		for (const std::filesystem::directory_entry &entry :
		     std::filesystem::recursive_directory_iterator(folder, ec)) {
			if (!entry.is_regular_file() || entry.path().extension() != ".gtzip") {
				continue;
			}
			files++;
			GtPackage package;
			std::string error;
			CHECK(readGtFile(entry.path().u8string(), package, error));
			GtDocument doc;
			std::string parseError;
			CHECK(parseGtDocument(package.documentXml, doc, parseError));
			for (const GtField &field : gtFields(doc)) {
				if (field.type == GtFieldType::Text) {
					text++;
				} else if (field.type == GtFieldType::Image) {
					image++;
				} else {
					colour++;
				}
			}
		}
		std::printf("GtFields: %d files, %d text, %d image, %d colour\n", files, text, image, colour);
		CHECK(files == 25);
		CHECK(text == 158 && image == 55 && colour == 0);
	}

	if (failures == 0) {
		std::printf("GtFields: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
