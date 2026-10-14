/* A test NDI sender: a moving bar and a 1 kHz tone at -20 dBFS, and once a second its connections and the
 * tally its receivers send back. Usage: bread-ndi-send [name]. Runs until Ctrl+C. */
#include "NdiLoad.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
	const std::string name = argc > 1 ? argv[1] : "Bread Test";

	HMODULE module = nullptr;
	std::wstring from;
	const NDIlib_v6 *ndi = loadNdi(module, from);
	if (!ndi || !ndi->initialize()) {
		std::puts("bread-ndi-send: NDI's library is not available");
		return 1;
	}

	NDIlib_send_create_t desc;
	desc.p_ndi_name = name.c_str();
	desc.clock_video = true; /* sending a frame waits for its time, so the loop runs at the frame rate */
	NDIlib_send_instance_t sender = ndi->send_create(&desc);
	if (!sender) {
		std::puts("bread-ndi-send: cannot create the sender");
		return 1;
	}

	const int width = 1280;
	const int height = 720;
	const int fps = 30;
	const int rate = 48000;
	const int samples = rate / fps;

	std::vector<uint8_t> picture((size_t)width * height * 4);
	std::vector<float> sound((size_t)samples * 2);

	NDIlib_video_frame_v2_t video;
	video.xres = width;
	video.yres = height;
	video.FourCC = NDIlib_FourCC_type_BGRX;
	video.frame_rate_N = fps;
	video.frame_rate_D = 1;
	video.frame_format_type = NDIlib_frame_format_type_progressive;
	video.p_data = picture.data();
	video.line_stride_in_bytes = width * 4;

	NDIlib_audio_frame_v3_t audio;
	audio.sample_rate = rate;
	audio.no_channels = 2;
	audio.no_samples = samples;
	audio.FourCC = NDIlib_FourCC_audio_type_FLTP;
	audio.p_data = reinterpret_cast<uint8_t *>(sound.data());
	audio.channel_stride_in_bytes = samples * (int)sizeof(float);

	std::printf("sending as '%s' on this PC\n", name.c_str());
	auto report = std::chrono::steady_clock::now();
	for (int64_t frame = 0;; frame++) {
		const int bar = (int)(frame * 8 % width);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				uint8_t *p = &picture[((size_t)y * width + x) * 4];
				const bool lit = x >= bar && x < bar + 64;
				p[0] = lit ? 255 : 40;
				p[1] = lit ? 255 : 40;
				p[2] = lit ? 255 : 160;
				p[3] = 255;
			}
		}
		for (int i = 0; i < samples; i++) {
			const double t = (double)(frame * samples + i) / rate;
			sound[(size_t)i] = sound[(size_t)(samples + i)] =
				0.1f * (float)std::sin(2 * 3.141592653589793 * 1000 * t);
		}
		ndi->send_send_audio_v3(sender, &audio);
		ndi->send_send_video_v2(sender, &video);

		if (std::chrono::steady_clock::now() >= report) {
			report += std::chrono::seconds(1);
			NDIlib_tally_t tally;
			ndi->send_get_tally(sender, &tally, 0);
			std::printf("connections %d  tally %s%s\n", ndi->send_get_no_connections(sender, 0),
				    tally.on_program ? "PGM " : "", tally.on_preview ? "PVW" : "");
			std::fflush(stdout);
		}
	}
}