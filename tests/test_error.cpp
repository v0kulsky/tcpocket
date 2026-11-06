#include <gtest/gtest.h>
#include "tcpocket/error.hpp"
#include <cerrno>

TEST(ErrorTest, CategoryProperties) {
    const auto& cat = tcpocket::tcpocket_category();
    EXPECT_STREQ(cat.name(), "tcpocket");

    EXPECT_EQ(cat.message(static_cast<int>(tcpocket::SocketError::Success)), "Success");
    EXPECT_EQ(cat.message(static_cast<int>(tcpocket::SocketError::ConnectionRefused)), "Connection refused");
    EXPECT_EQ(cat.message(static_cast<int>(tcpocket::SocketError::TimedOut)), "Operation timed out");
    EXPECT_EQ(cat.message(static_cast<int>(tcpocket::SocketError::Unknown)), "Unknown socket error");
    EXPECT_EQ(cat.message(99999), "Unknown socket error");
}

TEST(ErrorTest, MakeErrorCodeAndImplicitConversion) {
    std::error_code ec = tcpocket::SocketError::ConnectionReset;
    EXPECT_EQ(ec.category(), tcpocket::tcpocket_category());
    EXPECT_EQ(ec.value(), static_cast<int>(tcpocket::SocketError::ConnectionReset));
    EXPECT_TRUE(ec);

    std::error_code success_ec = tcpocket::SocketError::Success;
    EXPECT_FALSE(success_ec);
}

TEST(ErrorTest, ErrorCodeFromErrno) {
    EXPECT_EQ(tcpocket::error_code_from_errno(0), tcpocket::SocketError::Success);
    EXPECT_EQ(tcpocket::error_code_from_errno(ECONNREFUSED), tcpocket::SocketError::ConnectionRefused);
    EXPECT_EQ(tcpocket::error_code_from_errno(ETIMEDOUT), tcpocket::SocketError::TimedOut);
    EXPECT_EQ(tcpocket::error_code_from_errno(ECONNRESET), tcpocket::SocketError::ConnectionReset);
    EXPECT_EQ(tcpocket::error_code_from_errno(EPIPE), tcpocket::SocketError::BrokenPipe);
    EXPECT_EQ(tcpocket::error_code_from_errno(EAGAIN), tcpocket::SocketError::WouldBlock);
    EXPECT_EQ(tcpocket::error_code_from_errno(EADDRINUSE), tcpocket::SocketError::AddressInUse);
    EXPECT_EQ(tcpocket::error_code_from_errno(EBADF), tcpocket::SocketError::InvalidDescriptor);
}

TEST(ErrorTest, LastSocketError) {
    errno = ECONNREFUSED;
    EXPECT_EQ(tcpocket::last_socket_error(), tcpocket::SocketError::ConnectionRefused);
    errno = 0;
    EXPECT_EQ(tcpocket::last_socket_error(), tcpocket::SocketError::Success);
}

TEST(ErrorTest, TcpExceptionHandling) {
    tcpocket::TcpException ex1(tcpocket::SocketError::TimedOut);
    EXPECT_EQ(ex1.code(), tcpocket::SocketError::TimedOut);

    tcpocket::TcpException ex2(tcpocket::SocketError::ConnectionRefused, "Failed to connect");
    EXPECT_EQ(ex2.code(), tcpocket::SocketError::ConnectionRefused);
    std::string what = ex2.what();
    EXPECT_NE(what.find("Failed to connect"), std::string::npos);
    EXPECT_NE(what.find("Connection refused"), std::string::npos);
}
