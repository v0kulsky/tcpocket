#pragma once

#include "tcpocket/endpoint.hpp"
#include "tcpocket/error.hpp"
#include "tcpocket/socket_handle.hpp"

#include <chrono>
#include <cstddef>
#include <span>
#include <string_view>
#include <system_error>

namespace tcpocket {

enum class ShutdownMode {
    Read,
    Write,
    Both
};

class TcpStream {
public:
    TcpStream() noexcept = default;
    explicit TcpStream(SocketHandle socket, Endpoint remote_ep, Endpoint local_ep) noexcept;

    TcpStream(const TcpStream&) = delete;
    TcpStream& operator=(const TcpStream&) = delete;

    TcpStream(TcpStream&& other) noexcept;
    TcpStream& operator=(TcpStream&& other) noexcept;

    ~TcpStream() = default;

    static TcpStream connect(const Endpoint& endpoint,
                             std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());
    static TcpStream connect(const Endpoint& endpoint, std::error_code& ec,
                             std::chrono::milliseconds timeout = std::chrono::milliseconds::zero()) noexcept;

    static TcpStream connect(std::string_view host, uint16_t port,
                             std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());
    static TcpStream connect(std::string_view host, uint16_t port, std::error_code& ec,
                             std::chrono::milliseconds timeout = std::chrono::milliseconds::zero()) noexcept;

    [[nodiscard]] size_t read(std::span<std::byte> buffer);
    [[nodiscard]] size_t read(std::span<std::byte> buffer, std::error_code& ec) noexcept;
    [[nodiscard]] size_t read(std::span<char> buffer);
    [[nodiscard]] size_t read(std::span<char> buffer, std::error_code& ec) noexcept;

    void read_exact(std::span<std::byte> buffer);
    void read_exact(std::span<std::byte> buffer, std::error_code& ec) noexcept;
    void read_exact(std::span<char> buffer);
    void read_exact(std::span<char> buffer, std::error_code& ec) noexcept;

    [[nodiscard]] size_t write(std::span<const std::byte> data);
    [[nodiscard]] size_t write(std::span<const std::byte> data, std::error_code& ec) noexcept;
    [[nodiscard]] size_t write(std::span<const char> data);
    [[nodiscard]] size_t write(std::span<const char> data, std::error_code& ec) noexcept;
    [[nodiscard]] size_t write(std::string_view data);
    [[nodiscard]] size_t write(std::string_view data, std::error_code& ec) noexcept;

    void write_all(std::span<const std::byte> data);
    void write_all(std::span<const std::byte> data, std::error_code& ec) noexcept;
    void write_all(std::span<const char> data);
    void write_all(std::span<const char> data, std::error_code& ec) noexcept;
    void write_all(std::string_view data);
    void write_all(std::string_view data, std::error_code& ec) noexcept;

    void shutdown(ShutdownMode mode);
    void shutdown(ShutdownMode mode, std::error_code& ec) noexcept;
    void close() noexcept;

    void set_timeouts(std::chrono::milliseconds read_timeout, std::chrono::milliseconds write_timeout);
    void set_timeouts(std::chrono::milliseconds read_timeout, std::chrono::milliseconds write_timeout,
                      std::error_code& ec) noexcept;

    void set_read_timeout(std::chrono::milliseconds timeout);
    void set_read_timeout(std::chrono::milliseconds timeout, std::error_code& ec) noexcept;

    void set_write_timeout(std::chrono::milliseconds timeout);
    void set_write_timeout(std::chrono::milliseconds timeout, std::error_code& ec) noexcept;

    void set_nodelay(bool enable);
    void set_nodelay(bool enable, std::error_code& ec) noexcept;

    void set_nonblocking(bool enable);
    void set_nonblocking(bool enable, std::error_code& ec) noexcept;

    [[nodiscard]] bool is_open() const noexcept;
    explicit operator bool() const noexcept;
    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] const SocketHandle& socket_handle() const noexcept;
    [[nodiscard]] const Endpoint& local_endpoint() const noexcept;
    [[nodiscard]] const Endpoint& remote_endpoint() const noexcept;

private:
    SocketHandle socket_;
    Endpoint remote_endpoint_;
    Endpoint local_endpoint_;
};

} // namespace tcpocket
