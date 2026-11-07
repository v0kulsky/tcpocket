#pragma once

#include <string_view>

namespace tcpocket {

inline constexpr int version_major = 0;
inline constexpr int version_minor = 1;
inline constexpr int version_patch = 0;
inline constexpr std::string_view version_string = "0.1.0";

[[nodiscard]] constexpr std::string_view version() noexcept {
    return version_string;
}

} // namespace tcpocket
