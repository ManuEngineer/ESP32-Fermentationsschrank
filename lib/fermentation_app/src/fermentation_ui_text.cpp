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

std::vector<device_platform::TextPackManifest> makeFermentationUiTextPacks() {
    using device_platform::LocaleId;
    using device_platform::TextKey;
    using device_platform::TextNamespace;
    using device_platform::TextPackCapabilities;
    using device_platform::TextPackManifest;
    using device_platform::TextTranslation;
    const TextNamespace nameSpace{"fermentation"};
    const auto capabilities = TextPackCapabilities{"latin-de-en-es", 48U, true};
    const auto entries = std::array<std::pair<const char*, const char*>, 182U>{
        std::pair{"standby", "Ready"},
        std::pair{"running", "Process running"},
        std::pair{"waiting", "Waiting"},
        std::pair{"completed", "Completed"},
        std::pair{"restricted", "Restricted"},
        std::pair{"recovery", "Recovery"},
        std::pair{"unavailable", "Unavailable"},
        std::pair{"start", "Start"},
        std::pair{"preheat", "Preheat"},
        std::pair{"programs", "Recipes"},
        std::pair{"status", "Status"},
        std::pair{"service", "Service"},
        std::pair{"stop", "Stop"},
        std::pair{"details", "Details"},
        std::pair{"continue", "Continue"},
        std::pair{"ok", "OK"},
        std::pair{"cool-now", "Cool now"},
        std::pair{"back", "Back"},
        std::pair{"home", "Home"},
        std::pair{"up", "Up"},
        std::pair{"down", "Down"},
        std::pair{"confirm", "Confirm"},
        std::pair{"cancel", "Cancel"},
        std::pair{"service-locked", "Service unavailable"},
        std::pair{"service-home-locked", "Service off"},
        std::pair{"resume-fallback", "Resume fallback"},
        std::pair{"manual", "Manual"},
        std::pair{"manual-holding", "Manual holding"},
        std::pair{"manual-timed", "Manual timed"},
        std::pair{"technical", "Technical"},
        std::pair{"messages", "Messages"},
        std::pair{"message-detail", "Message detail"},
        std::pair{"diagnostics", "Diagnostics"},
        std::pair{"pin", "PIN"},
        std::pair{"language", "Language"},
        std::pair{"network", "WLAN"},
        std::pair{"clock", "Clock"},
        std::pair{"program-actions", "Recipe actions"},
        std::pair{"program-edit", "Edit recipe"},
        std::pair{"edit", "Edit"},
        std::pair{"copy", "Copy"},
        std::pair{"new", "New"},
        std::pair{"reset", "Reset"},
        std::pair{"delete", "Delete"},
        std::pair{"uninstall", "Uninstall"},
        std::pair{"save", "Save"},
        std::pair{"stop-turn-off", "Stop and turn off"},
        std::pair{"stop-and-cool", "Stop and cool"},
        std::pair{"acknowledge", "Acknowledge"},
        std::pair{"mute", "Mute"},
        std::pair{"fault-reset", "Reset fault"},
        std::pair{"program-not-installed", "Program not installed"},
        std::pair{"program-disabled", "Program disabled"},
        std::pair{"program-invalid", "Program invalid"},
        std::pair{"factory-reset-required", "Restore only via factory reset"},
        std::pair{"network-ap-only", "AP only"},
        std::pair{"network-home-wifi", "Home WiFi"},
        std::pair{"network-reconfigure", "WiFi setup"},
        std::pair{"network-current", "Current mode"},
        std::pair{"network-mode-required", "Select mode"},
        std::pair{"network-browser-setup", "Credentials: local browser setup"},
        std::pair{"network-ssid", "SSID: "},
        std::pair{"network-password", "Password: "},
        std::pair{"network-access-unavailable", "Access data unavailable"},
        std::pair{"network-ip-unavailable", "unavailable"},
        std::pair{"clock-trusted", "Time trusted"},
        std::pair{"clock-not-trusted", "Time not trusted"},
        std::pair{"web-access", "Web access"},
        std::pair{"web-access-open", "Web setup"},
        std::pair{"web-access-window-open", "Web setup allowed (10 min)"},
        std::pair{"web-access-closed", "Web setup not allowed yet"},
        std::pair{"web-access-unavailable", "Web setup not available"},
        std::pair{"message-product-insertion-requested", "Insert product"},
        std::pair{"message-target-reach-time-exceeded", "Target time exceeded"},
        std::pair{"message-user-decision-required", "Decision required"},
        std::pair{"message-run-completed", "Run completed"},
        std::pair{"message-run-aborted", "Run aborted"},
        std::pair{"message-recovery-pending", "Recovery pending"},
        std::pair{"message-safety-fault", "Safety fault"},
        std::pair{"message-class-information", "Information"},
        std::pair{"message-class-process-warning", "Process warning"},
        std::pair{"message-class-recovery", "Recovery"},
        std::pair{"message-class-decision-required", "Decision required"},
        std::pair{"message-class-safety-fault", "Safety fault"},
        std::pair{"message-acknowledged", "Acknowledged"},
        std::pair{"message-muted", "Muted"},
        // Endonyms in ASCII (the standard font has no n with tilde); the
        // same text in every pack so each language is recognisable.
        std::pair{"language-de", "Deutsch"},
        std::pair{"language-en", "English"},
        std::pair{"language-es", "Espanol"},
        std::pair{"language-change-failed", "Language not changed"},
        std::pair{"deferred-28", "Deferred (#28)"},
        std::pair{"recovery-time-correction-unavailable",
                  "Time correction: not available (R1)"},
        std::pair{"messages-empty", "No messages"},
        std::pair{"label-target", "Target: "},
        std::pair{"label-duration", "Duration: "},
        std::pair{"label-remaining", "Remaining: "},
        std::pair{"label-preheat", "Preheat: "},
        std::pair{"label-sensor", "Sensor: "},
        std::pair{"label-completion", "End: "},
        std::pair{"label-fault-code", "Fault code: "},
        std::pair{"value-on", "On"},
        std::pair{"value-off", "Off"},
        std::pair{"sensor-air", "Air"},
        std::pair{"sensor-product", "Product"},
        std::pair{"sensor-pref-product-else-air", "Product, else air"},
        std::pair{"sensor-pref-air-product-optional", "Air, product optional"},
        std::pair{"sensor-pref-product-required", "Product required"},
        std::pair{"sensor-pref-air-only", "Air only"},
        std::pair{"completion-finish", "Finish"},
        std::pair{"completion-cool-finish", "Cool, finish"},
        std::pair{"completion-cool-hold-duration", "Cool, hold (time)"},
        std::pair{"completion-cool-hold-manual", "Cool, hold to stop"},
        std::pair{"process-state-boot", "Starting"},
        std::pair{"process-state-safe-boot", "Safe boot"},
        std::pair{"process-state-standby", "Standby"},
        std::pair{"process-state-preheating", "Preheating"},
        std::pair{"process-state-waiting-for-product", "Waiting for product"},
        std::pair{"process-state-reaching-target", "Reaching target"},
        std::pair{"process-state-qualifying-target", "Qualifying target"},
        std::pair{"process-state-fermenting", "Fermenting"},
        std::pair{"process-state-cooling", "Cooling"},
        std::pair{"process-state-cool-holding", "Cool holding"},
        std::pair{"process-state-manual-holding", "Manual holding"},
        std::pair{"process-state-completed", "Completed"},
        std::pair{"process-state-recovery-evaluation", "Recovery evaluation"},
        std::pair{"process-state-fault", "Fault"},
        std::pair{"process-state-service-mode", "Service mode"},
        std::pair{"temperature-cabinet-air", "Cabinet air"},
        std::pair{"temperature-product", "Product"},
        std::pair{"temperature-cooling", "Cooling"},
        std::pair{"quality-valid", "valid"},
        std::pair{"quality-stale", "stale"},
        std::pair{"quality-failed", "failed"},
        std::pair{"label-cooling", "Cooling: "},
        std::pair{"label-hold", "Hold: "},
        std::pair{"field-target", "Target temp."},
        std::pair{"field-duration", "Duration"},
        std::pair{"field-cooling", "Cooling target"},
        std::pair{"field-hold", "Hold time"},
        std::pair{"backspace", "Del"},
        std::pair{"clear", "Clear"},
        std::pair{"start-values-invalid", "Start values invalid"},
        std::pair{"manual-parameters-not-released",
                  "Run parameters not released"},
        std::pair{"settings", "Settings"},
        std::pair{"settings-page", "Settings"},
        std::pair{"settings-time-zone", "Time / zone"},
        std::pair{"device-name", "Device name"},
        std::pair{"device-name-locked-run", "locked during run"},
        std::pair{"device-name-change-failed", "Name not changed"},
        std::pair{"program-name", "Name"},
        std::pair{"program-notes", "Note"},
        std::pair{"space", "Space"},
        std::pair{"kbd-lower", "abc"},
        std::pair{"kbd-upper", "ABC"},
        std::pair{"kbd-digits", "123"},
        std::pair{"kbd-symbols", "#+="},
        std::pair{"pf-name", "Name: "},
        std::pair{"pf-notes", "Note: "},
        std::pair{"pf-wait", "Product wait: "},
        std::pair{"pf-failure", "Probe fail: "},
        std::pair{"pf-delay", "Fallback in: "},
        std::pair{"pf-return", "Return: "},
        std::pair{"pf-reach", "Reach time: "},
        std::pair{"pt-wait", "Product wait"},
        std::pair{"pt-delay", "Fallback delay"},
        std::pair{"pt-reach", "Reach time"},
        std::pair{"policy-fallback", "Air after timeout"},
        std::pair{"policy-wait", "Wait for user"},
        std::pair{"policy-stop", "Stop safely"},
        std::pair{"return-air", "Stay on air"},
        std::pair{"return-manual", "Manual return"},
        std::pair{"return-auto", "Auto return"},
        std::pair{"discard", "Discard"},
        std::pair{"status-ready", "Application ready"},
        std::pair{"status-not-ready", "Application not ready"},
        std::pair{"recovery-mode-normal", "No recovery pending"},
        std::pair{"recovery-mode-waiting-for-trusted-time",
                  "Waiting for trusted time"},
        std::pair{"recovery-mode-current-run-recovered",
                  "Current run recovered"},
        std::pair{"recovery-mode-fallback-selection-required",
                  "Fallback selection required"},
        std::pair{"recovery-mode-rejected-or-fail-closed",
                  "Recovery rejected (fail-closed)"},
        std::pair{"recovery-mode-completed", "Run completed"},
        std::pair{"recovery-mode-cooling", "Cooling after recovery"},
    };
    const auto translated = [](const auto& source, const char* locale) {
        std::vector<TextTranslation> result;
        result.reserve(source.size());
        for (const auto& entry : source) {
            result.push_back(
                {{TextNamespace{"fermentation"}, entry.first}, entry.second});
        }
        if (std::string{locale} == "de") {
            const std::array<std::pair<const char*, const char*>, 182U> de{
                {std::pair{"standby", "Bereit"},
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
                 {"factory-reset-required",
                  "Wiederherstellung nur durch Werksreset"},
                 {"network-ap-only", "Nur AP"},
                 {"network-home-wifi", "Heimnetz"},
                 {"network-reconfigure", "Setup"},
                 {"network-current", "Aktueller Modus"},
                 {"network-mode-required", "Modus waehlen"},
                 {"network-browser-setup",
                  "Zugangsdaten: lokales Browser-Setup"},
                 {"network-ssid", "SSID: "},
                 {"network-password", "Passwort: "},
                 {"network-access-unavailable",
                  "Zugangsdaten nicht verfuegbar"},
                 {"network-ip-unavailable", "nicht verfuegbar"},
                 {"clock-trusted", "Zeit vertrauenswuerdig"},
                 {"clock-not-trusted", "Zeit nicht vertrauenswuerdig"},
                 {"web-access", "Webzugang"},
                 {"web-access-open", "Web-Setup"},
                 {"web-access-window-open", "Web-Setup frei (10 Min)"},
                 {"web-access-closed", "Web-Setup nicht freigegeben"},
                 {"web-access-unavailable", "Web-Setup nicht verfuegbar"},
                 {"message-product-insertion-requested", "Produkt einlegen"},
                 {"message-target-reach-time-exceeded",
                  "Zielzeit ueberschritten"},
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
                 {"manual-parameters-not-released",
                  "Laufparameter nicht freigegeben"},
                 {"settings", "Einstell."},
                 {"settings-page", "Einstellungen"},
                 {"settings-time-zone", "Zeit / Zeitzone"},
                 {"device-name", "Geraetename"},
                 {"device-name-locked-run", "gesperrt im Lauf"},
                 {"device-name-change-failed", "Name nicht geaendert"},
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
                 {"recovery-mode-current-run-recovered",
                  "Aktueller Lauf wiederhergestellt"},
                 {"recovery-mode-fallback-selection-required",
                  "Fallback-Auswahl noetig"},
                 {"recovery-mode-rejected-or-fail-closed",
                  "Wiederanlauf abgelehnt (fail-closed)"},
                 {"recovery-mode-completed", "Lauf abgeschlossen"},
                 {"recovery-mode-cooling", "Kuehlen nach Wiederanlauf"}}};
            for (const auto& replacement : de) {
                for (auto& entry : result) {
                    if (entry.key.value == replacement.first) {
                        entry.value = replacement.second;
                    }
                }
            }
        } else if (std::string{locale} == "es") {
            const std::array<std::pair<const char*, const char*>, 182U> es{
                {std::pair{"standby", "Listo"},
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
                 {"network-browser-setup",
                  "Credenciales: configuracion local en navegador"},
                 {"network-ssid", "SSID: "},
                 {"network-password", "Clave: "},
                 {"network-access-unavailable",
                  "Datos de acceso no disponibles"},
                 {"network-ip-unavailable", "no disponible"},
                 {"clock-trusted", "Hora fiable"},
                 {"clock-not-trusted", "Hora no fiable"},
                 {"web-access", "Acceso web"},
                 {"web-access-open", "Config. web"},
                 {"web-access-window-open", "Config. web permitida (10 min)"},
                 {"web-access-closed", "Config. web no permitida"},
                 {"web-access-unavailable", "Config. web no disponible"},
                 {"message-product-insertion-requested", "Insertar producto"},
                 {"message-target-reach-time-exceeded",
                  "Tiempo objetivo excedido"},
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
                 {"sensor-pref-air-product-optional",
                  "Aire, producto opcional"},
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
                 {"process-state-recovery-evaluation",
                  "Evaluando recuperacion"},
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
                 {"recovery-mode-waiting-for-trusted-time",
                  "Esperando hora fiable"},
                 {"recovery-mode-current-run-recovered",
                  "Proceso actual recuperado"},
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
                 {"discard", "Descartar"}}};
            for (const auto& replacement : es) {
                for (auto& entry : result) {
                    if (entry.key.value == replacement.first) {
                        entry.value = replacement.second;
                    }
                }
            }
        }
        return result;
    };
    return {
        {nameSpace, LocaleId{"de"}, capabilities, translated(entries, "de")},
        {nameSpace, LocaleId{"en"}, capabilities, translated(entries, "en")},
        {nameSpace, LocaleId{"es"}, capabilities, translated(entries, "es")},
    };
}

}  // namespace fermentation
