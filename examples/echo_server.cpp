#include <tcpocket/tcpocket.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    }

    try {
        auto listener = tcpocket::TcpListener::bind(port);
        std::cout << "[Echo Server] Listening on " << listener.local_endpoint().to_string()
                  << " (port " << port << ")\n";
        std::cout << "[Echo Server] Press Ctrl+C to terminate.\n";

        while (true) {
            auto [stream, client_ep] = listener.accept();
            std::cout << "[Echo Server] Accepted connection from " << client_ep.to_string() << '\n';

            // Spawn worker thread for each connected client
            std::thread([client = std::move(stream), ep = client_ep]() mutable {
                try {
                    char buffer[1024];
                    while (true) {
                        size_t n = client.read(std::span<char>(buffer));
                        if (n == 0) {
                            std::cout << "[Echo Server] Client " << ep.to_string() << " disconnected.\n";
                            break;
                        }

                        // Echo back the received data
                        client.write_all(std::span<const char>(buffer, n));
                    }
                } catch (const tcpocket::TcpException& ex) {
                    std::cerr << "[Echo Server] Client " << ep.to_string()
                              << " error: " << ex.what() << '\n';
                }
            }).detach();
        }
    } catch (const tcpocket::TcpException& ex) {
        std::cerr << "[Echo Server] Fatal error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
