/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

/* BreadStudio: after DistroAV 6.2.2 (9b1635f), src/ndi-source.cpp, cut down to what the desk's NDI tab
 * sets: the source, Low Bandwidth, Audio Only and Increase Buffer Size. Left out: PTZ, behaviour on
 * hide, timeout, sync modes, frame sync, the hardware-acceleration request, the alpha filter, colour
 * settings, the web control URL, and DistroAV's tally, which follows OBS's "active" and "showing";
 * tally follows breadstudio/shared/OnAir.hpp instead (network inputs spec §8). */

#include "Ndi.hpp"

namespace {

const char *ndi_get_name(void *)
{
	return obs_module_text("NDISource");
}

void *ndi_create(obs_data_t *, obs_source_t *source)
{
	return source;
}

void ndi_destroy(void *) {}

} // namespace

obs_source_info ndiSourceInfo()
{
	obs_source_info info = {};
	info.id = "ndi_source"; /* DistroAV's, so a collection made with DistroAV opens here */
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name = ndi_get_name;
	info.create = ndi_create;
	info.destroy = ndi_destroy;
	info.icon_type = OBS_ICON_TYPE_CAMERA;
	return info;
}