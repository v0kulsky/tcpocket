#pragma once

#include "tcpocket/endpoint.hpp"
#include "tcpocket/error.hpp"
#include "tcpocket/ip_address.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

namespace tcpocket {

class DnsResolver {
public:
    [[nodiscard]] static std::vector<Endpoint> resolve(
        std::string_view host,
        uint16_t port,
        std::optional<IpVersion> version = std::nullopt);

    [[nodiscard]] static std::vector<Endpoint> resolve(
        std::string_view host,
        uint16_t port,
        std::error_code& ec) noexcept;

    [[nodiscard]] static std::vector<Endpoint> resolve(
        std::string_view host,
        uint16_t port,
        std::optional<IpVersion> version,
        std::error_code& ec) noexcept;

    [[nodiscard]] static std::vector<Endpoint> resolve(
        std::string_view host,
        std::string_view service,
        std::optional<IpVersion> version = std::nullopt);

    [[nodiscard]] static std::vector<Endpoint> resolve(
        std::string_view host,
        std::string_view service,
        std::error_code& ec) noexcept;

    [[nodiscard]] static std::vector<Endpoint> resolve(
        std::string_view host,
        std::string_view service,
        std::optional<IpVersion> version,
        std::error_code& ec) noexcept;
};

} // namespace tcpocket
