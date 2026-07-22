#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "actuator_sink.hpp"

namespace device_platform {

enum class ActuatorCommandKind : std::uint8_t {
    Heating,
    Cooling,
    InsideFan,
    OutsideFan,
    Buzzer,
};

struct ActuatorCommand {
    ActuatorCommandKind kind;
    bool enabled;
};

// Mock-Aktorsenke fuer native Tests. Haelt den aktuellen Zustand aller
// Ausgaenge, journalisiert jeden Befehl (fest begrenzt, aelteste Eintraege
// werden verworfen) und macht eine jemals gleichzeitig aktive Heiz- und
// Kuehlfreigabe sichtbar, statt sie stillschweigend zuzulassen.
class MockActuatorSink final : public IActuatorSink {
   public:
    static constexpr std::size_t kMaxJournalEntries = 256;

    void setHeating(bool enabled) override;
    void setCooling(bool enabled) override;
    void setInsideFan(bool enabled) override;
    void setOutsideFan(bool enabled) override;
    void setBuzzer(bool enabled) override;

    [[nodiscard]] bool heating() const;
    [[nodiscard]] bool cooling() const;
    [[nodiscard]] bool insideFan() const;
    [[nodiscard]] bool outsideFan() const;
    [[nodiscard]] bool buzzer() const;

    [[nodiscard]] const std::vector<ActuatorCommand>& commandJournal() const;

    // Bleibt nach der ersten Beobachtung dauerhaft `true`, auch wenn spaeter
    // nur noch eine Richtung aktiv ist. So kann ein Test eine gleichzeitige
    // Freigabe sicher erkennen, selbst wenn sie nur einen Zyklus lang bestand.
    [[nodiscard]] bool simultaneousDirectionsObserved() const;

   private:
    void record(ActuatorCommandKind kind, bool enabled);
    void checkSimultaneousDirections();

    bool heating_{false};
    bool cooling_{false};
    bool insideFan_{false};
    bool outsideFan_{false};
    bool buzzer_{false};
    bool simultaneousDirectionsObserved_{false};
    std::vector<ActuatorCommand> journal_;
};

}  // namespace device_platform
