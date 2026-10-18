cmake_minimum_required(VERSION 3.28...3.30)

# The title path is Windows-only (design §8): Direct2D/DirectWrite and D3D11.
if(NOT OS_WINDOWS)
  return()
endif()

find_package(ZLIB REQUIRED)

add_library(bread-gt MODULE)
add_library(OBS::bread-gt ALIAS bread-gt)

target_sources(bread-gt PRIVATE gt.cpp GtPackage.cpp)
target_link_libraries(bread-gt PRIVATE OBS::libobs ZLIB::ZLIB)

set_target_properties_obs(bread-gt PROPERTIES FOLDER plugins/bread-gt PREFIX "")

# The GT title package reader: no OBS, no Qt.
add_executable(bread-gt-package-test test/GtPackageTest.cpp GtPackage.cpp)
target_include_directories(bread-gt-package-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
# libobs's ZLIB::ZLIB is zlib.lib, the import library for a zlib.dll nothing puts on PATH; link the
# static zlibstatic.lib the OBS deps ship beside it, so the test runs with no DLL on PATH.
get_filename_component(_bread_gt_zlib_dir "${ZLIB_LIBRARY_RELEASE}" DIRECTORY)
if(EXISTS "${_bread_gt_zlib_dir}/zlibstatic.lib")
  target_include_directories(bread-gt-package-test PRIVATE "${ZLIB_INCLUDE_DIR}")
  target_link_libraries(bread-gt-package-test PRIVATE "${_bread_gt_zlib_dir}/zlibstatic.lib")
else()
  target_link_libraries(bread-gt-package-test PRIVATE ZLIB::ZLIB)
endif()
target_compile_definitions(bread-gt-package-test PRIVATE GT_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures")
set_target_properties(
  bread-gt-package-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/gt/test-bin" FOLDER plugins/bread-gt
)
