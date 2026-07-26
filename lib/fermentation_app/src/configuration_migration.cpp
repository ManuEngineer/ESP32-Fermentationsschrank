#include "configuration_migration.hpp"

namespace fermentation {

CopyMigrationResult<ProgramCatalog>
migrateProgramCatalogDocumentsToCurrentSchema(const ProgramCatalog& source) {
    ProgramCatalog candidate;
    candidate.programs.reserve(source.programs.size());
    bool migratedAny = false;
    for (const auto& document : source.programs) {
        auto migration = migrateProgramToCurrentSchema(document);
        if ((migration.status != MigrationStatus::NotRequired &&
             migration.status != MigrationStatus::Migrated) ||
            !migration.document.has_value()) {
            return {CopyMigrationStatus::StepFailed, std::nullopt};
        }
        migratedAny =
            migratedAny || migration.status == MigrationStatus::Migrated;
        candidate.programs.push_back(std::move(*migration.document));
    }
    return {migratedAny ? CopyMigrationStatus::Migrated
                        : CopyMigrationStatus::NotRequired,
            std::move(candidate)};
}

}  // namespace fermentation
