#pragma once

#include <cstdint>

namespace device_platform {

// Portable diagnostic categories.  ResetCause is evidence only: it never
// decides whether an actuator may be enabled and it is not persisted.
enum class ResetCause : std::uint8_t {
    Unknown,
    PowerOn,
    External,
    Software,
    Panic,
    InterruptWatchdog,
    TaskWatchdog,
    Watchdog,
    DeepSleep,
    Brownout,
    Sdio,
    Usb,
    Jtag,
    Efuse,
    PowerGlitch,
    CpuLockup,
    Other,
};

class IResetCauseSource {
   public:
    IResetCauseSource() = default;
    virtual ~IResetCauseSource() = default;

    IResetCauseSource(const IResetCauseSource&) = delete;
    IResetCauseSource& operator=(const IResetCauseSource&) = delete;
    IResetCauseSource(IResetCauseSource&&) = delete;
    IResetCauseSource& operator=(IResetCauseSource&&) = delete;

    [[nodiscard]] virtual ResetCause resetCause() const noexcept = 0;
};

}  // namespace device_platform
