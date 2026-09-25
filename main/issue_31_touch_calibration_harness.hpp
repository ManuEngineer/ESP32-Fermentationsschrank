#pragma once

#if defined(APP_ISSUE_31_TOUCH_CALIBRATION_HARNESS)

namespace fermentation::issue_31_touch_calibration {

// Runs the actor-free, non-productive raw-touch capture sequence. The
// harness deliberately does not know about calibration records or the
// application UI; it only presents reference geometry and records the
// controller-native samples returned by the existing adapter.
void run() noexcept;

}  // namespace fermentation::issue_31_touch_calibration

#endif
