/* The GT title input's module: registers the bread_gt_title source. */
#include <obs-module.h>

#include "GtSettings.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("bread-gt", "en-US")

namespace gt {
obs_source_info gtTitleSourceInfo();
}

bool obs_module_load(void)
{
	obs_source_info info = gt::gtTitleSourceInfo();
	obs_register_source(&info);

	blog(LOG_INFO, "[bread-gt] GT title support loaded");
	return true;
}
