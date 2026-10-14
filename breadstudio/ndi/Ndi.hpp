#pragma once

#include "NdiLoad.hpp"

#include <obs-module.h>

#include <string>
#include <vector>

/* NDI's functions; null while NDI is off */
extern const NDIlib_v6 *ndiLib;

/* why NDI is off, for the NDI tab (spec §10); empty when it is on */
const std::string &ndiStatus();

/* the names of the NDI sources announced so far, "MACHINE (name)" */
std::vector<std::string> ndiSources();

/* the source type, filled in ndi-source.cpp */
obs_source_info ndiSourceInfo();