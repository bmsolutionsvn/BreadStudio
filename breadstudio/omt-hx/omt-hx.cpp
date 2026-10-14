/* The OMT source: plays one Open Media Transport source through omthx. One receiver reads plain OMT
 * (VMX), OMT-HX HEVC and OMT-HX H.264, and tells them apart by itself (network inputs spec N7). */
#include <obs-module.h>
#include <util/platform.h>

#include <atomic>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "omthx/omthx.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("omt-hx", "en-US")

/* exported by libomt.dll, but not declared in the libomt.h that OMT-HX ships */
extern "C" void omt_shutdown();

namespace {

struct omt_source {
	obs_source_t *source = nullptr;
	std::string address;
	std::unique_ptr<omthx::Receiver> rx;
	std::mutex rxMutex; /* guards rx against the UI thread */
	std::thread worker;
	std::atomic<bool> run{false};
	std::vector<uint8_t> scratch; /* a straight-alpha picture, premultiplied */
};

void worker_main(omt_source *s)
{
	omthx::VideoFrame video;
	omthx::AudioFrame audio;

	/* OMT timestamps are 100 ns units on the sender's clock. The first frame of either kind
	 * anchors them to OBS's clock, so picture and sound keep one base. */
	bool anchored = false;
	uint64_t base = 0;
	auto toObs = [&](int64_t omt) {
		if (!anchored) {
			base = os_gettime_ns() - (uint64_t)omt * 100;
			anchored = true;
		}
		return base + (uint64_t)omt * 100;
	};

	while (s->run) {
		if (s->rx->receiveVideo(video, 20)) {
			obs_source_frame frame = {};
			frame.format = VIDEO_FORMAT_BGRA;
			frame.width = (uint32_t)video.format.width;
			frame.height = (uint32_t)video.format.height;
			frame.linesize[0] = frame.width * 4;
			frame.timestamp = toObs(video.timestamp);
			frame.data[0] = video.bgra.data();

			/* OBS composites premultiplied pictures */
			if (video.format.alpha && !video.premultiplied) {
				s->scratch.resize(video.bgra.size());
				for (size_t i = 0; i < video.bgra.size(); i += 4) {
					const unsigned alpha = video.bgra[i + 3];
					for (size_t k = 0; k < 3; k++) {
						s->scratch[i + k] = (uint8_t)((video.bgra[i + k] * alpha + 127) / 255);
					}
					s->scratch[i + 3] = (uint8_t)alpha;
				}
				frame.data[0] = s->scratch.data();
			}
			obs_source_output_video(s->source, &frame);
		}

		while (s->rx->receiveAudio(audio, 0)) {
			/* the first two channels: OMT cameras send stereo */
			const int channels = audio.channels >= 2 ? 2 : 1;
			obs_source_audio out = {};
			for (int c = 0; c < channels; c++) {
				out.data[c] =
					(const uint8_t *)(audio.planar.data() + (size_t)c * (size_t)audio.samples);
			}
			out.frames = (uint32_t)audio.samples;
			out.speakers = channels == 2 ? SPEAKERS_STEREO : SPEAKERS_MONO;
			out.format = AUDIO_FORMAT_FLOAT_PLANAR;
			out.samples_per_sec = (uint32_t)audio.sampleRate;
			out.timestamp = toObs(audio.timestamp);
			obs_source_output_audio(s->source, &out);
		}
	}
}

void stop_receiver(omt_source *s)
{
	s->run = false;
	if (s->worker.joinable()) {
		s->worker.join();
	}
	std::lock_guard lock(s->rxMutex);
	s->rx.reset();
}

/* A receiver keeps trying by itself while its sender is away, and follows it when it comes back
 * in another format. */
void omt_update(void *data, obs_data_t *settings)
{
	auto *s = static_cast<omt_source *>(data);
	const std::string address = obs_data_get_string(settings, "address");
	if (address == s->address) {
		return;
	}

	stop_receiver(s);
	s->address = address;
	if (address.empty()) {
		return;
	}

	std::unique_ptr<omthx::Receiver> rx;
	try {
		rx = std::make_unique<omthx::Receiver>(address);
	} catch (const std::exception &e) {
		blog(LOG_WARNING, "[omt-hx] cannot open '%s': %s", address.c_str(), e.what());
		return;
	}
	{
		std::lock_guard lock(s->rxMutex);
		s->rx = std::move(rx);
	}
	s->run = true;
	s->worker = std::thread(worker_main, s);
}

const char *omt_get_name(void *)
{
	return obs_module_text("OMTSource");
}

void *omt_create(obs_data_t *settings, obs_source_t *source)
{
	auto *s = new omt_source();
	s->source = source;
	omt_update(s, settings);
	return s;
}

void omt_destroy(void *data)
{
	auto *s = static_cast<omt_source *>(data);
	stop_receiver(s);
	delete s;
}

void omt_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "address", "");
}

/* data is null when OBS asks for the type's settings without a source */
obs_properties_t *omt_properties(void *)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *address = obs_properties_add_list(props, "address", obs_module_text("Source"),
							  OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	for (const std::string &seen : omthx::discover()) {
		obs_property_list_add_string(address, seen.c_str(), seen.c_str());
	}
	return props;
}

} // namespace

static const obs_source_info omt_hx_source = {
	.id = "omt_hx_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO,
	.get_name = omt_get_name,
	.create = omt_create,
	.destroy = omt_destroy,
	.get_defaults = omt_defaults,
	.get_properties = omt_properties,
	.update = omt_update,
	.icon_type = OBS_ICON_TYPE_CAMERA,
};

bool obs_module_load(void)
{
	obs_register_source(&omt_hx_source);
	/* discovery needs a second or two: start it now, so the list is full when it is first shown */
	omthx::discover();
	return true;
}

/* after every source is gone */
void obs_module_unload(void)
{
	omt_shutdown();
}
