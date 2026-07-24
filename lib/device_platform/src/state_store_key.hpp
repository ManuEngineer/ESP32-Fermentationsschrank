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
};

// Vorwaertsdeklariert, da `StateStoreKey::create()` diesen Typ als
// Rueckgabetyp deklariert, bevor er selbst (nach `StateStoreKey`) vollstaendig
// definiert werden kann - er enthaelt ein `std::optional<StateStoreKey>` und
// braucht dafuer den bereits vollstaendigen `StateStoreKey`-Typ.
struct StateStoreKeyCreateResult;

// Begrenzter, binaersicherer Schluesselwert (beliebige Bytewerte inklusive
// eingebettetem NUL sind zulaessig; es gibt keine Zeichensatzeinschraenkung).
// Kennt keine konkrete Konfigurations-, Manifest-, Root- oder
// Anwendungsschluesselbedeutung - welcher Bytewert wofuer steht, entscheidet
// ausschliesslich die aufrufende Anwendung.
//
// Gueltig-by-construction: es gibt keinen oeffentlichen Konstruktor. Jede
// existierende Instanz enthaelt daher garantiert 1..kMaxLength Bytes;
// `create()` ist der einzige Erzeugungsweg und liefert bei einem leeren
// oder zu langen Eingabewert keinen Wert - ein ungueltiger Schluessel kann
// dadurch nie still als gueltiges `IStateStore`-Portargument verwendet
// werden.
//
// `kMaxLength` ist eine kleine, anwendungsneutrale Softwaregrenze dieses
// Ports und keine reale NVS- oder Flash-Garantie: sie ist bewusst nicht an
// die ESP32-NVS-Schluesselgrenze (technisch 15 Zeichen) gebunden, damit
// `device_platform` hardwareneutral bleibt (siehe
// lib/device_platform/AGENTS.md). Ein produktiver ESP32-Adapter, der echte
// NVS-Schluessel abbildet, muss eine engere Grenze eigenverantwortlich selbst
// durchsetzen.
class StateStoreKey {
   public:
    static constexpr std::size_t kMaxLength = 32U;

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
    return StateStoreKeyCreateResult{
        StateStoreKeyStatus::Success,
        StateStoreKey(std::move(bytes)),
    };
}

}  // namespace device_platform
