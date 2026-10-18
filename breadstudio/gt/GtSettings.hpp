#pragma once

namespace gt {
inline constexpr const char *SourceId = "bread_gt_title";
inline constexpr const char *KeyFile = "file";              /* string: the .gtzip path */
/* object: field -> string, only values differing from the file's */
inline constexpr const char *KeyValues = "values";
inline constexpr const char *KeyStyles = "styles";          /* object: field -> GtStyle object (below) */
/* object: field -> {start,duration,format,reverse,reverse_display} */
inline constexpr const char *KeyCountdowns = "countdowns";
inline constexpr const char *KeyDataSources = "data_sources"; /* string: vMix's DataSourcesXML, kept for project 3 */
inline constexpr const char *ProcInfo = "void gt_info(out string json)";
/* start|pause|stop|reset */
inline constexpr const char *ProcCountdown = "void gt_countdown(in string field, in string action)";
}
