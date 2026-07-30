#pragma once

#include <array>
#include <cstdint>

#include "storage_types.hpp"

namespace fermentation::configuration_storage_contract {

// Stabile Anwendungs-Recordtypen. Wert 0 bleibt gemaess Envelopevertrag
// reserviert. Diese Datei definiert weder Storezugriffe noch Slotwahl.
inline constexpr device_platform::RecordTypeId kUserConfigurationRecordType{1U};
inline constexpr device_platform::RecordTypeId kServiceConfigurationRecordType{
    2U};
inline constexpr device_platform::RecordTypeId kProgramCatalogRecordType{3U};
inline constexpr device_platform::RecordTypeId kConfigurationManifestRecordType{
    4U};
inline constexpr device_platform::RecordTypeId kConfigurationRootRecordType{5U};
inline constexpr device_platform::RecordTypeId
    kConfigurationBootstrapRecordType{6U};

// ADR-016-konforme kurze NVS-Schluessel. Es handelt sich nur um die stabile
// Namenskonvention fuer die vier Dokumentplaetze; Lesen, Schreiben, Rotation
// und Referenzschutz folgen nicht in Issue #55.
inline constexpr std::array<const char*, 4> kUserConfigurationSlotKeys{
    "uc0", "uc1", "uc2", "uc3"};
inline constexpr std::array<const char*, 4> kServiceConfigurationSlotKeys{
    "sc0", "sc1", "sc2", "sc3"};
inline constexpr std::array<const char*, 4> kProgramCatalogSlotKeys{
    "pc0", "pc1", "pc2", "pc3"};
inline constexpr std::array<const char*, 3> kConfigurationManifestSlotKeys{
    "cm0", "cm1", "cm2"};
inline constexpr std::array<const char*, 2> kConfigurationRootSlotKeys{"cr0",
                                                                       "cr1"};
inline constexpr std::array<const char*, 2> kConfigurationBootstrapSlotKeys{
    "cb0", "cb1"};

}  // namespace fermentation::configuration_storage_contract
