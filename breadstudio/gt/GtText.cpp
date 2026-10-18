#include "GtDocument.hpp"

#include <windows.h>

#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace gt {

namespace {

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

DWRITE_FONT_STRETCH toStretch(const std::string &name)
{
	if (name == "UltraCondensed") {
		return DWRITE_FONT_STRETCH_ULTRA_CONDENSED;
	}
	if (name == "ExtraCondensed") {
		return DWRITE_FONT_STRETCH_EXTRA_CONDENSED;
	}
	if (name == "Condensed") {
		return DWRITE_FONT_STRETCH_CONDENSED;
	}
	if (name == "SemiCondensed") {
		return DWRITE_FONT_STRETCH_SEMI_CONDENSED;
	}
	if (name == "SemiExpanded") {
		return DWRITE_FONT_STRETCH_SEMI_EXPANDED;
	}
	if (name == "Expanded") {
		return DWRITE_FONT_STRETCH_EXPANDED;
	}
	if (name == "ExtraExpanded") {
		return DWRITE_FONT_STRETCH_EXTRA_EXPANDED;
	}
	if (name == "UltraExpanded") {
		return DWRITE_FONT_STRETCH_ULTRA_EXPANDED;
	}
	return DWRITE_FONT_STRETCH_NORMAL;
}

DWRITE_TEXT_ALIGNMENT toAlignment(const std::string &name)
{
	if (name == "Center") {
		return DWRITE_TEXT_ALIGNMENT_CENTER;
	}
	if (name == "Right") {
		return DWRITE_TEXT_ALIGNMENT_TRAILING;
	}
	return DWRITE_TEXT_ALIGNMENT_LEADING;
}

/* Glyph runs become outlines (GetGlyphRunOutline), filled and stroked with the object's brushes (§3.2). */
class GlyphOutlineRenderer : public IDWriteTextRenderer {
public:
	GlyphOutlineRenderer(ID2D1RenderTarget *target, ID2D1Factory *factory, ID2D1Brush *fill, ID2D1Brush *stroke,
			     float strokeWidth)
		: target_(target), factory_(factory), fill_(fill), stroke_(stroke), strokeWidth_(strokeWidth)
	{
	}

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
		const ULONG count = InterlockedDecrement(&refs_);
		if (count == 0) {
			delete this;
		}
		return count;
	}

	HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void *, BOOL *disabled) override
	{
		*disabled = TRUE;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetCurrentTransform(void *, DWRITE_MATRIX *transform) override
	{
		D2D1_MATRIX_3X2_F current = D2D1::IdentityMatrix();
		target_->GetTransform(&current);
		transform->m11 = current.m11;
		transform->m12 = current.m12;
		transform->m21 = current.m21;
		transform->m22 = current.m22;
		transform->dx = current.dx;
		transform->dy = current.dy;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void *, FLOAT *pixelsPerDip) override
	{
		*pixelsPerDip = 1.0f;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE DrawGlyphRun(void *, FLOAT baselineOriginX, FLOAT baselineOriginY,
					       DWRITE_MEASURING_MODE, const DWRITE_GLYPH_RUN *run,
					       const DWRITE_GLYPH_RUN_DESCRIPTION *, IUnknown *) override
	{
		if (!run || run->glyphCount == 0 || !run->fontFace) {
			return S_OK;
		}
		ComPtr<ID2D1PathGeometry> path;
		ComPtr<ID2D1GeometrySink> sink;
		if (FAILED(factory_->CreatePathGeometry(&path)) || FAILED(path->Open(&sink))) {
			return S_OK;
		}
		if (FAILED(run->fontFace->GetGlyphRunOutline(run->fontEmSize, run->glyphIndices, run->glyphAdvances,
							     run->glyphOffsets, run->glyphCount, run->isSideways,
							     (run->bidiLevel & 1) != 0, sink.Get())) ||
		    FAILED(sink->Close())) {
			return S_OK;
		}
		D2D1_MATRIX_3X2_F saved = D2D1::IdentityMatrix();
		target_->GetTransform(&saved);
		target_->SetTransform(saved * D2D1::Matrix3x2F::Translation(baselineOriginX, baselineOriginY));
		if (fill_) {
			target_->FillGeometry(path.Get(), fill_);
		}
		if (stroke_ && strokeWidth_ > 0) {
			target_->DrawGeometry(path.Get(), stroke_, strokeWidth_);
		}
		target_->SetTransform(saved);
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
	ID2D1RenderTarget *target_;
	ID2D1Factory *factory_;
	ID2D1Brush *fill_;
	ID2D1Brush *stroke_;
	float strokeWidth_;
	LONG refs_ = 1;
};

} // namespace

bool gtDrawText(ID2D1DeviceContext *context, IDWriteFactory *dwrite, ID2D1Factory *d2dFactory, const GtObject &object,
		const GtStyle *style, const std::string &text, ID2D1Brush *fill, ID2D1Brush *stroke,
		std::string &missingFamily)
{
	if (!context || !dwrite || !d2dFactory) {
		return true;
	}
	missingFamily.clear();

	const std::string family = style && style->font ? *style->font : object.fontFamily;
	const float size = style && style->size ? *style->size : object.fontSize;
	const int weight = style && style->weight ? *style->weight : object.fontWeight;
	const bool italic = style && style->italic ? *style->italic : object.italic;
	const std::string stretchName = style && style->stretch ? *style->stretch : object.fontStretch;
	const float lineSpacing = style && style->lineSpacing ? *style->lineSpacing : object.lineSpacing;

	/* A family the system collection does not have falls back; say so, and draw anyway (§7). */
	ComPtr<IDWriteFontCollection> collection;
	UINT32 familyIndex = 0;
	BOOL found = FALSE;
	const std::wstring wideFamily = toWide(family);
	if (SUCCEEDED(dwrite->GetSystemFontCollection(&collection)) &&
	    SUCCEEDED(collection->FindFamilyName(wideFamily.c_str(), &familyIndex, &found)) && !found) {
		missingFamily = family;
	}

	const DWRITE_FONT_WEIGHT dwriteWeight = (DWRITE_FONT_WEIGHT)std::clamp(weight, 100, 950);
	ComPtr<IDWriteTextFormat> format;
	if (FAILED(dwrite->CreateTextFormat(wideFamily.c_str(), nullptr, dwriteWeight,
					    italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
					    toStretch(stretchName), size, L"", &format))) {
		return missingFamily.empty();
	}
	format->SetTextAlignment(toAlignment(object.textAlign));
	format->SetParagraphAlignment(object.verticalAlign == "Center" ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER
								      : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	format->SetWordWrapping(object.singleLine ? DWRITE_WORD_WRAPPING_NO_WRAP : DWRITE_WORD_WRAPPING_WRAP);
	if (lineSpacing > 0) {
		format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineSpacing, lineSpacing);
	}

	std::wstring wideText = toWide(text);
	if (object.uppercase && !wideText.empty()) {
		std::wstring upper(wideText.size(), L'\0');
		const int n = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, wideText.c_str(), (int)wideText.size(),
					    upper.data(), (int)upper.size(), nullptr, nullptr, 0);
		if (n > 0) {
			upper.resize((size_t)n);
			wideText = std::move(upper);
		}
	}

	const float maxWidth = object.width > 0 ? object.width : 1.0f;
	const float maxHeight = object.height > 0 ? object.height : 1.0f;
	ComPtr<IDWriteTextLayout> layout;
	if (FAILED(dwrite->CreateTextLayout(wideText.c_str(), (UINT32)wideText.size(), format.Get(), maxWidth, maxHeight,
					    &layout))) {
		return missingFamily.empty();
	}
	GlyphOutlineRenderer renderer(context, d2dFactory, fill, stroke, object.strokeThickness);
	layout->Draw(nullptr, &renderer, 0, 0);
	return missingFamily.empty();
}

} // namespace gt
