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

std::vector<device_platform::TextPackManifest> makeFermentationUiTextPacks() {
    using device_platform::LocaleId;
    using device_platform::TextKey;
    using device_platform::TextNamespace;
    using device_platform::TextPackCapabilities;
    using device_platform::TextPackManifest;
    using device_platform::TextTranslation;
    const TextNamespace nameSpace{"fermentation"};
    const auto capabilities = TextPackCapabilities{"latin-de-en-es", 48U, true};
    const auto entries = std::array<std::pair<const char*, const char*>, 88U>{
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
    };
    const auto translated = [](const auto& source, const char* locale) {
        std::vector<TextTranslation> result;
        result.reserve(source.size());
        for (const auto& entry : source) {
            result.push_back(
                {{TextNamespace{"fermentation"}, entry.first}, entry.second});
        }
        if (std::string{locale} == "de") {
            const std::array<std::pair<const char*, const char*>, 88U> de{
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
                 {"language-change-failed", "Sprache nicht geaendert"}}};
            for (const auto& replacement : de) {
                for (auto& entry : result) {
                    if (entry.key.value == replacement.first) {
                        entry.value = replacement.second;
                    }
                }
            }
        } else if (std::string{locale} == "es") {
            const std::array<std::pair<const char*, const char*>, 88U> es{
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
                 {"language-change-failed", "Idioma no cambiado"}}};
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
