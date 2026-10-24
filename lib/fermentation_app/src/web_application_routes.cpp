#include "web_application_routes.hpp"

#include <algorithm>
#include <cstdint>
#include <array>
#include <cstdio>
#include <string>
#include <utility>
#include <variant>

#include "web_json_codec.hpp"
#include "web_application_asset.hpp"
#include "crc32.hpp"
#include "configuration_text.hpp"
#include "configuration_limits.hpp"
#include "web_browser_policy.hpp"

namespace fermentation {
namespace {

constexpr char kRoutePath[] = "/internal/ui/run";
constexpr char kJsonContentType[] = "application/json; charset=utf-8";
constexpr char kStatusApiPath[] = "/api/v1/status";
constexpr char kTemperaturesApiPath[] = "/api/v1/temperatures";
constexpr char kAlertsApiPath[] = "/api/v1/alerts";
constexpr char kSettingsApiPath[] = "/api/v1/settings";
constexpr char kProgramsApiPath[] = "/api/v1/programs";
constexpr char kLoginPath[] = "/api/v1/login";
constexpr char kLogoutPath[] = "/api/v1/logout";
constexpr char kProvisionPath[] = "/api/v1/provision";
constexpr char kSessionCookieName[] = "FSSESSION=";
constexpr char kHtmlContentType[] = "text/html; charset=utf-8";
constexpr std::size_t kMaximumWebShellBytes = 2048U;
constexpr std::size_t kMaximumWebSetupShellBytes = 4096U;
constexpr std::size_t kMaximumWebAssetBytes = 16384U;
static_assert(sizeof(kWebApplicationAsset) - 1U + 1024U <=
              kMaximumWebAssetBytes);

const std::array<char, 24U>& assetPath() {
    static const auto path = [] {
        device_platform::Crc32IsoHdlc crc;
        static_cast<void>(crc.update(kWebApplicationAsset,
                                     sizeof(kWebApplicationAsset) - 1U));
        std::array<char, 24U> value{};
        std::snprintf(value.data(), value.size(), "/assets/app.%08x.js",
                      static_cast<unsigned>(crc.finalize()));
        return value;
    }();
    return path;
}

WebMutationOutcome outcome(std::uint16_t status, const char* body) {
    return {status, kJsonContentType, body};
}

constexpr auto kConflict = 409U;
constexpr auto kTooLarge = 413U;

WebMutationOutcome unavailable() {
    return replayOutcome(ReplayOutcomeCode::Unavailable);
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
    response.staticBodySuffix = {};
}

void setJsonResponse(device_platform::HttpResponse& response,
                     std::uint16_t status, const char* body) {
    response.statusCode = status;
    response.contentType = kJsonContentType;
    response.body = body;
    response.metadata = {};
    response.staticBodySuffix = {};
}

bool validWebMetadata(const device_platform::HttpRequest& request,
                      device_platform::HttpResponse& response) {
    if (device_platform::validateHttpRequestMetadata(request.metadata) ==
        device_platform::HttpMetadataValidation::Valid) {
        return true;
    }
    setJsonResponse(response, 400U, "{\"error\":\"invalid-request\"}");
    return false;
}

void setSessionCookie(device_platform::HttpResponse& response,
                      const std::string& value) {
    response.metadata.setCookie = std::string(kSessionCookieName) + value +
                                  "; HttpOnly; SameSite=Strict; Path=/";
}

void clearSessionCookie(device_platform::HttpResponse& response) {
    response.metadata.setCookie =
        "FSSESSION=; Max-Age=0; HttpOnly; SameSite=Strict; Path=/";
}

const char* authenticationStateName(WebAuthenticationState state) noexcept {
    switch (state) {
        case WebAuthenticationState::PasswordProtected:
            return "password-protected";
        case WebAuthenticationState::PasswordDisabled:
            return "password-disabled";
        case WebAuthenticationState::Unprovisioned:
            return "unprovisioned";
        case WebAuthenticationState::RecoveryRequired:
            return "recovery-required";
        case WebAuthenticationState::Indeterminate:
            return "indeterminate";
    }
    return "indeterminate";
}

const char* loginError(WebAuthenticationResultStatus status) noexcept {
    switch (status) {
        case WebAuthenticationResultStatus::Invalid:
            return "{\"error\":\"invalid-credentials\"}";
        case WebAuthenticationResultStatus::LockedOut:
            return "{\"error\":\"temporarily-locked\"}";
        case WebAuthenticationResultStatus::RecoveryRequired:
            return "{\"error\":\"recovery-required\"}";
        case WebAuthenticationResultStatus::KdfUnavailable:
            return "{\"error\":\"authentication-unavailable\"}";
        case WebAuthenticationResultStatus::Authenticated:
        case WebAuthenticationResultStatus::Disabled:
            break;
    }
    return "{\"error\":\"authentication-unavailable\"}";
}

std::optional<WebSessionHandle> findSession(
    const device_platform::HttpRequest& request, WebSessionManager& sessions,
    std::uint64_t nowMs) {
    if (!request.metadata.cookie.has_value()) return std::nullopt;
    const auto found = sessions.find(*request.metadata.cookie, nowMs);
    if (found.status != WebSessionStatus::Found || !found.handle.has_value()) {
        return std::nullopt;
    }
    return found.handle;
}

std::string retryAfterSeconds(std::uint64_t retryAfterMs) {
    const auto seconds =
        std::max<std::uint64_t>(1U, (retryAfterMs + 999U) / 1000U);
    return std::to_string(seconds);
}

// First-time access setup remains inline and independently bounded. The
// normal shell keeps session constants inline and loads the public asset.
std::string provisioningShell(std::string_view& staticBodySuffix) {
    static constexpr char kSuffix[] =
        "const L={de:{a:'Mit Passwort schuetzen (empfohlen)',b:'Passworts"
        "chutz deaktivieren',c:'Passwort',e:'Service-PIN',x:'Warnung: Ohn"
        "e Passwort kann jedes Geraet im lokalen Netz bedienen. Der Servi"
        "cebereich bleibt per PIN geschuetzt.',y:'Verstanden, Passwortsch"
        "utz deaktivieren',g:'Einrichten',ok:'Eingerichtet. Seite neu lad"
        "en.',na:'Noch nicht freigegeben: Web-Setup zuerst am Geraet frei"
        "geben, dann erneut versuchen.',ic:'Passwort oder PIN abgelehnt.'"
        ",ap:'Bereits eingerichtet.',rc:'Lokale Wiederherstellung noetig."
        "',fl:'Einrichtung fehlgeschlagen.'},en:{a:'Protect with a passwo"
        "rd (recommended)',b:'Disable password protection',c:'Password',e"
        ":'Service PIN',x:'Warning: without a password every device in th"
        "e local network can operate this device. The service area stays "
        "PIN protected.',y:'I understand, disable password protection',g:"
        "'Set up',ok:'Set up. Reload the page.',na:'Not released yet: rel"
        "ease web setup on the device first, then retry.',ic:'Password or"
        " PIN rejected.',ap:'Already set up.',rc:'Local recovery needed.'"
        ",fl:'Setup failed.'},es:{a:'Proteger con contrasena (recomendado"
        ")',b:'Desactivar la proteccion',c:'Contrasena',e:'PIN de servici"
        "o',x:'Aviso: sin contrasena cualquier dispositivo de la red loca"
        "l puede operar el equipo. El servicio sigue protegido por PIN.',"
        "y:'Entendido, desactivar la proteccion',g:'Configurar',ok:'Listo"
        ". Recargue la pagina.',na:'Aun no permitido: permita la configur"
        "acion web en el equipo y reintente.',ic:'Contrasena o PIN rechaz"
        "ados.',ap:'Ya configurado.',rc:'Requiere recuperacion local.',fl"
        ":'Fallo la configuracion.'}};const q=i=>document.getElementById("
        "i),sw=q('lang'),ok=/^(de|en|es)$/;let T,s,M;try{s=localStorage.l"
        "ang}catch(e){}function sl(c){c=ok.test(c)?c:'en';T=L[c];document"
        ".documentElement.lang=sw.value=c;for(const i of ['a','b','c','e'"
        ",'x','y','g'])q(i).textContent=T[i];q('m').textContent=M?T[M]:''"
        ";return c;}function sm(k){M=k;q('m').textContent=T[k];}sl(ok.tes"
        "t(s)?s:(navigator.language||'en').slice(0,2));sw.onchange=()=>{c"
        "onst c=sl(sw.value);try{localStorage.lang=c}catch(e){}};const d="
        "()=>document.querySelector('input[name=o]:checked').value==='d';"
        "const u=()=>{q('lw').hidden=d();q('x').hidden=q('lk').hidden=!d("
        ");};document.querySelectorAll('input[name=o]').forEach(i=>i.onch"
        "ange=u);q('f').onsubmit=async e=>{e.preventDefault();if(d()&&!q("
        "'k').checked){sm('y');return;}const b=d()?{mode:'disable',servic"
        "ePin:q('n').value,confirmDisable:true}:{mode:'protect',password:"
        "q('w').value,servicePin:q('n').value};try{const r=await fetch('/"
        "api/v1/provision',{method:'POST',credentials:'same-origin',heade"
        "rs:{'Content-Type':'application/json'},body:JSON.stringify(b)});"
        "const j=await r.json().catch(()=>({}));sm(r.ok?'ok':j.error==='p"
        "rovisioning-not-allowed'?'na':r.status===409?'ap':r.status===422"
        "?'ic':j.error==='recovery-required'?'rc':'fl');if(r.ok){q('w').v"
        "alue=q('n').value='';}}catch(x){sm('fl');}};u();</script></body>"
        "</html>";
    staticBodySuffix = kSuffix;
    return "<!doctype html><html lang=\"en\"><head><meta "
           "charset=\"utf-8\"><meta"
           " name=\"viewport\" "
           "content=\"width=device-width,initial-scale=1\"><t"
           "itle>Fermentation</title><style>body{font-family:system-ui;max-w"
           "idth:48rem;margin:auto;padding:1rem}form{display:grid;gap:.6rem}"
           "input,button,select{font:inherit;padding:.45rem}#m{font-weight:6"
           "00}</style></head><body><main><select id=lang aria-label=Languag"
           "e><option value=de>Deutsch<option value=en>English<option value="
           "es>Español</select><h1>Fermentation</h1><form id=\"f\"><label><inp"
           "ut type=\"radio\" name=\"o\" value=\"p\" checked> <span "
           "id=\"a\"></span>"
           "</label><label><input type=\"radio\" name=\"o\" value=\"d\"> <span "
           "id="
           "\"b\"></span></label><label id=\"lw\"><span id=\"c\"></span> "
           "<input id"
           "=\"w\" type=\"password\" "
           "autocomplete=\"new-password\"></label><label>"
           "<span id=\"e\"></span> <input id=\"n\" type=\"password\" "
           "inputmode=\"nu"
           "meric\" autocomplete=\"off\"></label><p id=\"x\" hidden></p><label "
           "id"
           "=\"lk\" hidden><input id=\"k\" type=\"checkbox\"> <span "
           "id=\"y\"></span>"
           "</label><button id=\"g\"></button><p "
           "id=\"m\"></p></form></main><scr"
           "ipt>";
}

std::string webShell(WebAuthenticationState state, const std::string& csrfToken,
                     bool authenticated, std::uint64_t mutationSequence,
                     std::string_view& staticBodySuffix) {
    if (state == WebAuthenticationState::Unprovisioned) {
        return provisioningShell(staticBodySuffix);
    }
    // This is intentionally a small static shell. It has no framework,
    // generated bundle, history or chart. The only mutation controls are the
    // run actions below. All three API requests are repeated together so
    // a reconnect receives a full snapshot.
    std::string html;
    html.reserve(kMaximumWebShellBytes);
    html +=
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta"
        " name=\"viewport\" content=\"width=device-width,initial-scale=1\"><t"
        "itle>Fermentation</title><style>body{font-family:system-ui;max-width"
        ":48rem;margin:auto;padding:1rem}nav{display:flex;flex-wrap:wrap;gap:."
        "5rem}"
        "nav[hidden]{display:none}main{display:grid;gap:1rem}section{b"
        "order:1px solid #bbb;border-radius:.5rem;padding:1rem}button,input,s"
        "elect{font:inherit;padding:.45rem;min-height:2.75rem}#connection{fon"
        "t-weight:600}#cols{display:grid;gap:1rem}@media(min-width:40rem){#co"
        "ls{grid-template-columns:1fr 1fr}}.x{opacity:.5}.safety-fault,.decis"
        "ion-required{color:#b00}</style></head><body><main>"
        "<select id=lang aria-label=Language><option value=de>Deutsch<option "
        "value=en>English<option value=es>Español</select>"
        "<h1>Fermentation<"
        "/h1><p id=\"auth-state\"></p><p id=\"warning\" hidden></p><form id=\""
        "login\"><label><span id=\"password-label\">Password</span> <input id"
        "=\"password\"type=\"password\" autocomplete=\"current-password\"></l"
        "abel><button>Login</button></form><button id=\"logout\" hidden>Logou"
        "t</button><section><div id=\"connection\">Offline</div></section><se"
        "ction id=\"board\" hidden><h2 id=\"mode\"></h2><div id=\"cols\"><div"
        " id=\"temps\"></div><div id=\"alerts\"></div></div></section><sectio"
        "n id=\"ops\" hidden><button data-a=\"ack\"></button> <button data-a="
        "\"mute\"></button> <button data-a=\"stop\"></button> <button data-a="
        "\"done\"></button> <select id=\"pg\"></select> <button data-a=\"star"
        "t\"></button> <span id=\"note\"></span></section></main><script>cons"
        "t S=";
    html += "'";
    html += csrfToken;
    html += "';const A='";
    html += authenticationStateName(state);
    html += "';const U=";
    html += authenticated ? "true" : "false";
    html += ";let Q=";
    html += std::to_string(mutationSequence);
    html +=
        ";</script><p id=asset-error hidden>Script unavailable / Skript nicht "
        "verfuegbar / Script no disponible. Reload.</p><script src=";
    html += assetPath().data();
    staticBodySuffix =
        " onerror=\"document.querySelector('#asset-error').hidden=false\"></"
        "script></body></html>";
    return html;
}

WebMutationOutcome requestError(std::uint16_t status, const char* body) {
    return outcome(status, body);
}

WebMutationOutcome reservationError(MutationReservationStatus status) {
    switch (status) {
        case MutationReservationStatus::InvalidSession:
            return requestError(401U, "{\"error\":\"session-required\"}");
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

ReplayOutcomeCode WebRunMutationHandler::projectRequestStatus(
    FermentationApplicationRequestStatus status) {
    switch (status) {
        case FermentationApplicationRequestStatus::Prepared:
            return ReplayOutcomeCode::Unavailable;
        case FermentationApplicationRequestStatus::StaleProgramCatalog:
            return ReplayOutcomeCode::Stale;
        case FermentationApplicationRequestStatus::ProgramUnavailable:
        case FermentationApplicationRequestStatus::InvalidInput:
            return ReplayOutcomeCode::Rejected;
        case FermentationApplicationRequestStatus::NotInitialized:
        case FermentationApplicationRequestStatus::Unavailable:
        case FermentationApplicationRequestStatus::Overflow:
            return ReplayOutcomeCode::Unavailable;
    }
    return ReplayOutcomeCode::Unavailable;
}

ReplayOutcomeCode WebRunMutationHandler::projectCommandResult(
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
                           ? ReplayOutcomeCode::Applied
                           : ReplayOutcomeCode::Unavailable;
            case RunPersistenceResultStatus::Busy:
                return owningWithCategory(result, Category::Busy)
                           ? ReplayOutcomeCode::Busy
                           : ReplayOutcomeCode::Unavailable;
            case RunPersistenceResultStatus::StaleDecision:
                return owningWithCategory(result, Category::Rejected)
                           ? ReplayOutcomeCode::Stale
                           : ReplayOutcomeCode::Unavailable;
            case RunPersistenceResultStatus::NotInitialized:
            case RunPersistenceResultStatus::RecoveryPending:
            case RunPersistenceResultStatus::PersistenceIndeterminate:
            case RunPersistenceResultStatus::PersistenceCommittedApplyFailed:
            case RunPersistenceResultStatus::Blocked:
                return ReplayOutcomeCode::Unavailable;
            case RunPersistenceResultStatus::WriteFailed:
                return owningWithCategory(result, Category::Rejected)
                           ? ReplayOutcomeCode::WriteFailed
                           : ReplayOutcomeCode::Unavailable;
            case RunPersistenceResultStatus::CapacityExceeded:
                return owningWithCategory(result, Category::Rejected)
                           ? ReplayOutcomeCode::TooLarge
                           : ReplayOutcomeCode::Unavailable;
            case RunPersistenceResultStatus::NotEligible:
            case RunPersistenceResultStatus::NotAllowedInState:
            case RunPersistenceResultStatus::InvalidDecision:
            case RunPersistenceResultStatus::TimeMismatch:
            case RunPersistenceResultStatus::TimeWentBackwards:
            case RunPersistenceResultStatus::CounterOverflow:
            case RunPersistenceResultStatus::NotDue:
            case RunPersistenceResultStatus::NoActiveRun:
                return owningWithCategory(result, Category::Rejected)
                           ? ReplayOutcomeCode::Rejected
                           : ReplayOutcomeCode::Unavailable;
        }
        return ReplayOutcomeCode::Unavailable;
    }

    if (const auto* status = std::get_if<CommandStatus>(&result.detail)) {
        switch (*status) {
            case CommandStatus::Applied:
            case CommandStatus::NoChange:
            case CommandStatus::AlreadyProcessed:
                return owningWithCategory(result, Category::Accepted)
                           ? ReplayOutcomeCode::Applied
                           : ReplayOutcomeCode::Unavailable;
            case CommandStatus::Proposed:
                // An accepted proposal is DecisionOnly, never a web success.
                return ReplayOutcomeCode::Unavailable;
            case CommandStatus::NotConfirmed:
                return result.phase ==
                                   FermentationUiCommandPhase::DecisionOnly &&
                               result.category == Category::ConfirmationRequired
                           ? ReplayOutcomeCode::ConfirmationRequired
                           : ReplayOutcomeCode::Unavailable;
            case CommandStatus::StaleState:
                return decisionOnlyRejected(result)
                           ? ReplayOutcomeCode::Stale
                           : ReplayOutcomeCode::Unavailable;
            case CommandStatus::ContextMissing:
                return ReplayOutcomeCode::Unavailable;
            case CommandStatus::CapacityReached:
                return decisionOnlyRejected(result)
                           ? ReplayOutcomeCode::TooLarge
                           : ReplayOutcomeCode::Unavailable;
            case CommandStatus::NotAllowedInState:
            case CommandStatus::InvalidInput:
            case CommandStatus::SafetyRejected:
                return decisionOnlyRejected(result)
                           ? ReplayOutcomeCode::Rejected
                           : ReplayOutcomeCode::Unavailable;
        }
        return ReplayOutcomeCode::Unavailable;
    }

    if (const auto* status = std::get_if<DecisionStatus>(&result.detail)) {
        if (result.phase != FermentationUiCommandPhase::DecisionOnly ||
            result.category != Category::Rejected) {
            return ReplayOutcomeCode::Unavailable;
        }
        switch (*status) {
            case DecisionStatus::Proposed:
                return ReplayOutcomeCode::Unavailable;
            case DecisionStatus::NoTransition:
            case DecisionStatus::Rejected:
            case DecisionStatus::InvalidInput:
            case DecisionStatus::TimeWentBackwards:
                return ReplayOutcomeCode::Rejected;
        }
    }

    // Configuration, network and unknown details are not valid outcomes for
    // this run-only route. A category alone can never authorize success.
    return ReplayOutcomeCode::Unavailable;
}

ReplayOutcomeCode WebRunMutationHandler::projectConfigurationResult(
    const ApplicationConfigurationChangeResult& result) {
    using Preview = ConfigurationPreviewStatus;
    using Commit = ConfigurationCommitStatus;
    switch (result.preview) {
        case Preview::Success:
            break;
        case Preview::ConfigurationRuntimeUnavailable:
        case Preview::PreviewNotFound:
            return ReplayOutcomeCode::Unavailable;
        case Preview::ConfigurationModelBudgetBusy:
        case Preview::PreviewSuperseded:
            return ReplayOutcomeCode::Busy;
        case Preview::InvalidCandidate:
        case Preview::NotAllowed:
            return ReplayOutcomeCode::Rejected;
        case Preview::StateChanged:
            return ReplayOutcomeCode::Stale;
    }
    if (result.preview != Preview::Success)
        return ReplayOutcomeCode::Unavailable;
    switch (result.commit) {
        case Commit::Activated:
        case Commit::NoChange:
            return ReplayOutcomeCode::Applied;
        case Commit::ConfigurationMutationBusy:
        case Commit::PreviewSuperseded:
            return ReplayOutcomeCode::Busy;
        case Commit::ConfigurationConflictFailure:
            return ReplayOutcomeCode::Stale;
        case Commit::ConfigurationValidationFailure:
            return ReplayOutcomeCode::Rejected;
        case Commit::PersistenceFailure:
        case Commit::CapacityFailure:
            return ReplayOutcomeCode::WriteFailed;
        case Commit::ConfigurationCommitIndeterminate:
        case Commit::ConfigurationRuntimeFailure:
        case Commit::PreviewNotFound:
        case Commit::ReadyForConfirmation:
            return ReplayOutcomeCode::Unavailable;
    }
    return ReplayOutcomeCode::Unavailable;
}

bool WebRunMutationHandler::handle(const device_platform::HttpRequest& request,
                                   device_platform::HttpResponse& response) {
    response.staticBodySuffix = {};
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

    WebMutationDto dto;
    const auto decodeStatus = decodeWebMutation(request.body, dto);
    if (decodeStatus != WebRunMutationDecodeStatus::Success) {
        const auto tooLarge =
            decodeStatus == WebRunMutationDecodeStatus::TooLarge;
        setResponse(response,
                    requestError(tooLarge ? kTooLarge : 400U,
                                 tooLarge ? "{\"error\":\"request-too-large\"}"
                                          : "{\"error\":\"invalid-json\"}"));
        return true;
    }

    ReplayDigest digest{};
    if (!sessions_.mutationDigest(request, digest)) {
        setResponse(response, unavailable());
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

    const auto reservation =
        sessions_.reserveMutation(*session.handle, nowMs, *sequence, digest);
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

    ReplayOutcomeCode projected = ReplayOutcomeCode::Unavailable;
    if (const auto* config = std::get_if<WebConfigurationMutationDto>(&dto)) {
        projected =
            projectConfigurationResult(application_.applyWebConfigurationChange(
                config->change, config->expected));
    } else {
        projected = applyRunMutation(std::get<WebRunMutationDto>(dto), nowMs);
    }
    if (!sessions_.completeMutation(*session.handle,
                                    timeSource_.monotonicMillis(),
                                    *reservation.sequence, digest, projected)) {
        setResponse(response, unavailable());
        return true;
    }
    setResponse(response, replayOutcome(projected));
    return true;
}

ReplayOutcomeCode WebRunMutationHandler::applyRunMutation(
    const WebRunMutationDto& run, std::uint64_t nowMs) {
    ReplayOutcomeCode projected = ReplayOutcomeCode::Unavailable;
    FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::WebInterface;
    context.monotonicMillis = nowMs;
    context.expected = run.expected;

    auto prepared = application_.prepareEnvelope(context, run.intent);
    if (prepared.status != FermentationApplicationRequestStatus::Prepared ||
        !prepared.request.has_value()) {
        projected = projectRequestStatus(prepared.status);
    } else {
        const auto confirmedStatus =
            application_.confirmPreparedInPlace(prepared);
        if (confirmedStatus != FermentationApplicationRequestStatus::Prepared ||
            !prepared.request.has_value()) {
            projected = projectRequestStatus(confirmedStatus);
        } else {
            projected = projectCommandResult(
                application_.applyConfirmedPrepared(*prepared.request));
        }
    }
    return projected;
}

bool WebReadOnlyApiHandler::handle(const device_platform::HttpRequest& request,
                                   device_platform::HttpResponse& response) {
    response.staticBodySuffix = {};
    const bool status = request.path == kStatusApiPath;
    const bool temperatures = request.path == kTemperaturesApiPath;
    const bool alerts = request.path == kAlertsApiPath;
    const bool settings = request.path == kSettingsApiPath;
    const bool programs = request.path == kProgramsApiPath;
    const bool detail = request.path.compare(0U, sizeof(kProgramsApiPath),
                                             "/api/v1/programs/") == 0;
    if (!status && !temperatures && !alerts && !programs && !detail &&
        !settings)
        return false;
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

    std::string body;
    bool encoded = false;
    if (settings) {
        const auto snapshot = application_.webSettings();
        encoded = snapshot.has_value() && encodeWebApiSettings(*snapshot, body);
    } else if (detail) {
        const auto id = request.path.substr(sizeof(kProgramsApiPath));
        if (validateLowercaseIdentifier(
                id, configuration_limits::kMinimumProgramIdBytes,
                configuration_limits::kMaximumProgramIdBytes) !=
            ConfigurationTextStatus::Success) {
            setJsonResponse(response, 400U,
                            "{\"error\":\"invalid-program-id\"}");
            return true;
        }
        const auto source = application_.uiPresentationSource();
        if (source.has_value()) {
            const auto& entries = source->programCatalog.programs;
            const auto entry = std::find_if(
                entries.begin(), entries.end(), [&id](const auto& value) {
                    return value.program.id == id && value.program.installed &&
                           value.program.enabled;
                });
            if (entry == entries.end()) {
                setJsonResponse(response, 404U,
                                "{\"error\":\"program-not-found\"}");
                return true;
            }
            encoded = encodeWebApiProgram(*source, *entry, body);
        }
    } else if (programs) {
        // An unavailable catalog is 503, never an empty list.
        const auto source = application_.uiPresentationSource();
        encoded = source.has_value() && encodeWebApiPrograms(*source, body);
    } else {
        const auto snapshot = application_.uiSnapshot();
        encoded = status ? encodeWebApiStatus(
                               snapshot, application_.deviceInfoView(), body)
                  : temperatures ? encodeWebApiTemperatures(snapshot, body)
                                 : encodeWebApiAlerts(snapshot, body);
    }
    if (!encoded || body.size() > kMaximumWebApiResponseBodyBytes) {
        setResponse(response, unavailable());
        return true;
    }
    response.statusCode = 200U;
    response.contentType = kJsonContentType;
    response.body = std::move(body);
    response.metadata = {};
    response.staticBodySuffix = {};
    return true;
}

bool WebRouteDispatcher::handleAsset(
    const device_platform::HttpRequest& request,
    device_platform::HttpResponse& response) {
    if (request.path != assetPath().data()) return false;
    if (request.method != "GET") {
        setJsonResponse(response, 405U, "{\"error\":\"method-not-allowed\"}");
        return true;
    }
    if (!validWebMetadata(request, response)) return true;
    if (!request.body.empty()) {
        setJsonResponse(response, 400U, "{\"error\":\"body-not-allowed\"}");
        return true;
    }
    response.body.clear();
#ifdef APP_PROFILE_NATIVE
    response.body.append(extraAssetBytesForTest_, 'x');
#endif
    if (sizeof(kWebApplicationAsset) - 1U + response.body.size() >
        kMaximumWebAssetBytes) {
        setJsonResponse(response, 503U,
                        "{\"error\":\"web-asset-unavailable\"}");
        return true;
    }
    response.statusCode = 200U;
    response.contentType = "text/javascript; charset=utf-8";
    response.metadata = {};
    response.metadata.cache =
        device_platform::HttpResponseCache::ImmutableAsset;
    response.staticBodySuffix = kWebApplicationAsset;
    return true;
}

bool WebRouteDispatcher::handleShell(
    const device_platform::HttpRequest& request,
    device_platform::HttpResponse& response) {
    if (request.method != "GET") {
        setJsonResponse(response, 405U, "{\"error\":\"method-not-allowed\"}");
        return true;
    }

    const auto state = application_.webAuthenticationState();
    std::optional<WebSessionHandle> session;
    std::string csrf;
    if (state == WebAuthenticationState::PasswordDisabled ||
        state == WebAuthenticationState::PasswordProtected) {
        session =
            findSession(request, sessions_, timeSource_.monotonicMillis());
        if (!session.has_value() &&
            state == WebAuthenticationState::PasswordDisabled) {
            // Disabled mode: the anonymous session is issued under the same
            // trust-boundary recheck as a login (no KDF run in this mode).
            const auto authentication = application_.authenticateWebPassword(
                std::string{}, timeSource_.monotonicMillis());
            const auto issued = application_.issueWebSession(
                authentication, timeSource_.monotonicMillis());
            if (issued.status == WebSessionIssueStatus::Created) {
                session = issued.session.handle;
                csrf = issued.session.csrfToken;
                setSessionCookie(response, issued.session.cookieValue);
            }
        }
        if (session.has_value() && csrf.empty()) {
            csrf = sessions_.csrfToken(*session, timeSource_.monotonicMillis())
                       .value_or("");
        }
    }

    std::uint64_t mutationSequence = 0U;
    if (session.has_value()) {
        const auto sequence =
            sessions_.mutationSequence(*session, timeSource_.monotonicMillis());
        if (sequence.state == MutationSequenceState::Available) {
            mutationSequence = sequence.nextMutationSeq;
        }
    }

    response.statusCode = 200U;
    response.contentType = kHtmlContentType;
    response.body = webShell(state, csrf, session.has_value(), mutationSequence,
                             response.staticBodySuffix);
#ifdef APP_PROFILE_NATIVE
    response.body.append(extraShellBytesForTest_, 'x');
#endif
    if (response.body.size() + response.staticBodySuffix.size() >
        (state == WebAuthenticationState::Unprovisioned
             ? kMaximumWebSetupShellBytes
             : kMaximumWebShellBytes)) {
        setJsonResponse(response, 503U,
                        "{\"error\":\"web-shell-unavailable\"}");
    }
    return true;
}

bool WebRouteDispatcher::handleLogin(
    const device_platform::HttpRequest& request,
    device_platform::HttpResponse& response) {
    if (request.method != "POST") {
        setJsonResponse(response, 405U, "{\"error\":\"method-not-allowed\"}");
        return true;
    }
    if (!validWebMetadata(request, response)) return true;
    if (!request.metadata.contentType.has_value() ||
        !web_browser_policy::exactJsonContentType(
            *request.metadata.contentType)) {
        setJsonResponse(response, 415U,
                        "{\"error\":\"unsupported-media-type\"}");
        return true;
    }
    if (!web_browser_policy::sameOrigin(request)) {
        setJsonResponse(response, 403U, "{\"error\":\"origin-rejected\"}");
        return true;
    }

    WebLoginDto dto;
    const auto decoded = decodeWebLogin(request.body, dto);
    if (decoded != WebLoginDecodeStatus::Success) {
        setJsonResponse(response,
                        decoded == WebLoginDecodeStatus::TooLarge ? 413U : 400U,
                        decoded == WebLoginDecodeStatus::TooLarge
                            ? "{\"error\":\"request-too-large\"}"
                            : "{\"error\":\"invalid-json\"}");
        return true;
    }

    const auto state = application_.webAuthenticationState();
    if (state == WebAuthenticationState::Unprovisioned) {
        setJsonResponse(response, 503U,
                        "{\"error\":\"provisioning-required\"}");
        return true;
    }
    if (state == WebAuthenticationState::RecoveryRequired ||
        state == WebAuthenticationState::Indeterminate) {
        setJsonResponse(response, 503U, "{\"error\":\"recovery-required\"}");
        return true;
    }

    // The Application wrapper deliberately releases its shared gate before
    // entering the potentially multi-second PSA PBKDF2 operation. In
    // password-disabled mode it returns Disabled without any KDF run.
    const auto authentication = application_.authenticateWebPassword(
        dto.password, timeSource_.monotonicMillis());
    if (authentication.status != WebAuthenticationResultStatus::Authenticated &&
        authentication.status != WebAuthenticationResultStatus::Disabled) {
        const auto status =
            authentication.status == WebAuthenticationResultStatus::Invalid
                ? 401U
            : authentication.status == WebAuthenticationResultStatus::LockedOut
                ? 429U
                : 503U;
        setJsonResponse(response, status, loginError(authentication.status));
        if (authentication.status == WebAuthenticationResultStatus::LockedOut) {
            response.metadata.retryAfter =
                retryAfterSeconds(authentication.retryAfterMs);
        }
        return true;
    }

    // Recheck and session creation are atomic against trust-boundary
    // revocations (see FermentationApplication::issueWebSession).
    const auto issued = application_.issueWebSession(
        authentication, timeSource_.monotonicMillis());
    if (issued.status == WebSessionIssueStatus::TrustBoundaryChanged) {
        setJsonResponse(response, 409U,
                        "{\"error\":\"authentication-changed\"}");
        return true;
    }
    if (issued.status != WebSessionIssueStatus::Created) {
        setJsonResponse(response, 503U, "{\"error\":\"session-unavailable\"}");
        return true;
    }
    const auto& created = issued.session;
    response.statusCode = 200U;
    response.contentType = kJsonContentType;
    response.body =
        authentication.status == WebAuthenticationResultStatus::Disabled
            ? "{\"authenticated\":true,\"csrfToken\":\"" + created.csrfToken +
                  "\",\"passwordProtection\":\"disabled\"}"
            : "{\"authenticated\":true,\"csrfToken\":\"" + created.csrfToken +
                  "\",\"passwordProtection\":\"enabled\"}";
    response.metadata = {};
    response.staticBodySuffix = {};
    setSessionCookie(response, created.cookieValue);
    return true;
}

bool WebRouteDispatcher::handleProvision(
    const device_platform::HttpRequest& request,
    device_platform::HttpResponse& response) {
    if (request.method != "POST") {
        setJsonResponse(response, 405U, "{\"error\":\"method-not-allowed\"}");
        return true;
    }
    if (!validWebMetadata(request, response)) return true;
    if (!request.metadata.contentType.has_value() ||
        !web_browser_policy::exactJsonContentType(
            *request.metadata.contentType)) {
        setJsonResponse(response, 415U,
                        "{\"error\":\"unsupported-media-type\"}");
        return true;
    }
    if (!web_browser_policy::sameOrigin(request)) {
        setJsonResponse(response, 403U, "{\"error\":\"origin-rejected\"}");
        return true;
    }

    // First-time setup has no session, CSRF token or replay sequence; the
    // authorization is the local release window owned by the Application.
    WebProvisionDto dto;
    const auto decoded = decodeWebProvision(request.body, dto);
    if (decoded != WebProvisionDecodeStatus::Success) {
        if (decoded == WebProvisionDecodeStatus::TooLarge) {
            setJsonResponse(response, 413U,
                            "{\"error\":\"request-too-large\"}");
        } else {
            setJsonResponse(response, 400U, "{\"error\":\"invalid-json\"}");
        }
        return true;
    }

    switch (application_.provisionWebAccess(dto.mode, dto.password,
                                            dto.servicePin)) {
        case WebProvisionStatus::Provisioned:
            setJsonResponse(response, 200U,
                            dto.mode == WebProvisionMode::Protect
                                ? "{\"provisioned\":true,"
                                  "\"passwordProtection\":\"enabled\"}"
                                : "{\"provisioned\":true,"
                                  "\"passwordProtection\":\"disabled\"}");
            break;
        case WebProvisionStatus::NotAllowed:
            setJsonResponse(response, 403U,
                            "{\"error\":\"provisioning-not-allowed\"}");
            break;
        case WebProvisionStatus::AlreadyProvisioned:
            setJsonResponse(response, 409U,
                            "{\"error\":\"already-provisioned\"}");
            break;
        case WebProvisionStatus::InvalidCredentials:
            setJsonResponse(response, 422U,
                            "{\"error\":\"invalid-credentials\"}");
            break;
        case WebProvisionStatus::Failed:
            setJsonResponse(response, 503U,
                            "{\"error\":\"provisioning-failed\"}");
            break;
        case WebProvisionStatus::RecoveryRequired:
        default:
            setJsonResponse(response, 503U,
                            "{\"error\":\"recovery-required\"}");
            break;
    }
    return true;
}

bool WebRouteDispatcher::handleLogout(
    const device_platform::HttpRequest& request,
    device_platform::HttpResponse& response) {
    if (request.method != "POST") {
        setJsonResponse(response, 405U, "{\"error\":\"method-not-allowed\"}");
        return true;
    }
    if (!validWebMetadata(request, response)) return true;
    if (!request.metadata.contentType.has_value() ||
        !web_browser_policy::exactJsonContentType(
            *request.metadata.contentType)) {
        setJsonResponse(response, 415U,
                        "{\"error\":\"unsupported-media-type\"}");
        return true;
    }
    if (!request.body.empty()) {
        setJsonResponse(response, 400U, "{\"error\":\"body-not-allowed\"}");
        return true;
    }
    if (!web_browser_policy::sameOrigin(request)) {
        setJsonResponse(response, 403U, "{\"error\":\"origin-rejected\"}");
        return true;
    }
    const auto session =
        findSession(request, sessions_, timeSource_.monotonicMillis());
    if (!session.has_value()) {
        setJsonResponse(response, 401U, "{\"error\":\"session-required\"}");
        return true;
    }
    if (!request.metadata.csrfToken.has_value() ||
        !sessions_.validateCsrf(*session, *request.metadata.csrfToken,
                                timeSource_.monotonicMillis())) {
        setJsonResponse(response, 403U, "{\"error\":\"csrf-rejected\"}");
        return true;
    }
    sessions_.revokeServiceLease(*session);
    sessions_.revoke(*session);
    setJsonResponse(response, 200U, "{\"loggedOut\":true}");
    clearSessionCookie(response);
    return true;
}

bool WebRouteDispatcher::handleReadOnly(
    const device_platform::HttpRequest& request,
    device_platform::HttpResponse& response) {
    if (!validWebMetadata(request, response)) return true;
    const auto state = application_.webAuthenticationState();
    if (state == WebAuthenticationState::Unprovisioned ||
        state == WebAuthenticationState::RecoveryRequired ||
        state == WebAuthenticationState::Indeterminate) {
        setJsonResponse(response, 503U,
                        "{\"error\":\"authentication-unavailable\"}");
        return true;
    }
    const auto session =
        findSession(request, sessions_, timeSource_.monotonicMillis());
    if (!session.has_value()) {
        setJsonResponse(response, 401U, "{\"error\":\"session-required\"}");
        return true;
    }
    // No touch(): read-only polling must not renew the 30-minute idle limit.
    return readOnly_.handle(request, response);
}

bool WebRouteDispatcher::handleRun(const device_platform::HttpRequest& request,
                                   device_platform::HttpResponse& response) {
    const auto state = application_.webAuthenticationState();
    if (state == WebAuthenticationState::Unprovisioned ||
        state == WebAuthenticationState::RecoveryRequired ||
        state == WebAuthenticationState::Indeterminate) {
        setJsonResponse(response, 503U,
                        "{\"error\":\"authentication-unavailable\"}");
        return true;
    }
    return run_.handle(request, response);
}

bool WebRouteDispatcher::handle(const device_platform::HttpRequest& request,
                                device_platform::HttpResponse& response) {
    response.staticBodySuffix = {};
    response.metadata = {};
    const bool networkPath = request.path == "/api/network/status" ||
                             request.path == "/api/network/scan" ||
                             request.path == "/api/network/candidate";
    if (application_.networkSetupFlowActive()) {
        // During setup the existing setup page and its network routes own the
        // surface. Normal Web routes are not allowed to displace it.
        if (request.path == "/" || networkPath) {
            return networkSetupRoutes_.handle(request, response);
        }
        return false;
    }
    if (networkPath) return networkSetupRoutes_.handle(request, response);
    if (request.path.compare(0U, 8U, "/assets/") == 0)
        return handleAsset(request, response);
    if (request.path == "/") return handleShell(request, response);
    if (request.path == kLoginPath) return handleLogin(request, response);
    if (request.path == kLogoutPath) return handleLogout(request, response);
    if (request.path == kProvisionPath) {
        return handleProvision(request, response);
    }
    if (request.path == kStatusApiPath ||
        request.path == kTemperaturesApiPath ||
        request.path == kAlertsApiPath || request.path == kProgramsApiPath ||
        request.path == kSettingsApiPath ||
        request.path.compare(0U, sizeof(kProgramsApiPath),
                             "/api/v1/programs/") == 0) {
        return handleReadOnly(request, response);
    }
    if (request.path == kRoutePath) return handleRun(request, response);
    return false;
}

}  // namespace fermentation
