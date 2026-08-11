#include "logger/socket_sink.hpp"

#include <chrono>
#include <mutex>
#include <string>

#include "absl/log/log_entry.h"
#include "absl/time/clock.h"
#include "logger/format.hpp"
#include "logger/socket.hpp"

namespace logger {

class SocketImpl {
public:
    SocketImpl(const std::string& host, int port) : host_(host), port_(port) {
        // Winsock is initialized by the sink itself (WSAStartup keeps a call
        // counter, repeated calls are safe).
        socketInit();
    }

    ~SocketImpl() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connected_) {
            tcpClose(socket_);
        }
        socketCleanup();
    }

    void send(const absl::LogEntry& entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ensureConnected()) {
            return;
        }

        Record record;
        record.message = entry.text_message();
        record.level = fromAbslSeverity(entry.log_severity());
        record.timestamp = absl::ToChronoTime(entry.timestamp());
        const std::string line = formatRecord(record) + "\n";

        if (!tcpSend(socket_, line.data(), static_cast<int>(line.size()))) {
            // Connection broken: close and reconnect on the next message.
            // Lost entries are not resurrected - the journal is kept in the
            // file, the socket only mirrors the stream.
            tcpClose(socket_);
            socket_ = kInvalidSocket;
            connected_ = false;
        }
    }

private:
    bool ensureConnected() {
        if (connected_) {
            return true;
        }
        bool ok = false;
        socket_ = tcpConnect(host_, port_, ok);
        connected_ = ok;
        return connected_;
    }

    std::string host_;
    int port_;
    std::mutex mutex_;
    socket_handle socket_ = kInvalidSocket;
    bool connected_ = false;
};

SocketLogSink::SocketLogSink(const std::string& host, int port)
    : impl_(new SocketImpl(host, port)) {}

SocketLogSink::~SocketLogSink() = default;

void SocketLogSink::Send(const absl::LogEntry& entry) {
    impl_->send(entry);
}

}  // namespace logger