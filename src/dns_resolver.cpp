#include "tcpocket/dns_resolver.hpp"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>
#include <memory>
#include <string>

namespace tcpocket {

namespace {

std::error_code map_gai_error(int gai_err) noexcept {
    switch (gai_err) {
        case 0:
            return make_error_code(SocketError::Success);
        case EAI_NONAME:
#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
        case EAI_NODATA:
#endif
            return make_error_code(SocketError::HostUnreachable);
        case EAI_AGAIN:
            return make_error_code(SocketError::TimedOut);
        case EAI_FAIL:
            return make_error_code(SocketError::NetworkUnreachable);
        case EAI_SERVICE:
            return make_error_code(SocketError::InvalidArgument);
        case EAI_FAMILY:
            return make_error_code(SocketError::OperationNotSupported);
        case EAI_BADFLAGS:
            return make_error_code(SocketError::InvalidArgument);
        case EAI_SYSTEM:
            return last_socket_error();
        case EAI_MEMORY:
        default:
            return make_error_code(SocketError::Unknown);
    }
}

struct AddrInfoDeleter {
    void operator()(::addrinfo* p) const noexcept {
        if (p != nullptr) {
            ::freeaddrinfo(p);
        }
    }
};

using UniqueAddrInfo = std::unique_ptr<::addrinfo, AddrInfoDeleter>;

} // namespace

std::vector<Endpoint> DnsResolver::resolve(
    std::string_view host,
    uint16_t port,
    std::optional<IpVersion> version) {
    std::error_code ec;
    auto result = resolve(host, port, version, ec);
    if (ec) {
        throw TcpException(ec, "Failed to resolve host");
    }
    return result;
}

std::vector<Endpoint> DnsResolver::resolve(
    std::string_view host,
    uint16_t port,
    std::error_code& ec) noexcept {
    return resolve(host, port, std::nullopt, ec);
}

std::vector<Endpoint> DnsResolver::resolve(
    std::string_view host,
    uint16_t port,
    std::optional<IpVersion> version,
    std::error_code& ec) noexcept {
    ec.clear();

    if (host.empty()) {
        ec = make_error_code(SocketError::InvalidArgument);
        return {};
    }

    std::error_code ip_ec;
    auto ip = IpAddress::from_string(host, ip_ec);
    if (!ip_ec) {
        if (!version.has_value() ||
            (*version == IpVersion::V4 && ip.is_ipv4()) ||
            (*version == IpVersion::V6 && ip.is_ipv6())) {
            return {Endpoint(ip, port)};
        }
        ec = make_error_code(SocketError::HostUnreachable);
        return {};
    }

    std::string host_str(host);
    std::string port_str = std::to_string(port);

    struct addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (version.has_value()) {
        hints.ai_family = (*version == IpVersion::V4) ? AF_INET : AF_INET6;
    } else {
        hints.ai_family = AF_UNSPEC;
    }

    ::addrinfo* res_raw = nullptr;
    int rc = ::getaddrinfo(host_str.c_str(), port_str.c_str(), &hints, &res_raw);
    UniqueAddrInfo res(res_raw);

    if (rc != 0) {
        ec = map_gai_error(rc);
        return {};
    }

    std::vector<Endpoint> endpoints;
    for (auto* cur = res.get(); cur != nullptr; cur = cur->ai_next) {
        if (cur->ai_addr == nullptr) {
            continue;
        }
        std::error_code ep_ec;
        auto ep = Endpoint::from_sockaddr(cur->ai_addr, cur->ai_addrlen, ep_ec);
        if (!ep_ec) {
            if (std::find(endpoints.begin(), endpoints.end(), ep) == endpoints.end()) {
                endpoints.push_back(ep);
            }
        }
    }

    if (endpoints.empty()) {
        ec = make_error_code(SocketError::HostUnreachable);
        return {};
    }

    return endpoints;
}

std::vector<Endpoint> DnsResolver::resolve(
    std::string_view host,
    std::string_view service,
    std::optional<IpVersion> version) {
    std::error_code ec;
    auto result = resolve(host, service, version, ec);
    if (ec) {
        throw TcpException(ec, "Failed to resolve host and service");
    }
    return result;
}

std::vector<Endpoint> DnsResolver::resolve(
    std::string_view host,
    std::string_view service,
    std::error_code& ec) noexcept {
    return resolve(host, service, std::nullopt, ec);
}

std::vector<Endpoint> DnsResolver::resolve(
    std::string_view host,
    std::string_view service,
    std::optional<IpVersion> version,
    std::error_code& ec) noexcept {
    ec.clear();

    if (host.empty() && service.empty()) {
        ec = make_error_code(SocketError::InvalidArgument);
        return {};
    }

    std::string host_str(host);
    std::string service_str(service);

    struct addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (version.has_value()) {
        hints.ai_family = (*version == IpVersion::V4) ? AF_INET : AF_INET6;
    } else {
        hints.ai_family = AF_UNSPEC;
    }

    const char* node_ptr = host.empty() ? nullptr : host_str.c_str();
    const char* serv_ptr = service.empty() ? nullptr : service_str.c_str();

    ::addrinfo* res_raw = nullptr;
    int rc = ::getaddrinfo(node_ptr, serv_ptr, &hints, &res_raw);
    UniqueAddrInfo res(res_raw);

    if (rc != 0) {
        ec = map_gai_error(rc);
        return {};
    }

    std::vector<Endpoint> endpoints;
    for (auto* cur = res.get(); cur != nullptr; cur = cur->ai_next) {
        if (cur->ai_addr == nullptr) {
            continue;
        }
        std::error_code ep_ec;
        auto ep = Endpoint::from_sockaddr(cur->ai_addr, cur->ai_addrlen, ep_ec);
        if (!ep_ec) {
            if (std::find(endpoints.begin(), endpoints.end(), ep) == endpoints.end()) {
                endpoints.push_back(ep);
            }
        }
    }

    if (endpoints.empty()) {
        ec = make_error_code(SocketError::HostUnreachable);
        return {};
    }

    return endpoints;
}

} // namespace tcpocket
