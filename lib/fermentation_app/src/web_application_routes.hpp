#pragma once

#include <optional>

#include "authentication_records.hpp"
#include "fermentation_application.hpp"
#include "network_setup_routes.hpp"
#include "web_session.hpp"

namespace fermentation {

class WebApplicationRoutes final : public device_platform::IHttpRouteSink {
   public:
    WebApplicationRoutes(FermentationApplication& application,
                         AuthenticationDomain& authentication,
                         WebSessionManager& sessions,
                         const device_platform::ITimeSource& timeSource)
        : application_(application),
          authentication_(authentication),
          sessions_(sessions),
          timeSource_(timeSource) {}

    [[nodiscard]] bool handle(const device_platform::HttpRequest& request,
                              device_platform::HttpResponse& response) override;

   private:
    [[nodiscard]] bool browserPolicy(
        const device_platform::HttpRequest& request,
        device_platform::HttpResponse& response, bool mutation) const;
    [[nodiscard]] std::optional<WebSessionHandle> session(
        const device_platform::HttpRequest& request,
        device_platform::HttpResponse& response, bool mutation);
    [[nodiscard]] bool complete(WebSessionHandle handle, std::uint64_t sequence,
                                const std::string& fingerprint,
                                device_platform::HttpResponse& response);

    FermentationApplication& application_;
    AuthenticationDomain& authentication_;
    WebSessionManager& sessions_;
    const device_platform::ITimeSource& timeSource_;
};

// One dispatcher is registered with the one #164-owned HTTP lifecycle. Setup
// owns its routes while setup is active; normal Web/API routes own everything
// else. Neither consumer starts a server or keeps a second route registry.
class WebRouteDispatcher final : public device_platform::IHttpRouteSink {
   public:
    WebRouteDispatcher(NetworkSetupRoutes& setup, WebApplicationRoutes& web)
        : setup_(setup), web_(web) {}

    [[nodiscard]] bool handle(
        const device_platform::HttpRequest& request,
        device_platform::HttpResponse& response) override {
        if (setup_.ownsRoute(request)) {
            return setup_.handle(request, response);
        }
        return web_.handle(request, response);
    }

   private:
    NetworkSetupRoutes& setup_;
    WebApplicationRoutes& web_;
};

}  // namespace fermentation
