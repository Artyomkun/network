# Logging library and demo applications

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

Test assignment for the "C++ Developer (intern)" position: a library for
writing messages to a journal with different severity levels, plus
applications demonstrating it.

- Language: C++17 (a C++20 build is also available), the standard library
  plus absl::log - the only agreed exception to the "STL only" rule
  (see "Why the journal is built on absl::log"). Networking uses the
  system APIs (POSIX/Winsock).
- Build: CMake, GCC (primary), MSVC/MinGW - in CI. Target OS: current
  ubuntu/debian releases.
- Neither the library nor the applications throw exceptions
  (business logic without `try/catch`).

## Why the journal is built on absl::log, not written from scratch

Writing our own logger from scratch would be reinventing the wheel:
absl::log is battle-tested by thousands of projects and covers the low-level
part (message registration, formatting, level filtering, thread safety).
Using absl::log **was agreed with the customer as the only exception** to the
"STL only" rule. Everything else - the journal API, the record format, the
sinks, the applications - is written in this project:

1. **Own API**: `logger::configureFileJournal` (file, default level, flush
   mode, rotation), `setDefaultLevel`, `flushFileJournal`, levels
   `logger::Level { Debug, Info, Error }`.
2. **Own record format and its parser**: `formatRecord` / `parseRecord`
   (lines like `[2026-08-11 20:05:11] [INFO] message`).
3. **Own sinks**: `FileLogSink` (64 KB buffer, size-based rotation, loss
   accounting on write errors) and `SocketLogSink` (TCP, reconnects after
   a break).
4. **Demo applications and tests**: a queue with backpressure, a statistics
   collector, 26 unit tests.

Why not spdlog/Boost.Log: they add nothing over absl::log for this task but
drag in extra dependencies.

Performance is not critical for the assignment: even the "slow" stream-based
formatting path reaches ~1 million records per second (see "Benchmarks");
what can be done to speed up file writes is described there as well.

## Project layout

```
├── CMakeLists.txt          # root build file, common options
├── logger/                 # Part 1: the journal library
│   ├── include/logger/     #   public headers (interface)
│   └── src/                #   implementation
├── app/                    # Part 2: console multithreaded application
├── stats/                  # Part 3*: collecting statistics over socket data
└── tests/                  # unit tests (no third-party libraries)
```

## Building

Static library (default):

```bash
cmake -S . -B build
cmake --build build -j
```

Shared library:

```bash
cmake -S . -B build-shared -DLOGGER_BUILD_SHARED=ON
cmake --build build-shared -j
```

CMake options:

| Option | Default | Purpose |
| --- | --- | --- |
| `LOGGER_BUILD_SHARED` | `OFF` | build the library as a shared one |
| `LOGGER_ENABLE_SOCKETS` | `ON` | the socket sink (Part 1.5) and the stats application (Part 3) |
| `LOGGER_BUILD_TESTS` | `ON` | build unit tests |

Separate build targets:

- `logger` - the library (static `liblogger.a` or shared `liblogger.so`);
- `journal_app` - the Part 2 application;
- `stats_app` - the Part 3* application;
- `logger_tests` - unit tests.

## Continuous integration (CI)

Every push and pull request is built and tested on GitHub Actions:
Linux, Windows and macOS runners (C++17 and C++20), Debian bookworm and
trixie containers, plus a strict `-Werror` job.

### Why RISC-V is not in CI

GitHub Actions has no native RISC-V runners. The only way to run the
tests on RISC-V would be QEMU emulation (a `linux/riscv64` container
under `docker/setup-qemu-action`), which would make the abseil build and
the test run several times slower and would not catch any real defect:
the code is standard C++ plus POSIX/Winsock and is architecture-agnostic
by design. Cross-compiling without a runner would verify only the compile
step, not the tests, so it was not added either.

### Why a separate "CISC" job is not in CI

"RISC vs CISC" is already covered by the existing matrix: all current
runners (ubuntu, windows, macOS) are x86-64 - a CISC architecture, and
the macOS arm64 runner is RISC. A 32-bit x86 (i686) job would add no
mainstream runner to the matrix and is not a target of the assignment;
the code uses `size_t` and standard types and does not exercise any
32-bit-specific behavior. An i386 container was considered but only
doubles the build time without verifying anything new.

## Running

### Part 2: console journal application

```
journal_app <logfile> [level]
```

- `logfile` - journal file path;
- `level` - default severity level (`debug` | `info` | `error`), default `info`.

The user enters messages in the `[level:] message` format. If the level
prefix is omitted, the default level is used. Each message is handed to a
dedicated writer thread through a thread-safe queue. An empty line or `exit`
quits the application.

```bash
$ ./journal_app app.log info
Journal: app.log, default level: INFO
Enter messages ([level:] message; empty line or 'exit' to quit):
error: database is unreachable
fetching user data
info: login ok
exit
```

The contents of `app.log`:

```
[2026-08-11 20:05:11] [ERROR] database is unreachable
[2026-08-11 20:05:13] [INFO] fetching user data
[2026-08-11 20:05:14] [INFO] login ok
```

Messages below the default level (e.g. `debug:` at level `info`) are not
written to the journal.

### Part 1.5: writing the journal to a socket

The socket sink is plugged into the journal through the absl::log sink
registry and mirrors everything written to the file:

```cpp
#include <absl/log/log.h>
#include <absl/log/log_sink_registry.h>
#include <logger/socket_sink.hpp>

logger::SocketLogSink sink("127.0.0.1", 9000);
absl::AddLogSink(&sink);          // all LOG() calls are now mirrored to the socket
LOG(INFO) << "something went wrong";
absl::RemoveLogSink(&sink);       // must be called BEFORE the sink is destroyed
```

The connection is established on the first write and restored after a break.

### Part 3*: collecting statistics over socket data

```
stats_app <host> <port> <N> <T>
```

- `host` - interface to listen on (`127.0.0.1`, `*` - all interfaces);
- `port` - TCP port;
- `N` - print statistics after every N-th received message;
- `T` - print statistics on timeout T seconds if they changed since the
  last output.

The application receives log messages (in the library format), prints each
message to the console and keeps statistics: total message count, per-level
counts, last-hour count, and minimum/maximum/average message lengths.

```bash
$ ./stats_app 127.0.0.1 9000 100 5
```

## Journal record format

Each record is a single line:

```
[2026-08-11 20:05:11] [ERROR] message text
      ^receive time      ^level  ^text
```

Formatting and parsing live in `logger::formatRecord` / `logger::parseRecord`.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

The unit tests cover: level parsing, file writing (including multithreaded),
record formatting/parsing, level filtering and runtime level changes, logger
thread safety, statistics, the message queue (including capacity bounding)
and journal rotation. The test framework is our own (no third-party
libraries), 26 tests.

## Benchmarks

Micro-benchmarks (`bench/bench_logger`) measure the median latency of one
operation. To compare standards fairly, configure the project twice and run
both binaries:

```bash
cmake -S . -B build17 -DLOGGER_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release
cmake -S . -B build20 -DLOGGER_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Release
./build17/bench/bench_logger
./build20/bench/bench_logger
```

Example results (Windows 10, GCC 16.1 mingw-w64, Release, `-O3`, median of
three runs):

| benchmark | C++17, ns/op | C++20, ns/op |
| --- | ---: | ---: |
| `format: ostringstream (classic)` | 1190 | 1175 |
| `format: manual append + reserve` | 903 | 918 |
| `format: std::format` | — | 1039 |
| `log: filtered (below min level)` | 620 | 605 |
| `log: discard sink (full path)` | 873 | 895 |
| `log: file sink (flush per message)` | 9735 | 10655 |
| `log: file sink (buffered 64K)` | 2424 | 2280 |
| `log: 4 threads (contention)` | 2760 | 3560 |
| `log: socket sink (localhost TCP)` | 18178 | 23930 |

What this means:

- The full `LOG()` path with a discard sink (level filtering + time prefix +
  delivery) is about 0.9 us, i.e. ~1.1M records/s.
- Flushing per message costs ~10 us (a system call). By default the file
  sink runs in `Buffered` mode: it accumulates records in memory and flushes
  the 64 KB buffer at once (or by a 1 s timer), so the system call is
  amortized over ~500 records and a write costs ~2.2 us (comparable to the
  full path + formatting). The delay before a record appears in the file is
  at most one second.
- Time formatting (local `localtime` + `strftime` conversion) is the most
  expensive operation after I/O; it is cached per second and runs at most
  once a second.
- Multithreaded writes are bounded by the sink's internal mutex: 4 threads
  give ~300K records/s at ~3 us latency. The sink is designed to be
  thread-safe (ordered writes), not to serialize at the logger level.
- The socket is the most expensive path (~18-24 us): one `send` system call
  per record. Acceptable for remote journal aggregation.

### What can be done to speed up file writes

The numbers above were obtained without special tricks. If file writes
become the bottleneck, performance can be raised in increasing order of
complexity:

| Technique | Expected effect | Cost |
| --- | --- | --- |
| Bigger flush buffer (currently 64 KB, e.g. 1 MB) | the system call fires even less often | +up to 1 MB memory |
| Longer flush interval (currently 1 s) | more records per single flush | record delivery delay |
| Dedicated writer thread: the sink queues lines, a writer thread performs the write | logging threads do not wait on system calls | one more queue, loss risk on crash |
| Packet writes via `writev`/`iovec` | all buffered lines in one call | noticeably more complex code |
| `mmap`-backed file: write to memory, the kernel flushes pages | zero copies and no system calls on the hot path | harder flush and rotation control |
| `io_uring` (Linux) / IOCP (Windows) | fully asynchronous writes | overkill for this task |

The golden rule: do not use `Immediate` mode without a reason - flushing
the buffer per message costs ~10 us versus ~2 us for buffered writes.

## Library architecture

```
absl::log ──► the absl::LogSink interface (sink registry)
                 ├── FileLogSink    - text file writes
                 │                    (64 KB buffer, rotation, loss accounting)
                 └── SocketLogSink  - TCP sends
                                      (reconnect after a break)
```

- `logger/logger.hpp` - journal configuration facade: `configureFileJournal`
  (file, default level, flush mode, size-based rotation), `setDefaultLevel`
  (runtime level change), `flushFileJournal`. The write itself is done by
  the standard `absl::LOG(severity)`.
- `FileLogSink` / `SocketLogSink` - sinks derived from `absl::LogSink`,
  thread-safe (internal `std::mutex`). Adding a new write destination is
  just a new `LogSink` class.
- The record format and its parser - free functions `formatRecord` /
  `parseRecord`.
- Errors are reported through return values (`bool`) and the `good()` state,
  without exceptions.
- Levels are given by the `logger::Level { Debug, Info, Error }` enum.

## Assignment compliance

- Part 1: library with a file journal; 2 build variants (static/shared);
  initialization parameters - file name and default level; the journal
  stores text, level, time; default level can change at runtime.
- Part 1.5*: `SocketLogSink` with the same interface as the file one.
- Part 2: multithreaded console application; thread-safe data transfer;
  waits for new input; parameters - file name and default level.
- Part 3*: console statistics collector over a socket; message count and
  length statistics; output after the N-th message and on timeout T when
  changed.
- C++17, OOP, CMake + GCC, separate build targets, the only external
  dependency is absl::log (agreed with the customer), no exceptions in
  business logic, error handling, a single git repository.

## License

Apache License 2.0. See [LICENSE](LICENSE) for details. Every source file
carries an SPDX header (`SPDX-License-Identifier: Apache-2.0`) for
automated license compliance scanning.
