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

/* BreadStudio: after DistroAV 6.2.2 (9b1635f), src/ndi-source.cpp, cut down to what the desk's NDI tab
 * sets: the source, Low Bandwidth, Audio Only and Increase Buffer Size. Left out: PTZ, behaviour on
 * hide, timeout, sync modes, frame sync, the hardware-acceleration request, the alpha filter, colour
 * settings, the web control URL, and DistroAV's tally, which follows OBS's "active" and "showing";
 * tally follows breadstudio/shared/OnAir.hpp instead (network inputs spec §8). */

#include "Ndi.hpp"

#include <media-io/video-io.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <thread>

namespace {

/* DistroAV's values, kept so its scene collections open here */
constexpr int BandwidthHighest = 0;
constexpr int BandwidthLowest = 1;
constexpr int BandwidthAudioOnly = 2;
constexpr int LatencyUnbuffered = 2;

struct ndi_source {
	obs_source_t *source = nullptr;
	std::string name;   /* "MACHINE (name)" */
	int bandwidth = -1; /* -1 until the first update */
	std::thread worker;
	std::atomic<bool> run{false};
	std::atomic<int> tally{0}; /* 1 program, 2 preview; worked out on the UI thread, sent by the worker */
	float tallyWait = 0.0f;    /* graphics thread */
};

speaker_layout layoutFor(int channels)
{
	switch (channels) {
	case 1:
		return SPEAKERS_MONO;
	case 2:
		return SPEAKERS_STEREO;
	case 3:
		return SPEAKERS_2POINT1;
	case 4:
		return SPEAKERS_4POINT0;
	case 5:
		return SPEAKERS_4POINT1;
	case 6:
		return SPEAKERS_5POINT1;
	case 8:
		return SPEAKERS_7POINT1;
	default:
		return SPEAKERS_UNKNOWN;
	}
}

void outputVideo(ndi_source *s, const NDIlib_video_frame_v2_t &video)
{
	obs_source_frame frame = {};
	switch (video.FourCC) {
	case NDIlib_FourCC_type_BGRA:
		frame.format = VIDEO_FORMAT_BGRA;
		break;
	case NDIlib_FourCC_type_BGRX:
		frame.format = VIDEO_FORMAT_BGRX;
		break;
	case NDIlib_FourCC_type_UYVY:
		frame.format = VIDEO_FORMAT_UYVY;
		break;
	default:
		return; /* UYVY_BGRA gives only these three */
	}

	frame.width = (uint32_t)video.xres;
	frame.height = (uint32_t)video.yres;
	frame.linesize[0] = (uint32_t)video.line_stride_in_bytes;
	frame.data[0] = video.p_data;
	/* DistroAV's defaults: the sender's timecode, BT.709, partial range */
	frame.timestamp = (uint64_t)(video.timecode * 100);
	video_format_get_parameters(VIDEO_CS_709, VIDEO_RANGE_PARTIAL, frame.color_matrix, frame.color_range_min,
				    frame.color_range_max);
	obs_source_output_video(s->source, &frame);
}

void outputAudio(ndi_source *s, const NDIlib_audio_frame_v3_t &audio)
{
	const int channels = std::min(audio.no_channels, 8);
	obs_source_audio out = {};
	out.speakers = layoutFor(channels);
	if (out.speakers == SPEAKERS_UNKNOWN) {
		return;
	}
	out.format = AUDIO_FORMAT_FLOAT_PLANAR;
	out.samples_per_sec = (uint32_t)audio.sample_rate;
	out.frames = (uint32_t)audio.no_samples;
	out.timestamp = (uint64_t)(audio.timecode * 100);
	for (int c = 0; c < channels; c++) {
		out.data[c] = audio.p_data + (size_t)c * (size_t)audio.channel_stride_in_bytes;
	}
	obs_source_output_audio(s->source, &out);
}

/* NDI's receiver connects, and reconnects after its sender comes back, by itself */
void worker_main(ndi_source *s)
{
	NDIlib_recv_create_v3_t desc;
	desc.source_to_connect_to.p_ndi_name = s->name.c_str();
	desc.color_format = NDIlib_recv_color_format_UYVY_BGRA; /* BGRA when the sender has alpha */
	desc.bandwidth = s->bandwidth == BandwidthLowest      ? NDIlib_recv_bandwidth_lowest
			 : s->bandwidth == BandwidthAudioOnly ? NDIlib_recv_bandwidth_audio_only
							      : NDIlib_recv_bandwidth_highest;
	desc.allow_video_fields = false;
	desc.p_ndi_recv_name = "BreadStudio";

	NDIlib_recv_instance_t recv = ndiLib->recv_create_v3(&desc);
	if (!recv) {
		blog(LOG_WARNING, "[bread-ndi] cannot receive '%s'", s->name.c_str());
		return;
	}

	/* NDI sends the last tally again by itself whenever the receiver reconnects */
	int sent = -1;
	NDIlib_video_frame_v2_t video;
	NDIlib_audio_frame_v3_t audio;
	while (s->run) {
		const int want = s->tally;
		if (want != sent) {
			NDIlib_tally_t tally;
			tally.on_program = (want & 1) != 0;
			tally.on_preview = (want & 2) != 0;
			ndiLib->recv_set_tally(recv, &tally);
			sent = want;
		}

		switch (ndiLib->recv_capture_v3(recv, &video, &audio, nullptr, 100)) {
		case NDIlib_frame_type_video:
			outputVideo(s, video);
			ndiLib->recv_free_video_v2(recv, &video);
			break;
		case NDIlib_frame_type_audio:
			outputAudio(s, audio);
			ndiLib->recv_free_audio_v3(recv, &audio);
			break;
		default:
			break;
		}
	}

	ndiLib->recv_destroy(recv);
}

void stop(ndi_source *s)
{
	s->run = false;
	if (s->worker.joinable()) {
		s->worker.join();
	}
}

void ndi_update(void *data, obs_data_t *settings)
{
	auto *s = static_cast<ndi_source *>(data);

	/* Increase Buffer Size: OBS's own buffering. Changing it does not restart the receiver. */
	obs_source_set_async_unbuffered(s->source, obs_data_get_int(settings, "latency") == LatencyUnbuffered);

	const std::string name = obs_data_get_string(settings, "ndi_source_name");
	const int bandwidth = (int)obs_data_get_int(settings, "ndi_bw_mode");
	if (name == s->name && bandwidth == s->bandwidth) {
		return;
	}

	stop(s);
	s->name = name;
	s->bandwidth = bandwidth;
	/* no old picture under a new source, nor under Audio Only */
	obs_source_output_video(s->source, nullptr);
	if (name.empty() || !ndiLib) {
		return;
	}
	s->run = true;
	s->worker = std::thread(worker_main, s);
}

const char *ndi_get_name(void *)
{
	return obs_module_text("NDISource");
}

void *ndi_create(obs_data_t *settings, obs_source_t *source)
{
	auto *s = new ndi_source();
	s->source = source;
	ndi_update(s, settings);
	return s;
}

void ndi_destroy(void *data)
{
	auto *s = static_cast<ndi_source *>(data);
	stop(s);
	delete s;
}

void ndi_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "ndi_source_name", "");
	obs_data_set_default_int(settings, "ndi_bw_mode", BandwidthHighest);
	obs_data_set_default_int(settings, "latency", LatencyUnbuffered);
}

/* OBS's own window shows these; the desk's NDI tab reads the list and `status` (spec §6.1). data is null
 * when OBS asks for the type's settings without a source. */
obs_properties_t *ndi_properties(void *)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *name = obs_properties_add_list(props, "ndi_source_name", obs_module_text("Source"),
						       OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	for (const std::string &seen : ndiSources()) {
		obs_property_list_add_string(name, seen.c_str(), seen.c_str());
	}

	obs_property_t *bandwidth = obs_properties_add_list(props, "ndi_bw_mode", obs_module_text("Bandwidth"),
							    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(bandwidth, obs_module_text("Bandwidth.Highest"), BandwidthHighest);
	obs_property_list_add_int(bandwidth, obs_module_text("Bandwidth.Lowest"), BandwidthLowest);
	obs_property_list_add_int(bandwidth, obs_module_text("Bandwidth.AudioOnly"), BandwidthAudioOnly);

	obs_property_t *buffer = obs_properties_add_list(props, "latency", obs_module_text("Buffer"),
							 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(buffer, obs_module_text("Buffer.Normal"), LatencyUnbuffered);
	obs_property_list_add_int(buffer, obs_module_text("Buffer.Increased"), 0);

	if (!ndiStatus().empty()) {
		obs_properties_add_text(props, "status", ndiStatus().c_str(), OBS_TEXT_INFO);
	}
	return props;
}

} // namespace

obs_source_info ndiSourceInfo()
{
	obs_source_info info = {};
	info.id = "ndi_source"; /* DistroAV's, so a collection made with DistroAV opens here */
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name = ndi_get_name;
	info.create = ndi_create;
	info.destroy = ndi_destroy;
	info.get_defaults = ndi_defaults;
	info.get_properties = ndi_properties;
	info.update = ndi_update;
	info.icon_type = OBS_ICON_TYPE_CAMERA;
	return info;
}