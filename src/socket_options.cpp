#include "tcpocket/socket_options.hpp"

#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>

namespace tcpocket {

namespace {

void set_bool_sockopt(const SocketHandle& socket, int level, int optname,
                      bool enable, std::error_code& ec) noexcept {
    if (!socket.is_valid()) {
        ec = SocketError::InvalidDescriptor;
        return;
    }
    int optval = enable ? 1 : 0;
    if (::setsockopt(socket.native_handle(), level, optname, &optval, sizeof(optval)) < 0) {
        ec = last_socket_error();
    } else {
        ec.clear();
    }
}

void set_timeval_sockopt(const SocketHandle& socket, int optname,
                         std::chrono::milliseconds timeout, std::error_code& ec) noexcept {
    if (!socket.is_valid()) {
        ec = SocketError::InvalidDescriptor;
        return;
    }
    if (timeout.count() < 0) {
        ec = SocketError::InvalidArgument;
        return;
    }
    struct timeval tv{};
    tv.tv_sec  = static_cast<time_t>(timeout.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
    if (::setsockopt(socket.native_handle(), SOL_SOCKET, optname, &tv, sizeof(tv)) < 0) {
        ec = last_socket_error();
    } else {
        ec.clear();
    }
}

void throw_if_error(std::error_code ec, const char* msg) {
    if (ec) {
        throw TcpException(msg, ec);
    }
}

} // namespace

void set_reuse_address(const SocketHandle& socket, bool enable, std::error_code& ec) noexcept {
    set_bool_sockopt(socket, SOL_SOCKET, SO_REUSEADDR, enable, ec);
}

void set_reuse_address(const SocketHandle& socket, bool enable) {
    std::error_code ec;
    set_reuse_address(socket, enable, ec);
    throw_if_error(ec, "Failed to set SO_REUSEADDR");
}

void set_reuse_port(const SocketHandle& socket, bool enable, std::error_code& ec) noexcept {
    set_bool_sockopt(socket, SOL_SOCKET, SO_REUSEPORT, enable, ec);
}

void set_reuse_port(const SocketHandle& socket, bool enable) {
    std::error_code ec;
    set_reuse_port(socket, enable, ec);
    throw_if_error(ec, "Failed to set SO_REUSEPORT");
}

void set_tcp_nodelay(const SocketHandle& socket, bool enable, std::error_code& ec) noexcept {
    set_bool_sockopt(socket, IPPROTO_TCP, TCP_NODELAY, enable, ec);
}

void set_tcp_nodelay(const SocketHandle& socket, bool enable) {
    std::error_code ec;
    set_tcp_nodelay(socket, enable, ec);
    throw_if_error(ec, "Failed to set TCP_NODELAY");
}

void set_keep_alive(const SocketHandle& socket, bool enable, std::error_code& ec) noexcept {
    set_bool_sockopt(socket, SOL_SOCKET, SO_KEEPALIVE, enable, ec);
}

void set_keep_alive(const SocketHandle& socket, bool enable) {
    std::error_code ec;
    set_keep_alive(socket, enable, ec);
    throw_if_error(ec, "Failed to set SO_KEEPALIVE");
}

void set_receive_timeout(const SocketHandle& socket, std::chrono::milliseconds timeout,
                         std::error_code& ec) noexcept {
    set_timeval_sockopt(socket, SO_RCVTIMEO, timeout, ec);
}

void set_receive_timeout(const SocketHandle& socket, std::chrono::milliseconds timeout) {
    std::error_code ec;
    set_receive_timeout(socket, timeout, ec);
    throw_if_error(ec, "Failed to set SO_RCVTIMEO");
}

void set_send_timeout(const SocketHandle& socket, std::chrono::milliseconds timeout,
                      std::error_code& ec) noexcept {
    set_timeval_sockopt(socket, SO_SNDTIMEO, timeout, ec);
}

void set_send_timeout(const SocketHandle& socket, std::chrono::milliseconds timeout) {
    std::error_code ec;
    set_send_timeout(socket, timeout, ec);
    throw_if_error(ec, "Failed to set SO_SNDTIMEO");
}

void set_nonblocking(const SocketHandle& socket, bool enable, std::error_code& ec) noexcept {
    if (!socket.is_valid()) {
        ec = SocketError::InvalidDescriptor;
        return;
    }
    int flags = ::fcntl(socket.native_handle(), F_GETFL);
    if (flags < 0) {
        ec = last_socket_error();
        return;
    }
    if (enable) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    if (::fcntl(socket.native_handle(), F_SETFL, flags) < 0) {
        ec = last_socket_error();
    } else {
        ec.clear();
    }
}

void set_nonblocking(const SocketHandle& socket, bool enable) {
    std::error_code ec;
    set_nonblocking(socket, enable, ec);
    throw_if_error(ec, "Failed to set O_NONBLOCK");
}

std::error_code get_socket_error(const SocketHandle& socket, std::error_code& ec) noexcept {
    if (!socket.is_valid()) {
        ec = SocketError::InvalidDescriptor;
        return {};
    }
    ec.clear();
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    if (::getsockopt(socket.native_handle(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        ec = last_socket_error();
        return {};
    }
    return error_code_from_errno(optval);
}

std::error_code get_socket_error(const SocketHandle& socket) {
    std::error_code ec;
    std::error_code sock_err = get_socket_error(socket, ec);
    throw_if_error(ec, "Failed to get SO_ERROR");
    return sock_err;
}

} // namespace tcpocket
