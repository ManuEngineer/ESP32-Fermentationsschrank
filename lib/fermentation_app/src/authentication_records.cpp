#include "authentication_records.hpp"

#include <algorithm>
#include <limits>
#include <mutex>

#include "big_endian_codec.hpp"
#include "byte_buffer.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "state_store_key.hpp"
#include "storage_envelope.hpp"

namespace fermentation {
namespace {

constexpr std::size_t kSingleCredentialPayloadBytes =
    2U + 4U + 8U + 1U + kAuthenticationSaltBytes + 1U +
    kAuthenticationVerifierBytes + 4U + 1U + 8U;
constexpr std::size_t kCredentialPayloadBytes =
    1U + 2U * kSingleCredentialPayloadBytes;
constexpr std::size_t kRootPayloadBytes = 1U + 8U + 8U;
constexpr std::size_t kEnvelopeOverheadBytes = 37U;
constexpr std::size_t kMaximumCredentialEnvelopeBytes =
    kCredentialPayloadBytes + kEnvelopeOverheadBytes;
constexpr std::size_t kMaximumRootEnvelopeBytes =
    kRootPayloadBytes + kEnvelopeOverheadBytes;
constexpr std::uint64_t kMaximumLockoutDurationMs = 30ULL * 60ULL * 1000ULL;

bool validLockout(const AuthLockoutState& state,
                  std::uint32_t maximumFailures) noexcept {
    return state.failedAttempts < maximumFailures &&
           state.lockoutStage <= 31U &&
           state.lockoutRemainingMs <= kMaximumLockoutDurationMs &&
           (state.lockoutRemainingMs == 0U || state.failedAttempts == 0U);
}

bool writeVerifier(device_platform::ByteWriter& writer,
                   const AuthVerifier& verifier,
                   const AuthLockoutState& lockout,
                   std::uint64_t credentialEpoch,
                   std::uint32_t maximumFailures) {
    using namespace device_platform::big_endian;
    return verifier.algorithmId == kAuthenticationPbkdf2Sha256Algorithm &&
           verifier.workFactor == kAuthenticationPbkdf2Sha256WorkFactor &&
           credentialEpoch != 0U && validLockout(lockout, maximumFailures) &&
           writeUint16(writer, verifier.algorithmId) &&
           writeUint32(writer, verifier.workFactor) &&
           writeUint64(writer, credentialEpoch) &&
           writeUint8(writer,
                      static_cast<std::uint8_t>(verifier.salt.size())) &&
           writer.writeBytes(verifier.salt.data(), verifier.salt.size()) &&
           writeUint8(writer,
                      static_cast<std::uint8_t>(verifier.verifier.size())) &&
           writer.writeBytes(verifier.verifier.data(),
                             verifier.verifier.size()) &&
           writeUint32(writer, lockout.failedAttempts) &&
           writeUint8(writer, lockout.lockoutStage) &&
           writeUint64(writer, lockout.lockoutRemainingMs);
}

bool readVerifier(device_platform::ByteReader& reader, AuthVerifier& verifier,
                  AuthLockoutState& lockout, std::uint64_t& credentialEpoch,
                  std::uint32_t maximumFailures) {
    using namespace device_platform::big_endian;
    std::uint8_t saltLength = 0U;
    std::uint8_t verifierLength = 0U;
    if (!readUint16(reader, verifier.algorithmId) ||
        !readUint32(reader, verifier.workFactor) ||
        !readUint64(reader, credentialEpoch) ||
        !readUint8(reader, saltLength) || saltLength != verifier.salt.size() ||
        !reader.readBytes(verifier.salt.data(), verifier.salt.size()) ||
        !readUint8(reader, verifierLength) ||
        verifierLength != verifier.verifier.size() ||
        !reader.readBytes(verifier.verifier.data(), verifier.verifier.size()) ||
        !readUint32(reader, lockout.failedAttempts) ||
        !readUint8(reader, lockout.lockoutStage) ||
        !readUint64(reader, lockout.lockoutRemainingMs)) {
        return false;
    }
    return verifier.algorithmId == kAuthenticationPbkdf2Sha256Algorithm &&
           verifier.workFactor == kAuthenticationPbkdf2Sha256WorkFactor &&
           credentialEpoch != 0U && validLockout(lockout, maximumFailures);
}

device_platform::StateStoreKey keyFor(const char* name) {
    auto created = device_platform::StateStoreKey::create(name);
    // Keys are compile-time constants validated by the storage contract.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    return std::move(*created.key);
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

AuthenticationWriteStatus mapWrite(device_platform::StateStoreWriteStatus s) {
    switch (s) {
        case device_platform::StateStoreWriteStatus::Success:
            return AuthenticationWriteStatus::Success;
        case device_platform::StateStoreWriteStatus::WriteError:
            return AuthenticationWriteStatus::WriteError;
        case device_platform::StateStoreWriteStatus::CapacityError:
            return AuthenticationWriteStatus::CapacityError;
        case device_platform::StateStoreWriteStatus::CommitOutcomeUnknown:
            return AuthenticationWriteStatus::CommitOutcomeUnknown;
    }
    return AuthenticationWriteStatus::IntegrityFailure;
}

template <typename Record, typename Decoder>
AuthenticationWriteStatus writeExact(device_platform::IStateStore& store,
                                     const device_platform::StateStoreKey& key,
                                     const std::string& encoded,
                                     std::size_t maxBytes, Decoder decode,
                                     const Record& expected) {
    const auto writeStatus = store.write(key, encoded);
    if (writeStatus == device_platform::StateStoreWriteStatus::WriteError ||
        writeStatus == device_platform::StateStoreWriteStatus::CapacityError) {
        return mapWrite(writeStatus);
    }
    const auto readback = store.read(key, maxBytes);
    if (readback.status != device_platform::StateStoreReadStatus::Success) {
        return writeStatus == device_platform::StateStoreWriteStatus::
                                  CommitOutcomeUnknown
                   ? AuthenticationWriteStatus::CommitOutcomeUnknown
                   : AuthenticationWriteStatus::ReadbackFailure;
    }
    const auto decoded = decode(readback.value);
    if (!decoded.value.has_value() || !(*decoded.value == expected) ||
        readback.value != encoded) {
        return writeStatus == device_platform::StateStoreWriteStatus::
                                  CommitOutcomeUnknown
                   ? AuthenticationWriteStatus::CommitOutcomeUnknown
                   : AuthenticationWriteStatus::IntegrityFailure;
    }
    return AuthenticationWriteStatus::Success;
}

bool incremented(std::uint64_t previous, std::uint64_t target) noexcept {
    return previous != std::numeric_limits<std::uint64_t>::max() &&
           target == previous + 1U;
}

bool validRootTransition(const AuthProvisioningRoot& previous,
                         const AuthProvisioningRoot& target) noexcept {
    if (previous.storageEpoch != target.storageEpoch ||
        previous.authDomainGeneration != target.authDomainGeneration ||
        previous.bootstrapSequenceBinding != target.bootstrapSequenceBinding ||
        !incremented(previous.recordSequence, target.recordSequence)) {
        return false;
    }
    switch (previous.state) {
        case AuthProvisioningState::Unprovisioned:
            return target.state == AuthProvisioningState::Provisioning;
        case AuthProvisioningState::Provisioning:
            return target.state == AuthProvisioningState::Provisioned ||
                   target.state == AuthProvisioningState::Indeterminate ||
                   target.state == AuthProvisioningState::RecoveryRequired;
        case AuthProvisioningState::Provisioned:
            return target.state == AuthProvisioningState::Provisioning;
        case AuthProvisioningState::Indeterminate:
        case AuthProvisioningState::RecoveryRequired:
            return false;
    }
    return false;
}

template <typename Record, typename Decoder>
AuthenticationWriteStatus compareAndWrite(
    device_platform::IStateStore& store,
    const device_platform::StateStoreKey& key, const Record& expected,
    const Record& target, std::size_t maxBytes,
    AuthenticationRecordCodecStatus (*encode)(const Record&, std::string&),
    Decoder decode) {
    auto oldEncoded = std::string{};
    auto newEncoded = std::string{};
    if (encode(expected, oldEncoded) !=
            AuthenticationRecordCodecStatus::Success ||
        encode(target, newEncoded) !=
            AuthenticationRecordCodecStatus::Success) {
        return AuthenticationWriteStatus::IntegrityFailure;
    }
    const auto current = store.read(key, maxBytes);
    if (current.status != device_platform::StateStoreReadStatus::Success) {
        return current.status ==
                       device_platform::StateStoreReadStatus::ReadError
                   ? AuthenticationWriteStatus::ReadbackFailure
               : current.status ==
                       device_platform::StateStoreReadStatus::CapacityError
                   ? AuthenticationWriteStatus::CapacityError
                   : AuthenticationWriteStatus::InvalidTransition;
    }
    const auto decoded = decode(current.value);
    if (!decoded.value.has_value() || !(*decoded.value == expected) ||
        current.value != oldEncoded) {
        return AuthenticationWriteStatus::InvalidTransition;
    }
    return writeExact(store, key, newEncoded, maxBytes, decode, target);
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
            (width == 4U && codePoint < 0x10000U) || codePoint > 0x10FFFFU ||
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
           left.lockoutRemainingMs == right.lockoutRemainingMs;
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
           left.recordSequence == right.recordSequence &&
           left.state == right.state &&
           left.authDomainGeneration == right.authDomainGeneration &&
           left.bootstrapSequenceBinding == right.bootstrapSequenceBinding;
}

bool isPlausible(const AuthenticationCredentialRecord& record) noexcept {
    return record.storageEpoch.value() != 0U && record.recordSequence != 0U &&
           record.webCredentialEpoch != 0U &&
           record.servicePinCredentialEpoch != 0U &&
           record.webPassword.algorithmId ==
               kAuthenticationPbkdf2Sha256Algorithm &&
           record.webPassword.workFactor ==
               kAuthenticationPbkdf2Sha256WorkFactor &&
           record.servicePin.algorithmId ==
               kAuthenticationPbkdf2Sha256Algorithm &&
           record.servicePin.workFactor ==
               kAuthenticationPbkdf2Sha256WorkFactor &&
           validLockout(record.webLockout, 5U) &&
           validLockout(record.servicePinLockout, 3U);
}

bool isPlausible(const AuthProvisioningRoot& root) noexcept {
    switch (root.state) {
        case AuthProvisioningState::Unprovisioned:
        case AuthProvisioningState::Provisioning:
        case AuthProvisioningState::Provisioned:
        case AuthProvisioningState::Indeterminate:
        case AuthProvisioningState::RecoveryRequired:
            return root.storageEpoch.value() != 0U &&
                   root.recordSequence != 0U &&
                   root.authDomainGeneration != 0U &&
                   root.bootstrapSequenceBinding != 0U;
    }
    return false;
}

AuthenticationRecordCodecStatus encodeAuthenticationCredential(
    const AuthenticationCredentialRecord& record, std::string& out) {
    if (!isPlausible(record))
        return AuthenticationRecordCodecStatus::InvalidModel;
    device_platform::ByteWriter payload(kCredentialPayloadBytes);
    if (!device_platform::big_endian::writeBool(payload,
                                                record.webPasswordEnabled) ||
        !writeVerifier(payload, record.webPassword, record.webLockout,
                       record.webCredentialEpoch, 5U) ||
        !writeVerifier(payload, record.servicePin, record.servicePinLockout,
                       record.servicePinCredentialEpoch, 3U) ||
        payload.size() != kCredentialPayloadBytes) {
        return AuthenticationRecordCodecStatus::CapacityExceeded;
    }
    std::string encoded;
    const auto status = device_platform::encodeEnvelope(
        {configuration_storage_contract::kAuthenticationRecordType,
         configuration_storage_contract::kAuthenticationSchemaVersion,
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
    if (envelope.status ==
            device_platform::EnvelopeDecodeStatus::UnknownEnvelopeVersion ||
        (envelope.status == device_platform::EnvelopeDecodeStatus::Success &&
         envelope.envelope.has_value() &&
         envelope.envelope->schemaVersion >
             configuration_storage_contract::kAuthenticationSchemaVersion)) {
        return {AuthenticationRecordCodecStatus::UnsupportedSchema,
                std::nullopt};
    }
    if (envelope.status != device_platform::EnvelopeDecodeStatus::Success ||
        !envelope.envelope.has_value()) {
        return {AuthenticationRecordCodecStatus::InvalidEnvelope, std::nullopt};
    }
    const auto& value = *envelope.envelope;
    if (value.recordTypeId !=
        configuration_storage_contract::kAuthenticationRecordType) {
        return {AuthenticationRecordCodecStatus::RecordIdentityMismatch,
                std::nullopt};
    }
    if (value.schemaVersion !=
            configuration_storage_contract::kAuthenticationSchemaVersion ||
        value.utcUnixSeconds.has_value() ||
        value.payload.size() != kCredentialPayloadBytes) {
        return {AuthenticationRecordCodecStatus::InvalidModel, std::nullopt};
    }
    device_platform::ByteReader reader(value.payload);
    AuthenticationCredentialRecord record;
    record.storageEpoch = value.storageEpoch;
    record.recordSequence = value.versionValue;
    if (!device_platform::big_endian::readBool(reader,
                                               record.webPasswordEnabled) ||
        !readVerifier(reader, record.webPassword, record.webLockout,
                      record.webCredentialEpoch, 5U) ||
        !readVerifier(reader, record.servicePin, record.servicePinLockout,
                      record.servicePinCredentialEpoch, 3U) ||
        reader.remaining() != 0U || !isPlausible(record)) {
        return {AuthenticationRecordCodecStatus::InvalidModel, std::nullopt};
    }
    return {AuthenticationRecordCodecStatus::Success, record};
}

AuthenticationRecordCodecStatus encodeAuthProvisioningRoot(
    const AuthProvisioningRoot& root, std::string& out) {
    if (!isPlausible(root))
        return AuthenticationRecordCodecStatus::InvalidModel;
    device_platform::ByteWriter payload(kRootPayloadBytes);
    if (!device_platform::big_endian::writeUint8(
            payload, static_cast<std::uint8_t>(root.state)) ||
        !device_platform::big_endian::writeUint64(payload,
                                                  root.authDomainGeneration) ||
        !device_platform::big_endian::writeUint64(
            payload, root.bootstrapSequenceBinding) ||
        payload.size() != kRootPayloadBytes) {
        return AuthenticationRecordCodecStatus::CapacityExceeded;
    }
    std::string encoded;
    const auto status = device_platform::encodeEnvelope(
        {configuration_storage_contract::kAuthenticationRootRecordType,
         configuration_storage_contract::kAuthenticationRootSchemaVersion,
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
    if (envelope.status ==
            device_platform::EnvelopeDecodeStatus::UnknownEnvelopeVersion ||
        (envelope.status == device_platform::EnvelopeDecodeStatus::Success &&
         envelope.envelope.has_value() &&
         envelope.envelope->schemaVersion >
             configuration_storage_contract::
                 kAuthenticationRootSchemaVersion)) {
        return {AuthenticationRecordCodecStatus::UnsupportedSchema,
                std::nullopt};
    }
    if (envelope.status != device_platform::EnvelopeDecodeStatus::Success ||
        !envelope.envelope.has_value()) {
        return {AuthenticationRecordCodecStatus::InvalidEnvelope, std::nullopt};
    }
    const auto& value = *envelope.envelope;
    if (value.recordTypeId !=
        configuration_storage_contract::kAuthenticationRootRecordType) {
        return {AuthenticationRecordCodecStatus::RecordIdentityMismatch,
                std::nullopt};
    }
    if (value.schemaVersion !=
            configuration_storage_contract::kAuthenticationRootSchemaVersion ||
        value.utcUnixSeconds.has_value() ||
        value.payload.size() != kRootPayloadBytes) {
        return {AuthenticationRecordCodecStatus::InvalidModel, std::nullopt};
    }
    device_platform::ByteReader reader(value.payload);
    std::uint8_t state = 0U;
    AuthProvisioningRoot root;
    root.storageEpoch = value.storageEpoch;
    root.recordSequence = value.versionValue;
    if (!device_platform::big_endian::readUint8(reader, state) ||
        !device_platform::big_endian::readUint64(reader,
                                                 root.authDomainGeneration) ||
        !device_platform::big_endian::readUint64(
            reader, root.bootstrapSequenceBinding) ||
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
    const auto read = store_.read(
        keyFor(configuration_storage_contract::kAuthenticationStoreKey),
        kMaximumCredentialEnvelopeBytes);
    if (read.status != device_platform::StateStoreReadStatus::Success)
        return {mapRead(read.status), std::nullopt};
    const auto decoded = decodeAuthenticationCredential(read.value);
    if (!decoded.value.has_value()) {
        return {
            decoded.status == AuthenticationRecordCodecStatus::UnsupportedSchema
                ? AuthenticationReadStatus::UnsupportedSchema
                : AuthenticationReadStatus::IntegrityFailure,
            std::nullopt};
    }
    if (decoded.value->storageEpoch != expectedEpoch)
        return {AuthenticationReadStatus::DifferentEpoch, decoded.value};
    return {AuthenticationReadStatus::Success, decoded.value};
}

AuthProvisioningRootReadResult AuthenticationRecordStore::readRoot(
    device_platform::StorageEpoch expectedEpoch) const {
    const auto read = store_.read(
        keyFor(configuration_storage_contract::kAuthenticationRootStoreKey),
        kMaximumRootEnvelopeBytes);
    if (read.status != device_platform::StateStoreReadStatus::Success)
        return {mapRead(read.status), std::nullopt};
    const auto decoded = decodeAuthProvisioningRoot(read.value);
    if (!decoded.value.has_value()) {
        return {
            decoded.status == AuthenticationRecordCodecStatus::UnsupportedSchema
                ? AuthenticationReadStatus::UnsupportedSchema
                : AuthenticationReadStatus::IntegrityFailure,
            std::nullopt};
    }
    if (decoded.value->storageEpoch != expectedEpoch)
        return {AuthenticationReadStatus::DifferentEpoch, decoded.value};
    return {AuthenticationReadStatus::Success, decoded.value};
}

AuthenticationWriteStatus AuthenticationRecordStore::writeCredentials(
    const AuthenticationCredentialRecord& expected,
    const AuthenticationCredentialRecord& target) {
    if (!isPlausible(expected) || !isPlausible(target) ||
        expected.storageEpoch != target.storageEpoch ||
        !incremented(expected.recordSequence, target.recordSequence)) {
        return AuthenticationWriteStatus::InvalidTransition;
    }
    return compareAndWrite(
        store_, keyFor(configuration_storage_contract::kAuthenticationStoreKey),
        expected, target, kMaximumCredentialEnvelopeBytes,
        encodeAuthenticationCredential, decodeAuthenticationCredential);
}

AuthenticationWriteStatus AuthenticationRecordStore::writeRoot(
    const AuthProvisioningRoot& expected, const AuthProvisioningRoot& target) {
    if (!isPlausible(expected) || !isPlausible(target) ||
        !validRootTransition(expected, target)) {
        return AuthenticationWriteStatus::InvalidTransition;
    }
    return compareAndWrite(
        store_,
        keyFor(configuration_storage_contract::kAuthenticationRootStoreKey),
        expected, target, kMaximumRootEnvelopeBytes, encodeAuthProvisioningRoot,
        decodeAuthProvisioningRoot);
}

AuthenticationWriteStatus AuthenticationRecordStore::writeInitialCredentials(
    const AuthenticationCredentialRecord& record,
    bool authorizedEpochReplacement) {
    if (!isPlausible(record) || record.recordSequence != 1U)
        return AuthenticationWriteStatus::InvalidTransition;
    const auto key =
        keyFor(configuration_storage_contract::kAuthenticationStoreKey);
    const auto existing = store_.read(key, kMaximumCredentialEnvelopeBytes);
    if (existing.status == device_platform::StateStoreReadStatus::ReadError)
        return AuthenticationWriteStatus::ReadbackFailure;
    if (existing.status == device_platform::StateStoreReadStatus::CapacityError)
        return AuthenticationWriteStatus::CapacityError;
    if (existing.status == device_platform::StateStoreReadStatus::Success) {
        const auto decoded = decodeAuthenticationCredential(existing.value);
        if (!decoded.value.has_value())
            return AuthenticationWriteStatus::IntegrityFailure;
        if (!authorizedEpochReplacement ||
            decoded.value->storageEpoch == record.storageEpoch)
            return AuthenticationWriteStatus::InvalidTransition;
    }
    std::string encoded;
    if (encodeAuthenticationCredential(record, encoded) !=
        AuthenticationRecordCodecStatus::Success)
        return AuthenticationWriteStatus::IntegrityFailure;
    return writeExact(store_, key, encoded, kMaximumCredentialEnvelopeBytes,
                      decodeAuthenticationCredential, record);
}

AuthenticationWriteStatus AuthenticationRecordStore::writeInitialRoot(
    const AuthProvisioningRoot& root, bool authorizedEpochReplacement) {
    if (!isPlausible(root) || root.recordSequence != 1U ||
        root.state != AuthProvisioningState::Unprovisioned)
        return AuthenticationWriteStatus::InvalidTransition;
    const auto key =
        keyFor(configuration_storage_contract::kAuthenticationRootStoreKey);
    const auto existing = store_.read(key, kMaximumRootEnvelopeBytes);
    if (existing.status == device_platform::StateStoreReadStatus::ReadError)
        return AuthenticationWriteStatus::ReadbackFailure;
    if (existing.status == device_platform::StateStoreReadStatus::CapacityError)
        return AuthenticationWriteStatus::CapacityError;
    if (existing.status == device_platform::StateStoreReadStatus::Success) {
        const auto decoded = decodeAuthProvisioningRoot(existing.value);
        if (!decoded.value.has_value())
            return AuthenticationWriteStatus::IntegrityFailure;
        if (!authorizedEpochReplacement ||
            decoded.value->storageEpoch == root.storageEpoch)
            return AuthenticationWriteStatus::InvalidTransition;
    }
    std::string encoded;
    if (encodeAuthProvisioningRoot(root, encoded) !=
        AuthenticationRecordCodecStatus::Success)
        return AuthenticationWriteStatus::IntegrityFailure;
    return writeExact(store_, key, encoded, kMaximumRootEnvelopeBytes,
                      decodeAuthProvisioningRoot, root);
}

AuthInputStatus validateWebPassword(const std::string& value) noexcept {
    if (value.size() > 256U) return AuthInputStatus::CapacityExceeded;
    std::size_t codePoints = 0U;
    if (!validUtf8(value, codePoints)) return AuthInputStatus::InvalidUtf8;
    return codePoints >= 15U && codePoints <= 64U
               ? AuthInputStatus::Valid
               : AuthInputStatus::InvalidLength;
}

AuthInputStatus validateServicePin(const std::string& value) noexcept {
    if (value.size() != 4U) return AuthInputStatus::InvalidLength;
    for (const unsigned char byte : value) {
        if (byte < '0' || byte > '9') return AuthInputStatus::InvalidLength;
    }
    return AuthInputStatus::Valid;
}

bool constantTimeEqual(
    const std::array<std::uint8_t, kAuthenticationVerifierBytes>& left,
    const std::array<std::uint8_t, kAuthenticationVerifierBytes>&
        right) noexcept {
    std::uint8_t difference = 0U;
    for (std::size_t index = 0U; index < left.size(); ++index)
        difference |= left[index] ^ right[index];
    return difference == 0U;
}

AuthenticationCredentialReadResult AuthenticationDomain::readActiveCredentials(
    const AuthenticationBootstrapContext& context) const {
    const auto root = readActiveRoot(context);
    if (root.status != AuthenticationReadStatus::Success ||
        !root.value.has_value() ||
        root.value->state != AuthProvisioningState::Provisioned) {
        return {AuthenticationReadStatus::IntegrityFailure, std::nullopt};
    }
    return store_.readCredentials(context.storageEpoch());
}

AuthProvisioningRootReadResult AuthenticationDomain::readActiveRoot(
    const AuthenticationBootstrapContext& context) const {
    if (!context.validFor(*store_.storeIdentity()))
        return {AuthenticationReadStatus::IntegrityFailure, std::nullopt};
    auto root = store_.readRoot(context.storageEpoch());
    if (root.status != AuthenticationReadStatus::Success ||
        !root.value.has_value() ||
        root.value->bootstrapSequenceBinding != context.bootstrapSequence()) {
        return {AuthenticationReadStatus::IntegrityFailure, std::nullopt};
    }
    return root;
}

AuthBootstrapStatus AuthenticationDomain::inspect(
    const AuthenticationBootstrapContext& context) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto root = readActiveRoot(context);
    if (root.status != AuthenticationReadStatus::Success ||
        !root.value.has_value())
        return AuthBootstrapStatus::RecoveryRequired;
    if (root.value->state == AuthProvisioningState::Unprovisioned) {
        const auto credentials = store_.readCredentials(context.storageEpoch());
        return credentials.status == AuthenticationReadStatus::NotFound ||
                       credentials.status ==
                           AuthenticationReadStatus::DifferentEpoch
                   ? AuthBootstrapStatus::BootstrapAllowed
                   : AuthBootstrapStatus::RecoveryRequired;
    }
    if (root.value->state == AuthProvisioningState::Provisioned) {
        const auto credentials = store_.readCredentials(context.storageEpoch());
        return credentials.status == AuthenticationReadStatus::Success &&
                       credentials.value.has_value()
                   ? AuthBootstrapStatus::AlreadyProvisioned
                   : AuthBootstrapStatus::RecoveryRequired;
    }
    return AuthBootstrapStatus::RecoveryRequired;
}

std::uint64_t AuthenticationDomain::effectiveLockoutRemaining(
    const AuthLockoutState& persisted, std::uint64_t recordSequence,
    std::uint64_t nowMs, LockoutClock& clock) const noexcept {
    if (persisted.lockoutRemainingMs == 0U) {
        clock = LockoutClock{};
        return 0U;
    }
    if (!clock.initialized || clock.recordSequence != recordSequence ||
        nowMs < clock.anchorMs) {
        clock = LockoutClock{recordSequence, nowMs,
                             persisted.lockoutRemainingMs, true};
        return clock.remainingMs;
    }
    const auto elapsed = nowMs - clock.anchorMs;
    if (elapsed >= clock.remainingMs) {
        clock = LockoutClock{};
        return 0U;
    }
    clock.remainingMs -= elapsed;
    clock.anchorMs = nowMs;
    return clock.remainingMs;
}

bool AuthenticationDomain::makeVerifier(const std::string& secret,
                                        AuthVerifier& out) {
    out.algorithmId = kAuthenticationPbkdf2Sha256Algorithm;
    out.workFactor = kAuthenticationPbkdf2Sha256WorkFactor;
    if (!random_.fill(out.salt.data(), out.salt.size())) return false;
    return kdf_.derive(secret, out, out.verifier);
}

AuthBootstrapStatus AuthenticationDomain::bootstrap(
    const AuthenticationBootstrapContext& context,
    device_platform::UiSurface surface, bool confirmed,
    const std::string& password, const std::string& servicePin) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto epoch = context.storageEpoch();
    if (!context.validFor(*store_.storeIdentity()))
        return AuthBootstrapStatus::RecoveryRequired;
    if (surface != device_platform::UiSurface::LocalDisplay || !confirmed)
        return AuthBootstrapStatus::InvalidInput;
    if (validateWebPassword(password) != AuthInputStatus::Valid ||
        validateServicePin(servicePin) != AuthInputStatus::Valid)
        return AuthBootstrapStatus::InvalidInput;
    auto rootResult = readActiveRoot(context);
    if (rootResult.status != AuthenticationReadStatus::Success ||
        !rootResult.value.has_value() ||
        rootResult.value->state != AuthProvisioningState::Unprovisioned)
        return AuthBootstrapStatus::RecoveryRequired;
    const auto credentialsBefore = store_.readCredentials(epoch);
    if (credentialsBefore.status != AuthenticationReadStatus::NotFound &&
        credentialsBefore.status != AuthenticationReadStatus::DifferentEpoch)
        return AuthBootstrapStatus::RecoveryRequired;

    auto provisioning = *rootResult.value;
    if (provisioning.recordSequence ==
        std::numeric_limits<std::uint64_t>::max())
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    const auto rootBefore = provisioning;
    provisioning.state = AuthProvisioningState::Provisioning;
    ++provisioning.recordSequence;
    const auto phase = store_.writeRoot(rootBefore, provisioning);
    if (phase == AuthenticationWriteStatus::CommitOutcomeUnknown)
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    if (phase != AuthenticationWriteStatus::Success)
        return AuthBootstrapStatus::PersistenceFailure;

    AuthenticationCredentialRecord record;
    record.storageEpoch = epoch;
    if (!makeVerifier(password, record.webPassword) ||
        !makeVerifier(servicePin, record.servicePin)) {
        const auto recovery = provisioning;
        if (provisioning.recordSequence !=
            std::numeric_limits<std::uint64_t>::max()) {
            provisioning.state = AuthProvisioningState::RecoveryRequired;
            ++provisioning.recordSequence;
            static_cast<void>(store_.writeRoot(recovery, provisioning));
        }
        return AuthBootstrapStatus::KdfUnavailable;
    }
    const auto credentialWrite = store_.writeInitialCredentials(record, true);
    if (credentialWrite != AuthenticationWriteStatus::Success) {
        if (credentialWrite == AuthenticationWriteStatus::CommitOutcomeUnknown)
            return AuthBootstrapStatus::CommitOutcomeUnknown;
        const auto recovery = provisioning;
        if (provisioning.recordSequence !=
            std::numeric_limits<std::uint64_t>::max()) {
            provisioning.state = AuthProvisioningState::RecoveryRequired;
            ++provisioning.recordSequence;
            static_cast<void>(store_.writeRoot(recovery, provisioning));
        }
        return AuthBootstrapStatus::PersistenceFailure;
    }
    if (provisioning.recordSequence ==
        std::numeric_limits<std::uint64_t>::max())
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    const auto completedFrom = provisioning;
    provisioning.state = AuthProvisioningState::Provisioned;
    ++provisioning.recordSequence;
    const auto completed = store_.writeRoot(completedFrom, provisioning);
    if (completed != AuthenticationWriteStatus::Success)
        return completed == AuthenticationWriteStatus::CommitOutcomeUnknown
                   ? AuthBootstrapStatus::CommitOutcomeUnknown
                   : AuthBootstrapStatus::PersistenceFailure;
    return AuthBootstrapStatus::BootstrapAllowed;
}

AuthCheckStatus AuthenticationDomain::verifyWebPassword(
    const AuthenticationBootstrapContext& context, const std::string& password,
    std::uint64_t nowMs, std::uint64_t& retryAfterMs) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!context.validFor(*store_.storeIdentity()))
        return AuthCheckStatus::RecoveryRequired;
    retryAfterMs = 0U;
    if (validateWebPassword(password) != AuthInputStatus::Valid)
        return AuthCheckStatus::Invalid;
    auto loaded = readActiveCredentials(context);
    if (loaded.status != AuthenticationReadStatus::Success || !loaded.value)
        return AuthCheckStatus::RecoveryRequired;
    auto record = *loaded.value;
    const auto expected = record;
    if (!record.webPasswordEnabled) return AuthCheckStatus::Disabled;
    auto& lockout = record.webLockout;
    auto remaining = effectiveLockoutRemaining(lockout, record.recordSequence,
                                               nowMs, webLockoutClock_);
    if (remaining != 0U) {
        retryAfterMs = remaining;
        return AuthCheckStatus::LockedOut;
    }
    lockout.lockoutRemainingMs = 0U;
    std::array<std::uint8_t, kAuthenticationVerifierBytes> derived{};
    if (!kdf_.derive(password, record.webPassword, derived))
        return AuthCheckStatus::KdfUnavailable;
    if (constantTimeEqual(derived, record.webPassword.verifier)) {
        record.webLockout = AuthLockoutState{};
    } else {
        ++lockout.failedAttempts;
        if (lockout.failedAttempts >= 5U) {
            lockout.failedAttempts = 0U;
            if (lockout.lockoutStage < 31U) ++lockout.lockoutStage;
            const auto exponent = std::min<std::uint8_t>(
                static_cast<std::uint8_t>(lockout.lockoutStage - 1U), 5U);
            lockout.lockoutRemainingMs = std::min<std::uint64_t>(
                15ULL * 60ULL * 1000ULL, (30ULL * 1000ULL) << exponent);
            retryAfterMs = lockout.lockoutRemainingMs;
        }
    }
    if (record.recordSequence == std::numeric_limits<std::uint64_t>::max())
        return AuthCheckStatus::RecoveryRequired;
    auto target = record;
    ++target.recordSequence;
    const auto persisted = store_.writeCredentials(expected, target);
    if (persisted != AuthenticationWriteStatus::Success)
        return AuthCheckStatus::RecoveryRequired;
    webLockoutClock_ = LockoutClock{target.recordSequence, nowMs,
                                    target.webLockout.lockoutRemainingMs, true};
    return constantTimeEqual(derived, record.webPassword.verifier)
               ? AuthCheckStatus::Authenticated
               : AuthCheckStatus::Invalid;
}

AuthCheckStatus AuthenticationDomain::verifyServicePin(
    const AuthenticationBootstrapContext& context,
    const std::string& servicePin, std::uint64_t nowMs,
    std::uint64_t& retryAfterMs) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!context.validFor(*store_.storeIdentity()))
        return AuthCheckStatus::RecoveryRequired;
    retryAfterMs = 0U;
    if (validateServicePin(servicePin) != AuthInputStatus::Valid)
        return AuthCheckStatus::Invalid;
    auto loaded = readActiveCredentials(context);
    if (loaded.status != AuthenticationReadStatus::Success || !loaded.value)
        return AuthCheckStatus::RecoveryRequired;
    auto record = *loaded.value;
    const auto expected = record;
    auto& lockout = record.servicePinLockout;
    const auto remaining = effectiveLockoutRemaining(
        lockout, record.recordSequence, nowMs, servicePinLockoutClock_);
    if (remaining != 0U) {
        retryAfterMs = remaining;
        return AuthCheckStatus::LockedOut;
    }
    lockout.lockoutRemainingMs = 0U;
    std::array<std::uint8_t, kAuthenticationVerifierBytes> derived{};
    if (!kdf_.derive(servicePin, record.servicePin, derived))
        return AuthCheckStatus::KdfUnavailable;
    const bool authenticated =
        constantTimeEqual(derived, record.servicePin.verifier);
    if (authenticated) {
        record.servicePinLockout = AuthLockoutState{};
    } else {
        ++lockout.failedAttempts;
        if (lockout.failedAttempts >= 3U) {
            lockout.failedAttempts = 0U;
            if (lockout.lockoutStage < 31U) ++lockout.lockoutStage;
            const auto exponent = std::min<std::uint8_t>(
                static_cast<std::uint8_t>(lockout.lockoutStage - 1U), 6U);
            lockout.lockoutRemainingMs = std::min<std::uint64_t>(
                30ULL * 60ULL * 1000ULL, (30ULL * 1000ULL) << exponent);
            retryAfterMs = lockout.lockoutRemainingMs;
        }
    }
    if (record.recordSequence == std::numeric_limits<std::uint64_t>::max())
        return AuthCheckStatus::RecoveryRequired;
    auto target = record;
    ++target.recordSequence;
    const auto persisted = store_.writeCredentials(expected, target);
    if (persisted != AuthenticationWriteStatus::Success)
        return AuthCheckStatus::RecoveryRequired;
    servicePinLockoutClock_ =
        LockoutClock{target.recordSequence, nowMs,
                     target.servicePinLockout.lockoutRemainingMs, true};
    return authenticated ? AuthCheckStatus::Authenticated
                         : AuthCheckStatus::Invalid;
}

AuthBootstrapStatus AuthenticationDomain::changeWebPassword(
    const AuthenticationBootstrapContext& context, const std::string& current,
    const std::string& replacement, std::uint64_t nowMs) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto epoch = context.storageEpoch();
    if (!context.validFor(*store_.storeIdentity()))
        return AuthBootstrapStatus::RecoveryRequired;
    if (validateWebPassword(replacement) != AuthInputStatus::Valid)
        return AuthBootstrapStatus::InvalidInput;
    std::uint64_t retryAfter = 0U;
    if (verifyWebPassword(context, current, nowMs, retryAfter) !=
        AuthCheckStatus::Authenticated)
        return AuthBootstrapStatus::RecoveryRequired;
    auto rootRead = readActiveRoot(context);
    auto credentialRead = store_.readCredentials(epoch);
    if (!rootRead.value || !credentialRead.value)
        return AuthBootstrapStatus::RecoveryRequired;
    auto verifier = credentialRead.value->webPassword;
    if (!makeVerifier(replacement, verifier) ||
        credentialRead.value->webCredentialEpoch ==
            std::numeric_limits<std::uint64_t>::max() ||
        credentialRead.value->recordSequence ==
            std::numeric_limits<std::uint64_t>::max())
        return AuthBootstrapStatus::KdfUnavailable;
    auto rootProvisioning = *rootRead.value;
    auto rootLocked = rootProvisioning;
    if (rootProvisioning.recordSequence ==
        std::numeric_limits<std::uint64_t>::max())
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    rootLocked.state = AuthProvisioningState::Provisioning;
    ++rootLocked.recordSequence;
    if (store_.writeRoot(rootProvisioning, rootLocked) !=
        AuthenticationWriteStatus::Success)
        return AuthBootstrapStatus::RecoveryRequired;
    auto target = *credentialRead.value;
    target.webPassword = verifier;
    ++target.webCredentialEpoch;
    ++target.recordSequence;
    target.webLockout = AuthLockoutState{};
    const auto credStatus =
        store_.writeCredentials(*credentialRead.value, target);
    if (credStatus != AuthenticationWriteStatus::Success)
        return credStatus == AuthenticationWriteStatus::CommitOutcomeUnknown
                   ? AuthBootstrapStatus::CommitOutcomeUnknown
                   : AuthBootstrapStatus::RecoveryRequired;
    auto rootProvisioned = rootLocked;
    if (rootLocked.recordSequence == std::numeric_limits<std::uint64_t>::max())
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    rootProvisioned.state = AuthProvisioningState::Provisioned;
    ++rootProvisioned.recordSequence;
    return store_.writeRoot(rootLocked, rootProvisioned) ==
                   AuthenticationWriteStatus::Success
               ? AuthBootstrapStatus::BootstrapAllowed
               : AuthBootstrapStatus::RecoveryRequired;
}

AuthBootstrapStatus AuthenticationDomain::changeServicePin(
    const AuthenticationBootstrapContext& context, const std::string& current,
    const std::string& replacement, std::uint64_t nowMs) {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto epoch = context.storageEpoch();
    if (!context.validFor(*store_.storeIdentity()))
        return AuthBootstrapStatus::RecoveryRequired;
    if (validateServicePin(replacement) != AuthInputStatus::Valid)
        return AuthBootstrapStatus::InvalidInput;
    std::uint64_t retryAfter = 0U;
    if (verifyServicePin(context, current, nowMs, retryAfter) !=
        AuthCheckStatus::Authenticated)
        return AuthBootstrapStatus::RecoveryRequired;
    auto rootRead = readActiveRoot(context);
    auto credentialRead = store_.readCredentials(epoch);
    if (!rootRead.value || !credentialRead.value)
        return AuthBootstrapStatus::RecoveryRequired;
    auto verifier = credentialRead.value->servicePin;
    if (!makeVerifier(replacement, verifier) ||
        credentialRead.value->servicePinCredentialEpoch ==
            std::numeric_limits<std::uint64_t>::max() ||
        credentialRead.value->recordSequence ==
            std::numeric_limits<std::uint64_t>::max())
        return AuthBootstrapStatus::KdfUnavailable;
    auto rootProvisioning = *rootRead.value;
    auto rootLocked = rootProvisioning;
    if (rootProvisioning.recordSequence ==
        std::numeric_limits<std::uint64_t>::max())
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    rootLocked.state = AuthProvisioningState::Provisioning;
    ++rootLocked.recordSequence;
    if (store_.writeRoot(rootProvisioning, rootLocked) !=
        AuthenticationWriteStatus::Success)
        return AuthBootstrapStatus::RecoveryRequired;
    auto target = *credentialRead.value;
    target.servicePin = verifier;
    ++target.servicePinCredentialEpoch;
    ++target.recordSequence;
    target.servicePinLockout = AuthLockoutState{};
    const auto credStatus =
        store_.writeCredentials(*credentialRead.value, target);
    if (credStatus != AuthenticationWriteStatus::Success)
        return credStatus == AuthenticationWriteStatus::CommitOutcomeUnknown
                   ? AuthBootstrapStatus::CommitOutcomeUnknown
                   : AuthBootstrapStatus::RecoveryRequired;
    auto rootProvisioned = rootLocked;
    if (rootLocked.recordSequence == std::numeric_limits<std::uint64_t>::max())
        return AuthBootstrapStatus::CommitOutcomeUnknown;
    rootProvisioned.state = AuthProvisioningState::Provisioned;
    ++rootProvisioned.recordSequence;
    return store_.writeRoot(rootLocked, rootProvisioned) ==
                   AuthenticationWriteStatus::Success
               ? AuthBootstrapStatus::BootstrapAllowed
               : AuthBootstrapStatus::RecoveryRequired;
}

std::optional<bool> AuthenticationDomain::webPasswordEnabled(
    const AuthenticationBootstrapContext& context) const {
    const std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto root = readActiveRoot(context);
    if (!root.value || root.value->state != AuthProvisioningState::Provisioned)
        return std::nullopt;
    const auto credentials = store_.readCredentials(context.storageEpoch());
    if (credentials.status != AuthenticationReadStatus::Success ||
        !credentials.value)
        return std::nullopt;
    return credentials.value->webPasswordEnabled;
}

}  // namespace fermentation
