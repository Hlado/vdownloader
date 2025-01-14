set(SOURCE_FILES vd/Decoder.cpp
                 vd/DecodingUtils.cpp
                 vd/Errors.cpp
                 vd/LibavH264Decoder.cpp
                 vd/Mp4Container.cpp
                 vd/Mp4Utils.cpp
                 vd/OpenH264Decoder.cpp
                 vd/Options.cpp
                 vd/Sources.cpp
                 vd/Tga.cpp
                 vd/Track.cpp
                 vd/Utils.cpp)

set(HEADER_FILES vd/Ap4ByteStream.h
                 vd/Ap4Headers.h
                 vd/Ap4Helpers.h
                 vd/Decoder.h
                 vd/DecodingUtils.h
                 vd/Errors.h
                 vd/LibavH264Decoder.h
                 vd/Mp4Container.h
                 vd/Mp4Utils.h
                 vd/OpenH264Decoder.h
                 vd/Options.h
                 vd/Preprocessor.h
                 vd/Sources.h
                 vd/Tga.h
                 vd/Track.h
                 vd/Utils.h)

set(LIB_NAME ${PROJECT_NAME}lib)

add_library(${LIB_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(${LIB_NAME} PUBLIC ap4)
target_link_libraries(${LIB_NAME} PUBLIC httplib::httplib)
target_link_libraries(${LIB_NAME} PUBLIC yuv)
target_link_libraries(${LIB_NAME} PUBLIC Poco::Foundation)
target_link_libraries(${LIB_NAME} PUBLIC PkgConfig::openh264)
target_link_libraries(${LIB_NAME} PUBLIC ${FFMPEG_LIBRARIES})
target_include_directories(${LIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/3rdparty/args)
target_include_directories(${LIB_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${LIB_NAME} PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_link_directories(${LIB_NAME} PUBLIC ${FFMPEG_LIBRARY_DIRS})
target_compile_features(${LIB_NAME} PUBLIC cxx_std_20)

add_executable(${PROJECT_NAME} Main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIB_NAME})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
    
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)