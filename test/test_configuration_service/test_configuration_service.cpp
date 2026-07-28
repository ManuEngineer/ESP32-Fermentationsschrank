#include <unity.h>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "configuration_documents.hpp"
#include "configuration_graph.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_limits.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_service.hpp"
#include "configuration_storage_contract.hpp"
#include "state_store.hpp"

namespace {

class EmptyStore final : public device_platform::IStateStore {
   public:
    device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey&, const std::string&) override {
        return device_platform::StateStoreWriteStatus::WriteError;
    }
    device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey&, std::size_t) const override {
        return {device_platform::StateStoreReadStatus::NotFound, {}};
    }
};

class Resolver final : public device_platform::ITimeZoneResolver {
   public:
    device_platform::TimeZonePrepareResult prepare(
        const std::string& identifier) const override {
        if (identifier != "Europe/Zurich") {
            return {
                device_platform::TimeZonePrepareStatus::UnsupportedIdentifier,
                std::nullopt};
        }
        return {device_platform::TimeZonePrepareStatus::Success,
                device_platform::PreparedTimeZone{identifier}};
    }
};

fermentation::LoadedConfigurationGraph graph() {
    const device_platform::StorageEpoch epoch{1U};
    fermentation::ConfigurationManifest manifest{
        fermentation::decodeChangeOrigin(1U),
        fermentation::decodeChangeOperation(2U),
        {fermentation::configuration_storage_contract::
             kUserConfigurationRecordType,
         device_platform::SlotId{0U},
         fermentation::UserConfigurationRevision{1U}, 1U, 0U, 0U, epoch},
        {fermentation::configuration_storage_contract::
             kServiceConfigurationRecordType,
         device_platform::SlotId{0U},
         fermentation::ServiceConfigurationRevision{1U}, 1U, 0U, 0U, epoch},
        {fermentation::configuration_storage_contract::
             kProgramCatalogRecordType,
         device_platform::SlotId{0U}, fermentation::ProgramCatalogRevision{1U},
         1U, 0U, 0U, epoch}};
    fermentation::ConfigurationManifestReference manifestReference{
        fermentation::configuration_storage_contract::
            kConfigurationManifestRecordType,
        device_platform::SlotId{0U},
        fermentation::ConfigurationManifestGeneration{1U},
        1U,
        104U,
        0U,
        epoch};
    fermentation::ConfigurationGraphBranch branch{
        manifestReference,
        manifest,
        std::make_shared<const fermentation::UserConfiguration>(
            fermentation::UserConfiguration{"de", "Europe/Zurich",
                                            "Fermentationsschrank"}),
        std::make_shared<const fermentation::ServiceConfiguration>(),
        std::make_shared<const fermentation::ProgramCatalog>(
            fermentation::makeFactoryProgramCatalog()),
        "manifest"};
    fermentation::ConfigurationRootRecord root{manifestReference, std::nullopt};
    return {device_platform::SlotId{0U},
            fermentation::ConfigurationRootSequence{1U},
            root,
            "root",
            std::move(branch),
            std::nullopt,
            false};
}

struct Fixture {
    EmptyStore store;
    Resolver resolver;
    fermentation::ConfigurationGraphStore graphStore{store, resolver};
    fermentation::ConfigurationMutationCoordinator coordinator;
    fermentation::ConfigurationService service{coordinator, graphStore,
                                               resolver};

    Fixture() { TEST_ASSERT_TRUE(service.initialize(graph())); }
};

void test_initial_runtime_is_available_through_move_only_lease() {
    Fixture fixture;
    auto result = fixture.service.acquireRuntime();
    TEST_ASSERT_TRUE(
        result.status ==
        fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted);
    TEST_ASSERT_TRUE(result.lease.valid());
    TEST_ASSERT_EQUAL_STRING(
        "Fermentationsschrank",
        result.lease->userConfiguration().deviceName.c_str());
    TEST_ASSERT_EQUAL_UINT64(1U, result.lease->volatileGenerationId());
}

void test_runtime_reader_limit_is_enforced_and_released() {
    Fixture fixture;
    std::vector<fermentation::RuntimeConfigurationReadLease> leases;
    for (std::size_t index = 0U;
         index <
         fermentation::configuration_limits::kMaxRuntimeConfigurationReadLeases;
         ++index) {
        auto result = fixture.service.acquireRuntime();
        TEST_ASSERT_TRUE(
            result.status ==
            fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted);
        leases.push_back(std::move(result.lease));
    }
    auto ninth = fixture.service.acquireRuntime();
    TEST_ASSERT_TRUE(
        ninth.status ==
        fermentation::RuntimeConfigurationReadStatus::RuntimeReadLeaseBusy);
    leases.pop_back();
    auto available = fixture.service.acquireRuntime();
    TEST_ASSERT_TRUE(
        available.status ==
        fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted);
}

void test_changed_preview_owns_the_only_second_model_generation() {
    Fixture fixture;
    auto build = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(build.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_TRUE(build.lease.replaceUserConfiguration(
        {"de", "Europe/Zurich", "Neuer Name"}));
    auto second = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(
        second.status ==
        fermentation::ConfigurationPreviewStatus::ConfigurationModelBudgetBusy);
    auto installed = fixture.service.installPreview(
        std::move(build.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(installed.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_TRUE(installed.preview.has_value());
    TEST_ASSERT_FALSE(installed.preview->noChange);
    TEST_ASSERT_TRUE(installed.preview->changes.userConfiguration);
    TEST_ASSERT_EQUAL_UINT32(2U, fixture.service.fullModelGenerationCount());
    auto blocked = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(
        blocked.status ==
        fermentation::ConfigurationPreviewStatus::ConfigurationModelBudgetBusy);
    TEST_ASSERT_TRUE(fixture.service.cancelPreview(installed.preview->handle) ==
                     fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.service.fullModelGenerationCount());
}

void test_no_change_preview_is_lightweight_and_identity_bound() {
    Fixture fixture;
    auto build = fixture.service.beginPreview();
    auto installed = fixture.service.installPreview(
        std::move(build.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(installed.preview->noChange);
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.service.fullModelGenerationCount());
    TEST_ASSERT_TRUE(
        fixture.service.cancelPreview(installed.preview->handle + 1U) ==
        fermentation::ConfigurationPreviewStatus::PreviewSuperseded);
    TEST_ASSERT_TRUE(fixture.service.visiblePreview().has_value());
    TEST_ASSERT_TRUE(fixture.service.cancelPreview(installed.preview->handle) ==
                     fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_FALSE(fixture.service.visiblePreview().has_value());
}

void test_invalid_new_request_does_not_replace_visible_no_change_preview() {
    Fixture fixture;
    auto firstBuild = fixture.service.beginPreview();
    auto first = fixture.service.installPreview(
        std::move(firstBuild.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    auto invalidBuild = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(invalidBuild.lease.replaceUserConfiguration(
        {"xx", "Europe/Zurich", "Ungueltig"}));
    auto invalid = fixture.service.installPreview(
        std::move(invalidBuild.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(
        invalid.status ==
        fermentation::ConfigurationPreviewStatus::InvalidCandidate);
    TEST_ASSERT_EQUAL_UINT64(first.preview->handle,
                             fixture.service.visiblePreview()->handle);
}

void test_abandoned_build_lease_releases_model_budget() {
    Fixture fixture;
    {
        auto abandoned = fixture.service.beginPreview();
        TEST_ASSERT_TRUE(abandoned.lease.valid());
    }
    auto next = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(next.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_initial_runtime_is_available_through_move_only_lease);
    RUN_TEST(test_runtime_reader_limit_is_enforced_and_released);
    RUN_TEST(test_changed_preview_owns_the_only_second_model_generation);
    RUN_TEST(test_no_change_preview_is_lightweight_and_identity_bound);
    RUN_TEST(
        test_invalid_new_request_does_not_replace_visible_no_change_preview);
    RUN_TEST(test_abandoned_build_lease_releases_model_budget);
    return UNITY_END();
}
