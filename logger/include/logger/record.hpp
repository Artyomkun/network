// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <string>

#include "logger/export.hpp"
#include "logger/level.hpp"

namespace logger {

// A single journal entry: message text, severity level and receive time.
struct LOGGER_API Record {
    std::string message;
    Level level = Level::Info;
    std::chrono::system_clock::time_point timestamp;
};

}  // namespace logger