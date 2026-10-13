cmake_minimum_required(VERSION 3.28...3.30)

find_package(Qt6 REQUIRED Widgets)

add_library(bread-desk MODULE)
add_library(OBS::bread-desk ALIAS bread-desk)

target_sources(bread-desk PRIVATE Desk.hpp desk.cpp)

target_link_libraries(bread-desk PRIVATE OBS::frontend-api OBS::libobs Qt::Widgets)

if(OS_WINDOWS)
  configure_file(cmake/windows/obs-module.rc.in bread-desk.rc)
  target_sources(bread-desk PRIVATE bread-desk.rc)
endif()

set_target_properties_obs(
  bread-desk
  PROPERTIES FOLDER plugins/bread-desk
             PREFIX ""
             AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON
)

target_sources(
  bread-desk
  PRIVATE
    AudioBed.cpp
    AudioBed.hpp
    BusMeters.cpp
    BusMeters.hpp
    DeskLayout.cpp
    DeskLayout.hpp
    InputBar.cpp
    InputBar.hpp
    InputModel.cpp
    InputModel.hpp
    InputSettings.cpp
    InputSettings.hpp
    LayerGraph.cpp
    LayerGraph.hpp
    Meter.cpp
    Meter.hpp
    Mixer.cpp
    Mixer.hpp
    StreamAddress.cpp
    StreamAddress.hpp
    TileDisplay.cpp
    TileDisplay.hpp
)

# The one automated test: no OBS, no Qt.
add_executable(bread-desk-test test/LayerGraphTest.cpp LayerGraph.cpp)
target_include_directories(bread-desk-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(
  bread-desk-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/desk/test-bin" FOLDER plugins/bread-desk
)

# The stream address function: no OBS, no Qt.
add_executable(bread-stream-test test/StreamAddressTest.cpp StreamAddress.cpp)
target_include_directories(bread-stream-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(
  bread-stream-test
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/breadstudio/desk/test-bin" FOLDER plugins/bread-desk
)
