#include <unity.h>

#include <limits>
#include <optional>
#include <utility>

#include "application_run_identity.hpp"
#include "fermentation_ui_commands.hpp"

namespace {

using device_platform::StorageEpoch;
using fermentation::ApplicationRunIdentity;
using fermentation::CommandId;
using fermentation::ProgramCatalogRevision;

void test_empty_identity_space_allocates_from_one() {
    auto identity = ApplicationRunIdentity::create(
        StorageEpoch{7U}, std::optional<CommandId>{CommandId{0U}});
    TEST_ASSERT_TRUE(identity.has_value());

    auto allocator = std::move(*identity);
    const auto first = allocator.allocateCommandId();
    TEST_ASSERT_TRUE(first.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, *first);

    const auto uiId = allocator.allocateUiRequestId();
    TEST_ASSERT_TRUE(uiId.has_value());
    TEST_ASSERT_EQUAL_UINT64(2U, uiId->value);
    const auto runId = allocator.makeRunId(*first);
    TEST_ASSERT_TRUE(runId.has_value());
    TEST_ASSERT_EQUAL_STRING("e7-c1", runId->c_str());
}

void test_identity_uses_committed_high_water_and_rejects_invalid_bases() {
    auto identity = ApplicationRunIdentity::create(
        StorageEpoch{3U}, std::optional<CommandId>{CommandId{41U}});
    TEST_ASSERT_TRUE(identity.has_value());
    auto allocator = std::move(*identity);
    TEST_ASSERT_EQUAL_UINT64(42U, *allocator.allocateCommandId());

    TEST_ASSERT_FALSE(ApplicationRunIdentity::create(
                          StorageEpoch{}, std::optional<CommandId>{0U})
                          .has_value());
    TEST_ASSERT_FALSE(
        ApplicationRunIdentity::create(StorageEpoch{3U}, std::nullopt)
            .has_value());
}

void test_identity_overflow_does_not_wrap_or_issue_zero() {
    auto identity = ApplicationRunIdentity::create(
        StorageEpoch{1U},
        std::optional<CommandId>{std::numeric_limits<CommandId>::max()});
    TEST_ASSERT_TRUE(identity.has_value());
    auto allocator = std::move(*identity);
    TEST_ASSERT_FALSE(allocator.allocateCommandId().has_value());
    TEST_ASSERT_FALSE(allocator.allocateUiRequestId().has_value());
}

void test_catalog_revision_maps_to_neutral_run_provenance_without_truncation() {
    const auto mapped = fermentation::makeRunProgramSourceRevision(
        ProgramCatalogRevision{0x1'0000'0000ULL + 9U});
    TEST_ASSERT_TRUE(mapped.has_value());
    TEST_ASSERT_EQUAL_UINT64(0x1'0000'0000ULL + 9U, mapped->value());
    TEST_ASSERT_FALSE(
        fermentation::makeRunProgramSourceRevision(ProgramCatalogRevision{})
            .has_value());
}

void test_ui_id_is_copied_verbatim_into_existing_command_envelope() {
    fermentation::FermentationUiCommandContext context;
    context.requestId.value = 19U;
    context.surface = device_platform::UiSurface::WebInterface;
    context.monotonicMillis = 100U;
    const auto envelope =
        fermentation::FermentationUiCommandBridge::makeEnvelope(context);
    TEST_ASSERT_EQUAL_UINT64(19U, envelope.id);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::CommandSource::WebInterface),
        static_cast<int>(envelope.source));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_identity_space_allocates_from_one);
    RUN_TEST(test_identity_uses_committed_high_water_and_rejects_invalid_bases);
    RUN_TEST(test_identity_overflow_does_not_wrap_or_issue_zero);
    RUN_TEST(
        test_catalog_revision_maps_to_neutral_run_provenance_without_truncation);
    RUN_TEST(test_ui_id_is_copied_verbatim_into_existing_command_envelope);
    return UNITY_END();
}
