#pragma once

namespace device_platform {

class IPlatformServices {
   public:
    virtual ~IPlatformServices() = default;

    virtual bool ready() const = 0;
};

}  // namespace device_platform
