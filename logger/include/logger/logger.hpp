#pragma once

#include <cstddef>
#include <string>

#include "logger/export.hpp"
#include "logger/file_sink.hpp"
#include "logger/level.hpp"

namespace logger {

// Journal setup on top of absl::log.
//
// The write itself is done by the standard absl::LOG(severity) - see README
// for why we do not write our own logger. Here we only tune it: the target
// file, the importance threshold and its runtime change.

// Directs the journal to a file and sets the default severity level.
// Messages below the threshold are discarded by absl::log itself.
// Returns false if the file is not writable.
//
// max_bytes > 0 enables size-based file rotation (see FileLogSink): the
// journal on disk does not grow without limit. With max_bytes == 0 there is
// no rotation.
LOGGER_API bool configureFileJournal(const std::string& path, Level min_level,
                                     FileLogSink::FlushMode mode = FileLogSink::FlushMode::Buffered,
                                     std::size_t max_bytes = 0, int max_files = 3);

// Changes the default severity level at runtime (an analogue of
// Logger::setDefaultLevel in the original assignment).
LOGGER_API void setDefaultLevel(Level level);

// Flushes buffered file journal entries to the file
// (see FileLogSink::flush).
LOGGER_API void flushFileJournal();

}  // namespace logger