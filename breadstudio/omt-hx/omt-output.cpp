/* bread_omt_output: an output's picture and sound sent as plain OMT (VMX) or OMT-HX, on omthx's
 * Sender (outputs spec §4.3). The engine gives it a canvas and one audio bus. A BGRA canvas (alpha)
 * and a BT.709 NV12 canvas go to omthx as they are; any other canvas is converted to the YUV the
 * codec takes, never to RGB. Its receiver count is offered through `get_receivers`. */

#include <obs-module.h>

#include <omthx/omthx.h>

#include <media-io/video-scaler.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include <cstring>
#include <exception>

namespace {

struct OmtOutput {
	obs_output_t *output = nullptr;
	std::string name;
	omthx::Mode mode = omthx::Mode::Full;
	omthx::Codec codec = omthx::Codec::Hevc;

	std::shared_ptr<omthx::Sender> sender;
	std::mutex senderMutex;

	uint32_t width = 0;
	uint32_t height = 0;
	int fpsNum = 0;
	int fpsDen = 1;
	bool wantAlpha = false; /* the output sends alpha, from the engine's "alpha" setting */
	bool alpha = false;
	/* what goes to omthx: the BGRA canvas, NV12 (the canvas's, or converted), or UYVY (converted) */
	enum class Picture { Bgra, Nv12, Uyvy } picture = Picture::Bgra;
	bool fullRange = false; /* the NV12 picture's range */
	int sampleRate = 0;
	size_t channels = 0;
	std::atomic<bool> started = false;

	video_scaler_t *scaler = nullptr; /* converts a canvas omthx cannot take as it is */
	std::vector<uint8_t> converted;   /* the converted frame: NV12 (luma, then chroma) or UYVY */
};

/* create() adds this to the output's proc handler before its definition below */
void getReceivers(void *data, calldata_t *call);

const char *omtOutputName(void *)
{
	return obs_module_text("OmtOutput");
}

void omtOutputDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "name", "BreadStudio output");
	obs_data_set_default_string(settings, "mode", "vmx");
}

void omtOutputUpdate(void *data, obs_data_t *settings)
{
	auto *o = static_cast<OmtOutput *>(data);
	o->name = obs_data_get_string(settings, "name");
	const std::string mode = obs_data_get_string(settings, "mode");
	/* the engine passes "vmx", "hevc" or "h264"; OMT (VMX) is the default */
	o->mode = mode == "hevc" || mode == "h264" ? omthx::Mode::Hx : omthx::Mode::Full;
	o->codec = mode == "h264" ? omthx::Codec::H264 : omthx::Codec::Hevc;
	o->wantAlpha = obs_data_get_bool(settings, "alpha");
}

void *omtOutputCreate(obs_data_t *settings, obs_output_t *output)
{
	auto *o = new OmtOutput();
	o->output = output;
	omtOutputUpdate(o, settings);
	proc_handler_add(obs_output_get_proc_handler(output), "void get_receivers(out int count)", getReceivers, o);
	return o;
}

void omtOutputDestroy(void *data)
{
	auto *o = static_cast<OmtOutput *>(data);
	video_scaler_destroy(o->scaler);
	o->scaler = nullptr;
	delete o;
}

bool omtOutputStart(void *data)
{
	auto *o = static_cast<OmtOutput *>(data);
	if (o->started) {
		return false;
	}

	video_t *video = obs_output_video(o->output);
	audio_t *audio = obs_output_audio(o->output);
	uint32_t flags = 0;
	if (video) {
		const video_output_info *info = video_output_get_info(video);
		o->width = info->width;
		o->height = info->height;
		o->fpsNum = (int)info->fps_num;
		o->fpsDen = (int)info->fps_den;
		/* a BGRA canvas is premultiplied, which is what sendVideo is told; alpha costs a picture of
		 * double height, so it is coded only when the output sends it */
		o->alpha = o->wantAlpha && info->format == VIDEO_FORMAT_BGRA;
		const bool bt709 = info->colorspace == VIDEO_CS_DEFAULT || info->colorspace == VIDEO_CS_709 ||
				   info->colorspace == VIDEO_CS_SRGB;
		const bool hx = o->mode == omthx::Mode::Hx;
		o->fullRange = info->range == VIDEO_RANGE_FULL;
		/* BT.709 NV12 (OBS's default) goes as it is: to the HX encoder in either range, to VMX in
		 * limited range, the only one VMX reads. Anything else but BGRA is converted on the CPU to
		 * the YUV the codec takes (NV12 for HX, UYVY for VMX), which needs no colour matrix. */
		const bool asIs = info->format == VIDEO_FORMAT_NV12 && bt709 && (hx || !o->fullRange);
		if (info->format == VIDEO_FORMAT_BGRA) {
			o->picture = OmtOutput::Picture::Bgra;
		} else if (asIs) {
			o->picture = OmtOutput::Picture::Nv12;
		} else {
			o->picture = hx ? OmtOutput::Picture::Nv12 : OmtOutput::Picture::Uyvy;
			video_scale_info src = {};
			src.format = info->format;
			src.width = info->width;
			src.height = info->height;
			src.colorspace = info->colorspace;
			src.range = info->range;
			video_scale_info dst = src;
			dst.format = hx ? VIDEO_FORMAT_NV12 : VIDEO_FORMAT_UYVY;
			dst.colorspace = VIDEO_CS_709;
			dst.range = VIDEO_RANGE_PARTIAL;
			o->fullRange = false;
			if (video_scaler_create(&o->scaler, &dst, &src, VIDEO_SCALE_POINT) != VIDEO_SCALER_SUCCESS) {
				blog(LOG_WARNING, "[bread-omt] output '%s': the canvas cannot be converted to %s",
				     o->name.c_str(), hx ? "NV12" : "UYVY");
				return false;
			}
			const size_t pixels = (size_t)o->width * o->height;
			o->converted.resize(hx ? pixels * 3 / 2 : pixels * 2); /* NV12, or UYVY */
		}
		flags |= OBS_OUTPUT_VIDEO;
	}
	if (audio) {
		o->sampleRate = audio_output_get_sample_rate(audio);
		o->channels = audio_output_get_channels(audio);
		flags |= OBS_OUTPUT_AUDIO;
	}
	if (!flags) {
		return false;
	}

	std::lock_guard lock(o->senderMutex);
	try {
		o->sender = std::make_shared<omthx::Sender>(o->name, o->mode, omthx::Quality::High, o->codec);
	} catch (const std::exception &error) {
		blog(LOG_WARNING, "[bread-omt] output '%s': cannot start: %s", o->name.c_str(), error.what());
		return false;
	}
	o->started = obs_output_begin_data_capture(o->output, flags);
	if (!o->started) {
		o->sender.reset();
	}
	return o->started;
}

void omtOutputStop(void *data, uint64_t)
{
	auto *o = static_cast<OmtOutput *>(data);
	if (!o->started) {
		return;
	}
	o->started = false;
	obs_output_end_data_capture(o->output);

	{
		std::lock_guard lock(o->senderMutex);
		o->sender.reset();
	}

	video_scaler_destroy(o->scaler);
	o->scaler = nullptr;
}

void getReceivers(void *data, calldata_t *call)
{
	auto *o = static_cast<OmtOutput *>(data);
	std::shared_ptr<omthx::Sender> sender;
	{
		std::lock_guard lock(o->senderMutex);
		sender = o->sender;
	}
	int count = 0;
	if (sender) {
		count = sender->stats().connections;
	}
	calldata_set_int(call, "count", count);
}

void omtOutputRawVideo(void *data, video_data *frame)
{
	auto *o = static_cast<OmtOutput *>(data);
	if (!o->started || o->width == 0 || o->height == 0) {
		return;
	}

	std::shared_ptr<omthx::Sender> sender;
	{
		std::lock_guard lock(o->senderMutex);
		sender = o->sender;
	}
	if (!sender) {
		return;
	}

	omthx::VideoFormat format;
	format.width = (int)o->width;
	format.height = (int)o->height;
	format.fpsN = o->fpsNum;
	format.fpsD = o->fpsDen;
	format.alpha = o->alpha;

	/* OMT ticks: the frame's timestamp is in nanoseconds, divided by 100 to reach OMT ticks */
	const int64_t timestamp = (int64_t)frame->timestamp / 100;
	const size_t w = o->width, h = o->height;
	if (o->picture == OmtOutput::Picture::Bgra) {
		const std::span<const uint8_t> bgra(frame->data[0], w * h * 4);
		sender->sendVideo(format, bgra, o->alpha, timestamp);
		return;
	}

	const uint8_t *planes[2] = {frame->data[0], frame->data[1]};
	uint32_t linesizes[2] = {frame->linesize[0], frame->linesize[1]};
	if (o->scaler) {
		uint8_t *out[2] = {o->converted.data(), o->converted.data() + w * h};
		uint32_t outLinesize[2] = {(uint32_t)(o->picture == OmtOutput::Picture::Uyvy ? w * 2 : w), (uint32_t)w};
		if (!video_scaler_scale(o->scaler, out, outLinesize, frame->data, frame->linesize)) {
			return;
		}
		planes[0] = out[0];
		planes[1] = out[1];
		linesizes[0] = outLinesize[0];
		linesizes[1] = outLinesize[1];
	}
	if (o->picture == OmtOutput::Picture::Nv12) {
		sender->sendVideoNv12(format, planes[0], (int)linesizes[0], planes[1], (int)linesizes[1], o->fullRange,
				      timestamp);
	} else {
		sender->sendVideoUyvy(format, planes[0], (int)linesizes[0], nullptr, 0, false, timestamp);
	}
}

void omtOutputRawAudio(void *data, audio_data *frame)
{
	auto *o = static_cast<OmtOutput *>(data);
	if (!o->started || o->sampleRate == 0 || o->channels == 0) {
		return;
	}

	std::shared_ptr<omthx::Sender> sender;
	{
		std::lock_guard lock(o->senderMutex);
		sender = o->sender;
	}
	if (!sender) {
		return;
	}

	/* OBS gives one plane per channel; the loop assembles them one channel after another, as omthx wants */
	std::vector<float> planar((size_t)frame->frames * o->channels);
	for (size_t c = 0; c < o->channels; c++) {
		std::memcpy(planar.data() + c * (size_t)frame->frames, frame->data[c],
			    (size_t)frame->frames * sizeof(float));
	}
	sender->sendAudio(planar, o->sampleRate, (int)o->channels, (int64_t)frame->timestamp / 100);
}

} // namespace

obs_output_info omtOutputInfo()
{
	obs_output_info info = {};
	info.id = "bread_omt_output";
	info.flags = OBS_OUTPUT_AV;
	info.get_name = omtOutputName;
	info.get_defaults = omtOutputDefaults;
	info.create = omtOutputCreate;
	info.start = omtOutputStart;
	info.stop = omtOutputStop;
	info.update = omtOutputUpdate;
	info.destroy = omtOutputDestroy;
	info.raw_video = omtOutputRawVideo;
	info.raw_audio = omtOutputRawAudio;
	return info;
}
