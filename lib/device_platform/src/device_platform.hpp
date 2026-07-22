#pragma once

#include "platform_services.hpp"

namespace device_platform {

struct PlatformStartupContext {
    bool configurationSafe;
};

class DevicePlatform final : public IPlatformServices {
   public:
    [[nodiscard]] bool begin(const PlatformStartupContext& context);
    void update();

    [[nodiscard]] bool ready() const override;

   private:
    bool ready_{false};
};

}  // namespace device_platform
