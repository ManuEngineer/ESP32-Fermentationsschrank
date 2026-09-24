#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <mutex>
#include <string>

#include "secure_random_source.hpp"
#include "state_store.hpp"
#include "storage_types.hpp"

namespace fermentation {

inline constexpr std::size_t kAuthenticationSaltBytes = 16U;
inline constexpr std::size_t kAuthenticationVerifierBytes = 32U;

enum class AuthProvisioningState : std::uint8_t {
    Unprovisioned = 1U,
    Provisioning = 2U,
    ProvisioningIndeterminate = 3U,
    Provisioned = 4U,
    RecoveryRequired = 5U,
};

enum class AuthCredentialKind : std::uint8_t {
    WebPassword = 1U,
    ServicePin = 2U,
};

struct AuthLockoutState {
    std::uint32_t failedAttempts{0U};
    std::uint8_t lockoutStage{0U};
    // Persisted duration, never an absolute monotonic timestamp. After a
    // reboot the owning domain starts this full duration again because the
    // monotonic clock has no trusted cross-boot continuity.
    std::uint64_t lockoutRemainingMs{0U};
};

struct AuthVerifier {
    // 1 identifies PBKDF2-HMAC-SHA-256.  A zero work factor is deliberately
    // invalid: it represents the still owner-gated measurement state and is
    // never a runtime fallback.
    std::uint16_t algorithmId{1U};
    std::uint32_t workFactor{0U};
    std::array<std::uint8_t, kAuthenticationSaltBytes> salt{};
    std::array<std::uint8_t, kAuthenticationVerifierBytes> verifier{};

    [[nodiscard]] bool valid() const noexcept {
        return algorithmId == 1U && workFactor != 0U;
    }
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
[[nodiscard]] bool isPlausible(
    const AuthenticationCredentialRecord& record) noexcept;
[[nodiscard]] bool isPlausible(const AuthProvisioningRoot& root) noexcept;

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
    WriteError,
    CapacityError,
    CommitOutcomeUnknown,
    ReadbackFailure,
    IntegrityFailure,
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
    [[nodiscard]] AuthenticationWriteStatus writeCredentials(
        const AuthenticationCredentialRecord& record);
    [[nodiscard]] AuthenticationWriteStatus writeRoot(
        const AuthProvisioningRoot& root);

   private:
    device_platform::IStateStore& store_;
};

// A KDF is injected by the platform adapter.  This keeps mbedTLS out of the
// portable application and makes native tests use deterministic test vectors
// without pretending that a host hash is the production primitive.
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
    AuthenticationDomain(
        AuthenticationRecordStore& store, IAuthenticationKdf& kdf,
        device_platform::ISecureRandomSource& random,
        std::optional<std::uint32_t> workFactorPolicy = std::nullopt)
        : store_(store),
          kdf_(kdf),
          random_(random),
          workFactorPolicy_(workFactorPolicy) {}

    [[nodiscard]] AuthBootstrapStatus inspect(
        device_platform::StorageEpoch epoch) const;
    // This is called only by the configuration/bootstrap owner after it has
    // positive new-epoch or Schema-3 first-consumer evidence. A missing root
    // is not interpreted here as unprovisioned.
    [[nodiscard]] AuthBootstrapStatus initializeUnprovisioned(
        device_platform::StorageEpoch epoch);
    [[nodiscard]] AuthBootstrapStatus bootstrap(
        device_platform::StorageEpoch epoch, const std::string& password,
        const std::string& servicePin);
    [[nodiscard]] AuthCheckStatus verifyWebPassword(
        device_platform::StorageEpoch epoch, const std::string& password,
        std::uint64_t nowMs, std::uint64_t& retryAfterMs);
    [[nodiscard]] AuthCheckStatus verifyServicePin(
        device_platform::StorageEpoch epoch, const std::string& servicePin,
        std::uint64_t nowMs, std::uint64_t& retryAfterMs);
    [[nodiscard]] AuthBootstrapStatus changeWebPassword(
        device_platform::StorageEpoch epoch, const std::string& current,
        const std::string& replacement, std::uint64_t nowMs);
    [[nodiscard]] AuthBootstrapStatus changeServicePin(
        device_platform::StorageEpoch epoch, const std::string& current,
        const std::string& replacement, std::uint64_t nowMs);
    [[nodiscard]] AuthBootstrapStatus setWebPasswordEnabled(
        device_platform::StorageEpoch epoch, const std::string& currentPassword,
        const std::string& replacementPassword, bool enabled, bool confirmed,
        std::uint64_t nowMs);
    [[nodiscard]] std::optional<bool> webPasswordEnabled(
        device_platform::StorageEpoch epoch) const;

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
                                    std::uint32_t workFactor,
                                    AuthVerifier& out);
    AuthenticationRecordStore& store_;
    IAuthenticationKdf& kdf_;
    device_platform::ISecureRandomSource& random_;
    std::optional<std::uint32_t> workFactorPolicy_;
    mutable LockoutClock webLockoutClock_;
    mutable LockoutClock servicePinLockoutClock_;
    mutable std::recursive_mutex mutex_;
};

}  // namespace fermentation
