#include <gtest/gtest.h>
#include "tcpocket/tcp_listener.hpp"
#include "tcpocket/tcp_stream.hpp"
#include "tcpocket/endpoint.hpp"
#include "tcpocket/ip_address.hpp"
#include "tcpocket/error.hpp"

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

TEST(TcpIntegrationTest, SigpipeMitigationOnClosedPeerWrite) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());
    auto server_ep = listener.local_endpoint();

    std::thread server_thread([&listener]() {
        auto [server_stream, client_ep] = listener.accept();
        ASSERT_TRUE(server_stream.is_open());
        // Abruptly close server socket
        server_stream.close();
    });

    auto client = tcpocket::TcpStream::connect(server_ep);
    ASSERT_TRUE(client.is_open());
    server_thread.join();

    // Verify SIGPIPE is not raised and write returns an error code
    std::error_code ec;
    // On Linux, the first write after peer close may succeed or return EPIPE/ECONNRESET,
    // and subsequent writes definitely trigger EPIPE. We loop to ensure write encounters broken pipe.
    for (int i = 0; i < 100; ++i) {
        static_cast<void>(client.write(std::string_view("payload after peer close"), ec));
        if (ec) {
            break;
        }
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_TRUE(ec);
    EXPECT_TRUE(ec == tcpocket::SocketError::BrokenPipe || ec == tcpocket::SocketError::ConnectionReset);
}

TEST(TcpIntegrationTest, GracefulEofDetectionOnPeerClose) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());
    auto server_ep = listener.local_endpoint();

    const std::string message = "final message before close";

    std::thread server_thread([&listener, &message]() {
        auto [server_stream, client_ep] = listener.accept();
        ASSERT_TRUE(server_stream.is_open());
        server_stream.write_all(std::string_view(message));
        // Gracefully close
        server_stream.close();
    });

    auto client = tcpocket::TcpStream::connect(server_ep);
    ASSERT_TRUE(client.is_open());

    char buf[128]{};
    size_t n = client.read(std::span<char>(buf));
    EXPECT_EQ(n, message.size());
    EXPECT_EQ(std::string_view(buf, n), message);

    // Next read must return 0 indicating EOF
    std::error_code ec;
    size_t eof_bytes = client.read(std::span<char>(buf), ec);
    EXPECT_FALSE(ec);
    EXPECT_EQ(eof_bytes, 0u);

    server_thread.join();
}

TEST(TcpIntegrationTest, HalfCloseWriteAllowsBidirectionalFlow) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());
    auto server_ep = listener.local_endpoint();

    std::thread server_thread([&listener]() {
        auto [server_stream, client_ep] = listener.accept();
        ASSERT_TRUE(server_stream.is_open());

        char buf[32]{};
        size_t n = server_stream.read(std::span<char>(buf));
        EXPECT_EQ(std::string_view(buf, n), "request");

        // Client shut down write, so next read must return 0 (EOF)
        size_t eof = server_stream.read(std::span<char>(buf));
        EXPECT_EQ(eof, 0u);

        // Server can still send response back to client
        server_stream.write_all(std::string_view("response after half-close"));
    });

    auto client = tcpocket::TcpStream::connect(server_ep);
    ASSERT_TRUE(client.is_open());

    client.write_all(std::string_view("request"));
    client.shutdown(tcpocket::ShutdownMode::Write);

    char buf[64]{};
    size_t n = client.read(std::span<char>(buf));
    EXPECT_EQ(std::string_view(buf, n), "response after half-close");

    server_thread.join();
}

TEST(TcpIntegrationTest, NonblockingListenerAcceptWouldBlock) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());

    listener.set_nonblocking(true);

    std::error_code ec;
    auto [stream, client_ep] = listener.accept(ec);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::WouldBlock);
    EXPECT_FALSE(stream.is_open());

    EXPECT_THROW(
        static_cast<void>(listener.accept()),
        tcpocket::TcpException
    );

    // Restore blocking mode
    listener.set_nonblocking(false);
}

TEST(TcpIntegrationTest, NonblockingStreamReadWouldBlock) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());
    auto server_ep = listener.local_endpoint();

    std::thread server_thread([&listener]() {
        auto [server_stream, client_ep] = listener.accept();
        ASSERT_TRUE(server_stream.is_open());
        // Sleep without sending data
        std::this_thread::sleep_for(100ms);
    });

    auto client = tcpocket::TcpStream::connect(server_ep);
    ASSERT_TRUE(client.is_open());

    client.set_nonblocking(true);

    std::error_code ec;
    char buf[32]{};
    size_t n = client.read(std::span<char>(buf), ec);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::WouldBlock);
    EXPECT_EQ(n, 0u);

    EXPECT_THROW(
        static_cast<void>(client.read(std::span<char>(buf))),
        tcpocket::TcpException
    );

    client.set_nonblocking(false);
    server_thread.join();
}

TEST(TcpIntegrationTest, SequentialClientsEchoPattern) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());
    auto server_ep = listener.local_endpoint();

    const int client_count = 5;

    std::thread server_thread([&listener, client_count]() {
        for (int i = 0; i < client_count; ++i) {
            auto [server_stream, client_ep] = listener.accept();
            ASSERT_TRUE(server_stream.is_open());

            char buf[64]{};
            size_t n = server_stream.read(std::span<char>(buf));
            ASSERT_GT(n, 0u);

            server_stream.write_all(std::span<const char>(buf, n));
        }
    });

    for (int i = 0; i < client_count; ++i) {
        auto client = tcpocket::TcpStream::connect(server_ep);
        ASSERT_TRUE(client.is_open());

        std::string msg = "hello_client_" + std::to_string(i);
        client.write_all(std::string_view(msg));

        char buf[64]{};
        size_t n = client.read(std::span<char>(buf));
        EXPECT_EQ(std::string_view(buf, n), msg);
    }

    server_thread.join();
}

TEST(TcpIntegrationTest, BidirectionalConcurrentDataTransfer) {
    auto listener = tcpocket::TcpListener::bind(tcpocket::Endpoint::loopback_ipv4(0));
    ASSERT_TRUE(listener.is_listening());
    auto server_ep = listener.local_endpoint();

    const size_t payload_size = 64 * 1024; // 64 KB
    std::vector<char> client_data(payload_size, 'C');
    std::vector<char> server_data(payload_size, 'S');

    std::thread server_thread([&listener, &client_data, &server_data, payload_size]() {
        auto [server_stream, client_ep] = listener.accept();
        ASSERT_TRUE(server_stream.is_open());

        std::vector<char> received(payload_size, '\0');
        server_stream.read_exact(std::span<char>(received));
        EXPECT_EQ(received, client_data);

        server_stream.write_all(std::span<const char>(server_data));
    });

    auto client = tcpocket::TcpStream::connect(server_ep);
    ASSERT_TRUE(client.is_open());

    client.write_all(std::span<const char>(client_data));

    std::vector<char> received(payload_size, '\0');
    client.read_exact(std::span<char>(received));
    EXPECT_EQ(received, server_data);

    server_thread.join();
}
