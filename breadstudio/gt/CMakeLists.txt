cmake_minimum_required(VERSION 3.28...3.30)

# The title path is Windows-only (design §8): Direct2D/DirectWrite and D3D11.
if(NOT OS_WINDOWS)
  return()
endif()

find_package(Qt6 REQUIRED Xml)
find_package(ZLIB REQUIRED)

add_library(bread-gt MODULE)
add_library(OBS::bread-gt ALIAS bread-gt)

target_sources(bread-gt PRIVATE gt.cpp gt-source.cpp GtPackage.cpp GtDocument.cpp GtFields.cpp GtClock.cpp
                                 GtRenderer.cpp GtText.cpp)
target_link_libraries(bread-gt PRIVATE OBS::libobs Qt6::Xml ZLIB::ZLIB d3d11 dxgi d2d1 dwrite windowscodecs)

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

# The document parser: no OBS, and the one gt test that needs Qt (Qt6::Xml for QDomDocument).
add_executable(bread-gt-document-test test/GtDocumentTest.cpp GtDocument.cpp)
target_include_directories(bread-gt-document-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bread-gt-document-test PRIVATE Qt6::Xml)
set_target_properties(
  bread-gt-document-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/gt/test-bin" FOLDER plugins/bread-gt
)

# The field rule: the document parser and the package reader together. Qt for the parser, zlib for the
# package; the local count case reads the owner's F:/Stream when it is there.
add_executable(bread-gt-fields-test test/GtFieldsTest.cpp GtFields.cpp GtPackage.cpp GtDocument.cpp)
target_include_directories(bread-gt-fields-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bread-gt-fields-test PRIVATE Qt6::Xml)
if(EXISTS "${_bread_gt_zlib_dir}/zlibstatic.lib")
  target_include_directories(bread-gt-fields-test PRIVATE "${ZLIB_INCLUDE_DIR}")
  target_link_libraries(bread-gt-fields-test PRIVATE "${_bread_gt_zlib_dir}/zlibstatic.lib")
else()
  target_link_libraries(bread-gt-fields-test PRIVATE ZLIB::ZLIB)
endif()
set_target_properties(
  bread-gt-fields-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/gt/test-bin" FOLDER plugins/bread-gt
)

# The clock formatter: no OBS, no Qt, no zlib.
add_executable(bread-gt-clock-test test/GtClockTest.cpp GtClock.cpp)
target_include_directories(bread-gt-clock-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(
  bread-gt-clock-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/gt/test-bin" FOLDER plugins/bread-gt
)

# The local render test: the Direct2D/DirectWrite renderer, the package reader and the parser. It runs
# on the owner's PC against F:/Stream and a folder of vMix snapshots, and skips when they are absent.
add_executable(bread-gt-render-test test/GtRenderTest.cpp GtRenderer.cpp GtText.cpp GtPackage.cpp GtDocument.cpp
                                  GtFields.cpp GtClock.cpp)
target_include_directories(bread-gt-render-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bread-gt-render-test PRIVATE Qt6::Xml d3d11 dxgi d2d1 dwrite windowscodecs)
if(EXISTS "${_bread_gt_zlib_dir}/zlibstatic.lib")
  target_include_directories(bread-gt-render-test PRIVATE "${ZLIB_INCLUDE_DIR}")
  target_link_libraries(bread-gt-render-test PRIVATE "${_bread_gt_zlib_dir}/zlibstatic.lib")
else()
  target_link_libraries(bread-gt-render-test PRIVATE ZLIB::ZLIB)
endif()
set_target_properties(
  bread-gt-render-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/gt/test-bin" FOLDER plugins/bread-gt
)
