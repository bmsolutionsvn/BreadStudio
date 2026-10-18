/* The bread_gt_title source (plan Task 7, design §4, §6, §7, §8).

   It draws a .gtzip on its own D3D11 device, on the same adapter OBS uses, into a keyed-mutex shared
   texture that OBS opens and composites: Direct2D never runs on OBS's device, and no frame is copied
   through the CPU. The source redraws only when a value, a clock, a countdown or a picture changes.

   Threads: update/settings work and picture decoding run on the calling (UI) thread, never drawing or
   touching the Direct2D renderer; the renderer, the device and the texture are used only on the
   graphics thread (video_render and the graphics-entered sections of create/update/destroy). Decoded
   picture pixels are handed to the renderer through a queue the graphics thread drains, so the
   renderer's D2D objects keep their single-thread affinity (design §8, plan Review Focus 4). */

#include <obs-module.h>

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <util/platform.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "GtClock.hpp"
#include "GtDocument.hpp"
#include "GtFields.hpp"
#include "GtPackage.hpp"
#include "GtRenderer.hpp"
#include "GtSettings.hpp"

using Microsoft::WRL::ComPtr;
using namespace gt;

namespace gt {
obs_source_info gtTitleSourceInfo();
}

namespace {

/* A picture file's pixels, decoded off the graphics thread and queued for the renderer. */
struct DecodedPicture {
	std::string path;
	std::vector<uint8_t> pixels;
	int width = 0;
	int height = 0;
};

struct gt_source {
	obs_source_t *source = nullptr;

	/* Settings, the document and everything the desk and the render tick read; never held across a
	 * graphics-entered call, so the only lock order is graphics -> mutex. */
	std::mutex mutex;
	std::string file;
	std::map<std::string, std::string> values;
	std::map<std::string, GtStyle> styles;
	std::map<std::string, GtCountdown> countdowns;
	std::map<std::string, GtCountdownSettings> appliedCountdowns; /* replaced only when its settings change */
	GtDocument document;
	std::set<std::string> resources; /* the package's resource filenames */
	bool loaded = false;
	bool everLoaded = false;
	std::string fileError;
	uint64_t loadedStamp = 0; /* the .gtzip's modified time as loaded */

	std::map<std::string, std::string> fieldSources; /* image field -> effective source string */
	std::set<std::string> decodedPaths;
	std::set<std::string> failedPaths;
	std::vector<GtWarning> baseWarnings;

	/* Handed to the renderer by the graphics thread. */
	bool pendingLoad = false;
	bool pendingReadable = false;
	GtPackage pendingPackage;
	GtDocument pendingDocument;
	std::vector<DecodedPicture> pendingPictures;

	std::atomic<bool> dirty{true};

	/* Graphics resources; used only while graphics is entered. */
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<ID3D11Texture2D> texture;
	ComPtr<IDXGIKeyedMutex> textureMutex;
	HANDLE sharedHandle = nullptr;
	gs_texture_t *obsTexture = nullptr;
	std::unique_ptr<GtRenderer> renderer;
	uint32_t width = 0;
	uint32_t height = 0;

	/* The last state drawn; the graphics thread only. */
	GtFrameState lastDrawn;
	bool lastValid = false;

	float tickWait = 0.0f;
};

void gtReloadTask(void *param);

const char *fieldTypeName(GtFieldType type)
{
	switch (type) {
	case GtFieldType::Image:
		return "image";
	case GtFieldType::Colour:
		return "colour";
	default:
		return "text";
	}
}

bool sameStyle(const GtStyle &a, const GtStyle &b)
{
	return a.font == b.font && a.size == b.size && a.fill == b.fill && a.weight == b.weight &&
	       a.italic == b.italic && a.stretch == b.stretch && a.lineSpacing == b.lineSpacing;
}

bool sameFrameState(const GtFrameState &a, const GtFrameState &b)
{
	if (a.text != b.text || a.picture != b.picture || a.colour != b.colour) {
		return false;
	}
	if (a.style.size() != b.style.size()) {
		return false;
	}
	auto left = a.style.begin();
	auto right = b.style.begin();
	for (; left != a.style.end(); ++left, ++right) {
		if (left->first != right->first || !sameStyle(left->second, right->second)) {
			return false;
		}
	}
	return true;
}

bool sameCountdownSettings(const GtCountdownSettings &a, const GtCountdownSettings &b)
{
	return a.start == b.start && a.duration == b.duration && a.format == b.format && a.reverse == b.reverse &&
	       a.reverseDisplay == b.reverseDisplay;
}

void appendWarning(std::vector<GtWarning> &list, const GtWarning &warning)
{
	for (const GtWarning &seen : list) {
		if (seen.kind == warning.kind && seen.detail == warning.detail) {
			return;
		}
	}
	list.push_back(warning);
}

/* One custom object's value or its default; a field's state is its default overridden by `values`. */
std::string fieldValue(gt_source *s, const GtField &field)
{
	const auto value = s->values.find(field.name);
	return value != s->values.end() ? value->second : field.defaultValue;
}

GtFrameState baseStateLocked(gt_source *s)
{
	GtFrameState state;
	for (const GtField &field : gtFields(s->document)) {
		const std::string value = fieldValue(s, field);
		if (field.type == GtFieldType::Text) {
			state.text[field.name] = value;
		} else if (field.type == GtFieldType::Image) {
			state.picture[field.name] = value;
		} else if (field.type == GtFieldType::Colour) {
			uint32_t argb = 0;
			if (gtParseColour(value, argb)) {
				state.colour[field.name] = argb;
			}
		}
		const auto style = s->styles.find(field.name);
		if (style != s->styles.end()) {
			state.style[field.name] = style->second;
		}
	}
	return state;
}

GtFrameState expandedStateLocked(gt_source *s)
{
	GtFrameState state = baseStateLocked(s);
	const GtClockTime now = gtNow();
	for (auto &entry : state.text) {
		entry.second = gtExpandClock(entry.second, now).text;
	}
	const int64_t nowMs = (int64_t)(os_gettime_ns() / 1000000);
	for (auto &entry : s->countdowns) {
		if (entry.second.active()) {
			state.text[entry.first] = entry.second.text(nowMs);
		}
	}
	return state;
}

std::map<std::string, std::string> fieldSourcesLocked(gt_source *s)
{
	std::map<std::string, std::string> sources;
	for (const GtField &field : gtFields(s->document)) {
		if (field.type == GtFieldType::Image) {
			sources[field.name] = fieldValue(s, field);
		}
	}
	return sources;
}

void rebuildWarningsLocked(gt_source *s)
{
	s->baseWarnings.clear();
	if (s->file.empty()) {
		s->baseWarnings.push_back({"file", "no title file"});
	} else if (!s->loaded) {
		s->baseWarnings.push_back({"file", s->file + ": " + s->fileError});
	}
	if (s->loaded && s->document.hasStoryboards) {
		s->baseWarnings.push_back({"animations", "the title has animations; the final state is drawn"});
	}
	for (const auto &entry : s->fieldSources) {
		const std::string &source = entry.second;
		if (source.empty() || s->resources.count(source)) {
			continue;
		}
		if (s->failedPaths.count(source)) {
			s->baseWarnings.push_back({"picture", entry.first + ": " + source});
		}
	}
}

void readSettingsLocked(gt_source *s, obs_data_t *settings)
{
	s->values.clear();
	obs_data_t *values = obs_data_get_obj(settings, gt::KeyValues);
	if (values) {
		for (obs_data_item_t *item = obs_data_first(values); item; obs_data_item_next(&item)) {
			const char *name = obs_data_item_get_name(item);
			if (!name) {
				continue;
			}
			if (obs_data_item_gettype(item) == OBS_DATA_STRING) {
				s->values[name] = obs_data_item_get_string(item);
			}
		}
	}

	s->styles.clear();
	obs_data_t *styles = obs_data_get_obj(settings, gt::KeyStyles);
	if (styles) {
		for (obs_data_item_t *item = obs_data_first(styles); item; obs_data_item_next(&item)) {
			const char *name = obs_data_item_get_name(item);
			obs_data_t *style = obs_data_item_get_obj(item);
			if (!name || !style) {
				continue;
			}
			GtStyle parsed;
			if (obs_data_has_user_value(style, "font")) {
				parsed.font = obs_data_get_string(style, "font");
			}
			if (obs_data_has_user_value(style, "size")) {
				parsed.size = (float)obs_data_get_double(style, "size");
			}
			if (obs_data_has_user_value(style, "fill")) {
				uint32_t argb = 0;
				if (gtParseColour(obs_data_get_string(style, "fill"), argb)) {
					parsed.fill = argb;
				}
			}
			if (obs_data_has_user_value(style, "weight")) {
				parsed.weight = gtFontWeight(obs_data_get_string(style, "weight"));
			}
			if (obs_data_has_user_value(style, "italic")) {
				parsed.italic = obs_data_get_bool(style, "italic");
			}
			if (obs_data_has_user_value(style, "stretch")) {
				parsed.stretch = obs_data_get_string(style, "stretch");
			}
			if (obs_data_has_user_value(style, "line_spacing")) {
				parsed.lineSpacing = (float)obs_data_get_double(style, "line_spacing");
			}
			s->styles[name] = parsed;
		}
	}

	std::map<std::string, GtCountdownSettings> desired;
	obs_data_t *countdowns = obs_data_get_obj(settings, gt::KeyCountdowns);
	if (countdowns) {
		for (obs_data_item_t *item = obs_data_first(countdowns); item; obs_data_item_next(&item)) {
			const char *name = obs_data_item_get_name(item);
			obs_data_t *settingsObject = obs_data_item_get_obj(item);
			if (!name || !settingsObject) {
				continue;
			}
			GtCountdownSettings parsed;
			if (obs_data_has_user_value(settingsObject, "start")) {
				parsed.start = obs_data_get_string(settingsObject, "start");
			}
			if (obs_data_has_user_value(settingsObject, "duration")) {
				parsed.duration = obs_data_get_string(settingsObject, "duration");
			}
			if (obs_data_has_user_value(settingsObject, "format")) {
				parsed.format = obs_data_get_string(settingsObject, "format");
			}
			if (obs_data_has_user_value(settingsObject, "reverse")) {
				parsed.reverse = obs_data_get_bool(settingsObject, "reverse");
			}
			if (obs_data_has_user_value(settingsObject, "reverse_display")) {
				parsed.reverseDisplay = obs_data_get_bool(settingsObject, "reverse_display");
			}
			desired[name] = parsed;
		}
	}

	for (auto it = s->countdowns.begin(); it != s->countdowns.end();) {
		if (!desired.count(it->first)) {
			it = s->countdowns.erase(it);
		} else {
			++it;
		}
	}
	for (auto it = s->appliedCountdowns.begin(); it != s->appliedCountdowns.end();) {
		if (!desired.count(it->first)) {
			it = s->appliedCountdowns.erase(it);
		} else {
			++it;
		}
	}
	for (const auto &entry : desired) {
		const auto applied = s->appliedCountdowns.find(entry.first);
		if (applied == s->appliedCountdowns.end() || !sameCountdownSettings(applied->second, entry.second)) {
			s->countdowns.insert_or_assign(entry.first, GtCountdown(entry.second));
			s->appliedCountdowns[entry.first] = entry.second;
		}
	}

	s->dirty = true;
}

uint64_t fileModified(const std::string &path)
{
	wchar_t *wide = nullptr;
	if (os_utf8_to_wcs_ptr(path.c_str(), 0, &wide) == 0 || !wide) {
		return 0;
	}
	WIN32_FILE_ATTRIBUTE_DATA data;
	uint64_t stamp = 0;
	if (GetFileAttributesExW(wide, GetFileExInfoStandard, &data)) {
		ULARGE_INTEGER value;
		value.LowPart = data.ftLastWriteTime.dwLowDateTime;
		value.HighPart = data.ftLastWriteTime.dwHighDateTime;
		stamp = value.QuadPart;
	}
	bfree(wide);
	return stamp;
}

void releaseTexture(gt_source *s)
{
	if (s->obsTexture) {
		gs_texture_destroy(s->obsTexture);
		s->obsTexture = nullptr;
	}
	s->textureMutex.Reset();
	s->texture.Reset();
	if (s->sharedHandle) {
		CloseHandle(s->sharedHandle);
		s->sharedHandle = nullptr;
	}
	s->width = 0;
	s->height = 0;
}

void createTexture(gt_source *s, uint32_t width, uint32_t height)
{
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

	if (FAILED(s->device->CreateTexture2D(&desc, nullptr, &s->texture))) {
		blog(LOG_WARNING, "[bread-gt] could not create the title texture");
		return;
	}
	if (FAILED(s->texture.As(&s->textureMutex))) {
		blog(LOG_WARNING, "[bread-gt] the title texture has no keyed mutex");
		s->texture.Reset();
		return;
	}
	ComPtr<IDXGIResource1> resource;
	if (FAILED(s->texture.As(&resource))) {
		s->textureMutex.Reset();
		s->texture.Reset();
		return;
	}
	HANDLE handle = nullptr;
	if (FAILED(resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
						nullptr, &handle))) {
		s->textureMutex.Reset();
		s->texture.Reset();
		return;
	}
	s->obsTexture = gs_texture_open_nt_shared((uint32_t)(uintptr_t)handle);
	if (!s->obsTexture) {
		CloseHandle(handle);
		s->textureMutex.Reset();
		s->texture.Reset();
		blog(LOG_WARNING, "[bread-gt] OBS could not open the shared title texture");
		return;
	}
	s->sharedHandle = handle;
	s->width = width;
	s->height = height;
	s->dirty = true;
}

/* The device and the shared texture; any thread while graphics is entered. */
void ensureDeviceTexture(gt_source *s)
{
	if (!s->device) {
		void *object = gs_get_device_obj();
		if (object) {
			ComPtr<ID3D11Device> obsDevice = static_cast<ID3D11Device *>(object);
			ComPtr<IDXGIDevice> dxgi;
			ComPtr<IDXGIAdapter> adapter;
			if (SUCCEEDED(obsDevice->QueryInterface(IID_PPV_ARGS(&dxgi))) &&
			    SUCCEEDED(dxgi->GetAdapter(&adapter))) {
				const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
								    D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
				D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;
				const HRESULT hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
								     D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
								     (UINT)ARRAYSIZE(levels), D3D11_SDK_VERSION, &s->device,
								     &got, &s->context);
				if (FAILED(hr)) {
					blog(LOG_WARNING, "[bread-gt] could not create the title device (0x%08lX)",
					     (unsigned long)hr);
				}
			}
		}
	}

	uint32_t width = 0;
	uint32_t height = 0;
	{
		std::lock_guard<std::mutex> lock(s->mutex);
		if (s->loaded) {
			width = (uint32_t)(s->document.width + 0.5f);
			height = (uint32_t)(s->document.height + 0.5f);
		}
	}

	if (width == 0 || height == 0) {
		releaseTexture(s);
		return;
	}
	if (s->obsTexture && width == s->width && height == s->height) {
		return;
	}
	releaseTexture(s);
	createTexture(s, width, height);
}

/* The renderer's Direct2D factory is single-threaded, so it is built and used on the graphics thread
 * only (plan Review Focus 4). */
void ensureRenderer(gt_source *s)
{
	if (s->renderer || !s->device) {
		return;
	}
	auto renderer = std::make_unique<GtRenderer>();
	std::string error;
	if (renderer->init(s->device.Get(), error)) {
		s->renderer = std::move(renderer);
	} else {
		blog(LOG_WARNING, "[bread-gt] could not start the title renderer: %s", error.c_str());
	}
}

/* Apply the document, package and decoded pictures the UI thread left for the graphics thread. */
void applyPending(gt_source *s)
{
	ensureRenderer(s);
	if (!s->renderer) {
		return;
	}

	GtPackage package;
	GtDocument document;
	std::vector<DecodedPicture> pictures;
	bool load = false;
	bool readable = false;
	{
		std::lock_guard<std::mutex> lock(s->mutex);
		load = s->pendingLoad;
		readable = s->pendingReadable;
		if (load) {
			package = std::move(s->pendingPackage);
			document = std::move(s->pendingDocument);
			s->pendingPackage = GtPackage();
			s->pendingDocument = GtDocument();
			s->pendingLoad = false;
		}
		pictures.swap(s->pendingPictures);
	}

	if (load) {
		if (readable) {
			s->renderer->load(package, document);
		} else {
			GtPackage empty;
			GtDocument emptyDocument;
			s->renderer->load(empty, emptyDocument);
		}
	}
	for (DecodedPicture &picture : pictures) {
		s->renderer->setPicture(picture.path, std::move(picture.pixels), picture.width, picture.height);
	}
}

/* Read the .gtzip and its document, then schedule them for the graphics thread, keeping values. */
void applyFile(gt_source *s, const std::string &path)
{
	GtPackage package;
	GtDocument document;
	std::set<std::string> resources;
	std::string error;
	bool readable = false;
	uint64_t stamp = 0;

	if (!path.empty()) {
		if (readGtFile(path, package, error)) {
			if (parseGtDocument(package.documentXml, document, error)) {
				readable = true;
				stamp = fileModified(path);
				for (const auto &entry : package.resources) {
					resources.insert(entry.first);
				}
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock(s->mutex);
		s->file = path;
		s->everLoaded = true;
		s->fileError = readable ? std::string() : error;
		s->loaded = readable;
		if (readable) {
			s->document = document;
			s->resources = std::move(resources);
			s->loadedStamp = stamp;
		} else {
			s->document = GtDocument();
			s->resources.clear();
			s->loadedStamp = 0;
		}
		/* A reload clears the renderer's pictures, so they are decoded and handed over again. */
		s->decodedPaths.clear();
		s->failedPaths.clear();
		s->pendingPackage = std::move(package);
		s->pendingDocument = s->document;
		s->pendingReadable = readable;
		s->pendingLoad = true;
	}
	s->dirty = true;

	obs_enter_graphics();
	ensureDeviceTexture(s);
	obs_leave_graphics();
}

/* Decode the changed picture files on the calling thread, never the graphics thread (Review Focus 4). */
void decodePictures(gt_source *s)
{
	std::vector<std::string> toDecode;
	std::set<std::string> needed;
	{
		std::lock_guard<std::mutex> lock(s->mutex);
		s->fieldSources = fieldSourcesLocked(s);
		for (const auto &entry : s->fieldSources) {
			const std::string &source = entry.second;
			if (source.empty() || s->resources.count(source)) {
				continue;
			}
			needed.insert(source);
			if (!s->decodedPaths.count(source) && !s->failedPaths.count(source)) {
				toDecode.push_back(source);
			}
		}
		for (auto it = s->decodedPaths.begin(); it != s->decodedPaths.end();) {
			if (needed.count(*it)) {
				++it;
			} else {
				it = s->decodedPaths.erase(it);
			}
		}
		for (auto it = s->failedPaths.begin(); it != s->failedPaths.end();) {
			if (needed.count(*it)) {
				++it;
			} else {
				it = s->failedPaths.erase(it);
			}
		}
	}

	std::vector<DecodedPicture> decoded;
	for (const std::string &path : toDecode) {
		DecodedPicture picture;
		picture.path = path;
		if (GtRenderer::decodePicture(path, picture.pixels, picture.width, picture.height)) {
			decoded.push_back(std::move(picture));
		} else {
			std::lock_guard<std::mutex> lock(s->mutex);
			s->failedPaths.insert(path);
		}
	}

	if (!decoded.empty()) {
		std::lock_guard<std::mutex> lock(s->mutex);
		for (DecodedPicture &picture : decoded) {
			s->decodedPaths.insert(picture.path);
			s->pendingPictures.push_back(std::move(picture));
		}
		s->dirty = true;
	}

	std::lock_guard<std::mutex> lock(s->mutex);
	rebuildWarningsLocked(s);
}

void gtUpdate(void *data, obs_data_t *settings)
{
	auto *s = static_cast<gt_source *>(data);
	if (!settings) {
		return;
	}

	const std::string newFile = obs_data_get_string(settings, gt::KeyFile);
	bool reload = false;
	{
		std::lock_guard<std::mutex> lock(s->mutex);
		readSettingsLocked(s, settings);
		reload = !s->everLoaded || newFile != s->file;
	}
	if (reload) {
		applyFile(s, newFile);
	}
	decodePictures(s);
}

void gtVideoTick(void *data, float seconds)
{
	auto *s = static_cast<gt_source *>(data);
	s->tickWait += seconds;
	if (s->tickWait < 1.0f) {
		return;
	}
	s->tickWait = 0.0f;

	std::string file;
	uint64_t stamp = 0;
	{
		std::lock_guard<std::mutex> lock(s->mutex);
		file = s->file;
		stamp = s->loadedStamp;
	}
	if (file.empty()) {
		return;
	}
	const uint64_t current = fileModified(file);
	if (current != 0 && current != stamp) {
		obs_queue_task(OBS_TASK_UI, gtReloadTask, obs_source_get_weak_source(s->source), false);
	}
}

void gtVideoRender(void *data, gs_effect_t *)
{
	auto *s = static_cast<gt_source *>(data);

	ensureDeviceTexture(s);
	applyPending(s);

	if (!s->obsTexture || !s->renderer) {
		return;
	}

	GtFrameState state;
	bool redraw = false;
	{
		std::lock_guard<std::mutex> lock(s->mutex);
		state = expandedStateLocked(s);
		redraw = s->dirty || !s->lastValid || !sameFrameState(state, s->lastDrawn);
	}

	if (redraw) {
		if (s->textureMutex && SUCCEEDED(s->textureMutex->AcquireSync(0, INFINITE))) {
			s->renderer->draw(s->texture.Get(), state);
			s->textureMutex->ReleaseSync(0);
		}
		s->lastDrawn = state;
		s->lastValid = true;
		s->dirty = false;
	}

	/* The texture is premultiplied, as OBS's own image path draws one (plan Task 7 Step 3). */
	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, s->obsTexture);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	if (gs_texture_acquire_sync(s->obsTexture, 0, INFINITE) == 0) {
		while (gs_effect_loop(effect, "Draw")) {
			gs_draw_sprite(s->obsTexture, 0, s->width, s->height);
		}
		gs_texture_release_sync(s->obsTexture, 0);
	}
	gs_blend_state_pop();
}

uint32_t gtGetWidth(void *data)
{
	auto *s = static_cast<gt_source *>(data);
	std::lock_guard<std::mutex> lock(s->mutex);
	return s->loaded ? (uint32_t)(s->document.width + 0.5f) : 0;
}

uint32_t gtGetHeight(void *data)
{
	auto *s = static_cast<gt_source *>(data);
	std::lock_guard<std::mutex> lock(s->mutex);
	return s->loaded ? (uint32_t)(s->document.height + 0.5f) : 0;
}

const char *gtGetName(void *)
{
	return obs_module_text("GtTitle");
}

void gtInfoProc(void *data, calldata_t *calldata)
{
	auto *s = static_cast<gt_source *>(data);

	obs_data_t *root = obs_data_create();
	obs_data_array_t *fields = obs_data_array_create();
	{
		std::lock_guard<std::mutex> lock(s->mutex);
		for (const GtField &field : gtFields(s->document)) {
			obs_data_t *item = obs_data_create();
			obs_data_set_string(item, "name", field.name.c_str());
			obs_data_set_string(item, "type", fieldTypeName(field.type));
			obs_data_set_string(item, "default", field.defaultValue.c_str());
			const auto countdown = s->countdowns.find(field.name);
			const bool counting = countdown != s->countdowns.end() && countdown->second.active();
			obs_data_set_bool(item, "countdown", counting);
			obs_data_array_push_back(fields, item);
			obs_data_release(item);
		}
	}
	obs_data_set_array(root, "fields", fields);
	obs_data_array_release(fields);

	std::vector<GtWarning> warnings;
	{
		std::lock_guard<std::mutex> lock(s->mutex);
		warnings = s->baseWarnings;
	}
	obs_enter_graphics();
	if (s->renderer) {
		for (const GtWarning &warning : s->renderer->warnings()) {
			appendWarning(warnings, warning);
		}
	}
	obs_leave_graphics();

	obs_data_array_t *array = obs_data_array_create();
	for (const GtWarning &warning : warnings) {
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "kind", warning.kind.c_str());
		obs_data_set_string(item, "detail", warning.detail.c_str());
		obs_data_array_push_back(array, item);
		obs_data_release(item);
	}
	obs_data_set_array(root, "warnings", array);
	obs_data_array_release(array);

	const char *json = obs_data_get_json(root);
	calldata_set_string(calldata, "json", json ? json : "{}");
	obs_data_release(root);
}

void gtCountdownProc(void *data, calldata_t *calldata)
{
	auto *s = static_cast<gt_source *>(data);
	const char *field = calldata_string(calldata, "field");
	const char *action = calldata_string(calldata, "action");
	if (!field || !action) {
		return;
	}
	const int64_t nowMs = (int64_t)(os_gettime_ns() / 1000000);
	const std::string wanted = action;

	std::lock_guard<std::mutex> lock(s->mutex);
	const auto countdown = s->countdowns.find(field);
	if (countdown == s->countdowns.end()) {
		return;
	}
	if (wanted == "start") {
		countdown->second.start(nowMs);
	} else if (wanted == "pause") {
		countdown->second.pause(nowMs);
	} else if (wanted == "stop") {
		countdown->second.stop(nowMs);
	} else if (wanted == "reset") {
		countdown->second.reset();
	}
	s->dirty = true;
}

void gtReloadTask(void *param)
{
	auto *weak = static_cast<obs_weak_source_t *>(param);
	obs_source_t *source = obs_weak_source_get_source(weak);
	obs_weak_source_release(weak);
	if (!source) {
		return;
	}
	auto *s = static_cast<gt_source *>(obs_obj_get_data(source));
	if (s) {
		std::string file;
		{
			std::lock_guard<std::mutex> lock(s->mutex);
			file = s->file;
		}
		if (!file.empty()) {
			applyFile(s, file);
			decodePictures(s);
		}
	}
	obs_source_release(source);
}

void *gtCreate(obs_data_t *settings, obs_source_t *source)
{
	auto *s = new gt_source();
	s->source = source;

	proc_handler_t *procs = obs_source_get_proc_handler(source);
	proc_handler_add(procs, gt::ProcInfo, gtInfoProc, s);
	proc_handler_add(procs, gt::ProcCountdown, gtCountdownProc, s);

	gtUpdate(s, settings ? settings : obs_source_get_settings(source));
	return s;
}

/* The renderer holds single-threaded Direct2D state, so it is released on the graphics thread; a task
 * left queued at process exit is reclaimed with the process. */
void gtDestroyRendererTask(void *param)
{
	delete static_cast<GtRenderer *>(param);
}

void gtDestroy(void *data)
{
	auto *s = static_cast<gt_source *>(data);

	obs_enter_graphics();
	releaseTexture(s);
	s->context.Reset();
	s->device.Reset();
	obs_leave_graphics();

	if (s->renderer) {
		obs_queue_task(OBS_TASK_GRAPHICS, gtDestroyRendererTask, s->renderer.release(), false);
	}
	delete s;
}

} // namespace

namespace gt {

obs_source_info gtTitleSourceInfo()
{
	obs_source_info info = {};
	info.id = gt::SourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	info.get_name = gtGetName;
	info.create = gtCreate;
	info.destroy = gtDestroy;
	info.get_width = gtGetWidth;
	info.get_height = gtGetHeight;
	info.update = gtUpdate;
	info.video_tick = gtVideoTick;
	info.video_render = gtVideoRender;
	info.icon_type = OBS_ICON_TYPE_IMAGE;
	return info;
}

} // namespace gt
