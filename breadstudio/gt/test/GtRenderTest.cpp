/* The local GT render test (plan Task 6, design §9).

   It draws each .gtzip under <gtzip folder> with its fields' defaults through
   GtRenderer, writes the result next to a vMix reference snapshot, and reports
   how far apart the two are. The owner's titles and the vMix references are
   not committed, so an absent folder is a SKIP, not a failure.

   It also draws a fixed document and the line "Nguyễn Văn Á" itself, so the
   renderer and DirectWrite's shaping are checked with no vMix at hand
   (Review Focus 2).

   Usage: GtRenderTest <gtzip folder> <reference folder> <out folder> */

#include "GtClock.hpp"
#include "GtDocument.hpp"
#include "GtFields.hpp"
#include "GtPackage.hpp"
#include "GtRenderer.hpp"

#include <windows.h>

#include <d3d11.h>
#include <dwrite.h>
#include <dxgi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
using namespace gt;

/* A file fails when the mean channel difference exceeds this, or this share of
 * pixels has a channel off by more than 32 (plan Task 6 Step 3). */
static const double MeanLimit = 2.0;
static const double BadLimit = 0.5; /* percent */
static const int BadChannel = 32;

/* vMix's snapshot shows its live clock; the test draws the fixed 21:05:09, so these are known. */
static const char *const LiveClockReason = "vMix's snapshot shows its live clock; the test draws a fixed 21:05:09";
struct KnownDifference {
	const char *path;
	const char *reason;
};
static const KnownDifference KnownDifferences[] = {
	{"CACAS24/GTs/MHC/Timer", LiveClockReason},
	{"CACAS24/GTs/MHC/Timer2", LiveClockReason},
	{"FVPL SUMMER 2024/COUNT DOWN MHC/COUNTDOWN", LiveClockReason},
};

static const char *knownReason(const std::string &name)
{
	const std::string stem = fs::path(name).replace_extension().generic_string();
	for (const KnownDifference &known : KnownDifferences) {
		if (stem == known.path) {
			return known.reason;
		}
	}
	return nullptr;
}

static void check(bool ok, int line, const char *what)
{
	if (!ok) {
		std::printf("FAIL line %d: %s\n", line, what);
	}
}

static int failures = 0;
#define REQUIRE(cond)                        \
	do {                                 \
		if (!(cond)) {               \
			check(false, __LINE__, #cond); \
			failures++;          \
		}                            \
	} while (0)

static std::wstring toWide(const std::string &utf8)
{
	if (utf8.empty()) {
		return std::wstring();
	}
	const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
	std::wstring out((size_t)n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), out.data(), n);
	return out;
}

static std::string toUtf8(const std::wstring &wide)
{
	if (wide.empty()) {
		return std::string();
	}
	const int n = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
	std::string out((size_t)n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), out.data(), n, nullptr, nullptr);
	return out;
}

static bool createDevice(ComPtr<ID3D11Device> &device, ComPtr<ID3D11DeviceContext> &context)
{
	const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
	const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	for (const D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
		D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;
		const HRESULT hr = D3D11CreateDevice(nullptr, type, nullptr, flags, levels,
						     (UINT)(sizeof(levels) / sizeof(levels[0])), D3D11_SDK_VERSION,
						     &device, &got, &context);
		if (SUCCEEDED(hr)) {
			return true;
		}
	}
	return false;
}

static bool createTarget(ID3D11Device *device, int width, int height, ComPtr<ID3D11Texture2D> &texture)
{
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = (UINT)width;
	desc.Height = (UINT)height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	return SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &texture));
}

/* The test's own readback, through a staging texture, not the source's (plan Task 6 Step 3). */
static bool readback(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *texture, int width,
		     int height, std::vector<uint8_t> &bgra)
{
	D3D11_TEXTURE2D_DESC desc = {};
	texture->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = 0;
	ComPtr<ID3D11Texture2D> staging;
	if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) {
		return false;
	}
	context->CopyResource(staging.Get(), texture);

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
		return false;
	}
	bgra.assign((size_t)width * height * 4, 0);
	for (int y = 0; y < height; y++) {
		std::memcpy(bgra.data() + (size_t)y * width * 4, (const uint8_t *)mapped.pData + (size_t)y * mapped.RowPitch,
			    (size_t)width * 4);
	}
	context->Unmap(staging.Get(), 0);
	return true;
}

static ComPtr<IWICImagingFactory> wicFactory()
{
	ComPtr<IWICImagingFactory> wic;
	CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
	return wic;
}

static bool formatHasAlpha(const GUID &format)
{
	return IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA) || IsEqualGUID(format, GUID_WICPixelFormat32bppRGBA) ||
	       IsEqualGUID(format, GUID_WICPixelFormat32bppPBGRA) || IsEqualGUID(format, GUID_WICPixelFormat32bppPRGBA) ||
	       IsEqualGUID(format, GUID_WICPixelFormat64bppBGRA) || IsEqualGUID(format, GUID_WICPixelFormat64bppRGBA);
}

/* Reads a PNG as straight BGRA and reports whether it carries alpha. */
static bool loadPng(const std::string &path, std::vector<uint8_t> &bgra, int &width, int &height, bool &hasAlpha)
{
	ComPtr<IWICImagingFactory> wic = wicFactory();
	if (!wic) {
		return false;
	}
	ComPtr<IWICBitmapDecoder> decoder;
	if (FAILED(wic->CreateDecoderFromFilename(toWide(path).c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
						   &decoder))) {
		return false;
	}
	ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(decoder->GetFrame(0, &frame))) {
		return false;
	}
	UINT w = 0, h = 0;
	if (FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0) {
		return false;
	}
	WICPixelFormatGUID source = {};
	frame->GetPixelFormat(&source);
	hasAlpha = formatHasAlpha(source);

	ComPtr<IWICFormatConverter> converter;
	if (FAILED(wic->CreateFormatConverter(&converter))) {
		return false;
	}
	if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
					 WICBitmapPaletteTypeCustom))) {
		return false;
	}
	width = (int)w;
	height = (int)h;
	bgra.assign((size_t)w * h * 4, 0);
	return SUCCEEDED(converter->CopyPixels(nullptr, w * 4, (UINT)bgra.size(), bgra.data()));
}

static bool savePng(const std::string &path, const std::vector<uint8_t> &bgra, int width, int height)
{
	ComPtr<IWICImagingFactory> wic = wicFactory();
	if (!wic) {
		return false;
	}
	ComPtr<IWICStream> stream;
	if (FAILED(wic->CreateStream(&stream)) ||
	    FAILED(stream->InitializeFromFilename(toWide(path).c_str(), GENERIC_WRITE))) {
		return false;
	}
	ComPtr<IWICBitmapEncoder> encoder;
	if (FAILED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
	    FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) {
		return false;
	}
	ComPtr<IWICBitmapFrameEncode> frame;
	ComPtr<IPropertyBag2> props;
	if (FAILED(encoder->CreateNewFrame(&frame, &props)) || FAILED(frame->Initialize(props.Get()))) {
		return false;
	}
	WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
	frame->SetSize((UINT)width, (UINT)height);
	if (FAILED(frame->SetPixelFormat(&format)) || !IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)) {
		return false;
	}
	/* PNG carries straight alpha; the render is premultiplied. Undo it so a written PNG is readable
	 * and so a reference written from one round-trips to the same premultiplied pixels. */
	std::vector<uint8_t> straight = bgra;
	for (size_t i = 0; i + 3 < straight.size(); i += 4) {
		const uint32_t alpha = straight[i + 3];
		if (alpha != 0 && alpha != 255) {
			for (int c = 0; c < 3; c++) {
				straight[i + c] = (uint8_t)std::min(255u, (straight[i + c] * 255u + alpha / 2) / alpha);
			}
		}
	}
	if (FAILED(frame->WritePixels((UINT)height, (UINT)width * 4, (UINT)straight.size(),
				      const_cast<BYTE *>(straight.data())))) {
		return false;
	}
	return SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
}

/* vMix's SnapshotInput PNG is labelled straight but stores premultiplied colour (plan Finding 1:
 * its pixel (960,1060), alpha 227, holds 212,110,9, which is its source 238,123,10 premultiplied —
 * 238*227/255, 123*227/255 — not the straight 238,123,10 the label implies). Our render is
 * premultiplied over black already, so both sides are compared as premultiplied RGB: the reference
 * channel is used as-is. A reference with no alpha has alpha 255, so its channel is that value over
 * black too — the same thing. */
static void compareImages(const std::vector<uint8_t> &ours, int ow, int oh, const std::vector<uint8_t> &ref, int rw,
			  int rh, bool refHasAlpha, double &mean, double &badPercent, std::vector<uint8_t> &diff)
{
	const int width = std::min(ow, rw);
	const int height = std::min(oh, rh);
	uint64_t sum = 0;
	uint64_t badCount = 0;
	const uint64_t pixels = (uint64_t)width * height;
	diff.assign((size_t)width * height * 4, 0);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			const uint8_t *o = ours.data() + ((size_t)y * ow + x) * 4;
			const uint8_t *r = ref.data() + ((size_t)y * rw + x) * 4;
			int worst = 0;
			for (int c = 0; c < 3; c++) {
				const int d = std::abs((int)o[c] - (int)r[c]);
				sum += (uint64_t)d;
				worst = std::max(worst, d);
			}
			if (worst > BadChannel) {
				badCount++;
			}
			uint8_t *d = diff.data() + ((size_t)y * width + x) * 4;
			for (int c = 0; c < 3; c++) {
				d[c] = (uint8_t)std::clamp(std::abs((int)o[c] - (int)r[c]), 0, 255);
			}
			d[3] = 255;
		}
	}
	mean = pixels ? (double)sum / (double)(pixels * 3) : 0.0;
	badPercent = pixels ? 100.0 * (double)badCount / (double)pixels : 0.0;
}

/* Task 1: an equal premultiplied pair reads zero (vMix's snapshot stores premultiplied colour), and a
 * one-channel difference of 40 is a bad pixel. */
static void compareCheck()
{
	const std::vector<uint8_t> ours = {9, 110, 212, 227}; /* BGRA, premultiplied */
	double mean = -1, bad = -1;
	std::vector<uint8_t> diff;
	compareImages(ours, 1, 1, ours, 1, 1, true, mean, bad, diff);
	REQUIRE(mean == 0.0);

	std::vector<uint8_t> shifted = ours;
	shifted[2] = (uint8_t)(ours[2] + 40);
	compareImages(ours, 1, 1, shifted, 1, 1, true, mean, bad, diff);
	REQUIRE(bad == 100.0);
}

/* One glyph run that has no glyphs: used to see whether shaping produced .notdef. */
class NotdefRenderer : public IDWriteTextRenderer {
public:
	bool sawNotdef = false;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override
	{
		if (!object) {
			return E_POINTER;
		}
		if (IsEqualIID(iid, __uuidof(IUnknown)) || IsEqualIID(iid, __uuidof(IDWriteTextRenderer)) ||
		    IsEqualIID(iid, __uuidof(IDWritePixelSnapping))) {
			*object = this;
			AddRef();
			return S_OK;
		}
		*object = nullptr;
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refs_); }
	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG n = InterlockedDecrement(&refs_);
		if (n == 0) {
			delete this;
		}
		return n;
	}

	HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void *, BOOL *disabled) override
	{
		*disabled = TRUE;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetCurrentTransform(void *, DWRITE_MATRIX *transform) override
	{
		*transform = DWRITE_MATRIX{1, 0, 0, 1, 0, 0};
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void *, FLOAT *pixelsPerDip) override
	{
		*pixelsPerDip = 1.0f;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE DrawGlyphRun(void *, FLOAT, FLOAT, DWRITE_MEASURING_MODE, const DWRITE_GLYPH_RUN *run,
					       const DWRITE_GLYPH_RUN_DESCRIPTION *, IUnknown *) override
	{
		for (UINT32 i = 0; i < run->glyphCount; i++) {
			if (run->glyphIndices[i] == 0) {
				sawNotdef = true;
			}
		}
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE DrawUnderline(void *, FLOAT, FLOAT, const DWRITE_UNDERLINE *, IUnknown *) override
	{
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE DrawStrikethrough(void *, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH *,
						    IUnknown *) override
	{
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE DrawInlineObject(void *, FLOAT, FLOAT, IDWriteInlineObject *, BOOL, BOOL,
						   IUnknown *) override
	{
		return S_OK;
	}

private:
	LONG refs_ = 1;
};

/* Review Focus 2: the Vietnamese line survives shaping with no .notdef glyph. */
static void glyphCheck()
{
	ComPtr<IDWriteFactory> dwrite;
	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&dwrite))) {
		check(false, __LINE__, "DWriteCreateFactory");
		failures++;
		return;
	}
	const std::wstring line = L"Nguy\u1ec5n V\u0103n \u00c1";
	ComPtr<IDWriteTextFormat> format;
	REQUIRE(SUCCEEDED(dwrite->CreateTextFormat(L"Arial", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
						   DWRITE_FONT_STRETCH_NORMAL, 32.0f, L"", &format)));
	if (!format) {
		return;
	}
	ComPtr<IDWriteTextLayout> layout;
	REQUIRE(SUCCEEDED(dwrite->CreateTextLayout(line.c_str(), (UINT32)line.size(), format.Get(), 512.0f, 128.0f, &layout)));
	if (!layout) {
		return;
	}
	NotdefRenderer renderer;
	REQUIRE(SUCCEEDED(layout->Draw(nullptr, &renderer, 0, 0)));
	REQUIRE(!renderer.sawNotdef);
}

/* The renderer's own check: a red rectangle and Vietnamese text, drawn and read
 * back, so a broken draw is caught with no vMix reference at hand. */
static void selfCheck(ID3D11Device *device, ID3D11DeviceContext *context)
{
	const int width = 320, height = 120;
	ComPtr<ID3D11Texture2D> target;
	REQUIRE(createTarget(device, width, height, target));
	if (!target) {
		return;
	}

	GtDocument document;
	document.width = (float)width;
	document.height = (float)height;
	GtObject rectangle;
	rectangle.kind = "Rectangle";
	rectangle.name = "Bg";
	rectangle.width = (float)width;
	rectangle.height = (float)height;
	rectangle.fill.type = GtBrushType::Solid;
	rectangle.fill.argb = 0xFFFF0000u;
	document.objects.push_back(rectangle);

	/* A linear gradient: makeBrush turns the stops into a gradient brush. */
	GtObject gradient;
	gradient.kind = "Rectangle";
	gradient.name = "Grad";
	gradient.x = 0;
	gradient.y = 56;
	gradient.width = 30;
	gradient.height = 30;
	gradient.fill.type = GtBrushType::Linear;
	gradient.fill.stops = {{0.0f, 0xFFFF0000u}, {1.0f, 0xFF00FF00u}};
	document.objects.push_back(gradient);

	/* A nested Layer: its child is drawn in the layer's composition space. */
	GtObject layer;
	layer.kind = "Layer";
	layer.name = "Group";
	layer.x = 40;
	layer.y = 56;
	layer.compWidth = 30;
	layer.compHeight = 30;
	GtObject layerChild;
	layerChild.kind = "Rectangle";
	layerChild.name = "InGroup";
	layerChild.width = 20;
	layerChild.height = 20;
	layerChild.fill.type = GtBrushType::Solid;
	layerChild.fill.argb = 0xFF00FF00u;
	layer.children.push_back(layerChild);
	document.objects.push_back(layer);

	/* A shadowed object: its shadow is drawn under it. */
	GtObject shadowed;
	shadowed.kind = "Rectangle";
	shadowed.name = "Shadowed";
	shadowed.x = 80;
	shadowed.y = 56;
	shadowed.width = 40;
	shadowed.height = 20;
	shadowed.fill.type = GtBrushType::Solid;
	shadowed.fill.argb = 0xFF0000FFu;
	GtEffect shadow;
	shadow.type = "Shadow";
	shadow.offsetX = 3;
	shadow.offsetY = 3;
	shadow.blur = 8;
	shadow.argb = 0x80000000u;
	shadowed.effects.push_back(shadow);
	document.objects.push_back(shadowed);

	/* A masked object: the clip is a smaller sibling shape (§3.3). */
	GtObject clip;
	clip.kind = "Rectangle";
	clip.name = "Clip";
	clip.x = 140;
	clip.y = 52;
	clip.width = 40;
	clip.height = 40;
	clip.opacity = 0;
	document.objects.push_back(clip);
	GtObject masked;
	masked.kind = "Rectangle";
	masked.name = "Masked";
	masked.x = 130;
	masked.y = 48;
	masked.width = 60;
	masked.height = 60;
	masked.mask = "Clip";
	masked.fill.type = GtBrushType::Solid;
	masked.fill.argb = 0xFF00FF00u;
	document.objects.push_back(masked);

	/* A Text3D drawn flat (depth not drawn), and an Image with no picture: both warn. */
	GtObject deep;
	deep.kind = "Text3D";
	deep.name = "Deep";
	deep.x = 200;
	deep.y = 56;
	deep.width = 60;
	deep.height = 30;
	deep.depth = 20;
	deep.text = "3D";
	deep.fontFamily = "Arial";
	deep.fontSize = 24;
	deep.fill.type = GtBrushType::Solid;
	deep.fill.argb = 0xFFFFFFFFu;
	document.objects.push_back(deep);
	GtObject image;
	image.kind = "Image";
	image.name = "Missing";
	image.x = 270;
	image.y = 56;
	image.width = 40;
	image.height = 40;
	image.bitmapSource = "Nope.png";
	document.objects.push_back(image);

	GtObject text;
	text.kind = "TextBlock";
	text.name = "VN";
	text.x = 4;
	text.y = 4;
	text.width = (float)(width - 8);
	text.height = 40;
	text.text = toUtf8(L"Nguy\u1ec5n V\u0103n \u00c1");
	text.fontFamily = "Arial";
	text.fontSize = 28;
	text.fill.type = GtBrushType::Solid;
	text.fill.argb = 0xFFFFFFFFu;
	document.objects.push_back(text);

	GtRenderer renderer;
	std::string error;
	REQUIRE(renderer.init(device, error));
	GtPackage package;
	renderer.load(package, document);

	GtFrameState state;
	REQUIRE(renderer.draw(target.Get(), state));

	std::vector<uint8_t> pixels;
	REQUIRE(readback(device, context, target.Get(), width, height, pixels));
	if (pixels.empty()) {
		return;
	}

	const auto channel = [&](int x, int y, int c) { return pixels[((size_t)y * width + x) * 4 + c]; };
	const uint8_t *corner = pixels.data() + ((size_t)(height - 4) * width + (width - 4)) * 4;
	REQUIRE(corner[2] > 200 && corner[1] < 64 && corner[0] < 64); /* BGRA red */
	REQUIRE(channel(50, 66, 1) > 200 && channel(50, 66, 2) < 64); /* layer child green */
	REQUIRE(channel(95, 66, 0) > 200 && channel(95, 66, 1) < 64); /* shadowed blue */
	REQUIRE(channel(150, 70, 1) > 200);                           /* masked green, inside the clip */
	REQUIRE(channel(135, 70, 2) > 200 && channel(135, 70, 1) < 64); /* masked red, clipped away */

	int white = 0;
	for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
		if (pixels[i] > 200 && pixels[i + 1] > 200 && pixels[i + 2] > 200 && pixels[i + 3] > 200) {
			white++;
		}
	}
	REQUIRE(white > 0);
}

/* Task 2 (Review Focus 2): a shadow is offset once, in the object's own space. The same must hold
 * when the object sits inside a Layer, whose translation is part of the object's matrix. Two
 * documents, drawn on their own, so one shadow cannot satisfy the other's check. */
static void shadowCheck(ID3D11Device *device, ID3D11DeviceContext *context)
{
	const int width = 320, height = 120;

	GtObject shadowed;
	shadowed.kind = "Rectangle";
	shadowed.name = "Shadowed";
	shadowed.width = 40;
	shadowed.height = 20;
	shadowed.fill.type = GtBrushType::Solid;
	shadowed.fill.argb = 0xFFFFFFFFu;
	GtEffect shadow;
	shadow.type = "Shadow";
	shadow.offsetX = 10;
	shadow.offsetY = 10;
	shadow.blur = 0;
	shadow.argb = 0xFF000000u;
	shadowed.effects.push_back(shadow);

	const auto draw = [&](const GtDocument &document, std::vector<uint8_t> &pixels) {
		ComPtr<ID3D11Texture2D> target;
		REQUIRE(createTarget(device, width, height, target));
		if (!target) {
			return;
		}
		GtRenderer renderer;
		std::string error;
		REQUIRE(renderer.init(device, error));
		GtPackage package;
		renderer.load(package, document);
		GtFrameState state;
		REQUIRE(renderer.draw(target.Get(), state));
		REQUIRE(readback(device, context, target.Get(), width, height, pixels));
	};

	GtDocument plain;
	plain.width = (float)width;
	plain.height = (float)height;
	GtObject atOrigin = shadowed;
	atOrigin.x = 80;
	atOrigin.y = 56;
	plain.objects.push_back(atOrigin);
	std::vector<uint8_t> plainPixels;
	draw(plain, plainPixels);
	if (!plainPixels.empty()) {
		const size_t pixel = ((size_t)80 * width + 125) * 4;
		REQUIRE(plainPixels[pixel + 3] > 128); /* inside the shadow, outside the object */
	}

	GtDocument layered;
	layered.width = (float)width;
	layered.height = (float)height;
	GtObject layer;
	layer.kind = "Layer";
	layer.name = "Group";
	layer.x = 20;
	layer.y = 0;
	layer.compWidth = (float)width;
	layer.compHeight = (float)height;
	GtObject inLayer = shadowed;
	inLayer.x = 80;
	inLayer.y = 56;
	layer.children.push_back(inLayer);
	layered.objects.push_back(layer);
	std::vector<uint8_t> layeredPixels;
	draw(layered, layeredPixels);
	if (!layeredPixels.empty()) {
		const size_t pixel = ((size_t)80 * width + 145) * 4;
		REQUIRE(layeredPixels[pixel + 3] > 128); /* the layer's Location is part of the matrix */
	}
}

/* Task 3 (Finding 4): vMix cuts a TextBlock's text at its box's top and bottom edges. This text
 * wraps past a 200x30 box; nothing below the box may be painted, and the text inside it must
 * still be drawn. */
/* A picture that an invisible object names is not missing: the owner's titles keep references to
 * layout-preview art stripped from the package, and vMix does not draw those objects. */
static void warningCheck(ID3D11Device *device, ID3D11DeviceContext *context)
{
	(void)context;
	GtDocument document;
	document.width = 64;
	document.height = 64;

	GtObject shown;
	shown.kind = "Image";
	shown.name = "Shown";
	shown.width = 64;
	shown.height = 64;
	shown.bitmapSource = "gone\\shown.png";
	document.objects.push_back(shown);

	GtObject hidden;
	hidden.kind = "Image";
	hidden.name = "Preview";
	hidden.width = 64;
	hidden.height = 64;
	hidden.visible = false;
	hidden.bitmapSource = "gone\\preview.png";
	document.objects.push_back(hidden);

	GtRenderer renderer;
	std::string error;
	REQUIRE(renderer.init(device, error));
	GtPackage package;
	renderer.load(package, document);

	bool missing = false;
	for (const GtWarning &warning : renderer.warnings()) {
		if (warning.kind != "missing-resources") {
			continue;
		}
		missing = true;
		REQUIRE(warning.detail.find("shown.png") != std::string::npos);
		REQUIRE(warning.detail.find("preview.png") == std::string::npos);
	}
	REQUIRE(missing);
}

static void clipCheck(ID3D11Device *device, ID3D11DeviceContext *context)
{
	const int width = 320, height = 120;
	ComPtr<ID3D11Texture2D> target;
	REQUIRE(createTarget(device, width, height, target));
	if (!target) {
		return;
	}

	GtDocument document;
	document.width = (float)width;
	document.height = (float)height;
	GtObject text;
	text.kind = "TextBlock";
	text.name = "Long";
	text.x = 10;
	text.y = 10;
	text.width = 200;
	text.height = 30;
	text.text = "ONE TWO THREE FOUR FIVE SIX SEVEN";
	text.fontFamily = "Arial";
	text.fontSize = 28;
	text.fill.type = GtBrushType::Solid;
	text.fill.argb = 0xFFFFFFFFu;
	document.objects.push_back(text);

	GtRenderer renderer;
	std::string error;
	REQUIRE(renderer.init(device, error));
	GtPackage package;
	renderer.load(package, document);
	GtFrameState state;
	REQUIRE(renderer.draw(target.Get(), state));

	std::vector<uint8_t> pixels;
	REQUIRE(readback(device, context, target.Get(), width, height, pixels));
	if (pixels.empty()) {
		return;
	}

	bool belowClear = true;
	bool white = false;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			const uint8_t *pixel = pixels.data() + ((size_t)y * width + x) * 4;
			if (y >= 41 && pixel[3] != 0) {
				belowClear = false;
			}
			const bool inside = y >= 10 && y < 40;
			if (inside && pixel[3] > 200 && pixel[0] > 200 && pixel[1] > 200 && pixel[2] > 200) {
				white = true;
			}
		}
	}
	REQUIRE(belowClear);
	REQUIRE(white);
}

static fs::path withSuffix(const fs::path &png, const char *suffix)
{
	fs::path out = png;
	out.replace_filename(png.stem().string() + suffix + png.extension().string());
	return out;
}

int main(int argc, char **argv)
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	if (!createDevice(device, context)) {
		std::printf("FAIL: no D3D11 device with BGRA support\n");
		return 1;
	}

	compareCheck();
	selfCheck(device.Get(), context.Get());
	shadowCheck(device.Get(), context.Get());
	clipCheck(device.Get(), context.Get());
	warningCheck(device.Get(), context.Get());
	glyphCheck();

	if (argc < 4) {
		std::printf("SKIP: no folders given (usage: GtRenderTest <gtzip folder> <reference folder> <out folder>)\n");
		return failures ? 1 : 0;
	}

	const fs::path source = fs::path(toWide(argv[1]));
	const fs::path reference = fs::path(toWide(argv[2]));
	const fs::path out = fs::path(toWide(argv[3]));
	if (!fs::is_directory(source)) {
		std::printf("SKIP: %s is not a folder\n", argv[1]);
		return failures ? 1 : 0;
	}
	if (!fs::is_directory(reference)) {
		std::printf("SKIP: reference folder %s is not there; comparison skipped\n", argv[2]);
		return failures ? 1 : 0;
	}

	std::error_code ec;
	fs::create_directories(out, ec);

	std::vector<fs::path> files;
	for (const fs::directory_entry &entry : fs::recursive_directory_iterator(source, ec)) {
		if (entry.is_regular_file() && entry.path().extension() == ".gtzip") {
			files.push_back(entry.path());
		}
	}
	std::sort(files.begin(), files.end());

	int failedFiles = 0;
	int compared = 0;
	for (const fs::path &file : files) {
		const std::string name = fs::relative(file, source, ec).generic_string();
		fs::path outPng = out / fs::path(name);
		outPng.replace_extension(".png");
		fs::path diffPng = withSuffix(outPng, "-diff");
		fs::create_directories(outPng.parent_path(), ec);

		GtPackage package;
		GtDocument document;
		std::string error;
		if (!readGtFile(file.string(), package, error)) {
			std::printf("%s FAIL: %s\n", name.c_str(), error.c_str());
			failedFiles++;
			continue;
		}
		if (!parseGtDocument(package.documentXml, document, error)) {
			std::printf("%s FAIL: %s\n", name.c_str(), error.c_str());
			failedFiles++;
			continue;
		}

		const int width = (int)std::lround(document.width);
		const int height = (int)std::lround(document.height);
		ComPtr<ID3D11Texture2D> target;
		if (width <= 0 || height <= 0 || !createTarget(device.Get(), width, height, target)) {
			std::printf("%s FAIL: could not make a %dx%d target\n", name.c_str(), width, height);
			failedFiles++;
			continue;
		}

		GtRenderer renderer;
		if (!renderer.init(device.Get(), error)) {
			std::printf("%s FAIL: init: %s\n", name.c_str(), error.c_str());
			failedFiles++;
			continue;
		}
		renderer.load(package, document);
		/* The source expands a clock's {0:...} when it draws (breadstudio/gt/gt-source.cpp), so the
		 * test must too, or a clock field would be compared to vMix as its raw format string. A
		 * fixed time keeps a render deterministic and therefore comparable. */
		GtFrameState state;
		const GtClockTime at = {2026, 10, 19, 21, 5, 9, 42, 1};
		for (const GtField &field : gtFields(document)) {
			if (field.type == GtFieldType::Text) {
				state.text[field.name] = gtExpandClock(field.defaultValue, at).text;
			}
		}
		std::vector<uint8_t> ours;
		if (!renderer.draw(target.Get(), state) ||
		    !readback(device.Get(), context.Get(), target.Get(), width, height, ours)) {
			std::printf("%s FAIL: draw or readback\n", name.c_str());
			failedFiles++;
			continue;
		}
		savePng(outPng.string(), ours, width, height);

		fs::path refPng = reference / fs::path(name);
		refPng.replace_extension(".png");
		std::vector<uint8_t> ref;
		int rw = 0, rh = 0;
		bool refHasAlpha = false;
		if (!loadPng(refPng.string(), ref, rw, rh, refHasAlpha)) {
			std::printf("%s FAIL: no reference at %s\n", name.c_str(), refPng.string().c_str());
			failedFiles++;
			continue;
		}
		if (rw != width || rh != height) {
			std::printf("%s FAIL: reference is %dx%d, render is %dx%d\n", name.c_str(), rw, rh, width, height);
			failedFiles++;
			continue;
		}

		double mean = 0, bad = 0;
		std::vector<uint8_t> diff;
		compareImages(ours, width, height, ref, rw, rh, refHasAlpha, mean, bad, diff);
		savePng(diffPng.string(), diff, width, height);

		const bool ok = mean <= MeanLimit && bad <= BadLimit;
		const char *known = ok ? nullptr : knownReason(name);
		if (known) {
			std::printf("%s mean=%.3f bad=%.3f%% known: %s\n", name.c_str(), mean, bad, known);
		} else {
			std::printf("%s mean=%.3f bad=%.3f%% %s\n", name.c_str(), mean, bad, ok ? "ok" : "FAIL");
			if (!ok) {
				failedFiles++;
			}
		}
		compared++;
	}

	std::printf("GtRender: %d file(s), %d compared, %d failed\n", (int)files.size(), compared, failedFiles);
	return failures + failedFiles;
}
