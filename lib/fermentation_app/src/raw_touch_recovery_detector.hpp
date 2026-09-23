#pragma once

#include <cstdint>
#include <optional>

#include "device_ui_hardware_ports.hpp"

namespace fermentation {

// Detects a continuously held raw touch contact for the PIN-independent
// calibration recovery path (see
// docs/tasks/issue-31-renderer-display-touch-calibration-plan.md,
// Abschnitt 8, "PIN-unabhaengige Raw-Touch-Kalibrierungs-Recovery"). This
// detector owns only the hold/release/duration state machine; it never
// decides UI navigation, PIN, factory reset or any other business action -
// the caller maps a positive detection onto the existing
// FermentationUiSafeBootTarget::RawTouchRecovery capability itself.
//
// minimumStrength and requiredHoldMicros are real hardware-derived
// parameters (the controller's contact/Z threshold and the exact hold
// duration this board needs to reliably distinguish a deliberate held
// contact from noise). Neither has a default: this type cannot be
// constructed with an invented value, so a composition root without
// owner-approved values for both cannot wire this detector into the boot
// path at all - touch recovery stays unavailable, not silently guessed.
class RawTouchRecoveryDetector final {
   public:
    RawTouchRecoveryDetector(std::uint16_t minimumStrength,
                             std::uint64_t requiredHoldMicros) noexcept
        : minimumStrength_(minimumStrength),
          requiredHoldMicros_(requiredHoldMicros) {}

    // Feeds one raw sample. Returns true exactly once, on the sample where
    // a continuously held contact first reaches requiredHoldMicros. Any
    // sample that is not a held contact (released, no contact, controller
    // error, or below minimumStrength) resets the hold start - there is no
    // partial credit and no debounce/glitch tolerance here; a real
    // contact-stability window is itself an open hardware-informed
    // parameter this type does not invent. Once triggered, further
    // observe() calls return false until reset() is called explicitly, so
    // a contact held well past the threshold does not repeatedly
    // re-trigger.
    [[nodiscard]] bool observe(
        const device_platform::RawTouchSample& sample) noexcept;

    // Explicit abort/reset, e.g. the boot recovery-evaluation window ended,
    // or the caller otherwise decided this contact no longer counts.
    void reset() noexcept;

    [[nodiscard]] bool holding() const noexcept {
        return holdStartUs_.has_value();
    }

   private:
    [[nodiscard]] bool isHeldContact(
        const device_platform::RawTouchSample& sample) const noexcept;

    std::uint16_t minimumStrength_;
    std::uint64_t requiredHoldMicros_;
    std::optional<std::uint64_t> holdStartUs_;
    bool triggered_{false};
};

}  // namespace fermentation
