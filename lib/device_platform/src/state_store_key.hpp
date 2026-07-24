#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

// Anwendungsneutraler technischer Schluesseltyp fuer `IStateStore`. Siehe
// docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Speicherport und
// Modulgrenzen".
namespace device_platform {

enum class StateStoreKeyStatus : uint8_t {
    Success,
    // `bytes.size()` uebersteigt `StateStoreKey::kMaxLength`.
    TooLong,
};

// Begrenzter, binaersicherer Schluesselwert (beliebige Bytewerte inklusive
// eingebettetem NUL sind zulaessig; es gibt keine Zeichensatzeinschraenkung).
// Kennt keine konkrete Konfigurations-, Manifest-, Root- oder
// Anwendungsschluesselbedeutung - welcher Bytewert wofuer steht, entscheidet
// ausschliesslich die aufrufende Anwendung. Instanzen
// entstehen ausschliesslich ueber `create()`, damit ein zu langer Schluessel
// nicht still als gueltiges Portargument verwendet werden kann.
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

    StateStoreKey() = default;

    [[nodiscard]] static StateStoreKeyStatus create(std::string bytes,
                                                    StateStoreKey& out) {
        if (bytes.size() > kMaxLength) {
            return StateStoreKeyStatus::TooLong;
        }
        out.bytes_ = std::move(bytes);
        return StateStoreKeyStatus::Success;
    }

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
    // nativen Testsimulator); keine fachliche Sortierbedeutung.
    friend bool operator<(const StateStoreKey& left,
                          const StateStoreKey& right) {
        return left.bytes_ < right.bytes_;
    }

   private:
    std::string bytes_;
};

}  // namespace device_platform
