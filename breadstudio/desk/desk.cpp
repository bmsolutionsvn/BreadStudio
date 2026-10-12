#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bread-desk", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "BreadStudio desk";
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[bread-desk] loaded");
	return true;
}