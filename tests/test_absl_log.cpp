// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/log/log_sink_registry.h"
#include "logger/file_sink.hpp"
#include "logger/level.hpp"
#include "logger/logger.hpp"

#include "test_framework.hpp"
#include "test_util.hpp"

namespace {

void logAt(logger::Level level, const std::string& message) {
    LOG(LEVEL(logger::toAbslSeverity(level))) << message;
}

}  // namespace

TEST("configureFileJournal writes all severities to the file") {
    const std::string path = testutil::tempPath("absl_write.log");
    CHECK(logger::configureFileJournal(path, logger::Level::Debug));

    logAt(logger::Level::Debug, "hello debug");
    logAt(logger::Level::Info, "hello info");
    logAt(logger::Level::Error, "hello error");

    logger::flushFileJournal();
    const std::vector<std::string> lines = testutil::readLines(path);
    CHECK_EQ(lines.size(), 3u);
    CHECK(lines[0].find("hello debug") != std::string::npos);
    CHECK(lines[1].find("hello info") != std::string::npos);
    CHECK(lines[2].find("hello error") != std::string::npos);
}

TEST("configureFileJournal reports an invalid path") {
    CHECK(!logger::configureFileJournal("Z:/nonexistent_dir/subdir/missing.log",
                                        logger::Level::Info));
}

TEST("messages below the default level are filtered") {
    const std::string path = testutil::tempPath("absl_filter.log");
    CHECK(logger::configureFileJournal(path, logger::Level::Info));

    logAt(logger::Level::Debug, "too noisy");
    logAt(logger::Level::Info, "ok info");
    logAt(logger::Level::Error, "ok error");

    logger::flushFileJournal();
    const std::vector<std::string> lines = testutil::readLines(path);
    CHECK_EQ(lines.size(), 2u);
    CHECK(lines[0].find("ok info") != std::string::npos);
    CHECK(lines[1].find("ok error") != std::string::npos);
}

TEST("setDefaultLevel changes the threshold at runtime") {
    const std::string path = testutil::tempPath("absl_relevel.log");
    CHECK(logger::configureFileJournal(path, logger::Level::Debug));

    logAt(logger::Level::Debug, "before");
    logger::setDefaultLevel(logger::Level::Error);
    logAt(logger::Level::Debug, "after debug");
    logAt(logger::Level::Info, "after info");
    logAt(logger::Level::Error, "after error");

    logger::flushFileJournal();
    const std::vector<std::string> lines = testutil::readLines(path);
    CHECK_EQ(lines.size(), 2u);
    CHECK(lines[0].find("before") != std::string::npos);
    CHECK(lines[1].find("after error") != std::string::npos);
}

TEST("records contain the severity and the timestamp") {
    const std::string path = testutil::tempPath("absl_format.log");
    CHECK(logger::configureFileJournal(path, logger::Level::Debug));

    logAt(logger::Level::Info, "formatted line");
    logger::flushFileJournal();
    const std::vector<std::string> lines = testutil::readLines(path);
    CHECK_EQ(lines.size(), 1u);
    // The line is in the format [YYYY-MM-DD HH:MM:SS] [LEVEL] message.
    CHECK(lines[0][0] == '[');
    CHECK(lines[0].find("[2026-") != std::string::npos);
    CHECK(lines[0].find("[INFO]") != std::string::npos);
    CHECK(lines[0].find("formatted line") != std::string::npos);
}

TEST("logging from multiple threads is safe") {
    const std::string path = testutil::tempPath("absl_threads.log");
    CHECK(logger::configureFileJournal(path, logger::Level::Debug));

    const int threads_count = 8;
    const int messages_per_thread = 100;
    std::vector<std::thread> threads;
    for (int i = 0; i < threads_count; ++i) {
        threads.emplace_back([i, messages_per_thread] {
            for (int j = 0; j < messages_per_thread; ++j) {
                logAt(logger::Level::Info, "thread " + std::to_string(i) + " message " +
                                               std::to_string(j));
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    logger::flushFileJournal();
    CHECK_EQ(testutil::countLines(path),
             static_cast<std::size_t>(threads_count * messages_per_thread));
}

TEST("FileLogSink rotates the journal by size and discards the oldest copy") {
    const std::string path = testutil::tempPath("absl_rotate.log");
    const std::size_t max_bytes = 128;
    const int max_files = 2;

    logger::FileLogSink sink(path, logger::FileLogSink::FlushMode::Immediate,
                             max_bytes, max_files);
    CHECK(sink.good());
    absl::AddLogSink(&sink);

    // ~2500 bytes at a 128-byte threshold - rotation triggers several times.
    for (int i = 0; i < 50; ++i) {
        LOG(LEVEL(absl::LogSeverity::kInfo)) << "rotation test payload";
    }
    absl::RemoveLogSink(&sink);

    // The current file and the rotated copies exist.
    CHECK(std::filesystem::exists(path));
    CHECK(std::filesystem::exists(path + ".1"));
    CHECK(std::filesystem::exists(path + ".2"));
    // No more than max_files copies: a third one must not exist.
    CHECK(!std::filesystem::exists(path + ".3"));

    // No file exceeds the threshold (any file is rotated as soon as its size
    // plus the next line crosses max_bytes).
    for (int i = 0; i <= max_files; ++i) {
        const std::string rotated = (i == 0) ? path : path + "." + std::to_string(i);
        std::error_code ec;
        const std::uintmax_t size = std::filesystem::file_size(rotated, ec);
        CHECK(!ec);
        CHECK(size <= max_bytes + 128);
    }
}