#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "configuration_graph.hpp"
#include "state_store.hpp"
#include "time_zone_resolver.hpp"

namespace fermentation {

enum class ConfigurationGraphLoadStatus : std::uint8_t {
    ConfigurationGraphAvailable,
    ConfigurationGraphUnavailable,
    ConfigurationGraphIntegrityFailure,
    RootReadError,
    RootCapacityError,
    RecordReadError,
    RecordCapacityError,
};

struct ConfigurationGraphDiagnostics {
    std::uint32_t notFoundSlots{0U};
    std::uint32_t invalidCandidates{0U};
    std::uint32_t exactDuplicateRecords{0U};
    std::uint32_t skippedHigherRoots{0U};
    bool fallbackUsed{false};
    bool unusableFallback{false};
    bool identicalRootTie{false};
};

struct ConfigurationGraphLoadResult {
    ConfigurationGraphLoadStatus status{
        ConfigurationGraphLoadStatus::ConfigurationGraphUnavailable};
    std::optional<LoadedConfigurationGraph> graph;
    ConfigurationGraphDiagnostics diagnostics;
};

enum class ConfigurationScanStatus : std::uint8_t {
    Success,
    ReadError,
    CapacityError,
    UnsupportedNewerConfigurationSchema,
    ConfigurationGraphIntegrityFailure,
    HighWaterOverflow,
    ActiveBasisMismatch,
};

struct ConfigurationHighWaterMarks {
    UserConfigurationRevision userConfiguration;
    ServiceConfigurationRevision serviceConfiguration;
    ProgramCatalogRevision programCatalog;
    ConfigurationManifestGeneration manifest;
    ConfigurationRootSequence root;
};

struct ConfigurationValidationScanResult {
    ConfigurationScanStatus status{ConfigurationScanStatus::ReadError};
    ConfigurationHighWaterMarks highWater;
    ConfigurationGraphDiagnostics diagnostics;
};

struct ConfigurationChangeMask {
    bool userConfiguration{false};
    bool serviceConfiguration{false};
    bool programCatalog{false};
};

enum class ConfigurationSlotPlanStatus : std::uint8_t {
    Success,
    NoUnreferencedSlotAvailable,
    HighWaterOverflow,
    InvalidCanonicalGraph,
};

struct ConfigurationSlotPlan {
    std::optional<device_platform::SlotId> userConfigurationSlot;
    std::optional<device_platform::SlotId> serviceConfigurationSlot;
    std::optional<device_platform::SlotId> programCatalogSlot;
    device_platform::SlotId manifestSlot;
    device_platform::SlotId rootSlot;
    std::optional<UserConfigurationRevision> userConfigurationRevision;
    std::optional<ServiceConfigurationRevision> serviceConfigurationRevision;
    std::optional<ProgramCatalogRevision> programCatalogRevision;
    ConfigurationManifestGeneration manifestGeneration;
    ConfigurationRootSequence rootSequence;
};

struct ConfigurationSlotPlanResult {
    ConfigurationSlotPlanStatus status{
        ConfigurationSlotPlanStatus::InvalidCanonicalGraph};
    std::optional<ConfigurationSlotPlan> plan;
};

class ConfigurationGraphStore {
   public:
    ConfigurationGraphStore(
        device_platform::IStateStore& store,
        const device_platform::ITimeZoneResolver& timeZoneResolver)
        : store_(store), timeZoneResolver_(timeZoneResolver) {}

    [[nodiscard]] ConfigurationGraphLoadResult loadCanonicalGraph(
        device_platform::StorageEpoch storageEpoch) const;

    [[nodiscard]] ConfigurationValidationScanResult validationScan(
        const LoadedConfigurationGraph& expectedActive) const;

    [[nodiscard]] ConfigurationSlotPlanResult planSlots(
        const LoadedConfigurationGraph& current,
        const ConfigurationHighWaterMarks& highWater,
        ConfigurationChangeMask changes) const;

   private:
    device_platform::IStateStore& store_;
    const device_platform::ITimeZoneResolver& timeZoneResolver_;
};

}  // namespace fermentation
