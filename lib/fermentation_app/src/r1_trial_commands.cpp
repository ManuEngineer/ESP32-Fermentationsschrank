#include "r1_trial_commands.hpp"

#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace fermentation {

namespace {

constexpr std::size_t kMaxTokens = 14U;
constexpr std::size_t kMaxTokenLength = 31U;

struct Tokens {
    std::array<std::string_view, kMaxTokens> items{};
    std::size_t count{0U};
    bool overflow{false};
};

Tokens tokenize(std::string_view line) {
    Tokens tokens;
    std::size_t i = 0U;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= line.size()) break;
        const std::size_t start = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') ++i;
        if (tokens.count == kMaxTokens) {
            tokens.overflow = true;
            return tokens;
        }
        tokens.items[tokens.count++] = line.substr(start, i - start);
    }
    return tokens;
}

bool parseUnsigned(std::string_view token, std::uint64_t& out) {
    if (token.empty() || token.size() > 18U) return false;
    std::uint64_t value = 0U;
    for (const char c : token) {
        if (c < '0' || c > '9') return false;
        value = value * 10U + static_cast<std::uint64_t>(c - '0');
    }
    out = value;
    return true;
}

bool parseDouble(std::string_view token, double& out) {
    if (token.empty() || token.size() > kMaxTokenLength) return false;
    // Decimal only: digits, one optional sign, one optional '.'; no exponent,
    // no hex, no "inf"/"nan".
    std::size_t digits = 0U;
    for (std::size_t i = 0U; i < token.size(); ++i) {
        const char c = token[i];
        if (c >= '0' && c <= '9') {
            ++digits;
        } else if (c == '.') {
            continue;
        } else if ((c == '-' || c == '+') && i == 0U) {
            continue;
        } else {
            return false;
        }
    }
    if (digits == 0U) return false;
    char buffer[kMaxTokenLength + 1U] = {};
    for (std::size_t i = 0U; i < token.size(); ++i) buffer[i] = token[i];
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(buffer, &end);
    if (errno != 0 || end == nullptr || *end != '\0' || !std::isfinite(value)) {
        return false;
    }
    out = value;
    return true;
}

bool is(std::string_view token, std::string_view expected) {
    return token == expected;
}

}  // namespace

R1TrialCommand parseR1TrialCommand(std::string_view line) {
    R1TrialCommand command;
    const auto tokens = tokenize(line);
    if (tokens.overflow || tokens.count == 0U) return command;
    const auto& t = tokens.items;

    if (is(t[0], "trial") && tokens.count == 2U && is(t[1], "status")) {
        command.kind = R1TrialCommandKind::Status;
        return command;
    }
    if (is(t[0], "planner")) {
        if (tokens.count == 2U && is(t[1], "show")) {
            command.kind = R1TrialCommandKind::PlannerShow;
        } else if (tokens.count == 2U && is(t[1], "clear")) {
            command.kind = R1TrialCommandKind::PlannerClear;
        } else if (tokens.count == 12U && is(t[1], "set")) {
            auto& p = command.planner;
            double threshold = 0.0;
            if (parseUnsigned(t[2], p.switchingWindowMillis) &&
                parseUnsigned(t[3], p.minimumOnMillis) &&
                parseUnsigned(t[4], p.minimumOffMillis) &&
                parseUnsigned(t[5], p.polarityDeadTimeMillis) &&
                parseUnsigned(t[6], p.pulseAccumulatorCapMillis) &&
                parseDouble(t[7], threshold) &&
                parseUnsigned(t[8],
                              p.counterDirectionConfirmationDurationMillis) &&
                parseUnsigned(t[9], p.requestWatchdogMillis) &&
                parseUnsigned(t[10], p.outerFanPostRunMillis) &&
                parseUnsigned(t[11], p.innerFanPostRunMillis)) {
                p.counterDirectionConfirmationQuoteThreshold = threshold;
                command.kind = R1TrialCommandKind::PlannerSet;
            } else {
                command.planner = ActuatorPlannerParameters{};
            }
        }
        return command;
    }
    if (is(t[0], "thermal")) {
        if (tokens.count == 2U && is(t[1], "show")) {
            command.kind = R1TrialCommandKind::ThermalShow;
        } else if (tokens.count == 2U && is(t[1], "clear")) {
            command.kind = R1TrialCommandKind::ThermalClear;
        } else if (tokens.count == 14U && is(t[1], "set")) {
            auto& l = command.thermal;
            if (parseDouble(t[2], l.airEmergencyLowerCelsius) &&
                parseDouble(t[3], l.airInterventionLowerCelsius) &&
                parseDouble(t[4], l.airInterventionUpperCelsius) &&
                parseDouble(t[5], l.airEmergencyUpperCelsius) &&
                parseDouble(t[6], l.coolingEmergencyLowerCelsius) &&
                parseDouble(t[7], l.coolingInterventionLowerCelsius) &&
                parseDouble(t[8], l.coolingInterventionUpperCelsius) &&
                parseDouble(t[9], l.coolingEmergencyUpperCelsius) &&
                parseDouble(t[10], l.coolingMaxRiseCelsiusPerMinute) &&
                parseUnsigned(t[11], l.trendWindowMillis) &&
                parseUnsigned(t[12], l.maxSampleAgeMillis) &&
                parseUnsigned(t[13], l.invalidLatchMillis)) {
                command.kind = R1TrialCommandKind::ThermalSet;
            } else {
                command.thermal = ThermalSafetyLimits{};
            }
        }
        return command;
    }
    return command;
}

}  // namespace fermentation
