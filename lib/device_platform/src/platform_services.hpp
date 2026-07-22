#pragma once

namespace device_platform {

class IPlatformServices {
   public:
    IPlatformServices() = default;
    virtual ~IPlatformServices() = default;

    IPlatformServices(const IPlatformServices&) = delete;
    IPlatformServices& operator=(const IPlatformServices&) = delete;
    IPlatformServices(IPlatformServices&&) = delete;
    IPlatformServices& operator=(IPlatformServices&&) = delete;

    [[nodiscard]] virtual bool ready() const = 0;
};

}  // namespace device_platform
