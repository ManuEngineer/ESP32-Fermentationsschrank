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
    // A manual run needs the technical run limits of an owner that does not
    // exist (O5, #172 S9); the private body behind that guard gives these
    // tests a real active run to prove the device name gate.
    static FermentationApplicationRequestResult prepareStartManualTimedBody(
        FermentationApplication& application,
        const FermentationUiCommandContext& context,
        const ManualTimedRunValues& values) {
        return application.prepareStartManualTimedUnguarded(context, values);
    }
};

}  // namespace fermentation

namespace {

using namespace fermentation;

CrossRolePlausibilityContext validEvidence() {
    CrossRolePlausibilityContext evidence;
    evidence.air.quality = device_platform::SensorQuality::Valid;
    evidence.cooling.quality = device_platform::SensorQuality::Valid;
    evidence.product.quality = device_platform::SensorQuality::Valid;
    return evidence;
}

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
        application.publishOwningRuntimeEvidence(validEvidence());
    }

    std::optional<UserConfigurationRevision> revision() {
        return application.uiSnapshot()
            .revisions.expectedUserConfigurationRevision;
    }
    std::string deviceName() {
        const auto source = application.uiPresentationSource();
        TEST_ASSERT_TRUE(source.has_value());
        return source->deviceName;
    }
    ApplicationConfigurationChangeResult rename(const std::string& name) {
        return application.applyUserSettings({name}, revision());
    }
    void startRun() {
        ManualTimedRunValues values;
        values.targetTemperatureCelsius = 30.0;
        values.durationMinutes = 60U;
        values.qualificationBandCelsius = 0.5;
        values.qualificationDurationMinutes = 10U;
        values.maximumTargetReachMinutes = 180U;
        FermentationUiCommandContext context;
        context.expected = application.uiSnapshot().revisions;
        context.monotonicMillis = timeSource.monotonicMillis();
        const auto prepared =
            FermentationApplicationTestAccess::prepareStartManualTimedBody(
                application, context, values);
        TEST_ASSERT_TRUE(prepared.request.has_value());
        const auto confirmed = application.confirmPrepared(prepared);
        TEST_ASSERT_TRUE(confirmed.request.has_value());
        const auto applied =
            application.applyConfirmedPrepared(*confirmed.request);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
            static_cast<int>(applied.phase));
        TEST_ASSERT_TRUE(application.uiSnapshot().home.mode ==
                         FermentationHomeMode::ActiveRun);
    }
    void stopRun() {
        FermentationUiCommandContext context;
        context.expected = application.uiSnapshot().revisions;
        context.monotonicMillis = timeSource.monotonicMillis();
        FermentationUiStopRunIntent stop;
        stop.option = StopOption::AbortAndTurnOff;
        const auto prepared = application.prepareStop(context, stop);
        TEST_ASSERT_TRUE(prepared.request.has_value());
        const auto confirmed = application.confirmPrepared(prepared);
        TEST_ASSERT_TRUE(confirmed.request.has_value());
        static_cast<void>(
            application.applyConfirmedPrepared(*confirmed.request));
        TEST_ASSERT_TRUE(application.uiSnapshot().home.mode ==
                         FermentationHomeMode::Standby);
    }
};

void assertActivated(const ApplicationConfigurationChangeResult& result) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationPreviewStatus::Success),
                          static_cast<int>(result.preview));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(result.commit));
}

void test_ascii_name_commits_and_changes_the_revision() {
    Fixture fixture;
    const auto before = fixture.revision();
    TEST_ASSERT_TRUE(before.has_value());
    assertActivated(fixture.rename("Gaerschrank Keller"));
    TEST_ASSERT_EQUAL_STRING("Gaerschrank Keller",
                             fixture.deviceName().c_str());
    TEST_ASSERT_TRUE(fixture.revision() != before);
}

void test_multibyte_name_commits_without_cutting_a_character() {
    Fixture fixture;
    // 2- and 3-byte scalars: "Gärschrank Küche €"
    const std::string name =
        "G\xC3\xA4rschrank K\xC3\xBC"
        "che \xE2\x82\xAC";
    assertActivated(fixture.rename(name));
    TEST_ASSERT_EQUAL_STRING(name.c_str(), fixture.deviceName().c_str());
}

void test_name_limits_are_the_configuration_text_rules() {
    Fixture fixture;
    const auto original = fixture.deviceName();
    const auto before = fixture.revision();
    const std::string tooLong(49U, 'a');
    std::string tooManyBytes;
    for (int index = 0; index < 33; ++index) tooManyBytes += "\xE2\x82\xAC";
    const std::string rejected[] = {
        "",
        " leading",
        "trailing ",
        "   ",
        tooLong,
        tooManyBytes,
        std::string("ctl\x01name"),
        std::string("bad\xC3"),
        std::string("nul\0name", 8U),
    };
    for (const auto& name : rejected) {
        const auto result = fixture.rename(name);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ConfigurationPreviewStatus::InvalidCandidate),
            static_cast<int>(result.preview));
        TEST_ASSERT_EQUAL_STRING(original.c_str(),
                                 fixture.deviceName().c_str());
        TEST_ASSERT_TRUE(fixture.revision() == before);
    }
    // 48 scalars and 96 bytes are the accepted upper bounds; no preview slot
    // stayed taken by the rejected candidates.
    assertActivated(fixture.rename(std::string(48U, 'b')));
}

void test_same_name_is_no_change_and_missing_or_stale_revision_is_rejected() {
    Fixture fixture;
    const auto current = fixture.deviceName();
    const auto before = fixture.revision();
    const auto same = fixture.application.applyUserSettings({current}, before);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationPreviewStatus::Success),
                          static_cast<int>(same.preview));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationCommitStatus::NoChange),
                          static_cast<int>(same.commit));
    TEST_ASSERT_TRUE(fixture.revision() == before);

    const auto missing =
        fixture.application.applyUserSettings({"Anderer Name"}, std::nullopt);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::StateChanged),
        static_cast<int>(missing.preview));
    TEST_ASSERT_EQUAL_STRING(current.c_str(), fixture.deviceName().c_str());

    assertActivated(fixture.application.applyUserSettings({"Erster"}, before));
    const auto stale =
        fixture.application.applyUserSettings({"Zweiter"}, before);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationPreviewStatus::Success),
                          static_cast<int>(stale.preview));
    TEST_ASSERT_TRUE(stale.commit != ConfigurationCommitStatus::Activated);
    TEST_ASSERT_EQUAL_STRING("Erster", fixture.deviceName().c_str());
    // The preview slot was released: the next current change works.
    assertActivated(fixture.rename("Dritter"));
}

void test_a_missing_name_is_an_invalid_candidate() {
    Fixture fixture;
    const auto result = fixture.application.applyUserSettings(
        {std::nullopt}, fixture.revision());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::InvalidCandidate),
        static_cast<int>(result.preview));
}

// O4: the Application refuses the change while a run is active, on every
// surface, before any preview slot is taken; afterwards it works again.
void test_an_active_run_blocks_the_change_in_the_application() {
    Fixture fixture;
    const auto original = fixture.deviceName();
    fixture.startRun();
    const auto before = fixture.revision();
    const auto result = fixture.rename("Waehrend des Laufs");
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::NotAllowed),
        static_cast<int>(result.preview));
    TEST_ASSERT_EQUAL_STRING(original.c_str(), fixture.deviceName().c_str());
    TEST_ASSERT_TRUE(fixture.revision() == before);

    fixture.stopRun();
    assertActivated(fixture.rename("Nach dem Lauf"));
    TEST_ASSERT_EQUAL_STRING("Nach dem Lauf", fixture.deviceName().c_str());
}

void test_not_started_application_has_no_configuration_runtime() {
    FermentationApplication application;
    const auto result = application.applyUserSettings(
        {"Name"},
        std::optional<UserConfigurationRevision>{UserConfigurationRevision{}});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable),
        static_cast<int>(result.preview));
}

void test_the_name_persists_across_a_restart() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    const std::string name = "K\xC3\xBChlschrank";
    {
        device_platform::DevicePlatform platform;
        device_platform::VirtualTimeSource timeSource;
        FermentationApplication application;
        TEST_ASSERT_TRUE(platform.begin({true}));
        timeSource.setUnixTimeSeconds(1'700'000'000LL);
        TEST_ASSERT_TRUE(
            application.begin(platform, store, timeZoneResolver, timeSource));
        assertActivated(application.applyUserSettings(
            {name}, application.uiSnapshot()
                        .revisions.expectedUserConfigurationRevision));
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
    TEST_ASSERT_EQUAL_STRING(name.c_str(), source->deviceName.c_str());
}

// Like every public Application entry (D5) the change waits for the shared
// application gate.
void test_apply_user_settings_waits_for_the_application_gate() {
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
                fixture.application.applyUserSettings({"Wartend"}, revision);
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
    TEST_ASSERT_EQUAL_STRING("Wartend", fixture.deviceName().c_str());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ascii_name_commits_and_changes_the_revision);
    RUN_TEST(test_multibyte_name_commits_without_cutting_a_character);
    RUN_TEST(test_name_limits_are_the_configuration_text_rules);
    RUN_TEST(
        test_same_name_is_no_change_and_missing_or_stale_revision_is_rejected);
    RUN_TEST(test_a_missing_name_is_an_invalid_candidate);
    RUN_TEST(test_an_active_run_blocks_the_change_in_the_application);
    RUN_TEST(test_not_started_application_has_no_configuration_runtime);
    RUN_TEST(test_the_name_persists_across_a_restart);
    RUN_TEST(test_apply_user_settings_waits_for_the_application_gate);
    return UNITY_END();
}
