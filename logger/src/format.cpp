// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include "logger/format.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

#include "logger/platform.hpp"

#if LOGGER_HAS_STD_FORMAT
#include <format>
#endif

namespace logger {

namespace {

// Formats a time point in local time as "YYYY-MM-DD HH:MM:SS".
//
// Local conversion (localtime + strftime) is expensive - units of
// microseconds - while almost all journal entries fall into the same second.
// We keep a cache of the last formatted second: repeated calls with the same
// value return the ready string in O(1). The cache is shared by all sinks
// and thread-safe.
std::string formatTime(const std::chrono::system_clock::time_point& point) {
    static std::mutex cache_mutex;
    static std::time_t cached_raw = 0;
    static std::string cached_text;

    const std::time_t raw = std::chrono::system_clock::to_time_t(point);
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (raw == cached_raw) {
            return cached_text;
        }
    }

    // Cache miss: compute outside the lock. The race is harmless - several
    // threads may compute the same value and the cache gets overwritten with
    // the identical string.
    std::tm local{};
#if defined(LOGGER_PLATFORM_WINDOWS)
    localtime_s(&local, &raw);
#else
    localtime_r(&raw, &local);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
    const std::string text(buffer);
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cached_raw = raw;
        cached_text = text;
    }
    return text;
}

}  // namespace

#if LOGGER_HAS_STD_FORMAT
// Branch by standard version: when built under C++20+ the formatting uses
// std::format (faster than streams), otherwise the classic stream-based
// path. Both produce an identical string.
std::string formatRecord(const Record& record) {
    return std::format("[{}] [{}] {}", formatTime(record.timestamp),
                       toString(record.level), record.message);
}
#else
std::string formatRecord(const Record& record) {
    const std::string time = formatTime(record.timestamp);
    const std::string level = toString(record.level);
    std::string out;
    out.reserve(time.size() + level.size() + record.message.size() + 8);
    out += '[';
    out += time;
    out += "] [";
    out += level;
    out += "] ";
    out += record.message;
    return out;
}
#endif

bool parseRecord(const std::string& line, Record& out) {
    if (line.size() < 4 || line[0] != '[') {
        return false;
    }
    const std::string::size_type time_close = line.find(']');
    if (time_close == std::string::npos) {
        return false;
    }

    std::tm parsed_tm{};
    std::istringstream time_in(line.substr(1, time_close - 1));
    time_in >> std::get_time(&parsed_tm, "%Y-%m-%d %H:%M:%S");
    if (time_in.fail()) {
        return false;
    }
    const std::time_t raw = std::mktime(&parsed_tm);
    out.timestamp = std::chrono::system_clock::from_time_t(raw);

    const std::string::size_type level_open = line.find('[', time_close);
    if (level_open == std::string::npos) {
        return false;
    }
    const std::string::size_type level_close = line.find(']', level_open);
    if (level_close == std::string::npos) {
        return false;
    }
    const std::string level_text = line.substr(level_open + 1, level_close - level_open - 1);
    if (!parseLevel(level_text, out.level)) {
        return false;
    }

    if (level_close + 2 > line.size()) {
        return false;
    }
    out.message = line.substr(level_close + 2);
    return true;
}

}  // namespace logger
