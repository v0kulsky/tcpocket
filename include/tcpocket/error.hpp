#pragma once

#include <cerrno>
#include <string>
#include <string_view>
#include <system_error>

namespace tcpocket {

enum class SocketError {
    Success = 0,
    ConnectionRefused,
    TimedOut,
    ConnectionReset,
    BrokenPipe,
    WouldBlock,
    NetworkUnreachable,
    HostUnreachable,
    AddressInUse,
    AddressNotAvailable,
    InvalidDescriptor,
    InvalidArgument,
    OperationNotSupported,
    Unknown
};

[[nodiscard]] const std::error_category& tcpocket_category() noexcept;
[[nodiscard]] std::error_code make_error_code(SocketError e) noexcept;
[[nodiscard]] std::error_code error_code_from_errno(int err) noexcept;
[[nodiscard]] std::error_code last_socket_error() noexcept;

class TcpException : public std::system_error {
public:
    explicit TcpException(std::error_code ec)
        : std::system_error(ec) {}

    TcpException(std::error_code ec, std::string_view message)
        : std::system_error(ec, std::string(message)) {}

    TcpException(std::string_view message, std::error_code ec)
        : std::system_error(ec, std::string(message)) {}

    explicit TcpException(SocketError err)
        : std::system_error(make_error_code(err)) {}

    TcpException(SocketError err, std::string_view message)
        : std::system_error(make_error_code(err), std::string(message)) {}

    TcpException(std::string_view message, SocketError err)
        : std::system_error(make_error_code(err), std::string(message)) {}
};

} // namespace tcpocket

template <>
struct std::is_error_code_enum<tcpocket::SocketError> : std::true_type {};
