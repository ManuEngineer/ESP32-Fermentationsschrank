#pragma once

#if defined(APP_ISSUE_29_BRINGUP_PROBE)

namespace fermentation::issue_29_bringup {

// Starts the transient bring-up diagnostic task, waits for its bounded
// completion protocol, and logs the #29 evidence. The caller remains the
// composition root; the large CommandDecision path never runs on app_main's
// stack.
[[nodiscard]] bool run();

}  // namespace fermentation::issue_29_bringup

#endif
