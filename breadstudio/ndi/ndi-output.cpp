/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

/* BreadStudio: after DistroAV 6.2.2 (9b1635f), src/ndi-output.cpp, cut down to what the desk's
 * Outputs window sets: one name, both picture and sound, no groups, no per-stream switches, and no
 * config file or Qt. DistroAV's I444 -> UYVY converter is gone: the engine's canvas is NV12 or
 * BGRA, and NDI takes both as they are (outputs spec §4.3). Its receiver count is offered through
 * `get_receivers`, which the engine reads. */

#include "Ndi.hpp"

#include <atomic>
#include <cstring>
#include <mutex>

namespace {

struct NdiOutput {
	obs_output_t *output = nullptr;
	std::string name;

	NDIlib_send_instance_t sender = nullptr;
	std::mutex senderMutex;

	uint32_t width = 0;
	uint32_t height = 0;
	int fpsNum = 0;
	int fpsDen = 1;
	NDIlib_FourCC_video_type_e fourcc = NDIlib_FourCC_video_type_BGRA;
	uint32_t sampleRate = 0;
	size_t channels = 0;
	std::atomic<bool> started = false;
	std::vector<uint8_t> audioBuffer; /* the planes, one channel after another, for FLTP */
};

/* create() adds this to the output's proc handler before its definition below */
void getReceivers(void *data, calldata_t *call);

const char *ndiOutputName(void *)
{
	return obs_module_text("NdiOutput");
}

void ndiOutputDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "name", "BreadStudio output");
}

void ndiOutputUpdate(void *data, obs_data_t *settings)
{
	auto *o = static_cast<NdiOutput *>(data);
	o->name = obs_data_get_string(settings, "name");
}

void *ndiOutputCreate(obs_data_t *settings, obs_output_t *output)
{
	auto *o = new NdiOutput();
	o->output = output;
	ndiOutputUpdate(o, settings);
	proc_handler_add(obs_output_get_proc_handler(output), "void get_receivers(out int count)", getReceivers, o);
	return o;
}

void ndiOutputDestroy(void *data)
{
	delete static_cast<NdiOutput *>(data);
}

bool ndiOutputStart(void *data)
{
	auto *o = static_cast<NdiOutput *>(data);
	if (o->started || !ndiLib) {
		return false;
	}

	uint32_t flags = 0;
	video_t *video = obs_output_video(o->output);
	audio_t *audio = obs_output_audio(o->output);

	if (video) {
		const video_output_info *info = video_output_get_info(video);
		o->width = info->width;
		o->height = info->height;
		o->fpsNum = (int)info->fps_num;
		o->fpsDen = (int)info->fps_den;
		switch (info->format) {
		case VIDEO_FORMAT_NV12:
			o->fourcc = NDIlib_FourCC_video_type_NV12;
			break;
		case VIDEO_FORMAT_BGRA:
			o->fourcc = NDIlib_FourCC_video_type_BGRA;
			break;
		case VIDEO_FORMAT_BGRX:
			o->fourcc = NDIlib_FourCC_video_type_BGRX;
			break;
		default:
			/* the engine's canvas is NV12 or BGRA only (Plan D, outputs spec §4.2) */
			blog(LOG_WARNING, "[bread-ndi] output '%s': unsupported video format %d", o->name.c_str(),
			     (int)info->format);
			return false;
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

	NDIlib_send_create_t desc;
	desc.p_ndi_name = o->name.c_str();
	desc.p_groups = nullptr;
	desc.clock_video = false;
	desc.clock_audio = false;

	std::lock_guard lock(o->senderMutex);
	o->sender = ndiLib->send_create(&desc);
	if (!o->sender) {
		blog(LOG_WARNING, "[bread-ndi] output '%s': cannot create the NDI sender", o->name.c_str());
		return false;
	}
	o->started = obs_output_begin_data_capture(o->output, flags);
	if (!o->started) {
		ndiLib->send_destroy(o->sender);
		o->sender = nullptr;
	}
	return o->started;
}

void ndiOutputStop(void *data, uint64_t)
{
	auto *o = static_cast<NdiOutput *>(data);
	if (!o->started) {
		return;
	}
	o->started = false;
	obs_output_end_data_capture(o->output);

	std::lock_guard lock(o->senderMutex);
	if (o->sender) {
		ndiLib->send_destroy(o->sender);
		o->sender = nullptr;
	}
}

void getReceivers(void *data, calldata_t *call)
{
	auto *o = static_cast<NdiOutput *>(data);
	int count = 0;
	std::lock_guard lock(o->senderMutex);
	if (o->sender) {
		count = ndiLib ? ndiLib->send_get_no_connections(o->sender, 10) : 0;
	}
	calldata_set_int(call, "count", count);
}

void ndiOutputRawVideo(void *data, video_data *frame)
{
	auto *o = static_cast<NdiOutput *>(data);
	if (!o->started || o->width == 0 || o->height == 0) {
		return;
	}

	std::lock_guard lock(o->senderMutex);
	if (!o->sender) {
		return;
	}

	NDIlib_video_frame_v2_t video;
	video.xres = (int)o->width;
	video.yres = (int)o->height;
	video.frame_rate_N = o->fpsNum;
	video.frame_rate_D = o->fpsDen;
	video.FourCC = o->fourcc;
	video.frame_format_type = NDIlib_frame_format_type_progressive;
	video.timecode = NDIlib_send_timecode_synthesize;
	video.timestamp = (int64_t)(frame->timestamp / 100);
	video.p_data = frame->data[0];
	video.line_stride_in_bytes = (int)frame->linesize[0];
	video.p_metadata = nullptr;

	/* async: the frame stays OBS's until the next one, exactly as DistroAV sends it */
	ndiLib->send_send_video_async_v2(o->sender, &video);
}

void ndiOutputRawAudio(void *data, audio_data *frame)
{
	auto *o = static_cast<NdiOutput *>(data);
	if (!o->started || o->sampleRate == 0 || o->channels == 0) {
		return;
	}

	std::lock_guard lock(o->senderMutex);
	if (!o->sender) {
		return;
	}

	NDIlib_audio_frame_v3_t audio;
	audio.sample_rate = (int)o->sampleRate;
	audio.no_channels = (int)o->channels;
	audio.no_samples = (int)frame->frames;
	audio.timecode = NDIlib_send_timecode_synthesize;
	audio.timestamp = (int64_t)(frame->timestamp / 100);
	audio.FourCC = NDIlib_FourCC_audio_type_FLTP;
	audio.channel_stride_in_bytes = (int)(frame->frames * sizeof(float));

	/* OBS hands planar float with one plane per channel; NDI's FLTP wants the channels one after
	 * another, so a plane's samples are copied into a buffer that outlives the send */
	const size_t plane = (size_t)frame->frames * sizeof(float);
	o->audioBuffer.resize((size_t)o->channels * plane);
	for (size_t c = 0; c < o->channels; c++) {
		std::memcpy(o->audioBuffer.data() + c * plane, frame->data[c], plane);
	}
	audio.p_data = o->audioBuffer.data();

	ndiLib->send_send_audio_v3(o->sender, &audio);
}

} // namespace

obs_output_info ndiOutputInfo()
{
	obs_output_info info = {};
	info.id = "bread_ndi_output";
	info.flags = OBS_OUTPUT_AV;
	info.get_name = ndiOutputName;
	info.get_defaults = ndiOutputDefaults;
	info.create = ndiOutputCreate;
	info.start = ndiOutputStart;
	info.stop = ndiOutputStop;
	info.update = ndiOutputUpdate;
	info.destroy = ndiOutputDestroy;
	info.raw_video = ndiOutputRawVideo;
	info.raw_audio = ndiOutputRawAudio;
	return info;
}
