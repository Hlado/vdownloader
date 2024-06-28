cmake_minimum_required(VERSION 3.28)

project(vdownloader CXX)

include(CTest)

find_package(bento4 CONFIG REQUIRED)

set(BUILD_APPS OFF CACHE BOOL INTERNAL FORCE)
add_subdirectory(3rdparty/Bento4)

add_subdirectory(src)

if(BUILD_TESTING)
    #add_subdirectory(tests)
endif()