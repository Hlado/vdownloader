if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    add_custom_target(valgrind_check
                      COMMAND valgrind "\"$<TARGET_FILE:${PROJECT_NAME}>\"" -t 2 "\"${PROJECT_SOURCE_DIR}/tests/data/squares.mp4\"" 0s-3s:2 4s-7s:2 8s-11s:2 12s-15s:2
                      COMMENT "Running valgrind check")
    add_dependencies(valgrind_check ${PROJECT_NAME})
endif()