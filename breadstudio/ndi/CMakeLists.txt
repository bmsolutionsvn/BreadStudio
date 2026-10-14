cmake_minimum_required(VERSION 3.28...3.30)

add_library(bread-ndi MODULE)
add_library(OBS::bread-ndi ALIAS bread-ndi)

target_sources(bread-ndi PRIVATE Ndi.hpp NdiLoad.hpp ndi.cpp ndi-source.cpp)
target_include_directories(bread-ndi PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ndi-sdk/include")
target_link_libraries(bread-ndi PRIVATE OBS::libobs)

# NDI's DLL ships beside obs64.exe (spec §7.2), not beside the plugin: OBS loads every DLL in
# obs-plugins as a plugin. It comes from the NDI SDK on the build machine and is never committed.
set(NDI_SDK_DIR "$ENV{NDI_SDK_DIR}" CACHE PATH "NDI 6 SDK")
if(EXISTS "${NDI_SDK_DIR}/Bin/x64/Processing.NDI.Lib.x64.dll")
  add_custom_command(
    TARGET bread-ndi
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_EXECUTABLE_DESTINATION}"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different "${NDI_SDK_DIR}/Bin/x64/Processing.NDI.Lib.x64.dll"
      "${NDI_SDK_DIR}/Bin/x64/Processing.NDI.Lib.Licenses.txt" "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_EXECUTABLE_DESTINATION}"
    VERBATIM
  )
else()
  message(
    WARNING
    "No NDI SDK at '${NDI_SDK_DIR}': BreadStudio is built without NDI's DLL, and NDI stays off unless NDI's runtime is installed. Install the NDI 6 SDK, or set -DNDI_SDK_DIR=<path>."
  )
endif()

set_target_properties_obs(bread-ndi PROPERTIES FOLDER plugins/bread-ndi PREFIX "")