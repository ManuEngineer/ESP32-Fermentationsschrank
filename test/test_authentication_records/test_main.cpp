#include <unity.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "authentication_records.hpp"
#include "simulated_persistent_state_store.hpp"

namespace {

class Random final : public device_platform::ISecureRandomSource {
   public:
    bool fill(void* buffer, std::size_t length) override {
        if (buffer == nullptr && length != 0U) return false;
        auto* bytes = static_cast<std::uint8_t*>(buffer);
        for (std::size_t i = 0U; i < length; ++i) bytes[i] = next_++;
        return true;
    }

   private:
    std::uint8_t next_{1U};
};

class Kdf final : public fermentation::IAuthenticationKdf {
   public:
    bool derive(const std::string& secret,
                const fermentation::AuthVerifier& parameters,
                std::array<std::uint8_t,
                           fermentation::kAuthenticationVerifierBytes>& out)
        override {
        if (!parameters.valid()) return false;
        ++calls;
        std::uint8_t value = 0U;
        for (const auto byte : secret) value ^= static_cast<std::uint8_t>(byte);
        for (const auto byte : parameters.salt) value ^= byte;
        out.fill(value);
        return true;
    }

    std::uint32_t calls{0U};
};

void test_auth_bootstrap_requires_positive_root_and_roundtrips() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    fermentation::AuthenticationRecordStore records(store);
    Random random;
    Kdf kdf;
    auto domain = std::make_unique<fermentation::AuthenticationDomain>(
        records, kdf, random, 100U);
    const device_platform::StorageEpoch epoch{7U};

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::RecoveryRequired),
        static_cast<int>(domain->inspect(epoch)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain->initializeUnprovisioned(epoch)));
    fermentation::AuthenticationDomain unavailable(records, kdf, random);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::KdfUnavailable),
        static_cast<int>(unavailable.bootstrap(epoch, "a-valid-password", "1234")));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain->bootstrap(epoch, "a-valid-password", "1234")));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::AlreadyProvisioned),
        static_cast<int>(domain->inspect(epoch)));
}

void test_auth_wrong_password_persists_lockout_before_result() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    fermentation::AuthenticationRecordStore records(store);
    Random random;
    Kdf kdf;
    fermentation::AuthenticationDomain domain(records, kdf, random, 100U);
    const device_platform::StorageEpoch epoch{8U};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain.initializeUnprovisioned(epoch)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain.bootstrap(epoch, "another-valid-password", "9876")));
    std::uint64_t retry = 0U;
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::AuthCheckStatus::Invalid),
                              static_cast<int>(domain.verifyWebPassword(
                                  epoch, "wrong-password-value", 1000U + i, retry)));
    }
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::AuthCheckStatus::LockedOut),
                          static_cast<int>(domain.verifyWebPassword(
                              epoch, "wrong-password-value", 2000U, retry)));
    TEST_ASSERT_GREATER_THAN(0, retry);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::AuthCheckStatus::Authenticated),
                          static_cast<int>(domain.verifyWebPassword(
                              epoch, "another-valid-password", 33'000U, retry)));
}

void test_corrupt_root_never_reopens_bootstrap() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    fermentation::AuthenticationRecordStore records(store);
    Random random;
    Kdf kdf;
    fermentation::AuthenticationDomain domain(records, kdf, random, 100U);
    const device_platform::StorageEpoch epoch{9U};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain.initializeUnprovisioned(epoch)));
    auto key = device_platform::StateStoreKey::create("authroot0");
    TEST_ASSERT_TRUE(key.key.has_value());
    store.injectCorruption(*key.key, "corrupt");
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::RecoveryRequired),
        static_cast<int>(domain.inspect(epoch)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::RecoveryRequired),
        static_cast<int>(domain.bootstrap(epoch, "a-valid-password", "1234")));
}

void test_existing_credentials_never_reopen_unprovisioned_bootstrap() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    fermentation::AuthenticationRecordStore records(store);
    Random random;
    Kdf kdf;
    fermentation::AuthenticationDomain domain(records, kdf, random, 100U);
    const device_platform::StorageEpoch epoch{11U};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain.initializeUnprovisioned(epoch)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain.bootstrap(epoch, "existing-password", "1357")));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthenticationWriteStatus::Success),
        static_cast<int>(records.writeRoot(
            fermentation::AuthProvisioningRoot{
                epoch, 99U, fermentation::AuthProvisioningState::Unprovisioned,
                1U})));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::RecoveryRequired),
        static_cast<int>(domain.bootstrap(epoch, "replacement-password", "2468")));
}

void test_password_utf8_bounds_are_exact() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::AuthInputStatus::InvalidLength),
                          static_cast<int>(fermentation::validateWebPassword("short")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::AuthInputStatus::Valid),
                          static_cast<int>(fermentation::validateWebPassword(
                              "123456789012345")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::AuthInputStatus::Valid),
                          static_cast<int>(fermentation::validateWebPassword(
                              "äöüäöüäöüäöüäöüä")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::AuthInputStatus::InvalidUtf8),
                          static_cast<int>(fermentation::validateWebPassword("\xC3")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::AuthInputStatus::Valid),
                          static_cast<int>(fermentation::validateServicePin("1234")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fermentation::AuthInputStatus::InvalidLength),
                          static_cast<int>(fermentation::validateServicePin("123")));
}

void test_service_pin_and_password_mode_are_owned_by_auth_domain() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    fermentation::AuthenticationRecordStore records(store);
    Random random;
    Kdf kdf;
    fermentation::AuthenticationDomain domain(records, kdf, random, 100U);
    const device_platform::StorageEpoch epoch{10U};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain.initializeUnprovisioned(epoch)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain.bootstrap(epoch, "initial-password", "2468")));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain.setWebPasswordEnabled(
            epoch, "initial-password", "", false, true, 1000U)));
    const auto disabled = domain.webPasswordEnabled(epoch);
    TEST_ASSERT_TRUE(disabled.has_value());
    TEST_ASSERT_FALSE(*disabled);
    std::uint64_t retry = 0U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthCheckStatus::Disabled),
        static_cast<int>(domain.verifyWebPassword(
            epoch, "initial-password", 1001U, retry)));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain.setWebPasswordEnabled(
            epoch, "", "replacement-password", true, true, 1002U)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthCheckStatus::Authenticated),
        static_cast<int>(domain.verifyWebPassword(
            epoch, "replacement-password", 1003U, retry)));

    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::AuthCheckStatus::Invalid),
            static_cast<int>(domain.verifyServicePin(epoch, "0000", 2000U + i,
                                                     retry)));
    }
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthCheckStatus::LockedOut),
        static_cast<int>(domain.verifyServicePin(epoch, "2468", 2003U, retry)));
    TEST_ASSERT_GREATER_THAN(0, retry);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthCheckStatus::Authenticated),
        static_cast<int>(domain.verifyServicePin(epoch, "2468", 33'000U,
                                                 retry)));
}

void test_lockout_restarts_conservatively_after_reboot_without_kdf_or_write() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    fermentation::AuthenticationRecordStore records(store);
    Random random;
    Kdf kdf;
    const device_platform::StorageEpoch epoch{12U};
    fermentation::AuthenticationDomain first(records, kdf, random, 100U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(first.initializeUnprovisioned(epoch)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(first.bootstrap(epoch, "lockout-password", "8642")));
    std::uint64_t retry = 0U;
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::AuthCheckStatus::Invalid),
            static_cast<int>(first.verifyWebPassword(
                epoch, "wrong-lockout-password", 1000U + i, retry)));
    }
    const auto callsBeforeReboot = kdf.calls;
    fermentation::AuthenticationDomain afterReboot(records, kdf, random, 100U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthCheckStatus::LockedOut),
        static_cast<int>(afterReboot.verifyWebPassword(
            epoch, "lockout-password", 1005U, retry)));
    TEST_ASSERT_EQUAL_UINT64(30'000U, retry);
    TEST_ASSERT_EQUAL_UINT32(callsBeforeReboot, kdf.calls);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthCheckStatus::Authenticated),
        static_cast<int>(afterReboot.verifyWebPassword(
            epoch, "lockout-password", 31'005U, retry)));
}

void test_service_pin_reboot_lockout_covers_elevated_and_maximum_stages() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    fermentation::AuthenticationRecordStore records(store);
    Random random;
    Kdf kdf;
    const device_platform::StorageEpoch epoch{13U};
    auto domain = std::make_unique<fermentation::AuthenticationDomain>(
        records, kdf, random, 100U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain->initializeUnprovisioned(epoch)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(domain->bootstrap(epoch, "service-lockout-password", "8642")));

    std::uint64_t retry = 0U;
    std::uint64_t now = 1000U;
    // Three failures create each service-PIN lockout.  Recreate the domain
    // between stages to prove that persisted duration, not uptime, is used.
    int failuresToTrigger = 3;
    for (std::uint8_t stage = 1U; stage <= 7U; ++stage) {
        for (int failure = 0; failure < failuresToTrigger; ++failure) {
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(fermentation::AuthCheckStatus::Invalid),
                static_cast<int>(domain->verifyServicePin(epoch, "0000", now++, retry)));
        }
        fermentation::AuthenticationDomain rebooted(records, kdf, random, 100U);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::AuthCheckStatus::LockedOut),
            static_cast<int>(rebooted.verifyServicePin(epoch, "8642", now, retry)));
        const auto expected = stage < 7U
                                  ? (30'000ULL << (stage - 1U))
                                  : 30ULL * 60ULL * 1000ULL;
        TEST_ASSERT_EQUAL_UINT64(expected, retry);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::AuthCheckStatus::Invalid),
            static_cast<int>(rebooted.verifyServicePin(
                epoch, "0000", now + expected, retry)));
        domain = std::make_unique<fermentation::AuthenticationDomain>(
            records, kdf, random, 100U);
        failuresToTrigger = 2;
        now += expected;
    }
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_auth_bootstrap_requires_positive_root_and_roundtrips);
    RUN_TEST(test_auth_wrong_password_persists_lockout_before_result);
    RUN_TEST(test_corrupt_root_never_reopens_bootstrap);
    RUN_TEST(test_existing_credentials_never_reopen_unprovisioned_bootstrap);
    RUN_TEST(test_password_utf8_bounds_are_exact);
    RUN_TEST(test_service_pin_and_password_mode_are_owned_by_auth_domain);
    RUN_TEST(test_lockout_restarts_conservatively_after_reboot_without_kdf_or_write);
    RUN_TEST(test_service_pin_reboot_lockout_covers_elevated_and_maximum_stages);
    return UNITY_END();
}
