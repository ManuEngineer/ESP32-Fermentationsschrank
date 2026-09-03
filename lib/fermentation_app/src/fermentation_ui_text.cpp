#include "fermentation_ui_text.hpp"

namespace fermentation {

std::vector<device_platform::TextPackManifest> makeFermentationUiTextPacks() {
    using device_platform::LocaleId;
    using device_platform::TextKey;
    using device_platform::TextNamespace;
    using device_platform::TextPackCapabilities;
    using device_platform::TextPackManifest;
    const TextNamespace nameSpace{"fermentation"};
    const auto capabilities = TextPackCapabilities{"latin-de-en-es", 48U, true};
    return {
        {nameSpace,
         LocaleId{"de"},
         capabilities,
         {{{nameSpace, "standby"}, "Bereit"},
          {{nameSpace, "running"}, "Prozess laeuft"},
          {{nameSpace, "recovery"}, "Wiederherstellung"},
          {{nameSpace, "resume-fallback"}, "Fallback fortsetzen"},
          {{nameSpace, "service-required"}, "Service erforderlich"}}},
        {nameSpace,
         LocaleId{"en"},
         capabilities,
         {{{nameSpace, "standby"}, "Ready"},
          {{nameSpace, "running"}, "Process running"},
          {{nameSpace, "recovery"}, "Recovery"},
          {{nameSpace, "resume-fallback"}, "Resume fallback"},
          {{nameSpace, "service-required"}, "Service required"}}},
        {nameSpace,
         LocaleId{"es"},
         capabilities,
         {{{nameSpace, "standby"}, "Listo"},
          {{nameSpace, "running"}, "Proceso en curso"},
          {{nameSpace, "recovery"}, "Recuperacion"},
          {{nameSpace, "resume-fallback"}, "Reanudar respaldo"},
          {{nameSpace, "service-required"}, "Servicio requerido"}}},
    };
}

}  // namespace fermentation
