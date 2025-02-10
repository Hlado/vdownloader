set(PARENT_PROJECT_NAME ${PROJECT_NAME})

project(${PROJECT_NAME}tests CXX)

find_package(GTest REQUIRED CONFIG)

add_executable(${PROJECT_NAME} Ap4ByteStreamTests.cpp
                               Ap4HelpersTests.cpp
                               DecoderTests.cpp
                               Mp4ContainerTests.cpp
                               OptionsTests.cpp
                               SourcesTests.cpp
                               TestContainer.h
                               TrackTests.cpp
                               UtilsTests.cpp)

#https://github.com/google/googletest/issues/2157#issuecomment-1004443132
target_link_libraries(${PROJECT_NAME}
                      PRIVATE GTest::gtest GTest::gmock_main GTest::gtest_main GTest::gmock
                      ${PARENT_PROJECT_NAME}lib)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})