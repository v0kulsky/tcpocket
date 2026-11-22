#include <tcpocket/tcpocket.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
    std::string host = "example.com";
    std::string path = "/";
    uint16_t port = 80;

    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        path = argv[2];
    }
    if (argc > 3) {
        port = static_cast<uint16_t>(std::stoi(argv[3]));
    }

    std::cout << "[HTTP GET] Connecting to " << host << ":" << port << "...\n";

    try {
        auto stream = tcpocket::TcpStream::connect(host, port, 5000ms);
        stream.set_timeouts(10000ms, 10000ms);

        std::cout << "[HTTP GET] Connected from local "
                  << stream.local_endpoint().to_string()
                  << " to remote " << stream.remote_endpoint().to_string() << "\n\n";

        // Construct HTTP/1.1 request using C++20 std::format
        std::string request = std::format(
            "GET {} HTTP/1.1\r\n"
            "Host: {}\r\n"
            "User-Agent: tcpocket/0.1.0\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n\r\n",
            path, host);

        // Send full HTTP request
        stream.write_all(std::string_view(request));

        // Read response until EOF (server will close the connection because Connection: close was sent)

        // Read response until EOF
        std::cout << "--- HTTP RESPONSE START ---\n";
        char buffer[4096];
        size_t total_bytes = 0;

        while (true) {
            size_t n = stream.read(std::span<char>(buffer));
            if (n == 0) {
                break; // Server closed connection (clean EOF)
            }
            std::cout.write(buffer, static_cast<std::streamsize>(n));
            total_bytes += n;
        }

        std::cout << "\n--- HTTP RESPONSE END ---\n";
        std::cout << "[HTTP GET] Received " << total_bytes << " bytes total.\n";
    } catch (const tcpocket::TcpException& ex) {
        std::cerr << "[HTTP GET] Network error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
