#include <gtest/gtest.h>
#include "tcpocket/dns_resolver.hpp"
#include "tcpocket/endpoint.hpp"
#include "tcpocket/ip_address.hpp"
#include "tcpocket/error.hpp"

#include <optional>
#include <string>
#include <vector>

TEST(DnsResolverTest, ResolveLocalhostWithPort) {
    auto endpoints = tcpocket::DnsResolver::resolve("localhost", 8080);
    ASSERT_FALSE(endpoints.empty());

    bool has_loopback = false;
    for (const auto& ep : endpoints) {
        EXPECT_EQ(ep.port(), 8080);
        if (ep.address().is_loopback()) {
            has_loopback = true;
        }
    }
    EXPECT_TRUE(has_loopback);

    std::error_code ec;
    auto endpoints_noexcept = tcpocket::DnsResolver::resolve("localhost", 8080, ec);
    EXPECT_FALSE(ec);
    EXPECT_FALSE(endpoints_noexcept.empty());
}

TEST(DnsResolverTest, ResolveNumericIpv4) {
    auto endpoints = tcpocket::DnsResolver::resolve("127.0.0.1", 9000);
    ASSERT_EQ(endpoints.size(), 1u);
    EXPECT_EQ(endpoints[0], tcpocket::Endpoint(tcpocket::IpAddress::loopback_ipv4(), 9000));

    std::error_code ec;
    auto endpoints_noexcept = tcpocket::DnsResolver::resolve("127.0.0.1", 9000, ec);
    EXPECT_FALSE(ec);
    ASSERT_EQ(endpoints_noexcept.size(), 1u);
    EXPECT_EQ(endpoints_noexcept[0], endpoints[0]);
}

TEST(DnsResolverTest, ResolveNumericIpv6) {
    auto endpoints = tcpocket::DnsResolver::resolve("::1", 9000);
    ASSERT_EQ(endpoints.size(), 1u);
    EXPECT_EQ(endpoints[0], tcpocket::Endpoint(tcpocket::IpAddress::loopback_ipv6(), 9000));

    std::error_code ec;
    auto endpoints_noexcept = tcpocket::DnsResolver::resolve("::1", 9000, ec);
    EXPECT_FALSE(ec);
    ASSERT_EQ(endpoints_noexcept.size(), 1u);
    EXPECT_EQ(endpoints_noexcept[0], endpoints[0]);
}

TEST(DnsResolverTest, ResolveServiceHttp) {
    auto endpoints = tcpocket::DnsResolver::resolve("localhost", "http");
    ASSERT_FALSE(endpoints.empty());

    for (const auto& ep : endpoints) {
        EXPECT_EQ(ep.port(), 80);
        EXPECT_TRUE(ep.address().is_loopback());
    }

    std::error_code ec;
    auto endpoints_noexcept = tcpocket::DnsResolver::resolve("localhost", "http", ec);
    EXPECT_FALSE(ec);
    EXPECT_FALSE(endpoints_noexcept.empty());
}

TEST(DnsResolverTest, ResolveServiceHttps) {
    auto endpoints = tcpocket::DnsResolver::resolve("localhost", "https");
    ASSERT_FALSE(endpoints.empty());

    for (const auto& ep : endpoints) {
        EXPECT_EQ(ep.port(), 443);
        EXPECT_TRUE(ep.address().is_loopback());
    }

    std::error_code ec;
    auto endpoints_noexcept = tcpocket::DnsResolver::resolve("localhost", "https", ec);
    EXPECT_FALSE(ec);
    EXPECT_FALSE(endpoints_noexcept.empty());
}

TEST(DnsResolverTest, FilterIpv4Only) {
    auto endpoints = tcpocket::DnsResolver::resolve("localhost", 8080, tcpocket::IpVersion::V4);
    ASSERT_FALSE(endpoints.empty());

    for (const auto& ep : endpoints) {
        EXPECT_TRUE(ep.address().is_ipv4());
        EXPECT_FALSE(ep.address().is_ipv6());
        EXPECT_EQ(ep.port(), 8080);
    }

    std::error_code ec;
    auto endpoints_noexcept = tcpocket::DnsResolver::resolve("localhost", 8080, tcpocket::IpVersion::V4, ec);
    EXPECT_FALSE(ec);
    EXPECT_FALSE(endpoints_noexcept.empty());
}

TEST(DnsResolverTest, FilterIpv6Only) {
    std::error_code ec;
    auto endpoints = tcpocket::DnsResolver::resolve("localhost", 8080, tcpocket::IpVersion::V6, ec);
    if (!ec) {
        for (const auto& ep : endpoints) {
            EXPECT_TRUE(ep.address().is_ipv6());
            EXPECT_FALSE(ep.address().is_ipv4());
            EXPECT_EQ(ep.port(), 8080);
        }
    }
}

TEST(DnsResolverTest, NonExistentDomainThrows) {
    EXPECT_THROW({
        try {
            static_cast<void>(tcpocket::DnsResolver::resolve("this-domain-does-not-exist.invalid", 80));
        } catch (const tcpocket::TcpException& ex) {
            EXPECT_EQ(ex.code(), tcpocket::SocketError::HostUnreachable);
            throw;
        }
    }, tcpocket::TcpException);
}

TEST(DnsResolverTest, NonExistentDomainErrorCode) {
    std::error_code ec;
    auto endpoints = tcpocket::DnsResolver::resolve("this-domain-does-not-exist.invalid", 80, ec);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::HostUnreachable);
    EXPECT_TRUE(endpoints.empty());
}

TEST(DnsResolverTest, InvalidServiceThrows) {
    EXPECT_THROW({
        try {
            static_cast<void>(tcpocket::DnsResolver::resolve("localhost", "invalid-service-name-xyz-99999"));
        } catch (const tcpocket::TcpException& ex) {
            EXPECT_TRUE(ex.code() == tcpocket::SocketError::InvalidArgument ||
                        ex.code() == tcpocket::SocketError::OperationNotSupported ||
                        ex.code() == tcpocket::SocketError::HostUnreachable);
            throw;
        }
    }, tcpocket::TcpException);
}

TEST(DnsResolverTest, InvalidServiceErrorCode) {
    std::error_code ec;
    auto endpoints = tcpocket::DnsResolver::resolve("localhost", "invalid-service-name-xyz-99999", ec);
    EXPECT_TRUE(ec);
    EXPECT_TRUE(endpoints.empty());
}

TEST(DnsResolverTest, EmptyHostAndService) {
    std::error_code ec;
    auto endpoints = tcpocket::DnsResolver::resolve("", "", ec);
    EXPECT_TRUE(ec);
    EXPECT_TRUE(endpoints.empty());

    EXPECT_THROW({
        static_cast<void>(tcpocket::DnsResolver::resolve("", ""));
    }, tcpocket::TcpException);
}

TEST(DnsResolverTest, ResolveMultipleEndpointsForHostAndService) {
    auto endpoints = tcpocket::DnsResolver::resolve("localhost", "http");
    ASSERT_GT(endpoints.size(), 1u);

    bool has_ipv4 = false;
    bool has_ipv6 = false;
    for (const auto& ep : endpoints) {
        EXPECT_EQ(ep.port(), 80);
        EXPECT_TRUE(ep.address().is_loopback());
        if (ep.address().is_ipv4()) {
            has_ipv4 = true;
        }
        if (ep.address().is_ipv6()) {
            has_ipv6 = true;
        }
    }

    EXPECT_TRUE(has_ipv4);
    EXPECT_TRUE(has_ipv6);
    EXPECT_NE(endpoints[0], endpoints[1]);
}
