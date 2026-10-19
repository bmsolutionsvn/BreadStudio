#include "GtRenderer.hpp"

#include <windows.h>

/* The Direct2D effect CLSIDs are DEFINE_GUID declarations; this instantiates them here. */
#include <initguid.h>

#include <d2d1_1.h>
#include <d2d1effects.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "GtPackage.hpp"

using Microsoft::WRL::ComPtr;

/* GtText.cpp lays out one text object and outlines its glyph runs. Returns false when the object's
 * effective font family is not installed, naming it in missingFamily. */
namespace gt {
bool gtDrawText(ID2D1DeviceContext *context, IDWriteFactory *dwrite, ID2D1Factory *d2dFactory, const GtObject &object,
		const GtStyle *style, const std::string &text, ID2D1Brush *fill, ID2D1Brush *stroke,
		std::string &missingFamily);
}

namespace gt {

struct GtRendererImpl {
	struct Picture {
		std::vector<uint8_t> pixels;
		int width = 0;
		int height = 0;
	};

	ComPtr<ID3D11Device> device;
	ComPtr<ID2D1Factory1> d2dFactory;
	ComPtr<IDWriteFactory> dwrite;
	ComPtr<ID2D1Device> d2dDevice;
	ComPtr<ID2D1DeviceContext> context;

	ID3D11Texture2D *targetTexture = nullptr; /* borrowed for the draw, not owned */
	ComPtr<ID2D1Bitmap1> targetBitmap;

	GtDocument document;
	std::map<std::string, Picture> resources; /* decoded package pictures, by source name */
	std::map<std::string, Picture> pictures;  /* setPicture pixels, by field path */
	std::map<std::string, ComPtr<ID2D1Bitmap1>> bitmaps;

	std::vector<GtWarning> loadWarnings;
	std::vector<GtWarning> drawWarnings;
};

namespace {

D2D1_COLOR_F toColour(uint32_t argb)
{
	return D2D1::ColorF(((argb >> 16) & 0xFF) / 255.0f, ((argb >> 8) & 0xFF) / 255.0f, (argb & 0xFF) / 255.0f,
			    ((argb >> 24) & 0xFF) / 255.0f);
}

/* A draw's warnings, the font families to report, and whether this is a shadow's dry run. */
struct DrawContext {
	const GtFrameState *state = nullptr;
	bool recordOnly = false;
	std::vector<GtWarning> warnings;
	std::map<std::string, std::vector<std::string>> fontFamilies;
};

/* Each shadowed object's own drawing, once, for reuse as its shadow effect's input. */
using ShadowLists = std::map<const GtObject *, ComPtr<ID2D1CommandList>>;

void addWarning(DrawContext &ctx, const std::string &kind, const std::string &detail)
{
	if (ctx.recordOnly) {
		return;
	}
	ctx.warnings.push_back({kind, detail});
}

std::string fieldName(const GtObject &object)
{
	if (object.kind == "TextBlock" || object.kind == "Text3D") {
		return object.name + ".Text";
	}
	if (object.kind == "Image") {
		return object.name + ".Source";
	}
	if (object.kind == "Rectangle" || object.kind == "Ellipse") {
		return object.name + ".Fill.Color";
	}
	return std::string();
}

const GtObject *findSibling(const std::vector<GtObject> &objects, const std::string &name)
{
	for (const GtObject &object : objects) {
		if (object.name == name) {
			return &object;
		}
	}
	return nullptr;
}

const GtEffect *findShadow(const GtObject &object)
{
	for (const GtEffect &effect : object.effects) {
		if (effect.type == "Shadow") {
			return &effect;
		}
	}
	return nullptr;
}

/* Location, then every Skew about the object's centre (§3.2). */
D2D1_MATRIX_3X2_F objectTransform(const GtObject &object, const D2D1_MATRIX_3X2_F &parent, DrawContext &ctx)
{
	D2D1_MATRIX_3X2_F matrix = parent * D2D1::Matrix3x2F::Translation(object.x, object.y);
	for (const GtEffect &effect : object.effects) {
		if (effect.type == "Skew") {
			matrix = matrix * D2D1::Matrix3x2F::Skew(effect.angle, 0.0f,
								 D2D1::Point2F(object.width / 2, object.height / 2));
		} else if (effect.type != "Shadow") {
			addWarning(ctx, "unsupported", object.name + ": " + effect.type);
		}
	}
	return matrix;
}

ComPtr<ID2D1Brush> makeBrush(GtRendererImpl &impl, const GtBrush &brush, uint32_t overrideArgb, bool hasOverride,
			     float width, float height)
{
	ComPtr<ID2D1Brush> result;
	uint32_t argb = brush.argb;
	D2D1_BRUSH_PROPERTIES properties = D2D1::BrushProperties();

	if (hasOverride) {
		ComPtr<ID2D1SolidColorBrush> solid;
		if (SUCCEEDED(impl.context->CreateSolidColorBrush(toColour(overrideArgb), properties, &solid))) {
			result = solid;
		}
		return result;
	}

	switch (brush.type) {
	case GtBrushType::None:
		return result;
	case GtBrushType::Transparent: {
		ComPtr<ID2D1SolidColorBrush> solid;
		if (SUCCEEDED(impl.context->CreateSolidColorBrush(D2D1::ColorF(0, 0), properties, &solid))) {
			result = solid;
		}
		return result;
	}
	case GtBrushType::Solid: {
		ComPtr<ID2D1SolidColorBrush> solid;
		if (SUCCEEDED(impl.context->CreateSolidColorBrush(toColour(argb), properties, &solid))) {
			result = solid;
		}
		return result;
	}
	case GtBrushType::Linear:
		break;
	}

	/* A gradient with no stops is the brush's own colour; a lone colour is solid. */
	if (brush.stops.size() < 2) {
		ComPtr<ID2D1SolidColorBrush> solid;
		if (SUCCEEDED(impl.context->CreateSolidColorBrush(toColour(argb), properties, &solid))) {
			result = solid;
		}
		return result;
	}

	std::vector<D2D1_GRADIENT_STOP> stops;
	stops.reserve(brush.stops.size());
	for (const auto &stop : brush.stops) {
		stops.push_back({stop.first, toColour(stop.second)});
	}
	std::sort(stops.begin(), stops.end(),
		  [](const D2D1_GRADIENT_STOP &a, const D2D1_GRADIENT_STOP &b) { return a.position < b.position; });

	ComPtr<ID2D1GradientStopCollection> collection;
	if (FAILED(impl.context->CreateGradientStopCollection(stops.data(), (UINT32)stops.size(), D2D1_GAMMA_2_2,
							     D2D1_EXTEND_MODE_CLAMP, &collection))) {
		return result;
	}
	const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES gradient = D2D1::LinearGradientBrushProperties(
		D2D1::Point2F(brush.startX * width, brush.startY * height),
		D2D1::Point2F(brush.endX * width, brush.endY * height));
	ComPtr<ID2D1LinearGradientBrush> linear;
	if (SUCCEEDED(impl.context->CreateLinearGradientBrush(gradient, properties, collection.Get(), &linear))) {
		result = linear;
	}
	return result;
}

/* A mask shape's geometry, at its own composition position (§3.3). */
ComPtr<ID2D1Geometry> shapeGeometry(GtRendererImpl &impl, const GtObject &shape)
{
	ComPtr<ID2D1Geometry> geometry;
	if (shape.kind == "Rectangle") {
		const D2D1_ROUNDED_RECT rect = D2D1::RoundedRect(
			D2D1::RectF(shape.x, shape.y, shape.x + shape.width, shape.y + shape.height), shape.radiusX,
			shape.radiusY);
		ComPtr<ID2D1RoundedRectangleGeometry> rounded;
		if (SUCCEEDED(impl.d2dFactory->CreateRoundedRectangleGeometry(rect, &rounded))) {
			geometry = rounded;
		}
	} else if (shape.kind == "Ellipse") {
		const D2D1_ELLIPSE ellipse = D2D1::Ellipse(
			D2D1::Point2F(shape.x + shape.width / 2, shape.y + shape.height / 2), shape.width / 2,
			shape.height / 2);
		ComPtr<ID2D1EllipseGeometry> oval;
		if (SUCCEEDED(impl.d2dFactory->CreateEllipseGeometry(ellipse, &oval))) {
			geometry = oval;
		}
	}
	return geometry;
}

ID2D1Bitmap1 *getBitmap(GtRendererImpl &impl, const std::string &source)
{
	const auto cached = impl.bitmaps.find(source);
	if (cached != impl.bitmaps.end()) {
		return cached->second.Get();
	}
	const GtRendererImpl::Picture *picture = nullptr;
	const auto resource = impl.resources.find(source);
	if (resource != impl.resources.end()) {
		picture = &resource->second;
	} else {
		const auto file = impl.pictures.find(source);
		if (file != impl.pictures.end()) {
			picture = &file->second;
		}
	}
	if (!picture || picture->pixels.empty() || picture->width <= 0 || picture->height <= 0) {
		return nullptr;
	}
	const D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
	ComPtr<ID2D1Bitmap1> bitmap;
	if (FAILED(impl.context->CreateBitmap(D2D1::SizeU((UINT32)picture->width, (UINT32)picture->height),
					      picture->pixels.data(), (UINT32)picture->width * 4, properties, &bitmap))) {
		return nullptr;
	}
	ComPtr<ID2D1Bitmap1> added = bitmap;
	impl.bitmaps[source] = added;
	return impl.bitmaps[source].Get();
}

void drawImage(GtRendererImpl &impl, DrawContext &ctx, const GtObject &object)
{
	std::string source = object.bitmapSource;
	const std::string field = fieldName(object);
	if (!field.empty()) {
		const auto value = ctx.state->picture.find(field);
		if (value != ctx.state->picture.end()) {
			source = value->second;
		}
	}
	if (source.empty()) {
		return;
	}
	ID2D1Bitmap1 *bitmap = getBitmap(impl, source);
	if (!bitmap) {
		addWarning(ctx, "picture", field.empty() ? source : field + ": " + source);
		return;
	}
	const float bitmapWidth = (float)bitmap->GetPixelSize().width;
	const float bitmapHeight = (float)bitmap->GetPixelSize().height;
	const D2D1_RECT_F src = D2D1::RectF(0, 0, bitmapWidth, bitmapHeight);
	/* vMix scales a picture into the object's rectangle; the object's size is the box, not the
	 * picture's own pixels. MAN OF THE MATCH's photo is 3460x5189 in a 1312x1271 field, and vMix
	 * draws it about 848 wide (uniform) rather than 3460, which is what the reference comparison
	 * against vMix's own snapshots showed. */
	D2D1_RECT_F dest = D2D1::RectF(0, 0, bitmapWidth, bitmapHeight); /* Normal: its own pixels */
	if (object.sizeMode != "Normal" && bitmapWidth > 0 && bitmapHeight > 0 && object.width > 0 &&
	    object.height > 0) {
		float w = object.width;
		float h = object.height;
		if (object.sizeMode != "Stretch") {
			const float sx = object.width / bitmapWidth;
			const float sy = object.height / bitmapHeight;
			const float scale = sx < sy ? sx : sy; /* uniform, so the aspect is kept */
			w = bitmapWidth * scale;
			h = bitmapHeight * scale;
		}
		float x = 0;
		float y = 0;
		if (object.sizeMode == "Centered") {
			x = (object.width - w) / 2;
			y = (object.height - h) / 2;
		} else if (object.sizeMode == "TopRight") {
			x = object.width - w;
		}
		dest = D2D1::RectF(x, y, x + w, y + h);
	}
	impl.context->DrawBitmap(bitmap, dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, src);
}

void drawShape(GtRendererImpl &impl, DrawContext &ctx, const GtObject &object)
{
	const std::string field = fieldName(object);
	uint32_t argb = 0;
	bool hasOverride = false;
	if (!field.empty()) {
		const auto value = ctx.state->colour.find(field);
		if (value != ctx.state->colour.end()) {
			argb = value->second;
			hasOverride = true;
		}
	}
	ComPtr<ID2D1Brush> fill = makeBrush(impl, object.fill, argb, hasOverride, object.width, object.height);
	ComPtr<ID2D1Brush> stroke = makeBrush(impl, object.stroke, 0, false, object.width, object.height);
	const bool hasStroke = stroke && object.strokeThickness > 0;

	if (object.kind == "Ellipse") {
		const D2D1_ELLIPSE ellipse = D2D1::Ellipse(
			D2D1::Point2F(object.width / 2, object.height / 2), object.width / 2, object.height / 2);
		if (fill) {
			impl.context->FillEllipse(ellipse, fill.Get());
		}
		if (hasStroke) {
			impl.context->DrawEllipse(ellipse, stroke.Get(), object.strokeThickness, nullptr);
		}
		return;
	}
	const D2D1_ROUNDED_RECT rect = D2D1::RoundedRect(D2D1::RectF(0, 0, object.width, object.height),
							 object.radiusX, object.radiusY);
	if (fill) {
		impl.context->FillRoundedRectangle(rect, fill.Get());
	}
	if (hasStroke) {
		impl.context->DrawRoundedRectangle(rect, stroke.Get(), object.strokeThickness, nullptr);
	}
}

void drawText(GtRendererImpl &impl, DrawContext &ctx, const GtObject &object)
{
	const std::string field = fieldName(object);
	std::string text = object.text;
	const GtStyle *style = nullptr;
	if (!field.empty()) {
		const auto value = ctx.state->text.find(field);
		if (value != ctx.state->text.end()) {
			text = value->second;
		}
		const auto override = ctx.state->style.find(field);
		if (override != ctx.state->style.end()) {
			style = &override->second;
		}
	}
	if (object.kind == "Text3D" && object.depth > 0) {
		addWarning(ctx, "text3d", object.name + ": its depth is not drawn");
	}

	uint32_t fillOverride = 0;
	bool hasOverride = false;
	if (style && style->fill) {
		fillOverride = *style->fill;
		hasOverride = true;
	}
	ComPtr<ID2D1Brush> fill = makeBrush(impl, object.fill, fillOverride, hasOverride, object.width, object.height);
	ComPtr<ID2D1Brush> stroke = makeBrush(impl, object.stroke, 0, false, object.width, object.height);
	/* vMix cuts a TextBlock's text at its box's top and bottom edges (plan Finding 4): text that
	 * wraps past Dimensions must not paint over its neighbours. The context is in the object's
	 * space here, so the box is (0,0) to its size. */
	const float clipWidth = object.width > 0 ? object.width : 1.0f;
	const float clipHeight = object.height > 0 ? object.height : 1.0f;
	/* ponytail: an axis-aligned clip uses the transformed box's bounds under Skew, not the skewed
	 * box itself; a geometry layer is the upgrade if a skewed overflowing title ever appears. */
	impl.context->PushAxisAlignedClip(D2D1::RectF(0, 0, clipWidth, clipHeight),
					  D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	std::string missingFamily;
	const bool drawn = gtDrawText(impl.context.Get(), impl.dwrite.Get(), impl.d2dFactory.Get(), object, style, text,
				      fill.Get(), stroke.Get(), missingFamily);
	impl.context->PopAxisAlignedClip();
	if (!drawn) {
		ctx.fontFamilies[missingFamily].push_back(field.empty() ? object.name : field);
	}
}

void drawContent(GtRendererImpl &impl, DrawContext &ctx, const std::vector<GtObject> &siblings, const GtObject &object,
		 const D2D1_MATRIX_3X2_F &transform, const ShadowLists &lists);

void drawComposition(GtRendererImpl &impl, DrawContext &ctx, const std::vector<GtObject> &objects,
		     const D2D1_MATRIX_3X2_F &transform, const ShadowLists &lists);

void drawContent(GtRendererImpl &impl, DrawContext &ctx, const std::vector<GtObject> &siblings, const GtObject &object,
		 const D2D1_MATRIX_3X2_F &transform, const ShadowLists &lists)
{
	if (object.kind == "Layer") {
		drawComposition(impl, ctx, object.children, transform, lists);
	} else if (object.kind == "TextBlock" || object.kind == "Text3D") {
		drawText(impl, ctx, object);
	} else if (object.kind == "Image") {
		drawImage(impl, ctx, object);
	} else if (object.kind == "Rectangle" || object.kind == "Ellipse") {
		drawShape(impl, ctx, object);
	} else {
		addWarning(ctx, "unsupported", object.kind + " " + object.name);
	}
}

/* Records a shadowed object's own drawing once, outside BeginDraw, as its shadow's input. */
void recordShadows(GtRendererImpl &impl, const std::vector<GtObject> &objects, const D2D1_MATRIX_3X2_F &transform,
		   const GtFrameState &state, ShadowLists &lists)
{
	for (const GtObject &object : objects) {
		if (!object.visible) {
			continue;
		}
		DrawContext scratch;
		scratch.recordOnly = true;
		scratch.state = &state;
		const D2D1_MATRIX_3X2_F matrix = objectTransform(object, transform, scratch);
		if (findShadow(object)) {
			ComPtr<ID2D1CommandList> list;
			ComPtr<ID2D1Image> saved;
			if (SUCCEEDED(impl.context->CreateCommandList(&list))) {
				ShadowLists nested; /* a shadow drawn into a shadow's own recording would recurse */
				impl.context->GetTarget(&saved);
				impl.context->SetTarget(list.Get());
				impl.context->BeginDraw();
				impl.context->SetTransform(matrix);
				drawContent(impl, scratch, objects, object, matrix, nested);
				impl.context->EndDraw();
				list->Close();
				impl.context->SetTarget(saved.Get());
				lists[&object] = list;
			}
		}
		if (object.kind == "Layer") {
			recordShadows(impl, object.children, matrix, state, lists);
		}
	}
}

void drawShadow(GtRendererImpl &impl, DrawContext &ctx, const GtObject &object, const D2D1_MATRIX_3X2_F &matrix,
		ID2D1CommandList *list, const GtEffect &shadow)
{
	ComPtr<ID2D1Effect> effect;
	if (FAILED(impl.context->CreateEffect(CLSID_D2D1Shadow, &effect))) {
		addWarning(ctx, "unsupported", object.name + ": Shadow");
		return;
	}
	const float deviation = shadow.blur > 0 ? shadow.blur / 2.0f : 2.0f;
	effect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, deviation);
	effect->SetValue(D2D1_SHADOW_PROP_COLOR,
			 D2D1::Vector4F(((shadow.argb >> 16) & 0xFF) / 255.0f, ((shadow.argb >> 8) & 0xFF) / 255.0f,
					(shadow.argb & 0xFF) / 255.0f, ((shadow.argb >> 24) & 0xFF) / 255.0f));
	effect->SetInput(0, list);
	ComPtr<ID2D1Image> output;
	effect->GetOutput(&output);
	/* The list already holds world-space drawing, so move it by the offset in the object's own
	 * space: inverse(matrix) * Translation(offset) * matrix (row-vector order, left applied first). */
	D2D1_MATRIX_3X2_F moved = D2D1::Matrix3x2F::Translation(shadow.offsetX, shadow.offsetY);
	D2D1::Matrix3x2F inverse(matrix.m11, matrix.m12, matrix.m21, matrix.m22, matrix.dx, matrix.dy);
	if (inverse.Invert()) {
		moved = inverse * moved * matrix;
	}
	impl.context->SetTransform(moved);
	impl.context->DrawImage(output.Get());
	impl.context->SetTransform(matrix);
}

void drawObject(GtRendererImpl &impl, DrawContext &ctx, const std::vector<GtObject> &siblings, const GtObject &object,
		const D2D1_MATRIX_3X2_F &transform,
		const std::map<const GtObject *, ComPtr<ID2D1CommandList>> &lists)
{
	if (!object.visible) {
		return;
	}
	const D2D1_MATRIX_3X2_F matrix = objectTransform(object, transform, ctx);

	ComPtr<ID2D1Geometry> mask;
	if (!object.mask.empty()) {
		const GtObject *shape = findSibling(siblings, object.mask);
		if (shape) {
			mask = shapeGeometry(impl, *shape);
		}
	}
	const bool needLayer = object.opacity < 1.0f || mask;
	if (needLayer) {
		D2D1_LAYER_PARAMETERS1 parameters = {};
		parameters.contentBounds = D2D1::InfiniteRect();
		parameters.geometricMask = mask.Get();
		parameters.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
		parameters.maskTransform = D2D1::IdentityMatrix();
		parameters.opacity = object.opacity;
		parameters.opacityBrush = nullptr;
		parameters.layerOptions = D2D1_LAYER_OPTIONS1_NONE;
		impl.context->SetTransform(transform);
		impl.context->PushLayer(&parameters, nullptr);
	}
	impl.context->SetTransform(matrix);

	const auto recorded = lists.find(&object);
	if (recorded != lists.end()) {
		if (const GtEffect *shadow = findShadow(object)) {
			drawShadow(impl, ctx, object, matrix, recorded->second.Get(), *shadow);
		}
	}
	drawContent(impl, ctx, siblings, object, matrix, lists);

	if (needLayer) {
		impl.context->PopLayer();
	}
}

void drawComposition(GtRendererImpl &impl, DrawContext &ctx, const std::vector<GtObject> &objects,
		     const D2D1_MATRIX_3X2_F &transform, const ShadowLists &lists)
{
	for (const GtObject &object : objects) {
		drawObject(impl, ctx, objects, object, transform, lists);
	}
}

/* Referenced resource names, in the whole tree, that no decoded picture answers (design §7). */
void collectReferencedResources(const std::vector<GtObject> &objects, std::vector<std::string> &names)
{
	for (const GtObject &object : objects) {
		if (object.kind == "Image" && !object.bitmapSource.empty()) {
			if (std::find(names.begin(), names.end(), object.bitmapSource) == names.end()) {
				names.push_back(object.bitmapSource);
			}
		}
		if (!object.children.empty()) {
			collectReferencedResources(object.children, names);
		}
	}
}

void appendUnique(std::vector<GtWarning> &out, const std::vector<GtWarning> &warnings)
{
	for (const GtWarning &warning : warnings) {
		const bool seen = std::any_of(out.begin(), out.end(), [&](const GtWarning &existing) {
			return existing.kind == warning.kind && existing.detail == warning.detail;
		});
		if (!seen) {
			out.push_back(warning);
		}
	}
}

/* Both WIC decodes: a file on disk, or a package resource's bytes. Premultiplied BGRA. */
bool decodeWithWic(IWICImagingFactory *wic, IWICBitmapDecoder *decoder, std::vector<uint8_t> &bgraPremul, int &width,
		   int &height)
{
	ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(decoder->GetFrame(0, &frame))) {
		return false;
	}
	UINT w = 0, h = 0;
	if (FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0) {
		return false;
	}
	ComPtr<IWICFormatConverter> converter;
	if (FAILED(wic->CreateFormatConverter(&converter))) {
		return false;
	}
	if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
					 WICBitmapPaletteTypeCustom))) {
		return false;
	}
	width = (int)w;
	height = (int)h;
	bgraPremul.assign((size_t)w * h * 4, 0);
	return SUCCEEDED(converter->CopyPixels(nullptr, w * 4, (UINT)bgraPremul.size(), bgraPremul.data()));
}

IWICImagingFactory *createWic()
{
	IWICImagingFactory *wic = nullptr;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))) {
		return nullptr;
	}
	return wic;
}

std::wstring toWide(const std::string &utf8)
{
	if (utf8.empty()) {
		return std::wstring();
	}
	const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
	std::wstring out((size_t)n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), out.data(), n);
	return out;
}

} // namespace

GtRenderer::GtRenderer() : impl_(new GtRendererImpl()) {}

GtRenderer::~GtRenderer()
{
	delete impl_;
}

bool GtRenderer::init(ID3D11Device *device, std::string &error)
{
	impl_->device = device;
	if (!impl_->device) {
		error = "no D3D11 device";
		return false;
	}

	D2D1_FACTORY_OPTIONS options = {};
	if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options,
				     (void **)&impl_->d2dFactory))) {
		error = "could not create the Direct2D factory";
		return false;
	}
	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
				       (IUnknown **)&impl_->dwrite))) {
		error = "could not create the DirectWrite factory";
		return false;
	}

	ComPtr<IDXGIDevice> dxgiDevice;
	if (FAILED(impl_->device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) ||
	    FAILED(impl_->d2dFactory->CreateDevice(dxgiDevice.Get(), &impl_->d2dDevice)) ||
	    FAILED(impl_->d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &impl_->context))) {
		error = "could not create the Direct2D device context";
		return false;
	}
	impl_->context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
	impl_->context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	return true;
}

void GtRenderer::load(const GtPackage &package, const GtDocument &document)
{
	impl_->document = document;
	impl_->resources.clear();
	impl_->pictures.clear();
	impl_->bitmaps.clear();
	impl_->loadWarnings.clear();
	impl_->drawWarnings.clear();

	IWICImagingFactory *wic = createWic();
	for (const auto &resource : package.resources) {
		if (!wic || resource.second.empty()) {
			continue;
		}
		ComPtr<IWICStream> stream;
		ComPtr<IWICBitmapDecoder> decoder;
		if (FAILED(wic->CreateStream(&stream)) ||
		    FAILED(stream->InitializeFromMemory(const_cast<BYTE *>(resource.second.data()),
							(UINT)resource.second.size())) ||
		    FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) {
			continue;
		}
		GtRendererImpl::Picture picture;
		if (decodeWithWic(wic, decoder.Get(), picture.pixels, picture.width, picture.height)) {
			impl_->resources[resource.first] = std::move(picture);
		}
	}
	if (wic) {
		wic->Release();
	}

	std::vector<std::string> referenced;
	collectReferencedResources(document.objects, referenced);
	std::vector<std::string> missing;
	for (const std::string &name : referenced) {
		if (impl_->resources.find(name) == impl_->resources.end()) {
			missing.push_back(name);
		}
	}
	if (!missing.empty()) {
		std::string detail;
		for (const std::string &name : missing) {
			if (!detail.empty()) {
				detail += ", ";
			}
			detail += name;
		}
		impl_->loadWarnings.push_back({"missing-resources", detail});
	}
	if (document.hasStoryboards) {
		impl_->loadWarnings.push_back({"animations", "the title has animations; the final state is drawn"});
	}
}

void GtRenderer::setPicture(const std::string &path, std::vector<uint8_t> bgraPremul, int w, int h)
{
	GtRendererImpl::Picture picture;
	picture.pixels = std::move(bgraPremul);
	picture.width = w;
	picture.height = h;
	impl_->pictures[path] = std::move(picture);
	impl_->bitmaps.erase(path);
}

bool GtRenderer::decodePicture(const std::string &path, std::vector<uint8_t> &bgraPremul, int &w, int &h)
{
	const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	IWICImagingFactory *wic = createWic();
	bool ok = false;
	if (wic) {
		ComPtr<IWICBitmapDecoder> decoder;
		const std::wstring wide = toWide(path);
		if (SUCCEEDED(wic->CreateDecoderFromFilename(wide.c_str(), nullptr, GENERIC_READ,
							     WICDecodeMetadataCacheOnLoad, &decoder))) {
			ok = decodeWithWic(wic, decoder.Get(), bgraPremul, w, h);
		}
		wic->Release();
	}
	if (SUCCEEDED(initialized)) {
		CoUninitialize();
	}
	return ok;
}

bool GtRenderer::draw(ID3D11Texture2D *target, const GtFrameState &state)
{
	if (!impl_->context || !target || !impl_->device) {
		return false;
	}

	if (impl_->targetTexture != target || !impl_->targetBitmap) {
		ComPtr<IDXGISurface> surface;
		if (FAILED(target->QueryInterface(IID_PPV_ARGS(&surface)))) {
			return false;
		}
		const D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
		impl_->targetBitmap.Reset();
		if (FAILED(impl_->context->CreateBitmapFromDxgiSurface(surface.Get(), &properties, &impl_->targetBitmap))) {
			impl_->targetTexture = nullptr;
			return false;
		}
		impl_->targetTexture = target;
	}

	impl_->context->SetTarget(impl_->targetBitmap.Get());
	impl_->context->SetTransform(D2D1::IdentityMatrix());

	DrawContext ctx;
	ctx.state = &state;
	/* Shadow command lists are recorded before BeginDraw; the device context cannot record to a
	 * target while a BeginDraw/EndDraw pair is open. */
	ShadowLists lists;
	recordShadows(*impl_, impl_->document.objects, D2D1::IdentityMatrix(), state, lists);

	impl_->context->BeginDraw();
	impl_->context->Clear(D2D1::ColorF(0, 0));
	drawComposition(*impl_, ctx, impl_->document.objects, D2D1::IdentityMatrix(), lists);

	for (const auto &family : ctx.fontFamilies) {
		std::string fields;
		std::vector<std::string> unique = family.second;
		std::sort(unique.begin(), unique.end());
		unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
		for (const std::string &field : unique) {
			if (!fields.empty()) {
				fields += ", ";
			}
			fields += field;
		}
		ctx.warnings.push_back({"font", family.first + ": " + fields});
	}
	impl_->drawWarnings = ctx.warnings;

	const HRESULT hr = impl_->context->EndDraw();
	impl_->context->SetTarget(nullptr);
	return SUCCEEDED(hr);
}

std::vector<GtWarning> GtRenderer::warnings() const
{
	std::vector<GtWarning> all = impl_->loadWarnings;
	appendUnique(all, impl_->drawWarnings);
	return all;
}

} // namespace gt
