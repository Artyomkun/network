// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include "absl/log/globals.h"
#include "absl/log/initialize.h"

#include "test_framework.hpp"

int main() {
    // Without InitializeLog absl writes test messages to stderr even below
    // the severity threshold (see bench/bench_logger.cpp, same quirk).
    absl::InitializeLog();
    absl::SetStderrThreshold(absl::LogSeverityAtLeast(absl::LogSeverity::kFatal));
    return testfw::runAll() == 0 ? 0 : 1;
}