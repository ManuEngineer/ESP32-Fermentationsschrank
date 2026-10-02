#pragma once

#include "fermentation_application.hpp"
#include "http_server_lifecycle.hpp"
#include "time_source.hpp"
#include "web_session.hpp"

namespace fermentation {

struct WebRunMutationDto {
    FermentationUiExpectedRevisions expected;
    FermentationUiEnvelopePayload intent;
};

// Narrow, synchronous application handler for POST /internal/ui/run. It is
// deliberately not registered by the current composition root. It decodes
// the bounded DTO from the exact request body whose bytes are replay-bound;
// composition must still serialize calls with the Application owner.
class WebRunMutationHandler final {
   public:
    WebRunMutationHandler(FermentationApplication& application,
                          WebSessionManager& sessions,
                          const device_platform::ITimeSource& timeSource)
        : application_(application),
          sessions_(sessions),
          timeSource_(timeSource) {}

    // Returns false only when the path is not this handler's route.
    [[nodiscard]] bool handle(const device_platform::HttpRequest& request,
                              device_platform::HttpResponse& response);

   private:
    friend struct WebRunMutationHandlerTestAccess;

    [[nodiscard]] static ReplayOutcomeCode projectCommandResult(
        const FermentationUiCommandResult& result);
    [[nodiscard]] static ReplayOutcomeCode projectRequestStatus(
        FermentationApplicationRequestStatus status);

    FermentationApplication& application_;
    WebSessionManager& sessions_;
    const device_platform::ITimeSource& timeSource_;
};

// Read-only endpoint projector. The productive dispatcher remains responsible
// for the configured session/authentication policy before calling it.
class WebReadOnlyApiHandler final {
   public:
    explicit WebReadOnlyApiHandler(FermentationApplication& application)
        : application_(application) {}

    [[nodiscard]] bool handle(const device_platform::HttpRequest& request,
                              device_platform::HttpResponse& response);

   private:
    FermentationApplication& application_;
};

// The one productive route sink. It gives the existing setup/recovery route
// priority while setup is active and otherwise composes authentication,
// read-only APIs and the bounded static shell. The run-mutation handler is
// intentionally not a member and therefore cannot be reached productively.
class WebRouteDispatcher final : public device_platform::IHttpRouteSink {
   public:
    WebRouteDispatcher(NetworkSetupRoutes& networkSetupRoutes,
                       FermentationApplication& application,
                       WebSessionManager& sessions,
                       const device_platform::ITimeSource& timeSource)
        : networkSetupRoutes_(networkSetupRoutes),
          application_(application),
          sessions_(sessions),
          timeSource_(timeSource),
          readOnly_(application) {}

    [[nodiscard]] bool handle(const device_platform::HttpRequest& request,
                              device_platform::HttpResponse& response) override;

   private:
    [[nodiscard]] bool handleShell(const device_platform::HttpRequest& request,
                                   device_platform::HttpResponse& response);
    [[nodiscard]] bool handleLogin(const device_platform::HttpRequest& request,
                                   device_platform::HttpResponse& response);
    [[nodiscard]] bool handleLogout(const device_platform::HttpRequest& request,
                                    device_platform::HttpResponse& response);
    [[nodiscard]] bool handleReadOnly(
        const device_platform::HttpRequest& request,
        device_platform::HttpResponse& response);

    NetworkSetupRoutes& networkSetupRoutes_;
    FermentationApplication& application_;
    WebSessionManager& sessions_;
    const device_platform::ITimeSource& timeSource_;
    WebReadOnlyApiHandler readOnly_;
};

}  // namespace fermentation
