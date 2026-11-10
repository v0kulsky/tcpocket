#pragma once

#include "tcpocket/error.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <compare>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace tcpocket {

enum class IpVersion {
    V4,
    V6
};

class IpAddress {
public:
    IpAddress() noexcept;
    explicit IpAddress(::in_addr addr) noexcept;
    explicit IpAddress(::in6_addr addr) noexcept;

    [[nodiscard]] static IpAddress from_string(std::string_view ip_str);
    [[nodiscard]] static IpAddress from_string(std::string_view ip_str, std::error_code& ec) noexcept;

    [[nodiscard]] static IpAddress any_ipv4() noexcept;
    [[nodiscard]] static IpAddress loopback_ipv4() noexcept;
    [[nodiscard]] static IpAddress any_ipv6() noexcept;
    [[nodiscard]] static IpAddress loopback_ipv6() noexcept;

    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] IpVersion version() const noexcept;
    [[nodiscard]] bool is_ipv4() const noexcept;
    [[nodiscard]] bool is_ipv6() const noexcept;
    [[nodiscard]] bool is_loopback() const noexcept;
    [[nodiscard]] bool is_multicast() const noexcept;
    [[nodiscard]] bool is_unspecified() const noexcept;

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept;

    [[nodiscard]] bool operator==(const IpAddress& other) const noexcept;
    [[nodiscard]] std::strong_ordering operator<=>(const IpAddress& other) const noexcept;

private:
    std::variant<::in_addr, ::in6_addr> addr_;
};

} // namespace tcpocket

template <>
struct std::hash<tcpocket::IpAddress> {
    [[nodiscard]] std::size_t operator()(const tcpocket::IpAddress& ip) const noexcept;
};
