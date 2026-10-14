/* The NDI plugin: loads NDI's DLL, registers the NDI source, and finds senders (network inputs spec §7). */
#include "Ndi.hpp"

#include <util/platform.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bread-ndi", "en-US")

const NDIlib_v6 *ndiLib = nullptr;

namespace {

HMODULE library = nullptr;
std::string status;

} // namespace

const std::string &ndiStatus()
{
	return status;
}

std::vector<std::string> ndiSources()
{
	return {};
}

const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

bool obs_module_load(void)
{
	std::wstring from;
	ndiLib = loadNdi(library, from);
	if (!ndiLib) {
		status = obs_module_text("Status.NoLibrary");
	} else if (!ndiLib->initialize()) {
		/* NDI needs SSE4.2; DistroAV calls this an unsupported CPU */
		status = obs_module_text("Status.NoStart");
		ndiLib->destroy();
		ndiLib = nullptr;
	} else {
		char *path = nullptr;
		os_wcs_to_utf8_ptr(from.c_str(), 0, &path);
		blog(LOG_INFO, "[bread-ndi] NDI %s loaded from %s", ndiLib->version(), path ? path : "?");
		bfree(path);
	}
	if (!status.empty()) {
		blog(LOG_WARNING, "[bread-ndi] %s", status.c_str());
	}

	/* registered even while NDI is off, so NDI inputs in a collection load and save back unchanged */
	const obs_source_info info = ndiSourceInfo();
	obs_register_source(&info);
	return true;
}

/* after every source is gone */
void obs_module_unload(void)
{
	if (ndiLib) {
		ndiLib->destroy();
		ndiLib = nullptr;
	}
	if (library) {
		FreeLibrary(library);
		library = nullptr;
	}
}