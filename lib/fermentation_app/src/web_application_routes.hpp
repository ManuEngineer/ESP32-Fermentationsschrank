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

// Narrow, synchronous application handler for POST /internal/ui/run. It decodes
// the bounded DTO from the exact request body whose bytes are replay-bound.
// The Application serializes its public calls itself; the dispatcher gates the
// route by authentication state before calling the handler.
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
// read-only APIs, the run-mutation route and the bounded static shell.
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
          readOnly_(application),
          run_(application, sessions, timeSource) {}

    [[nodiscard]] bool handle(const device_platform::HttpRequest& request,
                              device_platform::HttpResponse& response) override;

   private:
#ifdef APP_PROFILE_NATIVE
    friend struct WebRouteDispatcherTestAccess;
    inline static std::size_t extraShellBytesForTest_{0U};
    inline static std::size_t extraAssetBytesForTest_{0U};
#endif
    [[nodiscard]] bool handleAsset(const device_platform::HttpRequest& request,
                                   device_platform::HttpResponse& response);
    [[nodiscard]] bool handleShell(const device_platform::HttpRequest& request,
                                   device_platform::HttpResponse& response);
    [[nodiscard]] bool handleLogin(const device_platform::HttpRequest& request,
                                   device_platform::HttpResponse& response);
    [[nodiscard]] bool handleLogout(const device_platform::HttpRequest& request,
                                    device_platform::HttpResponse& response);
    [[nodiscard]] bool handleProvision(
        const device_platform::HttpRequest& request,
        device_platform::HttpResponse& response);
    [[nodiscard]] bool handleReadOnly(
        const device_platform::HttpRequest& request,
        device_platform::HttpResponse& response);

    [[nodiscard]] bool handleRun(const device_platform::HttpRequest& request,
                                 device_platform::HttpResponse& response);

    NetworkSetupRoutes& networkSetupRoutes_;
    FermentationApplication& application_;
    WebSessionManager& sessions_;
    const device_platform::ITimeSource& timeSource_;
    WebReadOnlyApiHandler readOnly_;
    WebRunMutationHandler run_;
};

}  // namespace fermentation
