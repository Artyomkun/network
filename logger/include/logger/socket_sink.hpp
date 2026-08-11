// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

#include "absl/log/log_sink.h"
#include "logger/export.hpp"

namespace logger {

class SocketImpl;

// absl::log sink that sends every message to a TCP socket.
// The connection is established on the first message and restored after a
// break. Lines on the wire use logger::formatRecord - the same format the
// statistics application reads.
class LOGGER_API SocketLogSink : public absl::LogSink {
public:
    SocketLogSink(const std::string& host, int port);
    ~SocketLogSink() override;

    SocketLogSink(const SocketLogSink&) = delete;
    SocketLogSink& operator=(const SocketLogSink&) = delete;

    void Send(const absl::LogEntry& entry) override;

private:
    std::unique_ptr<SocketImpl> impl_;
};

}  // namespace logger
