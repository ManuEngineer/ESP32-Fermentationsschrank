#pragma once

#include <cstdint>

#include "binary_output_sink.hpp"
#include "output_polarity.hpp"

namespace device_platform_esp_idf {

enum class BinaryOutputBeginResult : std::uint8_t {
    PolarityUnconfirmed,
    Ready,
    Failed,
};

// Konkreter GPIO-Adapter fuer genau einen binaeren Ausgang. Der Adapter kennt
// keine Rolle. Mit `OutputPolarity::Unconfirmed` fuehrt er keinerlei
// GPIO-Operation aus (der Pin bleibt unberuehrt); das ist keine Aussage, dass
// ein angeschlossener Verbraucher aus ist. Ein Einschalten ist nur nach
// erfolgreichem `begin()` moeglich; jeder Fehler ist fail-closed.
class EspIdfBinaryOutputSink final : public device_platform::IBinaryOutputSink {
   public:
    EspIdfBinaryOutputSink(int gpioNumber,
                           device_platform::OutputPolarity polarity) noexcept;

    [[nodiscard]] BinaryOutputBeginResult begin() noexcept;
    [[nodiscard]] bool setEnabled(bool enabled) override;

   private:
    enum class State : std::uint8_t { NotStarted, Unconfirmed, Ready, Faulted };

    void driveInactiveBestEffort() noexcept;
    [[nodiscard]] int inactiveLevel() const noexcept;
    [[nodiscard]] int levelFor(bool enabled) const noexcept;

    int gpioNumber_;
    device_platform::OutputPolarity polarity_;
    State state_{State::NotStarted};
};

}  // namespace device_platform_esp_idf
