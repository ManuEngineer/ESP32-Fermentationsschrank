#pragma once

#include "platform_services.hpp"

namespace fermentation {

class FermentationApplication {
   public:
    [[nodiscard]] bool begin(
        device_platform::IPlatformServices& platformServices);
    void update();

    [[nodiscard]] bool ready() const;

   private:
    device_platform::IPlatformServices* platformServices_{nullptr};
};

}  // namespace fermentation
