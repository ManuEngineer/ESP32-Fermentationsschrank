#include "web_application_routes.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
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
constexpr char kLoginPath[] = "/api/v1/login";
constexpr char kLogoutPath[] = "/api/v1/logout";
constexpr char kProvisionPath[] = "/api/v1/provision";
constexpr char kSessionCookieName[] = "FSSESSION=";
constexpr char kHtmlContentType[] = "text/html; charset=utf-8";
constexpr std::size_t kMaximumWebShellBytes = 4096U;

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
}

void setJsonResponse(device_platform::HttpResponse& response,
                     std::uint16_t status, const char* body) {
    response.statusCode = status;
    response.contentType = kJsonContentType;
    response.body = body;
    response.metadata = {};
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

// Shell for the Unprovisioned state: the first-time web access setup form.
// Login, status polling and the session controls have no function in this
// state (the API answers 503), so this page replaces them within the same
// shell size limit. The form only collects input and posts it to
// POST /api/v1/provision; the Application decides everything (release
// window, credential rules, state). It issues no session.
std::string provisioningShell() {
    return "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,"
           "initial-scale=1\"><title>Fermentation</"
           "title><style>body{font-family:"
           "system-ui;max-width:48rem;margin:auto;padding:1rem}form{display:"
           "grid;"
           "gap:.6rem}input,button{font:inherit;padding:.45rem}#m{font-weight:"
           "600}"
           "</style></head><body><main><h1>Fermentation</h1><form id=\"f\">"
           "<label><input type=\"radio\" name=\"o\" value=\"p\" checked> "
           "<span id=\"a\"></span></label><label><input type=\"radio\" "
           "name=\"o\" value=\"d\"> <span id=\"b\"></span></label>"
           "<label id=\"lw\"><span id=\"c\"></span> <input id=\"w\" "
           "type=\"password\" autocomplete=\"new-password\"></label>"
           "<label><span id=\"e\"></span> <input id=\"n\" type=\"password\" "
           "inputmode=\"numeric\" autocomplete=\"off\"></label>"
           "<p id=\"x\" hidden></p><label id=\"lk\" hidden><input id=\"k\" "
           "type=\"checkbox\"> <span id=\"y\"></span></label>"
           "<button id=\"g\"></button><p id=\"m\"></p></form></main><script>"
           "const L={de:{a:'Mit Passwort schuetzen "
           "(empfohlen)',b:'Passwortschutz "
           "deaktivieren',c:'Passwort',e:'Service-PIN',x:'Warnung: Ohne "
           "Passwort "
           "kann jedes Geraet im lokalen Netz bedienen. Der Servicebereich "
           "bleibt "
           "per PIN geschuetzt.',y:'Verstanden, Passwortschutz deaktivieren',"
           "g:'Einrichten',ok:'Eingerichtet. Seite neu laden.',na:'Noch nicht "
           "freigegeben: Web-Setup zuerst am Geraet freigeben, dann erneut "
           "versuchen.',ic:'Passwort oder PIN abgelehnt.',ap:'Bereits "
           "eingerichtet.',rc:'Lokale Wiederherstellung "
           "noetig.',fl:'Einrichtung "
           "fehlgeschlagen.'},en:{a:'Protect with a password (recommended)',b:'"
           "Disable password protection',c:'Password',e:'Service "
           "PIN',x:'Warning:"
           " without a password every device in the local network can operate "
           "this device. The service area stays PIN protected.',y:'I "
           "understand, "
           "disable password protection',g:'Set up',ok:'Set up. Reload the "
           "page.',"
           "na:'Not released yet: release web setup on the device first, then "
           "retry.',ic:'Password or PIN rejected.',ap:'Already set "
           "up.',rc:'Local "
           "recovery needed.',fl:'Setup failed.'},es:{a:'Proteger con "
           "contrasena "
           "(recomendado)',b:'Desactivar la proteccion',c:'Contrasena',e:'PIN "
           "de "
           "servicio',x:'Aviso: sin contrasena cualquier dispositivo de la red "
           "local puede operar el equipo. El servicio sigue protegido por "
           "PIN.',"
           "y:'Entendido, desactivar la proteccion',g:'Configurar',ok:'Listo. "
           "Recargue la pagina.',na:'Aun no permitido: permita la "
           "configuracion "
           "web en el equipo y reintente.',ic:'Contrasena o PIN "
           "rechazados.',ap:'Ya"
           " configurado.',rc:'Requiere recuperacion local.',fl:'Fallo la "
           "configuracion.'}};const g=(navigator.language||'en').slice(0,2),"
           "T=L[g]||L.en,q=i=>document.getElementById(i);"
           "document.documentElement.lang=L[g]?g:'en';for(const i of "
           "['a','b','c','e','x','y','g'])q(i).textContent=T[i];"
           "const "
           "d=()=>document.querySelector('input[name=o]:checked').value==='d';"
           "const "
           "u=()=>{q('lw').hidden=d();q('x').hidden=q('lk').hidden=!d();};"
           "document.querySelectorAll('input[name=o]').forEach(i=>i.onchange=u)"
           ";"
           "q('f').onsubmit=async "
           "e=>{e.preventDefault();if(d()&&!q('k').checked)"
           "{q('m').textContent=T.y;return;}const b=d()?{mode:'disable',"
           "servicePin:q('n').value,confirmDisable:true}:{mode:'protect',"
           "password:"
           "q('w').value,servicePin:q('n').value};try{const r=await "
           "fetch('/api/v1/provision',{method:'POST',credentials:'same-origin',"
           "headers:{'Content-Type':'application/"
           "json'},body:JSON.stringify(b)});"
           "const j=await "
           "r.json().catch(()=>({}));q('m').textContent=r.ok?T.ok:"
           "j.error==='provisioning-not-allowed'?T.na:r.status===409?T.ap:r."
           "status===422?T.ic:"
           "j.error==='recovery-required'?T.rc:T.fl;if(r.ok){q('w').value=q('n'"
           ")"
           ".value='';}}catch(x){q('m').textContent=T.fl;}};u();</script></"
           "body>"
           "</html>";
}

std::string webShell(WebAuthenticationState state, const std::string& csrfToken,
                     bool authenticated) {
    if (state == WebAuthenticationState::Unprovisioned) {
        return provisioningShell();
    }
    // This is intentionally a small static shell. It has no framework,
    // generated bundle, history, chart or mutation control. All three API
    // requests are repeated together so a reconnect receives a full snapshot.
    std::string html =
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" "
        "content=\"width=device-width,initial-scale=1\">"
        "<title>Fermentation</title><style>body{font-family:system-ui;"
        "max-width:48rem;margin:auto;padding:1rem}main{display:grid;gap:1rem}"
        "section{border:1px solid #bbb;border-radius:.5rem;padding:1rem}"
        "button,input{font:inherit;padding:.45rem}#connection{font-weight:600}"
        "</style></head><body><main><h1>Fermentation</h1>"
        "<p id=\"auth-state\"></p><p id=\"warning\" hidden></p>"
        "<form id=\"login\"><label><span id=\"password-label\">Password"
        "</span> <input id=\"password\""
        "type=\"password\" autocomplete=\"current-password\"></label>"
        "<button>Login</button></form><button id=\"logout\" "
        "hidden>Logout</button>"
        "<section><div id=\"connection\">Offline</div>"
        "<pre id=\"status\">No snapshot</pre></section></main>"
        "<script>const S=";
    html += "'";
    html += csrfToken;
    html += "';const A='";
    html += authenticationStateName(state);
    html += "';const U=";
    html += authenticated ? "true" : "false";
    html +=
        ";const L={de:{login:'Anmelden',logout:'Abmelden',offline:'Offline',"
        "stale:'Veraltet',online:'Online',warning:'Passwortschutz deaktiviert',"
        "required:'Lokale Provisionierung oder Recovery erforderlich',"
        "passwordLabel:'Passwort',noSnapshot:'Kein Snapshot',"
        "protected:'Passwortschutz aktiv',disabled:'Passwortschutz "
        "deaktiviert'},"
        "en:{login:'Login',logout:'Logout',offline:'Offline',stale:'Stale',"
        "online:'Online',warning:'Password protection is disabled',"
        "required:'Local provisioning or recovery "
        "required',passwordLabel:'Password',"
        "noSnapshot:'No snapshot',protected:'Password protection enabled',"
        "disabled:'Password protection disabled'},"
        "es:{login:'Iniciar sesion',logout:'Cerrar sesion',offline:'Sin "
        "conexion',"
        "stale:'Anticuado',online:'En linea',warning:'Proteccion por "
        "contrasena desactivada',"
        "required:'Se requiere provision local o recuperacion',"
        "passwordLabel:'Contrasena',noSnapshot:'Sin instantanea',"
        "protected:'Proteccion por contrasena activa',"
        "disabled:'Proteccion por contrasena desactivada'}};"
        "const lang=(navigator.language||'en').slice(0,2);const "
        "locale=L[lang]?lang:'en';const T=L[locale]||L.en;"
        "document.documentElement.lang=locale;"
        "document.querySelector('#login button').textContent=T.login;"
        "document.querySelector('#logout').textContent=T.logout;"
        "document.querySelector('#password-label').textContent=T.passwordLabel;"
        "const "
        "auth=document.querySelector('#auth-state'),warning=document."
        "querySelector('#warning'),"
        "login=document.querySelector('#login'),logout=document.querySelector('"
        "#logout'),"
        "connection=document.querySelector('#connection'),status=document."
        "querySelector('#status');"
        "auth.textContent=A==='unprovisioned'||A==='recovery-required'||A==='"
        "indeterminate'?T.required:A==='password-protected'?T.protected:T."
        "disabled;"
        "warning.textContent=T.warning;warning.hidden=A!=='password-disabled';"
        "status.textContent=T.noSnapshot;login.hidden=A!=='password-protected'|"
        "|U;"
        "logout.hidden=!U;"
        "async function refresh(){try{const r=await "
        "Promise.all(['/api/v1/status',"
        "'/api/v1/temperatures','/api/v1/"
        "alerts'].map(x=>fetch(x,{credentials:'same-origin',"
        "cache:'no-store'})));if(r.some(x=>!x.ok))throw Error('offline');"
        "const p=await "
        "Promise.all(r.map(x=>x.json()));status.textContent=JSON.stringify(p,"
        "null,2);"
        "connection.textContent=T.online;connection.dataset.state='online';}"
        "catch(e){"
        "connection.textContent=T.stale;connection.dataset.state='stale';}}"
        "login.addEventListener('submit',async e=>{e.preventDefault();const "
        "r=await fetch('/api/v1/login',"
        "{method:'POST',credentials:'same-origin',headers:{'Content-Type':'"
        "application/json'},"
        "body:JSON.stringify({password:document.querySelector('#password')."
        "value})});"
        "if(r.ok){const p=await "
        "r.json();window.__WEB_CSRF__=p.csrfToken||'';logout.hidden=!window.__"
        "WEB_CSRF__;"
        "login.hidden=true;document.querySelector('#password').value='';"
        "refresh();}});"
        "logout.addEventListener('click',async()=>{await "
        "fetch('/api/v1/logout',{method:'POST',"
        "credentials:'same-origin',headers:{'Content-Type':'application/json',"
        "'X-CSRF-Token':window.__WEB_CSRF__|"
        "|''}});"
        "window.__WEB_CSRF__='';logout.hidden=true;login.hidden=A!=='password-"
        "protected';});"
        "window.__WEB_CSRF__=S;refresh();"
        "setInterval(refresh,5000);</script></body></html>";
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

    FermentationUiCommandContext context;
    context.surface = device_platform::UiSurface::WebInterface;
    context.monotonicMillis = nowMs;
    context.expected = dto.expected;

    ReplayOutcomeCode projected = ReplayOutcomeCode::Unavailable;
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

    if (!sessions_.completeMutation(*session.handle,
                                    timeSource_.monotonicMillis(),
                                    *reservation.sequence, digest, projected)) {
        setResponse(response, unavailable());
        return true;
    }
    setResponse(response, replayOutcome(projected));
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

    response.statusCode = 200U;
    response.contentType = kHtmlContentType;
    response.body = webShell(state, csrf, session.has_value());
    if (response.body.size() > kMaximumWebShellBytes) {
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

bool WebRouteDispatcher::handle(const device_platform::HttpRequest& request,
                                device_platform::HttpResponse& response) {
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
    if (request.path == "/") return handleShell(request, response);
    if (request.path == kLoginPath) return handleLogin(request, response);
    if (request.path == kLogoutPath) return handleLogout(request, response);
    if (request.path == kProvisionPath) {
        return handleProvision(request, response);
    }
    if (request.path == kStatusApiPath ||
        request.path == kTemperaturesApiPath ||
        request.path == kAlertsApiPath) {
        return handleReadOnly(request, response);
    }
    // No productive branch exists for /internal/ui/run in Slice 4C.
    return false;
}

}  // namespace fermentation
