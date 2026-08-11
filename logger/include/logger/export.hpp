// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(_WIN32)
#  if defined(LOGGER_BUILD_SHARED)
#    if defined(LOGGER_BUILDING_LIBRARY)
#      define LOGGER_API __declspec(dllexport)
#    else
#      define LOGGER_API __declspec(dllimport)
#    endif
#  else
#    define LOGGER_API
#  endif
#else
#  define LOGGER_API __attribute__((visibility("default")))
#endif

