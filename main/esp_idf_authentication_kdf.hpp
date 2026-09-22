#pragma once

#include "authentication_records.hpp"

namespace device_platform_esp_idf {

class EspIdfPbkdf2HmacSha256 final : public fermentation::IAuthenticationKdf {
   public:
    [[nodiscard]] bool derive(
        const std::string& secret, const fermentation::AuthVerifier& parameters,
        std::array<std::uint8_t, fermentation::kAuthenticationVerifierBytes>&
            out) override;
};

}  // namespace device_platform_esp_idf
