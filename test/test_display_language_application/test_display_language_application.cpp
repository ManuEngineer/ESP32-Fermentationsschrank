#include <unity.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_commands.hpp"
#include "mock_time_zone_resolver.hpp"
#include "simulated_persistent_state_store.hpp"
#include "virtual_time_source.hpp"

namespace fermentation {

class FermentationApplicationTestAccess {
   public:
    static ApplicationCallSerializer::Guard enter(
        FermentationApplication& application) {
        return application.applicationCallSerializer_.enter();
    }
};

}  // namespace fermentation

namespace {

using namespace fermentation;

struct Fixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    FermentationApplication application;

    Fixture() {
        TEST_ASSERT_TRUE(platform.begin({true}));
        timeSource.setUnixTimeSeconds(1'700'000'000LL);
        TEST_ASSERT_TRUE(
            application.begin(platform, store, timeZoneResolver, timeSource));
    }

    std::optional<UserConfigurationRevision> revision() {
        return application.uiSnapshot()
            .revisions.expectedUserConfigurationRevision;
    }
    std::string language() {
        const auto source = application.uiPresentationSource();
        TEST_ASSERT_TRUE(source.has_value());
        return source->displayLocale.value();
    }
};

void test_commit_activates_the_language_and_changes_the_revision() {
    Fixture fixture;
    const auto before = fixture.revision();
    TEST_ASSERT_TRUE(before.has_value());
    TEST_ASSERT_TRUE(fixture.language() != "es");

    const auto result = fixture.application.applyDisplayLanguage("es", before);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationPreviewStatus::Success),
                          static_cast<int>(result.preview));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(result.commit));
    TEST_ASSERT_EQUAL_STRING("es", fixture.language().c_str());
    TEST_ASSERT_TRUE(fixture.revision() != before);
}

void test_same_language_is_no_change_and_keeps_the_revision() {
    Fixture fixture;
    const auto current = fixture.language();
    const auto before = fixture.revision();
    const auto result =
        fixture.application.applyDisplayLanguage(current, before);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationPreviewStatus::Success),
                          static_cast<int>(result.preview));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationCommitStatus::NoChange),
                          static_cast<int>(result.commit));
    TEST_ASSERT_TRUE(fixture.revision() == before);
    TEST_ASSERT_EQUAL_STRING(current.c_str(), fixture.language().c_str());
}

void test_unknown_language_is_rejected_without_a_change() {
    Fixture fixture;
    const auto current = fixture.language();
    const auto before = fixture.revision();
    for (const char* id : {"xx", "", "DE", "de "}) {
        const auto result =
            fixture.application.applyDisplayLanguage(id, before);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ConfigurationPreviewStatus::InvalidCandidate),
            static_cast<int>(result.preview));
    }
    TEST_ASSERT_TRUE(fixture.revision() == before);
    TEST_ASSERT_EQUAL_STRING(current.c_str(), fixture.language().c_str());
}

void test_missing_revision_is_rejected_fail_closed() {
    Fixture fixture;
    const auto current = fixture.language();
    const auto result =
        fixture.application.applyDisplayLanguage("es", std::nullopt);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::StateChanged),
        static_cast<int>(result.preview));
    TEST_ASSERT_EQUAL_STRING(current.c_str(), fixture.language().c_str());
}

// A stale revision is refused, nothing changes, and the one visible preview
// slot is released so the next change with a fresh revision still works.
void test_stale_revision_is_rejected_and_releases_the_preview_slot() {
    Fixture fixture;
    const auto stale = fixture.revision();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(
            fixture.application.applyDisplayLanguage("es", stale).commit));
    const auto afterFirst = fixture.revision();
    TEST_ASSERT_TRUE(afterFirst != stale);

    const auto rejected = fixture.application.applyDisplayLanguage("en", stale);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationPreviewStatus::Success),
                          static_cast<int>(rejected.preview));
    TEST_ASSERT_TRUE(rejected.commit != ConfigurationCommitStatus::Activated);
    TEST_ASSERT_TRUE(rejected.commit != ConfigurationCommitStatus::NoChange);
    TEST_ASSERT_EQUAL_STRING("es", fixture.language().c_str());
    TEST_ASSERT_TRUE(fixture.revision() == afterFirst);

    const auto next =
        fixture.application.applyDisplayLanguage("en", fixture.revision());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(next.commit));
    TEST_ASSERT_EQUAL_STRING("en", fixture.language().c_str());
}

void test_not_started_application_has_no_configuration_runtime() {
    FermentationApplication application;
    const auto result = application.applyDisplayLanguage(
        "es",
        std::optional<UserConfigurationRevision>{UserConfigurationRevision{}});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable),
        static_cast<int>(result.preview));
}

void test_language_persists_across_a_restart() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    {
        device_platform::DevicePlatform platform;
        device_platform::VirtualTimeSource timeSource;
        FermentationApplication application;
        TEST_ASSERT_TRUE(platform.begin({true}));
        timeSource.setUnixTimeSeconds(1'700'000'000LL);
        TEST_ASSERT_TRUE(
            application.begin(platform, store, timeZoneResolver, timeSource));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ConfigurationCommitStatus::Activated),
            static_cast<int>(
                application
                    .applyDisplayLanguage(
                        "es", application.uiSnapshot()
                                  .revisions.expectedUserConfigurationRevision)
                    .commit));
    }
    device_platform::DevicePlatform platform;
    device_platform::VirtualTimeSource timeSource;
    FermentationApplication restarted;
    TEST_ASSERT_TRUE(platform.begin({true}));
    timeSource.setUnixTimeSeconds(1'700'000'100LL);
    TEST_ASSERT_TRUE(
        restarted.begin(platform, store, timeZoneResolver, timeSource));
    const auto source = restarted.uiPresentationSource();
    TEST_ASSERT_TRUE(source.has_value());
    TEST_ASSERT_EQUAL_STRING("es", source->displayLocale.value().c_str());
}

// Like every public Application entry (D5) the change waits for the shared
// application gate, so a Web callback and the touch loop cannot interleave.
void test_apply_display_language_waits_for_the_application_gate() {
    Fixture fixture;
    std::mutex synchronization;
    std::condition_variable changed;
    bool workerStarted = false;
    bool workerDone = false;
    const auto revision = fixture.revision();

    std::thread worker;
    {
        auto held =
            FermentationApplicationTestAccess::enter(fixture.application);
        worker = std::thread([&] {
            {
                std::lock_guard<std::mutex> lock(synchronization);
                workerStarted = true;
            }
            changed.notify_one();
            const auto result =
                fixture.application.applyDisplayLanguage("es", revision);
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(ConfigurationCommitStatus::Activated),
                static_cast<int>(result.commit));
            {
                std::lock_guard<std::mutex> lock(synchronization);
                workerDone = true;
            }
            changed.notify_one();
        });
        {
            std::unique_lock<std::mutex> lock(synchronization);
            TEST_ASSERT_TRUE(changed.wait_for(lock, std::chrono::seconds(1),
                                              [&] { return workerStarted; }));
            // Give the worker a chance to (wrongly) pass the gate.
            changed.wait_for(lock, std::chrono::milliseconds(100),
                             [&] { return workerDone; });
            TEST_ASSERT_FALSE(workerDone);
        }
    }
    {
        std::unique_lock<std::mutex> lock(synchronization);
        TEST_ASSERT_TRUE(changed.wait_for(lock, std::chrono::seconds(2),
                                          [&] { return workerDone; }));
    }
    worker.join();
    TEST_ASSERT_EQUAL_STRING("es", fixture.language().c_str());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_commit_activates_the_language_and_changes_the_revision);
    RUN_TEST(test_same_language_is_no_change_and_keeps_the_revision);
    RUN_TEST(test_unknown_language_is_rejected_without_a_change);
    RUN_TEST(test_missing_revision_is_rejected_fail_closed);
    RUN_TEST(test_stale_revision_is_rejected_and_releases_the_preview_slot);
    RUN_TEST(test_not_started_application_has_no_configuration_runtime);
    RUN_TEST(test_language_persists_across_a_restart);
    RUN_TEST(test_apply_display_language_waits_for_the_application_gate);
    return UNITY_END();
}
