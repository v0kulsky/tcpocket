#include "tcpocket/tcp_listener.hpp"
#include "tcpocket/dns_resolver.hpp"
#include "tcpocket/socket_options.hpp"

#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace tcpocket {

TcpListener::TcpListener(SocketHandle socket, Endpoint local_ep) noexcept
    : socket_(std::move(socket))
    , local_endpoint_(local_ep) {}

TcpListener::TcpListener(TcpListener&& other) noexcept
    : socket_(std::move(other.socket_))
    , local_endpoint_(std::exchange(other.local_endpoint_, Endpoint{})) {}

TcpListener& TcpListener::operator=(TcpListener&& other) noexcept {
    if (this != &other) {
        socket_ = std::move(other.socket_);
        local_endpoint_ = std::exchange(other.local_endpoint_, Endpoint{});
    }
    return *this;
}

TcpListener TcpListener::bind(const Endpoint& endpoint, int backlog) {
    std::error_code ec;
    auto listener = bind(endpoint, ec, backlog);
    if (ec) {
        throw TcpException(ec, "Failed to bind listener to endpoint");
    }
    return listener;
}

TcpListener TcpListener::bind(const Endpoint& endpoint, std::error_code& ec, int backlog) noexcept {
    int domain = endpoint.address().is_ipv6() ? AF_INET6 : AF_INET;
    auto socket = SocketHandle::create_stream_socket(domain, ec);
    if (ec) {
        return TcpListener{};
    }

    tcpocket::set_reuse_address(socket, true, ec);
    if (ec) {
        return TcpListener{};
    }

    sockaddr_storage storage{};
    socklen_t len = endpoint.to_sockaddr(storage);

    if (::bind(socket.native_handle(), reinterpret_cast<const sockaddr*>(&storage), len) < 0) {
        ec = last_socket_error();
        return TcpListener{};
    }

    if (::listen(socket.native_handle(), backlog) < 0) {
        ec = last_socket_error();
        return TcpListener{};
    }

    sockaddr_storage local_addr{};
    socklen_t local_len = sizeof(local_addr);
    if (::getsockname(socket.native_handle(), reinterpret_cast<sockaddr*>(&local_addr), &local_len) < 0) {
        ec = last_socket_error();
        return TcpListener{};
    }

    auto local_ep = Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&local_addr), local_len, ec);
    if (ec) {
        return TcpListener{};
    }

    ec.clear();
    return TcpListener(std::move(socket), local_ep);
}

TcpListener TcpListener::bind(uint16_t port, bool ipv6_dual_stack, int backlog) {
    std::error_code ec;
    auto listener = bind(port, ec, ipv6_dual_stack, backlog);
    if (ec) {
        throw TcpException(ec, "Failed to bind listener to port");
    }
    return listener;
}

TcpListener TcpListener::bind(uint16_t port, std::error_code& ec, bool ipv6_dual_stack, int backlog) noexcept {
    if (!ipv6_dual_stack) {
        return bind(Endpoint::any_ipv4(port), ec, backlog);
    }

    auto socket = SocketHandle::create_stream_socket(AF_INET6, ec);
    if (ec) {
        return TcpListener{};
    }

    tcpocket::set_reuse_address(socket, true, ec);
    if (ec) {
        return TcpListener{};
    }

    int v6only = 0;
    if (::setsockopt(socket.native_handle(), IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0) {
        ec = last_socket_error();
        return TcpListener{};
    }

    auto endpoint = Endpoint::any_ipv6(port);
    sockaddr_storage storage{};
    socklen_t len = endpoint.to_sockaddr(storage);

    if (::bind(socket.native_handle(), reinterpret_cast<const sockaddr*>(&storage), len) < 0) {
        ec = last_socket_error();
        return TcpListener{};
    }

    if (::listen(socket.native_handle(), backlog) < 0) {
        ec = last_socket_error();
        return TcpListener{};
    }

    sockaddr_storage local_addr{};
    socklen_t local_len = sizeof(local_addr);
    if (::getsockname(socket.native_handle(), reinterpret_cast<sockaddr*>(&local_addr), &local_len) < 0) {
        ec = last_socket_error();
        return TcpListener{};
    }

    auto local_ep = Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&local_addr), local_len, ec);
    if (ec) {
        return TcpListener{};
    }

    ec.clear();
    return TcpListener(std::move(socket), local_ep);
}

TcpListener TcpListener::bind(std::string_view host, uint16_t port, int backlog) {
    std::error_code ec;
    auto listener = bind(host, port, ec, backlog);
    if (ec) {
        throw TcpException(ec, "Failed to bind listener to host and port");
    }
    return listener;
}

TcpListener TcpListener::bind(std::string_view host, uint16_t port, std::error_code& ec, int backlog) noexcept {
    auto endpoints = DnsResolver::resolve(host, port, ec);
    if (ec) {
        return TcpListener{};
    }
    if (endpoints.empty()) {
        ec = SocketError::HostUnreachable;
        return TcpListener{};
    }

    std::error_code last_ec;
    for (const auto& ep : endpoints) {
        std::error_code bind_ec;
        auto listener = bind(ep, bind_ec, backlog);
        if (!bind_ec) {
            ec.clear();
            return listener;
        }
        last_ec = bind_ec;
    }

    ec = last_ec;
    return TcpListener{};
}

std::pair<TcpStream, Endpoint> TcpListener::accept() {
    std::error_code ec;
    auto result = accept(ec);
    if (ec) {
        throw TcpException(ec, "Accept failed");
    }
    return result;
}

std::pair<TcpStream, Endpoint> TcpListener::accept(std::error_code& ec) noexcept {
    if (!is_listening()) {
        ec = SocketError::InvalidDescriptor;
        return {};
    }

    sockaddr_storage client_storage{};
    socklen_t client_len = sizeof(client_storage);
    int client_fd = -1;

    while (true) {
#if defined(SOCK_CLOEXEC)
        client_fd = ::accept4(socket_.native_handle(), reinterpret_cast<sockaddr*>(&client_storage), &client_len, SOCK_CLOEXEC);
#else
        client_fd = ::accept(socket_.native_handle(), reinterpret_cast<sockaddr*>(&client_storage), &client_len);
        if (client_fd >= 0) {
            int flags = ::fcntl(client_fd, F_GETFD);
            if (flags != -1) {
                ::fcntl(client_fd, F_SETFD, flags | FD_CLOEXEC);
            }
        }
#endif
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            ec = last_socket_error();
            return {};
        }
        break;
    }

    SocketHandle client_handle(client_fd);
    auto client_ep = Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&client_storage), client_len, ec);
    if (ec) {
        return {};
    }

    sockaddr_storage local_storage{};
    socklen_t local_len = sizeof(local_storage);
    if (::getsockname(client_handle.native_handle(), reinterpret_cast<sockaddr*>(&local_storage), &local_len) < 0) {
        ec = last_socket_error();
        return {};
    }

    auto local_ep = Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&local_storage), local_len, ec);
    if (ec) {
        return {};
    }

    ec.clear();
    return {TcpStream(std::move(client_handle), client_ep, local_ep), client_ep};
}

std::pair<TcpStream, Endpoint> TcpListener::accept(std::chrono::milliseconds timeout) {
    std::error_code ec;
    auto result = accept(timeout, ec);
    if (ec) {
        throw TcpException(ec, "Timed accept failed");
    }
    return result;
}

std::pair<TcpStream, Endpoint> TcpListener::accept(std::chrono::milliseconds timeout, std::error_code& ec) noexcept {
    if (!is_listening()) {
        ec = SocketError::InvalidDescriptor;
        return {};
    }

    struct pollfd pfd{};
    pfd.fd = socket_.native_handle();
    pfd.events = POLLIN;

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
            return {};
        }
        break;
    }

    if (poll_res == 0) {
        ec = SocketError::TimedOut;
        return {};
    }

    return accept(ec);
}

bool TcpListener::is_listening() const noexcept {
    return socket_.is_valid();
}

TcpListener::operator bool() const noexcept {
    return is_listening();
}

int TcpListener::native_handle() const noexcept {
    return socket_.native_handle();
}

const SocketHandle& TcpListener::socket_handle() const noexcept {
    return socket_;
}

const Endpoint& TcpListener::local_endpoint() const noexcept {
    return local_endpoint_;
}

void TcpListener::close() noexcept {
    socket_.close();
    local_endpoint_ = Endpoint{};
}

void TcpListener::set_nonblocking(bool enable) {
    tcpocket::set_nonblocking(socket_, enable);
}

void TcpListener::set_nonblocking(bool enable, std::error_code& ec) noexcept {
    tcpocket::set_nonblocking(socket_, enable, ec);
}

} // namespace tcpocket
