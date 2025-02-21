set(SOURCE_FILES vd/Errors.cpp
                 vd/ImageFormats.cpp
                 vd/LibavUtils.cpp
                 vd/Options.cpp
                 vd/Sources.cpp
                 vd/Utils.cpp
                 vd/VideoStream.cpp)

set(HEADER_FILES vd/Errors.h
                 vd/ImageFormats.h
                 vd/Libav.h
                 vd/LibavUtils.h
                 vd/Options.h
                 vd/Preprocessor.h
                 vd/Sources.h
                 vd/Utils.h
                 vd/VideoStream.h)

set(LIB_NAME ${PROJECT_NAME}lib)

add_library(${LIB_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(${LIB_NAME} PUBLIC ada::ada)
target_link_libraries(${LIB_NAME} PUBLIC httplib::httplib)
target_link_libraries(${LIB_NAME} PUBLIC ${FFMPEG_LIBRARIES})
target_include_directories(${LIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/3rdparty/args)
target_include_directories(${LIB_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${LIB_NAME} PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_include_directories(${LIB_NAME} PRIVATE ${Stb_INCLUDE_DIR})
target_link_directories(${LIB_NAME} PUBLIC ${FFMPEG_LIBRARY_DIRS})
target_compile_features(${LIB_NAME} PUBLIC cxx_std_20)

add_executable(${PROJECT_NAME} Main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIB_NAME})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
    
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)