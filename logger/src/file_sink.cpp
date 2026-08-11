// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include "logger/file_sink.hpp"

#include <chrono>
#include <filesystem>
#include <system_error>

#include "absl/log/log_entry.h"
#include "absl/time/clock.h"
#include "logger/format.hpp"

namespace logger {

namespace {

using Clock = std::chrono::steady_clock;

// The buffer is flushed when it accumulates this volume or after this
// interval, whichever comes first.
constexpr std::size_t kFlushThreshold = 64 * 1024;
constexpr auto kFlushInterval = std::chrono::seconds(1);

}  // namespace

FileLogSink::FileLogSink(const std::string& path, FlushMode mode,
                         std::size_t max_bytes, int max_files)
    : opened_(false),
      mode_(mode),
      path_(path),
      max_bytes_(max_bytes),
      max_files_(max_files > 0 ? max_files : 1),
      last_flush_(Clock::now()) {
    stream_.open(path_, std::ios::app);
    if (stream_.is_open()) {
        opened_ = true;
        // Rotation accounts for the existing file size (appending after a
        // restart must not reset the counter).
        std::error_code ec;
        const std::uintmax_t size = std::filesystem::file_size(path_, ec);
        file_size_ = ec ? 0 : static_cast<std::size_t>(size);
    }
}

FileLogSink::~FileLogSink() {
    // The buffer tail is not lost even if the application never calls flush().
    std::lock_guard<std::mutex> lock(mutex_);
    flushLocked();
}

void FileLogSink::Send(const absl::LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!opened_) {
        return;
    }
    // The line is formatted ourselves via formatRecord: the time comes from
    // the entry (local time via strftime), the level and text from absl.
    // The file format matches the socket protocol:
    //   [2026-08-11 20:05:11] [INFO] message
    Record record;
    record.message = entry.text_message();
    record.level = fromAbslSeverity(entry.log_severity());
    record.timestamp = absl::ToChronoTime(entry.timestamp());
    const std::string line = formatRecord(record) + "\n";

    if (mode_ == FlushMode::Immediate) {
        if (max_bytes_ > 0 && file_size_ + line.size() > max_bytes_) {
            rotateLocked();
            if (!opened_) {
                // Reopen failed: the entry did not make it, account the loss.
                dropped_bytes_ += line.size();
                return;
            }
        }
        stream_ << line;
        stream_.flush();
        if (stream_) {
            file_size_ += line.size();
        } else {
            dropped_bytes_ += line.size();
        }
        return;
    }

    buffer_ += line;
    const Clock::time_point now = Clock::now();
    if (buffer_.size() >= kFlushThreshold || now - last_flush_ >= kFlushInterval) {
        flushLocked();
    }
}

void FileLogSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    flushLocked();
}

void FileLogSink::flushLocked() {
    if (!opened_ || buffer_.empty()) {
        return;
    }
    if (max_bytes_ > 0 && file_size_ + buffer_.size() > max_bytes_) {
        rotateLocked();
        if (!opened_) {
            // Rotation failed (e.g. the disk is full): the new file could
            // not be opened, so there is nowhere to write the buffer. Do not
            // hold it in memory forever - account the loss and free it. The
            // next write will retry the open.
            dropped_bytes_ += buffer_.size();
            buffer_.clear();
            return;
        }
    }

    stream_ << buffer_;
    stream_.flush();
    if (stream_) {
        file_size_ += buffer_.size();
        buffer_.clear();
        last_flush_ = Clock::now();
        return;
    }

    // The write failed (full disk etc.). The buffer is not dropped right
    // away: it stays in memory and the next message retries the write - the
    // error may be transient. Memory growth is bounded: when kHardBufferCap
    // is exceeded the buffer is dropped and accounted in droppedBytes().
    if (buffer_.size() > kHardBufferCap) {
        dropped_bytes_ += buffer_.size();
        buffer_.clear();
    }
}

void FileLogSink::rotateLocked() {
    // Shift: path -> path.1, path.1 -> path.2, ..., then a fresh path.
    // On Windows rename does not overwrite an existing file, so the target
    // of every shift is removed first.
    stream_.close();
    stream_.clear();

    std::error_code ec;
    const std::string oldest = path_ + "." + std::to_string(max_files_);
    std::filesystem::remove(oldest, ec);
    for (int i = max_files_ - 1; i >= 1; --i) {
        const std::string from = path_ + "." + std::to_string(i);
        const std::string to = path_ + "." + std::to_string(i + 1);
        std::filesystem::remove(to, ec);
        std::filesystem::rename(from, to, ec);
    }
    const std::string first = path_ + ".1";
    std::filesystem::remove(first, ec);
    std::filesystem::rename(path_, first, ec);

    stream_.open(path_, std::ios::app);
    opened_ = stream_.is_open();
    file_size_ = 0;
}

bool FileLogSink::good() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return opened_;
}

std::size_t FileLogSink::droppedBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_bytes_;
}

}  // namespace logger
