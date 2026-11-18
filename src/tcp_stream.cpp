#include "tcpocket/tcp_stream.hpp"
#include "tcpocket/dns_resolver.hpp"
#include "tcpocket/socket_options.hpp"

#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace tcpocket {

TcpStream::TcpStream(SocketHandle socket, Endpoint remote_ep, Endpoint local_ep) noexcept
    : socket_(std::move(socket))
    , remote_endpoint_(remote_ep)
    , local_endpoint_(local_ep) {}

TcpStream::TcpStream(TcpStream&& other) noexcept
    : socket_(std::move(other.socket_))
    , remote_endpoint_(std::exchange(other.remote_endpoint_, Endpoint{}))
    , local_endpoint_(std::exchange(other.local_endpoint_, Endpoint{})) {}

TcpStream& TcpStream::operator=(TcpStream&& other) noexcept {
    if (this != &other) {
        socket_ = std::move(other.socket_);
        remote_endpoint_ = std::exchange(other.remote_endpoint_, Endpoint{});
        local_endpoint_ = std::exchange(other.local_endpoint_, Endpoint{});
    }
    return *this;
}

TcpStream TcpStream::connect(const Endpoint& endpoint, std::chrono::milliseconds timeout) {
    std::error_code ec;
    auto stream = connect(endpoint, ec, timeout);
    if (ec) {
        throw TcpException(ec, "Failed to connect to endpoint");
    }
    return stream;
}

TcpStream TcpStream::connect(const Endpoint& endpoint, std::error_code& ec,
                             std::chrono::milliseconds timeout) noexcept {
    int domain = endpoint.address().is_ipv6() ? AF_INET6 : AF_INET;
    auto socket = SocketHandle::create_stream_socket(domain);
    if (!socket.is_valid()) {
        ec = last_socket_error();
        return TcpStream{};
    }

    sockaddr_storage storage{};
    socklen_t len = endpoint.to_sockaddr(storage);

    if (timeout > std::chrono::milliseconds::zero()) {
        tcpocket::set_nonblocking(socket, true, ec);
        if (ec) {
            return TcpStream{};
        }

        int res = 0;
        while ((res = ::connect(socket.native_handle(), reinterpret_cast<const sockaddr*>(&storage), len)) < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (res < 0) {
            if (errno != EINPROGRESS) {
                ec = last_socket_error();
                return TcpStream{};
            }

            struct pollfd pfd{};
            pfd.fd = socket.native_handle();
            pfd.events = POLLOUT;

            auto start_time = std::chrono::steady_clock::now();
            int poll_res = 0;
            while (true) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time);
                if (elapsed >= timeout) {
                    poll_res = 0;
                    break;
                }
                int remaining_ms = static_cast<int>((timeout - elapsed).count());
                poll_res = ::poll(&pfd, 1, remaining_ms);
                if (poll_res < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    ec = last_socket_error();
                    return TcpStream{};
                }
                break;
            }

            if (poll_res == 0) {
                ec = SocketError::TimedOut;
                return TcpStream{};
            }

            std::error_code opt_ec;
            std::error_code so_error = get_socket_error(socket, opt_ec);
            if (opt_ec) {
                ec = opt_ec;
                return TcpStream{};
            }
            if (so_error) {
                ec = so_error;
                return TcpStream{};
            }
        }

        tcpocket::set_nonblocking(socket, false, ec);
        if (ec) {
            return TcpStream{};
        }
    } else {
        int res = 0;
        while ((res = ::connect(socket.native_handle(), reinterpret_cast<const sockaddr*>(&storage), len)) < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (res < 0) {
            ec = last_socket_error();
            return TcpStream{};
        }
    }

    sockaddr_storage local_storage{};
    socklen_t local_len = sizeof(local_storage);
    if (::getsockname(socket.native_handle(), reinterpret_cast<sockaddr*>(&local_storage), &local_len) < 0) {
        ec = last_socket_error();
        return TcpStream{};
    }

    Endpoint local_ep = Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&local_storage), local_len, ec);
    if (ec) {
        return TcpStream{};
    }

    ec.clear();
    return TcpStream(std::move(socket), endpoint, local_ep);
}

TcpStream TcpStream::connect(std::string_view host, uint16_t port, std::chrono::milliseconds timeout) {
    std::error_code ec;
    auto stream = connect(host, port, ec, timeout);
    if (ec) {
        throw TcpException(ec, "Failed to connect to host");
    }
    return stream;
}

TcpStream TcpStream::connect(std::string_view host, uint16_t port, std::error_code& ec,
                             std::chrono::milliseconds timeout) noexcept {
    auto endpoints = DnsResolver::resolve(host, port, ec);
    if (ec) {
        return TcpStream{};
    }
    if (endpoints.empty()) {
        ec = SocketError::HostUnreachable;
        return TcpStream{};
    }

    std::error_code last_ec;
    for (const auto& ep : endpoints) {
        std::error_code conn_ec;
        auto stream = connect(ep, conn_ec, timeout);
        if (!conn_ec) {
            ec.clear();
            return stream;
        }
        last_ec = conn_ec;
    }

    ec = last_ec;
    return TcpStream{};
}

size_t TcpStream::read(std::span<std::byte> buffer, std::error_code& ec) noexcept {
    if (!is_open()) {
        ec = SocketError::InvalidDescriptor;
        return 0;
    }
    if (buffer.empty()) {
        ec.clear();
        return 0;
    }

    ssize_t n = 0;
    while ((n = ::recv(socket_.native_handle(), buffer.data(), buffer.size(), 0)) < 0) {
        if (errno == EINTR) {
            continue;
        }
        break;
    }

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            int flags = ::fcntl(socket_.native_handle(), F_GETFL, 0);
            if (flags >= 0 && (flags & O_NONBLOCK) == 0) {
                ec = SocketError::TimedOut;
                return 0;
            }
        }
        ec = last_socket_error();
        return 0;
    }

    ec.clear();
    return static_cast<size_t>(n);
}

size_t TcpStream::read(std::span<std::byte> buffer) {
    std::error_code ec;
    size_t n = read(buffer, ec);
    if (ec) {
        throw TcpException(ec, "Read failed");
    }
    return n;
}

size_t TcpStream::read(std::span<char> buffer, std::error_code& ec) noexcept {
    return read(std::span<std::byte>(reinterpret_cast<std::byte*>(buffer.data()), buffer.size()), ec);
}

size_t TcpStream::read(std::span<char> buffer) {
    std::error_code ec;
    size_t n = read(buffer, ec);
    if (ec) {
        throw TcpException(ec, "Read failed");
    }
    return n;
}

void TcpStream::read_exact(std::span<std::byte> buffer, std::error_code& ec) noexcept {
    if (!is_open()) {
        ec = SocketError::InvalidDescriptor;
        return;
    }
    size_t total = 0;
    while (total < buffer.size()) {
        size_t n = read(buffer.subspan(total), ec);
        if (ec) {
            return;
        }
        if (n == 0) {
            ec = SocketError::ConnectionReset;
            return;
        }
        total += n;
    }
    ec.clear();
}

void TcpStream::read_exact(std::span<std::byte> buffer) {
    std::error_code ec;
    read_exact(buffer, ec);
    if (ec) {
        throw TcpException(ec, "Read exact failed");
    }
}

void TcpStream::read_exact(std::span<char> buffer, std::error_code& ec) noexcept {
    read_exact(std::span<std::byte>(reinterpret_cast<std::byte*>(buffer.data()), buffer.size()), ec);
}

void TcpStream::read_exact(std::span<char> buffer) {
    std::error_code ec;
    read_exact(buffer, ec);
    if (ec) {
        throw TcpException(ec, "Read exact failed");
    }
}

size_t TcpStream::write(std::span<const std::byte> data, std::error_code& ec) noexcept {
    if (!is_open()) {
        ec = SocketError::InvalidDescriptor;
        return 0;
    }
    if (data.empty()) {
        ec.clear();
        return 0;
    }

    ssize_t n = 0;
    while ((n = ::send(socket_.native_handle(), data.data(), data.size(), MSG_NOSIGNAL)) < 0) {
        if (errno == EINTR) {
            continue;
        }
        break;
    }

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            int flags = ::fcntl(socket_.native_handle(), F_GETFL, 0);
            if (flags >= 0 && (flags & O_NONBLOCK) == 0) {
                ec = SocketError::TimedOut;
                return 0;
            }
        }
        ec = last_socket_error();
        return 0;
    }

    ec.clear();
    return static_cast<size_t>(n);
}

size_t TcpStream::write(std::span<const std::byte> data) {
    std::error_code ec;
    size_t n = write(data, ec);
    if (ec) {
        throw TcpException(ec, "Write failed");
    }
    return n;
}

size_t TcpStream::write(std::span<const char> data, std::error_code& ec) noexcept {
    return write(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()), ec);
}

size_t TcpStream::write(std::span<const char> data) {
    std::error_code ec;
    size_t n = write(data, ec);
    if (ec) {
        throw TcpException(ec, "Write failed");
    }
    return n;
}

size_t TcpStream::write(std::string_view data, std::error_code& ec) noexcept {
    return write(std::span<const char>(data.data(), data.size()), ec);
}

size_t TcpStream::write(std::string_view data) {
    std::error_code ec;
    size_t n = write(data, ec);
    if (ec) {
        throw TcpException(ec, "Write failed");
    }
    return n;
}

void TcpStream::write_all(std::span<const std::byte> data, std::error_code& ec) noexcept {
    if (!is_open()) {
        ec = SocketError::InvalidDescriptor;
        return;
    }
    size_t total = 0;
    while (total < data.size()) {
        size_t n = write(data.subspan(total), ec);
        if (ec) {
            return;
        }
        total += n;
    }
    ec.clear();
}

void TcpStream::write_all(std::span<const std::byte> data) {
    std::error_code ec;
    write_all(data, ec);
    if (ec) {
        throw TcpException(ec, "Write all failed");
    }
}

void TcpStream::write_all(std::span<const char> data, std::error_code& ec) noexcept {
    write_all(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()), ec);
}

void TcpStream::write_all(std::span<const char> data) {
    std::error_code ec;
    write_all(data, ec);
    if (ec) {
        throw TcpException(ec, "Write all failed");
    }
}

void TcpStream::write_all(std::string_view data, std::error_code& ec) noexcept {
    write_all(std::span<const char>(data.data(), data.size()), ec);
}

void TcpStream::write_all(std::string_view data) {
    std::error_code ec;
    write_all(data, ec);
    if (ec) {
        throw TcpException(ec, "Write all failed");
    }
}

void TcpStream::shutdown(ShutdownMode mode, std::error_code& ec) noexcept {
    if (!is_open()) {
        ec = SocketError::InvalidDescriptor;
        return;
    }
    int how = 0;
    switch (mode) {
    case ShutdownMode::Read:
        how = SHUT_RD;
        break;
    case ShutdownMode::Write:
        how = SHUT_WR;
        break;
    case ShutdownMode::Both:
        how = SHUT_RDWR;
        break;
    }
    if (::shutdown(socket_.native_handle(), how) < 0) {
        ec = last_socket_error();
    } else {
        ec.clear();
    }
}

void TcpStream::shutdown(ShutdownMode mode) {
    std::error_code ec;
    shutdown(mode, ec);
    if (ec) {
        throw TcpException(ec, "Shutdown failed");
    }
}

void TcpStream::close() noexcept {
    socket_.close();
    remote_endpoint_ = Endpoint{};
    local_endpoint_ = Endpoint{};
}

void TcpStream::set_timeouts(std::chrono::milliseconds read_timeout,
                             std::chrono::milliseconds write_timeout,
                             std::error_code& ec) noexcept {
    set_read_timeout(read_timeout, ec);
    if (ec) {
        return;
    }
    set_write_timeout(write_timeout, ec);
}

void TcpStream::set_timeouts(std::chrono::milliseconds read_timeout,
                             std::chrono::milliseconds write_timeout) {
    std::error_code ec;
    set_timeouts(read_timeout, write_timeout, ec);
    if (ec) {
        throw TcpException(ec, "Failed to set timeouts");
    }
}

void TcpStream::set_read_timeout(std::chrono::milliseconds timeout, std::error_code& ec) noexcept {
    set_receive_timeout(socket_, timeout, ec);
}

void TcpStream::set_read_timeout(std::chrono::milliseconds timeout) {
    std::error_code ec;
    set_read_timeout(timeout, ec);
    if (ec) {
        throw TcpException(ec, "Failed to set read timeout");
    }
}

void TcpStream::set_write_timeout(std::chrono::milliseconds timeout, std::error_code& ec) noexcept {
    set_send_timeout(socket_, timeout, ec);
}

void TcpStream::set_write_timeout(std::chrono::milliseconds timeout) {
    std::error_code ec;
    set_write_timeout(timeout, ec);
    if (ec) {
        throw TcpException(ec, "Failed to set write timeout");
    }
}

void TcpStream::set_nodelay(bool enable, std::error_code& ec) noexcept {
    set_tcp_nodelay(socket_, enable, ec);
}

void TcpStream::set_nodelay(bool enable) {
    std::error_code ec;
    set_nodelay(enable, ec);
    if (ec) {
        throw TcpException(ec, "Failed to set TCP_NODELAY");
    }
}

void TcpStream::set_nonblocking(bool enable, std::error_code& ec) noexcept {
    tcpocket::set_nonblocking(socket_, enable, ec);
}

void TcpStream::set_nonblocking(bool enable) {
    std::error_code ec;
    set_nonblocking(enable, ec);
    if (ec) {
        throw TcpException(ec, "Failed to set nonblocking mode");
    }
}

bool TcpStream::is_open() const noexcept {
    return socket_.is_valid();
}

TcpStream::operator bool() const noexcept {
    return is_open();
}

int TcpStream::native_handle() const noexcept {
    return socket_.native_handle();
}

const SocketHandle& TcpStream::socket_handle() const noexcept {
    return socket_;
}

const Endpoint& TcpStream::local_endpoint() const noexcept {
    return local_endpoint_;
}

const Endpoint& TcpStream::remote_endpoint() const noexcept {
    return remote_endpoint_;
}

} // namespace tcpocket
