// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <vector>

#include "logger/level.hpp"
#include "logger/record.hpp"

namespace stats {

// Collects statistics over journal entries:
//  - message counts: total, per level, and over the last hour;
//  - message lengths: minimum, maximum, average.
class Statistics {
public:
    void add(const logger::Record& record);

    std::size_t total() const;
    std::size_t byLevel(logger::Level level) const;
    std::size_t lastHour() const;

    std::size_t minLength() const;
    std::size_t maxLength() const;
    double averageLength() const;

    // Prints the collected statistics to the console.
    void print() const;

private:
    std::size_t total_ = 0;
    std::size_t counts_[3] = {0, 0, 0};
    std::size_t min_ = 0;
    std::size_t max_ = 0;
    std::size_t length_sum_ = 0;
    // Entry receive times (monotonically increasing) for the last-hour
    // count. mutable - outdated timestamps are removed during the count.
    mutable std::deque<std::chrono::system_clock::time_point> timestamps_;
};

}  // namespace stats
