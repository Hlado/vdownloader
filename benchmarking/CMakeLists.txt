set(WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR}/working_dir)

add_custom_target(benchmarking_prep
                  ${CMAKE_COMMAND} -E make_directory ${WORKING_DIR})

set(${PROJECT_NAME}_BENCMARKING_URL "https://www.youtube.com/watch?v=aqz-KE-bpKQ" CACHE STRING "Video file url/path")
set(${PROJECT_NAME}_BENCMARKING_YT_FORMAT 299 CACHE STRING "Format to request from youtube")
set(${PROJECT_NAME}_BENCMARKING_RUNS 3 CACHE STRING "Number of runs")
set(${PROJECT_NAME}_BENCMARKING_REFERENCE_PATH "$<TARGET_FILE:${PROJECT_NAME}>" CACHE FILEPATH "Path to reference version of app")

#Ok, ideally, we want to benchmark release build only,
#but it seems it can't be achieved without extensive work,
#so we just give up and leave burden of checking for build type to user.
#
#Actually, simple generator expression check for configuration may be enough,
#but that's low priority improvement
macro(benchmark target_name script_name comment)
    add_custom_target(${target_name}
                      COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/${script_name} $<TARGET_FILE:${PROJECT_NAME}> ${${PROJECT_NAME}_BENCMARKING_URL} ${${PROJECT_NAME}_BENCMARKING_YT_FORMAT} ${${PROJECT_NAME}_BENCMARKING_RUNS}
                      DEPENDS benchmarking_prep
                      WORKING_DIRECTORY ${WORKING_DIR}
                      COMMENT ${comment})
    add_dependencies(${target_name} ${PROJECT_NAME})
endmacro()

benchmark(benchmarking_ffmpeg ffmpeg_benchmark.py "Running ffmpeg benchmark")
benchmark(benchmarking_ffmpeg_parallel ffmpeg_parallel_benchmark.py "Running ffmpeg parallel benchmark")
benchmark(benchmarking_threading threading_benchmark.py "Running threading benchmark")

add_custom_target(benchmarking_reference
                  COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/reference_benchmark.py $<TARGET_FILE:${PROJECT_NAME}> ${${PROJECT_NAME}_BENCMARKING_REFERENCE_PATH} ${${PROJECT_NAME}_BENCMARKING_URL} ${${PROJECT_NAME}_BENCMARKING_YT_FORMAT} ${${PROJECT_NAME}_BENCMARKING_RUNS}
                  DEPENDS benchmarking_prep
                  WORKING_DIRECTORY ${WORKING_DIR}
                  COMMENT "Running reference benchmark")
add_dependencies(benchmarking_reference ${PROJECT_NAME})

set_target_properties(benchmarking_prep benchmarking_ffmpeg benchmarking_ffmpeg_parallel benchmarking_threading benchmarking_reference
                      PROPERTIES FOLDER benchmarking)