#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

#include "ds18b20_bus.hpp"
#include "ds18b20_channel_source.hpp"
#include "ds18b20_sampling_engine.hpp"
#include "time_source.hpp"

// Sampling-Task des DS18B20-Pfads (Issue #30, Plan 4/5.4). Die Treiber
// blockieren, deshalb laeuft die gesamte Busarbeit in genau einem Task fuer
// beide Busse; die Hauptschleife ruft nie eine Busfunktion. Anwendungsneutral:
// nur technische Kanaele (ADR-013).
namespace device_platform_esp_idf {

// Hardware-Budgetgroessen des Tasks. Sie werden nur nach Hardwaremessung und
// Ownerfreigabe gesetzt (Plan O4/6.1 H3); es gibt keinen Standardwert und nie
// einen geratenen oder TBD-Wert.
struct Ds18b20TaskBudget {
    uint32_t stackBytes{0U};
    uint32_t priority{0U};
    int32_t coreId{0};
};

// Vom Owner freigegebenes Produktbudget (Issue #190, C3): Stack 4096 B,
// Prioritaet 2, Core 1. Grundlage: hoechster beobachteter Stackbedarf 1376 B
// bei 6144 B Probe-Budget (Bring-up-Messung, docs/audits/
// ISSUE190_DS18B20_BRINGUP_EVIDENCE.md, M6); die Fehlerpfade CrcFault/BusFault
// sind nicht gemessen und durch die Reserve abgedeckt. Wird das Budget wieder
// entfernt (std::nullopt), erzeugt start() weder Task noch Bus und alle Kanaele
// bleiben MissingSample (fail-closed).
inline constexpr std::optional<Ds18b20TaskBudget> kApprovedDs18b20TaskBudget =
    Ds18b20TaskBudget{4096U, 2U, 1};

enum class Ds18b20SamplerStartResult : uint8_t {
    Started,
    BudgetNotApproved,
    AlreadyStarted,
    InvalidBudget,
    BusUnavailable,
    TaskCreateFailed,
};

[[nodiscard]] const char* ds18b20SamplerStartResultName(
    Ds18b20SamplerStartResult result) noexcept;

class Ds18b20Sampler {
   public:
    // Die Engine bedient zwei Weiterleitungsbusse; die echten RMT-Busse
    // entstehen erst in start().
    explicit Ds18b20Sampler(const device_platform::ITimeSource& time);
    ~Ds18b20Sampler();
    Ds18b20Sampler(const Ds18b20Sampler&) = delete;
    Ds18b20Sampler& operator=(const Ds18b20Sampler&) = delete;

    // `expectedRomBusGpio`: Bus 0 (Kanaele 0/1), `singleDeviceBusGpio`: Bus 1
    // (Kanal 2). Die Bindung wird nur hier (beim Boot) gesetzt.
    [[nodiscard]] Ds18b20SamplerStartResult start(
        const std::optional<Ds18b20TaskBudget>& budget,
        const std::array<
            std::optional<device_platform::Ds18b20ChannelBinding>,
            device_platform::Ds18b20SamplingEngine::kExpectedRomChannelCount>&
            bindings,
        int expectedRomBusGpio, int singleDeviceBusGpio);

    // Ein ungueltiger Kanal liefert eine Quelle, die immer `MissingSample`
    // ohne Identitaet meldet - nie die Probe eines anderen Kanals.
    [[nodiscard]] const device_platform::ITemperatureSource& source(
        uint8_t channel) const;
    [[nodiscard]] device_platform::Ds18b20EnumerationReport report() const {
        return engine_.report();
    }
    [[nodiscard]] bool running() const noexcept { return started_; }

   private:
    class ForwardingBus;
    static void taskEntry(void* context);
    void run();

    const device_platform::ITimeSource& time_;
    std::unique_ptr<ForwardingBus> expectedRomForward_;
    std::unique_ptr<ForwardingBus> singleDeviceForward_;
    device_platform::Ds18b20SamplingEngine engine_;
    std::array<device_platform::Ds18b20ChannelSource,
               device_platform::Ds18b20SamplingEngine::kChannelCount>
        sources_;
    device_platform::Ds18b20ChannelSource invalidChannelSource_;
    std::unique_ptr<device_platform::IDs18b20Bus> expectedRomHardware_;
    std::unique_ptr<device_platform::IDs18b20Bus> singleDeviceHardware_;
    bool started_{false};
};

}  // namespace device_platform_esp_idf
