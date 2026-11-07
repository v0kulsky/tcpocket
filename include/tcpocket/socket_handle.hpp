#pragma once

#include "tcpocket/error.hpp"

#include <sys/socket.h>
#include <system_error>

namespace tcpocket {

class SocketHandle {
public:
    inline static constexpr int invalid_handle = -1;

    SocketHandle() noexcept = default;
    explicit SocketHandle(int fd) noexcept;
    ~SocketHandle();

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    SocketHandle(SocketHandle&& other) noexcept;
    SocketHandle& operator=(SocketHandle&& other) noexcept;

    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
    explicit operator bool() const noexcept;

    int release() noexcept;
    void reset(int new_fd = invalid_handle) noexcept;
    void close() noexcept;

    [[nodiscard]] static SocketHandle create_stream_socket(int domain = AF_INET);
    [[nodiscard]] static SocketHandle create_stream_socket(int domain, std::error_code& ec) noexcept;

private:
    int fd_{invalid_handle};
};

} // namespace tcpocket
