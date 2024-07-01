set(PARENT_PROJECT_NAME ${PROJECT_NAME})

project(${PROJECT_NAME}tests CXX)

find_package(GTest REQUIRED CONFIG)

add_executable(${PROJECT_NAME} HttpSourceTests.cpp
                               UtilsTests.cpp)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest GTest::gtest_main ${PARENT_PROJECT_NAME}lib)

#configure_file(
#    ${PROJECT_SOURCE_DIR}/data/Big_Buck_Bunny_360_10s_1MB.mp4
#    ${PROJECT_BINARY_DIR}/Big_Buck_Bunny_360_10s_1MB.mp4
#    COPYONLY)
    
#working_dir==project_dir?? i think it depends on generator heavily

add_test(${PROJECT_NAME} ${PROJECT_NAME})