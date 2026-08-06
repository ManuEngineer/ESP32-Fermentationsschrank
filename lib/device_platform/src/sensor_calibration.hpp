#pragma once

#include "sensor_identity.hpp"
#include "sensor_offset.hpp"

namespace device_platform {

// Bindet zwei bereits gueltig-by-construction erzeugte Werte. Die Kombination
// benoetigt keine zweite Validierungs- oder Persistenzschicht.
class SensorCalibration {
   public:
    SensorCalibration(SensorIdentity identity, SensorOffset offset)
        : identity_(identity), offset_(offset) {}

    [[nodiscard]] SensorIdentity identity() const { return identity_; }
    [[nodiscard]] SensorOffset offset() const { return offset_; }

   private:
    SensorIdentity identity_;
    SensorOffset offset_;
};

}  // namespace device_platform
