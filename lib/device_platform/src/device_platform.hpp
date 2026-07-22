#pragma once

#include "platform_services.hpp"

namespace device_platform {

struct PlatformStartupContext {
    bool configurationSafe;
};

class DevicePlatform final : public IPlatformServices {
   public:
    bool begin(const PlatformStartupContext& context);
    void update();

    bool ready() const override;

   private:
    bool ready_{false};
};

}  // namespace device_platform
