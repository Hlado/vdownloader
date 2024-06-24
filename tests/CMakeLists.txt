set(PARENT_PROJECT_NAME ${PROJECT_NAME})

project(${PROJECT_NAME}tests CXX)

find_package(GTest REQUIRED CONFIG)

add_executable(${PROJECT_NAME} FormatContextTests.cpp)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest GTest::gtest_main ${PARENT_PROJECT_NAME}lib)

configure_file(
    ${PROJECT_SOURCE_DIR}/data/sample__240__libx265__aac__30s__video.mkv
    ${PROJECT_BINARY_DIR}/sample__240__libx265__aac__30s__video.mkv
    COPYONLY )
    
add_test(${PROJECT_NAME} ${PROJECT_NAME})