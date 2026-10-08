#include "ds18b20_sampler_task.hpp"

#include "ds18b20_onewire_bus.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace device_platform_esp_idf {

using device_platform::Ds18b20DriverResult;

// Leitet an den echten Bus weiter, sobald start() ihn angelegt hat. Vorher
// (und nach einem Fehlstart) liefert er `Other`; die Engine wird in diesen
// Zustaenden nie gesteppt.
class Ds18b20Sampler::ForwardingBus final
    : public device_platform::IDs18b20Bus {
   public:
    void setTarget(device_platform::IDs18b20Bus* target) { target_ = target; }

    [[nodiscard]] Ds18b20DriverResult presence() override {
        return target_ != nullptr ? target_->presence()
                                  : Ds18b20DriverResult::Other;
    }
    [[nodiscard]] device_platform::Ds18b20Enumeration enumerate() override {
        return target_ != nullptr ? target_->enumerate()
                                  : device_platform::Ds18b20Enumeration{};
    }
    [[nodiscard]] Ds18b20DriverResult startConversionAll() override {
        return target_ != nullptr ? target_->startConversionAll()
                                  : Ds18b20DriverResult::Other;
    }
    [[nodiscard]] device_platform::Ds18b20ScratchpadRead read(
        device_platform::OneWireRom rom) override {
        return target_ != nullptr ? target_->read(rom)
                                  : device_platform::Ds18b20ScratchpadRead{};
    }
    [[nodiscard]] Ds18b20DriverResult setResolution12(
        device_platform::OneWireRom rom) override {
        return target_ != nullptr ? target_->setResolution12(rom)
                                  : Ds18b20DriverResult::Other;
    }

   private:
    device_platform::IDs18b20Bus* target_{nullptr};
};

const char* ds18b20SamplerStartResultName(
    Ds18b20SamplerStartResult result) noexcept {
    switch (result) {
        case Ds18b20SamplerStartResult::Started:
            return "started";
        case Ds18b20SamplerStartResult::BudgetNotApproved:
            return "disabled: budget not approved";
        case Ds18b20SamplerStartResult::AlreadyStarted:
            return "already started";
        case Ds18b20SamplerStartResult::InvalidBudget:
            return "invalid budget";
        case Ds18b20SamplerStartResult::BusUnavailable:
            return "bus unavailable";
        case Ds18b20SamplerStartResult::TaskCreateFailed:
            return "task create failed";
    }
    return "unknown";
}

Ds18b20Sampler::Ds18b20Sampler(const device_platform::ITimeSource& time)
    : time_(time),
      expectedRomForward_(std::make_unique<ForwardingBus>()),
      singleDeviceForward_(std::make_unique<ForwardingBus>()),
      engine_(time, *expectedRomForward_, *singleDeviceForward_),
      sources_{{{engine_, 0U}, {engine_, 1U}, {engine_, 2U}}} {}

Ds18b20Sampler::~Ds18b20Sampler() = default;

const device_platform::ITemperatureSource& Ds18b20Sampler::source(
    uint8_t channel) const {
    return sources_[channel < sources_.size() ? channel : 0U];
}

Ds18b20SamplerStartResult Ds18b20Sampler::start(
    const std::optional<Ds18b20TaskBudget>& budget,
    const std::array<
        std::optional<device_platform::Ds18b20ChannelBinding>,
        device_platform::Ds18b20SamplingEngine::kExpectedRomChannelCount>&
        bindings,
    int expectedRomBusGpio, int singleDeviceBusGpio) {
    if (started_) return Ds18b20SamplerStartResult::AlreadyStarted;
    if (!budget.has_value())
        return Ds18b20SamplerStartResult::BudgetNotApproved;
    if (budget->stackBytes == 0U || budget->priority == 0U) {
        return Ds18b20SamplerStartResult::InvalidBudget;
    }
    expectedRomHardware_ = Ds18b20OnewireBus::create(expectedRomBusGpio);
    singleDeviceHardware_ = Ds18b20OnewireBus::create(singleDeviceBusGpio);
    if (expectedRomHardware_ == nullptr || singleDeviceHardware_ == nullptr) {
        expectedRomHardware_.reset();
        singleDeviceHardware_.reset();
        return Ds18b20SamplerStartResult::BusUnavailable;
    }
    expectedRomForward_->setTarget(expectedRomHardware_.get());
    singleDeviceForward_->setTarget(singleDeviceHardware_.get());
    engine_.setBinding(bindings);
    const BaseType_t created = xTaskCreatePinnedToCore(
        &Ds18b20Sampler::taskEntry, "ds18b20_sampler", budget->stackBytes, this,
        static_cast<UBaseType_t>(budget->priority), nullptr,
        static_cast<BaseType_t>(budget->coreId));
    if (created != pdPASS) {
        expectedRomForward_->setTarget(nullptr);
        singleDeviceForward_->setTarget(nullptr);
        expectedRomHardware_.reset();
        singleDeviceHardware_.reset();
        return Ds18b20SamplerStartResult::TaskCreateFailed;
    }
    started_ = true;
    return Ds18b20SamplerStartResult::Started;
}

void Ds18b20Sampler::taskEntry(void* context) {
    static_cast<Ds18b20Sampler*>(context)->run();
}

void Ds18b20Sampler::run() {
    for (;;) {
        engine_.step();
        const uint64_t now = time_.monotonicMillis();
        const uint64_t due = engine_.nextDueMillis();
        const uint64_t waitMs = due > now ? due - now : 1U;
        vTaskDelay(pdMS_TO_TICKS(waitMs) > 0 ? pdMS_TO_TICKS(waitMs) : 1);
    }
}

}  // namespace device_platform_esp_idf
