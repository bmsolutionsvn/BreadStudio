cmake_minimum_required(VERSION 3.28...3.30)

# omthx comes from the OMT-HX checkout as source files, compiled against OBS's own FFmpeg (the FFmpeg::
# targets), so the process has one FFmpeg. Its CMake project is not used: it would find a second one. Only its
# cmake/omt.cmake is, which builds libomt and libvmx from the checkout's omt/ submodules and needs no FFmpeg.
set(OMTHX_ROOT "${CMAKE_SOURCE_DIR}/../OMT-HX" CACHE PATH "OMT-HX checkout")
if(NOT EXISTS "${OMTHX_ROOT}/cmake/omt.cmake" OR NOT EXISTS "${OMTHX_ROOT}/src/link.cpp")
  message(FATAL_ERROR "The OMT plugin needs an OMT-HX checkout at '${OMTHX_ROOT}', or set -DOMTHX_ROOT=<path>.")
endif()
include("${OMTHX_ROOT}/cmake/omt.cmake")

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil swscale)

add_library(omt-hx MODULE)
add_library(OBS::omt-hx ALIAS omt-hx)

set_target_properties(omt-hx PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# All four: link.cpp holds the sender beside the receiver, and the sender needs the encoder. Never called.
target_sources(
  omt-hx
  PRIVATE
    omt-hx.cpp
    omt-output.cpp
    omt-output.hpp
    Catalog.cpp
    Catalog.hpp
    Describe.cpp
    Describe.hpp
    "${OMTHX_ROOT}/src/pure.cpp"
    "${OMTHX_ROOT}/src/encoder.cpp"
    "${OMTHX_ROOT}/src/decoder.cpp"
    "${OMTHX_ROOT}/src/link.cpp"
)

target_include_directories(
  omt-hx
  PRIVATE "${OMTHX_ROOT}/include" "${OMTHX_ROOT}/src"
)
target_include_directories(omt-hx PRIVATE "${CMAKE_SOURCE_DIR}/breadstudio/shared")
# omthx is built into the plugin, not used as omthx.dll
target_compile_definitions(omt-hx PRIVATE OMTHX_STATIC)

target_link_libraries(
  omt-hx
  PRIVATE
    OBS::libobs
    OBS::frontend-api
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swscale
    libomt
)

# libomt.dll loads libvmx.dll from the program's folder, so both go beside obs64.exe.
omthx_copy_runtime_to(omt-hx "${OBS_OUTPUT_DIR}/$<CONFIG>/${OBS_EXECUTABLE_DESTINATION}")

set_target_properties_obs(omt-hx PROPERTIES FOLDER plugins/omt-hx PREFIX "")

# The tile and "receiving now" lines: no OBS. Includes omthx.h, so C++20.
add_executable(bread-omt-test test/DescribeTest.cpp Describe.cpp)
target_include_directories(bread-omt-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${OMTHX_ROOT}/include")
target_compile_definitions(bread-omt-test PRIVATE OMTHX_STATIC)
set_target_properties(
  bread-omt-test
  PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/omt-hx/test-bin"
    FOLDER plugins/omt-hx
)

# The on-air rule against a small scene tree. Runs with libobs and needs obs.dll on PATH. Plan C's NDI
# plugin shares the header; the test lives here, with its first user.
add_executable(bread-onair-test "${CMAKE_SOURCE_DIR}/breadstudio/shared/test/OnAirTest.cpp")
target_include_directories(bread-onair-test PRIVATE "${CMAKE_SOURCE_DIR}/breadstudio/shared")
target_link_libraries(bread-onair-test PRIVATE OBS::libobs OBS::frontend-api)
set_target_properties(
  bread-onair-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/omt-hx/test-bin" FOLDER plugins/omt-hx
)