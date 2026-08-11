// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>

#include "logger/format.hpp"
#include "logger/socket.hpp"

#include "statistics.hpp"

namespace {

void printUsage(const char* program) {
    std::cerr << "Usage: " << program << " <host> <port> <N> <T>\n"
              << "  host - interface to listen on (e.g. 127.0.0.1, * for all)\n"
              << "  port - TCP port to listen on\n"
              << "  N    - print statistics after every N-th message\n"
              << "  T    - print statistics on timeout T seconds\n"
              << "         if it changed since the last output\n";
}

// Parses a string into an integer: only a fully numeric string is accepted
// (no trailing garbage like "5abc"). Returns false on any error, including
// overflow.
bool parseArg(const char* text, long long& value) {
    if (text == nullptr || *text == '\0') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const long long parsed = std::strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }
    value = parsed;
    return true;
}

// Prints the statistics if they changed since the last output.
void printIfChanged(const stats::Statistics& statistics, std::size_t& last_printed) {
    if (statistics.total() != last_printed) {
        statistics.print();
        last_printed = statistics.total();
    }
}

// Handles one log line received from the socket:
// prints it to the console and adds it to the statistics.
void processLine(const std::string& line, stats::Statistics& statistics,
                 std::size_t n, std::size_t& last_printed) {
    logger::Record record;
    if (!logger::parseRecord(line, record)) {
        std::cout << "[unknown format] " << line << std::endl;
        return;
    }
    std::cout << line << std::endl;
    statistics.add(record);

    if (statistics.total() % n == 0) {
        printIfChanged(statistics, last_printed);
    }
}

// Splits the buffer into lines and feeds them to processLine.
void processBuffer(std::string& buffer, stats::Statistics& statistics,
                   std::size_t n, std::size_t& last_printed) {
    std::string::size_type pos;
    while ((pos = buffer.find('\n')) != std::string::npos) {
        processLine(buffer.substr(0, pos), statistics, n, last_printed);
        buffer.erase(0, pos + 1);
    }
    // Protection against unbounded buffer growth when no newline arrives.
    if (buffer.size() > 65536) {
        buffer.clear();
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string host = argv[1];

    long long parsed = 0;
    if (!parseArg(argv[2], parsed) || parsed <= 0 || parsed > 65535) {
        std::cerr << "Invalid port: " << argv[2] << " (must be 1..65535)\n";
        return 1;
    }
    const int port = static_cast<int>(parsed);

    if (!parseArg(argv[3], parsed) || parsed <= 0) {
        std::cerr << "Invalid N: " << argv[3] << " (must be a positive integer)\n";
        return 1;
    }
    const std::size_t n = static_cast<std::size_t>(parsed);

    if (!parseArg(argv[4], parsed) || parsed < 0) {
        std::cerr << "Invalid T: " << argv[4] << " (must be a non-negative integer)\n";
        return 1;
    }
    // tcpWait accepts milliseconds in int: large T must not overflow the
    // multiplication by 1000 or the int range.
    const long long timeout_ms = parsed * 1000;
    const int timeout_ms_clamped =
        timeout_ms > 2147483647LL ? 2147483647 : static_cast<int>(timeout_ms);

    if (!logger::socketInit()) {
        std::cerr << "Failed to initialize sockets\n";
        return 1;
    }

    bool ok = false;
    logger::SocketGuard listener(logger::tcpListen(host, port, ok));
    if (!ok) {
        std::cerr << "Cannot listen on " << host << ":" << port << "\n";
        logger::socketCleanup();
        return 1;
    }

    std::cout << "Listening on " << host << ":" << port
              << " (print after every " << n << " messages, timeout " << parsed
              << "s). Press Ctrl+C to stop." << std::endl;

    stats::Statistics statistics;
    std::size_t last_printed = 0;

    for (;;) {
        ok = false;
        // The conn destructor closes the socket at the end of the iteration -
        // including when an exception escapes the handler.
        logger::SocketGuard conn(logger::tcpAccept(listener.get(), ok));
        if (!ok) {
            std::cerr << "accept failed\n";
            break;
        }
        std::cout << "Client connected" << std::endl;

        std::string buffer;
        for (;;) {
            if (logger::tcpWait(conn.get(), timeout_ms_clamped)) {
                char data[4096];
                const int received = logger::tcpReceive(conn.get(), data, sizeof(data) - 1);
                if (received > 0) {
                    buffer.append(data, static_cast<std::size_t>(received));
                    processBuffer(buffer, statistics, n, last_printed);
                } else {
                    // The connection is closed or an error occurred.
                    break;
                }
            } else {
                // Timeout: print the statistics if they changed since the
                // last output.
                printIfChanged(statistics, last_printed);
            }
        }

        // The last line without '\n' (a cut mid-record) would otherwise wait
        // for the next packet; on disconnect we parse the remainder so the
        // tail is not lost.
        if (!buffer.empty()) {
            processBuffer(buffer, statistics, n, last_printed);
        }

        std::cout << "Client disconnected" << std::endl;
    }

    logger::socketCleanup();
    return 0;
}
