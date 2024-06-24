cmake_minimum_required(VERSION 3.29)

project(vdownloader CXX)

find_package(FFMPEG 6.1.1 EXACT REQUIRED)

enable_testing()
add_subdirectory(tests)
add_subdirectory(src)