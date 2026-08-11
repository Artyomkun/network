// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#include "logger/socket.hpp"

#include <cstring>
#include <string>

#include "logger/platform.hpp"

#if defined(LOGGER_PLATFORM_WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace logger {

namespace {

bool isInvalid(socket_handle sock) {
    return sock == kInvalidSocket;
}

int closeRaw(socket_handle sock) {
#if defined(LOGGER_PLATFORM_WINDOWS)
    return closesocket(sock);
#else
    return close(sock);
#endif
}

// Disables the Nagle algorithm: journal entries are small and infrequent,
// and accumulating them in the kernel buffer would add latency to delivery.
void enableNoDelay(socket_handle sock) {
    int enabled = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char*>(&enabled), sizeof(enabled));
}

// Suppresses SIGPIPE on platforms without MSG_NOSIGNAL (macOS/BSD).
void disableSigpipe(socket_handle sock) {
#if defined(LOGGER_HAS_SO_NOSIGPIPE)
    int enabled = 1;
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE,
               reinterpret_cast<const char*>(&enabled), sizeof(enabled));
#else
    (void)sock;
#endif
}

}  // namespace

bool socketInit() {
#if defined(LOGGER_PLATFORM_WINDOWS)
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

void socketCleanup() {
#if defined(LOGGER_PLATFORM_WINDOWS)
    WSACleanup();
#endif
}

socket_handle tcpConnect(const std::string& host, int port, bool& ok) {
    ok = false;
    socket_handle sock = socket(AF_INET, SOCK_STREAM, 0);
    if (isInvalid(sock)) {
        return kInvalidSocket;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(port));

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        // Not an IP address - resolve the host name. getaddrinfo (unlike
        // gethostbyname) is thread-safe and does not rely on global static
        // buffers; IPv6 is out of scope (the family is limited to AF_INET,
        // as in the rest of the socket code).
        struct addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* resolved = nullptr;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &resolved) != 0 ||
            resolved == nullptr) {
            closeRaw(sock);
            return kInvalidSocket;
        }
        // Take the first address from the list; the list is always released
        // via freeaddrinfo as soon as the address is copied.
        std::memcpy(&addr.sin_addr,
                    &reinterpret_cast<const sockaddr_in*>(resolved->ai_addr)->sin_addr,
                    sizeof(addr.sin_addr));
        freeaddrinfo(resolved);
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closeRaw(sock);
        return kInvalidSocket;
    }
    enableNoDelay(sock);
    disableSigpipe(sock);
    ok = true;
    return sock;
}

socket_handle tcpListen(const std::string& host, int port, bool& ok) {
    ok = false;
    socket_handle sock = socket(AF_INET, SOCK_STREAM, 0);
    if (isInvalid(sock)) {
        return kInvalidSocket;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(port));

    if (host.empty() || host == "*" || host == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        closeRaw(sock);
        return kInvalidSocket;
    }

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(sock, 4) != 0) {
        closeRaw(sock);
        return kInvalidSocket;
    }
    ok = true;
    return sock;
}

socket_handle tcpAccept(socket_handle listener, bool& ok) {
    ok = false;
    sockaddr_in peer;
#ifdef LOGGER_PLATFORM_WINDOWS
    int peer_len = sizeof(peer);
#else
    socklen_t peer_len = sizeof(peer);
#endif
    socket_handle sock = accept(listener, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (isInvalid(sock)) {
        return kInvalidSocket;
    }
    ok = true;
    return sock;
}

bool tcpSend(socket_handle sock, const char* data, int size) {
    const char* pos = data;
    int left = size;
    while (left > 0) {
#if defined(LOGGER_HAS_MSG_NOSIGNAL)
        // Linux: MSG_NOSIGNAL - writing to a broken socket does not kill the process.
        const int sent = static_cast<int>(send(sock, pos, static_cast<size_t>(left), MSG_NOSIGNAL));
#elif defined(LOGGER_HAS_SO_NOSIGPIPE)
        const int sent = static_cast<int>(send(sock, pos, static_cast<size_t>(left), 0));
#else
        const int sent = static_cast<int>(send(sock, pos, static_cast<size_t>(left), 0));
#endif
        if (sent <= 0) {
            return false;
        }
        pos += sent;
        left -= sent;
    }
    return true;
}

int tcpReceive(socket_handle sock, char* buffer, int size) {
    return static_cast<int>(recv(sock, buffer, static_cast<size_t>(size), 0));
}

bool tcpWait(socket_handle sock, int timeout_ms) {
    if (timeout_ms < 0) {
        timeout_ms = 0;
    }
    // FD_SET indexes the internal fd_set array: a descriptor outside the
    // FD_SETSIZE range is a buffer overrun. Such descriptors should not
    // exist, but checking is cheaper than finding out.
    if (sock >= static_cast<socket_handle>(FD_SETSIZE)) {
        return false;
    }
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
#if defined(LOGGER_PLATFORM_WINDOWS)
    // In Winsock the first argument of select is ignored; on POSIX it is nfds.
    const int result = select(0, &read_fds, nullptr, nullptr, &tv);
#else
    const int result = select(sock + 1, &read_fds, nullptr, nullptr, &tv);
#endif
    return result > 0;
}

void tcpClose(socket_handle sock) {
    if (!isInvalid(sock)) {
        closeRaw(sock);
    }
}

}  // namespace logger