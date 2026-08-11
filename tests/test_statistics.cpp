// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include <chrono>

#include "logger/record.hpp"

#include "../stats/statistics.hpp"
#include "test_framework.hpp"

namespace {

logger::Record makeRecord(logger::Level level, const std::string& message,
                          const std::chrono::system_clock::time_point& timestamp) {
    logger::Record record;
    record.level = level;
    record.message = message;
    record.timestamp = timestamp;
    return record;
}

}  // namespace

TEST("Statistics counts totals and levels") {
    const auto now = std::chrono::system_clock::now();
    stats::Statistics statistics;
    statistics.add(makeRecord(logger::Level::Debug, "d", now));
    statistics.add(makeRecord(logger::Level::Info, "i1", now));
    statistics.add(makeRecord(logger::Level::Info, "i2", now));
    statistics.add(makeRecord(logger::Level::Error, "e", now));

    CHECK_EQ(statistics.total(), 4u);
    CHECK_EQ(statistics.byLevel(logger::Level::Debug), 1u);
    CHECK_EQ(statistics.byLevel(logger::Level::Info), 2u);
    CHECK_EQ(statistics.byLevel(logger::Level::Error), 1u);
}

TEST("Statistics computes min/max/average message lengths") {
    const auto now = std::chrono::system_clock::now();
    stats::Statistics statistics;
    statistics.add(makeRecord(logger::Level::Info, "12345", now));   // 5
    statistics.add(makeRecord(logger::Level::Info, "1", now));       // 1
    statistics.add(makeRecord(logger::Level::Info, "123", now));     // 3

    CHECK_EQ(statistics.minLength(), 1u);
    CHECK_EQ(statistics.maxLength(), 5u);
    const double expected = (5.0 + 1.0 + 3.0) / 3.0;
    CHECK(statistics.averageLength() == expected);
}

TEST("Statistics counts only messages from the last hour") {
    const auto now = std::chrono::system_clock::now();
    stats::Statistics statistics;
    statistics.add(makeRecord(logger::Level::Info, "old",
                              now - std::chrono::hours(2)));
    statistics.add(makeRecord(logger::Level::Info, "fresh", now));
    statistics.add(makeRecord(logger::Level::Info, "fresh2", now));

    CHECK_EQ(statistics.lastHour(), 2u);
    CHECK_EQ(statistics.total(), 3u);
}

TEST("Statistics is empty before any messages") {
    stats::Statistics statistics;
    CHECK_EQ(statistics.total(), 0u);
    CHECK_EQ(statistics.minLength(), 0u);
    CHECK_EQ(statistics.maxLength(), 0u);
    CHECK_EQ(statistics.averageLength(), 0.0);
    CHECK_EQ(statistics.lastHour(), 0u);
}

