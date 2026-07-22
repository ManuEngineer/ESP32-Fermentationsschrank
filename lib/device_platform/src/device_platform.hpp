#pragma once

#include "platform_services.hpp"

namespace device_platform {

class DevicePlatform final : public IPlatformServices {
   public:
    bool begin();
    void update();

    bool ready() const override;
    const app_config::ProfilePolicy& profilePolicy() const override;

   private:
    bool ready_{false};
};

}  // namespace device_platform
