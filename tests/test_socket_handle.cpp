#include <gtest/gtest.h>
#include "tcpocket/socket_handle.hpp"
#include "tcpocket/error.hpp"

#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <type_traits>
#include <unistd.h>
#include <utility>

TEST(SocketHandleTest, TypeTraits) {
    static_assert(!std::is_copy_constructible_v<tcpocket::SocketHandle>);
    static_assert(!std::is_copy_assignable_v<tcpocket::SocketHandle>);
    static_assert(std::is_move_constructible_v<tcpocket::SocketHandle>);
    static_assert(std::is_move_assignable_v<tcpocket::SocketHandle>);
}

TEST(SocketHandleTest, DefaultConstructionIsInvalid) {
    tcpocket::SocketHandle socket;
    EXPECT_FALSE(socket.is_valid());
    EXPECT_FALSE(static_cast<bool>(socket));
    EXPECT_EQ(socket.native_handle(), tcpocket::SocketHandle::invalid_handle);
}

TEST(SocketHandleTest, ConstructWithRawFd) {
    int raw_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(raw_fd, 0);

    tcpocket::SocketHandle socket(raw_fd);
    EXPECT_TRUE(socket.is_valid());
    EXPECT_TRUE(static_cast<bool>(socket));
    EXPECT_EQ(socket.native_handle(), raw_fd);
}

TEST(SocketHandleTest, MoveConstructionTransfersOwnership) {
    int raw_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(raw_fd, 0);

    tcpocket::SocketHandle s1(raw_fd);
    tcpocket::SocketHandle s2(std::move(s1));

    EXPECT_TRUE(s2.is_valid());
    EXPECT_EQ(s2.native_handle(), raw_fd);
    EXPECT_FALSE(s1.is_valid());
    EXPECT_EQ(s1.native_handle(), tcpocket::SocketHandle::invalid_handle);
}

TEST(SocketHandleTest, MoveAssignmentClosesTargetAndTransfersOwnership) {
    int fd1 = ::socket(AF_INET, SOCK_STREAM, 0);
    int fd2 = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd1, 0);
    ASSERT_GE(fd2, 0);

    tcpocket::SocketHandle s1(fd1);
    tcpocket::SocketHandle s2(fd2);

    s2 = std::move(s1);

    EXPECT_TRUE(s2.is_valid());
    EXPECT_EQ(s2.native_handle(), fd1);
    EXPECT_FALSE(s1.is_valid());
    EXPECT_EQ(s1.native_handle(), tcpocket::SocketHandle::invalid_handle);

    EXPECT_EQ(::fcntl(fd2, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);
}

TEST(SocketHandleTest, MoveSelfAssignmentIsSafe) {
    int raw_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(raw_fd, 0);

    tcpocket::SocketHandle socket(raw_fd);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
    socket = std::move(socket);
#pragma GCC diagnostic pop

    EXPECT_TRUE(socket.is_valid());
    EXPECT_EQ(socket.native_handle(), raw_fd);
    EXPECT_NE(::fcntl(raw_fd, F_GETFD), -1);
}

TEST(SocketHandleTest, DestructorClosesFileDescriptor) {
    int raw_fd = -1;
    {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(fd, 0);
        raw_fd = fd;
        tcpocket::SocketHandle socket(fd);
        EXPECT_NE(::fcntl(raw_fd, F_GETFD), -1);
    }
    EXPECT_EQ(::fcntl(raw_fd, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);
}

TEST(SocketHandleTest, ReleaseTransfersRawFdWithoutClosing) {
    int raw_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(raw_fd, 0);

    tcpocket::SocketHandle socket(raw_fd);
    int released = socket.release();

    EXPECT_EQ(released, raw_fd);
    EXPECT_FALSE(socket.is_valid());
    EXPECT_EQ(socket.native_handle(), tcpocket::SocketHandle::invalid_handle);

    EXPECT_NE(::fcntl(released, F_GETFD), -1);
    ::close(released);
}

TEST(SocketHandleTest, ResetClosesPreviousAndAcquiresNew) {
    int fd1 = ::socket(AF_INET, SOCK_STREAM, 0);
    int fd2 = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd1, 0);
    ASSERT_GE(fd2, 0);

    tcpocket::SocketHandle socket(fd1);

    socket.reset(fd2);
    EXPECT_TRUE(socket.is_valid());
    EXPECT_EQ(socket.native_handle(), fd2);
    EXPECT_EQ(::fcntl(fd1, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);

    socket.reset();
    EXPECT_FALSE(socket.is_valid());
    EXPECT_EQ(socket.native_handle(), tcpocket::SocketHandle::invalid_handle);
    EXPECT_EQ(::fcntl(fd2, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);
}

TEST(SocketHandleTest, ExplicitClose) {
    int raw_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(raw_fd, 0);

    tcpocket::SocketHandle socket(raw_fd);
    socket.close();

    EXPECT_FALSE(socket.is_valid());
    EXPECT_EQ(socket.native_handle(), tcpocket::SocketHandle::invalid_handle);
    EXPECT_EQ(::fcntl(raw_fd, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);

    socket.close();
    EXPECT_FALSE(socket.is_valid());
}

TEST(SocketHandleTest, CreateStreamSocketIpv4) {
    auto socket = tcpocket::SocketHandle::create_stream_socket(AF_INET);
    EXPECT_TRUE(socket.is_valid());

    int flags = ::fcntl(socket.native_handle(), F_GETFD);
    EXPECT_NE(flags, -1);
    EXPECT_TRUE(flags & FD_CLOEXEC);
}

TEST(SocketHandleTest, CreateStreamSocketIpv6) {
    auto socket = tcpocket::SocketHandle::create_stream_socket(AF_INET6);
    EXPECT_TRUE(socket.is_valid());

    int flags = ::fcntl(socket.native_handle(), F_GETFD);
    EXPECT_NE(flags, -1);
    EXPECT_TRUE(flags & FD_CLOEXEC);
}

TEST(SocketHandleTest, CreateStreamSocketErrorHandling) {
    std::error_code ec;
    auto valid_socket = tcpocket::SocketHandle::create_stream_socket(AF_INET, ec);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(valid_socket.is_valid());

    auto invalid_socket = tcpocket::SocketHandle::create_stream_socket(-1, ec);
    EXPECT_TRUE(ec);
    EXPECT_FALSE(invalid_socket.is_valid());

    EXPECT_THROW(static_cast<void>(tcpocket::SocketHandle::create_stream_socket(-1)), tcpocket::TcpException);
}
