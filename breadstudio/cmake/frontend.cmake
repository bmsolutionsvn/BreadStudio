# Included from the last line of frontend/CMakeLists.txt.

file(STRINGS "${CMAKE_SOURCE_DIR}/breadstudio/VERSION" BREADSTUDIO_VERSION LIMIT_COUNT 1)

target_sources(
  obs-studio
  PRIVATE "${CMAKE_SOURCE_DIR}/breadstudio/src/Branding.cpp" "${CMAKE_SOURCE_DIR}/breadstudio/src/Branding.hpp"
)
target_include_directories(obs-studio PRIVATE "${CMAKE_SOURCE_DIR}/breadstudio/src")
target_compile_definitions(obs-studio PRIVATE BREADSTUDIO_VERSION="${BREADSTUDIO_VERSION}")

if(OS_WINDOWS)
  # frontend/cmake/os-windows.cmake has already generated obs.rc from OBS's
  # template. Generating ours to the same path replaces it, so OBS's
  # obs.rc.in stays unedited.
  configure_file("${CMAKE_SOURCE_DIR}/breadstudio/windows/breadstudio.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/obs.rc")
endif()
