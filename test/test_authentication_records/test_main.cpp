#include <unity.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "authentication_records.hpp"
#include "configuration_storage_contract.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"

namespace fermentation {
class AuthenticationBootstrapContextTestAccess {
   public:
    static AuthenticationBootstrapContext create(
        const device_platform::IStateStore& store,
        device_platform::StorageEpoch epoch, std::uint64_t sequence) {
        return AuthenticationBootstrapContext{store, epoch, sequence};
    }
};
}  // namespace fermentation

namespace {

class LocalStore final : public device_platform::IStateStore {
   public:
    device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey& key,
        const std::string& value) override {
        ++writes;
        if (unknownWriteAt == writes) {
            if (unknownCommitAt) values[key.bytes()] = value;
            return device_platform::StateStoreWriteStatus::CommitOutcomeUnknown;
        }
        values[key.bytes()] = value;
        return device_platform::StateStoreWriteStatus::Success;
    }

    device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey& key,
        std::size_t maxBytes) const override {
        ++reads;
        const auto found = values.find(key.bytes());
        if (found == values.end())
            return {device_platform::StateStoreReadStatus::NotFound, {}};
        if (found->second.size() > maxBytes)
            return {device_platform::StateStoreReadStatus::CapacityError, {}};
        return {device_platform::StateStoreReadStatus::Success, found->second};
    }

    void put(const char* name, std::string value) {
        const auto key = device_platform::StateStoreKey::create(name);
        TEST_ASSERT_TRUE(key.key.has_value());
        values[key.key->bytes()] = std::move(value);
    }

    std::map<std::string, std::string> values;
    std::size_t unknownWriteAt{0U};
    bool unknownCommitAt{false};
    std::size_t writes{0U};
    mutable std::size_t reads{0U};
};

class DeterministicTestKdf final : public fermentation::IAuthenticationKdf {
   public:
    bool derive(
        const std::string& secret, const fermentation::AuthVerifier& params,
        std::array<std::uint8_t, fermentation::kAuthenticationVerifierBytes>&
            out) override {
        ++calls;
        if (params.algorithmId !=
                fermentation::kAuthenticationPbkdf2Sha256Algorithm ||
            params.workFactor !=
                fermentation::kAuthenticationPbkdf2Sha256WorkFactor) {
            return false;
        }
        std::uint32_t state = 2166136261U;
        for (const unsigned char byte : secret)
            state = (state ^ byte) * 16777619U;
        for (const auto byte : params.salt) state = (state ^ byte) * 16777619U;
        for (auto& byte : out) {
            state = state * 1664525U + 1013904223U;
            byte = static_cast<std::uint8_t>(state >> 24U);
        }
        return true;
    }

    std::size_t calls{0U};
};

class TestRandom final : public device_platform::ISecureRandomSource {
   public:
    bool fill(void* buffer, std::size_t length) override {
        if (length == 0U) return true;
        if (buffer == nullptr || fail) return false;
        auto* bytes = static_cast<std::uint8_t*>(buffer);
        for (std::size_t index = 0U; index < length; ++index)
            bytes[index] = static_cast<std::uint8_t>(next++);
        return true;
    }

    bool fail{false};
    std::uint8_t next{1U};
};

std::string longPassword() { return "a sufficiently long password"; }

void seedRoot(
    LocalStore& store,
    device_platform::StorageEpoch epoch = device_platform::StorageEpoch{4U},
    std::uint64_t bootstrapSequence = 12U) {
    std::string bytes;
    const fermentation::AuthProvisioningRoot root{
        epoch, 1U, fermentation::AuthProvisioningState::Unprovisioned, 1U,
        bootstrapSequence};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::AuthenticationRecordCodecStatus::Success),
        static_cast<int>(
            fermentation::encodeAuthProvisioningRoot(root, bytes)));
    store.put(fermentation::configuration_storage_contract::
                  kAuthenticationRootStoreKey,
              std::move(bytes));
}

void test_auth_records_round_trip_and_fixed_kdf_policy() {
    fermentation::AuthenticationCredentialRecord credentials;
    credentials.storageEpoch = device_platform::StorageEpoch{4U};
    credentials.webPassword.salt[0] = 0x11U;
    credentials.webPassword.verifier[0] = 0x22U;
    credentials.servicePin.salt[0] = 0x33U;
    credentials.servicePin.verifier[0] = 0x44U;
    std::string encodedCredentials;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::AuthenticationRecordCodecStatus::Success),
        static_cast<int>(fermentation::encodeAuthenticationCredential(
            credentials, encodedCredentials)));
    const auto decodedCredentials =
        fermentation::decodeAuthenticationCredential(encodedCredentials);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::AuthenticationRecordCodecStatus::Success),
        static_cast<int>(decodedCredentials.status));
    TEST_ASSERT_TRUE(decodedCredentials.value.has_value());
    TEST_ASSERT_TRUE(*decodedCredentials.value == credentials);

    fermentation::AuthProvisioningRoot root{
        device_platform::StorageEpoch{4U}, 7U,
        fermentation::AuthProvisioningState::Provisioned, 3U, 12U};
    std::string encodedRoot;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::AuthenticationRecordCodecStatus::Success),
        static_cast<int>(
            fermentation::encodeAuthProvisioningRoot(root, encodedRoot)));
    const auto decodedRoot =
        fermentation::decodeAuthProvisioningRoot(encodedRoot);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::AuthenticationRecordCodecStatus::Success),
        static_cast<int>(decodedRoot.status));
    TEST_ASSERT_TRUE(decodedRoot.value.has_value());
    TEST_ASSERT_TRUE(*decodedRoot.value == root);

    credentials.webPassword.workFactor = 9999U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::AuthenticationRecordCodecStatus::InvalidModel),
        static_cast<int>(fermentation::encodeAuthenticationCredential(
            credentials, encodedCredentials)));
}

void test_bootstrap_requires_local_confirmed_input_and_provisions_both_secrets() {
    LocalStore store;
    seedRoot(store);
    fermentation::AuthenticationRecordStore records(store);
    DeterministicTestKdf kdf;
    TestRandom random;
    fermentation::AuthenticationDomain auth(records, kdf, random);
    const auto epoch = device_platform::StorageEpoch{4U};
    const auto context =
        fermentation::AuthenticationBootstrapContextTestAccess::create(
            store, epoch, 12U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(auth.inspect(context)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::InvalidInput),
        static_cast<int>(
            auth.bootstrap(context, device_platform::UiSurface::WebInterface,
                           true, longPassword(), "1234")));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::InvalidInput),
        static_cast<int>(
            auth.bootstrap(context, device_platform::UiSurface::LocalDisplay,
                           false, longPassword(), "1234")));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(
            auth.bootstrap(context, device_platform::UiSurface::LocalDisplay,
                           true, longPassword(), "1234")));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::AlreadyProvisioned),
        static_cast<int>(auth.inspect(context)));
    TEST_ASSERT_EQUAL_INT(2, static_cast<int>(kdf.calls));

    std::uint64_t retryAfterMs = 0U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthCheckStatus::Authenticated),
        static_cast<int>(auth.verifyWebPassword(context, longPassword(), 10U,
                                                retryAfterMs)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthCheckStatus::Authenticated),
        static_cast<int>(
            auth.verifyServicePin(context, "1234", 11U, retryAfterMs)));
}

void test_lockout_is_persisted_and_skips_kdf_while_active_and_after_reboot() {
    LocalStore store;
    seedRoot(store);
    fermentation::AuthenticationRecordStore records(store);
    DeterministicTestKdf kdf;
    TestRandom random;
    fermentation::AuthenticationDomain auth(records, kdf, random);
    const auto epoch = device_platform::StorageEpoch{4U};
    const auto context =
        fermentation::AuthenticationBootstrapContextTestAccess::create(
            store, epoch, 12U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(
            auth.bootstrap(context, device_platform::UiSurface::LocalDisplay,
                           true, longPassword(), "1234")));

    const std::string wrongPassword = "a wrong long password value";
    std::uint64_t retryAfterMs = 0U;
    for (std::uint8_t attempt = 1U; attempt <= 5U; ++attempt) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::AuthCheckStatus::Invalid),
            static_cast<int>(auth.verifyWebPassword(context, wrongPassword,
                                                    attempt, retryAfterMs)));
    }
    TEST_ASSERT_EQUAL_UINT64(30000U, retryAfterMs);
    const auto callsAfterLock = kdf.calls;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthCheckStatus::LockedOut),
        static_cast<int>(
            auth.verifyWebPassword(context, longPassword(), 6U, retryAfterMs)));
    TEST_ASSERT_EQUAL_UINT64(callsAfterLock, kdf.calls);

    fermentation::AuthenticationDomain rebooted(records, kdf, random);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthCheckStatus::LockedOut),
        static_cast<int>(rebooted.verifyWebPassword(context, longPassword(), 1U,
                                                    retryAfterMs)));
    TEST_ASSERT_EQUAL_UINT64(callsAfterLock, kdf.calls);
}

void test_epoch_and_bootstrap_binding_mismatches_fail_closed() {
    LocalStore store;
    seedRoot(store);
    fermentation::AuthenticationRecordStore records(store);
    DeterministicTestKdf kdf;
    TestRandom random;
    fermentation::AuthenticationDomain auth(records, kdf, random);
    const auto wrongSequence =
        fermentation::AuthenticationBootstrapContextTestAccess::create(
            store, device_platform::StorageEpoch{4U}, 13U);
    const auto wrongEpoch =
        fermentation::AuthenticationBootstrapContextTestAccess::create(
            store, device_platform::StorageEpoch{5U}, 12U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::RecoveryRequired),
        static_cast<int>(auth.inspect(wrongSequence)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::RecoveryRequired),
        static_cast<int>(auth.inspect(wrongEpoch)));
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(kdf.calls));
}

void test_codec_rejects_crc_corruption_and_store_rejects_stale_sequence() {
    LocalStore store;
    seedRoot(store);
    fermentation::AuthenticationRecordStore records(store);
    DeterministicTestKdf kdf;
    TestRandom random;
    fermentation::AuthenticationDomain auth(records, kdf, random);
    const auto epoch = device_platform::StorageEpoch{4U};
    const auto context =
        fermentation::AuthenticationBootstrapContextTestAccess::create(
            store, epoch, 12U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::BootstrapAllowed),
        static_cast<int>(
            auth.bootstrap(context, device_platform::UiSurface::LocalDisplay,
                           true, longPassword(), "1234")));
    auto loaded = records.readCredentials(epoch);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthenticationReadStatus::Success),
        static_cast<int>(loaded.status));
    TEST_ASSERT_TRUE(loaded.value.has_value());
    auto target = *loaded.value;
    ++target.recordSequence;
    target.webLockout.failedAttempts = 1U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthenticationWriteStatus::Success),
        static_cast<int>(records.writeCredentials(*loaded.value, target)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::AuthenticationWriteStatus::InvalidTransition),
        static_cast<int>(records.writeCredentials(*loaded.value, target)));

    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::AuthenticationRecordCodecStatus::Success),
        static_cast<int>(
            fermentation::encodeAuthenticationCredential(target, encoded)));
    encoded.back() = static_cast<char>(encoded.back() ^ 0x01);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::AuthenticationRecordCodecStatus::InvalidEnvelope),
        static_cast<int>(
            fermentation::decodeAuthenticationCredential(encoded).status));
}

void test_unknown_credential_commit_leaves_provisioning_fail_closed() {
    LocalStore committedStore;
    seedRoot(committedStore);
    fermentation::AuthenticationRecordStore committedRecords(committedStore);
    DeterministicTestKdf kdf;
    TestRandom random;
    fermentation::AuthenticationDomain committedAuth(committedRecords, kdf,
                                                     random);
    const auto context =
        fermentation::AuthenticationBootstrapContextTestAccess::create(
            committedStore, device_platform::StorageEpoch{4U}, 12U);
    committedStore.unknownWriteAt = 2U;
    committedStore.unknownCommitAt = false;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::AuthBootstrapStatus::CommitOutcomeUnknown),
        static_cast<int>(committedAuth.bootstrap(
            context, device_platform::UiSurface::LocalDisplay, true,
            longPassword(), "1234")));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::AuthBootstrapStatus::RecoveryRequired),
        static_cast<int>(committedAuth.inspect(context)));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_auth_records_round_trip_and_fixed_kdf_policy);
    RUN_TEST(
        test_bootstrap_requires_local_confirmed_input_and_provisions_both_secrets);
    RUN_TEST(
        test_lockout_is_persisted_and_skips_kdf_while_active_and_after_reboot);
    RUN_TEST(test_epoch_and_bootstrap_binding_mismatches_fail_closed);
    RUN_TEST(
        test_codec_rejects_crc_corruption_and_store_rejects_stale_sequence);
    RUN_TEST(test_unknown_credential_commit_leaves_provisioning_fail_closed);
    return UNITY_END();
}
