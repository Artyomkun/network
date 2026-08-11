// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include "logger/level.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace logger {

namespace {

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

}  // namespace

const char* toString(Level level) {
    switch (level) {
        case Level::Debug:
            return "DEBUG";
        case Level::Info:
            return "INFO";
        case Level::Error:
            return "ERROR";
    }
    return "INFO";
}

bool parseLevel(const std::string& text, Level& out) {
    const std::string lowered = toLower(text);
    if (lowered == "debug") {
        out = Level::Debug;
        return true;
    }
    if (lowered == "info") {
        out = Level::Info;
        return true;
    }
    if (lowered == "error") {
        out = Level::Error;
        return true;
    }
    return false;
}

absl::LogSeverity toAbslSeverity(Level level) {
    switch (level) {
        case Level::Debug:
            return absl::LogSeverity::kInfo;
        case Level::Info:
            return absl::LogSeverity::kWarning;
        case Level::Error:
            return absl::LogSeverity::kError;
    }
    return absl::LogSeverity::kError;
}

Level fromAbslSeverity(absl::LogSeverity severity) {
    if (severity < absl::LogSeverity::kWarning) {
        return Level::Debug;
    }
    if (severity < absl::LogSeverity::kError) {
        return Level::Info;
    }
    return Level::Error;
}

}  // namespace logger
