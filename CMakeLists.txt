cmake_minimum_required(VERSION 3.28)

project(vdownloader CXX)

include(CTest)

set(BUILD_APPS OFF CACHE BOOL INTERNAL FORCE)
add_subdirectory(3rdparty/Bento4)

find_package(httplib CONFIG REQUIRED)

find_package(Poco REQUIRED COMPONENTS Foundation)

find_package(PkgConfig)
pkg_check_modules(openh264 REQUIRED IMPORTED_TARGET openh264)


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