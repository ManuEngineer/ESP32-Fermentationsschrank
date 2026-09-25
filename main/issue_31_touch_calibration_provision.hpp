#pragma once

#if defined(APP_ISSUE_31_TOUCH_CALIBRATION_PROVISIONER)

namespace fermentation::issue_31_touch_calibration_provision {

// Runs the actor-free, bring-up-only provisioning of the reviewed R1 touch
// model. It opens the existing state store directly and never starts the
// application, display, touch driver, or any actuator path.
void run() noexcept;

}  // namespace fermentation::issue_31_touch_calibration_provision

#endif
