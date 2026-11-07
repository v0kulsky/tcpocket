#include <gtest/gtest.h>
#include "tcpocket/tcpocket.hpp"

TEST(VersionTest, VersionInfo) {
    EXPECT_FALSE(tcpocket::version().empty());
    EXPECT_EQ(tcpocket::version(), "0.1.0");
    EXPECT_EQ(tcpocket::version_string, "0.1.0");
    EXPECT_EQ(tcpocket::version_major, 0);
    EXPECT_EQ(tcpocket::version_minor, 1);
    EXPECT_EQ(tcpocket::version_patch, 0);
}
