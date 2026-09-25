#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include "device_ui_contracts.hpp"
#include "secure_random_source.hpp"
#include "state_store.hpp"
#include "storage_types.hpp"

namespace fermentation {

inline constexpr std::size_t kAuthenticationSaltBytes = 16U;
inline constexpr std::size_t kAuthenticationVerifierBytes = 32U;
inline constexpr std::uint16_t kAuthenticationPbkdf2Sha256Algorithm = 1U;
inline constexpr std::uint32_t kAuthenticationPbkdf2Sha256WorkFactor = 10000U;

enum class AuthProvisioningState : std::uint8_t {
    Unprovisioned = 1U,
    Provisioning = 2U,
    Provisioned = 3U,
    Indeterminate = 4U,
    RecoveryRequired = 5U,
};

struct AuthLockoutState {
    std::uint32_t failedAttempts{0U};
    std::uint8_t lockoutStage{0U};
    // Persisted duration, not an absolute clock value. After reboot the full
    // duration is conservatively restarted.
    std::uint64_t lockoutRemainingMs{0U};
};

struct AuthVerifier {
    std::uint16_t algorithmId{kAuthenticationPbkdf2Sha256Algorithm};
    std::uint32_t workFactor{kAuthenticationPbkdf2Sha256WorkFactor};
    std::array<std::uint8_t, kAuthenticationSaltBytes> salt{};
    std::array<std::uint8_t, kAuthenticationVerifierBytes> verifier{};
};

struct AuthenticationCredentialRecord {
    device_platform::StorageEpoch storageEpoch;
    std::uint64_t recordSequence{1U};
    bool webPasswordEnabled{true};
    AuthVerifier webPassword;
    AuthVerifier servicePin;
    std::uint64_t webCredentialEpoch{1U};
    std::uint64_t servicePinCredentialEpoch{1U};
    AuthLockoutState webLockout;
    AuthLockoutState servicePinLockout;
};

struct AuthProvisioningRoot {
    device_platform::StorageEpoch storageEpoch;
    std::uint64_t recordSequence{1U};
    AuthProvisioningState state{AuthProvisioningState::RecoveryRequired};
    std::uint64_t authDomainGeneration{1U};
    std::uint64_t bootstrapSequenceBinding{0U};
};

[[nodiscard]] bool operator==(const AuthVerifier& left,
                              const AuthVerifier& right) noexcept;
[[nodiscard]] bool operator==(const AuthLockoutState& left,
                              const AuthLockoutState& right) noexcept;
[[nodiscard]] bool operator==(
    const AuthenticationCredentialRecord& left,
    const AuthenticationCredentialRecord& right) noexcept;
[[nodiscard]] bool operator==(const AuthProvisioningRoot& left,
                              const AuthProvisioningRoot& right) noexcept;
[[nodiscard]] bool isPlausible(
    const AuthenticationCredentialRecord& record) noexcept;
[[nodiscard]] bool isPlausible(const AuthProvisioningRoot& root) noexcept;

enum class AuthenticationRecordCodecStatus : std::uint8_t {
    Success,
    InvalidModel,
    CapacityExceeded,
    InvalidEnvelope,
    UnsupportedSchema,
    RecordIdentityMismatch,
};

struct AuthenticationCredentialDecodeResult {
    AuthenticationRecordCodecStatus status{
        AuthenticationRecordCodecStatus::InvalidEnvelope};
    std::optional<AuthenticationCredentialRecord> value;
};

struct AuthProvisioningRootDecodeResult {
    AuthenticationRecordCodecStatus status{
        AuthenticationRecordCodecStatus::InvalidEnvelope};
    std::optional<AuthProvisioningRoot> value;
};

[[nodiscard]] AuthenticationRecordCodecStatus encodeAuthenticationCredential(
    const AuthenticationCredentialRecord& record, std::string& out);
[[nodiscard]] AuthenticationCredentialDecodeResult
decodeAuthenticationCredential(const std::string& bytes);
[[nodiscard]] AuthenticationRecordCodecStatus encodeAuthProvisioningRoot(
    const AuthProvisioningRoot& root, std::string& out);
[[nodiscard]] AuthProvisioningRootDecodeResult decodeAuthProvisioningRoot(
    const std::string& bytes);

enum class AuthenticationReadStatus : std::uint8_t {
    Success,
    NotFound,
    DifferentEpoch,
    ReadError,
    CapacityError,
    IntegrityFailure,
    UnsupportedSchema,
};

enum class AuthenticationWriteStatus : std::uint8_t {
    Success,
    InvalidTransition,
    CounterOverflow,
    WriteError,
    CapacityError,
    ReadbackFailure,
    IntegrityFailure,
    CommitOutcomeUnknown,
};

struct AuthenticationCredentialReadResult {
    AuthenticationReadStatus status{AuthenticationReadStatus::IntegrityFailure};
    std::optional<AuthenticationCredentialRecord> value;
};

struct AuthProvisioningRootReadResult {
    AuthenticationReadStatus status{AuthenticationReadStatus::IntegrityFailure};
    std::optional<AuthProvisioningRoot> value;
};

class AuthenticationRecordStore final {
   public:
    explicit AuthenticationRecordStore(device_platform::IStateStore& store)
        : store_(store) {}

    [[nodiscard]] AuthenticationCredentialReadResult readCredentials(
        device_platform::StorageEpoch expectedEpoch) const;
    [[nodiscard]] AuthProvisioningRootReadResult readRoot(
        device_platform::StorageEpoch expectedEpoch) const;

    // Updates are compare-and-swap against the complete expected record. The
    // target sequence must be exactly expected.sequence + 1.
    [[nodiscard]] AuthenticationWriteStatus writeCredentials(
        const AuthenticationCredentialRecord& expected,
        const AuthenticationCredentialRecord& target);
    [[nodiscard]] AuthenticationWriteStatus writeRoot(
        const AuthProvisioningRoot& expected,
        const AuthProvisioningRoot& target);

   private:
    friend class AuthenticationDomain;
    friend class ConfigurationRecoveryService;
    friend class AuthenticationRecordStoreTestAccess;
    [[nodiscard]] AuthenticationWriteStatus writeInitialCredentials(
        const AuthenticationCredentialRecord& record,
        bool authorizedEpochReplacement);
    [[nodiscard]] AuthenticationWriteStatus writeInitialRoot(
        const AuthProvisioningRoot& root, bool authorizedEpochReplacement);
    [[nodiscard]] const device_platform::IStateStore* storeIdentity() const {
        return &store_;
    }

    device_platform::IStateStore& store_;
};

class IAuthenticationKdf {
   public:
    IAuthenticationKdf() = default;
    virtual ~IAuthenticationKdf() = default;
    IAuthenticationKdf(const IAuthenticationKdf&) = delete;
    IAuthenticationKdf& operator=(const IAuthenticationKdf&) = delete;
    IAuthenticationKdf(IAuthenticationKdf&&) = delete;
    IAuthenticationKdf& operator=(IAuthenticationKdf&&) = delete;

    [[nodiscard]] virtual bool derive(
        const std::string& secret, const AuthVerifier& parameters,
        std::array<std::uint8_t, kAuthenticationVerifierBytes>& out) = 0;
};

class AuthenticationBootstrapContext final {
   public:
    AuthenticationBootstrapContext(const AuthenticationBootstrapContext&) =
        default;
    AuthenticationBootstrapContext& operator=(
        const AuthenticationBootstrapContext&) = default;
    AuthenticationBootstrapContext(AuthenticationBootstrapContext&&) = default;
    AuthenticationBootstrapContext& operator=(
        AuthenticationBootstrapContext&&) = default;
    ~AuthenticationBootstrapContext() = default;

    [[nodiscard]] bool validFor(
        const device_platform::IStateStore& store) const noexcept {
        return storeIdentity_ == &store && storageEpoch_.value() != 0U &&
               bootstrapSequence_ != 0U;
    }
    [[nodiscard]] device_platform::StorageEpoch storageEpoch() const noexcept {
        return storageEpoch_;
    }
    [[nodiscard]] std::uint64_t bootstrapSequence() const noexcept {
        return bootstrapSequence_;
    }

   private:
    friend class ConfigurationRecoveryService;
    friend class AuthenticationDomain;
    friend class AuthenticationBootstrapContextTestAccess;
    AuthenticationBootstrapContext(const device_platform::IStateStore& store,
                                   device_platform::StorageEpoch storageEpoch,
                                   std::uint64_t bootstrapSequence) noexcept
        : storeIdentity_(&store),
          storageEpoch_(storageEpoch),
          bootstrapSequence_(bootstrapSequence) {}

    const device_platform::IStateStore* storeIdentity_;
    device_platform::StorageEpoch storageEpoch_;
    std::uint64_t bootstrapSequence_;
};

enum class AuthenticationBootstrapResolutionStatus : std::uint8_t {
    Ready,
    MutationBusy,
    PersistenceFailure,
    RecoveryRequired,
    CounterOverflow,
};

struct AuthenticationBootstrapResolutionResult {
    AuthenticationBootstrapResolutionStatus status{
        AuthenticationBootstrapResolutionStatus::RecoveryRequired};
    std::optional<AuthenticationBootstrapContext> context;
};

enum class AuthInputStatus : std::uint8_t {
    Valid,
    InvalidUtf8,
    InvalidLength,
    CapacityExceeded,
};

[[nodiscard]] AuthInputStatus validateWebPassword(
    const std::string& value) noexcept;
[[nodiscard]] AuthInputStatus validateServicePin(
    const std::string& value) noexcept;
[[nodiscard]] bool constantTimeEqual(
    const std::array<std::uint8_t, kAuthenticationVerifierBytes>& left,
    const std::array<std::uint8_t, kAuthenticationVerifierBytes>&
        right) noexcept;

enum class AuthBootstrapStatus : std::uint8_t {
    BootstrapAllowed,
    NotProvisioned,
    RecoveryRequired,
    AlreadyProvisioned,
    InvalidInput,
    KdfUnavailable,
    PersistenceFailure,
    CommitOutcomeUnknown,
};

enum class AuthCheckStatus : std::uint8_t {
    Authenticated,
    Disabled,
    Invalid,
    LockedOut,
    RecoveryRequired,
    KdfUnavailable,
};

class AuthenticationDomain final {
   public:
    AuthenticationDomain(AuthenticationRecordStore& store,
                         IAuthenticationKdf& kdf,
                         device_platform::ISecureRandomSource& random)
        : store_(store), kdf_(kdf), random_(random) {}

    [[nodiscard]] AuthBootstrapStatus inspect(
        const AuthenticationBootstrapContext& context) const;
    // Recovery creates the unprovisioned root only after validating the
    // current ConfigurationBootstrap schema-3 marker and epoch.
    [[nodiscard]] AuthBootstrapStatus bootstrap(
        const AuthenticationBootstrapContext& context,
        device_platform::UiSurface surface, bool confirmed,
        const std::string& password, const std::string& servicePin);
    [[nodiscard]] AuthCheckStatus verifyWebPassword(
        const AuthenticationBootstrapContext& context,
        const std::string& password, std::uint64_t nowMs,
        std::uint64_t& retryAfterMs);
    [[nodiscard]] AuthCheckStatus verifyServicePin(
        const AuthenticationBootstrapContext& context,
        const std::string& servicePin, std::uint64_t nowMs,
        std::uint64_t& retryAfterMs);
    [[nodiscard]] AuthBootstrapStatus changeWebPassword(
        const AuthenticationBootstrapContext& context,
        const std::string& current, const std::string& replacement,
        std::uint64_t nowMs);
    [[nodiscard]] AuthBootstrapStatus changeServicePin(
        const AuthenticationBootstrapContext& context,
        const std::string& current, const std::string& replacement,
        std::uint64_t nowMs);
    [[nodiscard]] std::optional<bool> webPasswordEnabled(
        const AuthenticationBootstrapContext& context) const;

   private:
    struct LockoutClock {
        std::uint64_t recordSequence{0U};
        std::uint64_t anchorMs{0U};
        std::uint64_t remainingMs{0U};
        bool initialized{false};
    };

    [[nodiscard]] std::uint64_t effectiveLockoutRemaining(
        const AuthLockoutState& persisted, std::uint64_t recordSequence,
        std::uint64_t nowMs, LockoutClock& clock) const noexcept;
    [[nodiscard]] bool makeVerifier(const std::string& secret,
                                    AuthVerifier& out);
    [[nodiscard]] AuthenticationCredentialReadResult readActiveCredentials(
        const AuthenticationBootstrapContext& context) const;
    [[nodiscard]] AuthProvisioningRootReadResult readActiveRoot(
        const AuthenticationBootstrapContext& context) const;

    AuthenticationRecordStore& store_;
    IAuthenticationKdf& kdf_;
    device_platform::ISecureRandomSource& random_;
    mutable std::recursive_mutex mutex_;
    LockoutClock webLockoutClock_;
    LockoutClock servicePinLockoutClock_;
};

}  // namespace fermentation
