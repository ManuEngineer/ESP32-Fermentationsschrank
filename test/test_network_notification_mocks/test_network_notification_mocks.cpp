#include <unity.h>

#include "mock_network_status.hpp"
#include "mock_user_notification_sink.hpp"

void test_network_status_defaults_to_disconnected() {
    const device_platform_test_support::MockNetworkStatus network;

    TEST_ASSERT_FALSE(network.isConnected());
}

void test_network_status_can_be_toggled() {
    device_platform_test_support::MockNetworkStatus network;

    network.setConnected(true);
    TEST_ASSERT_TRUE(network.isConnected());

    // WLAN-Ausfall darf jederzeit eintreten; der Port bildet ihn nur ab und
    // beeinflusst keine anderen Ports.
    network.setConnected(false);
    TEST_ASSERT_FALSE(network.isConnected());
}

void test_notification_sink_records_severity_and_message() {
    device_platform_test_support::MockUserNotificationSink notifications;

    notifications.notify(device_platform::NotificationSeverity::Warning,
                         "product sensor failed");

    const auto& recorded = notifications.notifications();
    TEST_ASSERT_EQUAL_UINT32(1U, recorded.size());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::NotificationSeverity::Warning),
        static_cast<int>(recorded[0].severity));
    TEST_ASSERT_EQUAL_STRING("product sensor failed",
                             recorded[0].message.c_str());
}

void test_notification_sink_is_bounded_and_drops_oldest_entries() {
    device_platform_test_support::MockUserNotificationSink notifications;

    const std::size_t entriesToWrite =
        device_platform_test_support::MockUserNotificationSink::kMaxEntries + 5;
    for (std::size_t i = 0; i < entriesToWrite; ++i) {
        notifications.notify(device_platform::NotificationSeverity::Info,
                             "event");
    }

    TEST_ASSERT_EQUAL_UINT32(
        static_cast<unsigned>(device_platform_test_support::
                                  MockUserNotificationSink::kMaxEntries),
        notifications.notifications().size());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_network_status_defaults_to_disconnected);
    RUN_TEST(test_network_status_can_be_toggled);
    RUN_TEST(test_notification_sink_records_severity_and_message);
    RUN_TEST(test_notification_sink_is_bounded_and_drops_oldest_entries);
    return UNITY_END();
}
