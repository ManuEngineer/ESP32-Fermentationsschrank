#pragma once

#include "platform_services.hpp"

namespace fermentation {

class FermentationApplication {
   public:
    bool begin(device_platform::IPlatformServices& platformServices);
    void update();

    bool ready() const;

   private:
    device_platform::IPlatformServices* platformServices_{nullptr};
};

}  // namespace fermentation
