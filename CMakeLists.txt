cmake_minimum_required(VERSION 3.28)

project(vdownloader CXX)

include(CTest)

set(BUILD_APPS OFF CACHE BOOL INTERNAL FORCE)
add_subdirectory(3rdparty/Bento4)

find_package(httplib CONFIG REQUIRED)

find_package(Poco REQUIRED COMPONENTS Foundation)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()