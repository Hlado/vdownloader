set(WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR}/working_dir)

add_custom_target(benchmarking_prep
                  ${CMAKE_COMMAND} -E make_directory ${WORKING_DIR})
                      
#Ok, ideally, we want to benchmark release build only,
#but it seems it can't be achieved without extensive work,
#so we just give up and leave burden of checking for build type to user.
#
#Actually, simple generator expression check for configuration may be enough,
#but that's low priority improvement
add_custom_target(benchmarking_ffmpeg
                  COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg_benchmark.py $<TARGET_FILE:${PROJECT_NAME}> https://www.youtube.com/watch?v=aqz-KE-bpKQ 299 3
                  DEPENDS ${PROJECT_NAME} benchmarking_prep
                  WORKING_DIRECTORY ${WORKING_DIR}
                  COMMENT "Running ffmpeg benchmark")

add_custom_target(benchmarking_threading
                  COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/threading_benchmark.py $<TARGET_FILE:${PROJECT_NAME}> https://www.youtube.com/watch?v=aqz-KE-bpKQ 299 3
                  DEPENDS ${PROJECT_NAME} benchmarking_prep
                  WORKING_DIRECTORY ${WORKING_DIR}
                  COMMENT "Running threading benchmark")
      
set_target_properties(benchmarking_prep benchmarking_ffmpeg benchmarking_threading
                      PROPERTIES FOLDER benchmarking)