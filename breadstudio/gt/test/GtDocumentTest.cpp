#include "GtDocument.hpp"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace gt;

static int failures = 0;

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
		failures++;
	}
}

#define CHECK(cond) check((cond), __LINE__, #cond)

int main()
{
	std::string e;

	/* Defaults when the attributes are absent (design §3.2). */
	{
		GtDocument doc;
		CHECK(parseGtDocument(
			R"(<?xml version="1.0" encoding="utf-16"?><Composition Width="1920" Height="1080"><Layer Name="L" Dimensions="1920,1080,0"><Layer.Composition><Composition Width="1920" Height="1080"><TextBlock Name="T" /></Composition></Layer.Composition></Layer></Composition>)",
			doc, e));
		CHECK(doc.objects.size() == 1 && doc.objects[0].kind == "Layer");
		const GtObject &t = doc.objects[0].children[0];
		CHECK(t.kind == "TextBlock" && t.text == "Text" && t.fontFamily == "Arial" && t.fontSize == 36);
		CHECK(t.strokeThickness == 0 && t.textAlign == "Left" && t.wrapping == "Wrap" && t.autoSize == "Fixed");
		CHECK(t.width == 100 && t.height == 100 && t.opacity == 1 && t.visible);
	}

	/* Attributes and child properties, from Matchup.gtzip's NameL1. */
	{
		GtDocument doc;
		CHECK(parseGtDocument(
			R"(<?xml version="1.0" encoding="utf-16"?><Composition Width="1920" Height="1080"><Layer Name="Match1" Dimensions="743,158,0" Location="138,305,0"><Layer.Composition><Composition Width="743" Height="158"><TextBlock Name="NameL1" Dimensions="248,68,0" Location="80,90,0" Text="player name" FontFamily="Helvetica Neue" FontSize="28" FontWeight="Bold" TextAlign="Center" VerticalAlign="Center" TextEffect="Uppercase"><TextBlock.Fill><Brush Color="#FF000000" /></TextBlock.Fill><TextBlock.Stroke><Brush Color="#FFFFFFFF" /></TextBlock.Stroke></TextBlock></Composition></Layer.Composition></Layer></Composition>)",
			doc, e));
		const GtObject &n = doc.objects[0].children[0];
		CHECK(n.name == "NameL1" && n.text == "player name" && n.height == 68 && n.fontSize == 28);
		CHECK(n.x == 80 && n.y == 90 && n.width == 248 && n.fontWeight == 700 && n.uppercase);
		CHECK(n.textAlign == "Center" && n.verticalAlign == "Center");
		CHECK(n.fill.type == GtBrushType::Solid && n.fill.argb == 0xFF000000 && n.stroke.argb == 0xFFFFFFFF);
	}

	/* The Timer gradient: a linear brush whose first stop has no Position, so it is 0. */
	{
		GtDocument doc;
		CHECK(parseGtDocument(
			R"(<?xml version="1.0" encoding="utf-16"?><Composition Width="1920" Height="1080"><Layer Name="Layer1" Dimensions="1920,1080,0"><Layer.Composition><Composition Width="1920" Height="1080"><Text3D Name="Time" Dimensions="1920,348,20" Location="0,367,0" StrokeThickness="4" Text="{0:hh:mm:ss tt}" FontFamily="Bangers" FontSize="190" FontWeight="Bold" TextAlign="Center" VerticalAlign="Center"><Text3D.Fill><Brush Color="#FFFFFFFF" StartPoint="0.5000001,0" EndPoint="0.5,1" WrapX="Clamp" WrapY="Clamp"><Brush.Stops><GradientStop Color="#FFFF0000" /><GradientStop Position="1" Color="#FFFFFFFF" /></Brush.Stops></Brush></Text3D.Fill><Text3D.Stroke><Brush Color="#FF7F0F0F" /></Text3D.Stroke></Text3D></Composition></Layer.Composition></Layer></Composition>)",
			doc, e));
		const GtObject &g = doc.objects[0].children[0];
		CHECK(g.kind == "Text3D" && g.depth == 20 && g.strokeThickness == 4);
		CHECK(g.fill.type == GtBrushType::Linear && g.fill.stops.size() == 2 &&
		      g.fill.stops[1] == std::make_pair(1.0f, 0xFFFFFFFFu));
		CHECK(g.fill.stops[0].first == 0.0f && g.fill.stops[0].second == 0xFFFF0000u); /* Position absent = 0 */
		CHECK(g.fill.startX == 0.5000001f && g.fill.endX == 0.5f && g.fill.endY == 1.0f);
		CHECK(g.stroke.type == GtBrushType::Solid && g.stroke.argb == 0xFF7F0F0F);
	}

	/* Mask, data flags, opacity and effects. */
	{
		GtDocument doc;
		CHECK(parseGtDocument(
			R"(<?xml version="1.0" encoding="utf-16"?><Composition Width="1920" Height="1080"><Layer Name="L"><Layer.Composition><Composition Width="1920" Height="1080"><Image Name="IMG" Dimensions="200,200,0" DataFlags="Hidden"><Image.Bitmap><Bitmap Source="abc\BG.png" /></Image.Bitmap><Image.Mask><Mask Object="Rectangle1" /></Image.Mask></Image><Rectangle Name="R" Dimensions="100,100,0" Opacity="0" /><TextBlock Name="S"><TextBlock.Effects><Effect Type="Shadow" Offset="3,3" Color="#80000000" BlurAmount="8" Mode="Normal" /></TextBlock.Effects></TextBlock></Composition></Layer.Composition></Layer></Composition>)",
			doc, e));
		const std::vector<GtObject> &kids = doc.objects[0].children;
		CHECK(kids.size() == 3);
		const GtObject &img = kids[0];
		const GtObject &rect = kids[1];
		const GtObject &shadowed = kids[2];
		CHECK(img.mask == "Rectangle1" && img.dataFlags == 1u && img.dataFlagsSet);
		CHECK(img.bitmapSource == "abc\\BG.png");
		CHECK(rect.opacity == 0);
		CHECK(shadowed.effects.size() == 1 && shadowed.effects[0].type == "Shadow");
		CHECK(shadowed.effects[0].offsetX == 3 && shadowed.effects[0].offsetY == 3);
		CHECK(shadowed.effects[0].blur == 8 && shadowed.effects[0].argb == 0x80000000u);
	}

	/* A Storyboard is noted, not drawn; another element is kept and named once. */
	{
		GtDocument doc;
		CHECK(parseGtDocument(
			R"(<?xml version="1.0" encoding="utf-16"?><Composition Width="1920" Height="1080"><Storyboard Name="SB"><Storyboard.Animations /></Storyboard><Ticker Name="Tick1" /><Ticker Name="Tick2" /></Composition>)",
			doc, e));
		CHECK(doc.hasStoryboards);
		CHECK(doc.unknownKinds == std::vector<std::string>{"Ticker"});
		CHECK(doc.objects.size() == 2 && doc.objects[0].kind == "Ticker");
	}

	/* vMix's weight names map to numeric weights; anything else is Regular. */
	CHECK(gtFontWeight("DemiBold") == 600 && gtFontWeight("Black") == 900 && gtFontWeight("Medium") == 500);
	CHECK(gtFontWeight("Bold") == 700 && gtFontWeight("ExtraBold") == 800 && gtFontWeight("Regular") == 400);
	CHECK(gtFontWeight("Nonsense") == 400);

	/* Colours: #AARRGGBB keeps its alpha, #RRGGBB is opaque; anything else is refused. */
	{
		uint32_t c = 0;
		CHECK(gtParseColour("#FF000000", c) && c == 0xFF000000u);
		CHECK(gtParseColour("#00FFFF", c) && c == 0xFF00FFFFu);
		CHECK(!gtParseColour("ff0000", c) && !gtParseColour("#12345", c) && !gtParseColour("", c));
	}

	/* Vietnamese survives (Review Focus 2): a document value comes out byte-correct as UTF-8. */
	{
		GtDocument doc;
		CHECK(parseGtDocument(
			R"(<?xml version="1.0" encoding="utf-16"?><Composition Width="1920" Height="1080"><Layer Name="L"><Layer.Composition><Composition Width="1920" Height="1080"><TextBlock Name="VN" Text="Nguyễn Văn Á" /></Composition></Layer.Composition></Layer></Composition>)",
			doc, e));
		const GtObject &vn = doc.objects[0].children[0];
		CHECK(vn.text == "Nguyễn Văn Á");
	}

	/* Not a document at all: refused with a reason. */
	{
		GtDocument doc;
		std::string bad;
		CHECK(!parseGtDocument("<Composition><Layer></Composition>", doc, bad) && !bad.empty());
	}

	if (failures == 0) {
		std::printf("GtDocument: all checks passed\n");
	}
	return failures == 0 ? 0 : 1;
}
