#include "web_application_routes.hpp"

#include <utility>
#include <variant>

#include "web_json_codec.hpp"
#include "web_browser_policy.hpp"

namespace fermentation {
namespace {

constexpr char kRoutePath[] = "/internal/ui/run";
constexpr char kJsonContentType[] = "application/json; charset=utf-8";
constexpr char kStatusApiPath[] = "/api/v1/status";
constexpr char kTemperaturesApiPath[] = "/api/v1/temperatures";
constexpr char kAlertsApiPath[] = "/api/v1/alerts";
static_assert(kMaximumWebRunMutationBodyBytes + sizeof("POST") +
                      sizeof(kRoutePath) + 4U <=
                  kMaximumMutationFingerprintBytes,
              "maximum run DTO must fit the session replay fingerprint");

WebMutationOutcome outcome(std::uint16_t status, const char* body) {
    return {status, kJsonContentType, body};
}

constexpr auto kApplied = 200U;
constexpr auto kConflict = 409U;
constexpr auto kRejected = 422U;
constexpr auto kTooLarge = 413U;
constexpr auto kWriteFailed = 500U;
constexpr auto kUnavailable = 503U;

WebMutationOutcome unavailable() {
    return outcome(kUnavailable, "{\"outcome\":\"unavailable\"}");
}

bool decisionOnlyRejected(const FermentationUiCommandResult& result) {
    return result.phase == FermentationUiCommandPhase::DecisionOnly &&
           result.category ==
               device_platform::DeviceUiCommandOutcomeCategory::Rejected;
}

bool owningWithCategory(
    const FermentationUiCommandResult& result,
    device_platform::DeviceUiCommandOutcomeCategory category) {
    return result.phase == FermentationUiCommandPhase::OwningOutcome &&
           result.category == category;
}

void setResponse(device_platform::HttpResponse& response,
                 const WebMutationOutcome& projected) {
    response.statusCode = projected.statusCode;
    response.contentType = projected.contentType;
    response.body = projected.body;
    response.metadata = {};
}

WebMutationOutcome requestError(std::uint16_t status, const char* body) {
    return outcome(status, body);
}

WebMutationOutcome reservationError(MutationReservationStatus status) {
    switch (status) {
        case MutationReservationStatus::InvalidSession:
            return requestError(401U, "{\"error\":\"session-required\"}");
        case MutationReservationStatus::InvalidFingerprint:
            return requestError(kTooLarge, "{\"error\":\"request-too-large\"}");
        case MutationReservationStatus::Reserved:
        case MutationReservationStatus::ReplayOutcome:
            break;
        case MutationReservationStatus::InFlight:
        case MutationReservationStatus::SequenceConflict:
        case MutationReservationStatus::SequenceGap:
        case MutationReservationStatus::ReplayExpired:
        case MutationReservationStatus::SequenceReused:
        case MutationReservationStatus::Exhausted:
            return requestError(kConflict,
                                "{\"outcome\":\"sequence-conflict\"}");
    }
    return unavailable();
}

}  // namespace

WebMutationOutcome WebRunMutationHandler::projectRequestStatus(
    FermentationApplicationRequestStatus status) {
    switch (status) {
        case FermentationApplicationRequestStatus::Prepared:
            return unavailable();
        case FermentationApplicationRequestStatus::StaleProgramCatalog:
            return outcome(kConflict, "{\"outcome\":\"stale\"}");
        case FermentationApplicationRequestStatus::ProgramUnavailable:
        case FermentationApplicationRequestStatus::InvalidInput:
            return outcome(kRejected, "{\"outcome\":\"rejected\"}");
        case FermentationApplicationRequestStatus::NotInitialized:
        case FermentationApplicationRequestStatus::Unavailable:
        case FermentationApplicationRequestStatus::Overflow:
            return unavailable();
    }
    return unavailable();
}

WebMutationOutcome WebRunMutationHandler::projectCommandResult(
    const FermentationUiCommandResult& result) {
    using Category = device_platform::DeviceUiCommandOutcomeCategory;

    if (const auto* status =
            std::get_if<RunPersistenceResultStatus>(&result.detail)) {
        switch (*status) {
            case RunPersistenceResultStatus::Applied:
            case RunPersistenceResultStatus::CheckpointWritten:
            case RunPersistenceResultStatus::AlreadyProcessed:
            case RunPersistenceResultStatus::AlreadyPersisted:
                return owningWithCategory(result, Category::Accepted)
                           ? outcome(kApplied, "{\"outcome\":\"applied\"}")
                           : unavailable();
            case RunPersistenceResultStatus::Busy:
                return owningWithCategory(result, Category::Busy)
                           ? outcome(kConflict, "{\"outcome\":\"busy\"}")
                           : unavailable();
            case RunPersistenceResultStatus::StaleDecision:
                return owningWithCategory(result, Category::Rejected)
                           ? outcome(kConflict, "{\"outcome\":\"stale\"}")
                           : unavailable();
            case RunPersistenceResultStatus::NotInitialized:
            case RunPersistenceResultStatus::RecoveryPending:
            case RunPersistenceResultStatus::PersistenceIndeterminate:
            case RunPersistenceResultStatus::PersistenceCommittedApplyFailed:
            case RunPersistenceResultStatus::Blocked:
                return unavailable();
            case RunPersistenceResultStatus::WriteFailed:
                return owningWithCategory(result, Category::Rejected)
                           ? outcome(kWriteFailed,
                                     "{\"outcome\":\"write-failed\"}")
                           : unavailable();
            case RunPersistenceResultStatus::CapacityExceeded:
                return owningWithCategory(result, Category::Rejected)
                           ? outcome(kTooLarge, "{\"outcome\":\"too-large\"}")
                           : unavailable();
            case RunPersistenceResultStatus::NotEligible:
            case RunPersistenceResultStatus::NotAllowedInState:
            case RunPersistenceResultStatus::InvalidDecision:
            case RunPersistenceResultStatus::TimeMismatch:
            case RunPersistenceResultStatus::TimeWentBackwards:
            case RunPersistenceResultStatus::CounterOverflow:
            case RunPersistenceResultStatus::NotDue:
            case RunPersistenceResultStatus::NoActiveRun:
                return owningWithCategory(result, Category::Rejected)
                           ? outcome(kRejected, "{\"outcome\":\"rejected\"}")
                           : unavailable();
        }
        return unavailable();
    }

    if (const auto* status = std::get_if<CommandStatus>(&result.detail)) {
        switch (*status) {
            case CommandStatus::Applied:
            case CommandStatus::NoChange:
            case CommandStatus::AlreadyProcessed:
                return owningWithCategory(result, Category::Accepted)
                           ? outcome(kApplied, "{\"outcome\":\"applied\"}")
                           : unavailable();
            case CommandStatus::Proposed:
                // An accepted proposal is DecisionOnly, never a web success.
                return unavailable();
            case CommandStatus::NotConfirmed:
                return result.phase ==
                                   FermentationUiCommandPhase::DecisionOnly &&
                               result.category == Category::ConfirmationRequired
                           ? outcome(kConflict,
                                     "{\"outcome\":\"confirmation-required\"}")
                           : unavailable();
            case CommandStatus::StaleState:
                return decisionOnlyRejected(result)
                           ? outcome(kConflict, "{\"outcome\":\"stale\"}")
                           : unavailable();
            case CommandStatus::ContextMissing:
                return unavailable();
            case CommandStatus::CapacityReached:
                return decisionOnlyRejected(result)
                           ? outcome(kTooLarge, "{\"outcome\":\"too-large\"}")
                           : unavailable();
            case CommandStatus::NotAllowedInState:
            case CommandStatus::InvalidInput:
            case CommandStatus::SafetyRejected:
                return decisionOnlyRejected(result)
                           ? outcome(kRejected, "{\"outcome\":\"rejected\"}")
                           : unavailable();
        }
        return unavailable();
    }

    if (const auto* status = std::get_if<DecisionStatus>(&result.detail)) {
        if (result.phase != FermentationUiCommandPhase::DecisionOnly ||
            result.category != Category::Rejected) {
            return unavailable();
        }
        switch (*status) {
            case DecisionStatus::Proposed:
                return unavailable();
            case DecisionStatus::NoTransition:
            case DecisionStatus::Rejected:
            case DecisionStatus::InvalidInput:
            case DecisionStatus::TimeWentBackwards:
                return outcome(kRejected, "{\"outcome\":\"rejected\"}");
        }
    }

    // Configuration, network and unknown details are not valid outcomes for
    // this run-only route. A category alone can never authorize success.
    return unavailable();
}

bool WebRunMutationHandler::handle(const device_platform::HttpRequest& request,
                                   device_platform::HttpResponse& response) {
    if (request.path != kRoutePath) return false;
    if (request.method != "POST") {
        setResponse(response,
                    requestError(405U, "{\"error\":\"method-not-allowed\"}"));
        return true;
    }
    if (device_platform::validateHttpRequestMetadata(request.metadata) !=
        device_platform::HttpMetadataValidation::Valid) {
        setResponse(response,
                    requestError(400U, "{\"error\":\"invalid-request\"}"));
        return true;
    }
    if (!request.metadata.contentType.has_value() ||
        !web_browser_policy::exactJsonContentType(
            *request.metadata.contentType)) {
        setResponse(
            response,
            requestError(415U, "{\"error\":\"unsupported-media-type\"}"));
        return true;
    }
    if (!web_browser_policy::sameOrigin(request)) {
        setResponse(response,
                    requestError(403U, "{\"error\":\"origin-rejected\"}"));
        return true;
    }
    if (request.body.empty()) {
        setResponse(response,
                    requestError(400U, "{\"error\":\"body-required\"}"));
        return true;
    }

    WebRunMutationDto dto;
    const auto decodeStatus = decodeWebRunMutation(request.body, dto);
    if (decodeStatus != WebRunMutationDecodeStatus::Success) {
        const auto tooLarge =
            decodeStatus == WebRunMutationDecodeStatus::TooLarge;
        setResponse(response,
                    requestError(tooLarge ? kTooLarge : 400U,
                                 tooLarge ? "{\"error\":\"request-too-large\"}"
                                          : "{\"error\":\"invalid-json\"}"));
        return true;
    }

    const auto fingerprint = mutationFingerprint(request);
    if (fingerprint.empty()) {
        setResponse(
            response,
            requestError(kTooLarge, "{\"error\":\"request-too-large\"}"));
        return true;
    }
    if (!request.metadata.cookie.has_value()) {
        setResponse(response,
                    requestError(401U, "{\"error\":\"session-required\"}"));
        return true;
    }

    const auto nowMs = timeSource_.monotonicMillis();
    const auto session = sessions_.find(*request.metadata.cookie, nowMs);
    if (session.status != WebSessionStatus::Found ||
        !session.handle.has_value()) {
        setResponse(response,
                    requestError(401U, "{\"error\":\"session-required\"}"));
        return true;
    }
    if (!request.metadata.csrfToken.has_value() ||
        !sessions_.validateCsrf(*session.handle, *request.metadata.csrfToken,
                                nowMs)) {
        setResponse(response,
                    requestError(403U, "{\"error\":\"csrf-rejected\"}"));
        return true;
    }
    if (!request.metadata.mutationSeq.has_value()) {
        setResponse(response,
                    requestError(400U, "{\"error\":\"sequence-required\"}"));
        return true;
    }
    const auto sequence = parseMutationSequence(*request.metadata.mutationSeq);
    if (!sequence.has_value()) {
        setResponse(response,
                    requestError(400U, "{\"error\":\"invalid-sequence\"}"));
        return true;
    }

    // A validated, explicit mutation request is relevant activity. Lookup and
    // passive requests above intentionally do not renew the idle deadline.
    if (!sessions_.touch(*session.handle, nowMs)) {
        setResponse(response,
                    requestError(401U, "{\"error\":\"session-required\"}"));
        return true;
    }

    const auto reservation = sessions_.reserveMutation(*session.handle, nowMs,
                                                       *sequence, fingerprint);
    if (reservation.status == MutationReservationStatus::ReplayOutcome) {
        if (reservation.outcome.has_value()) {
            setResponse(response, *reservation.outcome);
        } else {
            setResponse(response, unavailable());
        }
        return true;
    }
    if (reservation.status != MutationReservationStatus::Reserved ||
        !reservation.sequence.has_value()) {
        setResponse(response, reservationError(reservation.status));
        return true;
    }

    FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::WebInterface;
    context.monotonicMillis = nowMs;
    context.expected = dto.expected;

    WebMutationOutcome projected;
    const auto prepared = application_.prepareEnvelope(context, dto.intent);
    if (prepared.status != FermentationApplicationRequestStatus::Prepared ||
        !prepared.request.has_value()) {
        projected = projectRequestStatus(prepared.status);
    } else {
        const auto confirmed = application_.confirmPrepared(prepared);
        if (confirmed.status !=
                FermentationApplicationRequestStatus::Prepared ||
            !confirmed.request.has_value()) {
            projected = projectRequestStatus(confirmed.status);
        } else {
            projected = projectCommandResult(
                application_.applyConfirmedPrepared(*confirmed.request));
        }
    }

    if (!sessions_.completeMutation(
            *session.handle, timeSource_.monotonicMillis(),
            *reservation.sequence, fingerprint, projected)) {
        setResponse(response, unavailable());
        return true;
    }
    setResponse(response, projected);
    return true;
}

bool WebReadOnlyApiHandler::handle(const device_platform::HttpRequest& request,
                                   device_platform::HttpResponse& response) {
    const bool status = request.path == kStatusApiPath;
    const bool temperatures = request.path == kTemperaturesApiPath;
    const bool alerts = request.path == kAlertsApiPath;
    if (!status && !temperatures && !alerts) return false;
    if (request.method != "GET") {
        setResponse(response,
                    requestError(405U, "{\"error\":\"method-not-allowed\"}"));
        return true;
    }
    if (!request.body.empty()) {
        setResponse(response,
                    requestError(400U, "{\"error\":\"body-not-allowed\"}"));
        return true;
    }

    const auto snapshot = application_.uiSnapshot();
    std::string body;
    const bool encoded = status ? encodeWebApiStatus(snapshot, body)
                         : temperatures
                             ? encodeWebApiTemperatures(snapshot, body)
                             : encodeWebApiAlerts(snapshot, body);
    if (!encoded || body.size() > kMaximumWebApiResponseBodyBytes) {
        setResponse(response, unavailable());
        return true;
    }
    response.statusCode = 200U;
    response.contentType = kJsonContentType;
    response.body = std::move(body);
    response.metadata = {};
    return true;
}

}  // namespace fermentation
