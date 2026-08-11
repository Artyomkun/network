// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

#include "absl/base/log_severity.h"
#include "logger/export.hpp"

namespace logger {

// Message severity levels in ascending order of importance.
// Messages below the default level are not written to the journal.
enum class Level {
    Debug = 0,
    Info = 1,
    Error = 2,
};

// Level name in uppercase ("DEBUG", "INFO", "ERROR").
LOGGER_API const char* toString(Level level);

// Parses a level name, case-insensitive. Returns false if the name is unknown.
LOGGER_API bool parseLevel(const std::string& text, Level& out);

// --- Mapping to absl::log severities -------------------------------------------
// absl::log has 4 built-in severities: INFO(0), WARNING(1), ERROR(2), FATAL(3).
// Our three levels map to them preserving order and filtering:
// Debug -> INFO, Info -> WARNING, Error -> ERROR.
LOGGER_API absl::LogSeverity toAbslSeverity(Level level);
LOGGER_API Level fromAbslSeverity(absl::LogSeverity severity);

}  // namespace logger
