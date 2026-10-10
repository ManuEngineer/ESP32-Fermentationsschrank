#pragma once

namespace device_platform {

// Anwendungsneutraler Port fuer genau einen binaeren Ausgang. Welche
// physische Rolle ein konkreter Ausgang hat (zum Beispiel ein Luefter oder
// ein Summer), entscheidet ausschliesslich die Anwendung beziehungsweise die
// Composition Root ueber die Zuordnung der konkreten Instanz. Der Port selbst
// kennt keine solchen Rollen.
class IBinaryOutputSink {
   public:
    IBinaryOutputSink() = default;
    virtual ~IBinaryOutputSink() = default;

    IBinaryOutputSink(const IBinaryOutputSink&) = delete;
    IBinaryOutputSink& operator=(const IBinaryOutputSink&) = delete;
    IBinaryOutputSink(IBinaryOutputSink&&) = delete;
    IBinaryOutputSink& operator=(IBinaryOutputSink&&) = delete;

    // Liefert `true` ausschliesslich, wenn der Befehl in einem
    // betriebsbereiten Zustand erfolgreich am Ausgang ausgefuehrt wurde.
    // Das ist keine Aussage ueber die Last (zum Beispiel Luefterdrehung).
    // `false` bei nicht gestartetem, unbestaetigtem oder verriegeltem
    // Ausgang, verworfenem Befehl oder Treiberfehler; ein Best-effort-AUS in
    // einem verriegelten Zustand liefert ebenfalls `false`.
    [[nodiscard]] virtual bool setEnabled(bool enabled) = 0;
};

}  // namespace device_platform
