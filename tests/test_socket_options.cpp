#include <gtest/gtest.h>
#include "tcpocket/socket_options.hpp"
#include "tcpocket/socket_handle.hpp"
#include "tcpocket/error.hpp"

#include <chrono>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

TEST(SocketOptionsTest, SetReuseAddress) {
    auto socket = tcpocket::SocketHandle::create_stream_socket(AF_INET);
    ASSERT_TRUE(socket.is_valid());

    // Enable SO_REUSEADDR using throwing overload
    tcpocket::set_reuse_address(socket, true);

    int optval = 0;
    socklen_t optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_REUSEADDR, &optval, &optlen), 0);
    EXPECT_NE(optval, 0);

    // Disable SO_REUSEADDR using throwing overload
    tcpocket::set_reuse_address(socket, false);

    optval = 0;
    optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_REUSEADDR, &optval, &optlen), 0);
    EXPECT_EQ(optval, 0);

    // Non-throwing overload
    std::error_code ec;
    tcpocket::set_reuse_address(socket, true, ec);
    EXPECT_FALSE(ec);

    optval = 0;
    optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_REUSEADDR, &optval, &optlen), 0);
    EXPECT_NE(optval, 0);
}

TEST(SocketOptionsTest, SetReusePort) {
    auto socket = tcpocket::SocketHandle::create_stream_socket(AF_INET);
    ASSERT_TRUE(socket.is_valid());

    // Enable SO_REUSEPORT using throwing overload
    tcpocket::set_reuse_port(socket, true);

    int optval = 0;
    socklen_t optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_REUSEPORT, &optval, &optlen), 0);
    EXPECT_NE(optval, 0);

    // Disable SO_REUSEPORT using throwing overload
    tcpocket::set_reuse_port(socket, false);

    optval = 0;
    optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_REUSEPORT, &optval, &optlen), 0);
    EXPECT_EQ(optval, 0);

    // Non-throwing overload
    std::error_code ec;
    tcpocket::set_reuse_port(socket, true, ec);
    EXPECT_FALSE(ec);

    optval = 0;
    optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_REUSEPORT, &optval, &optlen), 0);
    EXPECT_NE(optval, 0);
}

TEST(SocketOptionsTest, SetTcpNoDelay) {
    auto socket = tcpocket::SocketHandle::create_stream_socket(AF_INET);
    ASSERT_TRUE(socket.is_valid());

    // Enable TCP_NODELAY using throwing overload
    tcpocket::set_tcp_nodelay(socket, true);

    int optval = 0;
    socklen_t optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), IPPROTO_TCP, TCP_NODELAY, &optval, &optlen), 0);
    EXPECT_NE(optval, 0);

    // Disable TCP_NODELAY using throwing overload
    tcpocket::set_tcp_nodelay(socket, false);

    optval = 0;
    optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), IPPROTO_TCP, TCP_NODELAY, &optval, &optlen), 0);
    EXPECT_EQ(optval, 0);

    // Non-throwing overload
    std::error_code ec;
    tcpocket::set_tcp_nodelay(socket, true, ec);
    EXPECT_FALSE(ec);

    optval = 0;
    optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), IPPROTO_TCP, TCP_NODELAY, &optval, &optlen), 0);
    EXPECT_NE(optval, 0);
}

TEST(SocketOptionsTest, SetKeepAlive) {
    auto socket = tcpocket::SocketHandle::create_stream_socket(AF_INET);
    ASSERT_TRUE(socket.is_valid());

    // Enable SO_KEEPALIVE using throwing overload
    tcpocket::set_keep_alive(socket, true);

    int optval = 0;
    socklen_t optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen), 0);
    EXPECT_NE(optval, 0);

    // Disable SO_KEEPALIVE using throwing overload
    tcpocket::set_keep_alive(socket, false);

    optval = 0;
    optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen), 0);
    EXPECT_EQ(optval, 0);

    // Non-throwing overload
    std::error_code ec;
    tcpocket::set_keep_alive(socket, true, ec);
    EXPECT_FALSE(ec);

    optval = 0;
    optlen = sizeof(optval);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen), 0);
    EXPECT_NE(optval, 0);
}

TEST(SocketOptionsTest, SetReceiveTimeout) {
    auto socket = tcpocket::SocketHandle::create_stream_socket(AF_INET);
    ASSERT_TRUE(socket.is_valid());

    // Set 2500ms (2 seconds, 500000 microseconds)
    tcpocket::set_receive_timeout(socket, std::chrono::milliseconds(2500));

    struct timeval tv{};
    socklen_t optlen = sizeof(tv);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, &optlen), 0);
    EXPECT_EQ(tv.tv_sec, 2);
    EXPECT_EQ(tv.tv_usec, 500000);

    // Set 0ms (disable timeout)
    tcpocket::set_receive_timeout(socket, std::chrono::milliseconds(0));

    tv = {};
    optlen = sizeof(tv);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, &optlen), 0);
    EXPECT_EQ(tv.tv_sec, 0);
    EXPECT_EQ(tv.tv_usec, 0);

    // Non-throwing overload with sub-second timeout (150ms)
    std::error_code ec;
    tcpocket::set_receive_timeout(socket, std::chrono::milliseconds(150), ec);
    EXPECT_FALSE(ec);

    tv = {};
    optlen = sizeof(tv);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, &optlen), 0);
    EXPECT_EQ(tv.tv_sec, 0);
    EXPECT_EQ(tv.tv_usec, 150000);
}

TEST(SocketOptionsTest, SetSendTimeout) {
    auto socket = tcpocket::SocketHandle::create_stream_socket(AF_INET);
    ASSERT_TRUE(socket.is_valid());

    // Set 3200ms (3 seconds, 200000 microseconds)
    tcpocket::set_send_timeout(socket, std::chrono::milliseconds(3200));

    struct timeval tv{};
    socklen_t optlen = sizeof(tv);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, &optlen), 0);
    EXPECT_EQ(tv.tv_sec, 3);
    EXPECT_EQ(tv.tv_usec, 200000);

    // Set 0ms (disable timeout)
    tcpocket::set_send_timeout(socket, std::chrono::milliseconds(0));

    tv = {};
    optlen = sizeof(tv);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, &optlen), 0);
    EXPECT_EQ(tv.tv_sec, 0);
    EXPECT_EQ(tv.tv_usec, 0);

    // Non-throwing overload with sub-second timeout (750ms)
    std::error_code ec;
    tcpocket::set_send_timeout(socket, std::chrono::milliseconds(750), ec);
    EXPECT_FALSE(ec);

    tv = {};
    optlen = sizeof(tv);
    ASSERT_EQ(::getsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, &optlen), 0);
    EXPECT_EQ(tv.tv_sec, 0);
    EXPECT_EQ(tv.tv_usec, 750000);
}

TEST(SocketOptionsTest, NegativeTimeoutsReturnError) {
    auto socket = tcpocket::SocketHandle::create_stream_socket(AF_INET);
    ASSERT_TRUE(socket.is_valid());

    std::error_code ec;
    tcpocket::set_receive_timeout(socket, std::chrono::milliseconds(-100), ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidArgument);

    EXPECT_THROW(
        tcpocket::set_receive_timeout(socket, std::chrono::milliseconds(-1)),
        tcpocket::TcpException
    );

    tcpocket::set_send_timeout(socket, std::chrono::milliseconds(-100), ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidArgument);

    EXPECT_THROW(
        tcpocket::set_send_timeout(socket, std::chrono::milliseconds(-1)),
        tcpocket::TcpException
    );
}

TEST(SocketOptionsTest, SetNonblocking) {
    auto socket = tcpocket::SocketHandle::create_stream_socket(AF_INET);
    ASSERT_TRUE(socket.is_valid());

    int initial_flags = ::fcntl(socket.native_handle(), F_GETFL);
    ASSERT_NE(initial_flags, -1);
    EXPECT_FALSE(initial_flags & O_NONBLOCK);

    // Enable non-blocking mode using throwing overload
    tcpocket::set_nonblocking(socket, true);

    int flags = ::fcntl(socket.native_handle(), F_GETFL);
    ASSERT_NE(flags, -1);
    EXPECT_TRUE(flags & O_NONBLOCK);

    // Disable non-blocking mode using throwing overload
    tcpocket::set_nonblocking(socket, false);

    flags = ::fcntl(socket.native_handle(), F_GETFL);
    ASSERT_NE(flags, -1);
    EXPECT_FALSE(flags & O_NONBLOCK);

    // Non-throwing overload
    std::error_code ec;
    tcpocket::set_nonblocking(socket, true, ec);
    EXPECT_FALSE(ec);

    flags = ::fcntl(socket.native_handle(), F_GETFL);
    ASSERT_NE(flags, -1);
    EXPECT_TRUE(flags & O_NONBLOCK);
}

TEST(SocketOptionsTest, GetSocketErrorInitialState) {
    auto socket = tcpocket::SocketHandle::create_stream_socket(AF_INET);
    ASSERT_TRUE(socket.is_valid());

    // Non-throwing overload
    std::error_code ec;
    std::error_code sock_err = tcpocket::get_socket_error(socket, ec);
    EXPECT_FALSE(ec);
    EXPECT_EQ(sock_err, tcpocket::SocketError::Success);
    EXPECT_FALSE(sock_err);

    // Throwing overload
    std::error_code sock_err2 = tcpocket::get_socket_error(socket);
    EXPECT_EQ(sock_err2, tcpocket::SocketError::Success);
    EXPECT_FALSE(sock_err2);
}

TEST(SocketOptionsTest, NonThrowingOverloadsOnInvalidSocket) {
    tcpocket::SocketHandle invalid_socket;
    ASSERT_FALSE(invalid_socket.is_valid());

    std::error_code ec;

    tcpocket::set_reuse_address(invalid_socket, true, ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    tcpocket::set_reuse_port(invalid_socket, true, ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    tcpocket::set_tcp_nodelay(invalid_socket, true, ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    tcpocket::set_keep_alive(invalid_socket, true, ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    tcpocket::set_receive_timeout(invalid_socket, std::chrono::milliseconds(100), ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    tcpocket::set_send_timeout(invalid_socket, std::chrono::milliseconds(100), ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    tcpocket::set_nonblocking(invalid_socket, true, ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    static_cast<void>(tcpocket::get_socket_error(invalid_socket, ec));
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);
}

TEST(SocketOptionsTest, ThrowingOverloadsOnInvalidSocket) {
    tcpocket::SocketHandle invalid_socket;
    ASSERT_FALSE(invalid_socket.is_valid());

    try {
        tcpocket::set_reuse_address(invalid_socket, true);
        FAIL() << "Expected TcpException";
    } catch (const tcpocket::TcpException& ex) {
        EXPECT_EQ(ex.code(), tcpocket::SocketError::InvalidDescriptor);
    }

    try {
        tcpocket::set_reuse_port(invalid_socket, true);
        FAIL() << "Expected TcpException";
    } catch (const tcpocket::TcpException& ex) {
        EXPECT_EQ(ex.code(), tcpocket::SocketError::InvalidDescriptor);
    }

    try {
        tcpocket::set_tcp_nodelay(invalid_socket, true);
        FAIL() << "Expected TcpException";
    } catch (const tcpocket::TcpException& ex) {
        EXPECT_EQ(ex.code(), tcpocket::SocketError::InvalidDescriptor);
    }

    try {
        tcpocket::set_keep_alive(invalid_socket, true);
        FAIL() << "Expected TcpException";
    } catch (const tcpocket::TcpException& ex) {
        EXPECT_EQ(ex.code(), tcpocket::SocketError::InvalidDescriptor);
    }

    try {
        tcpocket::set_receive_timeout(invalid_socket, std::chrono::milliseconds(100));
        FAIL() << "Expected TcpException";
    } catch (const tcpocket::TcpException& ex) {
        EXPECT_EQ(ex.code(), tcpocket::SocketError::InvalidDescriptor);
    }

    try {
        tcpocket::set_send_timeout(invalid_socket, std::chrono::milliseconds(100));
        FAIL() << "Expected TcpException";
    } catch (const tcpocket::TcpException& ex) {
        EXPECT_EQ(ex.code(), tcpocket::SocketError::InvalidDescriptor);
    }

    try {
        tcpocket::set_nonblocking(invalid_socket, true);
        FAIL() << "Expected TcpException";
    } catch (const tcpocket::TcpException& ex) {
        EXPECT_EQ(ex.code(), tcpocket::SocketError::InvalidDescriptor);
    }

    try {
        static_cast<void>(tcpocket::get_socket_error(invalid_socket));
        FAIL() << "Expected TcpException";
    } catch (const tcpocket::TcpException& ex) {
        EXPECT_EQ(ex.code(), tcpocket::SocketError::InvalidDescriptor);
    }
}
