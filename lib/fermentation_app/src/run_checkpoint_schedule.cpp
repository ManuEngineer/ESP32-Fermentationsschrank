#include "run_checkpoint_schedule.hpp"

#include <limits>

namespace fermentation {

RunCheckpointSchedule::RunCheckpointSchedule(std::uint16_t intervalMinutes)
    : intervalMinutes_(intervalMinutes) {}

RunCheckpointScheduleStatus RunCheckpointSchedule::confirm(
    std::uint64_t monotonicMillis) {
    if (intervalMinutes_ < kMinimumRunCheckpointIntervalMinutes ||
        intervalMinutes_ > kMaximumRunCheckpointIntervalMinutes) {
        return RunCheckpointScheduleStatus::InvalidInterval;
    }
    if (lastConfirmedMillis_.has_value() &&
        monotonicMillis < *lastConfirmedMillis_) {
        return RunCheckpointScheduleStatus::TimeWentBackwards;
    }
    lastConfirmedMillis_ = monotonicMillis;
    return RunCheckpointScheduleStatus::Success;
}

RunCheckpointScheduleStatus RunCheckpointSchedule::due(
    std::uint64_t monotonicMillis) const {
    if (intervalMinutes_ < kMinimumRunCheckpointIntervalMinutes ||
        intervalMinutes_ > kMaximumRunCheckpointIntervalMinutes) {
        return RunCheckpointScheduleStatus::InvalidInterval;
    }
    if (!lastConfirmedMillis_.has_value()) return RunCheckpointScheduleStatus::NotDue;
    if (monotonicMillis < *lastConfirmedMillis_) return RunCheckpointScheduleStatus::TimeWentBackwards;
    const auto minutes = static_cast<std::uint64_t>(intervalMinutes_);
    if (minutes > std::numeric_limits<std::uint64_t>::max() / 60000U) {
        return RunCheckpointScheduleStatus::InvalidInterval;
    }
    const auto interval = minutes * 60000U;
    return monotonicMillis - *lastConfirmedMillis_ >= interval
               ? RunCheckpointScheduleStatus::Success
               : RunCheckpointScheduleStatus::NotDue;
}

void RunCheckpointSchedule::reset() { lastConfirmedMillis_.reset(); }

}  // namespace fermentation
