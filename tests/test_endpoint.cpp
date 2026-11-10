#include <gtest/gtest.h>
#include "tcpocket/endpoint.hpp"
#include "tcpocket/ip_address.hpp"
#include "tcpocket/error.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

TEST(EndpointTest, TypeTraits) {
    static_assert(std::is_default_constructible_v<tcpocket::Endpoint>);
    static_assert(std::is_copy_constructible_v<tcpocket::Endpoint>);
    static_assert(std::is_copy_assignable_v<tcpocket::Endpoint>);
    static_assert(std::is_move_constructible_v<tcpocket::Endpoint>);
    static_assert(std::is_move_assignable_v<tcpocket::Endpoint>);
    static_assert(std::is_nothrow_move_constructible_v<tcpocket::Endpoint>);
    static_assert(std::is_nothrow_move_assignable_v<tcpocket::Endpoint>);
}

TEST(EndpointTest, DefaultConstructor) {
    tcpocket::Endpoint ep;
    EXPECT_EQ(ep.address(), tcpocket::IpAddress::any_ipv4());
    EXPECT_EQ(ep.port(), 0);
    EXPECT_EQ(ep.to_string(), "0.0.0.0:0");
}

TEST(EndpointTest, ParameterizedConstructor) {
    auto addr_v4 = tcpocket::IpAddress::from_string("192.168.1.1");
    tcpocket::Endpoint ep_v4(addr_v4, 8080);
    EXPECT_EQ(ep_v4.address(), addr_v4);
    EXPECT_EQ(ep_v4.port(), 8080);
    EXPECT_EQ(ep_v4.to_string(), "192.168.1.1:8080");

    auto addr_v6 = tcpocket::IpAddress::from_string("2001:db8::1");
    tcpocket::Endpoint ep_v6(addr_v6, 443);
    EXPECT_EQ(ep_v6.address(), addr_v6);
    EXPECT_EQ(ep_v6.port(), 443);
    EXPECT_EQ(ep_v6.to_string(), "[2001:db8::1]:443");
}

TEST(EndpointTest, AccessorsAndMutators) {
    tcpocket::Endpoint ep;

    auto addr1 = tcpocket::IpAddress::from_string("10.0.0.1");
    ep.set_address(addr1);
    ep.set_port(9000);
    EXPECT_EQ(ep.address(), addr1);
    EXPECT_EQ(ep.port(), 9000);
    EXPECT_EQ(ep.to_string(), "10.0.0.1:9000");

    auto addr2 = tcpocket::IpAddress::from_string("::1");
    ep.set_address(addr2);
    ep.set_port(80);
    EXPECT_EQ(ep.address(), addr2);
    EXPECT_EQ(ep.port(), 80);
    EXPECT_EQ(ep.to_string(), "[::1]:80");
}

TEST(EndpointTest, StaticFactories) {
    auto any_v4 = tcpocket::Endpoint::any_ipv4(80);
    EXPECT_EQ(any_v4.address(), tcpocket::IpAddress::any_ipv4());
    EXPECT_EQ(any_v4.port(), 80);
    EXPECT_EQ(any_v4.to_string(), "0.0.0.0:80");

    auto lb_v4 = tcpocket::Endpoint::loopback_ipv4(8080);
    EXPECT_EQ(lb_v4.address(), tcpocket::IpAddress::loopback_ipv4());
    EXPECT_EQ(lb_v4.port(), 8080);
    EXPECT_EQ(lb_v4.to_string(), "127.0.0.1:8080");

    auto any_v6 = tcpocket::Endpoint::any_ipv6(443);
    EXPECT_EQ(any_v6.address(), tcpocket::IpAddress::any_ipv6());
    EXPECT_EQ(any_v6.port(), 443);
    EXPECT_EQ(any_v6.to_string(), "[::]:443");

    auto lb_v6 = tcpocket::Endpoint::loopback_ipv6(8443);
    EXPECT_EQ(lb_v6.address(), tcpocket::IpAddress::loopback_ipv6());
    EXPECT_EQ(lb_v6.port(), 8443);
    EXPECT_EQ(lb_v6.to_string(), "[::1]:8443");
}

TEST(EndpointTest, ParseValidEndpoints) {
    struct TestCase {
        std::string input;
        std::string expected_ip;
        uint16_t expected_port;
    };

    const std::vector<TestCase> test_cases = {
        {"127.0.0.1:8080", "127.0.0.1", 8080},
        {"0.0.0.0:0", "0.0.0.0", 0},
        {"192.168.1.1:65535", "192.168.1.1", 65535},
        {"[::1]:443", "::1", 443},
        {"[2001:db8::1]:80", "2001:db8::1", 80},
        {"[::]:0", "::", 0},
        {"[fe80::1]:8080", "fe80::1", 8080}
    };

    for (const auto& tc : test_cases) {
        auto ep = tcpocket::Endpoint::from_string(tc.input);
        EXPECT_EQ(ep.address(), tcpocket::IpAddress::from_string(tc.expected_ip));
        EXPECT_EQ(ep.port(), tc.expected_port);
        EXPECT_EQ(ep.to_string(), tc.input);

        std::error_code ec;
        auto ep_noexcept = tcpocket::Endpoint::from_string(tc.input, ec);
        EXPECT_FALSE(ec);
        EXPECT_EQ(ep_noexcept, ep);
    }
}

TEST(EndpointTest, ParseInvalidEndpoints) {
    const std::vector<std::string_view> invalid_inputs = {
        "127.0.0.1",
        "127.0.0.1:",
        "127.0.0.1:99999",
        "127.0.0.1:65536",
        "127.0.0.1:-1",
        "127.0.0.1:abc",
        "[::1]",
        "[::1]:",
        "::1:8080",
        "[::1:8080",
        "::1]:8080",
        "[:8080",
        ":8080",
        "foo:bar",
        "256.0.0.1:80",
        ""
    };

    for (const auto& invalid : invalid_inputs) {
        EXPECT_THROW({
            try {
                static_cast<void>(tcpocket::Endpoint::from_string(invalid));
            } catch (const tcpocket::TcpException& ex) {
                EXPECT_EQ(ex.code(), tcpocket::SocketError::InvalidArgument);
                throw;
            }
        }, tcpocket::TcpException);

        std::error_code ec;
        static_cast<void>(tcpocket::Endpoint::from_string(invalid, ec));
        EXPECT_TRUE(ec);
        EXPECT_EQ(ec, tcpocket::SocketError::InvalidArgument);
    }
}

TEST(EndpointTest, ToSockaddrIpv4) {
    auto ep = tcpocket::Endpoint::from_string("192.168.1.50:8080");
    sockaddr_storage storage{};
    socklen_t len = ep.to_sockaddr(storage);

    EXPECT_EQ(len, sizeof(sockaddr_in));
    EXPECT_EQ(storage.ss_family, AF_INET);

    const auto* sin = reinterpret_cast<const sockaddr_in*>(&storage);
    EXPECT_EQ(sin->sin_family, AF_INET);
    EXPECT_EQ(sin->sin_port, htons(8080));

    char ip_buf[INET_ADDRSTRLEN];
    ASSERT_NE(::inet_ntop(AF_INET, &sin->sin_addr, ip_buf, sizeof(ip_buf)), nullptr);
    EXPECT_STREQ(ip_buf, "192.168.1.50");
}

TEST(EndpointTest, ToSockaddrIpv6) {
    auto ep = tcpocket::Endpoint::from_string("[2001:db8::1]:9000");
    sockaddr_storage storage{};
    socklen_t len = ep.to_sockaddr(storage);

    EXPECT_EQ(len, sizeof(sockaddr_in6));
    EXPECT_EQ(storage.ss_family, AF_INET6);

    const auto* sin6 = reinterpret_cast<const sockaddr_in6*>(&storage);
    EXPECT_EQ(sin6->sin6_family, AF_INET6);
    EXPECT_EQ(sin6->sin6_port, htons(9000));

    char ip_buf[INET6_ADDRSTRLEN];
    ASSERT_NE(::inet_ntop(AF_INET6, &sin6->sin6_addr, ip_buf, sizeof(ip_buf)), nullptr);
    EXPECT_STREQ(ip_buf, "2001:db8::1");
}

TEST(EndpointTest, FromSockaddrIpv4) {
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(8080);
    ASSERT_EQ(::inet_pton(AF_INET, "10.0.0.1", &sin.sin_addr), 1);

    auto ep = tcpocket::Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
    EXPECT_EQ(ep.address(), tcpocket::IpAddress::from_string("10.0.0.1"));
    EXPECT_EQ(ep.port(), 8080);

    std::error_code ec;
    auto ep_noexcept = tcpocket::Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&sin), sizeof(sin), ec);
    EXPECT_FALSE(ec);
    EXPECT_EQ(ep_noexcept, ep);
}

TEST(EndpointTest, FromSockaddrIpv6) {
    sockaddr_in6 sin6{};
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(8443);
    ASSERT_EQ(::inet_pton(AF_INET6, "::1", &sin6.sin6_addr), 1);

    auto ep = tcpocket::Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&sin6), sizeof(sin6));
    EXPECT_EQ(ep.address(), tcpocket::IpAddress::from_string("::1"));
    EXPECT_EQ(ep.port(), 8443);

    std::error_code ec;
    auto ep_noexcept = tcpocket::Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&sin6), sizeof(sin6), ec);
    EXPECT_FALSE(ec);
    EXPECT_EQ(ep_noexcept, ep);
}

TEST(EndpointTest, FromSockaddrInvalid) {
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(80);

    // Insufficient length for IPv4
    EXPECT_THROW({
        try {
            static_cast<void>(tcpocket::Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&sin), sizeof(sin) - 1));
        } catch (const tcpocket::TcpException& ex) {
            EXPECT_EQ(ex.code(), tcpocket::SocketError::InvalidArgument);
            throw;
        }
    }, tcpocket::TcpException);

    std::error_code ec;
    static_cast<void>(tcpocket::Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&sin), sizeof(sin) - 1, ec));
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidArgument);

    // Null pointer
    ec.clear();
    static_cast<void>(tcpocket::Endpoint::from_sockaddr(nullptr, sizeof(sin), ec));
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidArgument);

    // Unsupported address family
    sockaddr_storage storage{};
    storage.ss_family = AF_UNIX;
    ec.clear();
    static_cast<void>(tcpocket::Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&storage), sizeof(storage), ec));
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidArgument);
}

TEST(EndpointTest, SockaddrRoundTrip) {
    const std::vector<std::string> endpoints = {
        "127.0.0.1:8080",
        "192.168.1.1:443",
        "0.0.0.0:0",
        "[::1]:8080",
        "[2001:db8::1]:53",
        "[::]:0"
    };

    for (const auto& ep_str : endpoints) {
        auto original = tcpocket::Endpoint::from_string(ep_str);
        sockaddr_storage storage{};
        socklen_t len = original.to_sockaddr(storage);

        auto restored = tcpocket::Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&storage), len);
        EXPECT_EQ(restored, original);
        EXPECT_EQ(restored.to_string(), ep_str);
    }
}

TEST(EndpointTest, ComparisonOperators) {
    auto ep1 = tcpocket::Endpoint::from_string("10.0.0.1:80");
    auto ep2 = tcpocket::Endpoint::from_string("10.0.0.1:80");
    auto ep3 = tcpocket::Endpoint::from_string("10.0.0.1:81");
    auto ep4 = tcpocket::Endpoint::from_string("10.0.0.2:80");
    auto ep_v6 = tcpocket::Endpoint::from_string("[::1]:80");

    EXPECT_EQ(ep1, ep2);
    EXPECT_NE(ep1, ep3);
    EXPECT_NE(ep1, ep4);
    EXPECT_NE(ep1, ep_v6);

    EXPECT_LT(ep1, ep3);
    EXPECT_LE(ep1, ep2);
    EXPECT_LE(ep1, ep3);
    EXPECT_GT(ep3, ep1);
    EXPECT_GE(ep1, ep2);
    EXPECT_GE(ep3, ep1);
    EXPECT_LT(ep1, ep4);

    EXPECT_TRUE((ep1 < ep_v6) || (ep_v6 < ep1));
}

TEST(EndpointTest, HashAndUnorderedContainers) {
    std::unordered_set<tcpocket::Endpoint> ep_set;

    auto ep1 = tcpocket::Endpoint::from_string("127.0.0.1:8080");
    auto ep2 = tcpocket::Endpoint::from_string("127.0.0.1:9090");
    auto ep3 = tcpocket::Endpoint::from_string("[::1]:8080");
    auto ep1_dup = tcpocket::Endpoint::from_string("127.0.0.1:8080");

    ep_set.insert(ep1);
    ep_set.insert(ep2);
    ep_set.insert(ep3);
    ep_set.insert(ep1_dup);

    EXPECT_EQ(ep_set.size(), 3u);
    EXPECT_EQ(ep_set.count(ep1), 1u);
    EXPECT_EQ(ep_set.count(ep2), 1u);
    EXPECT_EQ(ep_set.count(ep3), 1u);
    EXPECT_EQ(ep_set.count(tcpocket::Endpoint::from_string("10.0.0.1:80")), 0u);

    std::unordered_map<tcpocket::Endpoint, std::string> ep_map;
    ep_map[ep1] = "http_local_v4";
    ep_map[ep3] = "http_local_v6";

    EXPECT_EQ(ep_map[ep1_dup], "http_local_v4");
    EXPECT_EQ(ep_map[ep3], "http_local_v6");
}
