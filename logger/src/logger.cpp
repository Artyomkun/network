// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include "logger/logger.hpp"

#include <memory>

#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "absl/log/log_sink_registry.h"
#include "logger/file_sink.hpp"

namespace logger {

namespace {

// The single file sink per process (absl state is global).
//
// Journal configuration (configureFileJournal / flushFileJournal) is meant
// to be called from one thread before heavy logging starts: access to this
// pointer is not synchronized, concurrent reconfiguration from different
// threads is a data race. absl::log itself is thread-safe; the constraint
// only concerns switching sinks at runtime.
std::unique_ptr<FileLogSink>& fileSink() {
    static std::unique_ptr<FileLogSink> sink;
    return sink;
}

}  // namespace

bool configureFileJournal(const std::string& path, Level min_level,
                          FileLogSink::FlushMode mode, std::size_t max_bytes,
                          int max_files) {
    std::unique_ptr<FileLogSink>& sink = fileSink();

    if (sink && !sink->good()) {
        // The previous file is unavailable - try to reopen a new one.
        absl::RemoveLogSink(sink.get());
        sink.reset();
    }

    // absl::SetLogDestination was removed in newer versions: file output is
    // done through our own LogSink. File availability is checked by a probe.
    std::ofstream probe(path, std::ios::app);
    if (!probe.is_open()) {
        return false;
    }
    probe.close();

    auto new_sink = std::unique_ptr<FileLogSink>(new FileLogSink(path, mode, max_bytes, max_files));
    if (!new_sink->good()) {
        return false;
    }

    if (sink) {
        absl::RemoveLogSink(sink.get());
    }
    sink = std::move(new_sink);
    absl::AddLogSink(sink.get());
    setDefaultLevel(min_level);
    return true;
}

void setDefaultLevel(Level level) {
    absl::SetMinLogLevel(absl::LogSeverityAtLeast(toAbslSeverity(level)));
}

void flushFileJournal() {
    std::unique_ptr<FileLogSink>& sink = fileSink();
    if (sink) {
        sink->flush();
    }
}

}  // namespace logger
