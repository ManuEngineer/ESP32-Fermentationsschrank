#pragma once

#include <cstdint>

namespace device_platform {

// Anwendungsneutraler technischer Werttyp fuer Speicheridentifikatoren.
// Verhindert das versehentliche Vermischen von Zaehlern mit gleichem
// numerischem Wert, aber unterschiedlicher fachlicher Bedeutung (siehe
// docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Schema, Revision und
// Migration"). Kennt keine konkrete Bedeutung; die aufrufende Anwendung
// entscheidet, wofuer ein Wert steht.
template <typename Tag, typename Underlying>
class StrongId {
   public:
    using ValueType = Underlying;

    constexpr StrongId() = default;
    constexpr explicit StrongId(Underlying value) : value_(value) {}

    [[nodiscard]] constexpr Underlying value() const { return value_; }

    friend constexpr bool operator==(StrongId left, StrongId right) {
        return left.value_ == right.value_;
    }
    friend constexpr bool operator!=(StrongId left, StrongId right) {
        return !(left == right);
    }
    friend constexpr bool operator<(StrongId left, StrongId right) {
        return left.value_ < right.value_;
    }
    friend constexpr bool operator>(StrongId left, StrongId right) {
        return right < left;
    }
    friend constexpr bool operator<=(StrongId left, StrongId right) {
        return !(right < left);
    }
    friend constexpr bool operator>=(StrongId left, StrongId right) {
        return !(left < right);
    }

   private:
    Underlying value_{0};
};

namespace detail {
struct StorageEpochTag {};
struct RevisionTag {};
struct GenerationTag {};
struct RecordSequenceTag {};
struct SlotIdTag {};
struct RecordTypeIdTag {};
}  // namespace detail

// Speicherepoche; 0 ist reserviert und niemals ein gueltiger gespeicherter
// Wert. Ein Werksreset erhoeht die Epoche, ohne dass ein Ueberlauf still
// entsteht (Ueberlaufpruefung liegt beim Aufrufer, der die Epoche
// fortschreibt).
using StorageEpoch = StrongId<detail::StorageEpochTag, uint64_t>;

// Dokumentrevision. 0 ist reserviert; Zaehlung beginnt bei 1.
using Revision = StrongId<detail::RevisionTag, uint64_t>;

// Manifest- beziehungsweise Konfigurationsgeneration. 0 ist reserviert;
// Zaehlung beginnt bei 1.
using Generation = StrongId<detail::GenerationTag, uint64_t>;

// Monotone Mutationssequenz. 0 ist reserviert; Zaehlung beginnt bei 1.
using RecordSequence = StrongId<detail::RecordSequenceTag, uint64_t>;

// Logischer Index eines physischen Speicherplatzes innerhalb einer festen
// Slotmenge. Kennt keine Slotanzahl oder fachliche Bedeutung; beides legt die
// aufrufende Anwendung fest.
using SlotId = StrongId<detail::SlotIdTag, uint32_t>;

// Technischer Recordtyp-Bezeichner im Envelope. 0 ist reserviert und niemals
// gueltig. Die konkrete fachliche Bedeutung eines Record-Typs kennt
// `device_platform` nicht.
using RecordTypeId = StrongId<detail::RecordTypeIdTag, uint16_t>;

}  // namespace device_platform
