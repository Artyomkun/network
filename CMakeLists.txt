cmake_minimum_required(VERSION 3.16)
project(journal_logger LANGUAGES CXX)

# --- Platform: checked at build time ----------------------------------------------
# Determined before compilation and passed to the code as a macro
# LOGGER_PLATFORM_WINDOWS / LOGGER_PLATFORM_POSIX; the code branches on them
# (see logger/platform.hpp). Also rejects unsupported platforms.
if(WIN32)
    set(LOGGER_PLATFORM "Windows")
    set(LOGGER_SOCKETS "Winsock2")
    add_compile_definitions(LOGGER_PLATFORM_WINDOWS=1)
elseif(UNIX)
    set(LOGGER_PLATFORM "POSIX")
    set(LOGGER_SOCKETS "Berkeley sockets")
    add_compile_definitions(LOGGER_PLATFORM_POSIX=1)
else()
    message(FATAL_ERROR "Unsupported platform: add a branch in CMakeLists.txt")
endif()
message(STATUS "Platform: ${LOGGER_PLATFORM} (${LOGGER_SOCKETS})")

# --- C++ standard: chosen at configure time -----------------------------------------
# The code checks the version via __cplusplus (branch in platform.hpp) and
# enables the available paths: C++17 - minimum, C++20 - std::format etc.
set(LOGGER_CXX_STANDARD 17 CACHE STRING "C++ standard to build with: 17, 20 or 23")
set_property(CACHE LOGGER_CXX_STANDARD PROPERTY STRINGS 17 20 23)
set(CMAKE_CXX_STANDARD ${LOGGER_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")

if(MSVC)
    # Without /Zc:__cplusplus MSVC always reports 199711L.
    add_compile_options(/Zc:__cplusplus)
endif()

include(cmake/Quality.cmake)

# --- Dependencies ------------------------------------------------------------------
# Logging is performed by the ready-made absl::log library (Google Abseil):
# writing our own logger would be reinventing the wheel (see README,
# "Why the journal is built on absl::log").
# The first configuration requires git and network (abseil-cpp is cloned).
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "Use the parent's C++ standard" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "Disable abseil tests" FORCE)
include(FetchContent)
FetchContent_Declare(abseil-cpp
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG 20250127.0
)
FetchContent_MakeAvailable(abseil-cpp)
message(STATUS "Dependency: abseil-cpp (absl::log)")

if(WIN32)
    # MinGW compatibility: a recent mingw-w64 (UCRT, SDK >= 10.0.22621)
    # defaults to an NTDDI_VERSION of the Win11 level, which makes cctz
    # (absl/time) enable WinRT code (windows.globalization.h) that MinGW
    # cannot build. Lowering the version makes cctz use the fallback path of
    # local timezone detection; WinRT is not needed here.
    target_compile_definitions(absl_time_zone PRIVATE NTDDI_VERSION=NTDDI_WIN10)
endif()

# The logger library: shared (ON) or static (OFF) build.
option(LOGGER_BUILD_SHARED "Build the logger library as a shared library" OFF)
# Enables the socket sink (Part 1.5) and the statistics application (Part 3).
option(LOGGER_ENABLE_SOCKETS "Enable the socket sink (Part 1.5) and the stats app (Part 3)" ON)
# Enables building and running unit tests.
option(LOGGER_BUILD_TESTS "Build unit tests" ON)
# Enables micro-benchmarks (bench/bench_logger).
option(LOGGER_BUILD_BENCH "Build micro-benchmarks" ON)

add_subdirectory(logger)
add_subdirectory(app)
add_subdirectory(stats)

if(LOGGER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LOGGER_BUILD_BENCH)
    add_subdirectory(bench)
endif()