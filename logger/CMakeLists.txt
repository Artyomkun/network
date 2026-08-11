set(LOGGER_SOURCES
    src/level.cpp
    src/format.cpp
    src/file_sink.cpp
    src/logger.cpp
)

if(LOGGER_ENABLE_SOCKETS)
    list(APPEND LOGGER_SOURCES
        src/socket.cpp
        src/socket_sink.cpp
    )
endif()

if(LOGGER_BUILD_SHARED)
    add_library(logger SHARED ${LOGGER_SOURCES})
    target_compile_definitions(logger PRIVATE LOGGER_BUILD_SHARED LOGGER_BUILDING_LIBRARY)
else()
    add_library(logger STATIC ${LOGGER_SOURCES})
endif()

target_include_directories(logger PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
# Abseil headers are marked SYSTEM so that our strict warning flags do not
# spray warnings over third-party code.
target_include_directories(logger SYSTEM PUBLIC ${abseil-cpp_SOURCE_DIR})

# Logging is done by absl::log; our code only configures it and adds the
# socket sink.
target_link_libraries(logger PUBLIC
    absl::log
    absl::log_globals
    absl::log_initialize
    absl::log_sink
    absl::log_severity
    absl::time
)

if(LOGGER_ENABLE_SOCKETS AND WIN32)
    target_link_libraries(logger PRIVATE ws2_32)
endif()

apply_quality(logger)