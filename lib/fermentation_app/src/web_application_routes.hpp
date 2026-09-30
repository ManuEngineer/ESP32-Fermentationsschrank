#pragma once

#include "fermentation_application.hpp"
#include "http_server_lifecycle.hpp"
#include "time_source.hpp"
#include "web_session.hpp"

namespace fermentation {

// The transport's bounded DTO decoder supplies this value after decoding the
// body. This handler intentionally does not choose or depend on a JSON codec.
struct WebRunMutationDto {
    FermentationUiExpectedRevisions expected;
    FermentationUiEnvelopePayload intent;
};

// Narrow, synchronous application handler for POST /internal/ui/run. It is
// deliberately not registered by the current composition root. The future
// transport must decode the DTO from this same request and serialize calls
// with the Application owner; this class adds neither a parser nor a worker.
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
                              const WebRunMutationDto& dto,
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

}  // namespace fermentation
