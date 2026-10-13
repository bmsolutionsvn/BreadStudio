#pragma once

#include <obs.hpp>

namespace desk {

/* One hidden scene held on a spare output channel. The base of every input with AFV off
 * is an item in it, placed outside the canvas, so OBS mixes that base whatever is on
 * Program. libobs counts a source once when it is also on Program (obs-audio.c,
 * push_audio_tree2). */
class AudioBed {
public:
	void rebuild(); /* after a scene collection has loaded */
	void sync();    /* after inputs or AFV flags changed */
	void clear();   /* before a scene collection unloads; safe to call twice */

	/* audio follows video: heard only while the input is on Program */
	static bool afv(obs_source_t *input);
	void setAfv(obs_source_t *input, bool on);

private:
	OBSSceneAutoRelease scene;
	uint32_t channel = 0;
};

AudioBed &bed();

} // namespace desk
