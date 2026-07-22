#pragma once

#include <cstddef>
#include <vector>

#include "user_notification_sink.hpp"

namespace device_platform {

struct RecordedNotification {
    NotificationSeverity severity;
    std::string message;
};

// Zeichnet Benachrichtigungen fuer native Tests auf, fest begrenzt auf
// `kMaxEntries` (aelteste Eintraege werden verworfen).
class MockUserNotificationSink final : public IUserNotificationSink {
   public:
    static constexpr std::size_t kMaxEntries = 256;

    void notify(NotificationSeverity severity,
                const std::string& message) override;

    [[nodiscard]] const std::vector<RecordedNotification>& notifications()
        const;

   private:
    std::vector<RecordedNotification> notifications_;
};

}  // namespace device_platform
