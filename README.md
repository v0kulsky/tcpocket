# tcpocket

A modern C++20 library for handling TCP connections.

## Requirements

- **C++ Compiler:** `g++` (with C++20 support)
- **Build System:** `CMake` (>= 3.20) and `Ninja`
- **Testing:** GoogleTest (automatically retrieved via CMake `FetchContent` if not installed)

## Building

```bash
# Configure with Ninja and g++
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Debug

# Build library, app, and tests
cmake --build build
```

## Running

```bash
# Run application
./build/tcpocket_app

# Run unit tests
ctest --test-dir build --output-on-failure
# Or directly:
./build/unit_tests
```
