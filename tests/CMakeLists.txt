set(PARENT_PROJECT_NAME ${PROJECT_NAME})

project(${PROJECT_NAME}tests CXX)

find_package(GTest 1.14.0 EXACT REQUIRED CONFIG)

add_executable(${PROJECT_NAME} FormatContextTests.cpp)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest GTest::gtest_main ${PARENT_PROJECT_NAME}lib)
add_test(${PROJECT_NAME} ${PROJECT_NAME})