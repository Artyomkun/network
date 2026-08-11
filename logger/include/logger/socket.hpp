// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

#include "logger/export.hpp"

namespace logger {

#ifdef _WIN32
typedef unsigned long long socket_handle;
#else
typedef int socket_handle;
#endif

// The distinguished "invalid descriptor" value (INVALID_SOCKET in Winsock,
// -1 on POSIX). Returned by tcpConnect/tcpListen/tcpAccept on failure.
inline const socket_handle kInvalidSocket =
#ifdef _WIN32
    static_cast<socket_handle>(~0)
#else
    socket_handle(-1)
#endif
    ;

// Minimal socket wrapper without an RAII class: the returned descriptor is a
// "raw" resource and the owner must close it with tcpClose. Breaking this
// contract leaks the socket. RAII ownership is provided by SocketGuard.
// TODO: fold SocketGuard into an RAII SocketHandle class.

LOGGER_API bool socketInit();
LOGGER_API void socketCleanup();

LOGGER_API socket_handle tcpConnect(const std::string& host, int port, bool& ok);
LOGGER_API socket_handle tcpListen(const std::string& host, int port, bool& ok);
LOGGER_API socket_handle tcpAccept(socket_handle listener, bool& ok);

LOGGER_API bool tcpSend(socket_handle sock, const char* data, int size);
LOGGER_API int tcpReceive(socket_handle sock, char* buffer, int size);

LOGGER_API bool tcpWait(socket_handle sock, int timeout_ms);

LOGGER_API void tcpClose(socket_handle sock);

// RAII owner of a socket. Closes the descriptor in the destructor, so the
// socket cannot leak even if an exception or an early return exits the
// scope. Move-only, like unique_ptr.
class SocketGuard {
public:
    explicit SocketGuard(socket_handle handle) noexcept
        : handle_(handle) {}

    SocketGuard(SocketGuard&& other) noexcept
        : handle_(other.release()) {}

    SocketGuard& operator=(SocketGuard&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.release();
        }
        return *this;
    }

    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;

    ~SocketGuard() {
        close();
    }

    // Raw descriptor for tcpSend/tcpReceive/tcpWait/tcpAccept.
    socket_handle get() const noexcept {
        return handle_;
    }

    // Takes ownership away: the destructor will not close the descriptor.
    socket_handle release() noexcept {
        const socket_handle handle = handle_;
        handle_ = kInvalidSocket;
        return handle;
    }

    // Closes immediately (e.g. mid-scope, to report a close explicitly).
    void close() noexcept {
        if (handle_ != kInvalidSocket) {
            tcpClose(handle_);
            handle_ = kInvalidSocket;
        }
    }

private:
    socket_handle handle_ = kInvalidSocket;
};

}  // namespace logger