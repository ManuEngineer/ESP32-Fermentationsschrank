#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "secure_random_source.hpp"

namespace fermentation {

struct SoftApCredentials {
    std::string ssid;
    std::string password;
};

inline constexpr std::string_view kSoftApPasswordAlphabet{
    "ACDEFHJKMNPQRTUVWXYacdefhjkmnpqrtuvwxy3479"};
inline constexpr std::size_t kSoftApPasswordLength = 16U;
inline constexpr std::size_t kSoftApRawSsidMaximumBytes = 24U;
inline constexpr std::size_t kSoftApEscapedSsidMaximumBytes = 28U;

[[nodiscard]] bool isValidSoftApPassword(std::string_view password) noexcept;

[[nodiscard]] std::optional<std::string> deriveSoftApSsid(
    std::string_view deviceName);

[[nodiscard]] std::optional<SoftApCredentials> makeSoftApCredentials(
    device_platform::ISecureRandomSource& randomSource,
    std::string_view deviceName);

}  // namespace fermentation
