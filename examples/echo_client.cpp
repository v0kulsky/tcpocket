#include <tcpocket/tcpocket.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 8080;

    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = static_cast<uint16_t>(std::stoi(argv[2]));
    }

    try {
        std::cout << "[Echo Client] Connecting to " << host << ":" << port << "...\n";
        auto stream = tcpocket::TcpStream::connect(host, port, 3000ms);
        stream.set_timeouts(5000ms, 5000ms);

        std::cout << "[Echo Client] Connected from local "
                  << stream.local_endpoint().to_string() << '\n';

        if (argc > 3) {
            // One-shot mode: send single message from command line
            std::string message = argv[3];
            stream.write_all(std::string_view(message));

            char buffer[1024];
            size_t n = stream.read(std::span<char>(buffer));
            std::cout << "[Echo Client] Server replied: "
                      << std::string_view(buffer, n) << '\n';
            return EXIT_SUCCESS;
        }

        // Interactive mode
        std::cout << "[Echo Client] Enter messages (type '/quit' or press Ctrl+D to exit):\n";
        std::string line;
        char buffer[1024];

        while (true) {
            std::cout << "> " << std::flush;
            if (!std::getline(std::cin, line)) {
                break;
            }
            if (line == "/quit") {
                break;
            }
            if (line.empty()) {
                continue;
            }

            stream.write_all(std::string_view(line));

            size_t n = stream.read(std::span<char>(buffer));
            if (n == 0) {
                std::cout << "[Echo Client] Server closed connection.\n";
                break;
            }

            std::cout << "[Echo Client] Echo: " << std::string_view(buffer, n) << '\n';
        }

        std::cout << "[Echo Client] Closing connection...\n";
        stream.close();
    } catch (const tcpocket::TcpException& ex) {
        std::cerr << "[Echo Client] Network error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
