#pragma once

#include <obs.hpp>

namespace desk {

/* One hidden scene held on a spare output channel. The base of every input with AFV off
 * is an item in it, placed outside the canvas, so OBS mixes that base whatever is on
 * Program. libobs counts a source once when it is also on Program (obs-audio.c,
 * push_audio_tree2).
 *
 * sync() also mutes each input's base unless it should be heard: its own on/off switch is
 * on and, when AFV is on, the input is on Program. libobs mixes every source with audio
 * whether or not it is rendered, so the bed alone cannot gate an AFV-on input. */
class AudioBed {
public:
	void rebuild(); /* after a scene collection has loaded */
	void sync();    /* after inputs, AFV flags or the on/off switch changed, and on Program change */
	void clear();   /* before a scene collection unloads; safe to call twice */

	/* audio follows video: heard only while the input is on Program */
	static bool afv(obs_source_t *input);
	void setAfv(obs_source_t *input, bool on);

	/* the input's own on/off switch, independent of AFV (default on) */
	static bool audioOn(obs_source_t *input);
	void setAudioOn(obs_source_t *input, bool on);

private:
	OBSSceneAutoRelease scene;
	uint32_t channel = 0;
};

AudioBed &bed();

} // namespace desk
