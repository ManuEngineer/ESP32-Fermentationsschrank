#include "fermentation_application.hpp"

namespace fermentation {

bool FermentationApplication::begin(
    device_platform::IPlatformServices& platformServices) {
    if (!platformServices.ready()) {
        return false;
    }

    platformServices_ = &platformServices;
    return true;
}

void FermentationApplication::update() {
    // Die Fermentationslogik wird issueweise in diesem Modul implementiert.
}

bool FermentationApplication::ready() const {
    return platformServices_ != nullptr && platformServices_->ready();
}

}  // namespace fermentation
