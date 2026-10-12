#pragma once

#include <obs-module.h>

#include <QString>

#include <cstdint>

namespace desk {

inline constexpr const char *InputsDockId = "breadDeskInputs";
inline constexpr const char *MixerDockId = "breadDeskMixer";

/* the values OBS's multiview uses */
inline constexpr uint32_t ProgramColor = 0xFFD00000;
inline constexpr uint32_t PreviewColor = 0xFF00D000;
inline constexpr uint32_t IdleColor = 0xFF303030;

inline QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

} // namespace desk