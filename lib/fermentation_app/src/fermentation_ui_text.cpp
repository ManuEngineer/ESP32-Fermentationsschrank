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
    const auto entries = std::array<std::pair<const char*, const char*>, 25U>{
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
    };
    const auto translated = [](const auto& source, const char* locale) {
        std::vector<TextTranslation> result;
        result.reserve(source.size());
        for (const auto& entry : source) {
            result.push_back(
                {{TextNamespace{"fermentation"}, entry.first}, entry.second});
        }
        if (std::string{locale} == "de") {
            const std::array<std::pair<const char*, const char*>, 25U> de{
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
                 {"resume-fallback", "Fallback fortsetzen"}}};
            for (const auto& replacement : de) {
                for (auto& entry : result) {
                    if (entry.key.value == replacement.first) {
                        entry.value = replacement.second;
                    }
                }
            }
        } else if (std::string{locale} == "es") {
            const std::array<std::pair<const char*, const char*>, 25U> es{
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
                 {"resume-fallback", "Reanudar respaldo"}}};
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
