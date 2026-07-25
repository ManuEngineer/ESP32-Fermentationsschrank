#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include "configuration_documents.hpp"

namespace fermentation {

enum class CopyMigrationStatus : std::uint8_t {
    NotRequired,
    Migrated,
    UnsupportedSourceSchema,
    StepFailed,
};

template <typename Document>
struct CopyMigrationResult {
    CopyMigrationStatus status{CopyMigrationStatus::UnsupportedSourceSchema};
    std::optional<Document> document;
};

// Generischer Copy-Ablauf fuer fachliche Migrationen. Jeder Schritt erhaelt
// eine unveraenderliche Quelle und liefert eine neue Kopie. Bei einem Fehler
// wird weder die Quelle noch ein Teilergebnis veroeffentlicht.
template <typename Document, typename Step>
[[nodiscard]] CopyMigrationResult<Document> migrateDocumentCopy(
    const Document& source, std::uint32_t sourceSchema,
    std::uint32_t targetSchema, Step step) {
    if (sourceSchema > targetSchema || sourceSchema == 0U) {
        return {CopyMigrationStatus::UnsupportedSourceSchema, std::nullopt};
    }
    if (sourceSchema == targetSchema) {
        return {CopyMigrationStatus::NotRequired, source};
    }
    Document candidate = source;
    for (std::uint32_t schema = sourceSchema; schema < targetSchema; ++schema) {
        auto next = step(candidate, schema);
        if (!next.has_value()) {
            return {CopyMigrationStatus::StepFailed, std::nullopt};
        }
        candidate = std::move(*next);
    }
    return {CopyMigrationStatus::Migrated, std::move(candidate)};
}

[[nodiscard]] CopyMigrationResult<ProgramCatalog>
migrateProgramCatalogDocumentsToCurrentSchema(const ProgramCatalog& source);

}  // namespace fermentation
