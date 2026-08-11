// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include <string>

#include "absl/base/log_severity.h"
#include "logger/level.hpp"

#include "test_framework.hpp"

TEST("parseLevel accepts all levels in any case") {
    logger::Level level;
    CHECK(logger::parseLevel("debug", level));
    CHECK(level == logger::Level::Debug);
    CHECK(logger::parseLevel("INFO", level));
    CHECK(level == logger::Level::Info);
    CHECK(logger::parseLevel("Error", level));
    CHECK(level == logger::Level::Error);
    CHECK(logger::parseLevel("DEBUG", level));
    CHECK(level == logger::Level::Debug);
}

TEST("parseLevel rejects unknown levels") {
    logger::Level level = logger::Level::Info;
    CHECK(!logger::parseLevel("warning", level));
    CHECK(!logger::parseLevel("", level));
    CHECK(!logger::parseLevel("info3", level));
    CHECK(!logger::parseLevel("information", level));
    CHECK(level == logger::Level::Info);
}

TEST("toString returns uppercase names") {
    CHECK_EQ(std::string(logger::toString(logger::Level::Debug)), "DEBUG");
    CHECK_EQ(std::string(logger::toString(logger::Level::Info)), "INFO");
    CHECK_EQ(std::string(logger::toString(logger::Level::Error)), "ERROR");
}

TEST("toAbslSeverity preserves the ordering of levels") {
    CHECK(logger::toAbslSeverity(logger::Level::Debug) == absl::LogSeverity::kInfo);
    CHECK(logger::toAbslSeverity(logger::Level::Info) == absl::LogSeverity::kWarning);
    CHECK(logger::toAbslSeverity(logger::Level::Error) == absl::LogSeverity::kError);
}

TEST("fromAbslSeverity round-trips all levels") {
    CHECK(logger::fromAbslSeverity(logger::toAbslSeverity(logger::Level::Debug)) ==
          logger::Level::Debug);
    CHECK(logger::fromAbslSeverity(logger::toAbslSeverity(logger::Level::Info)) ==
          logger::Level::Info);
    CHECK(logger::fromAbslSeverity(logger::toAbslSeverity(logger::Level::Error)) ==
          logger::Level::Error);
    CHECK(logger::fromAbslSeverity(absl::LogSeverity::kFatal) == logger::Level::Error);
}
