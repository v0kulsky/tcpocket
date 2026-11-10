#include "tcpocket/ip_address.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

namespace tcpocket {

IpAddress::IpAddress() noexcept
    : addr_(::in_addr{}) {}

IpAddress::IpAddress(::in_addr addr) noexcept
    : addr_(addr) {}

IpAddress::IpAddress(::in6_addr addr) noexcept
    : addr_(addr) {}

IpAddress IpAddress::from_string(std::string_view ip_str, std::error_code& ec) noexcept {
    ec.clear();

    std::string str(ip_str);

    ::in_addr v4{};
    if (::inet_pton(AF_INET, str.c_str(), &v4) == 1) {
        return IpAddress{v4};
    }

    ::in6_addr v6{};
    if (::inet_pton(AF_INET6, str.c_str(), &v6) == 1) {
        return IpAddress{v6};
    }

    ec = make_error_code(SocketError::InvalidArgument);
    return IpAddress{};
}

IpAddress IpAddress::from_string(std::string_view ip_str) {
    std::error_code ec;
    auto ip = from_string(ip_str, ec);
    if (ec) {
        throw TcpException(ec, "Invalid IP address string");
    }
    return ip;
}

IpAddress IpAddress::any_ipv4() noexcept {
    return IpAddress{::in_addr{}};
}

IpAddress IpAddress::loopback_ipv4() noexcept {
    ::in_addr addr{};
    addr.s_addr = ::htonl(INADDR_LOOPBACK);
    return IpAddress{addr};
}

IpAddress IpAddress::any_ipv6() noexcept {
    return IpAddress{::in6_addr{}};
}

IpAddress IpAddress::loopback_ipv6() noexcept {
    return IpAddress{::in6addr_loopback};
}

std::string IpAddress::to_string() const {
    if (std::holds_alternative<::in_addr>(addr_)) {
        char buf[INET_ADDRSTRLEN];
        ::inet_ntop(AF_INET, &std::get<::in_addr>(addr_), buf, sizeof(buf));
        return buf;
    } else {
        char buf[INET6_ADDRSTRLEN];
        ::inet_ntop(AF_INET6, &std::get<::in6_addr>(addr_), buf, sizeof(buf));
        return buf;
    }
}

IpVersion IpAddress::version() const noexcept {
    return std::holds_alternative<::in_addr>(addr_) ? IpVersion::V4 : IpVersion::V6;
}

bool IpAddress::is_ipv4() const noexcept {
    return std::holds_alternative<::in_addr>(addr_);
}

bool IpAddress::is_ipv6() const noexcept {
    return std::holds_alternative<::in6_addr>(addr_);
}

bool IpAddress::is_loopback() const noexcept {
    if (std::holds_alternative<::in_addr>(addr_)) {
        const uint32_t s = ::ntohl(std::get<::in_addr>(addr_).s_addr);
        return (s >> 24) == 127;
    } else {
        return IN6_IS_ADDR_LOOPBACK(&std::get<::in6_addr>(addr_)) != 0;
    }
}

bool IpAddress::is_multicast() const noexcept {
    if (std::holds_alternative<::in_addr>(addr_)) {
        const uint32_t s = ::ntohl(std::get<::in_addr>(addr_).s_addr);
        return (s >> 28) == 0xE;
    } else {
        return IN6_IS_ADDR_MULTICAST(&std::get<::in6_addr>(addr_)) != 0;
    }
}

bool IpAddress::is_unspecified() const noexcept {
    if (std::holds_alternative<::in_addr>(addr_)) {
        return std::get<::in_addr>(addr_).s_addr == 0;
    } else {
        return IN6_IS_ADDR_UNSPECIFIED(&std::get<::in6_addr>(addr_)) != 0;
    }
}

std::span<const std::byte> IpAddress::bytes() const noexcept {
    if (std::holds_alternative<::in_addr>(addr_)) {
        const auto* p = reinterpret_cast<const std::byte*>(&std::get<::in_addr>(addr_));
        return {p, 4};
    } else {
        const auto* p = reinterpret_cast<const std::byte*>(&std::get<::in6_addr>(addr_));
        return {p, 16};
    }
}

bool IpAddress::operator==(const IpAddress& other) const noexcept {
    if (addr_.index() != other.addr_.index()) {
        return false;
    }
    if (std::holds_alternative<::in_addr>(addr_)) {
        return std::get<::in_addr>(addr_).s_addr == std::get<::in_addr>(other.addr_).s_addr;
    } else {
        return std::memcmp(&std::get<::in6_addr>(addr_), &std::get<::in6_addr>(other.addr_),
                           sizeof(::in6_addr)) == 0;
    }
}

std::strong_ordering IpAddress::operator<=>(const IpAddress& other) const noexcept {
    if (addr_.index() != other.addr_.index()) {
        return addr_.index() <=> other.addr_.index();
    }
    if (std::holds_alternative<::in_addr>(addr_)) {
        const uint32_t a = ::ntohl(std::get<::in_addr>(addr_).s_addr);
        const uint32_t b = ::ntohl(std::get<::in_addr>(other.addr_).s_addr);
        return a <=> b;
    } else {
        const int cmp = std::memcmp(&std::get<::in6_addr>(addr_), &std::get<::in6_addr>(other.addr_),
                                    sizeof(::in6_addr));
        if (cmp < 0) return std::strong_ordering::less;
        if (cmp > 0) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }
}

} // namespace tcpocket

std::size_t std::hash<tcpocket::IpAddress>::operator()(const tcpocket::IpAddress& ip) const noexcept {
    auto span = ip.bytes();
    std::size_t seed = span.size();
    for (auto b : span) {
        seed ^= static_cast<std::size_t>(b) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
}
