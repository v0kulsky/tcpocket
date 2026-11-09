#pragma once

#include "tcpocket/socket_handle.hpp"
#include "tcpocket/error.hpp"

#include <chrono>
#include <system_error>

namespace tcpocket {

void set_reuse_address(const SocketHandle& socket, bool enable);
void set_reuse_address(const SocketHandle& socket, bool enable, std::error_code& ec) noexcept;

void set_reuse_port(const SocketHandle& socket, bool enable);
void set_reuse_port(const SocketHandle& socket, bool enable, std::error_code& ec) noexcept;

void set_tcp_nodelay(const SocketHandle& socket, bool enable);
void set_tcp_nodelay(const SocketHandle& socket, bool enable, std::error_code& ec) noexcept;

void set_keep_alive(const SocketHandle& socket, bool enable);
void set_keep_alive(const SocketHandle& socket, bool enable, std::error_code& ec) noexcept;

void set_receive_timeout(const SocketHandle& socket, std::chrono::milliseconds timeout);
void set_receive_timeout(const SocketHandle& socket, std::chrono::milliseconds timeout,
                         std::error_code& ec) noexcept;

void set_send_timeout(const SocketHandle& socket, std::chrono::milliseconds timeout);
void set_send_timeout(const SocketHandle& socket, std::chrono::milliseconds timeout,
                      std::error_code& ec) noexcept;

void set_nonblocking(const SocketHandle& socket, bool enable);
void set_nonblocking(const SocketHandle& socket, bool enable, std::error_code& ec) noexcept;

[[nodiscard]] std::error_code get_socket_error(const SocketHandle& socket);
[[nodiscard]] std::error_code get_socket_error(const SocketHandle& socket,
                                                std::error_code& ec) noexcept;

} // namespace tcpocket
