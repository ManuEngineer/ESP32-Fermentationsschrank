#pragma once

#if defined(APP_ISSUE_32_OUTPUT_PULSE)

#include <array>
#include <cstddef>
#include <cstdint>

#include "binary_output_sink.hpp"

namespace fermentation::issue_32_output_pulse {

// Bring-up-only, manueller HIGH=EIN-Test einzelner Ausgaenge (OUT1/GPIO16
// Innenluefter, OUT2/GPIO17 Aussenluefter, OUT4/GPIO27 Reserve, Issue #32). Ein
// UART-Zeilenkommando loest hoechstens einen kurzen EIN/AUS-Zyklus aus; es
// gibt keinen automatischen EIN-Test beim Boot. Das Release-Profil enthaelt
// weder diese Datei noch einen Aufrufer. Kein Planner-/Driver-Pfad, keine
// Aktorfreigabe.
class Harness {
   public:
    Harness(device_platform::IBinaryOutputSink& out1,
            device_platform::IBinaryOutputSink& out2,
            device_platform::IBinaryOutputSink& out4) noexcept
        : out1_(out1), out2_(out2), out4_(out4) {}

    void start() noexcept;
    void update() noexcept;

   private:
    void processLine() noexcept;
    void beginPulse(device_platform::IBinaryOutputSink& sink,
                    const char* name) noexcept;
    void finishPulse(const char* result, const char* reason) noexcept;

    device_platform::IBinaryOutputSink& out1_;
    device_platform::IBinaryOutputSink& out2_;
    device_platform::IBinaryOutputSink& out4_;
    device_platform::IBinaryOutputSink* active_{nullptr};
    const char* activeName_{nullptr};
    std::int64_t deadlineUs_{0};
    std::array<char, 64U> line_{};
    std::size_t lineLength_{0U};
    bool lineOverflow_{false};
};

}  // namespace fermentation::issue_32_output_pulse

#endif
