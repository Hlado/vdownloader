add_custom_target(benchmarking_prep
                  ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/benchmarking)
         
#Ok, ideally, we want to benchmark release build only,
#but it seems it can't be achieved without extensive work,
#so we just give up and leave burden of checking for build type to user.
#
#Actually, simple generator expression check for configuration may be enough,
#but that's low priority improvement
add_custom_target(benchmarking
                  COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/benchmarking.py $<TARGET_FILE:${PROJECT_NAME}> https://www.youtube.com/watch?v=aqz-KE-bpKQ 299 3
                  DEPENDS ${PROJECT_NAME} benchmarking_prep
                  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/benchmarking
                  COMMENT "Running benchmark")