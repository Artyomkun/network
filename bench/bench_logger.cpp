// Micro-benchmarks of logging on absl::log.
//
// Measures the median latency of one operation (ns/op) and the throughput
// (ops/s):
//   1. string formatting (ostringstream - C++17 path, std::format - C++20
//      path, available when built under C++20+);
//   2. full LOG(): filtered message, discard sink, file write;
//   3. multithreaded writing;
//   4. file vs socket (local TCP).
//
// To compare standards, configure the project twice:
//   cmake -S . -B build17 -DLOGGER_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release
//   cmake -S . -B build20 -DLOGGER_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Release
// and run buildXX/bench/bench_logger.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "logger/level.hpp"
#include "logger/logger.hpp"
#include "logger/platform.hpp"
#include "logger/record.hpp"
#include "logger/socket.hpp"
#include "logger/socket_sink.hpp"

#if LOGGER_HAS_STD_FORMAT
#include <format>
#endif

namespace {

using Clock = std::chrono::steady_clock;

struct Result {
    std::string name;
    double ns_per_op;
};

// Warms the call up and returns the median latency of one operation over
// samples measurements of iterations operations each.
template <typename Fn>
double medianNs(int samples, int iterations, Fn&& fn) {
    for (int i = 0; i < 3; ++i) {
        fn();
    }
    std::vector<double> times;
    times.reserve(static_cast<std::size_t>(samples));
    for (int s = 0; s < samples; ++s) {
        const Clock::time_point start = Clock::now();
        for (int i = 0; i < iterations; ++i) {
            fn();
        }
        const Clock::time_point finish = Clock::now();
        times.push_back(std::chrono::duration<double, std::nano>(finish - start).count() /
                        static_cast<double>(iterations));
    }
    std::nth_element(times.begin(), times.begin() + static_cast<std::ptrdiff_t>(times.size() / 2),
                     times.end());
    return times[times.size() / 2];
}

void printHeader() {
#if defined(LOGGER_PLATFORM_WINDOWS)
    std::printf("platform : windows\n");
#else
    std::printf("platform : posix\n");
#endif
    std::printf("standard : c++%ld\n", static_cast<long>(__cplusplus / 100 % 100));
    std::printf("logger   : absl::log\n");
    std::printf("%-46s %12s %12s\n", "benchmark", "ns/op", "ops/s");
}

void printResult(const Result& result) {
    std::printf("%-46s %12.1f %12.0f\n", result.name.c_str(), result.ns_per_op,
                1.0e9 / result.ns_per_op);
}

std::string timeString(const std::chrono::system_clock::time_point& point) {
    const std::time_t raw = std::chrono::system_clock::to_time_t(point);
    std::tm local{};
#if defined(LOGGER_PLATFORM_WINDOWS)
    localtime_s(&local, &raw);
#else
    localtime_r(&raw, &local);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
    return std::string(buffer);
}

// --- Formatters: comparing formatting paths across standard versions ----------

std::string formatLegacy(const std::string& message) {
    std::ostringstream out;
    out << '[' << timeString(std::chrono::system_clock::now()) << "] [INFO] " << message;
    return out.str();
}

std::string formatFast(const std::string& message) {
    const std::string time = timeString(std::chrono::system_clock::now());
    std::string out;
    out.reserve(time.size() + message.size() + 12);
    out += "[";
    out += time;
    out += "] [INFO] ";
    out += message;
    return out;
}

#if LOGGER_HAS_STD_FORMAT
std::string formatStd(const std::string& message) {
    return std::format("[{}] [INFO] {}", timeString(std::chrono::system_clock::now()),
                       message);
}
#endif

void benchFormatters() {
    const std::string message = "benchmark message with a realistic length";

    printResult({"format: ostringstream (classic)", medianNs(11, 2000, [&] {
                     volatile int sink = static_cast<int>(formatLegacy(message).size());
                     (void)sink;
                 })});
    printResult({"format: manual append + reserve (C++17)", medianNs(11, 2000, [&] {
                     volatile int sink = static_cast<int>(formatFast(message).size());
                     (void)sink;
                 })});
#if LOGGER_HAS_STD_FORMAT
    printResult({"format: std::format (C++20)", medianNs(11, 2000, [&] {
                     volatile int sink = static_cast<int>(formatStd(message).size());
                     (void)sink;
                 })});
#endif
}

// --- Logging through absl::log ----------------------------------------------

constexpr const char* kBenchMessage = "benchmark message with a realistic length";

class DiscardSink : public absl::LogSink {
public:
    void Send(const absl::LogEntry&) override {}
};

void benchLogOverhead() {
    // Filtering: threshold higher than the message severity (kInfo < kWarning).
    absl::SetMinLogLevel(absl::LogSeverityAtLeast(absl::LogSeverity::kWarning));

    printResult({"log: filtered (below min level)", medianNs(11, 2000, [&] {
                     LOG(LEVEL(absl::LogSeverity::kInfo)) << kBenchMessage;
                 })});

    absl::SetMinLogLevel(absl::LogSeverityAtLeast(logger::toAbslSeverity(logger::Level::Debug)));
    DiscardSink sink;
    absl::AddLogSink(&sink);
    printResult({"log: discard sink (full path)", medianNs(11, 2000, [&] {
                     LOG(LEVEL(absl::LogSeverity::kWarning)) << kBenchMessage;
                 })});
    absl::RemoveLogSink(&sink);
}

void benchFileSink(const std::string& path) {
    if (!logger::configureFileJournal(path, logger::Level::Debug,
                                      logger::FileLogSink::FlushMode::Immediate)) {
        std::printf("  WARNING: cannot open %s, file benchmark skipped\n", path.c_str());
        return;
    }
    printResult({"log: file sink (flush per message)", medianNs(9, 200, [&] {
                     LOG(LEVEL(absl::LogSeverity::kWarning)) << kBenchMessage;
                 })});

    if (!logger::configureFileJournal(path, logger::Level::Debug,
                                      logger::FileLogSink::FlushMode::Buffered)) {
        std::printf("  WARNING: cannot reopen %s, buffered file benchmark skipped\n", path.c_str());
        return;
    }
    // 2000 records - the 64 KB buffer overflows several times per measurement,
    // so the amortized flush cost is honestly included in the number.
    printResult({"log: file sink (buffered 64K)", medianNs(11, 2000, [&] {
                     LOG(LEVEL(absl::LogSeverity::kWarning)) << kBenchMessage;
                 })});
}

void benchThreads() {
    absl::SetMinLogLevel(absl::LogSeverityAtLeast(logger::toAbslSeverity(logger::Level::Debug)));

    const int thread_count = 4;
    const int per_thread = 10'000;
    const auto start = Clock::now();
    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([per_thread] {
            for (int j = 0; j < per_thread; ++j) {
                LOG(LEVEL(absl::LogSeverity::kWarning)) << kBenchMessage;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    const auto finish = Clock::now();
    const double total = static_cast<double>(thread_count * per_thread);
    const double ns = std::chrono::duration<double, std::nano>(finish - start).count() / total;
    printResult({"log: 4 threads (contention)", ns});
}

void benchSocket() {
    logger::socketInit();
    const int port = 44770;

    bool ok = false;
    logger::SocketGuard listener(logger::tcpListen("127.0.0.1", port, ok));
    if (!ok) {
        std::printf("  WARNING: cannot bind port %d, socket benchmark skipped\n", port);
        logger::socketCleanup();
        return;
    }

    // The server thread reads and discards all data.
    bool accepted = false;
    std::thread server([&listener, &accepted] {
        logger::SocketGuard conn(logger::tcpAccept(listener.get(), accepted));
        if (!accepted) {
            return;
        }
        char buffer[4096];
        while (logger::tcpReceive(conn.get(), buffer, static_cast<int>(sizeof(buffer))) > 0) {
        }
    });

    {
        logger::SocketLogSink sink("127.0.0.1", port);
        absl::AddLogSink(&sink);

        absl::SetMinLogLevel(absl::LogSeverityAtLeast(logger::toAbslSeverity(logger::Level::Debug)));
        printResult({"log: socket sink (localhost TCP)", medianNs(7, 500, [&] {
                         LOG(LEVEL(absl::LogSeverity::kWarning)) << kBenchMessage;
                     })});

        absl::RemoveLogSink(&sink);
    }  // the sink destructor closes the client socket; only then does the
       // server thread finish in recv and join() unblock.
    server.join();
    // The listener is closed by the SocketGuard destructor.
    logger::socketCleanup();
}

}  // namespace

int main() {
    // Unbuffered output: when redirected to a file or a pipe the benchmark
    // progress is still visible immediately.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    // Without InitializeLog absl writes ALL messages to stderr (regardless
    // of SetStderrThreshold). Initialization enables threshold filtering.
    absl::InitializeLog();
    absl::SetStderrThreshold(absl::LogSeverityAtLeast(absl::LogSeverity::kFatal));

    printHeader();

    benchFormatters();
    benchLogOverhead();
    benchFileSink((std::filesystem::temp_directory_path() / "logger_bench.log").string());
    benchThreads();
    benchSocket();

    return 0;
}