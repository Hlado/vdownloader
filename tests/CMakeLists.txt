set(PARENT_PROJECT_NAME ${PROJECT_NAME})

project(${PROJECT_NAME}tests CXX)

find_package(GTest REQUIRED CONFIG)

add_executable(${PROJECT_NAME} LibavUtilsTests.cpp
                               OptionsTests.cpp
                               SourcesTests.cpp
                               UtilsTests.cpp
                               VideoStreamTests.cpp)

#https://github.com/google/googletest/issues/2157#issuecomment-1004443132
target_link_libraries(${PROJECT_NAME}
                      PRIVATE GTest::gtest GTest::gmock_main GTest::gtest_main GTest::gmock
                      ${PARENT_PROJECT_NAME}lib)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/data/squares.mp4 ${CMAKE_CURRENT_BINARY_DIR}/tests_data/squares.mp4 COPYONLY)