#include <gtest/gtest.h>
#include "tcpocket/tcpocket.hpp"

TEST(TcpocketInitTest, VersionNotEmpty) {
    EXPECT_FALSE(tcpocket::version().empty());
    EXPECT_EQ(tcpocket::version(), "0.1.0");
}
