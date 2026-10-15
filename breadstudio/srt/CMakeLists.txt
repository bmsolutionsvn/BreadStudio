cmake_minimum_required(VERSION 3.28...3.30)

# OBS's own FFmpeg, as the OMT plugin uses it: one FFmpeg in the process
find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil)

add_library(bread-srt MODULE)
add_library(OBS::bread-srt ALIAS bread-srt)

target_sources(bread-srt PRIVATE srt-output.cpp)
target_link_libraries(bread-srt PRIVATE OBS::libobs FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil)

set_target_properties_obs(bread-srt PROPERTIES FOLDER plugins/bread-srt PREFIX "")
