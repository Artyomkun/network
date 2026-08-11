#pragma once

#include <string>

#include "logger/export.hpp"
#include "logger/record.hpp"

namespace logger {

// Text representation of a journal entry for the socket protocol:
//   [2026-08-11 12:34:56] [INFO] message
// Used by SocketLogSink (sender) and stats_app (receiver).
LOGGER_API std::string formatRecord(const Record& record);

// Reverse parsing of a line produced by formatRecord.
// Returns false if the line does not match the format.
LOGGER_API bool parseRecord(const std::string& line, Record& out);

}  // namespace logger