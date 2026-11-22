#pragma once

#include "tcpocket/endpoint.hpp"
#include "tcpocket/error.hpp"
#include "tcpocket/socket_handle.hpp"
#include "tcpocket/tcp_stream.hpp"

#include <chrono>
#include <string_view>
#include <system_error>
#include <utility>

namespace tcpocket {

class TcpListener {
public:
    TcpListener() noexcept = default;

    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    TcpListener(TcpListener&& other) noexcept;
    TcpListener& operator=(TcpListener&& other) noexcept;

    ~TcpListener() = default;

    static TcpListener bind(const Endpoint& endpoint, int backlog = 128);
    static TcpListener bind(const Endpoint& endpoint, std::error_code& ec, int backlog = 128) noexcept;

    static TcpListener bind(uint16_t port, bool ipv6_dual_stack = false, int backlog = 128);
    static TcpListener bind(uint16_t port, std::error_code& ec, bool ipv6_dual_stack = false, int backlog = 128) noexcept;

    static TcpListener bind(std::string_view host, uint16_t port, int backlog = 128);
    static TcpListener bind(std::string_view host, uint16_t port, std::error_code& ec, int backlog = 128) noexcept;

    [[nodiscard]] std::pair<TcpStream, Endpoint> accept();
    [[nodiscard]] std::pair<TcpStream, Endpoint> accept(std::error_code& ec) noexcept;

    [[nodiscard]] std::pair<TcpStream, Endpoint> accept(std::chrono::milliseconds timeout);
    [[nodiscard]] std::pair<TcpStream, Endpoint> accept(std::chrono::milliseconds timeout, std::error_code& ec) noexcept;

    [[nodiscard]] bool is_listening() const noexcept;
    explicit operator bool() const noexcept;
    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] const SocketHandle& socket_handle() const noexcept;
    [[nodiscard]] const Endpoint& local_endpoint() const noexcept;

    void close() noexcept;

    void set_nonblocking(bool enable);
    void set_nonblocking(bool enable, std::error_code& ec) noexcept;

private:
    explicit TcpListener(SocketHandle socket, Endpoint local_ep) noexcept;

    SocketHandle socket_;
    Endpoint local_endpoint_;
};

} // namespace tcpocket
