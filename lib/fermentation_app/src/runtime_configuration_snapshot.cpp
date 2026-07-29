#include "runtime_configuration_snapshot.hpp"

#include <utility>

namespace fermentation {

RuntimeConfigurationSnapshot::RuntimeConfigurationSnapshot(
    device_platform::StorageEpoch storageEpoch,
    ConfigurationManifestReference manifestReference,
    UserConfigurationRevision userConfigurationRevision,
    ServiceConfigurationRevision serviceConfigurationRevision,
    ProgramCatalogRevision programCatalogRevision,
    std::shared_ptr<const UserConfiguration> userConfiguration,
    std::shared_ptr<const ServiceConfiguration> serviceConfiguration,
    std::shared_ptr<const ProgramCatalog> programCatalog,
    device_platform::PreparedTimeZone preparedTimeZone,
    std::uint64_t volatileGenerationId)
    : storageEpoch_(storageEpoch),
      manifestReference_(manifestReference),
      userConfigurationRevision_(userConfigurationRevision),
      serviceConfigurationRevision_(serviceConfigurationRevision),
      programCatalogRevision_(programCatalogRevision),
      userConfiguration_(std::move(userConfiguration)),
      serviceConfiguration_(std::move(serviceConfiguration)),
      programCatalog_(std::move(programCatalog)),
      preparedTimeZone_(std::move(preparedTimeZone)),
      volatileGenerationId_(volatileGenerationId) {}

}  // namespace fermentation
