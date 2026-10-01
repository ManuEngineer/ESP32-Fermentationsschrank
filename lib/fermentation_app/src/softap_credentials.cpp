#include "softap_credentials.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace fermentation {
namespace {

constexpr std::uint8_t kRejectionLimit =
    static_cast<std::uint8_t>(256U - (256U % kSoftApPasswordAlphabet.size()));

[[nodiscard]] bool isContinuation(std::uint8_t value) noexcept {
    return (value & 0xC0U) == 0x80U;
}

[[nodiscard]] std::optional<std::size_t> codePointLength(
    std::string_view value, std::size_t offset) noexcept {
    const auto first = static_cast<std::uint8_t>(value[offset]);
    if (first <= 0x7FU) {
        return 1U;
    }
    if (first >= 0xC2U && first <= 0xDFU) {
        if (offset + 1U >= value.size() ||
            !isContinuation(static_cast<std::uint8_t>(value[offset + 1U]))) {
            return std::nullopt;
        }
        return 2U;
    }
    if (first >= 0xE0U && first <= 0xEFU) {
        if (offset + 2U >= value.size()) {
            return std::nullopt;
        }
        const auto second = static_cast<std::uint8_t>(value[offset + 1U]);
        const auto third = static_cast<std::uint8_t>(value[offset + 2U]);
        if (!isContinuation(third) ||
            (first == 0xE0U && (second < 0xA0U || second > 0xBFU)) ||
            (first == 0xEDU && (second < 0x80U || second > 0x9FU)) ||
            (first != 0xE0U && first != 0xEDU && !isContinuation(second))) {
            return std::nullopt;
        }
        return 3U;
    }
    if (first >= 0xF0U && first <= 0xF4U) {
        if (offset + 3U >= value.size()) {
            return std::nullopt;
        }
        const auto second = static_cast<std::uint8_t>(value[offset + 1U]);
        const auto third = static_cast<std::uint8_t>(value[offset + 2U]);
        const auto fourth = static_cast<std::uint8_t>(value[offset + 3U]);
        if (!isContinuation(third) || !isContinuation(fourth) ||
            (first == 0xF0U && (second < 0x90U || second > 0xBFU)) ||
            (first == 0xF4U && (second < 0x80U || second > 0x8FU)) ||
            (first != 0xF0U && first != 0xF4U && !isContinuation(second))) {
            return std::nullopt;
        }
        return 4U;
    }
    return std::nullopt;
}

[[nodiscard]] bool requiresWifiEscape(char value) noexcept {
    return value == '\\' || value == ';' || value == ',' || value == ':' ||
           value == '"';
}

[[nodiscard]] std::size_t escapedLength(std::string_view value) noexcept {
    std::size_t length = value.size();
    for (const auto valueByte : value) {
        if (requiresWifiEscape(valueByte)) {
            ++length;
        }
    }
    return length;
}

}  // namespace

bool isValidSoftApPassword(std::string_view password) noexcept {
    if (password.size() != kSoftApPasswordLength) {
        return false;
    }
    for (const auto value : password) {
        if (kSoftApPasswordAlphabet.find(value) == std::string_view::npos) {
            return false;
        }
    }
    return true;
}

std::optional<std::string> deriveSoftApSsid(std::string_view deviceName) {
    if (deviceName.empty()) {
        return std::nullopt;
    }

    std::string candidate;
    candidate.reserve(std::min(deviceName.size(), kSoftApRawSsidMaximumBytes));
    std::vector<std::size_t> boundaries{0U};
    std::size_t offset = 0U;
    while (offset < deviceName.size()) {
        const auto length = codePointLength(deviceName, offset);
        if (!length.has_value() || offset + *length > deviceName.size()) {
            return std::nullopt;
        }
        if (offset + *length <= kSoftApRawSsidMaximumBytes) {
            candidate.append(deviceName.substr(offset, *length));
            boundaries.push_back(candidate.size());
        }
        offset += *length;
    }
    if (candidate.empty()) {
        return std::nullopt;
    }

    while (escapedLength(candidate) > kSoftApEscapedSsidMaximumBytes &&
           boundaries.size() > 1U) {
        boundaries.pop_back();
        candidate.resize(boundaries.back());
    }
    if (candidate.empty() ||
        escapedLength(candidate) > kSoftApEscapedSsidMaximumBytes) {
        return std::nullopt;
    }
    return candidate;
}

std::optional<SoftApCredentials> makeSoftApCredentials(
    device_platform::ISecureRandomSource& randomSource,
    std::string_view deviceName) {
    const auto ssid = deriveSoftApSsid(deviceName);
    if (!ssid.has_value()) {
        return std::nullopt;
    }

    std::string password;
    password.reserve(kSoftApPasswordLength);
    while (password.size() < kSoftApPasswordLength) {
        std::array<std::uint8_t, kSoftApPasswordLength> randomBytes{};
        if (!randomSource.fill(randomBytes.data(), randomBytes.size())) {
            return std::nullopt;
        }
        for (const auto randomByte : randomBytes) {
            if (randomByte >= kRejectionLimit) {
                continue;
            }
            password.push_back(
                kSoftApPasswordAlphabet[randomByte %
                                        kSoftApPasswordAlphabet.size()]);
            if (password.size() == kSoftApPasswordLength) {
                break;
            }
        }
    }
    return SoftApCredentials{*ssid, std::move(password)};
}

}  // namespace fermentation
