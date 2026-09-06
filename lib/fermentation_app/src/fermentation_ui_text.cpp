#include "fermentation_ui_text.hpp"

#include <array>
#include <string>
#include <utility>

namespace fermentation {

device_platform::TextKey fermentationTextKey(const char* value) {
    return {device_platform::TextNamespace{"fermentation"}, value};
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
    const auto entries = std::array<std::pair<const char*, const char*>, 53U>{
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
    };
    const auto translated = [](const auto& source, const char* locale) {
        std::vector<TextTranslation> result;
        result.reserve(source.size());
        for (const auto& entry : source) {
            result.push_back(
                {{TextNamespace{"fermentation"}, entry.first}, entry.second});
        }
        if (std::string{locale} == "de") {
            const std::array<std::pair<const char*, const char*>, 53U> de{
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
                 {"program-invalid", "Programm ungueltig"}}};
            for (const auto& replacement : de) {
                for (auto& entry : result) {
                    if (entry.key.value == replacement.first) {
                        entry.value = replacement.second;
                    }
                }
            }
        } else if (std::string{locale} == "es") {
            const std::array<std::pair<const char*, const char*>, 53U> es{
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
                 {"program-invalid", "Programa no valido"}}};
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
