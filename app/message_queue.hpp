// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>

#include "logger/record.hpp"

namespace app {

// Thread-safe journal entry queue with bounded capacity.
// A producer thread puts entries via push(), a consumer thread waits in pop().
//
// The capacity is bounded: if the consumer falls behind (slow disk), push()
// blocks until there is room. This protects memory from unbounded queue
// growth under a permanent producer/consumer speed mismatch.
class MessageQueue {
public:
    explicit MessageQueue(std::size_t max_size = 1024) : max_size_(max_size) {}

    // Blocks until the queue has room or the queue is stopped.
    // Returns false if the entry was not placed (the queue was stopped).
    bool push(const logger::Record& record) {
        std::unique_lock<std::mutex> lock(mutex_);
        has_space_.wait(lock, [this] { return stopped_ || queue_.size() < max_size_; });
        if (stopped_) {
            return false;
        }
        queue_.push(record);
        has_item_.notify_one();
        return true;
    }

    // Blocks until an entry appears or the queue is stopped.
    // An empty optional means the queue was stopped (after the stop the
    // remaining entries continue to be drained).
    std::optional<logger::Record> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        has_item_.wait(lock, [this] { return stopped_ || !queue_.empty(); });
        if (queue_.empty()) {
            return std::nullopt;
        }
        logger::Record record = queue_.front();
        queue_.pop();
        has_space_.notify_one();
        return record;
    }

    // Wakes up waiting threads; afterwards pop() returns empty and
    // push() returns false.
    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        has_item_.notify_all();
        has_space_.notify_all();
    }

private:
    std::size_t max_size_;
    std::mutex mutex_;
    std::condition_variable has_item_;
    std::condition_variable has_space_;
    std::queue<logger::Record> queue_;
    bool stopped_ = false;
};

}  // namespace app