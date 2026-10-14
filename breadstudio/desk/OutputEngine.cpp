#include "OutputEngine.hpp"
#include "ProgramSource.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <media-io/video-io.h>

#include <algorithm>
#include <atomic>
#include <cstring>

namespace desk {

static const char *SrtType = "bread_srt_output";
static const char *NdiType = "bread_ndi_output";
static const char *OmtType = "bread_omt_output";
static const char *RtmpType = "rtmp_output";

/* how each destination last stopped: OBS_OUTPUT_SUCCESS, or the failure; written by OBS's threads */
static std::array<std::atomic<int>, DestinationCount> lastStop;

static void onRtmpStop(void *param, calldata_t *call)
{
	lastStop[(size_t)(uintptr_t)param] = (int)calldata_int(call, "code");
}

static std::string senderName(int output)
{
	return "BreadStudio Output " + std::to_string(output + 1);
}

/* the best video encoder this PC has; HEVC falls back to H.264 (outputs spec §4.3) */
static const char *videoEncoderId(VideoCodec codec)
{
	static const char *hevc[] = {"h265_texture_amf", "obs_nvenc_hevc_tex", "obs_qsv11_hevc"};
	static const char *h264[] = {"h264_texture_amf", "obs_nvenc_h264_tex", "obs_qsv11_v2", "obs_x264"};
	if (codec == VideoCodec::Hevc) {
		for (const char *id : hevc) {
			if (obs_get_encoder_codec(id)) {
				return id;
			}
		}
	}
	for (const char *id : h264) {
		if (obs_get_encoder_codec(id)) {
			return id;
		}
	}
	return nullptr;
}

static int procInt(obs_output_t *output, const char *proc, const char *name, int fallback)
{
	calldata_t call;
	calldata_init(&call);
	int value = fallback;
	if (proc_handler_call(obs_output_get_proc_handler(output), proc, &call)) {
		value = (int)calldata_int(&call, name);
	}
	calldata_free(&call);
	return value;
}

static void stopOutput(OBSOutputAutoRelease &output)
{
	if (output) {
		obs_output_force_stop(output);
		output = nullptr;
	}
}

OutputEngine &OutputEngine::instance()
{
	static OutputEngine engine;
	return engine;
}

bool OutputEngine::hasOutputType(const char *id)
{
	return obs_output_get_display_name(id) != nullptr;
}

void OutputEngine::apply(const OutputsConfig &next)
{
	reconcile(next, live);
}

void OutputEngine::setStreaming(bool on)
{
	reconcile(wanted, on);
}

video_t *OutputEngine::videoOf(int output) const
{
	if (output == 0) {
		return obs_get_video();
	}
	const Running &r = running[(size_t)output];
	return r.canvas ? obs_canvas_get_video(r.canvas) : nullptr;
}

OBSSource OutputEngine::sourceFor(int output) const
{
	if (detached) {
		return OBSSource();
	}
	const OutputConfig &config = wanted.outputs[(size_t)output];
	switch (config.source) {
	case OutputSource::Program:
		return OBSSource(program.Get());
	case OutputSource::Preview:
		if (obs_frontend_preview_program_mode_active()) {
			OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
			return OBSSource(preview.Get());
		}
		return OBSSource(program.Get()); /* with Studio Mode off, Preview is Program */
	case OutputSource::MultiView:
		return OBSSource(); /* Plan E fills this in */
	case OutputSource::Input: {
		OBSSourceAutoRelease input = obs_get_source_by_uuid(config.input.c_str());
		if (input && !obs_source_removed(input)) {
			return OBSSource(input.Get());
		}
		return OBSSource(); /* deleted: black, the senders go on (outputs spec §7) */
	}
	}
	return OBSSource();
}

void OutputEngine::show(int output)
{
	Running &r = running[(size_t)output];
	if (!r.canvas) {
		r.shown = nullptr;
		return;
	}
	OBSSource next = sourceFor(output);
	if (next.Get() != r.shown.Get()) {
		obs_canvas_set_channel(r.canvas, 0, next);
		r.shown = next;
	}
}

void OutputEngine::refreshSources()
{
	detached = false;
	for (int i = 0; i < OutputCount; i++) {
		show(i);
	}
}

void OutputEngine::detachSources()
{
	detached = true;
	for (int i = 0; i < OutputCount; i++) {
		show(i);
	}
}

void OutputEngine::startSenders(int output)
{
	Running &r = running[(size_t)output];
	const OutputConfig &config = wanted.outputs[(size_t)output];
	const OutputRun run = outputRun(output, wanted, live, 0, 0);
	video_t *video = videoOf(output);
	if (!video) {
		return;
	}
	const std::string name = senderName(output);

	if (run.encoders && !r.video) {
		const char *id = videoEncoderId(config.codec);
		if (id) {
			OBSDataAutoRelease settings = obs_data_create();
			obs_data_set_string(settings, "rate_control", "CBR");
			obs_data_set_int(settings, "bitrate", config.bitrateKbps);
			obs_data_set_int(settings, "keyint_sec", 2);
			r.video = obs_video_encoder_create(id, (name + " video").c_str(), settings, nullptr);
			if (r.video) {
				obs_encoder_set_video(r.video, video);
			}
		} else {
			blog(LOG_WARNING, "[bread-desk] %s: no video encoder on this PC", name.c_str());
		}
		OBSDataAutoRelease audioSettings = obs_data_create();
		obs_data_set_int(audioSettings, "bitrate", 160);
		r.audio = obs_audio_encoder_create("ffmpeg_aac", (name + " audio").c_str(), audioSettings,
						   (size_t)config.bus, nullptr);
		if (r.audio) {
			obs_encoder_set_audio(r.audio, obs_get_audio());
		}
	}

	if (run.srt && !r.srt && r.video && r.audio && hasOutputType(SrtType)) {
		const SrtOutputTarget target = srtOutputTarget(config.srtForm);
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "url", target.url.c_str());
		OBSDataAutoRelease options = obs_data_create();
		for (const auto &[key, value] : target.options) {
			obs_data_set_string(options, key.c_str(), value.c_str());
		}
		obs_data_set_obj(settings, "options", options);
		r.srt = obs_output_create(SrtType, (name + " SRT").c_str(), settings, nullptr);
		if (r.srt) {
			obs_output_set_video_encoder(r.srt, r.video);
			obs_output_set_audio_encoder(r.srt, r.audio, 0);
			obs_output_start(r.srt);
		}
	}

	/* raw senders: Plan F's NDI and OMT outputs */
	auto startRaw = [&](OBSOutputAutoRelease &slot, const char *type, const char *mode) {
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "name", name.c_str());
		if (mode) {
			obs_data_set_string(settings, "mode", mode);
		}
		slot = obs_output_create(type, (name + " " + type).c_str(), settings, nullptr);
		if (slot) {
			obs_output_set_media(slot, video, obs_get_audio());
			obs_output_set_mixer(slot, (size_t)config.bus);
			obs_output_start(slot);
		}
	};
	if (run.ndi && !r.ndi && hasOutputType(NdiType)) {
		startRaw(r.ndi, NdiType, nullptr);
	}
	if (run.omt != OmtMode::Off && !r.omt && hasOutputType(OmtType)) {
		static const char *modes[] = {"", "vmx", "hevc", "h264"};
		startRaw(r.omt, OmtType, modes[(int)run.omt]);
	}
}

void OutputEngine::startDestination(int destination)
{
	const Destination &d = wanted.destinations[(size_t)destination];
	Running &r = running[(size_t)d.output];
	if (rtmp[(size_t)destination] || !r.video || !r.audio) {
		return;
	}

	const std::string name = "BreadStudio Stream " + std::to_string(destination + 1);
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "server", d.server.c_str());
	obs_data_set_string(settings, "key", d.key.c_str());
	services[(size_t)destination] = obs_service_create_private("rtmp_custom", name.c_str(), settings);

	OBSOutputAutoRelease output = obs_output_create(RtmpType, name.c_str(), nullptr, nullptr);
	if (!output || !services[(size_t)destination]) {
		return;
	}
	obs_output_set_service(output, services[(size_t)destination]);
	obs_output_set_video_encoder(output, r.video);
	obs_output_set_audio_encoder(output, r.audio, 0);
	/* as OBS: try again every two seconds; enough tries for a long outage */
	obs_output_set_reconnect_settings(output, 1000, 2);
	signal_handler_connect(obs_output_get_signal_handler(output), "stop", onRtmpStop,
			       (void *)(uintptr_t)destination);
	lastStop[(size_t)destination] = OBS_OUTPUT_SUCCESS;
	obs_output_start(output);
	rtmp[(size_t)destination] = std::move(output);
}

void OutputEngine::stopDestination(int destination)
{
	OBSOutputAutoRelease &output = rtmp[(size_t)destination];
	if (output) {
		signal_handler_disconnect(obs_output_get_signal_handler(output), "stop", onRtmpStop,
					  (void *)(uintptr_t)destination);
	}
	stopOutput(output);
	services[(size_t)destination] = nullptr;
}

void OutputEngine::reconcile(const OutputsConfig &next, bool nextLive)
{
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	const int programWidth = (int)ovi.output_width;
	const int programHeight = (int)ovi.output_height;

	std::array<OutputSteps, OutputCount> steps;
	for (int i = 0; i < OutputCount; i++) {
		steps[(size_t)i] = outputSteps(i, wanted, live, next, nextLive, programWidth, programHeight);
	}

	/* stop from the outside in: destinations, senders, encoders, canvases */
	for (int d = 0; d < DestinationCount; d++) {
		bool restart = false;
		for (const OutputSteps &s : steps) {
			restart = restart || s.rtmp[(size_t)d];
		}
		if (restart) {
			stopDestination(d);
		}
	}
	for (int i = 0; i < OutputCount; i++) {
		Running &r = running[(size_t)i];
		const OutputSteps &s = steps[(size_t)i];
		if (s.ndi) {
			stopOutput(r.ndi);
		}
		if (s.omt) {
			stopOutput(r.omt);
		}
		if (s.srt) {
			stopOutput(r.srt);
		}
		if (s.encoders) {
			r.video = nullptr;
			r.audio = nullptr;
		}
		if (s.canvas && r.canvas) {
			obs_canvas_set_channel(r.canvas, 0, nullptr);
			obs_canvas_remove(r.canvas);
			r.canvas = nullptr;
			r.shown = nullptr;
		}
	}

	wanted = next;
	live = nextLive;

	if (!program) {
		program = obs_source_create_private(ProgramSourceId, "BreadStudio Program", nullptr);
	}

	/* start from the inside out */
	for (int i = 0; i < OutputCount; i++) {
		Running &r = running[(size_t)i];
		const OutputRun run = outputRun(i, wanted, live, programWidth, programHeight);
		if (run.canvas && !r.canvas) {
			obs_video_info canvasInfo = ovi;
			/* inputs are laid out for the main mix's base size; the output size scales it */
			canvasInfo.output_width = (uint32_t)run.width;
			canvasInfo.output_height = (uint32_t)run.height;
			canvasInfo.output_format = run.alpha ? VIDEO_FORMAT_BGRA : VIDEO_FORMAT_NV12;
			const std::string name = senderName(i);
			r.canvas = obs_canvas_create_private(name.c_str(), &canvasInfo, ACTIVATE | EPHEMERAL);
			if (!r.canvas) {
				blog(LOG_WARNING, "[bread-desk] %s: OBS could not make its canvas", name.c_str());
			}
		}
		show(i);
		startSenders(i);
	}
	for (int d = 0; d < DestinationCount; d++) {
		const Destination &destination = wanted.destinations[(size_t)d];
		if (live && destination.on &&
		    outputRun(destination.output, wanted, live, programWidth, programHeight).rtmp[(size_t)d]) {
			startDestination(d);
		}
	}
}

void OutputEngine::shutdown()
{
	for (int d = 0; d < DestinationCount; d++) {
		stopDestination(d);
	}
	for (Running &r : running) {
		stopOutput(r.ndi);
		stopOutput(r.omt);
		stopOutput(r.srt);
		r.video = nullptr;
		r.audio = nullptr;
		if (r.canvas) {
			obs_canvas_set_channel(r.canvas, 0, nullptr);
			obs_canvas_remove(r.canvas);
			r.canvas = nullptr;
		}
		r.shown = nullptr;
	}
	program = nullptr;
	live = false;
}

static SenderStatus srtStatus(obs_output_t *output)
{
	SenderStatus status;
	if (!output) {
		return status;
	}
	static const SenderState states[] = {SenderState::Off, SenderState::Waiting, SenderState::Connected,
					     SenderState::Retrying};
	const int state = procInt(output, "get_state", "state", 0);
	status.state = state >= 0 && state <= 3 ? states[state] : SenderState::Off;
	status.bytes = obs_output_get_total_bytes(output);
	return status;
}

static SenderStatus rawStatus(obs_output_t *output)
{
	SenderStatus status;
	if (output) {
		status.state = obs_output_active(output) ? SenderState::Live : SenderState::Failed;
		status.receivers = procInt(output, "get_receivers", "count", -1);
	}
	return status;
}

OutputStatus OutputEngine::status(int output) const
{
	const Running &r = running[(size_t)output];
	OutputStatus status;
	video_t *video = videoOf(output);
	const bool sending = output == 0 ? (r.video || r.ndi || r.omt) : r.canvas.Get() != nullptr;
	if (video && sending) {
		status.width = (int)video_output_get_width(video);
		status.height = (int)video_output_get_height(video);
		status.skipped = video_output_get_skipped_frames(video);
	}
	if (r.video) {
		const char *name = obs_encoder_get_display_name(obs_encoder_get_id(r.video));
		status.encoder = name ? name : obs_encoder_get_id(r.video);
	}
	status.srt = srtStatus(r.srt);
	status.ndi = rawStatus(r.ndi);
	status.omt = rawStatus(r.omt);
	return status;
}

SenderStatus OutputEngine::destinationStatus(int destination) const
{
	SenderStatus status;
	obs_output_t *output = rtmp[(size_t)destination];
	if (!output) {
		return status;
	}
	if (obs_output_reconnecting(output)) {
		status.state = SenderState::Retrying;
	} else if (obs_output_active(output)) {
		status.state = SenderState::Live;
	} else if (lastStop[(size_t)destination] != OBS_OUTPUT_SUCCESS) {
		status.state = SenderState::Failed;
	} else {
		status.state = SenderState::Waiting;
	}
	status.bytes = obs_output_get_total_bytes(output);
	status.dropped = obs_output_get_frames_dropped(output);
	return status;
}

/* ---- What is saved, in the scene collection (outputs spec §8) ---- */

static const char *sourceNames[] = {"program", "preview", "multiview", "input"};
static const char *omtNames[] = {"off", "vmx", "hevc", "h264"};

template<typename E, size_t N> static E fromName(const char *(&names)[N], const char *name, E fallback)
{
	for (size_t i = 0; i < N; i++) {
		if (strcmp(names[i], name) == 0) {
			return (E)i;
		}
	}
	return fallback;
}

void OutputEngine::save(obs_data_t *data) const
{
	OBSDataAutoRelease saved = obs_data_create();

	OBSDataArrayAutoRelease outputs = obs_data_array_create();
	for (const OutputConfig &o : wanted.outputs) {
		OBSDataAutoRelease item = obs_data_create();
		obs_data_set_string(item, "source", sourceNames[(int)o.source]);
		obs_data_set_string(item, "input", o.input.c_str());
		obs_data_set_int(item, "width", o.width);
		obs_data_set_int(item, "height", o.height);
		obs_data_set_bool(item, "alpha", o.alpha);
		obs_data_set_int(item, "bus", o.bus);
		obs_data_set_bool(item, "ndi", o.ndi);
		obs_data_set_string(item, "omt", omtNames[(int)o.omt]);
		obs_data_set_bool(item, "srt", o.srt);
		obs_data_set_string(item, "srt_type", streamTypeId(o.srtForm.type));
		obs_data_set_string(item, "srt_host", o.srtForm.host.c_str());
		obs_data_set_int(item, "srt_port", o.srtForm.port);
		obs_data_set_int(item, "srt_latency_ms", o.srtForm.latencyMs);
		obs_data_set_string(item, "srt_passphrase", o.srtForm.passphrase.c_str());
		obs_data_set_int(item, "srt_key_length", o.srtForm.keyLength);
		obs_data_set_string(item, "srt_stream_id", o.srtForm.streamId.c_str());
		obs_data_set_string(item, "codec", o.codec == VideoCodec::Hevc ? "hevc" : "h264");
		obs_data_set_int(item, "bitrate", o.bitrateKbps);
		obs_data_array_push_back(outputs, item);
	}
	obs_data_set_array(saved, "outputs", outputs);

	OBSDataArrayAutoRelease destinations = obs_data_array_create();
	for (const Destination &d : wanted.destinations) {
		OBSDataAutoRelease item = obs_data_create();
		obs_data_set_bool(item, "on", d.on);
		obs_data_set_int(item, "output", d.output);
		obs_data_set_string(item, "server", d.server.c_str());
		obs_data_set_string(item, "key", d.key.c_str());
		obs_data_array_push_back(destinations, item);
	}
	obs_data_set_array(saved, "destinations", destinations);

	obs_data_set_obj(data, "bread_outputs", saved);
}

OutputsConfig OutputEngine::load(obs_data_t *data) const
{
	OutputsConfig config; /* a collection made before outputs existed: all off (outputs spec §8) */
	OBSDataAutoRelease saved = obs_data_get_obj(data, "bread_outputs");
	if (!saved) {
		return config;
	}

	OBSDataArrayAutoRelease outputs = obs_data_get_array(saved, "outputs");
	for (size_t i = 0; i < OutputCount && i < obs_data_array_count(outputs); i++) {
		OBSDataAutoRelease item = obs_data_array_item(outputs, i);
		OutputConfig &o = config.outputs[i];
		o.source = i == 0 ? OutputSource::Program
				  : fromName(sourceNames, obs_data_get_string(item, "source"), OutputSource::Program);
		o.input = obs_data_get_string(item, "input");
		o.width = (int)obs_data_get_int(item, "width");
		o.height = (int)obs_data_get_int(item, "height");
		o.alpha = i > 0 && obs_data_get_bool(item, "alpha");
		o.bus = std::clamp((int)obs_data_get_int(item, "bus"), 0, 5);
		o.ndi = obs_data_get_bool(item, "ndi");
		o.omt = fromName(omtNames, obs_data_get_string(item, "omt"), OmtMode::Off);
		o.srt = obs_data_get_bool(item, "srt");
		o.srtForm.type = streamTypeFromId(obs_data_get_string(item, "srt_type"));
		if (!isSrt(o.srtForm.type)) {
			o.srtForm.type = StreamType::SrtCaller;
		}
		o.srtForm.host = obs_data_get_string(item, "srt_host");
		o.srtForm.port = (int)obs_data_get_int(item, "srt_port");
		o.srtForm.latencyMs = (int)obs_data_get_int(item, "srt_latency_ms");
		o.srtForm.passphrase = obs_data_get_string(item, "srt_passphrase");
		o.srtForm.keyLength = (int)obs_data_get_int(item, "srt_key_length");
		o.srtForm.streamId = obs_data_get_string(item, "srt_stream_id");
		o.codec = strcmp(obs_data_get_string(item, "codec"), "hevc") == 0 ? VideoCodec::Hevc : VideoCodec::H264;
		o.bitrateKbps = std::max(500, (int)obs_data_get_int(item, "bitrate"));
		/* a sender whose settings cannot be used is off rather than failing */
		if (o.srt && streamProblem(o.srtForm)) {
			o.srt = false;
		}
	}

	OBSDataArrayAutoRelease destinations = obs_data_get_array(saved, "destinations");
	for (size_t i = 0; i < DestinationCount && i < obs_data_array_count(destinations); i++) {
		OBSDataAutoRelease item = obs_data_array_item(destinations, i);
		Destination &d = config.destinations[i];
		d.on = obs_data_get_bool(item, "on");
		d.output = std::clamp((int)obs_data_get_int(item, "output"), 0, OutputCount - 1);
		d.server = obs_data_get_string(item, "server");
		d.key = obs_data_get_string(item, "key");
	}
	return config;
}

} // namespace desk
