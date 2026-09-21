#include "web_application_routes.hpp"

#include <algorithm>
#include <string>

#include "web_api_codec.hpp"
#include "web_assets.hpp"

namespace fermentation {
namespace {

void jsonResponse(device_platform::HttpResponse& response, std::uint16_t status,
                  std::string body) {
    response.statusCode = status;
    response.contentType = "application/json; charset=utf-8";
    response.body = std::move(body);
}

void textResponse(device_platform::HttpResponse& response, std::uint16_t status,
                  const char* contentType, const char* body) {
    response.statusCode = status;
    response.contentType = contentType;
    response.body = body;
}

void errorResponse(device_platform::HttpResponse& response, std::uint16_t status,
                   const char* code, const char* message) {
    std::string body;
    if (encodeError(code, message, body) != WebApiCodecStatus::Success) {
        textResponse(response, 500U, "text/plain; charset=utf-8", "error");
        return;
    }
    jsonResponse(response, status, std::move(body));
}

bool sameOrigin(const device_platform::HttpRequest& request) {
    if (request.metadata.secFetchSite.has_value() &&
        (*request.metadata.secFetchSite == "cross-site" ||
         *request.metadata.secFetchSite == "cross-origin"))
        return false;
    const auto host = request.metadata.host.value_or(std::string{});
    const auto originMatchesHost = [&host](const std::string& value) {
        const auto scheme = value.find("://");
        if (scheme == std::string::npos || host.empty()) return false;
        const auto authorityStart = scheme + 3U;
        const auto authorityEnd = value.find('/', authorityStart);
        const auto authority = value.substr(
            authorityStart,
            authorityEnd == std::string::npos ? std::string::npos
                                               : authorityEnd - authorityStart);
        return authority == host && value.find('@', authorityStart) ==
                                      std::string::npos;
    };
    if (request.metadata.origin.has_value()) {
        return originMatchesHost(*request.metadata.origin);
    }
    if (request.metadata.referer.has_value()) {
        return originMatchesHost(*request.metadata.referer);
    }
    return true;
}

std::uint16_t networkStatusCode(NetworkConfigurationStatus status) {
    switch (status) {
        case NetworkConfigurationStatus::Applied:
            return 200U;
        case NetworkConfigurationStatus::CandidateRejected:
            return 422U;
        case NetworkConfigurationStatus::PersistenceFailure:
            return 500U;
        case NetworkConfigurationStatus::StateChanged:
            return 409U;
        case NetworkConfigurationStatus::CommitIndeterminate:
        case NetworkConfigurationStatus::RecoveryRequired:
            return 503U;
        default:
            return 409U;
    }
}

}  // namespace

bool WebApplicationRoutes::browserPolicy(
    const device_platform::HttpRequest& request,
    device_platform::HttpResponse& response, bool mutation) const {
    if (!sameOrigin(request)) {
        errorResponse(response, 403U, "origin_rejected", "same origin required");
        return false;
    }
    if (mutation) {
        if (request.method != "POST") {
            errorResponse(response, 405U, "method_not_allowed", "POST required");
            return false;
        }
        if (!request.metadata.contentType.has_value() ||
            request.metadata.contentType->find("application/json") != 0U) {
            errorResponse(response, 415U, "content_type_rejected",
                          "application/json required");
            return false;
        }
    }
    return true;
}

std::optional<WebSessionHandle> WebApplicationRoutes::session(
    const device_platform::HttpRequest& request,
    device_platform::HttpResponse& response, bool mutation) {
    if (!request.metadata.cookie.has_value()) {
        errorResponse(response, 401U, "authentication_required", "login required");
        return std::nullopt;
    }
    const auto found = sessions_.find(*request.metadata.cookie,
                                      timeSource_.monotonicMillis());
    if (found.status != WebSessionStatus::Found || !found.handle.has_value()) {
        errorResponse(response, 401U, "session_invalid", "login required");
        return std::nullopt;
    }
    if (mutation) {
        if (!request.metadata.csrfToken.has_value() ||
            !sessions_.validateCsrf(*found.handle, *request.metadata.csrfToken,
                                    timeSource_.monotonicMillis())) {
            errorResponse(response, 403U, "csrf_rejected", "csrf token required");
            return std::nullopt;
        }
    }
    return found.handle;
}

bool WebApplicationRoutes::complete(
    WebSessionHandle handle, std::uint64_t sequence, const std::string& fingerprint,
    device_platform::HttpResponse& response) {
    const WebMutationOutcome outcome{response.statusCode, response.contentType,
                                     response.body};
    return sessions_.completeMutation(handle, timeSource_.monotonicMillis(),
                                      sequence, fingerprint, outcome);
}

bool WebApplicationRoutes::handle(const device_platform::HttpRequest& request,
                                  device_platform::HttpResponse& response) {
    if (request.method == "GET" && request.path == "/") {
        const auto epoch = application_.currentStorageEpoch();
        if (epoch.has_value() &&
            authentication_.inspect(*epoch) == AuthBootstrapStatus::BootstrapAllowed) {
            textResponse(response, 200U, "text/html; charset=utf-8",
                         web_assets::kProvisioningHtml);
        } else {
            textResponse(response, 200U, "text/html; charset=utf-8",
                         web_assets::kIndexHtml);
        }
        return true;
    }
    if (request.method == "GET" && request.path == "/assets/app.css") {
        textResponse(response, 200U, "text/css; charset=utf-8", web_assets::kAppCss);
        return true;
    }
    if (request.method == "GET" && request.path == "/assets/app.js") {
        textResponse(response, 200U, "text/javascript; charset=utf-8", web_assets::kAppJs);
        return true;
    }
    if (request.path == "/login") {
        if (!browserPolicy(request, response, true)) return true;
        const auto epoch = application_.currentStorageEpoch();
        if (!epoch.has_value()) {
            errorResponse(response, 503U, "auth_recovery_required", "authentication unavailable");
            return true;
        }
        const auto authenticationState = authentication_.inspect(*epoch);
        if (authenticationState == AuthBootstrapStatus::BootstrapAllowed) {
            errorResponse(response, 503U, "auth_not_provisioned",
                          "local provisioning required");
            return true;
        }
        if (authenticationState != AuthBootstrapStatus::AlreadyProvisioned) {
            errorResponse(response, 503U, "auth_recovery_required",
                          "authentication unavailable");
            return true;
        }
        const auto enabled = authentication_.webPasswordEnabled(*epoch);
        std::string password;
        if (!enabled.has_value()) {
            errorResponse(response, 503U, "auth_recovery_required", "authentication unavailable");
            return true;
        }
        if (*enabled) {
            const auto parsed = decodeCredentialField(request.body, "password", 256U, password);
            if (parsed != WebApiCodecStatus::Success) {
                errorResponse(response, 401U, "authentication_failed", "authentication failed");
                return true;
            }
        }
        std::uint64_t retryAfterMs = 0U;
        const auto check = *enabled
                               ? authentication_.verifyWebPassword(
                                     *epoch, password, timeSource_.monotonicMillis(),
                                     retryAfterMs)
                               : AuthCheckStatus::Disabled;
        if (check != AuthCheckStatus::Authenticated &&
            check != AuthCheckStatus::Disabled) {
            if (check == AuthCheckStatus::LockedOut) {
                response.metadata.retryAfter =
                    std::to_string((retryAfterMs + 999U) / 1000U);
                errorResponse(response, 429U, "locked_out", "try again later");
            } else if (check == AuthCheckStatus::Invalid) {
                errorResponse(response, 401U, "authentication_failed", "authentication failed");
            } else {
                errorResponse(response, 503U, "auth_recovery_required", "authentication unavailable");
            }
            return true;
        }
        const auto created = sessions_.create(timeSource_.monotonicMillis());
        if (created.status != WebSessionStatus::Created || !created.handle.has_value()) {
            errorResponse(response, 503U, "session_capacity", "session unavailable");
            return true;
        }
        std::string body;
        if (encodeSessionHandoff(
                created.csrfToken,
                sessions_.mutationSequence(*created.handle,
                                           timeSource_.monotonicMillis()),
                body) != WebApiCodecStatus::Success) {
            sessions_.revoke(*created.handle);
            errorResponse(response, 503U, "session_unavailable", "session unavailable");
            return true;
        }
        jsonResponse(response, 200U, std::move(body));
        response.metadata.setCookie = "FSSESSION=" + created.cookieValue +
                                      "; HttpOnly; SameSite=Strict; Path=/";
        return true;
    }
    if (request.method == "POST" && request.path == "/logout") {
        if (!browserPolicy(request, response, true)) return true;
        const auto handle = session(request, response, true);
        if (!handle.has_value()) return true;
        if (!request.metadata.mutationSeq.has_value()) {
            errorResponse(response, 409U, "mutation_sequence_required",
                          "mutation sequence required");
            return true;
        }
        const auto sequence = parseMutationSequence(*request.metadata.mutationSeq);
        if (!sequence.has_value()) {
            errorResponse(response, 431U, "mutation_sequence_invalid",
                          "mutation sequence invalid");
            return true;
        }
        const auto fingerprint = mutationFingerprint(request);
        const auto reservation = sessions_.reserveMutation(
            *handle, timeSource_.monotonicMillis(), *sequence, fingerprint);
        if (reservation.status == MutationReservationStatus::ReplayOutcome &&
            reservation.outcome.has_value()) {
            response.statusCode = reservation.outcome->statusCode;
            response.contentType = reservation.outcome->contentType;
            response.body = reservation.outcome->body;
            return true;
        }
        if (reservation.status != MutationReservationStatus::Reserved) {
            errorResponse(response, reservation.status ==
                                             MutationReservationStatus::Exhausted
                                         ? 503U
                                         : 409U,
                          "mutation_sequence_conflict", "reload required");
            return true;
        }
        response.metadata.setCookie =
            "FSSESSION=; Max-Age=0; HttpOnly; SameSite=Strict; Path=/";
        response.statusCode = 204U;
        response.contentType = "application/json; charset=utf-8";
        response.body.clear();
        static_cast<void>(complete(*handle, *sequence, fingerprint, response));
        sessions_.revoke(*handle);
        return true;
    }
    if (request.path == "/api/v1/status" ||
        request.path == "/api/v1/temperatures" ||
        request.path == "/api/v1/alerts") {
        if (request.method != "GET") {
            errorResponse(response, 405U, "method_not_allowed", "GET required");
            return true;
        }
        if (!browserPolicy(request, response, false)) return true;
        if (!session(request, response, false).has_value()) return true;
        std::string body;
        const auto snapshot = application_.uiSnapshot();
        WebApiCodecStatus status = WebApiCodecStatus::InvalidJson;
        const auto epoch = application_.currentStorageEpoch();
        const auto passwordEnabled = epoch.has_value()
                                         ? authentication_.webPasswordEnabled(*epoch)
                                         : std::nullopt;
        if (request.path == "/api/v1/status")
            status = encodeStatus(snapshot, passwordEnabled, body);
        else if (request.path == "/api/v1/temperatures") status = encodeTemperatures(snapshot, body);
        else if (request.path == "/api/v1/alerts") status = encodeAlerts(snapshot, body);
        if (status != WebApiCodecStatus::Success) {
            errorResponse(response, 503U, "response_unavailable", "snapshot unavailable");
        } else {
            jsonResponse(response, 200U, std::move(body));
        }
        return true;
    }
    if (request.method == "GET" && request.path == "/internal/ui/snapshot") {
        if (!browserPolicy(request, response, false)) return true;
        const auto handle = session(request, response, false);
        if (!handle.has_value()) return true;
        std::string body;
        const auto snapshot = application_.uiSnapshot();
        const auto epoch = application_.currentStorageEpoch();
        const auto passwordEnabled = epoch.has_value()
                                         ? authentication_.webPasswordEnabled(*epoch)
                                         : std::nullopt;
        if (encodeUiSnapshot(
                snapshot,
                sessions_.mutationSequence(*handle, timeSource_.monotonicMillis()),
                passwordEnabled, body) != WebApiCodecStatus::Success) {
            errorResponse(response, 503U, "response_unavailable",
                          "snapshot unavailable");
        } else {
            jsonResponse(response, 200U, std::move(body));
        }
        return true;
    }
    if (request.method == "GET" && request.path == "/internal/service/status") {
        if (!browserPolicy(request, response, false)) return true;
        const auto handle = session(request, response, false);
        if (!handle.has_value()) return true;
        const bool active = sessions_.serviceLeaseActive(
            *handle, timeSource_.monotonicMillis());
        jsonResponse(response, 200U,
                     active ? "{\"serviceLease\":\"ACTIVE\"}"
                            : "{\"serviceLease\":\"REQUIRED\"}");
        return true;
    }
    if (request.method == "POST" && request.path == "/internal/service/unlock") {
        if (!browserPolicy(request, response, true)) return true;
        const auto handle = session(request, response, true);
        if (!handle.has_value()) return true;
        if (!request.metadata.mutationSeq.has_value()) {
            errorResponse(response, 409U, "mutation_sequence_required",
                          "mutation sequence required");
            return true;
        }
        const auto sequence = parseMutationSequence(*request.metadata.mutationSeq);
        std::string pin;
        if (!sequence.has_value() ||
            decodeCredentialField(request.body, "servicePin", 4U, pin) !=
                WebApiCodecStatus::Success) {
            errorResponse(response, 422U, "invalid_service_pin",
                          "invalid service pin");
            return true;
        }
        const auto fingerprint = mutationFingerprint(request);
        const auto reservation = sessions_.reserveMutation(
            *handle, timeSource_.monotonicMillis(), *sequence, fingerprint);
        if (reservation.status == MutationReservationStatus::ReplayOutcome &&
            reservation.outcome.has_value()) {
            response.statusCode = reservation.outcome->statusCode;
            response.contentType = reservation.outcome->contentType;
            response.body = reservation.outcome->body;
            return true;
        }
        if (reservation.status != MutationReservationStatus::Reserved) {
            errorResponse(response, 409U, "mutation_sequence_conflict",
                          "reload required");
            return true;
        }
        std::uint64_t retryAfterMs = 0U;
        const auto epoch = application_.currentStorageEpoch();
        const auto check = epoch.has_value()
                               ? authentication_.verifyServicePin(
                                     *epoch, pin,
                                     timeSource_.monotonicMillis(), retryAfterMs)
                               : AuthCheckStatus::RecoveryRequired;
        if (check == AuthCheckStatus::Authenticated &&
            sessions_.grantServiceLease(*handle,
                                        timeSource_.monotonicMillis())) {
            jsonResponse(response, 200U, "{\"serviceLease\":\"ACTIVE\"}");
        } else if (check == AuthCheckStatus::LockedOut) {
            response.metadata.retryAfter =
                std::to_string((retryAfterMs + 999U) / 1000U);
            errorResponse(response, 429U, "locked_out", "try again later");
        } else if (check == AuthCheckStatus::Invalid) {
            errorResponse(response, 401U, "authentication_failed",
                          "authentication failed");
        } else {
            errorResponse(response, 503U, "auth_recovery_required",
                          "authentication unavailable");
        }
        static_cast<void>(complete(*handle, *sequence, fingerprint, response));
        return true;
    }
    if (request.method == "POST" &&
        request.path == "/internal/auth/password-mode") {
        if (!browserPolicy(request, response, true)) return true;
        const auto handle = session(request, response, true);
        if (!handle.has_value()) return true;
        if (!sessions_.serviceLeaseActive(*handle,
                                          timeSource_.monotonicMillis())) {
            errorResponse(response, 403U, "service_authorization_required",
                          "service authorization required");
            return true;
        }
        if (!request.metadata.mutationSeq.has_value()) {
            errorResponse(response, 409U, "mutation_sequence_required",
                          "mutation sequence required");
            return true;
        }
        const auto sequence = parseMutationSequence(*request.metadata.mutationSeq);
        bool enabled = false;
        bool confirmed = false;
        if (!sequence.has_value() ||
            decodeBooleanField(request.body, "enabled", enabled) !=
                WebApiCodecStatus::Success ||
            decodeBooleanField(request.body, "confirmed", confirmed) !=
                WebApiCodecStatus::Success || !confirmed) {
            errorResponse(response, 422U, "invalid_password_mode",
                          "explicit confirmation required");
            return true;
        }
        const auto currentEnabled = application_.currentStorageEpoch().has_value()
                                        ? authentication_.webPasswordEnabled(
                                              *application_.currentStorageEpoch())
                                        : std::nullopt;
        if (!currentEnabled.has_value()) {
            errorResponse(response, 503U, "auth_recovery_required",
                          "authentication unavailable");
            return true;
        }
        std::string currentPassword;
        std::string replacementPassword;
        const auto credentialField = enabled ? "newPassword" : "currentPassword";
        if (decodeCredentialField(request.body, credentialField, 256U,
                                  enabled ? replacementPassword : currentPassword) !=
            WebApiCodecStatus::Success) {
            errorResponse(response, 422U, "invalid_password",
                          "invalid password");
            return true;
        }
        const auto fingerprint = mutationFingerprint(request);
        const auto reservation = sessions_.reserveMutation(
            *handle, timeSource_.monotonicMillis(), *sequence, fingerprint);
        if (reservation.status == MutationReservationStatus::ReplayOutcome &&
            reservation.outcome.has_value()) {
            response.statusCode = reservation.outcome->statusCode;
            response.contentType = reservation.outcome->contentType;
            response.body = reservation.outcome->body;
            return true;
        }
        if (reservation.status != MutationReservationStatus::Reserved) {
            errorResponse(response, reservation.status ==
                                             MutationReservationStatus::Exhausted
                                         ? 503U
                                         : 409U,
                          "mutation_sequence_conflict", "reload required");
            return true;
        }
        const auto epoch = application_.currentStorageEpoch();
        const auto outcome = epoch.has_value()
                                  ? authentication_.setWebPasswordEnabled(
                                        *epoch, currentPassword,
                                        replacementPassword, enabled, confirmed,
                                        timeSource_.monotonicMillis())
                                  : AuthBootstrapStatus::RecoveryRequired;
        switch (outcome) {
            case AuthBootstrapStatus::BootstrapAllowed:
                jsonResponse(response, 200U,
                             enabled ? "{\"webPasswordEnabled\":true}"
                                     : "{\"webPasswordEnabled\":false}");
                break;
            case AuthBootstrapStatus::InvalidInput:
                errorResponse(response, 422U, "invalid_password_mode",
                              "invalid password mode request");
                break;
            case AuthBootstrapStatus::KdfUnavailable:
                errorResponse(response, 503U, "kdf_unavailable",
                              "authentication unavailable");
                break;
            case AuthBootstrapStatus::CommitOutcomeUnknown:
                errorResponse(response, 503U, "commit_indeterminate",
                              "authentication recovery required");
                break;
            case AuthBootstrapStatus::PersistenceFailure:
                errorResponse(response, 500U, "persistence_failure",
                              "password mode change failed");
                break;
            default:
                errorResponse(response, 503U, "auth_recovery_required",
                              "authentication recovery required");
                break;
        }
        static_cast<void>(complete(*handle, *sequence, fingerprint, response));
        if (outcome == AuthBootstrapStatus::BootstrapAllowed) {
            sessions_.revokeAll();
        }
        return true;
    }
    if (request.method == "POST" &&
        (request.path == "/internal/auth/password" ||
         request.path == "/internal/auth/service-pin")) {
        if (!browserPolicy(request, response, true)) return true;
        const auto handle = session(request, response, true);
        if (!handle.has_value()) return true;
        const bool pinChange = request.path == "/internal/auth/service-pin";
        if (pinChange && !sessions_.serviceLeaseActive(
                             *handle, timeSource_.monotonicMillis())) {
            errorResponse(response, 403U, "service_authorization_required",
                          "service authorization required");
            return true;
        }
        if (!request.metadata.mutationSeq.has_value()) {
            errorResponse(response, 409U, "mutation_sequence_required",
                          "mutation sequence required");
            return true;
        }
        const auto sequence = parseMutationSequence(*request.metadata.mutationSeq);
        const char* currentField = pinChange ? "currentServicePin" : "currentPassword";
        const char* replacementField = pinChange ? "newServicePin" : "newPassword";
        std::string current;
        std::string replacement;
        const auto currentLimit = pinChange ? 4U : 256U;
        if (!sequence.has_value() ||
            decodeCredentialField(request.body, currentField, currentLimit, current) !=
                WebApiCodecStatus::Success ||
            decodeCredentialField(request.body, replacementField, currentLimit,
                                  replacement) != WebApiCodecStatus::Success) {
            errorResponse(response, 422U, "invalid_credential",
                          "invalid credential");
            return true;
        }
        const auto fingerprint = mutationFingerprint(request);
        const auto reservation = sessions_.reserveMutation(
            *handle, timeSource_.monotonicMillis(), *sequence, fingerprint);
        if (reservation.status == MutationReservationStatus::ReplayOutcome &&
            reservation.outcome.has_value()) {
            response.statusCode = reservation.outcome->statusCode;
            response.contentType = reservation.outcome->contentType;
            response.body = reservation.outcome->body;
            return true;
        }
        if (reservation.status != MutationReservationStatus::Reserved) {
            errorResponse(response, 409U, "mutation_sequence_conflict",
                          "reload required");
            return true;
        }
        const auto epoch = application_.currentStorageEpoch();
        const auto status = epoch.has_value()
                                ? (pinChange
                                       ? authentication_.changeServicePin(
                                             *epoch, current, replacement, 0U,
                                             timeSource_.monotonicMillis())
                                       : authentication_.changeWebPassword(
                                             *epoch, current, replacement, 0U,
                                             timeSource_.monotonicMillis()))
                                : AuthBootstrapStatus::RecoveryRequired;
        switch (status) {
            case AuthBootstrapStatus::BootstrapAllowed:
                jsonResponse(response, 200U,
                             pinChange ? "{\"status\":\"changed\"}"
                                       : "{\"status\":\"changed\"}");
                break;
            case AuthBootstrapStatus::InvalidInput:
                errorResponse(response, 422U, "invalid_credential",
                              "invalid credential");
                break;
            case AuthBootstrapStatus::KdfUnavailable:
                errorResponse(response, 503U, "kdf_unavailable",
                              "authentication unavailable");
                break;
            case AuthBootstrapStatus::CommitOutcomeUnknown:
                errorResponse(response, 503U, "commit_indeterminate",
                              "authentication recovery required");
                break;
            case AuthBootstrapStatus::PersistenceFailure:
                errorResponse(response, 500U, "persistence_failure",
                              "credential change failed");
                break;
            default:
                errorResponse(response, 503U, "auth_recovery_required",
                              "authentication recovery required");
                break;
        }
        static_cast<void>(complete(*handle, *sequence, fingerprint, response));
        if (status == AuthBootstrapStatus::BootstrapAllowed) {
            sessions_.revokeAll();
        }
        return true;
    }
    if (request.path == "/internal/ui/network/mode" ||
        request.path == "/internal/ui/network/reconfigure") {
        if (!browserPolicy(request, response, true)) return true;
        const auto handle = session(request, response, true);
        if (!handle.has_value()) return true;
        if (!request.metadata.mutationSeq.has_value()) {
            errorResponse(response, 409U, "mutation_sequence_required", "mutation sequence required");
            return true;
        }
        const auto sequence = parseMutationSequence(*request.metadata.mutationSeq);
        if (!sequence.has_value()) {
            errorResponse(response, 431U, "mutation_sequence_invalid", "mutation sequence invalid");
            return true;
        }
        const auto fingerprint = mutationFingerprint(request);
        const auto reservation = sessions_.reserveMutation(
            *handle, timeSource_.monotonicMillis(), *sequence, fingerprint);
        if (reservation.status == MutationReservationStatus::ReplayOutcome && reservation.outcome.has_value()) {
            response.statusCode = reservation.outcome->statusCode;
            response.contentType = reservation.outcome->contentType;
            response.body = reservation.outcome->body;
            return true;
        }
        if (reservation.status != MutationReservationStatus::Reserved) {
            const auto status = reservation.status == MutationReservationStatus::Exhausted ? 503U : 409U;
            errorResponse(response, status, "mutation_sequence_conflict", "reload required");
            return true;
        }
        if (request.path == "/internal/ui/network/reconfigure") {
            const auto outcome = application_.beginHomeWifiReconfiguration();
            jsonResponse(response, networkStatusCode(outcome.status),
                         outcome.status == NetworkConfigurationStatus::Applied
                             ? "{\"status\":\"accepted\"}"
                             : "{\"status\":\"unavailable\"}");
        } else {
            device_platform::NetworkMode mode;
            std::optional<UserConfigurationRevision> expectedRevision;
            if (decodeNetworkMode(request.body, mode, expectedRevision) !=
                WebApiCodecStatus::Success) {
                errorResponse(response, 422U, "invalid_network_mode", "invalid network mode");
            } else {
                const auto outcome = application_.applyNetworkMode(
                    mode, expectedRevision, ChangeOriginKind::WebInterface);
                jsonResponse(response, networkStatusCode(outcome.status),
                             outcome.status == NetworkConfigurationStatus::Applied
                                 ? "{\"status\":\"accepted\"}"
                                 : "{\"status\":\"rejected\"}");
            }
        }
        static_cast<void>(complete(*handle, *sequence, fingerprint, response));
        return true;
    }
    return false;
}

}  // namespace fermentation
