#include <gtest/gtest.h>
#include "tcpocket/tcp_listener.hpp"
#include "tcpocket/tcp_stream.hpp"
#include "tcpocket/socket_handle.hpp"
#include "tcpocket/endpoint.hpp"
#include "tcpocket/ip_address.hpp"
#include "tcpocket/error.hpp"

#include <chrono>
#include <fcntl.h>
#include <span>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

TEST(TcpListenerTest, TypeTraits) {
    static_assert(!std::is_copy_constructible_v<tcpocket::TcpListener>);
    static_assert(!std::is_copy_assignable_v<tcpocket::TcpListener>);
    static_assert(std::is_move_constructible_v<tcpocket::TcpListener>);
    static_assert(std::is_move_assignable_v<tcpocket::TcpListener>);
    static_assert(std::is_nothrow_move_constructible_v<tcpocket::TcpListener>);
    static_assert(std::is_nothrow_move_assignable_v<tcpocket::TcpListener>);
}

TEST(TcpListenerTest, DefaultConstructor) {
    tcpocket::TcpListener listener;
    EXPECT_FALSE(listener.is_listening());
    EXPECT_FALSE(static_cast<bool>(listener));
    EXPECT_EQ(listener.native_handle(), tcpocket::SocketHandle::invalid_handle);
    EXPECT_FALSE(listener.socket_handle().is_valid());
    EXPECT_EQ(listener.local_endpoint(), tcpocket::Endpoint{});
}

TEST(TcpListenerTest, MoveConstruction) {
    auto listener1 = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener1.is_listening());
    int raw_fd = listener1.native_handle();
    auto local_ep = listener1.local_endpoint();

    tcpocket::TcpListener listener2(std::move(listener1));

    EXPECT_TRUE(listener2.is_listening());
    EXPECT_EQ(listener2.native_handle(), raw_fd);
    EXPECT_EQ(listener2.local_endpoint(), local_ep);

    EXPECT_FALSE(listener1.is_listening());
    EXPECT_EQ(listener1.native_handle(), tcpocket::SocketHandle::invalid_handle);
    EXPECT_EQ(listener1.local_endpoint(), tcpocket::Endpoint{});
}

TEST(TcpListenerTest, MoveAssignment) {
    auto listener1 = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    auto listener2 = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));

    int fd1 = listener1.native_handle();
    int fd2 = listener2.native_handle();
    auto ep1 = listener1.local_endpoint();

    listener2 = std::move(listener1);

    EXPECT_TRUE(listener2.is_listening());
    EXPECT_EQ(listener2.native_handle(), fd1);
    EXPECT_EQ(listener2.local_endpoint(), ep1);

    EXPECT_EQ(::fcntl(fd2, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);

    EXPECT_FALSE(listener1.is_listening());
    EXPECT_EQ(listener1.native_handle(), tcpocket::SocketHandle::invalid_handle);
    EXPECT_EQ(listener1.local_endpoint(), tcpocket::Endpoint{});
}

TEST(TcpListenerTest, MoveSelfAssignmentIsSafe) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    int fd = listener.native_handle();
    auto ep = listener.local_endpoint();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
    listener = std::move(listener);
#pragma GCC diagnostic pop

    EXPECT_TRUE(listener.is_listening());
    EXPECT_EQ(listener.native_handle(), fd);
    EXPECT_EQ(listener.local_endpoint(), ep);
    EXPECT_NE(::fcntl(fd, F_GETFD), -1);
}

TEST(TcpListenerTest, BindByEndpointEphemeralIpv4) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    EXPECT_TRUE(listener.is_listening());
    EXPECT_TRUE(static_cast<bool>(listener));
    EXPECT_NE(listener.native_handle(), tcpocket::SocketHandle::invalid_handle);
    EXPECT_TRUE(listener.local_endpoint().address().is_loopback());
    EXPECT_NE(listener.local_endpoint().port(), 0);

    std::error_code ec;
    auto listener_noexcept = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0), ec);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(listener_noexcept.is_listening());
    EXPECT_NE(listener_noexcept.local_endpoint().port(), 0);
}

TEST(TcpListenerTest, BindByEndpointEphemeralIpv6) {
    std::error_code ec;
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv6(0), ec);
    if (!ec) {
        EXPECT_TRUE(listener.is_listening());
        EXPECT_TRUE(listener.local_endpoint().address().is_ipv6());
        EXPECT_NE(listener.local_endpoint().port(), 0);
    }
}

TEST(TcpListenerTest, BindByPort) {
    auto listener = tcpocket::TcpListener::bind(0);
    EXPECT_TRUE(listener.is_listening());
    EXPECT_TRUE(listener.local_endpoint().address().is_ipv4());
    EXPECT_NE(listener.local_endpoint().port(), 0);

    std::error_code ec;
    auto listener_noexcept = tcpocket::TcpListener::bind(0, ec);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(listener_noexcept.is_listening());
    EXPECT_NE(listener_noexcept.local_endpoint().port(), 0);
}

TEST(TcpListenerTest, BindByPortDualStack) {
    std::error_code ec;
    auto listener = tcpocket::TcpListener::bind(0, ec, /*ipv6_dual_stack=*/true);
    if (!ec) {
        EXPECT_TRUE(listener.is_listening());
        EXPECT_TRUE(listener.local_endpoint().address().is_ipv6());
        EXPECT_NE(listener.local_endpoint().port(), 0);

        int v6only = 1;
        socklen_t len = sizeof(v6only);
        if (::getsockopt(listener.native_handle(), IPPROTO_IPV6, IPV6_V6ONLY, &v6only, &len) == 0) {
            EXPECT_EQ(v6only, 0);
        }
    }
}

TEST(TcpListenerTest, BindByHostAndPort) {
    auto listener = tcpocket::TcpListener::bind("127.0.0.1", 0);
    EXPECT_TRUE(listener.is_listening());
    EXPECT_EQ(listener.local_endpoint().address(), tcpocket::IpAddress::loopback_ipv4());
    EXPECT_NE(listener.local_endpoint().port(), 0);

    std::error_code ec;
    auto listener_noexcept = tcpocket::TcpListener::bind("localhost", 0, ec);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(listener_noexcept.is_listening());
    EXPECT_NE(listener_noexcept.local_endpoint().port(), 0);
}

TEST(TcpListenerTest, BindDuplicatePortFails) {
    auto listener1 = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    uint16_t port = listener1.local_endpoint().port();
    ASSERT_NE(port, 0);

    EXPECT_THROW(
        static_cast<void>(tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(port))),
        tcpocket::TcpException
    );

    std::error_code ec;
    auto listener2 = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(port), ec);
    EXPECT_TRUE(ec);
    EXPECT_FALSE(listener2.is_listening());
    EXPECT_TRUE(ec == tcpocket::SocketError::AddressInUse || ec == tcpocket::SocketError::InvalidArgument);
}

TEST(TcpListenerTest, BindInvalidHostFails) {
    EXPECT_THROW(
        static_cast<void>(tcpocket::TcpListener::bind("invalid.nonexistent.domain.test", 0)),
        tcpocket::TcpException
    );

    std::error_code ec;
    auto listener = tcpocket::TcpListener::bind("invalid.nonexistent.domain.test", 0, ec);
    EXPECT_TRUE(ec);
    EXPECT_FALSE(listener.is_listening());
}

TEST(TcpListenerTest, BlockingAcceptSuccess) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());
    auto server_ep = listener.local_endpoint();

    std::thread client_thread([server_ep]() {
        auto client = tcpocket::TcpStream::connect(server_ep);
        ASSERT_TRUE(client.is_open());
        client.write_all(std::string_view("ping"));

        char buf[16]{};
        size_t n = client.read(std::span<char>(buf));
        EXPECT_EQ(std::string_view(buf, n), "pong");
    });

    auto [server_stream, client_ep] = listener.accept();
    EXPECT_TRUE(server_stream.is_open());
    EXPECT_EQ(server_stream.local_endpoint(), server_ep);
    EXPECT_EQ(server_stream.remote_endpoint(), client_ep);
    EXPECT_TRUE(client_ep.address().is_loopback());
    EXPECT_NE(client_ep.port(), 0);

    char buf[16]{};
    size_t n = server_stream.read(std::span<char>(buf));
    EXPECT_EQ(std::string_view(buf, n), "ping");
    server_stream.write_all(std::string_view("pong"));

    client_thread.join();
}

TEST(TcpListenerTest, BlockingAcceptNonThrowing) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());
    auto server_ep = listener.local_endpoint();

    std::thread client_thread([server_ep]() {
        auto client = tcpocket::TcpStream::connect(server_ep);
        ASSERT_TRUE(client.is_open());
    });

    std::error_code ec;
    auto [server_stream, client_ep] = listener.accept(ec);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(server_stream.is_open());
    EXPECT_EQ(server_stream.remote_endpoint(), client_ep);

    client_thread.join();
}

TEST(TcpListenerTest, TimedAcceptTimesOut) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());

    auto start = std::chrono::steady_clock::now();
    EXPECT_THROW(
        static_cast<void>(listener.accept(50ms)),
        tcpocket::TcpException
    );
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    EXPECT_GE(elapsed.count(), 40);

    std::error_code ec;
    auto [stream, ep] = listener.accept(50ms, ec);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::TimedOut);
    EXPECT_FALSE(stream.is_open());
}

TEST(TcpListenerTest, TimedAcceptSuccess) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());
    auto server_ep = listener.local_endpoint();

    std::thread client_thread([server_ep]() {
        std::this_thread::sleep_for(50ms);
        auto client = tcpocket::TcpStream::connect(server_ep);
        ASSERT_TRUE(client.is_open());
    });

    std::error_code ec;
    auto [stream, client_ep] = listener.accept(2000ms, ec);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(stream.is_open());
    EXPECT_EQ(stream.remote_endpoint(), client_ep);

    client_thread.join();
}

TEST(TcpListenerTest, ExplicitClose) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());
    int fd = listener.native_handle();

    listener.close();
    EXPECT_FALSE(listener.is_listening());
    EXPECT_FALSE(static_cast<bool>(listener));
    EXPECT_EQ(listener.native_handle(), tcpocket::SocketHandle::invalid_handle);
    EXPECT_EQ(listener.local_endpoint(), tcpocket::Endpoint{});

    EXPECT_EQ(::fcntl(fd, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);

    EXPECT_NO_THROW(listener.close());
}

TEST(TcpListenerTest, SetNonblocking) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());

    std::error_code ec;
    listener.set_nonblocking(true, ec);
    EXPECT_FALSE(ec);
    int flags = ::fcntl(listener.native_handle(), F_GETFL, 0);
    EXPECT_NE(flags & O_NONBLOCK, 0);

    listener.set_nonblocking(false);
    flags = ::fcntl(listener.native_handle(), F_GETFL, 0);
    EXPECT_EQ(flags & O_NONBLOCK, 0);
}

TEST(TcpListenerTest, OperationsOnClosedListener) {
    tcpocket::TcpListener listener;
    ASSERT_FALSE(listener.is_listening());

    std::error_code ec;
    auto [stream1, ep1] = listener.accept(ec);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);
    EXPECT_FALSE(stream1.is_open());

    auto [stream2, ep2] = listener.accept(50ms, ec);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);
    EXPECT_FALSE(stream2.is_open());

    listener.set_nonblocking(true, ec);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    EXPECT_THROW(
        static_cast<void>(listener.accept()),
        tcpocket::TcpException
    );
    EXPECT_THROW(
        static_cast<void>(listener.accept(50ms)),
        tcpocket::TcpException
    );
    EXPECT_THROW(
        listener.set_nonblocking(true),
        tcpocket::TcpException
    );
}
