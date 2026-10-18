cmake_minimum_required(VERSION 3.28...3.30)

add_library(bread-ndi MODULE)
add_library(OBS::bread-ndi ALIAS bread-ndi)

target_sources(bread-ndi PRIVATE Ndi.hpp NdiLoad.hpp ndi.cpp ndi-source.cpp ndi-output.cpp)
target_include_directories(bread-ndi PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ndi-sdk/include")
target_include_directories(bread-ndi PRIVATE "${CMAKE_SOURCE_DIR}/breadstudio/shared")
target_link_libraries(bread-ndi PRIVATE OBS::libobs OBS::frontend-api)

# NDI's DLL is never copied into BreadStudio's folder (network inputs spec §7.2, owner 2026-10-18): a
# GPL program must not ship it. The plugin loads it from the NDI Runtime, which the installer puts on
# the PC by running NDI's own runtime installer. A build machine needs the NDI 6 Runtime to use NDI.

set_target_properties_obs(bread-ndi PROPERTIES FOLDER plugins/bread-ndi PREFIX "")

# A test sender that prints the tally it gets back. It finds NDI's DLL through NDI's runtime folder.
add_executable(bread-ndi-send test/NdiSend.cpp)
target_include_directories(bread-ndi-send PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/ndi-sdk/include")
set_target_properties(
  bread-ndi-send
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/ndi/test-bin" FOLDER plugins/bread-ndi
)
