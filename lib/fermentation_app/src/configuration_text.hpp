#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace fermentation {

enum class ConfigurationTextStatus : std::uint8_t {
    Success,
    TooShort,
    TooLong,
    InvalidAsciiCharacter,
    InvalidHyphenPlacement,
    InvalidUtf8,
    TooManyScalars,
    ForbiddenCodePoint,
    OnlyWhitespace,
    LeadingOrTrailingWhitespace,
    InvalidTimeZoneStructure,
};

[[nodiscard]] ConfigurationTextStatus validateLowercaseIdentifier(
    const std::string& value, std::size_t minimumBytes,
    std::size_t maximumBytes);

[[nodiscard]] ConfigurationTextStatus validateTimeZoneIdentifierStructure(
    const std::string& value);

[[nodiscard]] ConfigurationTextStatus validateVisibleName(
    const std::string& value);

[[nodiscard]] ConfigurationTextStatus validateProgramNotes(
    const std::string& value);

// Einziger fuer Schema 1 erlaubter Normalisierungspunkt. Payloaddecoder und
// Encoder rufen diese Funktion nie implizit auf.
[[nodiscard]] std::string normalizeProgramNotesForPreparation(
    const std::string& value);

}  // namespace fermentation
