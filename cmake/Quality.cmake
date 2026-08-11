# Copyright 2026 Artyomkun
# SPDX-License-Identifier: Apache-2.0

# ============================================================================
# "Production-grade" build quality settings:
# strict warnings for all targets, optional Werror mode.
#
# Usage:
#   include(cmake/Quality.cmake)   # in the root CMakeLists.txt
#   apply_quality(<target>)        # for every target
# ============================================================================

option(LOGGER_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(LOGGER_WERROR "Treat compiler warnings as errors" OFF)

# On Windows the DLLs produced by a shared build (liblogger.dll and, when
# abseil is built as one monolith, abseil_dll.dll) sit in their build
# directories; copy them next to every executable so that the loader finds
# them without PATH tricks. No-op for static builds and for POSIX (there the
# loader finds the library via the build-tree RPATH set by CMake).
# Must be called from the directory that created <target>.
function(link_logger_runtime target)
    if(LOGGER_BUILD_SHARED AND WIN32)
        foreach(lib logger abseil_dll)
            if(TARGET ${lib})
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        $<TARGET_FILE:${lib}> $<TARGET_FILE_DIR:${target}>)
            endif()
        endforeach()
    endif()
endfunction()

function(apply_quality target)
    if(NOT LOGGER_ENABLE_WARNINGS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wformat=2
            -Wnull-dereference
        )
        if(LOGGER_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        # Exporting classes with STL members (std::string, std::mutex) and an
        # absl::LogSink base triggers C4251/C4275 on every export macro use;
        # both are benign for a C++ DLL and normally disabled by headers.
        target_compile_options(${target} PRIVATE /wd4251 /wd4275)
        if(LOGGER_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    endif()
endfunction()