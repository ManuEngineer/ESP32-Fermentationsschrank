#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "bidirectional_actuator_sink.hpp"

namespace device_platform_test_support {

enum class BidirectionalActuatorCommandKind : std::uint8_t {
    Forward,
    Reverse,
};

struct BidirectionalActuatorCommand {
    BidirectionalActuatorCommandKind kind;
    bool enabled;
};

// Mock fuer einen bidirektionalen Aktor. Haelt den aktuellen Zustand beider
// Richtungen, journalisiert jeden Befehl (fest begrenzt, aelteste Eintraege
// werden verworfen) und macht eine jemals gleichzeitig aktive Vorwaerts- und
// Rueckwaertsfreigabe dauerhaft sichtbar, statt sie stillschweigend
// zuzulassen.
class MockBidirectionalActuatorSink final
    : public device_platform::IBidirectionalActuatorSink {
   public:
    static constexpr std::size_t kMaxJournalEntries = 256;

    void setForward(bool enabled) override;
    void setReverse(bool enabled) override;

    [[nodiscard]] bool forward() const;
    [[nodiscard]] bool reverse() const;

    [[nodiscard]] const std::vector<BidirectionalActuatorCommand>&
    commandJournal() const;

    // Bleibt nach der ersten Beobachtung dauerhaft `true`, auch wenn spaeter
    // nur noch eine Richtung aktiv ist. So kann ein Test eine gleichzeitige
    // Freigabe sicher erkennen, selbst wenn sie nur einen Zyklus lang bestand.
    [[nodiscard]] bool simultaneousActivationObserved() const;

   private:
    void record(BidirectionalActuatorCommandKind kind, bool enabled);
    void checkSimultaneousActivation();

    bool forward_{false};
    bool reverse_{false};
    bool simultaneousActivationObserved_{false};
    std::vector<BidirectionalActuatorCommand> journal_;
};

}  // namespace device_platform_test_support
