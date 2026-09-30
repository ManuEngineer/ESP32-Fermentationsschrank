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

[[nodiscard]] std::optional<SoftApCredentials> makeSoftApCredentials(
    device_platform::ISecureRandomSource& randomSource);

}  // namespace fermentation
