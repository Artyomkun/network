// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include "statistics.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace stats {

void Statistics::add(const logger::Record& record) {
    const std::size_t len = record.message.size();
    if (total_ == 0) {
        min_ = len;
        max_ = len;
    } else {
        min_ = std::min(min_, len);
        max_ = std::max(max_, len);
    }
    ++total_;
    // A level with an unknown index (direct Level(N) assignment) does not
    // get into the counters but still counts as a message.
    const int level_index = static_cast<int>(record.level);
    if (level_index >= 0 && level_index < static_cast<int>(logger::Level::Error) + 1) {
        ++counts_[level_index];
    }
    length_sum_ += len;
    timestamps_.push_back(record.timestamp);
}

std::size_t Statistics::total() const {
    return total_;
}

std::size_t Statistics::byLevel(logger::Level level) const {
    return counts_[static_cast<int>(level)];
}

std::size_t Statistics::lastHour() const {
    const std::chrono::system_clock::time_point cutoff =
        std::chrono::system_clock::now() - std::chrono::hours(1);
    // timestamps_ is monotonically increasing, so binary search is valid.
    const auto it = std::upper_bound(timestamps_.begin(), timestamps_.end(), cutoff);
    const std::size_t count = static_cast<std::size_t>(timestamps_.end() - it);
    // Outdated timestamps are no longer needed - remove them to bound memory.
    if (count < timestamps_.size()) {
        timestamps_.erase(timestamps_.begin(), it);
    }
    return count;
}

std::size_t Statistics::minLength() const {
    return min_;
}

std::size_t Statistics::maxLength() const {
    return max_;
}

double Statistics::averageLength() const {
    if (total_ == 0) {
        return 0.0;
    }
    return static_cast<double>(length_sum_) / static_cast<double>(total_);
}

void Statistics::print() const {
    // std::endl - flush the buffer so the statistics are visible when the
    // output is redirected to a file or a pipe.
    std::cout << "--- statistics ---" << std::endl
              << "messages total:      " << total() << std::endl
              << "  debug:             " << byLevel(logger::Level::Debug) << std::endl
              << "  info:              " << byLevel(logger::Level::Info) << std::endl
              << "  error:             " << byLevel(logger::Level::Error) << std::endl
              << "messages last hour:  " << lastHour() << std::endl
              << "message length:      min " << minLength()
              << ", max " << maxLength()
              << ", average " << averageLength() << std::endl;
}

}  // namespace stats