/* bread_srt_output: an output's encoded picture and sound, muxed to MPEG-TS by FFmpeg and sent over
 * FFmpeg's own `srt` protocol, with SRT's settings as options and never in the address (outputs
 * spec §4.4). OBS's own MPEG-TS output carries its own copy of the SRT code and reads the settings
 * from the address, where a passphrase with '&' is cut short. */
#include <obs-module.h>
#include <media-io/audio-io.h>
#include <media-io/video-io.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bread-srt", "en-US")

namespace {

/* what get_state answers; the desk words them (outputs spec §3.3) */
enum State { StateOff = 0, StateWaiting = 1, StateConnected = 2, StateRetrying = 3 };

/* About ten seconds of a 60 fps picture with its sound. More means the network has fallen behind:
 * the queue is dropped and sending starts again at the next keyframe. */
constexpr size_t MaxQueued = 1200;

struct Packet {
	std::vector<uint8_t> data;
	int64_t pts = 0;
	int64_t dts = 0;
	AVRational timebase = {1, 1};
	bool video = false;
	bool keyframe = false;
};

struct SrtOutput {
	obs_output_t *output = nullptr;
	std::string url;
	std::vector<std::pair<std::string, std::string>> options;
	bool listener = false;

	std::thread worker;
	std::atomic<bool> stopping{false};
	std::atomic<int> state{StateOff};
	std::atomic<uint64_t> bytes{0};

	std::mutex mutex; /* guards everything below */
	std::condition_variable wake;
	std::deque<Packet> queue;
	bool connected = false; /* packets are kept only while connected */
	bool waitKey = true;    /* after a connect or a drop: nothing until a video keyframe */
};

std::string errorText(int code)
{
	char text[AV_ERROR_MAX_STRING_SIZE] = {};
	av_strerror(code, text, sizeof(text));
	return text;
}

/* FFmpeg calls this while it waits on the network: non-zero ends the wait */
int interrupted(void *opaque)
{
	return static_cast<SrtOutput *>(opaque)->stopping ? 1 : 0;
}

AVStream *addStream(AVFormatContext *context, obs_encoder_t *encoder)
{
	const AVCodecDescriptor *codec = avcodec_descriptor_get_by_name(obs_encoder_get_codec(encoder));
	if (!codec) {
		return nullptr;
	}
	AVStream *stream = avformat_new_stream(context, nullptr);
	if (!stream) {
		return nullptr;
	}

	AVCodecParameters *par = stream->codecpar;
	par->codec_type = codec->type;
	par->codec_id = codec->id;
	if (codec->type == AVMEDIA_TYPE_VIDEO) {
		const video_output_info *info = video_output_get_info(obs_encoder_video(encoder));
		par->width = (int)obs_encoder_get_width(encoder);
		par->height = (int)obs_encoder_get_height(encoder);
		stream->time_base = AVRational{(int)info->fps_den, (int)info->fps_num};
	} else {
		par->sample_rate = (int)obs_encoder_get_sample_rate(encoder);
		par->frame_size = (int)obs_encoder_get_frame_size(encoder);
		av_channel_layout_default(&par->ch_layout, (int)audio_output_get_channels(obs_encoder_audio(encoder)));
		stream->time_base = AVRational{1, par->sample_rate};
	}

	/* the H.264/HEVC headers and AAC's config; FFmpeg repeats them at keyframes in MPEG-TS */
	uint8_t *extra = nullptr;
	size_t size = 0;
	if (obs_encoder_get_extra_data(encoder, &extra, &size) && size > 0) {
		par->extradata = static_cast<uint8_t *>(av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE));
		std::memcpy(par->extradata, extra, size);
		par->extradata_size = (int)size;
	}
	return stream;
}

/* Sends queued packets until a write fails or the output stops. */
void sendUntilFailure(SrtOutput *s, AVFormatContext *context, AVStream *video, AVStream *audio)
{
	AVPacket *packet = av_packet_alloc();
	if (!packet) {
		blog(LOG_WARNING, "[bread-srt] '%s': cannot allocate a packet", obs_output_get_name(s->output));
		return;
	}
	while (!s->stopping) {
		Packet next;
		{
			std::unique_lock lock(s->mutex);
			s->wake.wait(lock, [s] { return s->stopping || !s->queue.empty(); });
			if (s->stopping) {
				break;
			}
			next = std::move(s->queue.front());
			s->queue.pop_front();
		}

		AVStream *stream = next.video ? video : audio;
		if (!stream) {
			continue;
		}
		av_packet_unref(packet);
		if (av_new_packet(packet, (int)next.data.size()) < 0) {
			break;
		}
		std::memcpy(packet->data, next.data.data(), next.data.size());
		packet->stream_index = stream->index;
		packet->pts = av_rescale_q(next.pts, next.timebase, stream->time_base);
		packet->dts = av_rescale_q(next.dts, next.timebase, stream->time_base);
		packet->flags = next.keyframe ? AV_PKT_FLAG_KEY : 0;

		const int written = av_interleaved_write_frame(context, packet);
		if (written < 0) {
			blog(LOG_WARNING, "[bread-srt] '%s': sending stopped: %s", obs_output_get_name(s->output),
			     errorText(written).c_str());
			break;
		}
		s->bytes += next.data.size();
	}
	av_packet_free(&packet);
}

/* Connects, sends, and on a failure connects again, until the output stops. */
void run(SrtOutput *s)
{
	obs_encoder_t *videoEncoder = obs_output_get_video_encoder(s->output);
	obs_encoder_t *audioEncoder = obs_output_get_audio_encoder(s->output, 0);

	while (!s->stopping) {
		s->state = StateWaiting;

		AVFormatContext *context = nullptr;
		int result = avformat_alloc_output_context2(&context, nullptr, "mpegts", nullptr);
		AVStream *video = result >= 0 ? addStream(context, videoEncoder) : nullptr;
		AVStream *audio = result >= 0 && audioEncoder ? addStream(context, audioEncoder) : nullptr;

		if (video) {
			AVDictionary *options = nullptr;
			for (const auto &[key, value] : s->options) {
				av_dict_set(&options, key.c_str(), value.c_str(), 0);
			}
			AVIOInterruptCB interrupt = {interrupted, s};
			/* a listener waits here for its caller; stopping ends the wait */
			result = avio_open2(&context->pb, s->url.c_str(), AVIO_FLAG_WRITE, &interrupt, &options);
			av_dict_free(&options);
			if (result >= 0) {
				result = avformat_write_header(context, nullptr);
			}

			if (result >= 0) {
				{
					std::lock_guard lock(s->mutex);
					s->queue.clear();
					s->connected = true;
					s->waitKey = true;
				}
				s->state = StateConnected;
				blog(LOG_INFO, "[bread-srt] '%s': connected to %s", obs_output_get_name(s->output),
				     s->url.c_str());
				sendUntilFailure(s, context, video, audio);
				{
					std::lock_guard lock(s->mutex);
					s->connected = false;
					s->queue.clear();
				}
			} else if (!s->stopping) {
				blog(LOG_WARNING, "[bread-srt] '%s': cannot open %s: %s",
				     obs_output_get_name(s->output), s->url.c_str(), errorText(result).c_str());
			}
		} else if (!s->stopping) {
			blog(LOG_WARNING,
			     "[bread-srt] '%s': cannot add the video stream; the encoder's codec is unknown",
			     obs_output_get_name(s->output));
		}

		if (context) {
			if (context->pb) {
				avio_closep(&context->pb);
			}
			avformat_free_context(context);
		}
		if (s->stopping) {
			break;
		}

		/* a caller tries again after two seconds; a listener listens again almost at once */
		s->state = StateRetrying;
		std::unique_lock lock(s->mutex);
		s->wake.wait_for(lock, s->listener ? std::chrono::milliseconds(200) : std::chrono::seconds(2),
				 [s] { return s->stopping.load(); });
	}
	s->state = StateOff;
}

void getState(void *data, calldata_t *call)
{
	calldata_set_int(call, "state", static_cast<SrtOutput *>(data)->state.load());
}

const char *srtName(void *)
{
	return obs_module_text("SrtOutput");
}

void *srtCreate(obs_data_t *, obs_output_t *output)
{
	auto *s = new SrtOutput();
	s->output = output;
	proc_handler_add(obs_output_get_proc_handler(output), "void get_state(out int state)", getState, s);
	return s;
}

void stopWorker(SrtOutput *s)
{
	s->stopping = true;
	s->wake.notify_all();
	if (s->worker.joinable()) {
		s->worker.join();
	}
}

void srtDestroy(void *data)
{
	auto *s = static_cast<SrtOutput *>(data);
	stopWorker(s);
	delete s;
}

bool srtStart(void *data)
{
	auto *s = static_cast<SrtOutput *>(data);

	s->bytes = 0;
	obs_data_t *settings = obs_output_get_settings(s->output);
	s->url = obs_data_get_string(settings, "url");
	s->options.clear();
	obs_data_t *options = obs_data_get_obj(settings, "options");
	for (obs_data_item_t *item = obs_data_first(options); item; obs_data_item_next(&item)) {
		s->options.emplace_back(obs_data_item_get_name(item), obs_data_item_get_string(item));
	}
	obs_data_release(options);
	obs_data_release(settings);

	s->listener = false;
	for (const auto &[key, value] : s->options) {
		if (key == "mode" && value == "listener") {
			s->listener = true;
		}
	}

	if (s->url.empty() || !obs_output_can_begin_data_capture(s->output, 0) ||
	    !obs_output_initialize_encoders(s->output, 0)) {
		return false;
	}

	s->stopping = false;
	s->worker = std::thread(run, s);
	if (!obs_output_begin_data_capture(s->output, 0)) {
		stopWorker(s);
		return false;
	}
	return true;
}

void srtStop(void *data, uint64_t)
{
	auto *s = static_cast<SrtOutput *>(data);
	stopWorker(s);
	obs_output_end_data_capture(s->output);
}

/* OBS's encoder threads: never wait on the network here */
void srtPacket(void *data, encoder_packet *packet)
{
	auto *s = static_cast<SrtOutput *>(data);
	if (!packet) {
		obs_output_signal_stop(s->output, OBS_OUTPUT_ENCODE_ERROR);
		return;
	}

	const bool video = packet->type == OBS_ENCODER_VIDEO;
	std::lock_guard lock(s->mutex);
	if (!s->connected) {
		return;
	}
	if (s->waitKey) {
		if (!video || !packet->keyframe) {
			return;
		}
		s->waitKey = false;
	}
	if (s->queue.size() >= MaxQueued) {
		blog(LOG_WARNING, "[bread-srt] '%s': the network fell behind; starting again at the next keyframe",
		     obs_output_get_name(s->output));
		s->queue.clear();
		s->waitKey = true;
		return;
	}

	Packet copy;
	copy.data.assign(packet->data, packet->data + packet->size);
	copy.pts = packet->pts;
	copy.dts = packet->dts;
	copy.timebase = AVRational{packet->timebase_num, packet->timebase_den};
	copy.video = video;
	copy.keyframe = packet->keyframe;
	s->queue.push_back(std::move(copy));
	s->wake.notify_one();
}

uint64_t srtBytes(void *data)
{
	return static_cast<SrtOutput *>(data)->bytes.load();
}

} // namespace

bool obs_module_load(void)
{
	obs_output_info info = {};
	info.id = "bread_srt_output";
	info.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED;
	info.encoded_video_codecs = "h264;hevc";
	info.encoded_audio_codecs = "aac";
	info.get_name = srtName;
	info.create = srtCreate;
	info.destroy = srtDestroy;
	info.start = srtStart;
	info.stop = srtStop;
	info.encoded_packet = srtPacket;
	info.get_total_bytes = srtBytes;
	obs_register_output(&info);
	return true;
}
