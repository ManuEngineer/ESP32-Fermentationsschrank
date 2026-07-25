#pragma once

#include <cstdint>
#include <limits>

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

enum class CheckedIncrementStatus : uint8_t {
    Success,
    // `current` ist der reservierte Wert 0, der fuer StorageEpoch, Revision,
    // Generation und RecordSequence niemals ein gueltiger gespeicherter Wert
    // ist. Ein Fortschreiben von 0 wuerde einen ungueltigen Ausgangswert
    // stillschweigend in einen scheinbar gueltigen Wert 1 verwandeln. `out`
    // bleibt in diesem Fall unveraendert. Der erste gueltige Wert 1 muss
    // explizit vom zustaendigen Bootstrap-/Anwendungscode erzeugt werden,
    // nicht durch Inkrementieren von 0.
    InvalidCurrentValue,
    // `current` ist bereits der maximal darstellbare Wert; ein Fortschreiben
    // wuerde stillschweigend auf 0 (den reservierten, niemals gueltigen
    // Wert) ueberlaufen. `out` bleibt in diesem Fall unveraendert.
    Overflow,
};

// Generischer, anwendungsneutraler Checked-Increment-Baustein fuer die
// starken uint64_t-Zaehlertypen (StorageEpoch, Revision, Generation,
// RecordSequence). Erhoeht einen bereits gueltigen (d. h. von 0
// verschiedenen) Wert um genau eins und lehnt sowohl den reservierten
// Ausgangswert 0 als auch einen Ueberlauf von UINT64_MAX auf 0 stabil ab,
// statt eine der beiden Situationen still in einen scheinbar gueltigen Wert
// zu verwandeln. Die Tags der starken Typen bleiben getrennt: ein
// `StorageEpoch` laesst sich nicht versehentlich als `Revision`
// fortschreiben. Kennt keine konkrete MutationSequence-Reservierung, Root-
// oder Anwendungstransaktionslogik - das bleibt Aufgabe der aufrufenden
// Anwendung (siehe docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Schema,
// Revision und Migration").
template <typename Tag>
[[nodiscard]] constexpr CheckedIncrementStatus checkedIncrement(
    StrongId<Tag, uint64_t> current, StrongId<Tag, uint64_t>& out) {
    const uint64_t raw = current.value();
    if (raw == 0U) {
        return CheckedIncrementStatus::InvalidCurrentValue;
    }
    if (raw == std::numeric_limits<uint64_t>::max()) {
        return CheckedIncrementStatus::Overflow;
    }
    out = StrongId<Tag, uint64_t>(raw + 1U);
    return CheckedIncrementStatus::Success;
}

}  // namespace device_platform
