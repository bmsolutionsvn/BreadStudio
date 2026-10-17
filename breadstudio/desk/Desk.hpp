#pragma once

#include <obs-module.h>

#include <QString>

#include <cstdint>

namespace desk {

class Project;

inline constexpr const char *InputsDockId = "breadDeskInputs";
inline constexpr const char *MixerDockId = "breadDeskMixer";
inline constexpr const char *FooterId = "breadDeskFooter";

/* the values OBS's multiview uses */
inline constexpr uint32_t ProgramColor = 0xFFD00000;
inline constexpr uint32_t PreviewColor = 0xFF00D000;
inline constexpr uint32_t IdleColor = 0xFF303030;

inline QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

/* call after anything changes the set, order or names of inputs; defined in desk.cpp */
void inputsChanged();

/* the one Project the desk has open (P2), made in obs_module_load and released at EXIT; defined
 * in desk.cpp. Null before the module loads and after it exits. */
Project *project();

/* The desk's own edit marks the open Project changed and re-titles the window with its dot (P9).
 * OBS's save callback marks the changedness itself, so a desk write the Project absorbs does not
 * come through here. Defined in desk.cpp. */
void markProjectChanged();

} // namespace desk
