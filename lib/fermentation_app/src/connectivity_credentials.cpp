#include "connectivity_credentials.hpp"

#include "configuration_limits.hpp"
#include "connectivity_credential_codec.hpp"
#include "configuration_storage_contract.hpp"
#include "crc32.hpp"
#include "storage_envelope.hpp"

namespace fermentation {
namespace {

device_platform::StateStoreKey credentialKey() {
    const auto result = device_platform::StateStoreKey::create(
        configuration_storage_contract::kConnectivityCredentialStoreKey);
    // The key is a compile-time contract value and is validated by the port.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    return *result.key;
}

}  // namespace

const device_platform::StateStoreKey&
ConnectivityCredentialStore::key() noexcept {
    static const auto value = credentialKey();
    return value;
}

ConnectivityCredentialLoadResult ConnectivityCredentialStore::load(
    device_platform::StorageEpoch expectedEpoch) const {
    const auto read = store_.read(
        key(),
        configuration_limits::kMaximumConnectivityCredentialEnvelopeBytes);
    if (read.status == device_platform::StateStoreReadStatus::NotFound) {
        return {ConnectivityCredentialLoadStatus::NotFound, std::nullopt};
    }
    if (read.status == device_platform::StateStoreReadStatus::CapacityError) {
        return {ConnectivityCredentialLoadStatus::CapacityError, std::nullopt};
    }
    if (read.status != device_platform::StateStoreReadStatus::Success) {
        return {ConnectivityCredentialLoadStatus::ReadError, std::nullopt};
    }
    const auto decoded = device_platform::decodeEnvelope(read.value);
    if (!decoded.envelope.has_value() ||
        decoded.envelope->recordTypeId !=
            configuration_storage_contract::kConnectivityCredentialRecordType ||
        decoded.envelope->schemaVersion !=
            configuration_storage_contract::
                kConnectivityCredentialSchemaVersion ||
        decoded.envelope->versionValue == 0U) {
        return {ConnectivityCredentialLoadStatus::InvalidRecord, std::nullopt};
    }
    if (decoded.envelope->storageEpoch != expectedEpoch) {
        return {ConnectivityCredentialLoadStatus::OtherEpoch, std::nullopt};
    }
    const auto payload = decodeConnectivityCredentialPayload(
        decoded.envelope->schemaVersion, decoded.envelope->payload);
    if (!payload.credential.has_value()) {
        return {ConnectivityCredentialLoadStatus::InvalidRecord, std::nullopt};
    }
    return {ConnectivityCredentialLoadStatus::Available,
            ConnectivityCredentialRecord{std::move(*payload.credential),
                                         decoded.envelope->storageEpoch,
                                         decoded.envelope->versionValue}};
}

ConnectivityCredentialWriteResult ConnectivityCredentialStore::write(
    const ConnectivityCredential& credential,
    device_platform::StorageEpoch epoch, std::uint64_t recordSequence) {
    if (recordSequence == 0U ||
        validateConnectivityCredential(credential) !=
            ConnectivityCredentialValidationStatus::Success) {
        return {ConnectivityCredentialWriteStatus::WriteFailure, 0U};
    }
    std::string payload;
    if (encodeConnectivityCredentialPayload(credential, payload) !=
        ConnectivityCredentialCodecStatus::Success) {
        return {ConnectivityCredentialWriteStatus::CapacityFailure, 0U};
    }
    std::string encoded;
    const auto envelopeStatus = device_platform::encodeEnvelope(
        {configuration_storage_contract::kConnectivityCredentialRecordType,
         configuration_storage_contract::kConnectivityCredentialSchemaVersion,
         epoch, recordSequence, std::nullopt, payload},
        encoded,
        configuration_limits::kMaximumConnectivityCredentialEnvelopeBytes);
    if (envelopeStatus ==
        device_platform::EnvelopeEncodeStatus::CapacityExceeded) {
        return {ConnectivityCredentialWriteStatus::CapacityFailure, 0U};
    }
    if (envelopeStatus != device_platform::EnvelopeEncodeStatus::Success) {
        return {ConnectivityCredentialWriteStatus::WriteFailure, 0U};
    }
    const auto writeStatus = store_.write(key(), encoded);
    if (writeStatus == device_platform::StateStoreWriteStatus::WriteError) {
        return {ConnectivityCredentialWriteStatus::WriteFailure, 0U};
    }
    if (writeStatus == device_platform::StateStoreWriteStatus::CapacityError) {
        return {ConnectivityCredentialWriteStatus::CapacityFailure, 0U};
    }
    const auto read = store_.read(
        key(),
        configuration_limits::kMaximumConnectivityCredentialEnvelopeBytes);
    if (read.status == device_platform::StateStoreReadStatus::Success &&
        read.value == encoded) {
        return {ConnectivityCredentialWriteStatus::Committed, recordSequence};
    }
    if (writeStatus ==
        device_platform::StateStoreWriteStatus::CommitOutcomeUnknown) {
        return {ConnectivityCredentialWriteStatus::Indeterminate, 0U};
    }
    return {read.status == device_platform::StateStoreReadStatus::CapacityError
                ? ConnectivityCredentialWriteStatus::CapacityFailure
                : ConnectivityCredentialWriteStatus::WriteFailure,
            0U};
}

}  // namespace fermentation
