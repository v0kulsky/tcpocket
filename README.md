# tcpocket

A lightweight, modern C++20 TCP networking library.

## Quick Start

### Client

```cpp
#include <tcpocket/tcpocket.hpp>
#include <iostream>
#include <span>
#include <string_view>

int main() {
    auto stream = tcpocket::TcpStream::connect("127.0.0.1", 8080);
    stream.write_all(std::string_view("Hello, tcpocket!\n"));

    char buffer[256];
    size_t n = stream.read(std::span<char>(buffer));
    std::cout.write(buffer, n);
}
```

### Server

```cpp
#include <tcpocket/tcpocket.hpp>
#include <span>
#include <thread>
#include <utility>

int main() {
    auto listener = tcpocket::TcpListener::bind(8080);

    while (true) {
        auto [client, ep] = listener.accept();
        std::thread([stream = std::move(client)]() mutable {
            char buffer[1024];
            while (true) {
                size_t n = stream.read(std::span<char>(buffer));
                if (n == 0) break;
                stream.write_all(std::span<const char>(buffer, n));
            }
        }).detach();
    }
}
```

## Building

Requires a C++20 compatible compiler and CMake (>= 3.20).

```bash
cmake -B build -G Ninja
cmake --build build
```

Or using the Docker build helper:

```bash
./scripts/docker/build.sh
```

## Examples

Pre-built sample applications are available in `examples/`:

```bash
# HTTP GET request
./scripts/docker/example.sh http_get example.com / 80

# Echo Server
./scripts/docker/example.sh echo_server 8080

# Echo Client
./scripts/docker/example.sh echo_client 127.0.0.1 8080 "Hello!"
```

## Testing

Run the test suite with Docker:

```bash
./scripts/docker/test.sh
```
