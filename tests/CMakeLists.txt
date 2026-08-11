# Copyright 2026 Artyomkun
# SPDX-License-Identifier: Apache-2.0

add_executable(logger_tests
    main.cpp
    test_level.cpp
    test_absl_log.cpp
    test_format.cpp
    test_message_queue.cpp
    test_statistics.cpp
    ${CMAKE_SOURCE_DIR}/stats/statistics.cpp
)

target_include_directories(logger_tests PRIVATE ${CMAKE_SOURCE_DIR}/stats)
target_link_libraries(logger_tests PRIVATE logger)

apply_quality(logger_tests)
link_logger_runtime(logger_tests)