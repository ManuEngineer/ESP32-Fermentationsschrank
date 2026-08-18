#include "fermentation_application.hpp"

namespace fermentation {

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices,
    device_platform::IResetCauseSource* resetCauseSource) {
    if (!platformServices.ready()) {
        return false;
    }

    platformServices_ = &platformServices;
    safetyCore_.beginBoot(resetCauseSource == nullptr
                              ? device_platform::ResetCause::Unknown
                              : resetCauseSource->resetCause());
    // The composition root has not supplied the real configuration,
    // persistence, sensor and planner evidence yet.  Treat that absence as
    // untrusted and keep the application fail-closed until a later explicit
    // validation path provides it.
    const SafetyCoreInput bootEvidence;
    static_cast<void>(safetyCore_.evaluate(bootEvidence));
    return true;
}

void FermentationApplication::update() {
    // Die Fermentationslogik wird issueweise in diesem Modul implementiert.
}

bool FermentationApplication::ready() const {
    return platformServices_ != nullptr && platformServices_->ready();
}

}  // namespace fermentation
