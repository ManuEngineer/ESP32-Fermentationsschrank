#pragma once

#if defined(APP_ISSUE_90_SLICE7_HARNESS)

#include <array>
#include <cstddef>
#include <cstdint>

#include "esp_timer_time_source.hpp"
#include "fermentation_application.hpp"

namespace fermentation::issue_90_slice7 {

class Harness final {
   public:
    explicit Harness(FermentationApplication& application) noexcept;

    void start() noexcept;
    void update() noexcept;

   private:
    enum class LoadMode : std::uint8_t { Stopped, Configuration, Run };

    void pollUart() noexcept;
    void processLine() noexcept;
    void emitStatus() const noexcept;
    void emitCommandResult(const char* command, bool passed) const noexcept;
    void arm(LoadMode mode) noexcept;
    void runLoadStep(std::uint64_t nowMicros) noexcept;
    [[nodiscard]] std::uint64_t nowMicros() const noexcept;
    [[nodiscard]] bool writeConfiguration() noexcept;
    [[nodiscard]] bool writeRun() noexcept;
    [[nodiscard]] bool stopActiveRun() noexcept;
    [[nodiscard]] bool discardPendingRun() noexcept;
    void requestRestart(const char* command) noexcept;

    FermentationApplication& application_;
    std::array<char, 160U> line_{};
    std::size_t lineLength_{0U};
    bool lineOverflow_{false};
    LoadMode loadMode_{LoadMode::Stopped};
    std::uint32_t loadIteration_{0U};
    std::uint64_t nextLoadAtMicros_{0U};
    std::uint64_t nextCommandId_{0x90000000U};
    device_platform_esp_idf::EspTimerTimeSource timeSource_;
};

}  // namespace fermentation::issue_90_slice7

#endif
