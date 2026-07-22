#pragma once

#include <cstdint>

#ifndef APP_TARGET_FLASH_MB
#error "APP_TARGET_FLASH_MB must be defined by the PlatformIO profile"
#endif

#ifndef APP_REQUIRE_PSRAM
#error "APP_REQUIRE_PSRAM must be defined by the PlatformIO profile"
#endif

#ifndef APP_WEB_OTA_ENABLED
#error "APP_WEB_OTA_ENABLED must be defined by the PlatformIO profile"
#endif

#ifndef APP_REAL_ACTUATORS_ENABLED
#error "APP_REAL_ACTUATORS_ENABLED must be defined by the PlatformIO profile"
#endif

#if (defined(APP_PROFILE_NATIVE) + defined(APP_PROFILE_ESP32_BRINGUP) + \
     defined(APP_PROFILE_ESP32_RELEASE)) != 1
#error "Exactly one application profile must be selected"
#endif

#if APP_TARGET_FLASH_MB != 4
#error "Release 1 targets exactly 4 MB flash"
#endif

#if APP_REQUIRE_PSRAM != 0
#error "Release 1 must not require PSRAM"
#endif

#if APP_WEB_OTA_ENABLED != 0
#error "Web OTA is a FUTURE_RELEASE feature"
#endif

#if APP_REAL_ACTUATORS_ENABLED != 0
#error "Issue #9 must not enable real actuators"
#endif

namespace app_config {

inline constexpr char kProjectName[] = "ESP32-Fermentationsschrank";
inline constexpr char kProjectOwner[] = "ManuEngineer";
inline constexpr char kFirmwareVersion[] = "0.1.0";
inline constexpr uint32_t kSerialBaud = 115200;
inline constexpr uint32_t kTargetFlashMegabytes = APP_TARGET_FLASH_MB;
inline constexpr uint32_t kTargetFlashBytes =
    kTargetFlashMegabytes * 1024U * 1024U;
inline constexpr bool kRequiresPsram = APP_REQUIRE_PSRAM != 0;
inline constexpr bool kWebOtaEnabled = APP_WEB_OTA_ENABLED != 0;
inline constexpr bool kRealActuatorsEnabledByDefault =
    APP_REAL_ACTUATORS_ENABLED != 0;

enum class BuildProfile : uint8_t {
    Native,
    Esp32Bringup,
    Esp32Release,
};

enum class HardwareState : uint8_t {
    NativeSimulation,
    HardwareUnverified,
};

enum class ActuatorPolicy : uint8_t {
    SimulationOnly,
    LockedForBringup,
    RequireVerifiedHardware,
};

struct ProfilePolicy {
    BuildProfile profile;
    HardwareState startupHardwareState;
    ActuatorPolicy actuatorPolicy;
    bool esp32Target;
    bool bringupDiagnostics;
    bool releasePolicy;
    bool webOtaEnabled;
    bool realActuatorsEnabled;
};

inline constexpr ProfilePolicy kNativeProfilePolicy{
    BuildProfile::Native,
    HardwareState::NativeSimulation,
    ActuatorPolicy::SimulationOnly,
    false,
    false,
    false,
    false,
    false,
};

inline constexpr ProfilePolicy kEsp32BringupProfilePolicy{
    BuildProfile::Esp32Bringup,
    HardwareState::HardwareUnverified,
    ActuatorPolicy::LockedForBringup,
    true,
    true,
    false,
    false,
    false,
};

inline constexpr ProfilePolicy kEsp32ReleaseProfilePolicy{
    BuildProfile::Esp32Release,
    HardwareState::HardwareUnverified,
    ActuatorPolicy::RequireVerifiedHardware,
    true,
    false,
    true,
    false,
    false,
};

constexpr bool hasSafeDefaults(const ProfilePolicy& policy) {
    return !policy.webOtaEnabled && !policy.realActuatorsEnabled &&
           (policy.profile != BuildProfile::Esp32Bringup ||
            (policy.startupHardwareState ==
                 HardwareState::HardwareUnverified &&
             policy.actuatorPolicy == ActuatorPolicy::LockedForBringup)) &&
           (policy.profile != BuildProfile::Esp32Release ||
            (policy.startupHardwareState ==
                 HardwareState::HardwareUnverified &&
             policy.actuatorPolicy ==
                 ActuatorPolicy::RequireVerifiedHardware));
}

static_assert(kTargetFlashMegabytes == 4U);
static_assert(!kRequiresPsram);
static_assert(!kWebOtaEnabled);
static_assert(!kRealActuatorsEnabledByDefault);
static_assert(hasSafeDefaults(kNativeProfilePolicy));
static_assert(hasSafeDefaults(kEsp32BringupProfilePolicy));
static_assert(hasSafeDefaults(kEsp32ReleaseProfilePolicy));

#if defined(APP_PROFILE_NATIVE)
inline constexpr ProfilePolicy kActiveProfilePolicy = kNativeProfilePolicy;
#elif defined(APP_PROFILE_ESP32_BRINGUP)
inline constexpr ProfilePolicy kActiveProfilePolicy =
    kEsp32BringupProfilePolicy;
#elif defined(APP_PROFILE_ESP32_RELEASE)
inline constexpr ProfilePolicy kActiveProfilePolicy =
    kEsp32ReleaseProfilePolicy;
#endif

constexpr const char* profileName(const BuildProfile profile) {
    switch (profile) {
        case BuildProfile::Native:
            return "native";
        case BuildProfile::Esp32Bringup:
            return "esp32_bringup";
        case BuildProfile::Esp32Release:
            return "esp32_release";
    }
    return "unknown";
}

constexpr const char* hardwareStateName(const HardwareState state) {
    switch (state) {
        case HardwareState::NativeSimulation:
            return "NATIVE_SIMULATION";
        case HardwareState::HardwareUnverified:
            return "HARDWARE_UNVERIFIED";
    }
    return "UNKNOWN";
}

constexpr const char* actuatorPolicyName(const ActuatorPolicy policy) {
    switch (policy) {
        case ActuatorPolicy::SimulationOnly:
            return "SIMULATION_ONLY";
        case ActuatorPolicy::LockedForBringup:
            return "LOCKED_FOR_BRINGUP";
        case ActuatorPolicy::RequireVerifiedHardware:
            return "REQUIRE_VERIFIED_HARDWARE";
    }
    return "UNKNOWN";
}

}  // namespace app_config
