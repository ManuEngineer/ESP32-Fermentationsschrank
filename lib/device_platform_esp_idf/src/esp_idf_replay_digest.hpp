#pragma once

#include "replay_digest.hpp"

namespace device_platform_esp_idf {

class EspIdfSha256ReplayDigest final : public device_platform::IReplayDigest {
   public:
    [[nodiscard]] bool digest(const device_platform::ReplayDigestInput& input,
                              device_platform::ReplayDigest& out) override;
};

}  // namespace device_platform_esp_idf
