// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <string>
#include <thread>

#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "logger/level.hpp"
#include "logger/logger.hpp"
#include "logger/record.hpp"

#include "message_queue.hpp"

namespace {

// Limits that bound journal resource growth: the in-memory queue (push
// blocks when full) and the on-disk file rotation threshold.
constexpr std::size_t kQueueLimit = 1024;
constexpr std::size_t kJournalMaxBytes = 16 * 1024 * 1024;
constexpr int kJournalMaxFiles = 3;

void printUsage(const char* program) {
    std::cerr << "Usage: " << program << " <logfile> [level]\n"
              << "  logfile - path to the journal file\n"
              << "  level   - default severity level (debug|info|error), default: info\n"
              << "Input: [level:] message, e.g. \"error: something failed\"\n"
              << "or just a message. Messages below the default level are ignored.\n"
              << "Empty line or 'exit' quits the program.\n";
}

// Parses user input: "[level:] message" or "message".
// If the level prefix is not recognized, the whole input is treated as a
// message at the default level.
// Peculiarity: a line starting with ':', e.g. ": hello", is not a prefix
// (an empty level name before the colon is invalid) and is also written to
// the journal as a whole message.
logger::Record parseInput(const std::string& line, logger::Level default_level) {
    logger::Record record;
    record.level = default_level;

    const std::string::size_type colon = line.find(':');
    if (colon != std::string::npos) {
        logger::Level level;
        if (logger::parseLevel(line.substr(0, colon), level)) {
            record.level = level;
            std::string message = line.substr(colon + 1);
            const std::string::size_type first = message.find_first_not_of(' ');
            if (first != std::string::npos) {
                message.erase(0, first);
            }
            record.message = message;
            return record;
        }
    }
    record.message = line;
    return record;
}

// Consumer thread: takes entries from the queue and writes them to the
// journal via absl::log. Level filtering is done by absl::log itself (the
// threshold is set in configureFileJournal); a dedicated writer thread lets
// the main thread avoid waiting on disk I/O.
void runWorker(app::MessageQueue& queue) {
    for (;;) {
        const std::optional<logger::Record> record = queue.pop();
        if (!record) {
            break;
        }
        LOG(LEVEL(logger::toAbslSeverity(record->level))) << record->message;
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        printUsage(argv[0]);
        return 1;
    }

    logger::Level default_level = logger::Level::Info;
    if (argc == 3 && !logger::parseLevel(argv[2], default_level)) {
        std::cerr << "Unknown level: " << argv[2] << "\n";
        printUsage(argv[0]);
        return 1;
    }

    absl::InitializeLog();
    // The journal is kept only in the file - do not duplicate messages in stderr.
    absl::SetStderrThreshold(absl::LogSeverityAtLeast(absl::LogSeverity::kFatal));
    // Rotation at 16 MB keeping 3 old files: the journal on disk does not
    // grow without limit.
    if (!logger::configureFileJournal(argv[1], default_level,
                                      logger::FileLogSink::FlushMode::Buffered,
                                      kJournalMaxBytes, kJournalMaxFiles)) {
        std::cerr << "Cannot open log file: " << argv[1] << "\n";
        return 1;
    }

    app::MessageQueue queue(kQueueLimit);
    std::thread worker(runWorker, std::ref(queue));

    std::cout << "Journal: " << argv[1]
              << ", default level: " << logger::toString(default_level) << "\n"
              << "Enter messages ([level:] message; empty line or 'exit' to quit):\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line == "exit") {
            break;
        }
        if (!queue.push(parseInput(line, default_level))) {
            break;  // the queue was stopped
        }
    }

    queue.stop();
    worker.join();
    return 0;
}
