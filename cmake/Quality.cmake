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
        if(LOGGER_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    endif()
endfunction()