#pragma once

// ============================================================================
// Single point defining the platform, compiler and C++ standard version.
//
// The rest of the code does not check system macros (_WIN32 etc.) directly;
// it branches on LOGGER_PLATFORM_* and LOGGER_HAS_* macros instead. The
// macros can also be supplied via CMake (add_compile_definitions); redefining
// them to the same value is harmless.
// ============================================================================

// --- Platform -------------------------------------------------------------------
// CMake also defines LOGGER_PLATFORM_WINDOWS / LOGGER_PLATFORM_POSIX; here
// they are completed for direct builds without CMake.
#if defined(_WIN32) || defined(_WIN64) || defined(LOGGER_PLATFORM_WINDOWS)
#  ifndef LOGGER_PLATFORM_WINDOWS
#    define LOGGER_PLATFORM_WINDOWS 1
#  endif
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__) || \
    defined(__posix__) || defined(LOGGER_PLATFORM_POSIX)
#  ifndef LOGGER_PLATFORM_POSIX
#    define LOGGER_PLATFORM_POSIX 1
#  endif
#else
#  error "Unsupported platform: add a branch in logger/platform.hpp"
#endif

// --- C++ standard version ---------------------------------------------------------
// Code that needs a newer standard is enabled by the corresponding branch.
// MSVC requires /Zc:__cplusplus (set in CMake).
#if __cplusplus >= 202600L
#  define LOGGER_CPP26 1
#elif __cplusplus >= 202302L
#  define LOGGER_CPP23 1
#elif __cplusplus >= 202002L
#  define LOGGER_CPP20 1
#elif __cplusplus >= 201703L
#  define LOGGER_CPP17 1
#endif

// --- Standard library features -------------------------------------------------------
// std::format appeared in C++20, but compiler support rolled out gradually,
// so we check for the header rather than the standard alone.
#if defined(LOGGER_CPP20) && defined(__has_include)
#  if __has_include(<format>)
#    define LOGGER_HAS_STD_FORMAT 1
#  endif
#endif

// --- Networking API specifics -------------------------------------------------------
#if defined(LOGGER_PLATFORM_POSIX)
// MSG_NOSIGNAL suppresses SIGPIPE when writing to a broken socket (Linux).
#  if defined(__linux__)
#    define LOGGER_HAS_MSG_NOSIGNAL 1
#  endif
// On macOS/BSD the same problem is solved with the SO_NOSIGPIPE option.
#  if defined(__APPLE__) || defined(__FreeBSD__)
#    define LOGGER_HAS_SO_NOSIGPIPE 1
#  endif
#endif