#include "tcpocket/socket_handle.hpp"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace tcpocket {

SocketHandle::SocketHandle(int fd) noexcept
    : fd_(fd) {}

SocketHandle::~SocketHandle() {
    close();
}

SocketHandle::SocketHandle(SocketHandle&& other) noexcept
    : fd_(other.release()) {}

SocketHandle& SocketHandle::operator=(SocketHandle&& other) noexcept {
    if (this != &other) {
        reset(other.release());
    }
    return *this;
}

int SocketHandle::native_handle() const noexcept {
    return fd_;
}

bool SocketHandle::is_valid() const noexcept {
    return fd_ != invalid_handle;
}

SocketHandle::operator bool() const noexcept {
    return is_valid();
}

int SocketHandle::release() noexcept {
    int old_fd = fd_;
    fd_ = invalid_handle;
    return old_fd;
}

void SocketHandle::reset(int new_fd) noexcept {
    if (fd_ == new_fd) {
        return;
    }
    close();
    fd_ = new_fd;
}

void SocketHandle::close() noexcept {
    if (is_valid()) {
        int fd = fd_;
        fd_ = invalid_handle;
        while (::close(fd) < 0) {
            if (errno != EINTR) {
                break;
            }
        }
    }
}

SocketHandle SocketHandle::create_stream_socket(int domain, std::error_code& ec) noexcept {
    ec.clear();
#if defined(SOCK_CLOEXEC)
    int fd = ::socket(domain, SOCK_STREAM | SOCK_CLOEXEC, 0);
#else
    int fd = ::socket(domain, SOCK_STREAM, 0);
    if (fd >= 0) {
        int flags = ::fcntl(fd, F_GETFD);
        if (flags != -1) {
            ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
        }
    }
#endif
    if (fd < 0) {
        ec = last_socket_error();
        return SocketHandle{};
    }
    return SocketHandle{fd};
}

SocketHandle SocketHandle::create_stream_socket(int domain) {
    std::error_code ec;
    auto socket = create_stream_socket(domain, ec);
    if (ec) {
        throw TcpException(ec, "Failed to create stream socket");
    }
    return socket;
}

} // namespace tcpocket
