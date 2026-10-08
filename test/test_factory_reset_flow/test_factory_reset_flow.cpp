// Issue #19, Plan Abschnitt 4 (R1-Werksreset): Ablauf-Zustandsautomat,
// Anwendungseinstieg (Vorbedingungen unter dem Guard, Resetkern, Netzwerk-/
// HTTP-Sequenz ausserhalb des Guards) und Fehlerfaelle. Hardwarefrei.
#include <unity.h>

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <variant>

#include "authentication_records.hpp"
#include "configuration_bootstrap_store.hpp"
#include "device_platform.hpp"
#include "factory_reset_flow.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_commands.hpp"
#include "mock_network_lifecycle.hpp"
#include "mock_secure_random_source.hpp"
#include "mock_time_zone_resolver.hpp"
#include "simulated_persistent_state_store.hpp"
#include "touch_calibration.hpp"
#include "virtual_time_source.hpp"

namespace fermentation {

// Test seam (declared friend by the application): puts the published runtime
// run state into a running-process state without driving the full start flow.
class FermentationApplicationTestAccess {
   public:
    static void setActiveManualRun(FermentationApplication& application,
                                   bool active) {
        TEST_ASSERT_NOT_NULL(application.runtimeRunState_.get());
        if (active) {
            application.runtimeRunState_->activeManualRun = ManualRunPlan{};
        } else {
            application.runtimeRunState_->activeManualRun.reset();
        }
    }
};

}  // namespace fermentation

namespace {

using fermentation::FactoryResetCoreResult;
using fermentation::FactoryResetFlow;
using fermentation::FactoryResetKind;
using fermentation::FactoryResetOutcome;
using fermentation::FactoryResetStage;

template <typename T, typename V>
struct VariantHas;
template <typename T, typename... Alternatives>
struct VariantHas<T, std::variant<Alternatives...>>
    : std::disjunction<std::is_same<T, Alternatives>...> {};
// The reset step is deliberately not an alternative of the shared UI command
// variant: a non-local surface can never carry it (plan 4.4, invariant 1).
static_assert(
    !VariantHas<
        fermentation::FermentationUiFactoryResetCommand,
        decltype(fermentation::FermentationUiCommand::operation)>::value,
    "factory reset must stay outside the shared UI command variant");

constexpr std::uint32_t kHoldMs = 1500U;  // Testwert, kein Produktwert.

void test_flow_without_configured_hold_is_unavailable_and_has_no_default() {
    FactoryResetFlow unset;
    TEST_ASSERT_FALSE(unset.configured());
    TEST_ASSERT_FALSE(unset.begin(FactoryResetKind::PinIndependent));
    FactoryResetFlow zero(0U);
    TEST_ASSERT_FALSE(zero.configured());
    TEST_ASSERT_FALSE(zero.begin(FactoryResetKind::PinIndependent));
    TEST_ASSERT_TRUE(FactoryResetStage::Idle == unset.stage());
}

void test_flow_requires_every_stage_in_order_and_cancel_resets() {
    FactoryResetFlow flow(kHoldMs);
    TEST_ASSERT_TRUE(flow.begin(FactoryResetKind::PinIndependent));
    TEST_ASSERT_TRUE(FactoryResetStage::Warning == flow.stage());
    // No hold progress before the stages are acknowledged.
    TEST_ASSERT_FALSE(flow.updateHold(true, 1000U));
    TEST_ASSERT_FALSE(flow.updateHold(true, 100000U));
    TEST_ASSERT_TRUE(FactoryResetStage::Warning == flow.stage());
    TEST_ASSERT_TRUE(flow.acknowledge());
    TEST_ASSERT_TRUE(FactoryResetStage::Confirm == flow.stage());
    TEST_ASSERT_FALSE(flow.updateHold(true, 200000U));
    TEST_ASSERT_TRUE(flow.acknowledge());
    TEST_ASSERT_TRUE(FactoryResetStage::Hold == flow.stage());
    // A further acknowledge cannot skip the hold.
    TEST_ASSERT_FALSE(flow.acknowledge());
    TEST_ASSERT_TRUE(FactoryResetStage::Hold == flow.stage());

    for (int stages = 0; stages < 3; ++stages) {
        FactoryResetFlow other(kHoldMs);
        TEST_ASSERT_TRUE(other.begin(FactoryResetKind::PinIndependent));
        for (int step = 0; step < stages; ++step) {
            TEST_ASSERT_TRUE(other.acknowledge());
        }
        other.cancel();
        TEST_ASSERT_TRUE(FactoryResetStage::Idle == other.stage());
        TEST_ASSERT_TRUE(FactoryResetOutcome::None == other.outcome());
    }
}

void test_flow_hold_needs_continuous_contact_for_the_full_duration() {
    FactoryResetFlow flow(kHoldMs);
    TEST_ASSERT_TRUE(flow.begin(FactoryResetKind::PinIndependent));
    TEST_ASSERT_TRUE(flow.acknowledge());
    TEST_ASSERT_TRUE(flow.acknowledge());
    TEST_ASSERT_FALSE(flow.updateHold(true, 10000U));
    TEST_ASSERT_FALSE(flow.updateHold(true, 10000U + kHoldMs - 1U));
    TEST_ASSERT_EQUAL_UINT32(kHoldMs - 1U,
                             flow.heldMillis(10000U + kHoldMs - 1U));
    // Release resets the progress; the full duration is needed again.
    TEST_ASSERT_FALSE(flow.updateHold(false, 10000U + kHoldMs));
    TEST_ASSERT_EQUAL_UINT32(0U, flow.heldMillis(10000U + kHoldMs));
    TEST_ASSERT_FALSE(flow.updateHold(true, 20000U));
    TEST_ASSERT_FALSE(flow.updateHold(true, 20000U + kHoldMs - 1U));
    // Backward time resets instead of completing.
    TEST_ASSERT_FALSE(flow.updateHold(true, 5000U));
    TEST_ASSERT_FALSE(flow.updateHold(true, 5000U + kHoldMs - 1U));
    TEST_ASSERT_TRUE(flow.updateHold(true, 5000U + kHoldMs));
    TEST_ASSERT_TRUE(FactoryResetStage::Executing == flow.stage());
    // Executing cannot be cancelled; finish and dismiss close the flow.
    flow.cancel();
    TEST_ASSERT_TRUE(FactoryResetStage::Executing == flow.stage());
    flow.finish(FactoryResetOutcome::Completed);
    TEST_ASSERT_TRUE(FactoryResetStage::Finished == flow.stage());
    TEST_ASSERT_FALSE(flow.begin(FactoryResetKind::PinIndependent));
    flow.dismiss();
    TEST_ASSERT_TRUE(FactoryResetStage::Idle == flow.stage());
    TEST_ASSERT_TRUE(flow.begin(FactoryResetKind::PinIndependent));
}

void test_flow_variant_a_waits_for_a_verified_pin_and_b_never_does() {
    FactoryResetFlow flow(kHoldMs);
    TEST_ASSERT_TRUE(flow.begin(FactoryResetKind::PinProtected));
    TEST_ASSERT_TRUE(FactoryResetStage::PinRequired == flow.stage());
    TEST_ASSERT_FALSE(flow.acknowledge());
    TEST_ASSERT_TRUE(flow.pinVerified());
    TEST_ASSERT_TRUE(FactoryResetStage::Warning == flow.stage());
    FactoryResetFlow independent(kHoldMs);
    TEST_ASSERT_TRUE(independent.begin(FactoryResetKind::PinIndependent));
    TEST_ASSERT_FALSE(independent.pinVerified());
}

void test_outcome_never_reports_success_for_an_unconfirmed_network_stop() {
    using fermentation::factoryResetBoundaryCrossed;
    using fermentation::factoryResetOutcomeFor;
    TEST_ASSERT_TRUE(
        FactoryResetOutcome::Completed ==
        factoryResetOutcomeFor(FactoryResetCoreResult::Completed, true));
    TEST_ASSERT_TRUE(
        FactoryResetOutcome::CompletedNetworkNotConfirmed ==
        factoryResetOutcomeFor(FactoryResetCoreResult::Completed, false));
    TEST_ASSERT_TRUE(FactoryResetOutcome::HandoffUnavailable ==
                     factoryResetOutcomeFor(
                         FactoryResetCoreResult::HandoffUnavailable, true));
    TEST_ASSERT_TRUE(
        FactoryResetOutcome::HandoffUnavailableNetworkNotConfirmed ==
        factoryResetOutcomeFor(FactoryResetCoreResult::HandoffUnavailable,
                               false));
    TEST_ASSERT_TRUE(
        FactoryResetOutcome::Unavailable ==
        factoryResetOutcomeFor(FactoryResetCoreResult::Unavailable, false));
    TEST_ASSERT_TRUE(
        FactoryResetOutcome::Failed ==
        factoryResetOutcomeFor(FactoryResetCoreResult::Failed, false));
    TEST_ASSERT_TRUE(
        factoryResetBoundaryCrossed(FactoryResetCoreResult::Completed));
    TEST_ASSERT_TRUE(factoryResetBoundaryCrossed(
        FactoryResetCoreResult::HandoffUnavailable));
    TEST_ASSERT_FALSE(
        factoryResetBoundaryCrossed(FactoryResetCoreResult::Unavailable));
    TEST_ASSERT_FALSE(
        factoryResetBoundaryCrossed(FactoryResetCoreResult::Failed));
}

// --- application level -----------------------------------------------------

class DeterministicKdf final : public fermentation::IAuthenticationKdf {
   public:
    bool derive(
        const std::string& secret, const fermentation::AuthVerifier& parameters,
        std::array<std::uint8_t, fermentation::kAuthenticationVerifierBytes>&
            out) override {
        std::uint32_t state = 2166136261U;
        for (const auto byte : secret) {
            state ^= static_cast<std::uint8_t>(byte);
            state *= 16777619U;
        }
        for (const auto byte : parameters.salt) {
            state ^= byte;
            state *= 16777619U;
        }
        for (std::size_t index = 0U; index < out.size(); ++index) {
            state ^= static_cast<std::uint32_t>(index + 1U);
            state *= 16777619U;
            out[index] = static_cast<std::uint8_t>(state >> 24U);
        }
        return true;
    }
};

// Records the stop order against the network mock and, when armed, proves that
// stop() runs outside the Application gate: another thread must be able to
// enter the gate while stop() is blocked.
class RecordingHttpServerLifecycle final
    : public device_platform::IHttpServerLifecycle {
   public:
    [[nodiscard]] bool start(device_platform::IHttpRouteSink& routes) override {
        routes_ = &routes;
        running_ = true;
        return true;
    }
    [[nodiscard]] bool stop() override {
        ++stopCalls;
        stoppedWithNetworkStopCalls =
            network != nullptr ? network->stopCallCount() : 0U;
        if (probe) {
            probeEnteredGate = probe();
        }
        running_ = false;
        routes_ = nullptr;
        return stopResult;
    }
    [[nodiscard]] bool running() const override { return running_; }

    device_platform_test_support::MockNetworkLifecycle* network{nullptr};
    std::function<bool()> probe;
    bool probeEnteredGate{false};
    bool stopResult{true};
    std::size_t stopCalls{0U};
    std::size_t stoppedWithNetworkStopCalls{0U};

   private:
    bool running_{false};
    device_platform::IHttpRouteSink* routes_{nullptr};
};

struct Fixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    device_platform_test_support::MockSecureRandomSource random;
    DeterministicKdf kdf;
    RecordingHttpServerLifecycle http;
    fermentation::FermentationApplication application;

    Fixture() {
        http.network = &network;
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                           timeSource, network, http, random,
                                           kdf));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::NetworkConfigurationStatus::Applied),
            static_cast<int>(
                application
                    .applyNetworkMode(device_platform::NetworkMode::AP_ONLY)
                    .status));
        application.setFactoryResetHoldMillis(kHoldMs);
    }

    std::uint64_t epoch() const {
        const auto scan =
            fermentation::ConfigurationBootstrapStore(
                const_cast<device_platform_test_support::
                               SimulatedPersistentStateStore&>(store))
                .scan();
        TEST_ASSERT_TRUE(scan.loaded.has_value());
        return scan.loaded->record.storageEpoch.value();
    }

    // Drives the PIN-independent flow up to the hold stage.
    void armHold() {
        TEST_ASSERT_TRUE(
            application.beginFactoryReset(FactoryResetKind::PinIndependent));
        TEST_ASSERT_TRUE(application.acknowledgeFactoryReset());
        TEST_ASSERT_TRUE(application.acknowledgeFactoryReset());
    }

    void holdToCompletion(std::uint64_t startMs = 1000U) {
        application.updateFactoryResetHold(true, startMs);
        application.updateFactoryResetHold(true, startMs + kHoldMs);
    }
};

void test_application_flow_is_unavailable_until_the_owner_parameter_is_set() {
    Fixture fixture;
    fixture.application.setFactoryResetHoldMillis(std::nullopt);
    TEST_ASSERT_FALSE(fixture.application.factoryResetView(0U).available);
    TEST_ASSERT_FALSE(fixture.application.beginFactoryReset(
        FactoryResetKind::PinIndependent));
    TEST_ASSERT_EQUAL_UINT32(
        0U, fixture.application.factoryResetView(0U).holdRequiredMillis);
}

void test_application_offers_only_the_pin_independent_variant() {
    Fixture fixture;
    TEST_ASSERT_TRUE(fixture.application.factoryResetView(0U).available);
    // Variant A needs a local PIN verification that does not exist (O-R2).
    TEST_ASSERT_FALSE(
        fixture.application.beginFactoryReset(FactoryResetKind::PinProtected));
    TEST_ASSERT_TRUE(FactoryResetStage::Idle ==
                     fixture.application.factoryResetView(0U).stage);
}

void test_cancel_at_every_stage_changes_no_reset_state() {
    Fixture fixture;
    const auto epochBefore = fixture.epoch();
    for (int stages = 0; stages < 3; ++stages) {
        TEST_ASSERT_TRUE(fixture.application.beginFactoryReset(
            FactoryResetKind::PinIndependent));
        for (int step = 0; step < stages; ++step) {
            TEST_ASSERT_TRUE(fixture.application.acknowledgeFactoryReset());
        }
        fixture.application.cancelFactoryReset();
        TEST_ASSERT_TRUE(FactoryResetStage::Idle ==
                         fixture.application.factoryResetView(0U).stage);
    }
    // An incomplete hold never reaches the core.
    fixture.armHold();
    fixture.application.updateFactoryResetHold(true, 1000U);
    fixture.application.updateFactoryResetHold(true, 1000U + kHoldMs - 1U);
    fixture.application.updateFactoryResetHold(false, 1000U + kHoldMs);
    TEST_ASSERT_EQUAL_UINT64(epochBefore, fixture.epoch());
    TEST_ASSERT_EQUAL_UINT32(
        0U, static_cast<std::uint32_t>(fixture.network.stopCallCount()));
    TEST_ASSERT_EQUAL_UINT32(
        0U, static_cast<std::uint32_t>(fixture.http.stopCalls));
}

void test_full_flow_runs_the_core_then_ends_network_then_http() {
    Fixture fixture;
    const auto epochBefore = fixture.epoch();
    // Touch calibration records survive the reset (device specific).
    const auto touchKey = device_platform::StateStoreKey::create(
        device_platform::kTouchCalibrationActiveKeyBytes);
    TEST_ASSERT_TRUE(touchKey.key.has_value());
    TEST_ASSERT_TRUE(fixture.store.write(*touchKey.key, "touch-marker") ==
                     device_platform::StateStoreWriteStatus::Success);
    fixture.armHold();
    fixture.holdToCompletion();

    const auto view = fixture.application.factoryResetView(0U);
    TEST_ASSERT_TRUE(FactoryResetStage::Finished == view.stage);
    TEST_ASSERT_TRUE(FactoryResetOutcome::Completed == view.outcome);
    TEST_ASSERT_EQUAL_UINT64(epochBefore + 1U, fixture.epoch());
    // Network first, then HTTP (the network had been stopped when HTTP ran).
    TEST_ASSERT_EQUAL_UINT32(
        1U, static_cast<std::uint32_t>(fixture.network.stopCallCount()));
    TEST_ASSERT_EQUAL_UINT32(
        1U, static_cast<std::uint32_t>(fixture.http.stopCalls));
    TEST_ASSERT_EQUAL_UINT32(1U, static_cast<std::uint32_t>(
                                     fixture.http.stoppedWithNetworkStopCalls));
    TEST_ASSERT_FALSE(fixture.http.running());
    TEST_ASSERT_TRUE(device_platform::NetworkLifecycleState::Stopped ==
                     fixture.network.status().state);
    TEST_ASSERT_FALSE(fixture.network.accessPointInfo().has_value());
    // The old network mode is dropped; no restart with old credentials.
    TEST_ASSERT_TRUE(device_platform::NetworkMode::UNSELECTED ==
                     fixture.application.networkMode());
    TEST_ASSERT_EQUAL_UINT32(
        1U, static_cast<std::uint32_t>(fixture.network.startCallCount()));
    // Touch calibration marker unchanged.
    const auto touch = fixture.store.read(*touchKey.key, 64U);
    TEST_ASSERT_TRUE(touch.status ==
                     device_platform::StateStoreReadStatus::Success);
    TEST_ASSERT_EQUAL_STRING("touch-marker", touch.value.c_str());
    // The result is acknowledged before a new flow may begin.
    TEST_ASSERT_FALSE(fixture.application.beginFactoryReset(
        FactoryResetKind::PinIndependent));
    fixture.application.dismissFactoryReset();
    TEST_ASSERT_TRUE(FactoryResetStage::Idle ==
                     fixture.application.factoryResetView(0U).stage);
}

void test_http_is_stopped_outside_the_application_gate() {
    Fixture fixture;
    // While stop() is blocked inside the HTTP adapter, another thread (an HTTP
    // handler) must be able to enter the Application gate. A stop under the
    // gate would deadlock; the 5 s timeout turns that into a test failure.
    fixture.http.probe = [&fixture] {
        auto done = std::make_shared<std::promise<void>>();
        auto future = done->get_future();
        std::thread handler([&fixture, done] {
            static_cast<void>(fixture.application.networkMode());
            done->set_value();
        });
        const bool entered = future.wait_for(std::chrono::seconds(5)) ==
                             std::future_status::ready;
        if (entered) {
            handler.join();
        } else {
            // A handler blocked on the gate means stop() ran under it.
            handler.detach();
        }
        return entered;
    };
    fixture.armHold();
    fixture.holdToCompletion();
    TEST_ASSERT_EQUAL_UINT32(
        1U, static_cast<std::uint32_t>(fixture.http.stopCalls));
    TEST_ASSERT_TRUE(fixture.http.probeEnteredGate);
    TEST_ASSERT_TRUE(FactoryResetOutcome::Completed ==
                     fixture.application.factoryResetView(0U).outcome);
}

void test_unconfirmed_network_stop_is_never_reported_as_success() {
    Fixture fixture;
    fixture.network.setStopStatus(
        device_platform::NetworkOperationStatus::Failed);
    const auto startsBefore = fixture.network.startCallCount();
    fixture.armHold();
    fixture.holdToCompletion();
    TEST_ASSERT_TRUE(FactoryResetOutcome::CompletedNetworkNotConfirmed ==
                     fixture.application.factoryResetView(0U).outcome);
    // HTTP is still attempted, and nothing is restarted with old credentials.
    TEST_ASSERT_EQUAL_UINT32(
        1U, static_cast<std::uint32_t>(fixture.http.stopCalls));
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<std::uint32_t>(startsBefore),
        static_cast<std::uint32_t>(fixture.network.startCallCount()));
    TEST_ASSERT_TRUE(device_platform::NetworkMode::UNSELECTED ==
                     fixture.application.networkMode());
}

void test_unconfirmed_http_stop_is_never_reported_as_success() {
    Fixture fixture;
    fixture.http.stopResult = false;
    fixture.armHold();
    fixture.holdToCompletion();
    TEST_ASSERT_TRUE(FactoryResetOutcome::CompletedNetworkNotConfirmed ==
                     fixture.application.factoryResetView(0U).outcome);
    TEST_ASSERT_EQUAL_UINT32(
        1U, static_cast<std::uint32_t>(fixture.network.stopCallCount()));
}

void test_a_running_process_blocks_the_flow_and_the_core_is_not_called() {
    Fixture fixture;
    fermentation::FermentationApplicationTestAccess::setActiveManualRun(
        fixture.application, true);
    TEST_ASSERT_FALSE(fixture.application.factoryResetView(0U).available);
    TEST_ASSERT_FALSE(fixture.application.beginFactoryReset(
        FactoryResetKind::PinIndependent));
    fermentation::FermentationApplicationTestAccess::setActiveManualRun(
        fixture.application, false);

    // A run that appears after the flow began is caught at execution time,
    // under the same gate as the core call.
    const auto epochBefore = fixture.epoch();
    fixture.armHold();
    fixture.application.updateFactoryResetHold(true, 1000U);
    fermentation::FermentationApplicationTestAccess::setActiveManualRun(
        fixture.application, true);
    fixture.application.updateFactoryResetHold(true, 1000U + kHoldMs);
    TEST_ASSERT_TRUE(FactoryResetOutcome::Rejected ==
                     fixture.application.factoryResetView(0U).outcome);
    TEST_ASSERT_EQUAL_UINT64(epochBefore, fixture.epoch());
    TEST_ASSERT_EQUAL_UINT32(
        0U, static_cast<std::uint32_t>(fixture.network.stopCallCount()));
    TEST_ASSERT_EQUAL_UINT32(
        0U, static_cast<std::uint32_t>(fixture.http.stopCalls));
}

void test_a_configuration_without_runtime_is_reported_unavailable() {
    // Stop finding S1 (R0): a configuration without a loaded runtime has no
    // storage epoch in the application, so the flow is not offered.
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    const auto bootstrapKey = device_platform::StateStoreKey::create("cb0");
    TEST_ASSERT_TRUE(bootstrapKey.key.has_value());
    TEST_ASSERT_TRUE(store.write(*bootstrapKey.key, "not-a-bootstrap-record") ==
                     device_platform::StateStoreWriteStatus::Success);
    fermentation::FermentationApplication application;
    TEST_ASSERT_TRUE(platform.begin({true}));
    static_cast<void>(application.begin(platform, store, timeZoneResolver));
    application.setFactoryResetHoldMillis(kHoldMs);
    TEST_ASSERT_FALSE(application.factoryResetView(0U).available);
    TEST_ASSERT_FALSE(
        application.beginFactoryReset(FactoryResetKind::PinIndependent));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(
        test_flow_without_configured_hold_is_unavailable_and_has_no_default);
    RUN_TEST(test_flow_requires_every_stage_in_order_and_cancel_resets);
    RUN_TEST(test_flow_hold_needs_continuous_contact_for_the_full_duration);
    RUN_TEST(test_flow_variant_a_waits_for_a_verified_pin_and_b_never_does);
    RUN_TEST(
        test_outcome_never_reports_success_for_an_unconfirmed_network_stop);
    RUN_TEST(
        test_application_flow_is_unavailable_until_the_owner_parameter_is_set);
    RUN_TEST(test_application_offers_only_the_pin_independent_variant);
    RUN_TEST(test_cancel_at_every_stage_changes_no_reset_state);
    RUN_TEST(test_full_flow_runs_the_core_then_ends_network_then_http);
    RUN_TEST(test_http_is_stopped_outside_the_application_gate);
    RUN_TEST(test_unconfirmed_network_stop_is_never_reported_as_success);
    RUN_TEST(test_unconfirmed_http_stop_is_never_reported_as_success);
    RUN_TEST(test_a_running_process_blocks_the_flow_and_the_core_is_not_called);
    RUN_TEST(test_a_configuration_without_runtime_is_reported_unavailable);
    return UNITY_END();
}
