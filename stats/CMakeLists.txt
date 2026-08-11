# Copyright 2026 Artyomkun
# SPDX-License-Identifier: Apache-2.0

if(NOT LOGGER_ENABLE_SOCKETS)
    message(FATAL_ERROR "The stats application requires LOGGER_ENABLE_SOCKETS")
endif()

add_executable(stats_app
    main.cpp
    statistics.cpp
)

target_link_libraries(stats_app PRIVATE logger)

apply_quality(stats_app)
link_logger_runtime(stats_app)
