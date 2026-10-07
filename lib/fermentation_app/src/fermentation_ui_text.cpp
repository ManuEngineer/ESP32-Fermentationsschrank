#include "fermentation_ui_text.hpp"

#include <array>
#include <string>
#include <utility>

namespace fermentation {

device_platform::TextKey fermentationTextKey(const char* value) {
    return {device_platform::TextNamespace{"fermentation"}, value};
}

device_platform::TextKey messageCodeTextKey(MessageCode code) {
    switch (code) {
        case MessageCode::ProductInsertionRequested:
            return fermentationTextKey("message-product-insertion-requested");
        case MessageCode::TargetReachTimeExceeded:
            return fermentationTextKey("message-target-reach-time-exceeded");
        case MessageCode::UserDecisionRequired:
            return fermentationTextKey("message-user-decision-required");
        case MessageCode::RunCompleted:
            return fermentationTextKey("message-run-completed");
        case MessageCode::RunAborted:
            return fermentationTextKey("message-run-aborted");
        case MessageCode::RecoveryPending:
            return fermentationTextKey("message-recovery-pending");
        case MessageCode::SafetyFault:
            return fermentationTextKey("message-safety-fault");
    }
    // An unknown code stays visible as its technical key instead of a guess.
    return fermentationTextKey("message-unknown");
}

device_platform::TextKey messageClassTextKey(MessageClass messageClass) {
    switch (messageClass) {
        case MessageClass::Information:
            return fermentationTextKey("message-class-information");
        case MessageClass::ProcessWarning:
            return fermentationTextKey("message-class-process-warning");
        case MessageClass::Recovery:
            return fermentationTextKey("message-class-recovery");
        case MessageClass::DecisionRequired:
            return fermentationTextKey("message-class-decision-required");
        case MessageClass::SafetyFault:
            return fermentationTextKey("message-class-safety-fault");
    }
    return fermentationTextKey("message-unknown");
}

device_platform::TextKey processStateTextKey(ProcessState state) {
    switch (state) {
        case ProcessState::Boot:
            return fermentationTextKey("process-state-boot");
        case ProcessState::SafeBoot:
            return fermentationTextKey("process-state-safe-boot");
        case ProcessState::Standby:
            return fermentationTextKey("process-state-standby");
        case ProcessState::Preheating:
            return fermentationTextKey("process-state-preheating");
        case ProcessState::WaitingForProduct:
            return fermentationTextKey("process-state-waiting-for-product");
        case ProcessState::ReachingTarget:
            return fermentationTextKey("process-state-reaching-target");
        case ProcessState::QualifyingTarget:
            return fermentationTextKey("process-state-qualifying-target");
        case ProcessState::Fermenting:
            return fermentationTextKey("process-state-fermenting");
        case ProcessState::Cooling:
            return fermentationTextKey("process-state-cooling");
        case ProcessState::CoolHolding:
            return fermentationTextKey("process-state-cool-holding");
        case ProcessState::ManualHolding:
            return fermentationTextKey("process-state-manual-holding");
        case ProcessState::Completed:
            return fermentationTextKey("process-state-completed");
        case ProcessState::RecoveryEvaluation:
            return fermentationTextKey("process-state-recovery-evaluation");
        case ProcessState::Fault:
            return fermentationTextKey("process-state-fault");
        case ProcessState::ServiceMode:
            return fermentationTextKey("process-state-service-mode");
    }
    return fermentationTextKey("message-unknown");
}

device_platform::TextKey recoveryModeTextKey(RecoveryViewMode mode) {
    switch (mode) {
        case RecoveryViewMode::Normal:
            return fermentationTextKey("recovery-mode-normal");
        case RecoveryViewMode::WaitingForTrustedTime:
            return fermentationTextKey(
                "recovery-mode-waiting-for-trusted-time");
        case RecoveryViewMode::CurrentRunRecovered:
            return fermentationTextKey("recovery-mode-current-run-recovered");
        case RecoveryViewMode::FallbackSelectionRequired:
            return fermentationTextKey(
                "recovery-mode-fallback-selection-required");
        case RecoveryViewMode::RecoveryRejectedOrFailClosed:
            return fermentationTextKey("recovery-mode-rejected-or-fail-closed");
        case RecoveryViewMode::Completed:
            return fermentationTextKey("recovery-mode-completed");
        case RecoveryViewMode::Cooling:
            return fermentationTextKey("recovery-mode-cooling");
    }
    return fermentationTextKey("message-unknown");
}

device_platform::TextKey sensorPreferenceTextKey(SensorPreference preference) {
    switch (preference) {
        case SensorPreference::ProductIfAvailableElseAir:
            return fermentationTextKey("sensor-pref-product-else-air");
        case SensorPreference::AirProductOptional:
            return fermentationTextKey("sensor-pref-air-product-optional");
        case SensorPreference::ProductRequired:
            return fermentationTextKey("sensor-pref-product-required");
        case SensorPreference::AirOnly:
            return fermentationTextKey("sensor-pref-air-only");
    }
    return fermentationTextKey("message-unknown");
}

device_platform::TextKey runSensorModeTextKey(RunSensorMode mode) {
    switch (mode) {
        case RunSensorMode::Product:
            return fermentationTextKey("sensor-product");
        case RunSensorMode::Air:
            return fermentationTextKey("sensor-air");
    }
    return fermentationTextKey("message-unknown");
}

device_platform::TextKey completionModeTextKey(CompletionMode mode) {
    switch (mode) {
        case CompletionMode::FinishWithoutCooling:
            return fermentationTextKey("completion-finish");
        case CompletionMode::CoolThenFinish:
            return fermentationTextKey("completion-cool-finish");
        case CompletionMode::CoolAndHoldForDuration:
            return fermentationTextKey("completion-cool-hold-duration");
        case CompletionMode::CoolAndHoldUntilManualStop:
            return fermentationTextKey("completion-cool-hold-manual");
    }
    return fermentationTextKey("message-unknown");
}

device_platform::TextKey temperatureRoleTextKey(
    FermentationTemperatureRole role) {
    switch (role) {
        case FermentationTemperatureRole::CabinetAir:
            return fermentationTextKey("temperature-cabinet-air");
        case FermentationTemperatureRole::Product:
            return fermentationTextKey("temperature-product");
        case FermentationTemperatureRole::Cooling:
            return fermentationTextKey("temperature-cooling");
    }
    return fermentationTextKey("message-unknown");
}

device_platform::TextKey sensorQualityTextKey(
    device_platform::SensorQuality quality) {
    switch (quality) {
        case device_platform::SensorQuality::Valid:
            return fermentationTextKey("quality-valid");
        case device_platform::SensorQuality::Stale:
            return fermentationTextKey("quality-stale");
        case device_platform::SensorQuality::Failed:
            return fermentationTextKey("quality-failed");
    }
    return fermentationTextKey("message-unknown");
}

namespace {

// Immutable firmware texts: constexpr tables of string-literal views, so they
// stay in read-only flash and are never copied into the heap.
constexpr std::size_t kTextCount = 183U;
using device_platform::TextTranslation;

constexpr std::array<TextTranslation, kTextCount> kEnglishTexts{{
    {"standby", "Ready"},
    {"running", "Process running"},
    {"waiting", "Waiting"},
    {"completed", "Completed"},
    {"restricted", "Restricted"},
    {"recovery", "Recovery"},
    {"unavailable", "Unavailable"},
    {"start", "Start"},
    {"preheat", "Preheat"},
    {"programs", "Recipes"},
    {"status", "Status"},
    {"service", "Service"},
    {"stop", "Stop"},
    {"details", "Details"},
    {"continue", "Continue"},
    {"ok", "OK"},
    {"cool-now", "Cool now"},
    {"back", "Back"},
    {"home", "Home"},
    {"up", "Up"},
    {"down", "Down"},
    {"confirm", "Confirm"},
    {"cancel", "Cancel"},
    {"service-locked", "Service unavailable"},
    {"service-home-locked", "Service off"},
    {"resume-fallback", "Resume fallback"},
    {"manual", "Manual"},
    {"manual-holding", "Manual holding"},
    {"manual-timed", "Manual timed"},
    {"technical", "Technical"},
    {"messages", "Messages"},
    {"message-detail", "Message detail"},
    {"diagnostics", "Diagnostics"},
    {"pin", "PIN"},
    {"language", "Language"},
    {"network", "WLAN"},
    {"clock", "Clock"},
    {"program-actions", "Recipe actions"},
    {"program-edit", "Edit recipe"},
    {"edit", "Edit"},
    {"copy", "Copy"},
    {"new", "New"},
    {"reset", "Reset"},
    {"delete", "Delete"},
    {"uninstall", "Uninstall"},
    {"save", "Save"},
    {"stop-turn-off", "Stop and turn off"},
    {"stop-and-cool", "Stop and cool"},
    {"acknowledge", "Acknowledge"},
    {"mute", "Mute"},
    {"fault-reset", "Reset fault"},
    {"program-not-installed", "Program not installed"},
    {"program-disabled", "Program disabled"},
    {"program-invalid", "Program invalid"},
    {"factory-reset-required", "Restore only via factory reset"},
    {"network-ap-only", "AP only"},
    {"network-home-wifi", "Home WiFi"},
    {"network-reconfigure", "WiFi setup"},
    {"network-current", "Current mode"},
    {"network-mode-required", "Select mode"},
    {"network-browser-setup", "Credentials: local browser setup"},
    {"network-ssid", "SSID: "},
    {"network-password", "Password: "},
    {"network-access-unavailable", "Access data unavailable"},
    {"network-ip-unavailable", "unavailable"},
    {"clock-trusted", "Time trusted"},
    {"clock-not-trusted", "Time not trusted"},
    {"web-access", "Web access"},
    {"web-access-open", "Web setup"},
    {"web-access-window-open", "Web setup allowed (10 min)"},
    {"web-access-closed", "Web setup not allowed yet"},
    {"web-access-unavailable", "Web setup not available"},
    {"message-product-insertion-requested", "Insert product"},
    {"message-target-reach-time-exceeded", "Target time exceeded"},
    {"message-user-decision-required", "Decision required"},
    {"message-run-completed", "Run completed"},
    {"message-run-aborted", "Run aborted"},
    {"message-recovery-pending", "Recovery pending"},
    {"message-safety-fault", "Safety fault"},
    {"message-class-information", "Information"},
    {"message-class-process-warning", "Process warning"},
    {"message-class-recovery", "Recovery"},
    {"message-class-decision-required", "Decision required"},
    {"message-class-safety-fault", "Safety fault"},
    {"message-acknowledged", "Acknowledged"},
    {"message-muted", "Muted"},
    {"language-de", "Deutsch"},
    {"language-en", "English"},
    {"language-es", "Espanol"},
    {"language-change-failed", "Language not changed"},
    {"deferred-28", "Deferred (#28)"},
    {"recovery-time-correction-unavailable",
     "Time correction: not available (R1)"},
    {"messages-empty", "No messages"},
    {"label-target", "Target: "},
    {"label-duration", "Duration: "},
    {"label-remaining", "Remaining: "},
    {"label-preheat", "Preheat: "},
    {"label-sensor", "Sensor: "},
    {"label-completion", "End: "},
    {"label-fault-code", "Fault code: "},
    {"value-on", "On"},
    {"value-off", "Off"},
    {"sensor-air", "Air"},
    {"sensor-product", "Product"},
    {"sensor-pref-product-else-air", "Product, else air"},
    {"sensor-pref-air-product-optional", "Air, product optional"},
    {"sensor-pref-product-required", "Product required"},
    {"sensor-pref-air-only", "Air only"},
    {"completion-finish", "Finish"},
    {"completion-cool-finish", "Cool, finish"},
    {"completion-cool-hold-duration", "Cool, hold (time)"},
    {"completion-cool-hold-manual", "Cool, hold to stop"},
    {"process-state-boot", "Starting"},
    {"process-state-safe-boot", "Safe boot"},
    {"process-state-standby", "Standby"},
    {"process-state-preheating", "Preheating"},
    {"process-state-waiting-for-product", "Waiting for product"},
    {"process-state-reaching-target", "Reaching target"},
    {"process-state-qualifying-target", "Qualifying target"},
    {"process-state-fermenting", "Fermenting"},
    {"process-state-cooling", "Cooling"},
    {"process-state-cool-holding", "Cool holding"},
    {"process-state-manual-holding", "Manual holding"},
    {"process-state-completed", "Completed"},
    {"process-state-recovery-evaluation", "Recovery evaluation"},
    {"process-state-fault", "Fault"},
    {"process-state-service-mode", "Service mode"},
    {"temperature-cabinet-air", "Cabinet air"},
    {"temperature-product", "Product"},
    {"temperature-cooling", "Cooling"},
    {"quality-valid", "valid"},
    {"quality-stale", "stale"},
    {"quality-failed", "failed"},
    {"label-cooling", "Cooling: "},
    {"label-hold", "Hold: "},
    {"field-target", "Target temp."},
    {"field-duration", "Duration"},
    {"field-cooling", "Cooling target"},
    {"field-hold", "Hold time"},
    {"backspace", "Del"},
    {"clear", "Clear"},
    {"start-values-invalid", "Start values invalid"},
    {"manual-parameters-not-released", "Run parameters not released"},
    {"settings", "Settings"},
    {"settings-page", "Settings"},
    {"settings-time-zone", "Time / zone"},
    {"device-name", "Device name"},
    {"device-name-locked-run", "locked during run"},
    {"device-name-change-failed", "Name not changed"},
    {"service-protected", "Service (PIN)"},
    {"program-name", "Name"},
    {"program-notes", "Note"},
    {"space", "Space"},
    {"kbd-lower", "abc"},
    {"kbd-upper", "ABC"},
    {"kbd-digits", "123"},
    {"kbd-symbols", "#+="},
    {"pf-name", "Name: "},
    {"pf-notes", "Note: "},
    {"pf-wait", "Product wait: "},
    {"pf-failure", "Probe fail: "},
    {"pf-delay", "Fallback in: "},
    {"pf-return", "Return: "},
    {"pf-reach", "Reach time: "},
    {"pt-wait", "Product wait"},
    {"pt-delay", "Fallback delay"},
    {"pt-reach", "Reach time"},
    {"policy-fallback", "Air after timeout"},
    {"policy-wait", "Wait for user"},
    {"policy-stop", "Stop safely"},
    {"return-air", "Stay on air"},
    {"return-manual", "Manual return"},
    {"return-auto", "Auto return"},
    {"discard", "Discard"},
    {"status-ready", "Application ready"},
    {"status-not-ready", "Application not ready"},
    {"recovery-mode-normal", "No recovery pending"},
    {"recovery-mode-waiting-for-trusted-time", "Waiting for trusted time"},
    {"recovery-mode-current-run-recovered", "Current run recovered"},
    {"recovery-mode-fallback-selection-required",
     "Fallback selection required"},
    {"recovery-mode-rejected-or-fail-closed",
     "Recovery rejected (fail-closed)"},
    {"recovery-mode-completed", "Run completed"},
    {"recovery-mode-cooling", "Cooling after recovery"},
}};

constexpr std::array<TextTranslation, kTextCount> kGermanTexts{{
    {"standby", "Bereit"},
    {"running", "Prozess laeuft"},
    {"waiting", "Wartet"},
    {"completed", "Abgeschlossen"},
    {"restricted", "Eingeschraenkt"},
    {"recovery", "Wiederherstellung"},
    {"unavailable", "Nicht verfuegbar"},
    {"start", "Start"},
    {"preheat", "Vorheizen"},
    {"programs", "Rezepte"},
    {"status", "Status"},
    {"service", "Service"},
    {"stop", "Stop"},
    {"details", "Details"},
    {"continue", "Weiter"},
    {"ok", "OK"},
    {"cool-now", "Jetzt kuehlen"},
    {"back", "Zurueck"},
    {"home", "Home"},
    {"up", "Auf"},
    {"down", "Ab"},
    {"confirm", "Bestaetigen"},
    {"cancel", "Abbrechen"},
    {"service-locked", "Service gesperrt"},
    {"service-home-locked", "Service aus"},
    {"resume-fallback", "Fallback fortsetzen"},
    {"manual", "Manuell"},
    {"manual-holding", "Manuelles Halten"},
    {"manual-timed", "Manueller Zeitlauf"},
    {"technical", "Technik"},
    {"messages", "Meldungen"},
    {"message-detail", "Meldungsdetail"},
    {"diagnostics", "Diagnose"},
    {"pin", "PIN"},
    {"language", "Sprache"},
    {"network", "WLAN"},
    {"clock", "Uhrzeit"},
    {"program-actions", "Rezeptaktionen"},
    {"program-edit", "Rezept bearbeiten"},
    {"edit", "Bearbeiten"},
    {"copy", "Kopieren"},
    {"new", "Neu"},
    {"reset", "Zuruecksetzen"},
    {"delete", "Loeschen"},
    {"uninstall", "Deinstallieren"},
    {"save", "Speichern"},
    {"stop-turn-off", "Stoppen und ausschalten"},
    {"stop-and-cool", "Stoppen und kuehlen"},
    {"acknowledge", "Quittieren"},
    {"mute", "Stummschalten"},
    {"fault-reset", "Fehlerreset"},
    {"program-not-installed", "Programm nicht installiert"},
    {"program-disabled", "Programm deaktiviert"},
    {"program-invalid", "Programm ungueltig"},
    {"factory-reset-required", "Wiederherstellung nur durch Werksreset"},
    {"network-ap-only", "Nur AP"},
    {"network-home-wifi", "Heimnetz"},
    {"network-reconfigure", "Setup"},
    {"network-current", "Aktueller Modus"},
    {"network-mode-required", "Modus waehlen"},
    {"network-browser-setup", "Zugangsdaten: lokales Browser-Setup"},
    {"network-ssid", "SSID: "},
    {"network-password", "Passwort: "},
    {"network-access-unavailable", "Zugangsdaten nicht verfuegbar"},
    {"network-ip-unavailable", "nicht verfuegbar"},
    {"clock-trusted", "Zeit vertrauenswuerdig"},
    {"clock-not-trusted", "Zeit nicht vertrauenswuerdig"},
    {"web-access", "Webzugang"},
    {"web-access-open", "Web-Setup"},
    {"web-access-window-open", "Web-Setup frei (10 Min)"},
    {"web-access-closed", "Web-Setup nicht freigegeben"},
    {"web-access-unavailable", "Web-Setup nicht verfuegbar"},
    {"message-product-insertion-requested", "Produkt einlegen"},
    {"message-target-reach-time-exceeded", "Zielzeit ueberschritten"},
    {"message-user-decision-required", "Entscheidung noetig"},
    {"message-run-completed", "Lauf abgeschlossen"},
    {"message-run-aborted", "Lauf abgebrochen"},
    {"message-recovery-pending", "Wiederanlauf offen"},
    {"message-safety-fault", "Sicherheitsfehler"},
    {"message-class-information", "Information"},
    {"message-class-process-warning", "Prozesswarnung"},
    {"message-class-recovery", "Wiederanlauf"},
    {"message-class-decision-required", "Entscheidung noetig"},
    {"message-class-safety-fault", "Sicherheitsfehler"},
    {"message-acknowledged", "Quittiert"},
    {"message-muted", "Stumm"},
    {"language-de", "Deutsch"},
    {"language-en", "English"},
    {"language-es", "Espanol"},
    {"language-change-failed", "Sprache nicht geaendert"},
    {"deferred-28", "zurueckgestellt (#28)"},
    {"recovery-time-correction-unavailable",
     "Zeitkorrektur: nicht verfuegbar (R1)"},
    {"messages-empty", "Keine Meldungen"},
    {"label-target", "Ziel: "},
    {"label-duration", "Dauer: "},
    {"label-remaining", "Rest: "},
    {"label-preheat", "Vorheizen: "},
    {"label-sensor", "Sensor: "},
    {"label-completion", "Abschluss: "},
    {"label-fault-code", "Fehlercode: "},
    {"value-on", "Ein"},
    {"value-off", "Aus"},
    {"sensor-air", "Luft"},
    {"sensor-product", "Produkt"},
    {"sensor-pref-product-else-air", "Produkt, sonst Luft"},
    {"sensor-pref-air-product-optional", "Luft, Produkt optional"},
    {"sensor-pref-product-required", "Produkt erforderlich"},
    {"sensor-pref-air-only", "Nur Luft"},
    {"completion-finish", "Beenden"},
    {"completion-cool-finish", "Kuehlen, beenden"},
    {"completion-cool-hold-duration", "Kuehlen, Zeit halten"},
    {"completion-cool-hold-manual", "Kuehlen, bis Stop"},
    {"process-state-boot", "Startet"},
    {"process-state-safe-boot", "Sicherer Start"},
    {"process-state-standby", "Bereit"},
    {"process-state-preheating", "Vorheizen"},
    {"process-state-waiting-for-product", "Warten auf Produkt"},
    {"process-state-reaching-target", "Ziel wird erreicht"},
    {"process-state-qualifying-target", "Ziel wird abgesichert"},
    {"process-state-fermenting", "Gaerung"},
    {"process-state-cooling", "Kuehlen"},
    {"process-state-cool-holding", "Kuehl halten"},
    {"process-state-manual-holding", "Manuelles Halten"},
    {"process-state-completed", "Abgeschlossen"},
    {"process-state-recovery-evaluation", "Wiederanlauf-Pruefung"},
    {"process-state-fault", "Stoerung"},
    {"process-state-service-mode", "Servicemodus"},
    {"temperature-cabinet-air", "Schrankluft"},
    {"temperature-product", "Produkt"},
    {"temperature-cooling", "Kuehlung"},
    {"quality-valid", "gueltig"},
    {"quality-stale", "veraltet"},
    {"quality-failed", "Fehler"},
    {"label-cooling", "Kuehlziel: "},
    {"label-hold", "Halten: "},
    {"field-target", "Zieltemp."},
    {"field-duration", "Dauer"},
    {"field-cooling", "Kuehlziel"},
    {"field-hold", "Haltedauer"},
    {"backspace", "Entf"},
    {"clear", "Leeren"},
    {"start-values-invalid", "Startwerte ungueltig"},
    {"manual-parameters-not-released", "Laufparameter nicht freigegeben"},
    {"settings", "Einstell."},
    {"settings-page", "Einstellungen"},
    {"settings-time-zone", "Zeit / Zeitzone"},
    {"device-name", "Geraetename"},
    {"device-name-locked-run", "gesperrt im Lauf"},
    {"device-name-change-failed", "Name nicht geaendert"},
    {"service-protected", "Service (PIN)"},
    {"program-name", "Name"},
    {"program-notes", "Notiz"},
    {"space", "Leer"},
    {"kbd-lower", "abc"},
    {"kbd-upper", "ABC"},
    {"kbd-digits", "123"},
    {"kbd-symbols", "#+="},
    {"pf-name", "Name: "},
    {"pf-notes", "Notiz: "},
    {"pf-wait", "Produktwarten: "},
    {"pf-failure", "Fuehlerausfall: "},
    {"pf-delay", "Fallback nach: "},
    {"pf-return", "Rueckkehr: "},
    {"pf-reach", "Zielzeit: "},
    {"pt-wait", "Produktwarten"},
    {"pt-delay", "Fallback-Zeit"},
    {"pt-reach", "Zielzeit"},
    {"policy-fallback", "Luft nach Zeit"},
    {"policy-wait", "Auf Nutzer warten"},
    {"policy-stop", "Sicher stoppen"},
    {"return-air", "Bei Luft bleiben"},
    {"return-manual", "Manuell zurueck"},
    {"return-auto", "Automatisch zurueck"},
    {"discard", "Verwerfen"},
    {"status-ready", "Anwendung bereit"},
    {"status-not-ready", "Anwendung nicht bereit"},
    {"recovery-mode-normal", "Kein Wiederanlauf offen"},
    {"recovery-mode-waiting-for-trusted-time",
     "Warten auf vertrauenswuerdige Zeit"},
    {"recovery-mode-current-run-recovered", "Aktueller Lauf wiederhergestellt"},
    {"recovery-mode-fallback-selection-required", "Fallback-Auswahl noetig"},
    {"recovery-mode-rejected-or-fail-closed",
     "Wiederanlauf abgelehnt (fail-closed)"},
    {"recovery-mode-completed", "Lauf abgeschlossen"},
    {"recovery-mode-cooling", "Kuehlen nach Wiederanlauf"},
}};

constexpr std::array<TextTranslation, kTextCount> kSpanishTexts{{
    {"standby", "Listo"},
    {"running", "Proceso en curso"},
    {"waiting", "Espera"},
    {"completed", "Completado"},
    {"restricted", "Restringido"},
    {"recovery", "Recuperacion"},
    {"unavailable", "No disponible"},
    {"start", "Iniciar"},
    {"preheat", "Precalentar"},
    {"programs", "Recetas"},
    {"status", "Estado"},
    {"service", "Servicio"},
    {"stop", "Detener"},
    {"details", "Detalles"},
    {"continue", "Continuar"},
    {"ok", "OK"},
    {"cool-now", "Enfriar ahora"},
    {"back", "Atras"},
    {"home", "Inicio"},
    {"up", "Arriba"},
    {"down", "Abajo"},
    {"confirm", "Confirmar"},
    {"cancel", "Cancelar"},
    {"service-locked", "Servicio bloqueado"},
    {"service-home-locked", "Servicio off"},
    {"resume-fallback", "Reanudar respaldo"},
    {"manual", "Manual"},
    {"manual-holding", "Mantenimiento manual"},
    {"manual-timed", "Tiempo manual"},
    {"technical", "Tecnico"},
    {"messages", "Mensajes"},
    {"message-detail", "Detalle del mensaje"},
    {"diagnostics", "Diagnostico"},
    {"pin", "PIN"},
    {"language", "Idioma"},
    {"network", "WLAN"},
    {"clock", "Hora"},
    {"program-actions", "Acciones de recetas"},
    {"program-edit", "Editar receta"},
    {"edit", "Editar"},
    {"copy", "Copiar"},
    {"new", "Nuevo"},
    {"reset", "Restablecer"},
    {"delete", "Eliminar"},
    {"uninstall", "Desinstalar"},
    {"save", "Guardar"},
    {"stop-turn-off", "Detener y apagar"},
    {"stop-and-cool", "Detener y enfriar"},
    {"acknowledge", "Confirmar"},
    {"mute", "Silenciar"},
    {"fault-reset", "Restablecer fallo"},
    {"program-not-installed", "Programa no instalado"},
    {"program-disabled", "Programa desactivado"},
    {"program-invalid", "Programa no valido"},
    {"factory-reset-required",
     "Restaurar solo mediante restablecimiento de fabrica"},
    {"network-ap-only", "Solo AP"},
    {"network-home-wifi", "WiFi casa"},
    {"network-reconfigure", "Ajustes"},
    {"network-current", "Modo actual"},
    {"network-mode-required", "Elegir modo"},
    {"network-browser-setup", "Credenciales: configuracion local en navegador"},
    {"network-ssid", "SSID: "},
    {"network-password", "Clave: "},
    {"network-access-unavailable", "Datos de acceso no disponibles"},
    {"network-ip-unavailable", "no disponible"},
    {"clock-trusted", "Hora fiable"},
    {"clock-not-trusted", "Hora no fiable"},
    {"web-access", "Acceso web"},
    {"web-access-open", "Config. web"},
    {"web-access-window-open", "Config. web permitida (10 min)"},
    {"web-access-closed", "Config. web no permitida"},
    {"web-access-unavailable", "Config. web no disponible"},
    {"message-product-insertion-requested", "Insertar producto"},
    {"message-target-reach-time-exceeded", "Tiempo objetivo excedido"},
    {"message-user-decision-required", "Decision necesaria"},
    {"message-run-completed", "Proceso completado"},
    {"message-run-aborted", "Proceso cancelado"},
    {"message-recovery-pending", "Recuperacion pendiente"},
    {"message-safety-fault", "Fallo de seguridad"},
    {"message-class-information", "Informacion"},
    {"message-class-process-warning", "Aviso de proceso"},
    {"message-class-recovery", "Recuperacion"},
    {"message-class-decision-required", "Decision necesaria"},
    {"message-class-safety-fault", "Fallo de seguridad"},
    {"message-acknowledged", "Confirmado"},
    {"message-muted", "Silenciado"},
    {"language-de", "Deutsch"},
    {"language-en", "English"},
    {"language-es", "Espanol"},
    {"language-change-failed", "Idioma no cambiado"},
    {"deferred-28", "Aplazado (#28)"},
    {"recovery-time-correction-unavailable",
     "Correccion de hora: no disponible (R1)"},
    {"messages-empty", "Sin mensajes"},
    {"label-target", "Objetivo: "},
    {"label-duration", "Duracion: "},
    {"label-remaining", "Restante: "},
    {"label-preheat", "Precalentar: "},
    {"label-sensor", "Sensor: "},
    {"label-completion", "Final: "},
    {"label-fault-code", "Codigo de fallo: "},
    {"value-on", "Si"},
    {"value-off", "No"},
    {"sensor-air", "Aire"},
    {"sensor-product", "Producto"},
    {"sensor-pref-product-else-air", "Producto, si no aire"},
    {"sensor-pref-air-product-optional", "Aire, producto opcional"},
    {"sensor-pref-product-required", "Producto obligatorio"},
    {"sensor-pref-air-only", "Solo aire"},
    {"completion-finish", "Terminar"},
    {"completion-cool-finish", "Enfriar, terminar"},
    {"completion-cool-hold-duration", "Enfriar, mantener"},
    {"completion-cool-hold-manual", "Enfriar, hasta parar"},
    {"process-state-boot", "Iniciando"},
    {"process-state-safe-boot", "Inicio seguro"},
    {"process-state-standby", "Listo"},
    {"process-state-preheating", "Precalentando"},
    {"process-state-waiting-for-product", "Esperando producto"},
    {"process-state-reaching-target", "Alcanzando objetivo"},
    {"process-state-qualifying-target", "Validando objetivo"},
    {"process-state-fermenting", "Fermentando"},
    {"process-state-cooling", "Enfriando"},
    {"process-state-cool-holding", "Manteniendo frio"},
    {"process-state-manual-holding", "Mantenimiento manual"},
    {"process-state-completed", "Completado"},
    {"process-state-recovery-evaluation", "Evaluando recuperacion"},
    {"process-state-fault", "Fallo"},
    {"process-state-service-mode", "Modo de servicio"},
    {"temperature-cabinet-air", "Aire del armario"},
    {"temperature-product", "Producto"},
    {"temperature-cooling", "Enfriamiento"},
    {"quality-valid", "valido"},
    {"quality-stale", "obsoleto"},
    {"quality-failed", "fallo"},
    {"label-cooling", "Frio: "},
    {"label-hold", "Mantener: "},
    {"field-target", "Temp. objetivo"},
    {"field-duration", "Duracion"},
    {"field-cooling", "Objetivo frio"},
    {"field-hold", "Tiempo mant."},
    {"backspace", "Borrar"},
    {"clear", "Limpiar"},
    {"start-values-invalid", "Valores no validos"},
    {"status-ready", "Aplicacion lista"},
    {"status-not-ready", "Aplicacion no lista"},
    {"recovery-mode-normal", "Sin recuperacion pendiente"},
    {"recovery-mode-waiting-for-trusted-time", "Esperando hora fiable"},
    {"recovery-mode-current-run-recovered", "Proceso actual recuperado"},
    {"recovery-mode-fallback-selection-required",
     "Seleccion de respaldo necesaria"},
    {"recovery-mode-rejected-or-fail-closed",
     "Recuperacion rechazada (fail-closed)"},
    {"recovery-mode-completed", "Proceso completado"},
    {"recovery-mode-cooling", "Enfriando tras recuperacion"},
    {"manual-parameters-not-released", "Parametros no liberados"},
    {"settings", "Ajustes"},
    {"settings-page", "Ajustes"},
    {"settings-time-zone", "Hora / zona"},
    {"device-name", "Nombre equipo"},
    {"device-name-locked-run", "bloqueado en curso"},
    {"device-name-change-failed", "Nombre no cambiado"},
    {"service-protected", "Servicio (PIN)"},
    {"program-name", "Nombre"},
    {"program-notes", "Nota"},
    {"space", "Espacio"},
    {"kbd-lower", "abc"},
    {"kbd-upper", "ABC"},
    {"kbd-digits", "123"},
    {"kbd-symbols", "#+="},
    {"pf-name", "Nombre: "},
    {"pf-notes", "Nota: "},
    {"pf-wait", "Espera prod.: "},
    {"pf-failure", "Fallo sonda: "},
    {"pf-delay", "Respaldo en: "},
    {"pf-return", "Retorno: "},
    {"pf-reach", "T. objetivo: "},
    {"pt-wait", "Espera prod."},
    {"pt-delay", "Retardo resp."},
    {"pt-reach", "Tiempo obj."},
    {"policy-fallback", "Aire tras tiempo"},
    {"policy-wait", "Esperar usuario"},
    {"policy-stop", "Parar seguro"},
    {"return-air", "Seguir en aire"},
    {"return-manual", "Retorno manual"},
    {"return-auto", "Retorno auto"},
    {"discard", "Descartar"},
}};

}  // namespace

std::vector<device_platform::TextPackManifest> makeFermentationUiTextPacks() {
    using device_platform::LocaleId;
    using device_platform::TextNamespace;
    using device_platform::TextPackCapabilities;
    using device_platform::TextPackManifest;
    const TextNamespace nameSpace{"fermentation"};
    const auto capabilities = TextPackCapabilities{"latin-de-en-es", 48U, true};
    std::vector<TextPackManifest> packs;
    packs.reserve(3U);
    packs.push_back(TextPackManifest{nameSpace, LocaleId{"de"}, capabilities,
                                     kGermanTexts});
    packs.push_back(TextPackManifest{nameSpace, LocaleId{"en"}, capabilities,
                                     kEnglishTexts});
    packs.push_back(TextPackManifest{nameSpace, LocaleId{"es"}, capabilities,
                                     kSpanishTexts});
    return packs;
}

}  // namespace fermentation
