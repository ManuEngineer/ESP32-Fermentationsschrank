#pragma once

#if defined(APP_ISSUE_90_NVS_HARDWARE_TEST)

namespace device_platform_esp_idf {

// Runs the #90-only, non-release UART harness.  The harness owns the
// partition initialization while no productive IStateStore consumer exists;
// NvsStateStore itself remains free of init/deinit responsibility.
[[nodiscard]] bool runIssue90NvsHardwareVerification();

}  // namespace device_platform_esp_idf

#endif
