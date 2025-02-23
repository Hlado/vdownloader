cmake_minimum_required(VERSION 3.28)

project(vdownloader CXX)

list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
include(CTest)

set(BUILD_APPS OFF CACHE BOOL INTERNAL FORCE)

find_package(ada CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)
find_package(Stb REQUIRED)

#We enforce warning intolerance only for our own code
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_subdirectory(benchmarking)

include(cmake/valgrind.cmake)