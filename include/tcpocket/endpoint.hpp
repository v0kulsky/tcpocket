#pragma once

#include "tcpocket/error.hpp"
#include "tcpocket/ip_address.hpp"

#include <sys/socket.h>

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace tcpocket {

class Endpoint {
public:
    Endpoint() noexcept;
    Endpoint(IpAddress address, uint16_t port) noexcept;

    [[nodiscard]] static Endpoint from_string(std::string_view endpoint_str);
    [[nodiscard]] static Endpoint from_string(std::string_view endpoint_str, std::error_code& ec) noexcept;

    [[nodiscard]] static Endpoint any_ipv4(uint16_t port) noexcept;
    [[nodiscard]] static Endpoint any_ipv6(uint16_t port) noexcept;
    [[nodiscard]] static Endpoint loopback_ipv4(uint16_t port) noexcept;
    [[nodiscard]] static Endpoint loopback_ipv6(uint16_t port) noexcept;

    [[nodiscard]] static Endpoint from_sockaddr(const sockaddr* sa, socklen_t len);
    [[nodiscard]] static Endpoint from_sockaddr(const sockaddr* sa, socklen_t len, std::error_code& ec) noexcept;
    socklen_t to_sockaddr(sockaddr_storage& storage) const noexcept;

    [[nodiscard]] const IpAddress& address() const noexcept;
    [[nodiscard]] uint16_t port() const noexcept;
    void set_address(IpAddress address) noexcept;
    void set_port(uint16_t port) noexcept;

    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] bool operator==(const Endpoint& other) const noexcept = default;
    [[nodiscard]] std::strong_ordering operator<=>(const Endpoint& other) const noexcept = default;

private:
    IpAddress address_{};
    uint16_t port_{0};
};

} // namespace tcpocket

template <>
struct std::hash<tcpocket::Endpoint> {
    [[nodiscard]] std::size_t operator()(const tcpocket::Endpoint& ep) const noexcept;
};
