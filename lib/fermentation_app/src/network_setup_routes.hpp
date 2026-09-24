#pragma once

#include "http_server_lifecycle.hpp"
#include "network_configuration_service.hpp"

namespace fermentation {

// The Issue #164 consumer is intentionally a small setup surface. It is
// registered on the one platform-owned HTTP lifecycle and contains no normal
// R1 Web UI, login, session or device-command logic.
class NetworkSetupRoutes final : public device_platform::IHttpRouteSink {
   public:
    explicit NetworkSetupRoutes(NetworkConfigurationService& networkService)
        : networkService_(networkService) {}

    [[nodiscard]] bool handle(const device_platform::HttpRequest& request,
                              device_platform::HttpResponse& response) override;
    [[nodiscard]] bool ownsRoute(
        const device_platform::HttpRequest& request) const noexcept;

   private:
    NetworkConfigurationService& networkService_;
};

}  // namespace fermentation
