set(SOURCE_FILES vd/Ap4HttpStream.cpp
                 vd/Errors.cpp)

set(HEADER_FILES vd/Ap4HttpStream.h
                 vd/Preprocessor.h
                 vd/Errors.h)

set(LIB_NAME ${PROJECT_NAME}lib)

add_library(${LIB_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(${LIB_NAME} PUBLIC ap4)
target_include_directories(${LIB_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(${LIB_NAME} PUBLIC cxx_std_20)


add_executable(${PROJECT_NAME} Main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIB_NAME})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
    
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)