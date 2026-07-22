#include "mock_user_notification_sink.hpp"

namespace device_platform_test_support {

void MockUserNotificationSink::notify(
    device_platform::NotificationSeverity severity,
    const std::string& message) {
    if (notifications_.size() >= kMaxEntries) {
        notifications_.erase(notifications_.begin());
    }
    notifications_.push_back(RecordedNotification{severity, message});
}

const std::vector<RecordedNotification>&
MockUserNotificationSink::notifications() const {
    return notifications_;
}

}  // namespace device_platform_test_support
