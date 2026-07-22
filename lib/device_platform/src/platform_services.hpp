#pragma once

#include "app_config.hpp"

namespace device_platform {

class IPlatformServices {
   public:
    virtual ~IPlatformServices() = default;

    virtual bool ready() const = 0;
    virtual const app_config::ProfilePolicy& profilePolicy() const = 0;
};

}  // namespace device_platform
