/* The NDI plugin: loads NDI's DLL, registers the NDI source, and finds senders (network inputs spec §7). */
#include "Ndi.hpp"

#include <util/platform.h>

#include <mutex>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bread-ndi", "en-US")

const NDIlib_v6 *ndiLib = nullptr;

namespace {

HMODULE library = nullptr;
std::string status;

/* One finder for the life of the plugin, so the list is ready whenever it is asked for. */
std::mutex finderMutex;
NDIlib_find_instance_t finder = nullptr;
std::string extraIps; /* "Also search" (spec §4.1): NDI's comma-separated list */

std::string finderFile()
{
	char *path = obs_module_config_path("finder.json");
	std::string result = path ? path : "";
	bfree(path);
	return result;
}

void loadExtraIps()
{
	obs_data_t *saved = obs_data_create_from_json_file_safe(finderFile().c_str(), "bak");
	if (saved) {
		extraIps = obs_data_get_string(saved, "extra_ips");
		obs_data_release(saved);
	}
}

void saveExtraIps()
{
	char *folder = obs_module_config_path("");
	if (folder) {
		os_mkdirs(folder);
		bfree(folder);
	}
	obs_data_t *saved = obs_data_create();
	obs_data_set_string(saved, "extra_ips", extraIps.c_str());
	obs_data_save_json_safe(saved, finderFile().c_str(), "tmp", "bak");
	obs_data_release(saved);
}

void getExtraIps(void *, calldata_t *data)
{
	std::lock_guard lock(finderMutex);
	calldata_set_string(data, "ips", extraIps.c_str());
}

/* the next question makes a new finder that asks these addresses too */
void setExtraIps(void *, calldata_t *data)
{
	const char *ips = calldata_string(data, "ips");
	std::lock_guard lock(finderMutex);
	extraIps = ips ? ips : "";
	saveExtraIps();
	if (finder && ndiLib) {
		ndiLib->find_destroy(finder);
	}
	finder = nullptr;
}

/* why NDI is off, for the Outputs window's greyed NDI column */
void getNdiStatus(void *, calldata_t *data)
{
	calldata_set_string(data, "status", status.c_str());
}

} // namespace

const std::string &ndiStatus()
{
	return status;
}

std::vector<std::string> ndiSources()
{
	std::vector<std::string> names;
	std::lock_guard lock(finderMutex);
	if (!ndiLib) {
		return names;
	}
	if (!finder) {
		NDIlib_find_create_t desc;
		desc.show_local_sources = true;
		desc.p_extra_ips = extraIps.empty() ? nullptr : extraIps.c_str();
		finder = ndiLib->find_create_v2(&desc);
	}

	uint32_t count = 0;
	const NDIlib_source_t *found = finder ? ndiLib->find_get_current_sources(finder, &count) : nullptr;
	for (uint32_t i = 0; i < count; i++) {
		names.emplace_back(found[i].p_ndi_name);
	}
	return names;
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

	/* the output only exists while NDI is on, so the Outputs window greys the NDI column otherwise */
	if (ndiLib) {
		const obs_output_info output = ndiOutputInfo();
		obs_register_output(&output);
	}

	loadExtraIps();
	/* the desk sets "Also search" through these, so it needs nothing of NDI (spec §6.1) */
	proc_handler_t *procs = obs_get_proc_handler();
	proc_handler_add(procs, "void bread_ndi_get_extra_ips(out string ips)", getExtraIps, nullptr);
	proc_handler_add(procs, "void bread_ndi_set_extra_ips(in string ips)", setExtraIps, nullptr);
	proc_handler_add(procs, "void bread_ndi_status(out string status)", getNdiStatus, nullptr);
	/* the finder needs a second or two: start it now, so the list is full when it is first shown */
	ndiSources();
	return true;
}

/* after every source is gone */
void obs_module_unload(void)
{
	{
		std::lock_guard lock(finderMutex);
		if (finder && ndiLib) {
			ndiLib->find_destroy(finder);
		}
		finder = nullptr;
	}
	if (ndiLib) {
		ndiLib->destroy();
		ndiLib = nullptr;
	}
	if (library) {
		FreeLibrary(library);
		library = nullptr;
	}
}
