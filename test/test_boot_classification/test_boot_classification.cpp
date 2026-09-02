#include <array>
#include <string>

#include <unity.h>

#include "boot_classification.hpp"
#include "run_persistence_codec.hpp"

namespace {

using namespace fermentation;
using namespace fermentation::boot_classification;

RunPersistenceSnapshot snapshotFor(ProcessState state) {
    RunPersistenceSnapshot snapshot;
    snapshot.variant = RunCheckpointVariant::ProgramRun;
    snapshot.processState.state = state;
    return snapshot;
}

void test_resume_eligibility_keeps_the_r1_phase_matrix() {
    constexpr std::array<ProcessState, 8> states = {
        ProcessState::Preheating,     ProcessState::WaitingForProduct,
        ProcessState::ReachingTarget, ProcessState::QualifyingTarget,
        ProcessState::Fermenting,     ProcessState::Cooling,
        ProcessState::CoolHolding,    ProcessState::ManualHolding,
    };
    constexpr std::array<bool, 8> eligible = {true,  false, false, false,
                                              false, true,  false, true};

    for (std::size_t index = 0U; index < states.size(); ++index) {
        const auto snapshot = snapshotFor(states[index]);
        TEST_ASSERT_TRUE(isR1ResumeEligible(snapshot) == eligible[index]);
    }
}

void test_current_fermenting_enters_r1_recovery_evaluation() {
    auto snapshot = snapshotFor(ProcessState::Fermenting);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunLoadDisposition::RecoveryEvaluation),
        static_cast<int>(
            classifyRunLoad(RunPersistenceLoadStatus::Current, &snapshot)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BootClassification::RecoveryEvaluation),
        static_cast<int>(
            classify(RunPersistenceLoadStatus::Current, &snapshot)));

    snapshot.processState.state = ProcessState::RecoveryEvaluation;
    TEST_ASSERT_FALSE(isR1ResumeEligible(snapshot));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BootClassification::DiscardableRun),
                          static_cast<int>(classify(
                              RunPersistenceLoadStatus::Current, &snapshot)));
}

void test_fallback_recovered_requires_explicit_selection() {
    const auto active = snapshotFor(ProcessState::Preheating);
    const auto completed = snapshotFor(ProcessState::Completed);
    const auto fault = snapshotFor(ProcessState::Fault);
    const auto status = RunPersistenceLoadStatus::FallbackRecovered;

    for (const auto* snapshot : {&active, &completed, &fault}) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunLoadDisposition::FallbackSelectionRequired),
            static_cast<int>(classifyRunLoad(status, snapshot)));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(BootClassification::FallbackSelectionRequired),
                              static_cast<int>(classify(status, snapshot)));
    }
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunLoadDisposition::SafeBoot),
                          static_cast<int>(classifyRunLoad(status, nullptr)));
}

void test_all_load_outcomes_map_to_the_r1_boot_classification() {
    auto snapshot = snapshotFor(ProcessState::Preheating);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BootClassification::NoRun),
        static_cast<int>(
            classify(RunPersistenceLoadStatus::NoPersistedRun, nullptr)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BootClassification::NoRun),
                          static_cast<int>(classify(
                              RunPersistenceLoadStatus::NoActiveRun, nullptr)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BootClassification::ResumeOffer),
                          static_cast<int>(classify(
                              RunPersistenceLoadStatus::Current, &snapshot)));

    snapshot.processState.state = ProcessState::Completed;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BootClassification::CompletedRun),
                          static_cast<int>(classify(
                              RunPersistenceLoadStatus::Current, &snapshot)));
    snapshot.processState.state = ProcessState::Fault;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BootClassification::TerminalRunFault),
        static_cast<int>(
            classify(RunPersistenceLoadStatus::Current, &snapshot)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BootClassification::SafeBoot),
        static_cast<int>(classify(RunPersistenceLoadStatus::Current, nullptr)));
}

void test_legacy_boot_and_safeboot_snapshots_are_rejected_as_invalid() {
    for (const auto state : {ProcessState::Boot, ProcessState::SafeBoot}) {
        auto snapshot = snapshotFor(state);
        std::string encoded;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceCodecStatus::InvalidSnapshot),
            static_cast<int>(encodeRunPersistenceSnapshot(snapshot, encoded)));
    }
}

void test_legacy_process_state_enum_order_is_unchanged() {
    // ProcessState is zero-based in C++; the codec owns the stable 1..15
    // wire mapping explicitly. This guards the enum order without treating
    // the C++ representation as the wire contract.
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(ProcessState::Boot));
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(ProcessState::SafeBoot));
    TEST_ASSERT_EQUAL_INT(2, static_cast<int>(ProcessState::Standby));
    TEST_ASSERT_EQUAL_INT(12,
                          static_cast<int>(ProcessState::RecoveryEvaluation));
    TEST_ASSERT_EQUAL_INT(13, static_cast<int>(ProcessState::Fault));
    TEST_ASSERT_EQUAL_INT(14, static_cast<int>(ProcessState::ServiceMode));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_resume_eligibility_keeps_the_r1_phase_matrix);
    RUN_TEST(test_current_fermenting_enters_r1_recovery_evaluation);
    RUN_TEST(test_fallback_recovered_requires_explicit_selection);
    RUN_TEST(test_all_load_outcomes_map_to_the_r1_boot_classification);
    RUN_TEST(test_legacy_boot_and_safeboot_snapshots_are_rejected_as_invalid);
    RUN_TEST(test_legacy_process_state_enum_order_is_unchanged);
    return UNITY_END();
}
