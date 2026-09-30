#include "softap_credentials.hpp"

#include <array>
#include <cstdint>
#include <utility>

namespace fermentation {
namespace {

constexpr std::uint8_t kRejectionLimit =
    static_cast<std::uint8_t>(256U - (256U % kSoftApPasswordAlphabet.size()));

}  // namespace

std::optional<SoftApCredentials> makeSoftApCredentials(
    device_platform::ISecureRandomSource& randomSource) {
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

    return SoftApCredentials{"Fermentation", std::move(password)};
}

}  // namespace fermentation
