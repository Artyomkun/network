if(NOT LOGGER_ENABLE_SOCKETS)
    message(FATAL_ERROR "The stats application requires LOGGER_ENABLE_SOCKETS")
endif()

add_executable(stats_app
    main.cpp
    statistics.cpp
)

target_link_libraries(stats_app PRIVATE logger)

apply_quality(stats_app)
