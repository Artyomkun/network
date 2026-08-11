// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <cstddef>
#include <fstream>
#include <mutex>
#include <string>

#include "absl/log/log_sink.h"
#include "logger/export.hpp"

namespace logger {

// absl::log sink writing messages to a text file. Each line carries the
// standard absl prefix (severity, date, time, thread) and the message text.
// Thread-safe.
//
// absl::log cannot write to a file out of the box (the public API only
// supports stderr and custom LogSinks), so file output is a simple LogSink,
// just like the socket one.
class LOGGER_API FileLogSink : public absl::LogSink {
public:
    // File write mode.
    enum class FlushMode {
        // Flush to the kernel after every message: the write is visible in
        // the file immediately, but each entry pays a system call (flush on
        // Windows is ~10 us - tens of times more than formatting itself).
        Immediate,
        // Buffering in memory and flushing by volume/interval: the system
        // call is amortized over ~500 records (64 KB buffer), so a write
        // costs like appending to a string - O(length) with no visible
        // constant. The maximum delay before an entry appears in the file
        // is one interval.
        Buffered,
    };

    // Opens the file for appending. On failure good() returns false.
    //
    // max_bytes > 0 enables rotation: when the file reaches this size it is
    // renamed to <path>.1, the previous <path>.N copies are shifted by one,
    // and a fresh file is opened. No more than max_files rotated copies are
    // kept; the oldest is deleted. This bounds the total journal size on
    // disk (the file does not grow without limit).
    // With max_bytes == 0 rotation is disabled and the file grows unbounded.
    explicit FileLogSink(const std::string& path, FlushMode mode = FlushMode::Buffered,
                         std::size_t max_bytes = 0, int max_files = 3);
    ~FileLogSink() override;

    FileLogSink(const FileLogSink&) = delete;
    FileLogSink& operator=(const FileLogSink&) = delete;

    void Send(const absl::LogEntry& entry) override;

    // Writes buffered entries to the file. In Buffered mode this happens
    // automatically on buffer overflow or by timer; an explicit call is
    // needed before reading the file (e.g. in tests) or on shutdown.
    void flush();

    bool good() const;

    // Total volume of entries dropped because of persistent write errors
    // (full disk etc.). A non-zero value means the journal is losing data;
    // the application can warn the operator.
    std::size_t droppedBytes() const;

private:
    void flushLocked();
    void rotateLocked();

    // Hard cap on the in-memory buffer accumulated while writes fail:
    // above it the buffer is dropped and accounted in dropped_bytes_.
    // Memory stays bounded even if the disk never recovers.
    static constexpr std::size_t kHardBufferCap = 8 * 64 * 1024;

    std::ofstream stream_;
    mutable std::mutex mutex_;
    bool opened_;
    FlushMode mode_;
    std::string path_;
    std::string buffer_;
    std::size_t file_size_ = 0;   // journal volume written by us; the file
                                  // may have started non-empty (appending)
    std::size_t max_bytes_ = 0;   // rotation threshold; 0 - rotation off
    int max_files_ = 3;
    std::size_t dropped_bytes_ = 0;
    std::chrono::steady_clock::time_point last_flush_;
};

}  // namespace logger
