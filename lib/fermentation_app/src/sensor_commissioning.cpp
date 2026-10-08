#include "sensor_commissioning.hpp"

#include <algorithm>

namespace fermentation {
namespace {

bool equalOffsets(const device_platform::SensorOffset& left,
                  const device_platform::SensorOffset& right) {
    return left.celsius() == right.celsius();
}

std::optional<device_platform::OneWireRom> parseRomHex(std::string_view text) {
    if (text.size() != 16U) return std::nullopt;
    device_platform::OneWireRom value = 0U;
    for (const char c : text) {
        value <<= 4U;
        if (c >= '0' && c <= '9') {
            value |= static_cast<device_platform::OneWireRom>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            value |= static_cast<device_platform::OneWireRom>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            value |= static_cast<device_platform::OneWireRom>(c - 'A' + 10);
        } else {
            return std::nullopt;
        }
    }
    if (value == 0U) return std::nullopt;
    return value;
}

std::optional<int32_t> parseInt32(std::string_view text) {
    if (text.empty() || text.size() > 7U) return std::nullopt;
    std::size_t index = 0U;
    bool negative = false;
    if (text[0] == '-' || text[0] == '+') {
        negative = text[0] == '-';
        index = 1U;
        if (text.size() == 1U) return std::nullopt;
    }
    int32_t value = 0;
    for (; index < text.size(); ++index) {
        if (text[index] < '0' || text[index] > '9') return std::nullopt;
        value = value * 10 + (text[index] - '0');
    }
    return negative ? -value : value;
}

std::vector<std::string_view> splitWords(std::string_view line) {
    std::vector<std::string_view> words;
    std::size_t index = 0U;
    while (index < line.size()) {
        while (index < line.size() &&
               (line[index] == ' ' || line[index] == '\t' ||
                line[index] == '\r' || line[index] == '\n')) {
            ++index;
        }
        const std::size_t begin = index;
        while (index < line.size() && line[index] != ' ' &&
               line[index] != '\t' && line[index] != '\r' &&
               line[index] != '\n') {
            ++index;
        }
        if (index > begin) words.push_back(line.substr(begin, index - begin));
    }
    return words;
}

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.substr(0U, prefix.size()) == prefix;
}

}  // namespace

std::optional<device_platform::SensorCalibration>
SensorCommissioningRecord::calibrationFor(
    device_platform::OneWireRom rom) const {
    const auto make = [rom](const SensorRomOffset& entry)
        -> std::optional<device_platform::SensorCalibration> {
        const auto identity = device_platform::SensorIdentity::create(rom);
        if (!identity.identity.has_value()) return std::nullopt;
        return device_platform::SensorCalibration(*identity.identity,
                                                  entry.offset);
    };
    if (chamberAir.has_value() && chamberAir->rom == rom)
        return make(*chamberAir);
    if (heatsink.has_value() && heatsink->rom == rom) return make(*heatsink);
    for (const auto& probe : productProbes) {
        if (probe.rom == rom) return make(probe);
    }
    return std::nullopt;
}

SensorCommissioningStatus validateSensorCommissioning(
    const SensorCommissioningRecord& record) {
    if (!record.chamberAir.has_value() || !record.heatsink.has_value()) {
        return SensorCommissioningStatus::MissingFixedRole;
    }
    if (record.productProbes.size() > kMaximumKnownProductProbes) {
        return SensorCommissioningStatus::TooManyProductProbes;
    }
    std::vector<device_platform::OneWireRom> roms;
    roms.push_back(record.chamberAir->rom);
    roms.push_back(record.heatsink->rom);
    for (const auto& probe : record.productProbes) roms.push_back(probe.rom);
    for (const auto rom : roms) {
        if (rom == 0U) return SensorCommissioningStatus::ZeroRom;
    }
    std::sort(roms.begin(), roms.end());
    if (std::adjacent_find(roms.begin(), roms.end()) != roms.end()) {
        return SensorCommissioningStatus::DuplicateRom;
    }
    return SensorCommissioningStatus::Success;
}

bool operator==(const SensorRomOffset& left, const SensorRomOffset& right) {
    return left.rom == right.rom && equalOffsets(left.offset, right.offset);
}

bool operator==(const SensorCommissioningRecord& left,
                const SensorCommissioningRecord& right) {
    return left.chamberAir == right.chamberAir &&
           left.heatsink == right.heatsink &&
           left.productProbes == right.productProbes;
}

std::array<std::optional<device_platform::Ds18b20ChannelBinding>,
           device_platform::Ds18b20SamplingEngine::kExpectedRomChannelCount>
toChannelBindings(const std::optional<SensorCommissioningRecord>& record) {
    std::array<std::optional<device_platform::Ds18b20ChannelBinding>,
               device_platform::Ds18b20SamplingEngine::kExpectedRomChannelCount>
        bindings{};
    if (!record.has_value() || validateSensorCommissioning(*record) !=
                                   SensorCommissioningStatus::Success) {
        return bindings;
    }
    bindings[0] = device_platform::Ds18b20ChannelBinding{
        kChamberAirChannel, record->chamberAir->rom};
    bindings[1] = device_platform::Ds18b20ChannelBinding{kHeatsinkChannel,
                                                         record->heatsink->rom};
    return bindings;
}

SensorCommissioningCommand parseSensorCommissioningCommand(
    std::string_view line) {
    SensorCommissioningCommand command;
    const auto words = splitWords(line);
    if (words.empty() || words[0] != "ds18b20") return command;
    if (words.size() == 2U && words[1] == "report") {
        command.kind = SensorCommissioningCommandKind::Report;
        return command;
    }
    if (words.size() == 2U && words[1] == "clear") {
        command.kind = SensorCommissioningCommandKind::Clear;
        return command;
    }
    if (words.size() == 4U && words[1] == "bind") {
        // ds18b20 bind air=<16 Hex> heatsink=<16 Hex>
        if (!startsWith(words[2], "air=") ||
            !startsWith(words[3], "heatsink=")) {
            return command;
        }
        const auto air = parseRomHex(words[2].substr(4U));
        const auto heatsink = parseRomHex(words[3].substr(9U));
        if (!air.has_value() || !heatsink.has_value()) return command;
        command.kind = SensorCommissioningCommandKind::Bind;
        command.chamberAirRom = air;
        command.heatsinkRom = heatsink;
        return command;
    }
    if (words.size() == 4U && words[1] == "offset") {
        // ds18b20 offset air|heatsink|product=<16 Hex> <ganze Milli-Grad>
        const auto milli = parseInt32(words[3]);
        if (!milli.has_value()) return command;
        if (words[2] == "air") {
            command.target = SensorCommissioningTarget::ChamberAir;
        } else if (words[2] == "heatsink") {
            command.target = SensorCommissioningTarget::Heatsink;
        } else if (startsWith(words[2], "product=")) {
            const auto rom = parseRomHex(words[2].substr(8U));
            if (!rom.has_value()) return command;
            command.target = SensorCommissioningTarget::Product;
            command.offsetRom = rom;
        } else {
            return command;
        }
        command.kind = SensorCommissioningCommandKind::Offset;
        command.offsetMilliCelsius = milli;
        return command;
    }
    return command;
}

SensorCommissioningApplyResult applySensorCommissioningCommand(
    const std::optional<SensorCommissioningRecord>& current,
    const SensorCommissioningCommand& command) {
    SensorCommissioningApplyResult result;
    const auto offsetOf =
        [](int32_t milli) -> std::optional<device_platform::SensorOffset> {
        return device_platform::SensorOffset::create(
                   static_cast<double>(milli) / 1000.0)
            .offset;
    };
    switch (command.kind) {
        case SensorCommissioningCommandKind::Clear:
            result.accepted = true;
            result.hasRecord = false;
            return result;
        case SensorCommissioningCommandKind::Bind: {
            // Bindung ersetzt die beiden festen ROMs; Offsets bleiben, wenn das
            // ROM bereits bekannt war, sonst 0 (explizit gesetzt).
            SensorCommissioningRecord record =
                current.has_value() ? *current : SensorCommissioningRecord{};
            const auto zero = offsetOf(0);
            const auto keep = [&](device_platform::OneWireRom rom)
                -> device_platform::SensorOffset {
                if (current.has_value()) {
                    const auto cal = current->calibrationFor(rom);
                    if (cal.has_value()) return cal->offset();
                }
                return *zero;
            };
            record.chamberAir = SensorRomOffset{*command.chamberAirRom,
                                                keep(*command.chamberAirRom)};
            record.heatsink = SensorRomOffset{*command.heatsinkRom,
                                              keep(*command.heatsinkRom)};
            if (validateSensorCommissioning(record) !=
                SensorCommissioningStatus::Success) {
                return result;
            }
            result.accepted = true;
            result.hasRecord = true;
            result.record = std::move(record);
            return result;
        }
        case SensorCommissioningCommandKind::Offset: {
            if (!current.has_value() ||
                validateSensorCommissioning(*current) !=
                    SensorCommissioningStatus::Success ||
                !command.offsetMilliCelsius.has_value()) {
                return result;
            }
            const auto offset = offsetOf(*command.offsetMilliCelsius);
            if (!offset.has_value()) return result;
            SensorCommissioningRecord record = *current;
            switch (command.target) {
                case SensorCommissioningTarget::ChamberAir:
                    record.chamberAir->offset = *offset;
                    break;
                case SensorCommissioningTarget::Heatsink:
                    record.heatsink->offset = *offset;
                    break;
                case SensorCommissioningTarget::Product: {
                    auto found =
                        std::find_if(record.productProbes.begin(),
                                     record.productProbes.end(),
                                     [&](const SensorRomOffset& probe) {
                                         return probe.rom == *command.offsetRom;
                                     });
                    if (found != record.productProbes.end()) {
                        found->offset = *offset;
                    } else {
                        record.productProbes.push_back(
                            SensorRomOffset{*command.offsetRom, *offset});
                    }
                    break;
                }
            }
            if (validateSensorCommissioning(record) !=
                SensorCommissioningStatus::Success) {
                return result;
            }
            result.accepted = true;
            result.hasRecord = true;
            result.record = std::move(record);
            return result;
        }
        case SensorCommissioningCommandKind::Report:
        case SensorCommissioningCommandKind::Rejected:
            return result;
    }
    return result;
}

}  // namespace fermentation
