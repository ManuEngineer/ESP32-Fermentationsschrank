#pragma once

#include <cstdint>
#include <optional>

// Anwendungsneutraler Sensoridentitaetstyp (z. B. eine 1-Wire-ROM-Adresse).
// Bewusst eigenstaendig und OHNE Abhaengigkeit auf storage_types.hpp: ein
// Sensorwerttyp haengt fachlich nicht vom Speichermodul ab (siehe
// docs/tasks/issue-20-sensor-quality-filtering-plan.md, Abschnitt 13a).
namespace device_platform {

enum class SensorIdentityStatus : uint8_t {
    Success,
    // 0 ist kein gueltiger Identitaetswert - er waere von einer "unbekannt"-
    // Bedeutung nicht unterscheidbar. "Unbekannt" wird ausschliesslich durch
    // Abwesenheit (std::optional<SensorIdentity>) dargestellt, nie durch
    // einen Wert innerhalb dieses Typs.
    ZeroIsNotAValidIdentity,
};

// Vorwaertsdeklariert, da sie ein std::optional<SensorIdentity> enthaelt und
// daher erst nach der vollstaendigen Definition von SensorIdentity folgen
// kann (siehe state_store_key.hpp-Muster).
struct SensorIdentityCreateResult;

// Gueltig-by-construction: kein oeffentlicher Konstruktor, `create()` ist der
// einzige Erzeugungsweg und lehnt 0 ab.
class SensorIdentity {
   public:
    [[nodiscard]] static SensorIdentityCreateResult create(uint64_t value);

    [[nodiscard]] constexpr uint64_t value() const { return value_; }

    friend constexpr bool operator==(SensorIdentity a, SensorIdentity b) {
        return a.value_ == b.value_;
    }
    friend constexpr bool operator!=(SensorIdentity a, SensorIdentity b) {
        return !(a == b);
    }

   private:
    constexpr explicit SensorIdentity(uint64_t value) : value_(value) {}
    uint64_t value_;
};

struct SensorIdentityCreateResult {
    SensorIdentityStatus status{SensorIdentityStatus::ZeroIsNotAValidIdentity};
    std::optional<SensorIdentity> identity;
};

inline SensorIdentityCreateResult SensorIdentity::create(uint64_t value) {
    if (value == 0U) {
        return SensorIdentityCreateResult{
            SensorIdentityStatus::ZeroIsNotAValidIdentity, std::nullopt};
    }
    return SensorIdentityCreateResult{SensorIdentityStatus::Success,
                                      SensorIdentity(value)};
}

}  // namespace device_platform
