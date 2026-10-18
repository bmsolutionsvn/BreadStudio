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

/* BreadStudio: after load_ndilib() in DistroAV 6.2.2 (9b1635f), src/plugin-main.cpp, rewritten on
 * LoadLibraryExW so that neither the plugin nor the test sender needs Qt. */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <Processing.NDI.Lib.h>
#include <Processing.NDI.DynamicLoad.h>

#include <string>
#include <vector>

/* NDI's DLL from the folder that holds the program first (a developer may drop one there; releases
 * never ship it), then from NDI's runtime folder (network inputs spec §7.2). Null when neither has a
 * usable one. `module` is the handle to free at the end; `from` the file that was loaded. */
inline const NDIlib_v6 *loadNdi(HMODULE &module, std::wstring &from)
{
	std::vector<std::wstring> folders;

	wchar_t exe[MAX_PATH] = {};
	const DWORD length = GetModuleFileNameW(nullptr, exe, MAX_PATH);
	if (length > 0 && length < MAX_PATH) {
		std::wstring folder(exe, length);
		folders.push_back(folder.substr(0, folder.find_last_of(L"\\/") + 1));
	}

	wchar_t runtime[MAX_PATH] = {};
	const DWORD runtimeLength = GetEnvironmentVariableW(L"NDI_RUNTIME_DIR_V6", runtime, MAX_PATH);
	if (runtimeLength > 0 && runtimeLength < MAX_PATH) {
		folders.push_back(std::wstring(runtime, runtimeLength) + L"\\");
	}

	for (const std::wstring &folder : folders) {
		const std::wstring path = folder + L"Processing.NDI.Lib.x64.dll";
		module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
		if (!module) {
			continue;
		}
		auto load = reinterpret_cast<const NDIlib_v6 *(*)(void)>(GetProcAddress(module, "NDIlib_v6_load"));
		const NDIlib_v6 *lib = load ? load() : nullptr;
		if (lib) {
			from = path;
			return lib;
		}
		FreeLibrary(module);
		module = nullptr;
	}
	return nullptr;
}