/* The GT title input's module. The source, the parser and the renderer come in later tasks;
 * for now the module only announces itself. */
#include <obs-module.h>

OBS_DECLARE_MODULE()

bool obs_module_load(void)
{
	blog(LOG_INFO, "[bread-gt] GT title support loaded");
	return true;
}
