#include <unity.h>

#include <cstdio>
#include <string>

#include "cJSON.h"
#include "control_context.hpp"
#include "fermentation_ui_projector.hpp"
#include "run_commands.hpp"
#include "standard_program_catalog.hpp"
#include "web_json_codec.hpp"

#include "../common/allocation_window.hpp"

namespace {

using namespace fermentation;

RunCommandState programState(ProcessState phase, CompletionMode mode,
                             const std::string& name = std::string()) {
    auto document = FactoryProgramCatalog::find("water-kefir");
    TEST_ASSERT_TRUE(document.has_value());
    document->program.fermentationStages.front().targetTemperatureCelsius =
        38.0;
    document->program.fermentationStages.front().durationMinutes = 120U;
    document->program.productSensorFailure.fallbackDelaySeconds = 30U;
    document->program.targetQualification.bandCelsius = 0.5;
    document->program.targetQualification.durationMinutes = 10U;
    document->program.maximumTargetReachMinutes = 180U;
    document->program.completion.mode = mode;
    document->program.completion.coolingTargetCelsius =
        mode == CompletionMode::FinishWithoutCooling
            ? std::nullopt
            : std::optional<double>{9.0};
    document->program.completion.holdDurationMinutes =
        mode == CompletionMode::CoolAndHoldForDuration
            ? std::optional<std::uint32_t>{30U}
            : std::nullopt;
    document->program.preheat = phase == ProcessState::Preheating;
    document->program.maximumProductWaitMinutes =
        phase == ProcessState::Preheating ? std::optional<std::uint32_t>{30U}
                                          : std::nullopt;
    if (!name.empty()) document->program.name = name;
    const auto run =
        ActiveRun::start(*document, ProgramSourceKind::FactoryCatalog,
                         ::fermentation::RunProgramSourceRevision{1U});
    TEST_ASSERT_TRUE(run.has_value());
    RunCommandState state;
    state.processState.state = phase;
    state.processState.stateEnteredAtMillis = 1000U;
    state.activeProgramRun = *run;
    state.activeRunId = "run-1";
    state.activeRunSensorMode = RunSensorMode::Product;
    state.processRunSnapshot = makeProcessRunSnapshot(*state.activeProgramRun);
    TEST_ASSERT_TRUE(state.processRunSnapshot.has_value());
    return state;
}

FermentationUiSnapshot project(const RunCommandState& state,
                               std::optional<std::uint64_t> now) {
    FermentationUiProjectionInput input;
    input.runState = &state;
    input.application.lifecycleState = ApplicationLifecycleState::Ready;
    input.monotonicMillis = now;
    return FermentationUiProjector::project(input);
}

void test_name_and_source_come_from_the_frozen_run_source() {
    const auto state =
        programState(ProcessState::Fermenting, CompletionMode::CoolThenFinish,
                     "Sauerteig \xC3\xA4 <b>");
    const auto snapshot = project(state, 5000U);
    TEST_ASSERT_EQUAL_STRING("Sauerteig \xC3\xA4 <b>",
                             snapshot.home.programName.c_str());
    TEST_ASSERT_TRUE(snapshot.home.runDetail.has_value());
    TEST_ASSERT_TRUE(snapshot.home.runDetail->sourceKind ==
                     ProgramSourceKind::FactoryCatalog);
}

void test_missing_and_manual_sources_have_no_program_name() {
    RunCommandState none;
    auto snapshot = project(none, 5000U);
    TEST_ASSERT_FALSE(snapshot.home.runDetail.has_value());
    TEST_ASSERT_TRUE(snapshot.home.programName.empty());

    RunCommandState hold;
    hold.processState.state = ProcessState::ManualHolding;
    hold.activeRunId = "manual-1";
    ManualRunPlan plan;
    plan.values.runId = "manual-1";
    plan.values.targetTemperatureCelsius = 21.0;
    plan.values.qualificationBandCelsius = 0.5;
    plan.values.qualificationDurationMinutes = 10U;
    plan.values.maximumTargetReachMinutes = 180U;
    hold.activeManualRun = plan;
    hold.activeRunSensorMode = RunSensorMode::Air;
    hold.processRunSnapshot = makeProcessRunSnapshot(*hold.activeManualRun);
    TEST_ASSERT_TRUE(hold.processRunSnapshot.has_value());
    snapshot = project(hold, 5000U);
    TEST_ASSERT_TRUE(snapshot.home.runDetail.has_value());
    TEST_ASSERT_FALSE(snapshot.home.runDetail->sourceKind.has_value());
    TEST_ASSERT_FALSE(snapshot.home.runDetail->completion.has_value());
    TEST_ASSERT_TRUE(snapshot.home.programName.empty());
    TEST_ASSERT_TRUE(snapshot.home.runDetail->time.remainingKind ==
                     RunDetailRemainingKind::Unbounded);
    TEST_ASSERT_FALSE(
        snapshot.home.runDetail->time.remainingSeconds.has_value());
    TEST_ASSERT_TRUE(snapshot.home.runDetail->targetCelsius.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(21.0, *snapshot.home.runDetail->targetCelsius);
}

void test_target_follows_the_control_context_resolver() {
    auto state =
        programState(ProcessState::Fermenting, CompletionMode::CoolThenFinish);
    auto snapshot = project(state, 5000U);
    TEST_ASSERT_EQUAL_DOUBLE(38.0, *snapshot.home.runDetail->targetCelsius);

    // Cooling and CoolHolding show the cooling target, never the
    // fermentation target.
    state = programState(ProcessState::Cooling, CompletionMode::CoolThenFinish);
    snapshot = project(state, 5000U);
    TEST_ASSERT_TRUE(snapshot.home.runDetail->targetCelsius.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(9.0, *snapshot.home.runDetail->targetCelsius);
    state = programState(ProcessState::CoolHolding,
                         CompletionMode::CoolAndHoldForDuration);
    snapshot = project(state, 5000U);
    TEST_ASSERT_TRUE(snapshot.home.runDetail->targetCelsius.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(9.0, *snapshot.home.runDetail->targetCelsius);
    for (const auto phase :
         {ProcessState::Completed, ProcessState::RecoveryEvaluation,
          ProcessState::Fault}) {
        state.processState.state = phase;
        snapshot = project(state, 5000U);
        TEST_ASSERT_FALSE(snapshot.home.runDetail->targetCelsius.has_value());
    }
}

void test_all_four_completion_modes_are_projected() {
    auto state = programState(ProcessState::Fermenting,
                              CompletionMode::FinishWithoutCooling);
    auto detail = project(state, 5000U).home.runDetail;
    TEST_ASSERT_TRUE(detail->completion->mode ==
                     CompletionMode::FinishWithoutCooling);
    TEST_ASSERT_FALSE(detail->completion->coolingTargetCelsius.has_value());
    TEST_ASSERT_FALSE(detail->completion->holdDurationMinutes.has_value());

    state =
        programState(ProcessState::Fermenting, CompletionMode::CoolThenFinish);
    detail = project(state, 5000U).home.runDetail;
    TEST_ASSERT_EQUAL_DOUBLE(9.0, *detail->completion->coolingTargetCelsius);
    TEST_ASSERT_FALSE(detail->completion->holdDurationMinutes.has_value());

    state = programState(ProcessState::Fermenting,
                         CompletionMode::CoolAndHoldForDuration);
    detail = project(state, 5000U).home.runDetail;
    TEST_ASSERT_EQUAL_UINT32(30U, *detail->completion->holdDurationMinutes);

    state = programState(ProcessState::Fermenting,
                         CompletionMode::CoolAndHoldUntilManualStop);
    detail = project(state, 5000U).home.runDetail;
    TEST_ASSERT_TRUE(detail->completion->mode ==
                     CompletionMode::CoolAndHoldUntilManualStop);
    TEST_ASSERT_FALSE(detail->completion->holdDurationMinutes.has_value());
}

void test_phase_time_is_exact_or_unknown() {
    auto state =
        programState(ProcessState::Fermenting, CompletionMode::CoolThenFinish);
    // 120 minute phase, entered at 1000 ms.
    auto time = project(state, 1000U + 61U * 1000U).home.runDetail->time;
    TEST_ASSERT_EQUAL_UINT64(61U, *time.elapsedSeconds);
    TEST_ASSERT_EQUAL_UINT64(7200U - 61U, *time.remainingSeconds);
    TEST_ASSERT_TRUE(time.remainingKind == RunDetailRemainingKind::Timed);

    // Past the duration the remaining time stops at 0; the end is decided by
    // the process state, never by this value.
    time = project(state, 1000U + 9000U * 1000U).home.runDetail->time;
    TEST_ASSERT_EQUAL_UINT64(0U, *time.remainingSeconds);
    TEST_ASSERT_TRUE(time.remainingKind == RunDetailRemainingKind::Timed);

    // Hours do not wrap at 24.
    time = project(state, 1000U + 90061U * 1000U).home.runDetail->time;
    TEST_ASSERT_EQUAL_UINT64(90061U, *time.elapsedSeconds);

    // Missing time source and a clock running backwards: unknown, not 0.
    time = project(state, std::nullopt).home.runDetail->time;
    TEST_ASSERT_FALSE(time.elapsedSeconds.has_value());
    TEST_ASSERT_TRUE(time.remainingKind == RunDetailRemainingKind::Unknown);
    time = project(state, 500U).home.runDetail->time;
    TEST_ASSERT_FALSE(time.elapsedSeconds.has_value());
    TEST_ASSERT_FALSE(time.remainingSeconds.has_value());

    // Exact prior value for this very phase is added.
    state.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
        ProcessState::Fermenting, PriorBootPhaseElapsed{100U, 100U}};
    time = project(state, 1000U + 10U * 1000U).home.runDetail->time;
    TEST_ASSERT_EQUAL_UINT64(110U, *time.elapsedSeconds);
    // Wrong tag, unclear bounds or missing upper bound: unknown.
    state.priorBootPhaseElapsed->taggedState = ProcessState::CoolHolding;
    TEST_ASSERT_FALSE(
        project(state, 20000U).home.runDetail->time.elapsedSeconds.has_value());
    state.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
        ProcessState::Fermenting, PriorBootPhaseElapsed{100U, 200U}};
    TEST_ASSERT_FALSE(
        project(state, 20000U).home.runDetail->time.elapsedSeconds.has_value());
    state.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
        ProcessState::Fermenting, PriorBootPhaseElapsed{100U, std::nullopt}};
    TEST_ASSERT_FALSE(
        project(state, 20000U).home.runDetail->time.elapsedSeconds.has_value());
}

void test_remaining_kind_per_phase() {
    auto state =
        programState(ProcessState::Preheating, CompletionMode::CoolThenFinish);
    auto time = project(state, 5000U).home.runDetail->time;
    TEST_ASSERT_TRUE(time.elapsedSeconds.has_value());
    TEST_ASSERT_TRUE(time.remainingKind == RunDetailRemainingKind::Unknown);

    state = programState(ProcessState::CoolHolding,
                         CompletionMode::CoolAndHoldForDuration);
    time = project(state, 1000U + 60U * 1000U).home.runDetail->time;
    TEST_ASSERT_TRUE(time.remainingKind == RunDetailRemainingKind::Timed);
    TEST_ASSERT_EQUAL_UINT64(30U * 60U - 60U, *time.remainingSeconds);

    state = programState(ProcessState::CoolHolding,
                         CompletionMode::CoolAndHoldUntilManualStop);
    time = project(state, 5000U).home.runDetail->time;
    TEST_ASSERT_TRUE(time.remainingKind == RunDetailRemainingKind::Unbounded);
    TEST_ASSERT_FALSE(time.remainingSeconds.has_value());

    state =
        programState(ProcessState::Completed, CompletionMode::CoolThenFinish);
    time = project(state, 5000U).home.runDetail->time;
    TEST_ASSERT_TRUE(time.remainingKind ==
                     RunDetailRemainingKind::NotApplicable);
    TEST_ASSERT_FALSE(time.elapsedSeconds.has_value());

    for (const auto phase :
         {ProcessState::Fault, ProcessState::RecoveryEvaluation}) {
        state.processState.state = phase;
        time = project(state, 5000U).home.runDetail->time;
        TEST_ASSERT_FALSE(time.elapsedSeconds.has_value());
        TEST_ASSERT_TRUE(time.remainingKind == RunDetailRemainingKind::Unknown);
    }
}

void test_phase_seconds_do_not_change_the_semantic_snapshot() {
    const auto state =
        programState(ProcessState::Fermenting, CompletionMode::CoolThenFinish);
    const auto first = project(state, 5000U);
    const auto later = project(state, 9000U);
    TEST_ASSERT_TRUE(equalFermentationUiSemanticSnapshot(first, later));
    auto changed = state;
    changed.processState.state = ProcessState::Cooling;
    TEST_ASSERT_FALSE(
        equalFermentationUiSemanticSnapshot(first, project(changed, 5000U)));
}

std::string status(const FermentationUiSnapshot& snapshot) {
    std::string body;
    TEST_ASSERT_TRUE(
        encodeWebApiStatus(snapshot, FermentationDeviceInfoView{}, body));
    return body;
}

bool has(const std::string& text, const char* fragment) {
    return text.find(fragment) != std::string::npos;
}

void test_status_codec_emits_nullable_run_object() {
    FermentationUiSnapshot standby;
    standby.home.mode = FermentationHomeMode::Standby;
    TEST_ASSERT_TRUE(has(status(standby), "\"run\":null"));

    auto state = programState(ProcessState::Fermenting,
                              CompletionMode::CoolAndHoldForDuration, "Brot");
    const auto body = status(project(state, 1000U + 61U * 1000U));
    TEST_ASSERT_TRUE(has(body,
                         "\"run\":{\"id\":\"run-1\",\"sourceKind\":"
                         "\"factory-catalog\",\"programName\":\"Brot\","
                         "\"targetCelsius\":38,\"completion\":{\"mode\":"
                         "\"cool-hold-duration\",\"coolingTargetCelsius\":9,"
                         "\"holdDurationMinutes\":30},\"time\":{\"scope\":"
                         "\"phase\",\"elapsedSeconds\":61,"
                         "\"remainingSeconds\":7139,\"remainingKind\":"
                         "\"timed\"}}"));
    // Existing members stay and the object order of the old fields is kept.
    TEST_ASSERT_TRUE(has(body, "\"version\":1,\"ready\":"));
    TEST_ASSERT_TRUE(has(body, "\"revisions\":{"));

    for (const auto& pair :
         {std::make_pair(CompletionMode::FinishWithoutCooling, "\"finish\""),
          std::make_pair(CompletionMode::CoolThenFinish, "\"cool-finish\""),
          std::make_pair(CompletionMode::CoolAndHoldUntilManualStop,
                         "\"cool-hold-manual\"")}) {
        state = programState(ProcessState::Fermenting, pair.first);
        const auto text = status(project(state, 5000U));
        TEST_ASSERT_TRUE(has(text, pair.second));
        TEST_ASSERT_FALSE(has(text, "\"programName\":\"\""));
    }

    // The two other source kinds map to their tokens; a pure manual hold and a
    // nameless manual timed run emit null instead of a placeholder.
    auto kinds = project(state, 5000U);
    kinds.home.runDetail->sourceKind = ProgramSourceKind::UserProgram;
    TEST_ASSERT_TRUE(has(status(kinds), "\"sourceKind\":\"user-program\""));
    kinds.home.runDetail->sourceKind = ProgramSourceKind::ManualTimed;
    kinds.home.programName.clear();
    auto manual = status(kinds);
    TEST_ASSERT_TRUE(has(manual, "\"sourceKind\":\"manual-timed\""));
    TEST_ASSERT_TRUE(has(manual, "\"programName\":null"));
    kinds.home.runDetail->sourceKind.reset();
    TEST_ASSERT_TRUE(has(status(kinds), "\"sourceKind\":null"));

    // Unknown time stays null, never 0.
    const auto unknown = status(project(state, std::nullopt));
    TEST_ASSERT_TRUE(has(unknown, "\"elapsedSeconds\":null"));
    TEST_ASSERT_TRUE(has(unknown, "\"remainingKind\":\"unknown\""));
}

FermentationUiSnapshot worstCaseSnapshot() {
    // 96 control bytes need 6 bytes each (\u00XX) in the JSON text; 48 byte run
    // id is the existing limit.
    auto state = programState(ProcessState::Fermenting,
                              CompletionMode::CoolAndHoldForDuration,
                              std::string(96U, '\x01'));
    state.activeRunId = std::string(48U, 'r');
    auto snapshot = project(state, 1000U + 90061U * 1000U);
    snapshot.revisions.expectedStateSequence = 4294967295U;
    snapshot.revisions.expectedRunRevision = 4294967295U;
    snapshot.revisions.expectedMessageRevision = 4294967295U;
    snapshot.revisions.expectedFaultRevision = 4294967295U;
    snapshot.revisions.expectedRecoveryEpisodeRevision = 4294967295U;
    snapshot.home.mode = FermentationHomeMode::ActiveRun;
    return snapshot;
}

void test_worst_case_status_body_fits_and_oversized_name_fails_closed() {
    const auto body = status(worstCaseSnapshot());
    TEST_ASSERT_TRUE(body.size() <= kMaximumWebApiResponseBodyBytes);
    TEST_ASSERT_TRUE(has(body, "\\u0001\\u0001"));
    std::printf("STATUS_WORST_CASE_BODY_BYTES=%zu\n", body.size());

    auto oversized = worstCaseSnapshot();
    oversized.home.programName.assign(600U, '\x01');
    std::string output = "unchanged";
    TEST_ASSERT_FALSE(
        encodeWebApiStatus(oversized, FermentationDeviceInfoView{}, output));
    TEST_ASSERT_EQUAL_STRING("unchanged", output.c_str());
}

using alloc_window::Window;

Window measure(const FermentationUiSnapshot& snapshot, std::string& output) {
    bool encoded = false;
    const auto window = alloc_window::measure([&] {
        encoded =
            encodeWebApiStatus(snapshot, FermentationDeviceInfoView{}, output);
    });
    TEST_ASSERT_TRUE(encoded);
    return window;
}

void test_status_encoding_allocations_cover_cjson_and_operator_new() {
    FermentationUiSnapshot standby;
    standby.home.mode = FermentationHomeMode::Standby;
    const auto worst = worstCaseSnapshot();
    std::string standbyBody;
    std::string worstBody;
    auto typicalState =
        programState(ProcessState::Fermenting, CompletionMode::CoolThenFinish,
                     "Sauerteig 24h");
    const auto typical = project(typicalState, 1000U + 61U * 1000U);
    std::string typicalBody;
    const auto typicalWindow = measure(typical, typicalBody);
    std::printf(
        "STATUS_ALLOC typical: count=%zu bytes=%zu largest=%zu peak_live=%zu "
        "cjson_count=%zu body=%zu\n",
        typicalWindow.count, typicalWindow.requestedBytes,
        typicalWindow.largestBlock, typicalWindow.peakLiveBytes,
        typicalWindow.cJsonCount, typicalBody.size());
    TEST_ASSERT_EQUAL_UINT32(
        0U, static_cast<std::uint32_t>(typicalWindow.cJsonLiveBytes));
    const auto base = measure(standby, standbyBody);
    const auto peak = measure(worst, worstBody);
    std::printf(
        "STATUS_ALLOC standby: count=%zu bytes=%zu largest=%zu peak_live=%zu "
        "cjson_count=%zu body=%zu\n",
        base.count, base.requestedBytes, base.largestBlock, base.peakLiveBytes,
        base.cJsonCount, standbyBody.size());
    std::printf(
        "STATUS_ALLOC worst:   count=%zu bytes=%zu largest=%zu peak_live=%zu "
        "cjson_count=%zu body=%zu\n",
        peak.count, peak.requestedBytes, peak.largestBlock, peak.peakLiveBytes,
        peak.cJsonCount, worstBody.size());
    // The cJSON path is really covered by the hooks and fully released; the
    // string is the only block allowed to outlive the window.
    TEST_ASSERT_TRUE(peak.cJsonCount > base.cJsonCount);
    TEST_ASSERT_EQUAL_UINT32(0U,
                             static_cast<std::uint32_t>(peak.cJsonLiveBytes));
    TEST_ASSERT_EQUAL_UINT32(0U,
                             static_cast<std::uint32_t>(base.cJsonLiveBytes));
    TEST_ASSERT_TRUE(peak.peakLiveBytes > worstBody.size());
    TEST_ASSERT_TRUE(peak.peakLiveBytes > base.peakLiveBytes);
    TEST_ASSERT_TRUE(peak.largestBlock <= kMaximumWebApiResponseBodyBytes);
}

void test_status_with_maximum_device_info_stays_bounded_and_is_released() {
    FermentationDeviceInfoView device;
    // 96 control bytes escape to 576 bytes: a boundary beyond what a
    // validated visible name can produce.
    device.deviceName = std::string(96U, '\x01');
    device.timeZoneId = std::string(64U, 'z');
    device.connection = device_platform::DeviceUiNetworkStatus::Connected;
    device_platform::LocalTime local;
    local.year = 2026;
    local.month = 10;
    local.day = 20;
    local.hour = 14;
    local.minute = 3;
    device.localTime = local;
    const auto worst = worstCaseSnapshot();
    std::string withDevice;
    bool encoded = false;
    const auto window = alloc_window::measure(
        [&] { encoded = encodeWebApiStatus(worst, device, withDevice); });
    TEST_ASSERT_TRUE(encoded);
    std::printf(
        "STATUS_ALLOC worst+device: count=%zu bytes=%zu largest=%zu "
        "peak_live=%zu cjson_count=%zu body=%zu\n",
        window.count, window.requestedBytes, window.largestBlock,
        window.peakLiveBytes, window.cJsonCount, withDevice.size());
    TEST_ASSERT_TRUE(withDevice.size() <= kMaximumWebApiResponseBodyBytes);
    TEST_ASSERT_TRUE(has(withDevice, "\"networkState\":\"connected\""));
    TEST_ASSERT_TRUE(has(withDevice, "\"local\":\"2026-10-20 14:03\""));
    TEST_ASSERT_EQUAL_UINT32(0U,
                             static_cast<std::uint32_t>(window.cJsonLiveBytes));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_name_and_source_come_from_the_frozen_run_source);
    RUN_TEST(test_missing_and_manual_sources_have_no_program_name);
    RUN_TEST(test_target_follows_the_control_context_resolver);
    RUN_TEST(test_all_four_completion_modes_are_projected);
    RUN_TEST(test_phase_time_is_exact_or_unknown);
    RUN_TEST(test_remaining_kind_per_phase);
    RUN_TEST(test_phase_seconds_do_not_change_the_semantic_snapshot);
    RUN_TEST(test_status_codec_emits_nullable_run_object);
    RUN_TEST(test_worst_case_status_body_fits_and_oversized_name_fails_closed);
    RUN_TEST(test_status_encoding_allocations_cover_cjson_and_operator_new);
    RUN_TEST(
        test_status_with_maximum_device_info_stays_bounded_and_is_released);
    return UNITY_END();
}
