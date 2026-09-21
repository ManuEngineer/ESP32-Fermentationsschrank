#include "authentication_records.hpp"

#include <algorithm>
#include <limits>

#include "big_endian_codec.hpp"
#include "byte_buffer.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "storage_envelope.hpp"

namespace fermentation {
namespace {

constexpr std::size_t kSingleCredentialPayloadBytes =
    2U + 4U + 8U + 1U + kAuthenticationSaltBytes + 1U +
    kAuthenticationVerifierBytes + 4U + 1U + 8U;
constexpr std::size_t kRecordBytes = 2U * kSingleCredentialPayloadBytes + 1U;
constexpr std::size_t kRootPayloadBytes = 1U + 8U;
constexpr std::size_t kMaximumCredentialEnvelopeBytes = kRecordBytes + 45U;
constexpr std::size_t kMaximumRootEnvelopeBytes = kRootPayloadBytes + 45U;

bool validLockout(const AuthLockoutState& state) noexcept {
    return state.lockoutStage <= 31U;
}

bool writeVerifier(device_platform::ByteWriter& writer,
                   const AuthVerifier& verifier,
                   const AuthLockoutState& lockout,
                   std::uint64_t credentialEpoch) {
    using namespace device_platform::big_endian;
    return verifier.valid() && validLockout(lockout) &&
           writeUint16(writer, verifier.algorithmId) &&
           writeUint32(writer, verifier.workFactor) &&
           writeUint64(writer, credentialEpoch) &&
           writeUint8(writer, static_cast<std::uint8_t>(verifier.salt.size())) &&
           writer.writeBytes(verifier.salt.data(), verifier.salt.size()) &&
           writeUint8(writer,
                      static_cast<std::uint8_t>(verifier.verifier.size())) &&
           writer.writeBytes(verifier.verifier.data(), verifier.verifier.size()) &&
           writeUint32(writer, lockout.failedAttempts) &&
           writeUint8(writer, lockout.lockoutStage) &&
           writeUint64(writer, lockout.lockoutUntilMonotonicMs);
}

bool readVerifier(device_platform::ByteReader& reader, AuthVerifier& verifier,
                  AuthLockoutState& lockout, std::uint64_t& credentialEpoch) {
    using namespace device_platform::big_endian;
    std::uint8_t saltLength = 0U;
    std::uint8_t verifierLength = 0U;
    if (!readUint16(reader, verifier.algorithmId) ||
        !readUint32(reader, verifier.workFactor) ||
        !readUint64(reader, credentialEpoch) ||
        !readUint8(reader, saltLength) ||
        saltLength != verifier.salt.size() ||
        !reader.readBytes(verifier.salt.data(), verifier.salt.size()) ||
        !readUint8(reader, verifierLength) ||
        verifierLength != verifier.verifier.size() ||
        !reader.readBytes(verifier.verifier.data(), verifier.verifier.size()) ||
        !readUint32(reader, lockout.failedAttempts) ||
        !readUint8(reader, lockout.lockoutStage) ||
        !readUint64(reader, lockout.lockoutUntilMonotonicMs)) {
        return false;
    }
    return verifier.valid() && validLockout(lockout) && credentialEpoch != 0U;
}

AuthenticationReadStatus mapRead(device_platform::StateStoreReadStatus status) {
    switch (status) {
        case device_platform::StateStoreReadStatus::Success:
            return AuthenticationReadStatus::Success;
        case device_platform::StateStoreReadStatus::NotFound:
            return AuthenticationReadStatus::NotFound;
        case device_platform::StateStoreReadStatus::ReadError:
            return AuthenticationReadStatus::ReadError;
        case device_platform::StateStoreReadStatus::CapacityError:
            return AuthenticationReadStatus::CapacityError;
    }
    return AuthenticationReadStatus::IntegrityFailure;
}

template <typename Record, typename Decoder>
AuthenticationWriteStatus writeAndReadback(
    device_platform::IStateStore& store, const device_platform::StateStoreKey& key,
    const std::string& encoded, std::size_t maximum, Decoder decoder,
    const Record& expected) {
    const auto status = store.write(key, encoded);
    if (status == device_platform::StateStoreWriteStatus::WriteError)
        return AuthenticationWriteStatus::WriteError;
    if (status == device_platform::StateStoreWriteStatus::CapacityError)
        return AuthenticationWriteStatus::CapacityError;
    const auto read = store.read(key, maximum);
    if (read.status != device_platform::StateStoreReadStatus::Success) {
        return status == device_platform::StateStoreWriteStatus::CommitOutcomeUnknown
                   ? AuthenticationWriteStatus::CommitOutcomeUnknown
                   : AuthenticationWriteStatus::ReadbackFailure;
    }
    const auto decoded = decoder(read.value);
    if (!decoded.value.has_value() || !(*decoded.value == expected) ||
        read.value != encoded) {
        return status == device_platform::StateStoreWriteStatus::CommitOutcomeUnknown
                   ? AuthenticationWriteStatus::CommitOutcomeUnknown
                   : AuthenticationWriteStatus::IntegrityFailure;
    }
    return AuthenticationWriteStatus::Success;
}

bool validUtf8(const std::string& value, std::size_t& codePoints) noexcept {
    codePoints = 0U;
    for (std::size_t i = 0U; i < value.size();) {
        const auto byte = static_cast<unsigned char>(value[i]);
        std::size_t width = 0U;
        std::uint32_t codePoint = 0U;
        if (byte <= 0x7FU) {
            width = 1U;
            codePoint = byte;
        } else if (byte >= 0xC2U && byte <= 0xDFU) {
            width = 2U;
            codePoint = byte & 0x1FU;
        } else if (byte >= 0xE0U && byte <= 0xEFU) {
            width = 3U;
            codePoint = byte & 0x0FU;
        } else if (byte >= 0xF0U && byte <= 0xF4U) {
            width = 4U;
            codePoint = byte & 0x07U;
        } else {
            return false;
        }
        if (i + width > value.size()) return false;
        for (std::size_t j = 1U; j < width; ++j) {
            const auto continuation = static_cast<unsigned char>(value[i + j]);
            if ((continuation & 0xC0U) != 0x80U) return false;
            codePoint = (codePoint << 6U) | (continuation & 0x3FU);
        }
        if ((width == 2U && codePoint < 0x80U) ||
            (width == 3U && codePoint < 0x800U) ||
            (width == 4U && codePoint < 0x10000U) ||
            codePoint > 0x10FFFFU ||
            (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
            return false;
        }
        ++codePoints;
        i += width;
    }
    return true;
}

}  // namespace

bool operator==(const AuthVerifier& left, const AuthVerifier& right) noexcept {
    return left.algorithmId == right.algorithmId &&
           left.workFactor == right.workFactor && left.salt == right.salt &&
           left.verifier == right.verifier;
}

bool operator==(const AuthLockoutState& left,
                const AuthLockoutState& right) noexcept {
    return left.failedAttempts == right.failedAttempts &&
           left.lockoutStage == right.lockoutStage &&
           left.lockoutUntilMonotonicMs == right.lockoutUntilMonotonicMs;
}

bool operator==(const AuthenticationCredentialRecord& left,
                const AuthenticationCredentialRecord& right) noexcept {
    return left.storageEpoch == right.storageEpoch &&
           left.recordSequence == right.recordSequence &&
           left.webPasswordEnabled == right.webPasswordEnabled &&
           left.webPassword == right.webPassword &&
           left.servicePin == right.servicePin &&
           left.webCredentialEpoch == right.webCredentialEpoch &&
           left.servicePinCredentialEpoch == right.servicePinCredentialEpoch &&
           left.webLockout == right.webLockout &&
           left.servicePinLockout == right.servicePinLockout;
}

bool operator==(const AuthProvisioningRoot& left,
                const AuthProvisioningRoot& right) noexcept {
    return left.storageEpoch == right.storageEpoch &&
           left.recordSequence == right.recordSequence && left.state == right.state &&
           left.authDomainGeneration == right.authDomainGeneration;
}

bool isPlausible(const AuthenticationCredentialRecord& record) noexcept {
    return record.storageEpoch.value() != 0U && record.recordSequence != 0U &&
           record.webCredentialEpoch != 0U && record.servicePinCredentialEpoch != 0U &&
           record.webPassword.valid() && record.servicePin.valid() &&
           validLockout(record.webLockout) && validLockout(record.servicePinLockout);
}

bool isPlausible(const AuthProvisioningRoot& root) noexcept {
    switch (root.state) {
        case AuthProvisioningState::Unprovisioned:
        case AuthProvisioningState::Provisioning:
        case AuthProvisioningState::ProvisioningIndeterminate:
        case AuthProvisioningState::Provisioned:
        case AuthProvisioningState::RecoveryRequired:
            return root.storageEpoch.value() != 0U && root.recordSequence != 0U &&
                   root.authDomainGeneration != 0U;
    }
    return false;
}

AuthenticationRecordCodecStatus encodeAuthenticationCredential(
    const AuthenticationCredentialRecord& record, std::string& out) {
    if (!isPlausible(record)) return AuthenticationRecordCodecStatus::InvalidModel;
    device_platform::ByteWriter payload(kRecordBytes);
    if (!device_platform::big_endian::writeBool(payload, record.webPasswordEnabled) ||
        !writeVerifier(payload, record.webPassword, record.webLockout,
                       record.webCredentialEpoch) ||
        !writeVerifier(payload, record.servicePin, record.servicePinLockout,
                       record.servicePinCredentialEpoch) ||
        payload.size() != kRecordBytes) {
        return AuthenticationRecordCodecStatus::CapacityExceeded;
    }
    std::string encoded;
    const auto status = device_platform::encodeEnvelope(
        {configuration_storage_contract::kAuthenticationRecordType, 1U,
         record.storageEpoch, record.recordSequence, std::nullopt,
         payload.takeBytes()},
        encoded, kMaximumCredentialEnvelopeBytes);
    if (status == device_platform::EnvelopeEncodeStatus::CapacityExceeded)
        return AuthenticationRecordCodecStatus::CapacityExceeded;
    if (status != device_platform::EnvelopeEncodeStatus::Success)
        return AuthenticationRecordCodecStatus::InvalidModel;
    out.swap(encoded);
    return AuthenticationRecordCodecStatus::Success;
}

AuthenticationCredentialDecodeResult decodeAuthenticationCredential(
    const std::string& bytes) {
    const auto envelope = device_platform::decodeEnvelope(bytes);
    if (envelope.status == device_platform::EnvelopeDecodeStatus::UnknownEnvelopeVersion)
        return {AuthenticationRecordCodecStatus::UnsupportedSchema, std::nullopt};
    if (envelope.status != device_platform::EnvelopeDecodeStatus::Success ||
        !envelope.envelope.has_value())
        return {AuthenticationRecordCodecStatus::InvalidEnvelope, std::nullopt};
    const auto& value = *envelope.envelope;
    if (value.recordTypeId != configuration_storage_contract::kAuthenticationRecordType)
        return {AuthenticationRecordCodecStatus::RecordIdentityMismatch, std::nullopt};
    if (value.schemaVersion != 1U || value.utcUnixSeconds.has_value() ||
        value.payload.size() != kRecordBytes)
        return {value.schemaVersion > 1U ? AuthenticationRecordCodecStatus::UnsupportedSchema
                                        : AuthenticationRecordCodecStatus::InvalidModel,
                std::nullopt};
    device_platform::ByteReader reader(value.payload);
    AuthenticationCredentialRecord record;
    record.storageEpoch = value.storageEpoch;
    record.recordSequence = value.versionValue;
    if (!device_platform::big_endian::readBool(reader, record.webPasswordEnabled) ||
        !readVerifier(reader, record.webPassword, record.webLockout,
                      record.webCredentialEpoch) ||
        !readVerifier(reader, record.servicePin, record.servicePinLockout,
                      record.servicePinCredentialEpoch) ||
        reader.remaining() != 0U || !isPlausible(record))
        return {AuthenticationRecordCodecStatus::InvalidModel, std::nullopt};
    return {AuthenticationRecordCodecStatus::Success, record};
}

AuthenticationRecordCodecStatus encodeAuthProvisioningRoot(
    const AuthProvisioningRoot& root, std::string& out) {
    if (!isPlausible(root)) return AuthenticationRecordCodecStatus::InvalidModel;
    device_platform::ByteWriter payload(kRootPayloadBytes);
    if (!device_platform::big_endian::writeUint8(
            payload, static_cast<std::uint8_t>(root.state)) ||
        !device_platform::big_endian::writeUint64(payload,
                                                  root.authDomainGeneration))
        return AuthenticationRecordCodecStatus::CapacityExceeded;
    std::string encoded;
    const auto status = device_platform::encodeEnvelope(
        {configuration_storage_contract::kAuthenticationRootRecordType, 1U,
         root.storageEpoch, root.recordSequence, std::nullopt,
         payload.takeBytes()},
        encoded, kMaximumRootEnvelopeBytes);
    if (status == device_platform::EnvelopeEncodeStatus::CapacityExceeded)
        return AuthenticationRecordCodecStatus::CapacityExceeded;
    if (status != device_platform::EnvelopeEncodeStatus::Success)
        return AuthenticationRecordCodecStatus::InvalidModel;
    out.swap(encoded);
    return AuthenticationRecordCodecStatus::Success;
}

AuthProvisioningRootDecodeResult decodeAuthProvisioningRoot(
    const std::string& bytes) {
    const auto envelope = device_platform::decodeEnvelope(bytes);
    if (envelope.status == device_platform::EnvelopeDecodeStatus::UnknownEnvelopeVersion)
        return {AuthenticationRecordCodecStatus::UnsupportedSchema, std::nullopt};
    if (envelope.status != device_platform::EnvelopeDecodeStatus::Success ||
        !envelope.envelope.has_value())
        return {AuthenticationRecordCodecStatus::InvalidEnvelope, std::nullopt};
    const auto& value = *envelope.envelope;
    if (value.recordTypeId != configuration_storage_contract::kAuthenticationRootRecordType)
        return {AuthenticationRecordCodecStatus::RecordIdentityMismatch, std::nullopt};
    if (value.schemaVersion != 1U || value.utcUnixSeconds.has_value() ||
        value.payload.size() != kRootPayloadBytes)
        return {value.schemaVersion > 1U ? AuthenticationRecordCodecStatus::UnsupportedSchema
                                        : AuthenticationRecordCodecStatus::InvalidModel,
                std::nullopt};
    device_platform::ByteReader reader(value.payload);
    std::uint8_t state = 0U;
    AuthProvisioningRoot root;
    root.storageEpoch = value.storageEpoch;
    root.recordSequence = value.versionValue;
    if (!device_platform::big_endian::readUint8(reader, state) ||
        !device_platform::big_endian::readUint64(reader,
                                                  root.authDomainGeneration) ||
        reader.remaining() != 0U) {
        return {AuthenticationRecordCodecStatus::InvalidModel, std::nullopt};
    }
    root.state = static_cast<AuthProvisioningState>(state);
    if (!isPlausible(root))
        return {AuthenticationRecordCodecStatus::InvalidModel, std::nullopt};
    return {AuthenticationRecordCodecStatus::Success, root};
}

AuthenticationCredentialReadResult AuthenticationRecordStore::readCredentials(
    device_platform::StorageEpoch expectedEpoch) const {
    const auto key = device_platform::StateStoreKey::create(
        configuration_storage_contract::kAuthenticationStoreKey);
    if (!key.key.has_value()) return {AuthenticationReadStatus::IntegrityFailure, std::nullopt};
    const auto read = store_.read(*key.key, kMaximumCredentialEnvelopeBytes);
    if (read.status != device_platform::StateStoreReadStatus::Success)
        return {mapRead(read.status), std::nullopt};
    const auto decoded = decodeAuthenticationCredential(read.value);
    if (!decoded.value.has_value()) {
        return {decoded.status == AuthenticationRecordCodecStatus::UnsupportedSchema
                    ? AuthenticationReadStatus::UnsupportedSchema
                    : AuthenticationReadStatus::IntegrityFailure,
                std::nullopt};
    }
    if (decoded.value->storageEpoch != expectedEpoch)
        return {AuthenticationReadStatus::IntegrityFailure, std::nullopt};
    return {AuthenticationReadStatus::Success, decoded.value};
}

AuthProvisioningRootReadResult AuthenticationRecordStore::readRoot(
    device_platform::StorageEpoch expectedEpoch) const {
    const auto key = device_platform::StateStoreKey::create(
        configuration_storage_contract::kAuthenticationRootStoreKey);
    if (!key.key.has_value()) return {AuthenticationReadStatus::IntegrityFailure, std::nullopt};
    const auto read = store_.read(*key.key, kMaximumRootEnvelopeBytes);
    if (read.status != device_platform::StateStoreReadStatus::Success)
        return {mapRead(read.status), std::nullopt};
    const auto decoded = decodeAuthProvisioningRoot(read.value);
    if (!decoded.value.has_value()) {
        return {decoded.status == AuthenticationRecordCodecStatus::UnsupportedSchema
                    ? AuthenticationReadStatus::UnsupportedSchema
                    : AuthenticationReadStatus::IntegrityFailure,
                std::nullopt};
    }
    if (decoded.value->storageEpoch != expectedEpoch)
        return {AuthenticationReadStatus::IntegrityFailure, std::nullopt};
    return {AuthenticationReadStatus::Success, decoded.value};
}

AuthenticationWriteStatus AuthenticationRecordStore::writeCredentials(
    const AuthenticationCredentialRecord& record) {
    std::string encoded;
    if (encodeAuthenticationCredential(record, encoded) !=
        AuthenticationRecordCodecStatus::Success)
        return AuthenticationWriteStatus::IntegrityFailure;
    const auto key = device_platform::StateStoreKey::create(
        configuration_storage_contract::kAuthenticationStoreKey);
    if (!key.key.has_value()) return AuthenticationWriteStatus::IntegrityFailure;
    return writeAndReadback<AuthenticationCredentialRecord>(
        store_, *key.key, encoded, kMaximumCredentialEnvelopeBytes,
        decodeAuthenticationCredential, record);
}

AuthenticationWriteStatus AuthenticationRecordStore::writeRoot(
    const AuthProvisioningRoot& root) {
    std::string encoded;
    if (encodeAuthProvisioningRoot(root, encoded) !=
        AuthenticationRecordCodecStatus::Success)
        return AuthenticationWriteStatus::IntegrityFailure;
    const auto key = device_platform::StateStoreKey::create(
        configuration_storage_contract::kAuthenticationRootStoreKey);
    if (!key.key.has_value()) return AuthenticationWriteStatus::IntegrityFailure;
    return writeAndReadback<AuthProvisioningRoot>(
        store_, *key.key, encoded, kMaximumRootEnvelopeBytes,
        decodeAuthProvisioningRoot, root);
}

AuthInputStatus validateWebPassword(const std::string& value) noexcept {
    if (value.size() > 256U) return AuthInputStatus::CapacityExceeded;
    std::size_t codePoints = 0U;
    if (!validUtf8(value, codePoints)) return AuthInputStatus::InvalidUtf8;
    return codePoints >= 15U && codePoints <= 64U ? AuthInputStatus::Valid
                                                  : AuthInputStatus::InvalidLength;
}

AuthInputStatus validateServicePin(const std::string& value) noexcept {
    if (value.size() != 4U) return AuthInputStatus::InvalidLength;
    for (const unsigned char byte : value)
        if (byte < '0' || byte > '9') return AuthInputStatus::InvalidLength;
    return AuthInputStatus::Valid;
}

bool constantTimeEqual(
    const std::array<std::uint8_t, kAuthenticationVerifierBytes>& left,
    const std::array<std::uint8_t, kAuthenticationVerifierBytes>& right)
    noexcept {
    std::uint8_t difference = 0U;
    for (std::size_t i = 0U; i < left.size(); ++i) difference |= left[i] ^ right[i];
    return difference == 0U;
}

AuthBootstrapStatus AuthenticationDomain::inspect(
    device_platform::StorageEpoch epoch) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto root = store_.readRoot(epoch);
    if (root.status != AuthenticationReadStatus::Success || !root.value.has_value())
        return root.status == AuthenticationReadStatus::NotFound
                   ? AuthBootstrapStatus::RecoveryRequired
                   : AuthBootstrapStatus::RecoveryRequired;
    if (root.value->state == AuthProvisioningState::Unprovisioned) {
        const auto credentials = store_.readCredentials(epoch);
        return credentials.status == AuthenticationReadStatus::NotFound
                   ? AuthBootstrapStatus::BootstrapAllowed
                   : AuthBootstrapStatus::RecoveryRequired;
    }
    if (root.value->state == AuthProvisioningState::Provisioned) {
        const auto credentials = store_.readCredentials(epoch);
        return credentials.status == AuthenticationReadStatus::Success
                   ? AuthBootstrapStatus::AlreadyProvisioned
                   : AuthBootstrapStatus::RecoveryRequired;
    }
    return AuthBootstrapStatus::RecoveryRequired;
}

AuthBootstrapStatus AuthenticationDomain::initializeUnprovisioned(
    device_platform::StorageEpoch epoch) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (epoch.value() == 0U) return AuthBootstrapStatus::RecoveryRequired;
    const auto existing = store_.readRoot(epoch);
    if (existing.status == AuthenticationReadStatus::Success) {
        if (!existing.value.has_value() ||
            existing.value->state != AuthProvisioningState::Unprovisioned) {
            return AuthBootstrapStatus::RecoveryRequired;
        }
        const auto credentials = store_.readCredentials(epoch);
        return credentials.status == AuthenticationReadStatus::NotFound
                   ? AuthBootstrapStatus::BootstrapAllowed
                   : AuthBootstrapStatus::RecoveryRequired;
    }
    if (existing.status != AuthenticationReadStatus::NotFound)
        return AuthBootstrapStatus::RecoveryRequired;
    AuthProvisioningRoot root{epoch, 1U, AuthProvisioningState::Unprovisioned,
                              1U};
    return store_.writeRoot(root) == AuthenticationWriteStatus::Success
               ? AuthBootstrapStatus::BootstrapAllowed
               : AuthBootstrapStatus::RecoveryRequired;
}

bool AuthenticationDomain::makeVerifier(const std::string& secret,
                                        std::uint32_t workFactor,
                                        AuthVerifier& out) {
    if (workFactor == 0U || !random_.fill(out.salt.data(), out.salt.size()))
        return false;
    out.algorithmId = 1U;
    out.workFactor = workFactor;
    return kdf_.derive(secret, out, out.verifier);
}

AuthBootstrapStatus AuthenticationDomain::bootstrap(
    device_platform::StorageEpoch epoch, const std::string& password,
    const std::string& servicePin, std::uint32_t measuredWorkFactor) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (validateWebPassword(password) != AuthInputStatus::Valid ||
        validateServicePin(servicePin) != AuthInputStatus::Valid)
        return AuthBootstrapStatus::InvalidInput;
    if (measuredWorkFactor == 0U) return AuthBootstrapStatus::KdfUnavailable;
    const auto current = store_.readRoot(epoch);
    if (current.status != AuthenticationReadStatus::Success ||
        !current.value.has_value() ||
        current.value->state != AuthProvisioningState::Unprovisioned)
        return AuthBootstrapStatus::RecoveryRequired;
    const auto existingCredentials = store_.readCredentials(epoch);
    if (existingCredentials.status != AuthenticationReadStatus::NotFound)
        return AuthBootstrapStatus::RecoveryRequired;
    auto provisioning = *current.value;
    if (provisioning.recordSequence == std::numeric_limits<std::uint64_t>::max()) {
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    }
    provisioning.state = AuthProvisioningState::Provisioning;
    ++provisioning.recordSequence;
    const auto phase = store_.writeRoot(provisioning);
    if (phase == AuthenticationWriteStatus::CommitOutcomeUnknown)
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    if (phase != AuthenticationWriteStatus::Success)
        return AuthBootstrapStatus::PersistenceFailure;
    const auto markRecovery = [&](AuthBootstrapStatus fallback) {
        if (provisioning.recordSequence ==
            std::numeric_limits<std::uint64_t>::max()) {
            return AuthBootstrapStatus::CommitOutcomeUnknown;
        }
        provisioning.state = AuthProvisioningState::RecoveryRequired;
        ++provisioning.recordSequence;
        const auto recovered = store_.writeRoot(provisioning);
        if (recovered == AuthenticationWriteStatus::CommitOutcomeUnknown) {
            return AuthBootstrapStatus::CommitOutcomeUnknown;
        }
        return recovered == AuthenticationWriteStatus::Success
                   ? fallback
                   : AuthBootstrapStatus::PersistenceFailure;
    };
    AuthenticationCredentialRecord record;
    record.storageEpoch = epoch;
    if (!makeVerifier(password, measuredWorkFactor, record.webPassword) ||
        !makeVerifier(servicePin, measuredWorkFactor, record.servicePin))
        return markRecovery(AuthBootstrapStatus::KdfUnavailable);
    const auto credentials = store_.writeCredentials(record);
    if (credentials == AuthenticationWriteStatus::CommitOutcomeUnknown)
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    if (credentials != AuthenticationWriteStatus::Success)
        return markRecovery(AuthBootstrapStatus::PersistenceFailure);
    if (provisioning.recordSequence == std::numeric_limits<std::uint64_t>::max()) {
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    }
    provisioning.state = AuthProvisioningState::Provisioned;
    ++provisioning.recordSequence;
    const auto completed = store_.writeRoot(provisioning);
    if (completed != AuthenticationWriteStatus::Success)
        return completed == AuthenticationWriteStatus::CommitOutcomeUnknown
                   ? AuthBootstrapStatus::CommitOutcomeUnknown
                   : AuthBootstrapStatus::PersistenceFailure;
    return AuthBootstrapStatus::BootstrapAllowed;
}

AuthCheckStatus AuthenticationDomain::verifyWebPassword(
    device_platform::StorageEpoch epoch, const std::string& password,
    std::uint64_t nowMs, std::uint64_t& retryAfterMs) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    retryAfterMs = 0U;
    if (validateWebPassword(password) != AuthInputStatus::Valid)
        return AuthCheckStatus::Invalid;
    const auto root = store_.readRoot(epoch);
    if (root.status != AuthenticationReadStatus::Success ||
        !root.value.has_value() ||
        root.value->state != AuthProvisioningState::Provisioned)
        return AuthCheckStatus::RecoveryRequired;
    auto credentials = store_.readCredentials(epoch);
    if (credentials.status != AuthenticationReadStatus::Success ||
        !credentials.value.has_value())
        return AuthCheckStatus::RecoveryRequired;
    auto record = *credentials.value;
    if (!record.webPasswordEnabled) return AuthCheckStatus::Disabled;
    auto& lockout = record.webLockout;
    if (nowMs < lockout.lockoutUntilMonotonicMs) {
        retryAfterMs = lockout.lockoutUntilMonotonicMs - nowMs;
        return AuthCheckStatus::LockedOut;
    }
    if (!record.webPassword.valid()) return AuthCheckStatus::KdfUnavailable;
    std::array<std::uint8_t, kAuthenticationVerifierBytes> derived{};
    if (!kdf_.derive(password, record.webPassword, derived))
        return AuthCheckStatus::KdfUnavailable;
    if (constantTimeEqual(derived, record.webPassword.verifier)) {
        lockout = AuthLockoutState{};
        if (record.recordSequence == std::numeric_limits<std::uint64_t>::max())
            return AuthCheckStatus::RecoveryRequired;
        ++record.recordSequence;
        return store_.writeCredentials(record) == AuthenticationWriteStatus::Success
                   ? AuthCheckStatus::Authenticated
                   : AuthCheckStatus::RecoveryRequired;
    }
    if (lockout.failedAttempts != std::numeric_limits<std::uint32_t>::max())
        ++lockout.failedAttempts;
    constexpr std::uint32_t kMaximumAttempts = 5U;
    if (lockout.failedAttempts >= kMaximumAttempts) {
        lockout.failedAttempts = 0U;
        if (lockout.lockoutStage < 31U) ++lockout.lockoutStage;
        const std::uint8_t exponent = static_cast<std::uint8_t>(
            std::min<std::uint8_t>(lockout.lockoutStage - 1U, 5U));
        const std::uint64_t duration =
            std::min<std::uint64_t>(15ULL * 60ULL * 1000ULL,
                                    (30ULL * 1000ULL) << exponent);
        if (nowMs > std::numeric_limits<std::uint64_t>::max() - duration)
            return AuthCheckStatus::RecoveryRequired;
        lockout.lockoutUntilMonotonicMs = nowMs + duration;
        retryAfterMs = duration;
    }
    if (record.recordSequence == std::numeric_limits<std::uint64_t>::max())
        return AuthCheckStatus::RecoveryRequired;
    ++record.recordSequence;
    const auto persisted = store_.writeCredentials(record);
    if (persisted != AuthenticationWriteStatus::Success)
        return AuthCheckStatus::RecoveryRequired;
    return AuthCheckStatus::Invalid;
}

AuthCheckStatus AuthenticationDomain::verifyServicePin(
    device_platform::StorageEpoch epoch, const std::string& servicePin,
    std::uint64_t nowMs, std::uint64_t& retryAfterMs) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    retryAfterMs = 0U;
    if (validateServicePin(servicePin) != AuthInputStatus::Valid) {
        return AuthCheckStatus::Invalid;
    }
    const auto root = store_.readRoot(epoch);
    if (root.status != AuthenticationReadStatus::Success ||
        !root.value.has_value() ||
        root.value->state != AuthProvisioningState::Provisioned) {
        return AuthCheckStatus::RecoveryRequired;
    }
    const auto credentials = store_.readCredentials(epoch);
    if (credentials.status != AuthenticationReadStatus::Success ||
        !credentials.value.has_value() || !credentials.value->servicePin.valid()) {
        return AuthCheckStatus::RecoveryRequired;
    }
    auto record = *credentials.value;
    auto& lockout = record.servicePinLockout;
    if (nowMs < lockout.lockoutUntilMonotonicMs) {
        retryAfterMs = lockout.lockoutUntilMonotonicMs - nowMs;
        return AuthCheckStatus::LockedOut;
    }
    std::array<std::uint8_t, kAuthenticationVerifierBytes> derived{};
    if (!kdf_.derive(servicePin, record.servicePin, derived)) {
        return AuthCheckStatus::KdfUnavailable;
    }
    if (constantTimeEqual(derived, record.servicePin.verifier)) {
        lockout = AuthLockoutState{};
        if (record.recordSequence == std::numeric_limits<std::uint64_t>::max()) {
            return AuthCheckStatus::RecoveryRequired;
        }
        ++record.recordSequence;
        return store_.writeCredentials(record) == AuthenticationWriteStatus::Success
                   ? AuthCheckStatus::Authenticated
                   : AuthCheckStatus::RecoveryRequired;
    }
    if (lockout.failedAttempts != std::numeric_limits<std::uint32_t>::max()) {
        ++lockout.failedAttempts;
    }
    constexpr std::uint32_t kMaximumAttempts = 3U;
    if (lockout.failedAttempts >= kMaximumAttempts) {
        lockout.failedAttempts = 0U;
        if (lockout.lockoutStage < 31U) ++lockout.lockoutStage;
        const auto exponent = static_cast<std::uint8_t>(
            std::min<std::uint8_t>(lockout.lockoutStage - 1U, 6U));
        const auto duration = std::min<std::uint64_t>(
            30ULL * 60ULL * 1000ULL, (30ULL * 1000ULL) << exponent);
        if (nowMs > std::numeric_limits<std::uint64_t>::max() - duration) {
            return AuthCheckStatus::RecoveryRequired;
        }
        lockout.lockoutUntilMonotonicMs = nowMs + duration;
        retryAfterMs = duration;
    }
    if (record.recordSequence == std::numeric_limits<std::uint64_t>::max()) {
        return AuthCheckStatus::RecoveryRequired;
    }
    ++record.recordSequence;
    const auto persisted = store_.writeCredentials(record);
    return persisted == AuthenticationWriteStatus::Success
               ? AuthCheckStatus::Invalid
               : AuthCheckStatus::RecoveryRequired;
}

AuthBootstrapStatus AuthenticationDomain::changeWebPassword(
    device_platform::StorageEpoch epoch, const std::string& current,
    const std::string& replacement, std::uint32_t measuredWorkFactor,
    std::uint64_t nowMs) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (validateWebPassword(replacement) != AuthInputStatus::Valid) {
        return AuthBootstrapStatus::InvalidInput;
    }
    std::uint64_t retryAfter = 0U;
    const auto verified = verifyWebPassword(epoch, current, nowMs, retryAfter);
    if (verified != AuthCheckStatus::Authenticated) {
        return verified == AuthCheckStatus::KdfUnavailable
                   ? AuthBootstrapStatus::KdfUnavailable
                   : verified == AuthCheckStatus::RecoveryRequired
                         ? AuthBootstrapStatus::RecoveryRequired
                         : AuthBootstrapStatus::InvalidInput;
    }
    const auto root = store_.readRoot(epoch);
    const auto credentials = store_.readCredentials(epoch);
    if (root.status != AuthenticationReadStatus::Success ||
        !root.value.has_value() || credentials.status != AuthenticationReadStatus::Success ||
        !credentials.value.has_value()) {
        return AuthBootstrapStatus::RecoveryRequired;
    }
    auto record = *credentials.value;
    if (measuredWorkFactor == 0U) {
        measuredWorkFactor = record.webPassword.workFactor;
    }
    if (measuredWorkFactor == 0U) {
        return AuthBootstrapStatus::KdfUnavailable;
    }
    if (record.webCredentialEpoch == std::numeric_limits<std::uint64_t>::max() ||
        record.recordSequence == std::numeric_limits<std::uint64_t>::max() ||
        !makeVerifier(replacement, measuredWorkFactor, record.webPassword)) {
        return AuthBootstrapStatus::PersistenceFailure;
    }
    ++record.webCredentialEpoch;
    ++record.recordSequence;
    record.webLockout = AuthLockoutState{};
    return store_.writeCredentials(record) == AuthenticationWriteStatus::Success
               ? AuthBootstrapStatus::BootstrapAllowed
               : AuthBootstrapStatus::RecoveryRequired;
}

AuthBootstrapStatus AuthenticationDomain::changeServicePin(
    device_platform::StorageEpoch epoch, const std::string& current,
    const std::string& replacement, std::uint32_t measuredWorkFactor,
    std::uint64_t nowMs) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (validateServicePin(replacement) != AuthInputStatus::Valid) {
        return AuthBootstrapStatus::InvalidInput;
    }
    std::uint64_t retryAfter = 0U;
    const auto verified = verifyServicePin(epoch, current, nowMs, retryAfter);
    if (verified != AuthCheckStatus::Authenticated) {
        return verified == AuthCheckStatus::KdfUnavailable
                   ? AuthBootstrapStatus::KdfUnavailable
                   : verified == AuthCheckStatus::RecoveryRequired
                         ? AuthBootstrapStatus::RecoveryRequired
                         : AuthBootstrapStatus::InvalidInput;
    }
    const auto credentials = store_.readCredentials(epoch);
    if (credentials.status != AuthenticationReadStatus::Success ||
        !credentials.value.has_value()) {
        return AuthBootstrapStatus::RecoveryRequired;
    }
    auto record = *credentials.value;
    if (measuredWorkFactor == 0U) {
        measuredWorkFactor = record.servicePin.workFactor;
    }
    if (measuredWorkFactor == 0U) {
        return AuthBootstrapStatus::KdfUnavailable;
    }
    if (record.servicePinCredentialEpoch ==
            std::numeric_limits<std::uint64_t>::max() ||
        record.recordSequence == std::numeric_limits<std::uint64_t>::max() ||
        !makeVerifier(replacement, measuredWorkFactor, record.servicePin)) {
        return AuthBootstrapStatus::PersistenceFailure;
    }
    ++record.servicePinCredentialEpoch;
    ++record.recordSequence;
    record.servicePinLockout = AuthLockoutState{};
    return store_.writeCredentials(record) == AuthenticationWriteStatus::Success
               ? AuthBootstrapStatus::BootstrapAllowed
               : AuthBootstrapStatus::RecoveryRequired;
}

AuthBootstrapStatus AuthenticationDomain::setWebPasswordEnabled(
    device_platform::StorageEpoch epoch, const std::string& currentPassword,
    const std::string& replacementPassword, bool enabled, bool confirmed,
    std::uint64_t nowMs) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!confirmed) return AuthBootstrapStatus::InvalidInput;

    const auto root = store_.readRoot(epoch);
    const auto credentials = store_.readCredentials(epoch);
    if (root.status != AuthenticationReadStatus::Success ||
        !root.value.has_value() ||
        root.value->state != AuthProvisioningState::Provisioned ||
        credentials.status != AuthenticationReadStatus::Success ||
        !credentials.value.has_value()) {
        return AuthBootstrapStatus::RecoveryRequired;
    }

    auto record = *credentials.value;
    if (record.webPasswordEnabled == enabled) {
        return AuthBootstrapStatus::BootstrapAllowed;
    }

    if (enabled) {
        if (validateWebPassword(replacementPassword) != AuthInputStatus::Valid) {
            return AuthBootstrapStatus::InvalidInput;
        }
        if (record.webPassword.workFactor == 0U) {
            return AuthBootstrapStatus::KdfUnavailable;
        }
        if (!makeVerifier(replacementPassword, record.webPassword.workFactor,
                          record.webPassword)) {
            return AuthBootstrapStatus::KdfUnavailable;
        }
        if (record.webCredentialEpoch == std::numeric_limits<std::uint64_t>::max())
            return AuthBootstrapStatus::CommitOutcomeUnknown;
        ++record.webCredentialEpoch;
        record.webPasswordEnabled = true;
    } else {
        std::uint64_t retryAfterMs = 0U;
        const auto verified = verifyWebPassword(
            epoch, currentPassword, nowMs, retryAfterMs);
        if (verified != AuthCheckStatus::Authenticated) {
            return verified == AuthCheckStatus::KdfUnavailable
                       ? AuthBootstrapStatus::KdfUnavailable
                       : verified == AuthCheckStatus::RecoveryRequired
                             ? AuthBootstrapStatus::RecoveryRequired
                             : AuthBootstrapStatus::InvalidInput;
        }
        const auto reread = store_.readCredentials(epoch);
        if (reread.status != AuthenticationReadStatus::Success ||
            !reread.value.has_value()) {
            return AuthBootstrapStatus::RecoveryRequired;
        }
        record = *reread.value;
        record.webPasswordEnabled = false;
    }

    if (record.recordSequence == std::numeric_limits<std::uint64_t>::max()) {
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    }
    ++record.recordSequence;
    const auto persisted = store_.writeCredentials(record);
    if (persisted == AuthenticationWriteStatus::Success) {
        return AuthBootstrapStatus::BootstrapAllowed;
    }
    if (persisted == AuthenticationWriteStatus::CommitOutcomeUnknown) {
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    }
    return AuthBootstrapStatus::PersistenceFailure;
}

std::optional<bool> AuthenticationDomain::webPasswordEnabled(
    device_platform::StorageEpoch epoch) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto root = store_.readRoot(epoch);
    if (root.status != AuthenticationReadStatus::Success ||
        !root.value.has_value() ||
        root.value->state != AuthProvisioningState::Provisioned)
        return std::nullopt;
    const auto credentials = store_.readCredentials(epoch);
    if (credentials.status != AuthenticationReadStatus::Success ||
        !credentials.value.has_value())
        return std::nullopt;
    return credentials.value->webPasswordEnabled;
}

}  // namespace fermentation
