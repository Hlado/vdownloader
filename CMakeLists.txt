cmake_minimum_required(VERSION 3.28)

project(vdownloader CXX)

find_package(FFMPEG REQUIRED)

enable_testing()
add_subdirectory(tests)
add_subdirectory(src)