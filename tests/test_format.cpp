// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include "logger/format.hpp"

#include "test_framework.hpp"

TEST("formatRecord produces the documented layout") {
    logger::Record record;
    record.level = logger::Level::Info;
    record.message = "started";
    record.timestamp = std::chrono::system_clock::from_time_t(1'700'000'000);

    // The time is printed in the local timezone, so the expectation is built
    // through a local conversion.
    std::tm local{};
    const std::time_t raw = 1'700'000'000;
#ifdef _WIN32
    localtime_s(&local, &raw);
#else
    localtime_r(&raw, &local);
#endif
    std::ostringstream expected;
    expected << '[' << std::put_time(&local, "%Y-%m-%d %H:%M:%S")
             << "] [INFO] started";

    CHECK_EQ(logger::formatRecord(record), expected.str());
}

TEST("parseRecord round-trips a formatted record") {
    logger::Record source;
    source.level = logger::Level::Error;
    source.message = "failure: disk full";
    source.timestamp = std::chrono::system_clock::from_time_t(1'700'000'000);

    logger::Record parsed;
    CHECK(logger::parseRecord(logger::formatRecord(source), parsed));
    CHECK(parsed.message == source.message);
    CHECK(parsed.level == source.level);
    CHECK(std::chrono::system_clock::to_time_t(parsed.timestamp) ==
          std::chrono::system_clock::to_time_t(source.timestamp));
}

TEST("parseRecord rejects malformed lines") {
    logger::Record parsed;
    CHECK(!logger::parseRecord("", parsed));
    CHECK(!logger::parseRecord("no brackets here", parsed));
    CHECK(!logger::parseRecord("[] [] msg", parsed));
    CHECK(!logger::parseRecord("[not a time] [INFO] msg", parsed));
    CHECK(!logger::parseRecord("[2026-01-01 00:00:00] [WARNING] msg", parsed));
    CHECK(!logger::parseRecord("[2026-01-01 00:00:00] [INFO]", parsed));
}

TEST("parseRecord keeps the rest of the line as the message") {
    logger::Record parsed;
    CHECK(logger::parseRecord("[2026-01-01 10:00:00] [DEBUG] hello world", parsed));
    CHECK(parsed.message == "hello world");
    CHECK(parsed.level == logger::Level::Debug);
}