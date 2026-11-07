#include "tcpocket/error.hpp"

#include <cerrno>
#include <string>

namespace tcpocket {

namespace {

class SocketErrorCategory : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override {
        return "tcpocket";
    }

    [[nodiscard]] std::string message(int ev) const override {
        switch (static_cast<SocketError>(ev)) {
            case SocketError::Success:
                return "Success";
            case SocketError::ConnectionRefused:
                return "Connection refused";
            case SocketError::TimedOut:
                return "Operation timed out";
            case SocketError::ConnectionReset:
                return "Connection reset by peer";
            case SocketError::BrokenPipe:
                return "Broken pipe";
            case SocketError::WouldBlock:
                return "Operation would block";
            case SocketError::NetworkUnreachable:
                return "Network unreachable";
            case SocketError::HostUnreachable:
                return "Host unreachable";
            case SocketError::AddressInUse:
                return "Address already in use";
            case SocketError::AddressNotAvailable:
                return "Address not available";
            case SocketError::InvalidDescriptor:
                return "Invalid socket descriptor";
            case SocketError::InvalidArgument:
                return "Invalid argument";
            case SocketError::OperationNotSupported:
                return "Operation not supported";
            case SocketError::Unknown:
            default:
                return "Unknown socket error";
        }
    }
};

const SocketErrorCategory socket_error_category_instance{};

} // namespace

const std::error_category& tcpocket_category() noexcept {
    return socket_error_category_instance;
}

std::error_code make_error_code(SocketError e) noexcept {
    return {static_cast<int>(e), tcpocket_category()};
}

std::error_code error_code_from_errno(int err) noexcept {
    switch (err) {
        case 0:
            return SocketError::Success;
        case ECONNREFUSED:
            return SocketError::ConnectionRefused;
        case ETIMEDOUT:
            return SocketError::TimedOut;
        case ECONNRESET:
            return SocketError::ConnectionReset;
        case EPIPE:
            return SocketError::BrokenPipe;
        case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
        case EWOULDBLOCK:
#endif
            return SocketError::WouldBlock;
        case ENETUNREACH:
            return SocketError::NetworkUnreachable;
        case EHOSTUNREACH:
            return SocketError::HostUnreachable;
        case EADDRINUSE:
            return SocketError::AddressInUse;
        case EADDRNOTAVAIL:
            return SocketError::AddressNotAvailable;
        case EBADF:
            return SocketError::InvalidDescriptor;
        case EINVAL:
            return SocketError::InvalidArgument;
        case EOPNOTSUPP:
#if defined(ENOTSUP) && (ENOTSUP != EOPNOTSUPP)
        case ENOTSUP:
#endif
        case EAFNOSUPPORT:
            return SocketError::OperationNotSupported;
        default:
            return SocketError::Unknown;
    }
}

std::error_code last_socket_error() noexcept {
    return error_code_from_errno(errno);
}

} // namespace tcpocket
