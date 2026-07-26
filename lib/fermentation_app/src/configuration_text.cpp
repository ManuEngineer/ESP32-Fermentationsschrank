#include "configuration_text.hpp"

#include <array>

#include "configuration_limits.hpp"

namespace fermentation {
namespace {

struct Utf8Scalar {
    bool valid{false};
    std::uint32_t value{0U};
    std::size_t width{0U};
};

Utf8Scalar decodeScalar(const std::string& text, std::size_t offset) {
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first <= 0x7FU) {
        return {true, first, 1U};
    }
    std::size_t width = 0U;
    std::uint32_t value = 0U;
    std::uint32_t minimum = 0U;
    if (first >= 0xC2U && first <= 0xDFU) {
        width = 2U;
        value = first & 0x1FU;
        minimum = 0x80U;
    } else if (first >= 0xE0U && first <= 0xEFU) {
        width = 3U;
        value = first & 0x0FU;
        minimum = 0x800U;
    } else if (first >= 0xF0U && first <= 0xF4U) {
        width = 4U;
        value = first & 0x07U;
        minimum = 0x10000U;
    } else {
        return {};
    }
    if (width > text.size() - offset) {
        return {};
    }
    for (std::size_t index = 1U; index < width; ++index) {
        const auto next = static_cast<unsigned char>(text[offset + index]);
        if ((next & 0xC0U) != 0x80U) {
            return {};
        }
        value = (value << 6U) | (next & 0x3FU);
    }
    if (value < minimum || value > 0x10FFFFU ||
        (value >= 0xD800U && value <= 0xDFFFU)) {
        return {};
    }
    return {true, value, width};
}

bool isUnicodeWhitespace(std::uint32_t value) {
    if ((value >= 0x0009U && value <= 0x000DU) || value == 0x0020U ||
        value == 0x0085U || value == 0x00A0U || value == 0x1680U ||
        (value >= 0x2000U && value <= 0x200AU)) {
        return true;
    }
    constexpr std::array<std::uint32_t, 4> remaining{0x2028U, 0x2029U, 0x202FU,
                                                     0x205FU};
    for (const auto whitespace : remaining) {
        if (value == whitespace) {
            return true;
        }
    }
    return value == 0x3000U;
}

bool isControl(std::uint32_t value) {
    return value <= 0x001FU || (value >= 0x007FU && value <= 0x009FU);
}

ConfigurationTextStatus validateUnicodeText(const std::string& value,
                                            std::size_t minimumScalars,
                                            std::size_t maximumScalars,
                                            std::size_t maximumBytes,
                                            bool notes) {
    if (value.size() > maximumBytes) {
        return ConfigurationTextStatus::TooLong;
    }
    std::size_t scalarCount = 0U;
    bool hasNonWhitespace = false;
    bool firstWhitespace = false;
    bool lastWhitespace = false;
    for (std::size_t offset = 0U; offset < value.size();) {
        const auto scalar = decodeScalar(value, offset);
        if (!scalar.valid) {
            return ConfigurationTextStatus::InvalidUtf8;
        }
        const bool whitespace = isUnicodeWhitespace(scalar.value);
        if (scalarCount == 0U) {
            firstWhitespace = whitespace;
        }
        lastWhitespace = whitespace;
        hasNonWhitespace = hasNonWhitespace || !whitespace;
        if (isControl(scalar.value) && (!notes || scalar.value != 0x000AU)) {
            return ConfigurationTextStatus::ForbiddenCodePoint;
        }
        if (scalar.value == 0x2028U || scalar.value == 0x2029U) {
            return ConfigurationTextStatus::ForbiddenCodePoint;
        }
        ++scalarCount;
        if (scalarCount > maximumScalars) {
            return ConfigurationTextStatus::TooManyScalars;
        }
        offset += scalar.width;
    }
    if (scalarCount < minimumScalars) {
        return ConfigurationTextStatus::TooShort;
    }
    if (!notes && !hasNonWhitespace) {
        return ConfigurationTextStatus::OnlyWhitespace;
    }
    if (!notes && (firstWhitespace || lastWhitespace)) {
        return ConfigurationTextStatus::LeadingOrTrailingWhitespace;
    }
    return ConfigurationTextStatus::Success;
}

bool allowedTimeZoneCharacter(unsigned char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '.' || value == '_' ||
           value == '-' || value == '+';
}

}  // namespace

ConfigurationTextStatus validateLowercaseIdentifier(const std::string& value,
                                                    std::size_t minimumBytes,
                                                    std::size_t maximumBytes) {
    if (value.size() < minimumBytes) {
        return ConfigurationTextStatus::TooShort;
    }
    if (value.size() > maximumBytes) {
        return ConfigurationTextStatus::TooLong;
    }
    const auto alphaNumeric = [](unsigned char byte) {
        return (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9');
    };
    bool previousHyphen = false;
    for (const char byte : value) {
        const auto current = static_cast<unsigned char>(byte);
        if (!alphaNumeric(current) && current != '-') {
            return ConfigurationTextStatus::InvalidAsciiCharacter;
        }
        if (current == '-' && previousHyphen) {
            return ConfigurationTextStatus::InvalidHyphenPlacement;
        }
        previousHyphen = current == '-';
    }
    if (!alphaNumeric(static_cast<unsigned char>(value.front())) ||
        !alphaNumeric(static_cast<unsigned char>(value.back()))) {
        return ConfigurationTextStatus::InvalidHyphenPlacement;
    }
    return ConfigurationTextStatus::Success;
}

ConfigurationTextStatus validateTimeZoneIdentifierStructure(
    const std::string& value) {
    using namespace configuration_limits;
    if (value.size() < kMinimumTimeZoneIdBytes) {
        return ConfigurationTextStatus::TooShort;
    }
    if (value.size() > kMaximumTimeZoneIdBytes) {
        return ConfigurationTextStatus::TooLong;
    }
    if (value == "Etc/Unknown" || value.front() == '/' || value.back() == '/') {
        return ConfigurationTextStatus::InvalidTimeZoneStructure;
    }
    std::size_t componentStart = 0U;
    for (std::size_t index = 0U; index <= value.size(); ++index) {
        if (index != value.size() && value[index] != '/') {
            if (!allowedTimeZoneCharacter(
                    static_cast<unsigned char>(value[index]))) {
                return ConfigurationTextStatus::InvalidAsciiCharacter;
            }
            continue;
        }
        const std::size_t length = index - componentStart;
        if (length == 0U || (length == 1U && value[componentStart] == '.') ||
            (length == 2U && value[componentStart] == '.' &&
             value[componentStart + 1U] == '.')) {
            return ConfigurationTextStatus::InvalidTimeZoneStructure;
        }
        componentStart = index + 1U;
    }
    return ConfigurationTextStatus::Success;
}

ConfigurationTextStatus validateVisibleName(const std::string& value) {
    using namespace configuration_limits;
    return validateUnicodeText(value, kMinimumVisibleNameScalars,
                               kMaximumVisibleNameScalars,
                               kMaximumVisibleNameBytes, false);
}

ConfigurationTextStatus validateProgramNotes(const std::string& value) {
    using namespace configuration_limits;
    return validateUnicodeText(value, 0U, kMaximumNotesScalars,
                               kMaximumNotesBytes, true);
}

std::string normalizeProgramNotesForPreparation(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (std::size_t index = 0U; index < value.size(); ++index) {
        if (value[index] != '\r') {
            normalized.push_back(value[index]);
            continue;
        }
        normalized.push_back('\n');
        if (index + 1U < value.size() && value[index + 1U] == '\n') {
            ++index;
        }
    }
    return normalized;
}

}  // namespace fermentation
