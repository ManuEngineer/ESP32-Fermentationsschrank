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

    virtual void setEnabled(bool enabled) = 0;
};

}  // namespace device_platform
