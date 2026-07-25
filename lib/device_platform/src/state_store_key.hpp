#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

// Anwendungsneutraler technischer Schluesseltyp fuer `IStateStore`. Siehe
// docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Speicherport und
// Modulgrenzen".
namespace device_platform {

enum class StateStoreKeyStatus : uint8_t {
    Success,
    // `bytes` ist leer. Ein leerer Schluessel ist kein gueltiger technischer
    // Schluessel (mindestens ein Byte erforderlich).
    Empty,
    // `bytes.size()` uebersteigt `StateStoreKey::kMaxLength`.
    TooLong,
    // Mindestens ein Byte liegt ausserhalb des erlaubten Zeichensatzes
    // `[A-Za-z0-9_.-]`.
    InvalidCharacter,
};

// Vorwaertsdeklariert, da `StateStoreKey::create()` diesen Typ als
// Rueckgabetyp deklariert, bevor er selbst (nach `StateStoreKey`) vollstaendig
// definiert werden kann - er enthaelt ein `std::optional<StateStoreKey>` und
// braucht dafuer den bereits vollstaendigen `StateStoreKey`-Typ.
struct StateStoreKeyCreateResult;

// Begrenzter Schluesselwert: 1 bis `kMaxLength` Bytes, jedes Byte aus
// `[A-Za-z0-9_.-]`. NUL, Leerzeichen und Pfadtrennzeichen sind unzulaessig.
// Kennt keine konkrete Konfigurations-, Manifest-, Root- oder
// Anwendungsschluesselbedeutung - welcher Schluessel wofuer steht, entscheidet
// ausschliesslich die aufrufende Anwendung.
//
// Gueltig-by-construction: es gibt keinen oeffentlichen Konstruktor. Jede
// existierende Instanz enthaelt daher garantiert 1..kMaxLength zulaessige
// Bytes; `create()` ist der einzige Erzeugungsweg und liefert bei einem
// leeren, zu langen oder zeichensatzverletzenden Eingabewert keinen Wert -
// ein ungueltiger Schluessel kann dadurch nie still als gueltiges
// `IStateStore`-Portargument verwendet werden.
//
// Der Schluesselraum und die Laengengrenze werden gemaess ADR-016 im Port
// erzwungen, nicht in einem Adapter: 15 Zeichen aus `[A-Za-z0-9_.-]` sind der
// kleinste gemeinsame Nenner plausibler Backends (NVS-Schluessel: ASCII,
// hoechstens 15 Zeichen; ebenso gueltig fuer dateibasierte Backends und in
// Logs lesbar). Die Nutzlast (`IStateStore`-Wert) bleibt davon unberuehrt
// binaersicher.
class StateStoreKey {
   public:
    static constexpr std::size_t kMaxLength = 15U;

    [[nodiscard]] static StateStoreKeyCreateResult create(std::string bytes);

    [[nodiscard]] const std::string& bytes() const { return bytes_; }
    [[nodiscard]] std::size_t size() const { return bytes_.size(); }

    friend bool operator==(const StateStoreKey& left,
                           const StateStoreKey& right) {
        return left.bytes_ == right.bytes_;
    }
    friend bool operator!=(const StateStoreKey& left,
                           const StateStoreKey& right) {
        return !(left == right);
    }
    // Fuer deterministische Verwendung als Ordnungs-/Map-Schluessel (z. B. im
    // nativen Testsimulator); keine fachliche Sortierbedeutung. `std::string`
    // vergleicht `char` fuer `lt`/`compare` normativ als `unsigned char`
    // (siehe [char.traits.specializations.char]), die Ordnung ist damit
    // unabhaengig von der (ggf. signed) nativen `char`-Darstellung
    // deterministisch nach unsigned Bytewerten.
    friend bool operator<(const StateStoreKey& left,
                          const StateStoreKey& right) {
        return left.bytes_ < right.bytes_;
    }

   private:
    explicit StateStoreKey(std::string bytes) : bytes_(std::move(bytes)) {}
    std::string bytes_;
};

static_assert(!std::is_default_constructible_v<StateStoreKey>,
              "StateStoreKey muss gueltig-by-construction bleiben: kein "
              "oeffentlicher Default-Zustand, der ungeprueft an IStateStore "
              "uebergeben werden koennte.");

struct StateStoreKeyCreateResult {
    StateStoreKeyStatus status{StateStoreKeyStatus::Empty};
    // Nur bei `status == Success` gesetzt.
    std::optional<StateStoreKey> key;
};

inline StateStoreKeyCreateResult StateStoreKey::create(std::string bytes) {
    if (bytes.empty()) {
        return StateStoreKeyCreateResult{StateStoreKeyStatus::Empty,
                                         std::nullopt};
    }
    if (bytes.size() > kMaxLength) {
        return StateStoreKeyCreateResult{StateStoreKeyStatus::TooLong,
                                         std::nullopt};
    }
    const auto isAllowed = [](char byte) {
        const auto value = static_cast<unsigned char>(byte);
        return (value >= 'A' && value <= 'Z') ||
               (value >= 'a' && value <= 'z') ||
               (value >= '0' && value <= '9') || value == '_' || value == '.' ||
               value == '-';
    };
    for (const char byte : bytes) {
        if (!isAllowed(byte)) {
            return StateStoreKeyCreateResult{
                StateStoreKeyStatus::InvalidCharacter, std::nullopt};
        }
    }
    return StateStoreKeyCreateResult{
        StateStoreKeyStatus::Success,
        StateStoreKey(std::move(bytes)),
    };
}

}  // namespace device_platform
