#include "AudioBed.hpp"
#include "InputModel.hpp"
#include "MultiviewSource.hpp"

#include <obs-frontend-api.h>
#include <graphics/vec2.h>

#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace desk {

static const char *AfvKey = "bread_afv";
static const char *AudioOnKey = "bread_audio_on";

/* OBS's frontend uses channels 0 to 6. The bed takes the highest free one. */
static const uint32_t FirstChannel = MAX_CHANNELS - 1;
static const uint32_t LastChannel = 7;

/* far outside any canvas, so the bed draws nothing */
static const float OffCanvas = 100000.0f;

AudioBed &bed()
{
	static AudioBed instance;
	return instance;
}

bool AudioBed::afv(obs_source_t *input)
{
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	if (obs_data_has_user_value(settings, AfvKey)) {
		return obs_data_get_bool(settings, AfvKey);
	}

	/* default: an input with a picture follows video; an audio-only input is always on */
	OBSSource base = baseSource(input);
	return base && (obs_source_get_output_flags(base) & OBS_SOURCE_VIDEO) != 0;
}

void AudioBed::setAfv(obs_source_t *input, bool on)
{
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	obs_data_set_bool(settings, AfvKey, on);
	sync();
	obs_frontend_save();
}

bool AudioBed::audioOn(obs_source_t *input)
{
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	if (obs_data_has_user_value(settings, AudioOnKey)) {
		return obs_data_get_bool(settings, AudioOnKey);
	}
	return true;
}

void AudioBed::setAudioOn(obs_source_t *input, bool on)
{
	OBSDataAutoRelease settings = obs_source_get_private_settings(input);
	obs_data_set_bool(settings, AudioOnKey, on);
	sync();
	obs_frontend_save();
}

void AudioBed::rebuild()
{
	clear();

	for (uint32_t candidate = FirstChannel; candidate >= LastChannel; candidate--) {
		OBSSourceAutoRelease taken = obs_get_output_source(candidate);
		if (!taken) {
			channel = candidate;
			break;
		}
	}

	if (channel == 0) {
		blog(LOG_ERROR, "[bread-desk] no free output channel for the audio bed; AFV off will not work");
		return;
	}
	if (channel != FirstChannel) {
		blog(LOG_INFO, "[bread-desk] audio bed is on output channel %u", channel);
	}

	scene = obs_scene_create_private("Bread audio bed");
	obs_set_output_source(channel, obs_scene_get_source(scene));
	sync();
}

void AudioBed::clear()
{
	if (!scene) {
		return;
	}

	/* OBS may already have emptied every channel; only take down what is still ours */
	OBSSourceAutoRelease current = obs_get_output_source(channel);
	if (current.Get() == obs_scene_get_source(scene)) {
		obs_set_output_source(channel, nullptr);
	}

	scene = nullptr;
	channel = 0;
}

/* true if `program` is a scene showing the desk's own MultiView, so every input it monitors is
 * on air (and so their audio is heard, with AFV on) */
static bool programShowsMultiview(obs_source_t *program)
{
	obs_scene_t *scene = program ? obs_scene_from_source(program) : nullptr;
	if (!scene) {
		return false;
	}

	bool found = false;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			obs_source_t *source = obs_sceneitem_get_source(item);
			if (source && strcmp(obs_source_get_id(source), MultiviewSourceId) == 0) {
				*static_cast<bool *>(param) = true;
				return false;
			}
			return true;
		},
		&found);
	return found;
}

void AudioBed::sync()
{
	/* drive each base's mute from AFV and Program: heard only if the input's own switch is
	 * on and, when AFV is on, the input is on Program. This runs even with no bed: the bed
	 * needs a free output channel, AFV does not. */
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	/* vMix's rule: an input is on Program when it is the Program scene, or when the Program
	 * scene is showing the desk's own MultiView, which puts every input it monitors on air. */
	const bool multiviewOnProgram = programShowsMultiview(program);
	for (OBSSource &input : inputs()) {
		OBSSource base = baseSource(input);
		if (!base) {
			continue;
		}
		const bool onProgram = program && (input.Get() == program.Get() || multiviewOnProgram);
		obs_source_set_muted(base, !audioOn(input) || (afv(input) && !onProgram));
	}

	if (!scene) {
		return;
	}

	/* the bases that should be in the bed */
	std::unordered_set<std::string> wanted;
	std::vector<OBSSource> wantedSources;
	for (OBSSource &input : inputs()) {
		if (afv(input)) {
			continue;
		}
		OBSSource base = baseSource(input);
		if (!hasAudio(base)) {
			continue;
		}
		if (wanted.insert(obs_source_get_uuid(base)).second) {
			wantedSources.push_back(base);
		}
	}

	/* what is in the bed now */
	struct Found {
		const std::unordered_set<std::string> *wanted;
		std::unordered_set<std::string> present;
		std::vector<OBSSceneItem> extra;
	} found;
	found.wanted = &wanted;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			auto *found = static_cast<Found *>(param);
			std::string uuid = obs_source_get_uuid(obs_sceneitem_get_source(item));
			if (found->wanted->count(uuid)) {
				found->present.insert(uuid);
			} else {
				found->extra.emplace_back(item);
			}
			return true;
		},
		&found);

	/* change only what differs, so an input that stays in the bed is not interrupted */
	for (OBSSceneItem &item : found.extra) {
		obs_sceneitem_remove(item);
	}

	for (OBSSource &base : wantedSources) {
		if (found.present.count(obs_source_get_uuid(base))) {
			continue;
		}
		obs_sceneitem_t *item = obs_scene_add(scene, base);
		struct vec2 pos;
		vec2_set(&pos, OffCanvas, OffCanvas);
		obs_sceneitem_set_pos(item, &pos);
	}
}

} // namespace desk
