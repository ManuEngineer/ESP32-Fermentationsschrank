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

    [[nodiscard]] static WebMutationOutcome projectCommandResult(
        const FermentationUiCommandResult& result);
    [[nodiscard]] static WebMutationOutcome projectRequestStatus(
        FermentationApplicationRequestStatus status);

    FermentationApplication& application_;
    WebSessionManager& sessions_;
    const device_platform::ITimeSource& timeSource_;
};

// Read-only endpoint projector. It is deliberately not registered by the
// current composition root. The future dispatcher remains responsible for
// the plan's configured session/authentication policy before calling it.
class WebReadOnlyApiHandler final {
   public:
    explicit WebReadOnlyApiHandler(FermentationApplication& application)
        : application_(application) {}

    [[nodiscard]] bool handle(const device_platform::HttpRequest& request,
                              device_platform::HttpResponse& response);

   private:
    FermentationApplication& application_;
};

}  // namespace fermentation
