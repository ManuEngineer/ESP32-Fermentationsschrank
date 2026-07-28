#pragma once

#include <cstdint>
#include <memory>

#include "configuration_graph.hpp"
#include "time_zone_resolver.hpp"

namespace fermentation {

class ConfigurationService;

class RuntimeConfigurationSnapshot {
   public:
    [[nodiscard]] device_platform::StorageEpoch storageEpoch() const {
        return storageEpoch_;
    }
    [[nodiscard]] const ConfigurationManifestReference& manifestReference()
        const {
        return manifestReference_;
    }
    [[nodiscard]] UserConfigurationRevision userConfigurationRevision() const {
        return userConfigurationRevision_;
    }
    [[nodiscard]] ServiceConfigurationRevision serviceConfigurationRevision()
        const {
        return serviceConfigurationRevision_;
    }
    [[nodiscard]] ProgramCatalogRevision programCatalogRevision() const {
        return programCatalogRevision_;
    }
    [[nodiscard]] const UserConfiguration& userConfiguration() const {
        return *userConfiguration_;
    }
    [[nodiscard]] const ServiceConfiguration& serviceConfiguration() const {
        return *serviceConfiguration_;
    }
    [[nodiscard]] const ProgramCatalog& programCatalog() const {
        return *programCatalog_;
    }
    [[nodiscard]] const device_platform::PreparedTimeZone& preparedTimeZone()
        const {
        return preparedTimeZone_;
    }
    [[nodiscard]] std::uint64_t volatileGenerationId() const {
        return volatileGenerationId_;
    }

   private:
    friend class ConfigurationService;
    RuntimeConfigurationSnapshot(
        device_platform::StorageEpoch storageEpoch,
        ConfigurationManifestReference manifestReference,
        UserConfigurationRevision userConfigurationRevision,
        ServiceConfigurationRevision serviceConfigurationRevision,
        ProgramCatalogRevision programCatalogRevision,
        std::shared_ptr<const UserConfiguration> userConfiguration,
        std::shared_ptr<const ServiceConfiguration> serviceConfiguration,
        std::shared_ptr<const ProgramCatalog> programCatalog,
        device_platform::PreparedTimeZone preparedTimeZone,
        std::uint64_t volatileGenerationId);

    device_platform::StorageEpoch storageEpoch_;
    ConfigurationManifestReference manifestReference_;
    UserConfigurationRevision userConfigurationRevision_;
    ServiceConfigurationRevision serviceConfigurationRevision_;
    ProgramCatalogRevision programCatalogRevision_;
    std::shared_ptr<const UserConfiguration> userConfiguration_;
    std::shared_ptr<const ServiceConfiguration> serviceConfiguration_;
    std::shared_ptr<const ProgramCatalog> programCatalog_;
    device_platform::PreparedTimeZone preparedTimeZone_;
    std::uint64_t volatileGenerationId_{0U};
};

}  // namespace fermentation
