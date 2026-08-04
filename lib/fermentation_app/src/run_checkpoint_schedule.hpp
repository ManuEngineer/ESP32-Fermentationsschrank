#pragma once

#include <cstdint>
#include <optional>

#include "run_persistence_contract.hpp"

namespace fermentation {

enum class RunCheckpointScheduleStatus : std::uint8_t {
    Success,
    NotDue,
    TimeWentBackwards,
    InvalidInterval,
};

class RunCheckpointSchedule {
   public:
    explicit RunCheckpointSchedule(
        std::uint16_t intervalMinutes = kDefaultRunCheckpointIntervalMinutes);

    [[nodiscard]] std::uint16_t intervalMinutes() const { return intervalMinutes_; }
    [[nodiscard]] RunCheckpointScheduleStatus confirm(
        std::uint64_t monotonicMillis);
    [[nodiscard]] RunCheckpointScheduleStatus due(
        std::uint64_t monotonicMillis) const;
    void reset();

   private:
    std::uint16_t intervalMinutes_;
    std::optional<std::uint64_t> lastConfirmedMillis_;
};

}  // namespace fermentation
