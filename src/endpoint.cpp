#include "tcpocket/endpoint.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <charconv>
#include <cstring>

namespace tcpocket {

Endpoint::Endpoint() noexcept
    : address_(IpAddress::any_ipv4()), port_(0) {}

Endpoint::Endpoint(IpAddress address, uint16_t port) noexcept
    : address_(address), port_(port) {}

Endpoint Endpoint::from_string(std::string_view endpoint_str, std::error_code& ec) noexcept {
    ec.clear();

    if (endpoint_str.empty()) {
        ec = make_error_code(SocketError::InvalidArgument);
        return Endpoint{};
    }

    std::string_view ip_str;
    std::string_view port_str;

    if (endpoint_str.front() == '[') {
        auto close_bracket = endpoint_str.find(']');
        if (close_bracket == std::string_view::npos) {
            ec = make_error_code(SocketError::InvalidArgument);
            return Endpoint{};
        }

        if (close_bracket + 1 >= endpoint_str.size() || endpoint_str[close_bracket + 1] != ':') {
            ec = make_error_code(SocketError::InvalidArgument);
            return Endpoint{};
        }

        ip_str = endpoint_str.substr(1, close_bracket - 1);
        port_str = endpoint_str.substr(close_bracket + 2);

        if (ip_str.empty() || port_str.empty()) {
            ec = make_error_code(SocketError::InvalidArgument);
            return Endpoint{};
        }

        auto addr = IpAddress::from_string(ip_str, ec);
        if (ec) {
            return Endpoint{};
        }

        if (!addr.is_ipv6()) {
            ec = make_error_code(SocketError::InvalidArgument);
            return Endpoint{};
        }

        unsigned long port_val = 0;
        auto [ptr, parse_ec] = std::from_chars(port_str.data(), port_str.data() + port_str.size(), port_val);
        if (parse_ec != std::errc{} || ptr != port_str.data() + port_str.size() || port_val > 65535) {
            ec = make_error_code(SocketError::InvalidArgument);
            return Endpoint{};
        }

        return Endpoint{addr, static_cast<uint16_t>(port_val)};
    }

    if (endpoint_str.find('[') != std::string_view::npos || endpoint_str.find(']') != std::string_view::npos) {
        ec = make_error_code(SocketError::InvalidArgument);
        return Endpoint{};
    }

    auto first_colon = endpoint_str.find(':');
    if (first_colon == std::string_view::npos) {
        ec = make_error_code(SocketError::InvalidArgument);
        return Endpoint{};
    }

    auto last_colon = endpoint_str.rfind(':');
    if (first_colon != last_colon) {
        ec = make_error_code(SocketError::InvalidArgument);
        return Endpoint{};
    }

    ip_str = endpoint_str.substr(0, first_colon);
    port_str = endpoint_str.substr(first_colon + 1);

    if (ip_str.empty() || port_str.empty()) {
        ec = make_error_code(SocketError::InvalidArgument);
        return Endpoint{};
    }

    auto addr = IpAddress::from_string(ip_str, ec);
    if (ec) {
        return Endpoint{};
    }

    if (!addr.is_ipv4()) {
        ec = make_error_code(SocketError::InvalidArgument);
        return Endpoint{};
    }

    unsigned long port_val = 0;
    auto [ptr, parse_ec] = std::from_chars(port_str.data(), port_str.data() + port_str.size(), port_val);
    if (parse_ec != std::errc{} || ptr != port_str.data() + port_str.size() || port_val > 65535) {
        ec = make_error_code(SocketError::InvalidArgument);
        return Endpoint{};
    }

    return Endpoint{addr, static_cast<uint16_t>(port_val)};
}

Endpoint Endpoint::from_string(std::string_view endpoint_str) {
    std::error_code ec;
    auto ep = from_string(endpoint_str, ec);
    if (ec) {
        throw TcpException(ec, "Invalid endpoint string");
    }
    return ep;
}

Endpoint Endpoint::any_ipv4(uint16_t port) noexcept {
    return Endpoint{IpAddress::any_ipv4(), port};
}

Endpoint Endpoint::any_ipv6(uint16_t port) noexcept {
    return Endpoint{IpAddress::any_ipv6(), port};
}

Endpoint Endpoint::loopback_ipv4(uint16_t port) noexcept {
    return Endpoint{IpAddress::loopback_ipv4(), port};
}

Endpoint Endpoint::loopback_ipv6(uint16_t port) noexcept {
    return Endpoint{IpAddress::loopback_ipv6(), port};
}

Endpoint Endpoint::from_sockaddr(const sockaddr* sa, socklen_t len, std::error_code& ec) noexcept {
    ec.clear();

    if (sa == nullptr || len < static_cast<socklen_t>(sizeof(sa_family_t))) {
        ec = make_error_code(SocketError::InvalidArgument);
        return Endpoint{};
    }

    if (sa->sa_family == AF_INET) {
        if (len < static_cast<socklen_t>(sizeof(sockaddr_in))) {
            ec = make_error_code(SocketError::InvalidArgument);
            return Endpoint{};
        }
        const auto* sin = reinterpret_cast<const sockaddr_in*>(sa);
        uint16_t port = ::ntohs(sin->sin_port);
        IpAddress addr(sin->sin_addr);
        return Endpoint(addr, port);
    }

    if (sa->sa_family == AF_INET6) {
        if (len < static_cast<socklen_t>(sizeof(sockaddr_in6))) {
            ec = make_error_code(SocketError::InvalidArgument);
            return Endpoint{};
        }
        const auto* sin6 = reinterpret_cast<const sockaddr_in6*>(sa);
        uint16_t port = ::ntohs(sin6->sin6_port);
        IpAddress addr(sin6->sin6_addr);
        return Endpoint(addr, port);
    }

    ec = make_error_code(SocketError::InvalidArgument);
    return Endpoint{};
}

Endpoint Endpoint::from_sockaddr(const sockaddr* sa, socklen_t len) {
    std::error_code ec;
    auto ep = from_sockaddr(sa, len, ec);
    if (ec) {
        throw TcpException(ec, "Invalid sockaddr");
    }
    return ep;
}

socklen_t Endpoint::to_sockaddr(sockaddr_storage& storage) const noexcept {
    std::memset(&storage, 0, sizeof(storage));

    if (address_.is_ipv4()) {
        auto* sin = reinterpret_cast<sockaddr_in*>(&storage);
        sin->sin_family = AF_INET;
        sin->sin_port = ::htons(port_);
        std::memcpy(&sin->sin_addr, address_.bytes().data(), sizeof(sin->sin_addr));
        return sizeof(sockaddr_in);
    } else {
        auto* sin6 = reinterpret_cast<sockaddr_in6*>(&storage);
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = ::htons(port_);
        std::memcpy(&sin6->sin6_addr, address_.bytes().data(), sizeof(sin6->sin6_addr));
        return sizeof(sockaddr_in6);
    }
}

const IpAddress& Endpoint::address() const noexcept {
    return address_;
}

uint16_t Endpoint::port() const noexcept {
    return port_;
}

void Endpoint::set_address(IpAddress address) noexcept {
    address_ = address;
}

void Endpoint::set_port(uint16_t port) noexcept {
    port_ = port;
}

std::string Endpoint::to_string() const {
    if (address_.is_ipv6()) {
        return "[" + address_.to_string() + "]:" + std::to_string(port_);
    }
    return address_.to_string() + ":" + std::to_string(port_);
}

} // namespace tcpocket

std::size_t std::hash<tcpocket::Endpoint>::operator()(const tcpocket::Endpoint& ep) const noexcept {
    std::size_t h1 = std::hash<tcpocket::IpAddress>{}(ep.address());
    std::size_t h2 = std::hash<uint16_t>{}(ep.port());
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
}
