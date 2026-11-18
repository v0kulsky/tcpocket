#include <gtest/gtest.h>
#include "tcpocket/tcp_stream.hpp"
#include "tcpocket/socket_handle.hpp"
#include "tcpocket/socket_options.hpp"
#include "tcpocket/endpoint.hpp"
#include "tcpocket/ip_address.hpp"
#include "tcpocket/error.hpp"

#include <chrono>
#include <cstddef>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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

namespace {

class LoopbackServer {
public:
    explicit LoopbackServer(bool ipv6 = false) {
        int domain = ipv6 ? AF_INET6 : AF_INET;
        listen_socket_ = tcpocket::SocketHandle::create_stream_socket(domain);
        if (!listen_socket_.is_valid()) {
            throw tcpocket::TcpException(tcpocket::last_socket_error(), "Failed to create test server socket");
        }

        tcpocket::set_reuse_address(listen_socket_, true);

        auto ep = ipv6 ? tcpocket::Endpoint::loopback_ipv6(0) : tcpocket::Endpoint::loopback_ipv4(0);
        sockaddr_storage storage{};
        socklen_t len = ep.to_sockaddr(storage);

        if (::bind(listen_socket_.native_handle(), reinterpret_cast<const sockaddr*>(&storage), len) < 0) {
            throw tcpocket::TcpException(tcpocket::last_socket_error(), "Failed to bind test server");
        }

        if (::listen(listen_socket_.native_handle(), 16) < 0) {
            throw tcpocket::TcpException(tcpocket::last_socket_error(), "Failed to listen on test server");
        }

        sockaddr_storage bound_addr{};
        socklen_t bound_len = sizeof(bound_addr);
        if (::getsockname(listen_socket_.native_handle(), reinterpret_cast<sockaddr*>(&bound_addr), &bound_len) < 0) {
            throw tcpocket::TcpException(tcpocket::last_socket_error(), "Failed to getsockname on test server");
        }
        endpoint_ = tcpocket::Endpoint::from_sockaddr(reinterpret_cast<const sockaddr*>(&bound_addr), bound_len);
    }

    [[nodiscard]] const tcpocket::Endpoint& endpoint() const noexcept {
        return endpoint_;
    }

    [[nodiscard]] uint16_t port() const noexcept {
        return endpoint_.port();
    }

    [[nodiscard]] int native_handle() const noexcept {
        return listen_socket_.native_handle();
    }

    [[nodiscard]] tcpocket::SocketHandle accept_client(sockaddr_storage* client_addr = nullptr) {
        sockaddr_storage ss{};
        socklen_t slen = sizeof(ss);
        int client_fd = ::accept4(listen_socket_.native_handle(), reinterpret_cast<sockaddr*>(&ss), &slen, SOCK_CLOEXEC);
        if (client_fd < 0) {
            throw tcpocket::TcpException(tcpocket::last_socket_error(), "Failed to accept client");
        }
        if (client_addr) {
            *client_addr = ss;
        }
        return tcpocket::SocketHandle(client_fd);
    }

private:
    tcpocket::SocketHandle listen_socket_;
    tcpocket::Endpoint endpoint_;
};

std::pair<tcpocket::SocketHandle, tcpocket::SocketHandle> create_connected_socket_pair() {
    int sv[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0) {
        throw tcpocket::TcpException(tcpocket::last_socket_error(), "Failed to create socketpair");
    }
    return {tcpocket::SocketHandle(sv[0]), tcpocket::SocketHandle(sv[1])};
}

} // namespace

TEST(TcpStreamTest, TypeTraits) {
    static_assert(!std::is_copy_constructible_v<tcpocket::TcpStream>);
    static_assert(!std::is_copy_assignable_v<tcpocket::TcpStream>);
    static_assert(std::is_move_constructible_v<tcpocket::TcpStream>);
    static_assert(std::is_move_assignable_v<tcpocket::TcpStream>);
    static_assert(std::is_nothrow_move_constructible_v<tcpocket::TcpStream>);
    static_assert(std::is_nothrow_move_assignable_v<tcpocket::TcpStream>);

    EXPECT_EQ(static_cast<int>(tcpocket::ShutdownMode::Read), SHUT_RD);
    EXPECT_EQ(static_cast<int>(tcpocket::ShutdownMode::Write), SHUT_WR);
    EXPECT_EQ(static_cast<int>(tcpocket::ShutdownMode::Both), SHUT_RDWR);
}

TEST(TcpStreamTest, DefaultConstructor) {
    tcpocket::TcpStream stream;
    EXPECT_FALSE(stream.is_open());
    EXPECT_FALSE(static_cast<bool>(stream));
    EXPECT_EQ(stream.native_handle(), tcpocket::SocketHandle::invalid_handle);
    EXPECT_FALSE(stream.socket_handle().is_valid());
    EXPECT_EQ(stream.local_endpoint(), tcpocket::Endpoint{});
    EXPECT_EQ(stream.remote_endpoint(), tcpocket::Endpoint{});
}

TEST(TcpStreamTest, ExplicitConstructorWithSocketHandle) {
    auto [s1, s2] = create_connected_socket_pair();
    int raw_fd = s1.native_handle();

    auto local_ep = tcpocket::Endpoint::loopback_ipv4(12345);
    auto remote_ep = tcpocket::Endpoint::loopback_ipv4(8080);

    tcpocket::TcpStream stream(std::move(s1), remote_ep, local_ep);
    EXPECT_TRUE(stream.is_open());
    EXPECT_TRUE(static_cast<bool>(stream));
    EXPECT_EQ(stream.native_handle(), raw_fd);
    EXPECT_TRUE(stream.socket_handle().is_valid());
    EXPECT_EQ(stream.socket_handle().native_handle(), raw_fd);
    EXPECT_EQ(stream.local_endpoint(), local_ep);
    EXPECT_EQ(stream.remote_endpoint(), remote_ep);
}

TEST(TcpStreamTest, MoveConstruction) {
    auto [s1, s2] = create_connected_socket_pair();
    int raw_fd = s1.native_handle();

    auto local_ep = tcpocket::Endpoint::loopback_ipv4(12345);
    auto remote_ep = tcpocket::Endpoint::loopback_ipv4(8080);

    tcpocket::TcpStream stream1(std::move(s1), remote_ep, local_ep);
    tcpocket::TcpStream stream2(std::move(stream1));

    EXPECT_TRUE(stream2.is_open());
    EXPECT_EQ(stream2.native_handle(), raw_fd);
    EXPECT_EQ(stream2.local_endpoint(), local_ep);
    EXPECT_EQ(stream2.remote_endpoint(), remote_ep);

    EXPECT_FALSE(stream1.is_open());
    EXPECT_EQ(stream1.native_handle(), tcpocket::SocketHandle::invalid_handle);
}

TEST(TcpStreamTest, MoveAssignment) {
    auto [s1, peer1] = create_connected_socket_pair();
    auto [s2, peer2] = create_connected_socket_pair();

    int fd1 = s1.native_handle();
    int fd2 = s2.native_handle();

    auto ep1 = tcpocket::Endpoint::loopback_ipv4(1111);
    auto ep2 = tcpocket::Endpoint::loopback_ipv4(2222);

    tcpocket::TcpStream stream1(std::move(s1), ep1, ep1);
    tcpocket::TcpStream stream2(std::move(s2), ep2, ep2);

    stream2 = std::move(stream1);

    EXPECT_TRUE(stream2.is_open());
    EXPECT_EQ(stream2.native_handle(), fd1);
    EXPECT_EQ(stream2.remote_endpoint(), ep1);

    EXPECT_EQ(::fcntl(fd2, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);

    EXPECT_FALSE(stream1.is_open());
    EXPECT_EQ(stream1.native_handle(), tcpocket::SocketHandle::invalid_handle);
}

TEST(TcpStreamTest, MoveSelfAssignmentIsSafe) {
    auto [s1, peer1] = create_connected_socket_pair();
    int fd = s1.native_handle();

    auto ep = tcpocket::Endpoint::loopback_ipv4(8080);
    tcpocket::TcpStream stream(std::move(s1), ep, ep);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
    stream = std::move(stream);
#pragma GCC diagnostic pop

    EXPECT_TRUE(stream.is_open());
    EXPECT_EQ(stream.native_handle(), fd);
    EXPECT_NE(::fcntl(fd, F_GETFD), -1);
}

TEST(TcpStreamTest, ConnectToEndpointSuccess) {
    LoopbackServer server;

    std::thread server_thread([&server]() {
        auto client = server.accept_client();
        EXPECT_TRUE(client.is_valid());
    });

    auto client_stream = tcpocket::TcpStream::connect(server.endpoint());
    EXPECT_TRUE(client_stream.is_open());
    EXPECT_EQ(client_stream.remote_endpoint(), server.endpoint());
    EXPECT_TRUE(client_stream.local_endpoint().address().is_loopback());
    EXPECT_NE(client_stream.local_endpoint().port(), 0);

    server_thread.join();
}

TEST(TcpStreamTest, ConnectToEndpointNonThrowingSuccess) {
    LoopbackServer server;

    std::thread server_thread([&server]() {
        auto client = server.accept_client();
        EXPECT_TRUE(client.is_valid());
    });

    std::error_code ec;
    auto client_stream = tcpocket::TcpStream::connect(server.endpoint(), ec);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(client_stream.is_open());
    EXPECT_EQ(client_stream.remote_endpoint(), server.endpoint());

    server_thread.join();
}

TEST(TcpStreamTest, ConnectWithHostnameAndPortSuccess) {
    LoopbackServer server;

    std::thread server_thread([&server]() {
        auto client = server.accept_client();
        EXPECT_TRUE(client.is_valid());
    });

    auto client_stream = tcpocket::TcpStream::connect("127.0.0.1", server.port());
    EXPECT_TRUE(client_stream.is_open());
    EXPECT_EQ(client_stream.remote_endpoint().port(), server.port());
    EXPECT_EQ(client_stream.remote_endpoint().address(), tcpocket::IpAddress::loopback_ipv4());

    server_thread.join();
}

TEST(TcpStreamTest, ConnectWithLocalhostResolution) {
    LoopbackServer server;

    std::thread server_thread([&server]() {
        auto client = server.accept_client();
        EXPECT_TRUE(client.is_valid());
    });

    std::error_code ec;
    auto client_stream = tcpocket::TcpStream::connect("localhost", server.port(), ec);
    EXPECT_FALSE(ec);
    EXPECT_TRUE(client_stream.is_open());
    EXPECT_EQ(client_stream.remote_endpoint().port(), server.port());

    server_thread.join();
}

TEST(TcpStreamTest, ConnectWithTimeoutSuccess) {
    LoopbackServer server;

    std::thread server_thread([&server]() {
        auto client = server.accept_client();
        EXPECT_TRUE(client.is_valid());
    });

    auto client_stream = tcpocket::TcpStream::connect(server.endpoint(), 2000ms);
    EXPECT_TRUE(client_stream.is_open());
    EXPECT_EQ(client_stream.remote_endpoint(), server.endpoint());

    server_thread.join();
}

TEST(TcpStreamTest, ConnectToNonExistentPortThrows) {
    auto dead_endpoint = tcpocket::Endpoint::loopback_ipv4(54321);

    EXPECT_THROW({
        auto stream = tcpocket::TcpStream::connect(dead_endpoint);
    }, tcpocket::TcpException);

    try {
        auto stream = tcpocket::TcpStream::connect(dead_endpoint);
        FAIL() << "Expected TcpException on refused connection";
    } catch (const tcpocket::TcpException& ex) {
        EXPECT_EQ(ex.code(), tcpocket::SocketError::ConnectionRefused);
    }
}

TEST(TcpStreamTest, ConnectToNonExistentPortNonThrowing) {
    auto dead_endpoint = tcpocket::Endpoint::loopback_ipv4(54322);

    std::error_code ec;
    auto stream = tcpocket::TcpStream::connect(dead_endpoint, ec);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::ConnectionRefused);
    EXPECT_FALSE(stream.is_open());
}

TEST(TcpStreamTest, ConnectToNonRoutableWithTimeoutTimesOut) {
    auto unreachable = tcpocket::Endpoint(tcpocket::IpAddress::from_string("192.0.2.1"), 80);

    auto start = std::chrono::steady_clock::now();
    std::error_code ec;
    auto stream = tcpocket::TcpStream::connect(unreachable, ec, 150ms);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    EXPECT_TRUE(ec);
    EXPECT_TRUE(ec == tcpocket::SocketError::TimedOut || ec == tcpocket::SocketError::NetworkUnreachable ||
                ec == tcpocket::SocketError::HostUnreachable);
    EXPECT_FALSE(stream.is_open());
    EXPECT_GE(elapsed.count(), 100);
}

TEST(TcpStreamTest, ConnectInvalidHostFails) {
    std::error_code ec;
    auto stream = tcpocket::TcpStream::connect("invalid.nonexistent.domain.test", 80, ec);
    EXPECT_TRUE(ec);
    EXPECT_FALSE(stream.is_open());

    EXPECT_THROW({
        tcpocket::TcpStream::connect("invalid.nonexistent.domain.test", 80);
    }, tcpocket::TcpException);
}

TEST(TcpStreamTest, ReadWriteCharSpanAndStringView) {
    LoopbackServer server;
    const std::string test_msg = "Hello TCPocket Stream!";

    std::thread server_thread([&server, &test_msg]() {
        auto client = server.accept_client();
        ASSERT_TRUE(client.is_valid());

        char buf[128]{};
        ssize_t n = ::recv(client.native_handle(), buf, sizeof(buf), 0);
        ASSERT_EQ(n, static_cast<ssize_t>(test_msg.size()));
        EXPECT_EQ(std::string_view(buf, n), test_msg);

        std::string reply = "Echo: " + test_msg;
        ::send(client.native_handle(), reply.data(), reply.size(), 0);
    });

    auto client = tcpocket::TcpStream::connect(server.endpoint());
    ASSERT_TRUE(client.is_open());

    size_t written = client.write(std::string_view(test_msg));
    EXPECT_EQ(written, test_msg.size());

    char recv_buf[128]{};
    size_t bytes_read = client.read(std::span<char>(recv_buf));
    std::string expected_reply = "Echo: " + test_msg;
    EXPECT_EQ(bytes_read, expected_reply.size());
    EXPECT_EQ(std::string_view(recv_buf, bytes_read), expected_reply);

    server_thread.join();
}

TEST(TcpStreamTest, ReadWriteNonThrowingOverloads) {
    LoopbackServer server;
    const std::string msg = "Non-throwing write test";

    std::thread server_thread([&server, &msg]() {
        auto client = server.accept_client();
        ASSERT_TRUE(client.is_valid());

        char buf[64]{};
        ssize_t n = ::recv(client.native_handle(), buf, sizeof(buf), 0);
        EXPECT_EQ(n, static_cast<ssize_t>(msg.size()));

        ::send(client.native_handle(), msg.data(), msg.size(), 0);
    });

    std::error_code ec;
    auto client = tcpocket::TcpStream::connect(server.endpoint(), ec);
    ASSERT_FALSE(ec);

    size_t written = client.write(std::span<const char>(msg.data(), msg.size()), ec);
    EXPECT_FALSE(ec);
    EXPECT_EQ(written, msg.size());

    char recv_buf[64]{};
    size_t bytes_read = client.read(std::span<char>(recv_buf), ec);
    EXPECT_FALSE(ec);
    EXPECT_EQ(bytes_read, msg.size());
    EXPECT_EQ(std::string_view(recv_buf, bytes_read), msg);

    server_thread.join();
}

TEST(TcpStreamTest, ReadWriteByteSpan) {
    LoopbackServer server;
    std::vector<std::byte> payload = {
        std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF},
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}
    };

    std::thread server_thread([&server, &payload]() {
        auto client = server.accept_client();
        ASSERT_TRUE(client.is_valid());

        std::vector<uint8_t> buf(payload.size());
        ssize_t n = ::recv(client.native_handle(), buf.data(), buf.size(), 0);
        ASSERT_EQ(n, static_cast<ssize_t>(payload.size()));

        for (auto& b : buf) {
            b ^= 0xFF;
        }
        ::send(client.native_handle(), buf.data(), buf.size(), 0);
    });

    auto client = tcpocket::TcpStream::connect(server.endpoint());
    ASSERT_TRUE(client.is_open());

    size_t written = client.write(std::span<const std::byte>(payload));
    EXPECT_EQ(written, payload.size());

    std::vector<std::byte> response(payload.size());
    size_t read_bytes = client.read(std::span<std::byte>(response));
    EXPECT_EQ(read_bytes, payload.size());

    for (size_t i = 0; i < payload.size(); ++i) {
        EXPECT_EQ(static_cast<uint8_t>(response[i]), static_cast<uint8_t>(payload[i]) ^ 0xFF);
    }

    server_thread.join();
}

TEST(TcpStreamTest, WriteAllAndReadExact) {
    LoopbackServer server;
    std::string large_string(8192, 'A');
    for (size_t i = 0; i < large_string.size(); ++i) {
        large_string[i] = static_cast<char>('A' + (i % 26));
    }

    std::thread server_thread([&server, &large_string]() {
        auto client = server.accept_client();
        ASSERT_TRUE(client.is_valid());

        std::vector<char> buf(large_string.size());
        size_t total = 0;
        while (total < large_string.size()) {
            ssize_t n = ::recv(client.native_handle(), buf.data() + total, large_string.size() - total, 0);
            ASSERT_GT(n, 0);
            total += static_cast<size_t>(n);
        }

        total = 0;
        while (total < large_string.size()) {
            ssize_t n = ::send(client.native_handle(), buf.data() + total, large_string.size() - total, MSG_NOSIGNAL);
            ASSERT_GT(n, 0);
            total += static_cast<size_t>(n);
        }
    });

    auto client = tcpocket::TcpStream::connect(server.endpoint());
    ASSERT_TRUE(client.is_open());

    client.write_all(std::string_view(large_string));

    std::vector<char> recv_buf(large_string.size(), '\0');
    client.read_exact(std::span<char>(recv_buf));
    EXPECT_EQ(std::string_view(recv_buf.data(), recv_buf.size()), large_string);

    server_thread.join();
}

TEST(TcpStreamTest, ReadExactPrematureEofThrowsOrSetsError) {
    LoopbackServer server;

    std::thread server_thread([&server]() {
        auto client = server.accept_client();
        ASSERT_TRUE(client.is_valid());
        ::send(client.native_handle(), "12345", 5, 0);
        client.close();
    });

    auto client = tcpocket::TcpStream::connect(server.endpoint());
    ASSERT_TRUE(client.is_open());

    char buf[10];
    EXPECT_THROW({
        client.read_exact(std::span<char>(buf));
    }, tcpocket::TcpException);

    server_thread.join();

    LoopbackServer server2;
    std::thread server_thread2([&server2]() {
        auto client = server2.accept_client();
        ASSERT_TRUE(client.is_valid());
        ::send(client.native_handle(), "abc", 3, 0);
        client.close();
    });

    auto client2 = tcpocket::TcpStream::connect(server2.endpoint());
    std::error_code ec;
    client2.read_exact(std::span<char>(buf), ec);
    EXPECT_TRUE(ec);
    EXPECT_TRUE(ec == tcpocket::SocketError::ConnectionReset || ec == tcpocket::SocketError::BrokenPipe);

    server_thread2.join();
}

TEST(TcpStreamTest, ReadEofReturnsZero) {
    LoopbackServer server;

    std::thread server_thread([&server]() {
        auto client = server.accept_client();
        ASSERT_TRUE(client.is_valid());
        client.close();
    });

    auto client = tcpocket::TcpStream::connect(server.endpoint());
    ASSERT_TRUE(client.is_open());

    char buf[64];
    size_t n = client.read(std::span<char>(buf));
    EXPECT_EQ(n, 0u);

    std::error_code ec;
    size_t n2 = client.read(std::span<char>(buf), ec);
    EXPECT_FALSE(ec);
    EXPECT_EQ(n2, 0u);

    server_thread.join();
}

TEST(TcpStreamTest, LargePayloadTransfer) {
    LoopbackServer server;
    const size_t total_size = 512 * 1024;
    std::vector<std::byte> sent_data(total_size);
    for (size_t i = 0; i < total_size; ++i) {
        sent_data[i] = static_cast<std::byte>(i & 0xFF);
    }

    std::thread server_thread([&server, total_size]() {
        auto client = server.accept_client();
        ASSERT_TRUE(client.is_valid());

        std::vector<std::byte> buffer(total_size);
        size_t received = 0;
        while (received < total_size) {
            ssize_t n = ::recv(client.native_handle(), buffer.data() + received, total_size - received, 0);
            ASSERT_GT(n, 0);
            received += static_cast<size_t>(n);
        }

        size_t sent = 0;
        while (sent < total_size) {
            ssize_t n = ::send(client.native_handle(), buffer.data() + sent, total_size - sent, MSG_NOSIGNAL);
            ASSERT_GT(n, 0);
            sent += static_cast<size_t>(n);
        }
    });

    auto client = tcpocket::TcpStream::connect(server.endpoint());
    ASSERT_TRUE(client.is_open());

    client.write_all(std::span<const std::byte>(sent_data));

    std::vector<std::byte> received_data(total_size);
    client.read_exact(std::span<std::byte>(received_data));

    EXPECT_EQ(sent_data, received_data);

    server_thread.join();
}

TEST(TcpStreamTest, ShutdownWriteSendsEofToPeer) {
    LoopbackServer server;

    std::thread server_thread([&server]() {
        auto client = server.accept_client();
        ASSERT_TRUE(client.is_valid());

        char buf[32];
        ssize_t n = ::recv(client.native_handle(), buf, sizeof(buf), 0);
        EXPECT_EQ(n, 4);
        EXPECT_EQ(std::string_view(buf, 4), "ping");

        n = ::recv(client.native_handle(), buf, sizeof(buf), 0);
        EXPECT_EQ(n, 0);

        ::send(client.native_handle(), "pong", 4, 0);
    });

    auto client = tcpocket::TcpStream::connect(server.endpoint());
    ASSERT_TRUE(client.is_open());

    client.write_all(std::string_view("ping"));
    client.shutdown(tcpocket::ShutdownMode::Write);

    char recv_buf[32];
    size_t bytes = client.read(std::span<char>(recv_buf));
    EXPECT_EQ(bytes, 4u);
    EXPECT_EQ(std::string_view(recv_buf, bytes), "pong");

    server_thread.join();
}

TEST(TcpStreamTest, ShutdownBothAndNonThrowing) {
    auto [s1, s2] = create_connected_socket_pair();

    auto ep = tcpocket::Endpoint::loopback_ipv4(9090);
    tcpocket::TcpStream stream(std::move(s1), ep, ep);

    std::error_code ec;
    stream.shutdown(tcpocket::ShutdownMode::Both, ec);
    EXPECT_FALSE(ec);

    char buf[16];
    size_t n = stream.read(std::span<char>(buf), ec);
    EXPECT_EQ(n, 0u);
}

TEST(TcpStreamTest, ExplicitClose) {
    auto [s1, s2] = create_connected_socket_pair();

    auto ep = tcpocket::Endpoint::loopback_ipv4(9090);
    tcpocket::TcpStream stream(std::move(s1), ep, ep);

    EXPECT_TRUE(stream.is_open());
    stream.close();
    EXPECT_FALSE(stream.is_open());
    EXPECT_EQ(stream.native_handle(), tcpocket::SocketHandle::invalid_handle);

    EXPECT_NO_THROW(stream.close());

    std::error_code ec;
    char buf[16];
    size_t n = stream.read(std::span<char>(buf), ec);
    EXPECT_TRUE(ec);
    EXPECT_EQ(n, 0u);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);
}

TEST(TcpStreamTest, SetReadTimeoutTriggersTimeoutError) {
    LoopbackServer server;

    std::thread server_thread([&server]() {
        auto client = server.accept_client();
        ASSERT_TRUE(client.is_valid());
        std::this_thread::sleep_for(250ms);
    });

    auto client = tcpocket::TcpStream::connect(server.endpoint());
    ASSERT_TRUE(client.is_open());

    client.set_read_timeout(50ms);

    char buf[32];
    EXPECT_THROW(
        static_cast<void>(client.read(std::span<char>(buf))),
        tcpocket::TcpException
    );

    try {
        static_cast<void>(client.read(std::span<char>(buf)));
    } catch (const tcpocket::TcpException& ex) {
        EXPECT_EQ(ex.code(), tcpocket::SocketError::TimedOut);
    }

    std::error_code ec;
    static_cast<void>(client.read(std::span<char>(buf), ec));
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec, tcpocket::SocketError::TimedOut);

    server_thread.join();
}

TEST(TcpStreamTest, SetTimeoutsAndSocketTuning) {
    LoopbackServer server;

    std::thread server_thread([&server]() {
        auto client = server.accept_client();
        ASSERT_TRUE(client.is_valid());
    });

    auto client = tcpocket::TcpStream::connect(server.endpoint());
    ASSERT_TRUE(client.is_open());

    std::error_code ec;
    client.set_timeouts(100ms, 200ms, ec);
    EXPECT_FALSE(ec);

    client.set_nodelay(true, ec);
    EXPECT_FALSE(ec);

    int nodelay = 0;
    socklen_t len = sizeof(nodelay);
    ASSERT_EQ(::getsockopt(client.native_handle(), IPPROTO_TCP, TCP_NODELAY, &nodelay, &len), 0);
    EXPECT_NE(nodelay, 0);

    client.set_nodelay(false);
    ASSERT_EQ(::getsockopt(client.native_handle(), IPPROTO_TCP, TCP_NODELAY, &nodelay, &len), 0);
    EXPECT_EQ(nodelay, 0);

    client.set_nonblocking(true, ec);
    EXPECT_FALSE(ec);
    int flags = ::fcntl(client.native_handle(), F_GETFL, 0);
    EXPECT_NE(flags & O_NONBLOCK, 0);

    client.set_nonblocking(false);
    flags = ::fcntl(client.native_handle(), F_GETFL, 0);
    EXPECT_EQ(flags & O_NONBLOCK, 0);

    server_thread.join();
}

TEST(TcpStreamTest, OperationsOnClosedStreamReturnInvalidDescriptor) {
    tcpocket::TcpStream stream;
    ASSERT_FALSE(stream.is_open());

    std::error_code ec;
    char buf[16];

    EXPECT_EQ(stream.read(std::span<char>(buf), ec), 0u);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    EXPECT_EQ(stream.write(std::string_view("test"), ec), 0u);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    stream.shutdown(tcpocket::ShutdownMode::Both, ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    stream.set_nodelay(true, ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    stream.set_read_timeout(100ms, ec);
    EXPECT_EQ(ec, tcpocket::SocketError::InvalidDescriptor);

    EXPECT_THROW(
        static_cast<void>(stream.read(std::span<char>(buf))),
        tcpocket::TcpException
    );
    EXPECT_THROW(
        static_cast<void>(stream.write(std::string_view("test"))),
        tcpocket::TcpException
    );
    EXPECT_THROW(stream.shutdown(tcpocket::ShutdownMode::Both), tcpocket::TcpException);
    EXPECT_THROW(stream.set_nodelay(true), tcpocket::TcpException);
}
