#include <gtest/gtest.h>
#include "tcpocket/ip_address.hpp"
#include "tcpocket/error.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

TEST(IpAddressTest, TypeTraits) {
    static_assert(std::is_default_constructible_v<tcpocket::IpAddress>);
    static_assert(std::is_copy_constructible_v<tcpocket::IpAddress>);
    static_assert(std::is_copy_assignable_v<tcpocket::IpAddress>);
    static_assert(std::is_move_constructible_v<tcpocket::IpAddress>);
    static_assert(std::is_move_assignable_v<tcpocket::IpAddress>);
    static_assert(std::is_nothrow_move_constructible_v<tcpocket::IpAddress>);
    static_assert(std::is_nothrow_move_assignable_v<tcpocket::IpAddress>);
}

TEST(IpAddressTest, DefaultConstructor) {
    tcpocket::IpAddress ip;
    EXPECT_EQ(ip.version(), tcpocket::IpVersion::V4);
    EXPECT_TRUE(ip.is_ipv4());
    EXPECT_FALSE(ip.is_ipv6());
    EXPECT_TRUE(ip.is_unspecified());
    EXPECT_FALSE(ip.is_loopback());
    EXPECT_FALSE(ip.is_multicast());
    EXPECT_EQ(ip.to_string(), "0.0.0.0");

    auto bytes = ip.bytes();
    ASSERT_EQ(bytes.size(), 4u);
    for (auto b : bytes) {
        EXPECT_EQ(b, std::byte{0});
    }
}

TEST(IpAddressTest, StaticFactoriesIpv4) {
    auto any_v4 = tcpocket::IpAddress::any_ipv4();
    EXPECT_EQ(any_v4.version(), tcpocket::IpVersion::V4);
    EXPECT_TRUE(any_v4.is_ipv4());
    EXPECT_FALSE(any_v4.is_ipv6());
    EXPECT_TRUE(any_v4.is_unspecified());
    EXPECT_FALSE(any_v4.is_loopback());
    EXPECT_FALSE(any_v4.is_multicast());
    EXPECT_EQ(any_v4.to_string(), "0.0.0.0");

    auto loopback_v4 = tcpocket::IpAddress::loopback_ipv4();
    EXPECT_EQ(loopback_v4.version(), tcpocket::IpVersion::V4);
    EXPECT_TRUE(loopback_v4.is_ipv4());
    EXPECT_FALSE(loopback_v4.is_ipv6());
    EXPECT_FALSE(loopback_v4.is_unspecified());
    EXPECT_TRUE(loopback_v4.is_loopback());
    EXPECT_FALSE(loopback_v4.is_multicast());
    EXPECT_EQ(loopback_v4.to_string(), "127.0.0.1");
}

TEST(IpAddressTest, StaticFactoriesIpv6) {
    auto any_v6 = tcpocket::IpAddress::any_ipv6();
    EXPECT_EQ(any_v6.version(), tcpocket::IpVersion::V6);
    EXPECT_FALSE(any_v6.is_ipv4());
    EXPECT_TRUE(any_v6.is_ipv6());
    EXPECT_TRUE(any_v6.is_unspecified());
    EXPECT_FALSE(any_v6.is_loopback());
    EXPECT_FALSE(any_v6.is_multicast());
    EXPECT_EQ(any_v6.to_string(), "::");

    auto loopback_v6 = tcpocket::IpAddress::loopback_ipv6();
    EXPECT_EQ(loopback_v6.version(), tcpocket::IpVersion::V6);
    EXPECT_FALSE(loopback_v6.is_ipv4());
    EXPECT_TRUE(loopback_v6.is_ipv6());
    EXPECT_FALSE(loopback_v6.is_unspecified());
    EXPECT_TRUE(loopback_v6.is_loopback());
    EXPECT_FALSE(loopback_v6.is_multicast());
    EXPECT_EQ(loopback_v6.to_string(), "::1");
}

TEST(IpAddressTest, ConstructFromNativeInAddr) {
    ::in_addr native_v4{};
    ASSERT_EQ(::inet_pton(AF_INET, "192.168.1.100", &native_v4), 1);

    tcpocket::IpAddress ip(native_v4);
    EXPECT_TRUE(ip.is_ipv4());
    EXPECT_FALSE(ip.is_ipv6());
    EXPECT_EQ(ip.version(), tcpocket::IpVersion::V4);
    EXPECT_EQ(ip.to_string(), "192.168.1.100");

    auto bytes = ip.bytes();
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], static_cast<std::byte>(192));
    EXPECT_EQ(bytes[1], static_cast<std::byte>(168));
    EXPECT_EQ(bytes[2], static_cast<std::byte>(1));
    EXPECT_EQ(bytes[3], static_cast<std::byte>(100));
}

TEST(IpAddressTest, ConstructFromNativeIn6Addr) {
    ::in6_addr native_v6{};
    ASSERT_EQ(::inet_pton(AF_INET6, "2001:db8::1", &native_v6), 1);

    tcpocket::IpAddress ip(native_v6);
    EXPECT_FALSE(ip.is_ipv4());
    EXPECT_TRUE(ip.is_ipv6());
    EXPECT_EQ(ip.version(), tcpocket::IpVersion::V6);
    EXPECT_EQ(ip.to_string(), "2001:db8::1");

    auto bytes = ip.bytes();
    ASSERT_EQ(bytes.size(), 16u);
    EXPECT_EQ(bytes[0], static_cast<std::byte>(0x20));
    EXPECT_EQ(bytes[1], static_cast<std::byte>(0x01));
    EXPECT_EQ(bytes[2], static_cast<std::byte>(0x0d));
    EXPECT_EQ(bytes[3], static_cast<std::byte>(0xb8));
    EXPECT_EQ(bytes[15], static_cast<std::byte>(0x01));
}

TEST(IpAddressTest, ParseValidIpv4Strings) {
    const std::vector<std::string> valid_v4 = {
        "127.0.0.1",
        "192.168.1.1",
        "10.0.0.1",
        "255.255.255.255",
        "0.0.0.0"
    };

    for (const auto& ip_str : valid_v4) {
        // Throwing overload
        auto ip = tcpocket::IpAddress::from_string(ip_str);
        EXPECT_TRUE(ip.is_ipv4());
        EXPECT_FALSE(ip.is_ipv6());
        EXPECT_EQ(ip.to_string(), ip_str);

        // Non-throwing overload
        std::error_code ec;
        auto ip_noexcept = tcpocket::IpAddress::from_string(ip_str, ec);
        EXPECT_FALSE(ec);
        EXPECT_EQ(ip_noexcept, ip);
    }
}

TEST(IpAddressTest, ParseValidIpv6Strings) {
    const std::vector<std::string> valid_v6 = {
        "::1",
        "fe80::1",
        "2001:db8::1",
        "::",
        "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"
    };

    for (const auto& ip_str : valid_v6) {
        // Throwing overload
        auto ip = tcpocket::IpAddress::from_string(ip_str);
        EXPECT_FALSE(ip.is_ipv4());
        EXPECT_TRUE(ip.is_ipv6());
        EXPECT_EQ(ip.version(), tcpocket::IpVersion::V6);

        // Non-throwing overload
        std::error_code ec;
        auto ip_noexcept = tcpocket::IpAddress::from_string(ip_str, ec);
        EXPECT_FALSE(ec);
        EXPECT_EQ(ip_noexcept, ip);

        // Round-trip check
        auto round_trip = tcpocket::IpAddress::from_string(ip.to_string());
        EXPECT_EQ(round_trip, ip);
    }
}

TEST(IpAddressTest, ParseInvalidIpStrings) {
    const std::vector<std::string_view> invalid_ips = {
        "256.0.0.1",
        "1.2.3",
        "1.2.3.4.5",
        "abc",
        ":::",
        "",
        "127.0.0.1:8080",
        "[::1]",
        "fe80:::1",
        "192.168.1. 1",
        "192.168.1.1/24"
    };

    for (const auto& invalid_ip : invalid_ips) {
        // Throwing overload must throw TcpException
        EXPECT_THROW({
            try {
                static_cast<void>(tcpocket::IpAddress::from_string(invalid_ip));
            } catch (const tcpocket::TcpException& ex) {
                EXPECT_EQ(ex.code(), tcpocket::SocketError::InvalidArgument);
                throw;
            }
        }, tcpocket::TcpException);

        // Non-throwing overload must set error_code to InvalidArgument
        std::error_code ec;
        static_cast<void>(tcpocket::IpAddress::from_string(invalid_ip, ec));
        EXPECT_TRUE(ec);
        EXPECT_EQ(ec, tcpocket::SocketError::InvalidArgument);
    }
}

TEST(IpAddressTest, ClassificationChecks) {
    // IPv4 Loopback (127.0.0.0/8)
    auto lb1 = tcpocket::IpAddress::from_string("127.0.0.1");
    auto lb2 = tcpocket::IpAddress::from_string("127.0.0.2");
    auto lb3 = tcpocket::IpAddress::from_string("127.255.255.254");
    EXPECT_TRUE(lb1.is_loopback());
    EXPECT_TRUE(lb2.is_loopback());
    EXPECT_TRUE(lb3.is_loopback());
    EXPECT_FALSE(lb1.is_multicast());
    EXPECT_FALSE(lb1.is_unspecified());

    // IPv6 Loopback (::1)
    auto lb_v6 = tcpocket::IpAddress::from_string("::1");
    EXPECT_TRUE(lb_v6.is_loopback());
    EXPECT_FALSE(lb_v6.is_multicast());
    EXPECT_FALSE(lb_v6.is_unspecified());

    // IPv4 Multicast (224.0.0.0 - 239.255.255.255)
    auto mcast1 = tcpocket::IpAddress::from_string("224.0.0.1");
    auto mcast2 = tcpocket::IpAddress::from_string("239.255.255.250");
    EXPECT_TRUE(mcast1.is_multicast());
    EXPECT_TRUE(mcast2.is_multicast());
    EXPECT_FALSE(mcast1.is_loopback());
    EXPECT_FALSE(mcast1.is_unspecified());

    // IPv6 Multicast (ff00::/8)
    auto mcast_v6_1 = tcpocket::IpAddress::from_string("ff02::1");
    auto mcast_v6_2 = tcpocket::IpAddress::from_string("ff01::1");
    EXPECT_TRUE(mcast_v6_1.is_multicast());
    EXPECT_TRUE(mcast_v6_2.is_multicast());
    EXPECT_FALSE(mcast_v6_1.is_loopback());
    EXPECT_FALSE(mcast_v6_1.is_unspecified());

    // Unspecified addresses
    auto unspec_v4 = tcpocket::IpAddress::from_string("0.0.0.0");
    auto unspec_v6 = tcpocket::IpAddress::from_string("::");
    EXPECT_TRUE(unspec_v4.is_unspecified());
    EXPECT_TRUE(unspec_v6.is_unspecified());
    EXPECT_FALSE(unspec_v4.is_loopback());
    EXPECT_FALSE(unspec_v6.is_loopback());
    EXPECT_FALSE(unspec_v4.is_multicast());
    EXPECT_FALSE(unspec_v6.is_multicast());

    // Regular unicast addresses
    auto unicast_v4 = tcpocket::IpAddress::from_string("8.8.8.8");
    auto unicast_v6 = tcpocket::IpAddress::from_string("2001:4860:4860::8888");
    EXPECT_FALSE(unicast_v4.is_loopback());
    EXPECT_FALSE(unicast_v4.is_multicast());
    EXPECT_FALSE(unicast_v4.is_unspecified());
    EXPECT_FALSE(unicast_v6.is_loopback());
    EXPECT_FALSE(unicast_v6.is_multicast());
    EXPECT_FALSE(unicast_v6.is_unspecified());
}

TEST(IpAddressTest, BytesAccessor) {
    auto ip_v4 = tcpocket::IpAddress::from_string("192.168.1.1");
    auto bytes_v4 = ip_v4.bytes();
    ASSERT_EQ(bytes_v4.size(), 4u);
    EXPECT_EQ(bytes_v4[0], static_cast<std::byte>(192));
    EXPECT_EQ(bytes_v4[1], static_cast<std::byte>(168));
    EXPECT_EQ(bytes_v4[2], static_cast<std::byte>(1));
    EXPECT_EQ(bytes_v4[3], static_cast<std::byte>(1));

    auto ip_v6 = tcpocket::IpAddress::from_string("::1");
    auto bytes_v6 = ip_v6.bytes();
    ASSERT_EQ(bytes_v6.size(), 16u);
    for (size_t i = 0; i < 15; ++i) {
        EXPECT_EQ(bytes_v6[i], std::byte{0});
    }
    EXPECT_EQ(bytes_v6[15], static_cast<std::byte>(1));
}

TEST(IpAddressTest, ComparisonOperators) {
    auto ip1 = tcpocket::IpAddress::from_string("10.0.0.1");
    auto ip2 = tcpocket::IpAddress::from_string("10.0.0.1");
    auto ip3 = tcpocket::IpAddress::from_string("10.0.0.2");
    auto ip4 = tcpocket::IpAddress::from_string("192.168.1.1");
    auto ip6 = tcpocket::IpAddress::from_string("::1");

    // Equality
    EXPECT_EQ(ip1, ip2);
    EXPECT_NE(ip1, ip3);
    EXPECT_NE(ip1, ip6);

    // Relational ordering (three-way comparison <=>)
    EXPECT_LT(ip1, ip3);
    EXPECT_LE(ip1, ip2);
    EXPECT_LE(ip1, ip3);
    EXPECT_GT(ip3, ip1);
    EXPECT_GE(ip1, ip2);
    EXPECT_GE(ip3, ip1);
    EXPECT_LT(ip3, ip4);

    // Cross-version comparison should provide a strict weak ordering
    EXPECT_TRUE((ip1 < ip6) || (ip6 < ip1));
    EXPECT_FALSE(ip1 == ip6);
}

TEST(IpAddressTest, HashAndUnorderedContainers) {
    std::unordered_set<tcpocket::IpAddress> ip_set;

    auto ip1 = tcpocket::IpAddress::from_string("127.0.0.1");
    auto ip2 = tcpocket::IpAddress::from_string("192.168.1.1");
    auto ip3 = tcpocket::IpAddress::from_string("::1");
    auto ip1_dup = tcpocket::IpAddress::from_string("127.0.0.1");

    ip_set.insert(ip1);
    ip_set.insert(ip2);
    ip_set.insert(ip3);
    ip_set.insert(ip1_dup);

    EXPECT_EQ(ip_set.size(), 3u);
    EXPECT_EQ(ip_set.count(ip1), 1u);
    EXPECT_EQ(ip_set.count(ip2), 1u);
    EXPECT_EQ(ip_set.count(ip3), 1u);
    EXPECT_EQ(ip_set.count(tcpocket::IpAddress::from_string("10.0.0.1")), 0u);

    std::unordered_map<tcpocket::IpAddress, std::string> ip_map;
    ip_map[ip1] = "localhost_v4";
    ip_map[ip3] = "localhost_v6";

    EXPECT_EQ(ip_map[ip1_dup], "localhost_v4");
    EXPECT_EQ(ip_map[ip3], "localhost_v6");
}
