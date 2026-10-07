#include <unity.h>

#include <cstddef>
#include <cstdlib>
#include <new>
#include <optional>

#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_presentation_cache.hpp"
#include "fermentation_ui_text.hpp"
#include "local_time.hpp"
#include "mock_network_lifecycle.hpp"
#include "mock_secure_random_source.hpp"
#include "mock_time_zone_resolver.hpp"
#include "simulated_persistent_state_store.hpp"

#include "virtual_time_source.hpp"

#include "../../main/fermentation_ui_press_dispatcher.hpp"

// Counting replacement of every global allocation function in this test
// executable. The count is only active between startCounting() and
// stopCounting(), so setup and warm-up are not measured.
namespace {
std::size_t gAllocationCount = 0U;
bool gCounting = false;

void startCounting() {
    gAllocationCount = 0U;
    gCounting = true;
}
std::size_t stopCounting() {
    gCounting = false;
    return gAllocationCount;
}

void* countedAllocate(std::size_t size) {
    if (gCounting) {
        ++gAllocationCount;
    }
    void* pointer = std::malloc(size == 0U ? 1U : size);
    if (pointer == nullptr) {
        throw std::bad_alloc();
    }
    return pointer;
}
void* countedAllocateAligned(std::size_t size, std::size_t alignment) {
    if (gCounting) {
        ++gAllocationCount;
    }
    void* pointer = nullptr;
    if (posix_memalign(&pointer,
                       alignment < sizeof(void*) ? sizeof(void*) : alignment,
                       size == 0U ? 1U : size) != 0) {
        throw std::bad_alloc();
    }
    return pointer;
}
}  // namespace

void* operator new(std::size_t size) { return countedAllocate(size); }
void* operator new[](std::size_t size) { return countedAllocate(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return countedAllocate(size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return countedAllocate(size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new(std::size_t size, std::align_val_t alignment) {
    return countedAllocateAligned(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
    return countedAllocateAligned(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}
void operator delete(void* pointer, std::align_val_t) noexcept {
    std::free(pointer);
}
void operator delete[](void* pointer, std::align_val_t) noexcept {
    std::free(pointer);
}
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}

namespace fermentation {

class FermentationApplicationTestAccess {
   public:
    static RunCommandState& runtimeState(FermentationApplication& application) {
        return *application.runtimeRunState_;
    }
};

}  // namespace fermentation

namespace {

using namespace fermentation;
using namespace fermentation::main_ui;

struct AppFixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;
    FermentationTouchWorkspace workspace;
    UiRenderGate gate;
    device_platform::LocaleId initialLocale{"en"};

    AppFixture() {
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    }

    // One loop step exactly as updateProductUi() does it without a touch
    // contact.
    bool step(
        std::optional<device_platform::DeviceUiTarget> pressed = std::nullopt,
        device_platform::DeviceUiNetworkStatus network =
            device_platform::DeviceUiNetworkStatus::Connected,
        std::optional<std::int64_t> utc = 1'700'000'000LL,
        const char* locale = nullptr) {
        if (locale != nullptr) {
            initialLocale = device_platform::LocaleId{locale};
        }
        gate.beginStep(application,
                       workspace.page() == FermentationUiPage::HeaderNetwork);
        return gate.renderRequired(application, workspace, pressed, network,
                                   utc);
    }

    // Warm up and mark the current state as rendered.
    void settle() {
        for (int loop = 0; loop < 3; ++loop) {
            if (step()) {
                gate.markRendered();
            }
        }
        TEST_ASSERT_FALSE(step());
    }
};

// Control: proves the counter is live and that the former per-loop pipeline
// (by-value snapshot) really allocated, so the zero above is a measurement.
void test_control_counter_detects_allocations_of_the_former_pipeline() {
    AppFixture fixture;
    fixture.settle();
    startCounting();
    {
        auto snapshot = fixture.application.uiSnapshot();
        static_cast<void>(snapshot);
    }
    const auto byValueSnapshot = stopCounting();
    TEST_ASSERT_TRUE(byValueSnapshot > 0U);

    startCounting();
    {
        auto* probe = new int(1);
        delete probe;
    }
    TEST_ASSERT_EQUAL_UINT32(1U, static_cast<std::uint32_t>(stopCounting()));
}

void test_unchanged_state_builds_no_render_and_allocates_nothing() {
    AppFixture fixture;
    fixture.settle();

    startCounting();
    bool redraw = false;
    for (int loop = 0; loop < 100; ++loop) {
        redraw = redraw || fixture.step();
    }
    const auto allocations = stopCounting();

    TEST_ASSERT_FALSE(redraw);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(allocations));
}

class MockHttpServerLifecycle final
    : public device_platform::IHttpServerLifecycle {
   public:
    [[nodiscard]] bool start(device_platform::IHttpRouteSink&) override {
        running_ = true;
        return true;
    }
    [[nodiscard]] bool stop() override {
        running_ = false;
        return true;
    }
    [[nodiscard]] bool running() const override { return running_; }

   private:
    bool running_{false};
};

// The real firmware wiring: an application with a network lifecycle whose
// SoftAP data (long, non-small-string SSID and password) is available, shown
// on HeaderNetwork.
struct NetworkFixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    device_platform_test_support::MockSecureRandomSource randomSource;
    MockHttpServerLifecycle http;
    FermentationApplication application;
    FermentationTouchWorkspace workspace;
    UiRenderGate gate;
    device_platform::LocaleId initialLocale{"en"};

    NetworkFixture() {
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                           timeSource, network, http,
                                           randomSource));
        TEST_ASSERT_TRUE(
            application.applyNetworkMode(device_platform::NetworkMode::AP_ONLY)
                .status == NetworkConfigurationStatus::Applied);
        workspace.setPage(FermentationUiPage::HeaderNetwork);
        const auto info = application.networkAccessPointInfo();
        TEST_ASSERT_TRUE(info.has_value());
        // Beyond the small-string buffer, so a copy really allocates.
        TEST_ASSERT_TRUE(info->ssid.size() > 15U);
        TEST_ASSERT_TRUE(info->password.size() > 15U);
    }

    bool step() {
        gate.beginStep(application,
                       workspace.page() == FermentationUiPage::HeaderNetwork);
        return gate.renderRequired(
            application, workspace, std::nullopt,
            device_platform::DeviceUiNetworkStatus::Connected, 1'700'000'000LL);
    }
    void settle() {
        for (int loop = 0; loop < 3; ++loop) {
            if (step()) {
                gate.markRendered();
            }
        }
        TEST_ASSERT_FALSE(step());
    }
    void restartAccessPoint() {
        TEST_ASSERT_TRUE(
            network.start(device_platform::NetworkMode::AP_ONLY, std::nullopt)
                .status == device_platform::NetworkOperationStatus::Applied);
    }
    void setCredentials(const std::string& ssid, const std::string& password) {
        TEST_ASSERT_TRUE(
            network.setAccessPointCredentials(ssid, password).status ==
            device_platform::NetworkOperationStatus::Applied);
    }
};

class WebAccessTestKdf final : public IAuthenticationKdf {
   public:
    bool derive(
        const std::string& secret, const AuthVerifier& parameters,
        std::array<std::uint8_t, kAuthenticationVerifierBytes>& out) override {
        std::uint32_t state = 2166136261U;
        for (const auto byte : secret) {
            state = (state ^ static_cast<std::uint8_t>(byte)) * 16777619U;
        }
        for (const auto byte : parameters.salt) {
            state = (state ^ byte) * 16777619U;
        }
        for (auto& byte : out) {
            state = state * 1664525U + 1013904223U;
            byte = static_cast<std::uint8_t>(state >> 24U);
        }
        return true;
    }
};

// Application with the authentication stack, showing the web access page.
struct WebAccessFixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    device_platform_test_support::MockNetworkLifecycle network;
    device_platform_test_support::MockSecureRandomSource randomSource;
    MockHttpServerLifecycle http;
    WebAccessTestKdf kdf;
    FermentationApplication application;
    FermentationTouchWorkspace workspace;
    UiRenderGate gate;
    device_platform::LocaleId initialLocale{"en"};

    WebAccessFixture() {
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver,
                                           timeSource, network, http,
                                           randomSource, kdf));
        workspace.setPage(FermentationUiPage::HeaderWebAccess);
    }

    bool step() {
        gate.beginStep(application, false);
        return gate.renderRequired(
            application, workspace, std::nullopt,
            device_platform::DeviceUiNetworkStatus::Connected, 1'700'000'000LL);
    }
    void settle() {
        for (int loop = 0; loop < 3; ++loop) {
            if (step()) {
                gate.markRendered();
            }
        }
        TEST_ASSERT_FALSE(step());
    }
};

void test_web_access_page_steady_state_allocates_nothing() {
    WebAccessFixture fixture;
    fixture.settle();
    TEST_ASSERT_TRUE(fixture.application.uiSnapshot().webAccess ==
                     FermentationWebAccessState::Closed);

    startCounting();
    bool redraw = false;
    for (int loop = 0; loop < 100; ++loop) {
        redraw = redraw || fixture.step();
    }
    const auto closedAllocations = stopCounting();
    TEST_ASSERT_FALSE(redraw);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(closedAllocations));

    // Releasing setup changes the visible state: one redraw, then quiet again
    // and still allocation free.
    TEST_ASSERT_TRUE(fixture.application.openWebProvisioningWindow());
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(fixture.step());
    startCounting();
    for (int loop = 0; loop < 100; ++loop) {
        redraw = redraw || fixture.step();
    }
    const auto openAllocations = stopCounting();
    TEST_ASSERT_FALSE(redraw);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(openAllocations));

    // Expiry is evaluated lazily by the Application and requests a redraw.
    fixture.timeSource.advanceMonotonicMillis(kWebProvisioningWindowMs);
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(fixture.step());
}

// S1: the program list page (content rows, pager window) is a steady state
// like the other pages: an unchanged list allocates nothing, and only a real
// visible event (held row, scrolled window) requests a redraw.
void test_program_list_page_steady_state_allocates_nothing_and_rows_redraw() {
    WebAccessFixture fixture;
    fixture.workspace.setPage(FermentationUiPage::ProgramList);
    fixture.settle();
    TEST_ASSERT_TRUE(fixture.gate.presentation().hasCopy());

    startCounting();
    bool redraw = false;
    for (int loop = 0; loop < 100; ++loop) {
        redraw = redraw || fixture.step();
    }
    const auto idleAllocations = stopCounting();
    TEST_ASSERT_FALSE(redraw);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(idleAllocations));

    // A held row changes the render key (kind, row, column).
    const device_platform::DeviceUiTarget row0{
        device_platform::DeviceUiTargetKind::ContentCell, 0U, 0U, 0U};
    const device_platform::DeviceUiTarget row1{
        device_platform::DeviceUiTargetKind::ContentCell, 0U, 1U, 0U};
    const auto redrawFor = [&fixture](
                               const device_platform::DeviceUiTarget& target) {
        fixture.gate.beginStep(fixture.application, false);
        return fixture.gate.renderRequired(
            fixture.application, fixture.workspace, target,
            device_platform::DeviceUiNetworkStatus::Connected, 1'700'000'000LL);
    };
    TEST_ASSERT_TRUE(redrawFor(row0));
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(redrawFor(row0));
    TEST_ASSERT_TRUE(redrawFor(row1));
    fixture.gate.markRendered();
    TEST_ASSERT_TRUE(redrawFor(row0));
    fixture.gate.markRendered();
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(fixture.step());
}

// S2: the message list page is a steady state too.
void test_message_list_page_steady_state_allocates_nothing() {
    WebAccessFixture fixture;
    fixture.workspace.setPage(FermentationUiPage::Messages);
    fixture.settle();
    startCounting();
    bool redraw = false;
    for (int loop = 0; loop < 100; ++loop) {
        redraw = redraw || fixture.step();
    }
    const auto allocations = stopCounting();
    TEST_ASSERT_FALSE(redraw);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(allocations));
    fixture.workspace.setPage(FermentationUiPage::MessageDetail);
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(fixture.step());
}

// S7: every read-only content page is a steady state too (no redraw, no
// allocation while nothing visible changes).
void test_s7_content_pages_steady_state_allocate_nothing() {
    const FermentationUiPage pages[] = {
        FermentationUiPage::ProgramSummary, FermentationUiPage::Process,
        FermentationUiPage::Technical,      FermentationUiPage::Completion,
        FermentationUiPage::Status,         FermentationUiPage::Diagnostics,
        FermentationUiPage::Service,        FermentationUiPage::Pin,
        FermentationUiPage::Recovery,
    };
    for (const auto page : pages) {
        WebAccessFixture fixture;
        fixture.workspace.setPage(FermentationUiPage::ProgramList);
        fixture.settle();
        TEST_ASSERT_TRUE(fixture.gate.presentation().hasCopy());
        if (page == FermentationUiPage::ProgramSummary) {
            const auto& catalog =
                fixture.gate.presentation().get().programCatalog;
            TEST_ASSERT_FALSE(catalog.programs.empty());
            TEST_ASSERT_TRUE(fixture.workspace.selectProgram(
                catalog.programs.front().program.id, catalog));
        } else {
            fixture.workspace.setPage(page);
        }
        TEST_ASSERT_TRUE(fixture.step());
        fixture.gate.markRendered();
        fixture.settle();
        TEST_ASSERT_TRUE(fixture.workspace.page() == page);
        startCounting();
        bool redraw = false;
        for (int loop = 0; loop < 100; ++loop) {
            redraw = redraw || fixture.step();
        }
        const auto allocations = stopCounting();
        TEST_ASSERT_FALSE(redraw);
        TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(allocations));
    }
}

// S8: the value edit page (keypad) is a steady state too. Its content is
// local workspace state, so only a workspace mutation (a key press) changes
// the render key; the key press path itself is covered with a startable
// catalog in test_local_touch_ui.
void test_value_edit_page_steady_state_allocates_nothing() {
    WebAccessFixture fixture;
    fixture.workspace.setPage(FermentationUiPage::ValueEdit);
    fixture.settle();
    startCounting();
    bool redraw = false;
    for (int loop = 0; loop < 100; ++loop) {
        redraw = redraw || fixture.step();
    }
    const auto allocations = stopCounting();
    TEST_ASSERT_FALSE(redraw);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(allocations));
    // A held keypad key or pager button (ContentCell, any column) changes
    // the render key once per distinct target and settles afterwards.
    const auto redrawFor = [&fixture](
                               const device_platform::DeviceUiTarget& target) {
        fixture.gate.beginStep(fixture.application, false);
        return fixture.gate.renderRequired(
            fixture.application, fixture.workspace, target,
            device_platform::DeviceUiNetworkStatus::Connected, 1'700'000'000LL);
    };
    const device_platform::DeviceUiTarget keyA{
        device_platform::DeviceUiTargetKind::ContentCell, 0U, 1U, 0U};
    const device_platform::DeviceUiTarget keyB{
        device_platform::DeviceUiTargetKind::ContentCell, 0U, 1U, 2U};
    TEST_ASSERT_TRUE(redrawFor(keyA));
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(redrawFor(keyA));
    TEST_ASSERT_TRUE(redrawFor(keyB));
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(redrawFor(keyB));
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    // A workspace mutation (what every key press does) redraws once.
    fixture.workspace.setProgramEditDirty(true);
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(fixture.step());
}

// S3: the language page is a steady state too.
void test_language_page_steady_state_allocates_nothing() {
    WebAccessFixture fixture;
    fixture.workspace.setPage(FermentationUiPage::HeaderLanguage);
    fixture.settle();
    startCounting();
    bool redraw = false;
    for (int loop = 0; loop < 100; ++loop) {
        redraw = redraw || fixture.step();
    }
    const auto allocations = stopCounting();
    TEST_ASSERT_FALSE(redraw);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(allocations));
}

void test_clock_page_steady_state_and_local_time_path_allocate_nothing() {
    WebAccessFixture fixture;
    fixture.workspace.setPage(FermentationUiPage::HeaderClock);
    fixture.settle();
    // The prepared zone rule reached the UI loop through the presentation
    // cache (not re-resolved), and converts without any allocation.
    const auto rule = fixture.gate.presentation().timeZoneRule();
    TEST_ASSERT_EQUAL(
        static_cast<int>(device_platform::DaylightSavingRule::EuropeanUnion),
        static_cast<int>(rule.dst));
    startCounting();
    bool redraw = false;
    std::uint32_t convertedHours = 0U;
    for (int loop = 0; loop < 100; ++loop) {
        redraw = redraw || fixture.step();
        const auto local = device_platform::toLocalTime(1'782'864'000LL, rule);
        convertedHours += local.has_value() ? local->hour : 99U;
    }
    const auto allocations = stopCounting();
    TEST_ASSERT_FALSE(redraw);
    TEST_ASSERT_EQUAL_UINT32(200U, convertedHours);  // 02:00 CEST, 100 times
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(allocations));
}

void test_web_access_page_keeps_the_presentation_copy_like_other_pages() {
    // Only HeaderNetwork evicts the program catalog copy (it does not consume
    // it); the web access page keeps the existing presentation contract.
    WebAccessFixture fixture;
    fixture.settle();
    TEST_ASSERT_TRUE(fixture.gate.presentation().hasCopy());
}

void test_header_network_with_real_access_point_data_allocates_nothing() {
    NetworkFixture fixture;
    fixture.settle();
    TEST_ASSERT_FALSE(fixture.gate.presentation().hasCopy());

    startCounting();
    bool redraw = false;
    for (int loop = 0; loop < 100; ++loop) {
        redraw = redraw || fixture.step();
    }
    const auto allocations = stopCounting();
    TEST_ASSERT_FALSE(redraw);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(allocations));
}

void test_control_by_value_access_point_fetch_allocates() {
    NetworkFixture fixture;
    fixture.settle();
    startCounting();
    {
        const auto info = fixture.application.networkAccessPointInfo();
        static_cast<void>(info);
    }
    TEST_ASSERT_TRUE(stopCounting() > 0U);
}

void test_access_point_changes_bump_the_revision_and_request_redraw() {
    NetworkFixture fixture;
    fixture.settle();
    const auto start = fixture.application.networkAccessPointRevision();

    // Starting again with unchanged data must not change the revision.
    fixture.restartAccessPoint();
    TEST_ASSERT_EQUAL_UINT64(start,
                             fixture.application.networkAccessPointRevision());
    TEST_ASSERT_FALSE(fixture.step());

    // SSID change.
    fixture.setCredentials("Fermentation-Setup-ssid-changed",
                           fixture.network.accessPointPassword());
    fixture.restartAccessPoint();
    const auto afterSsid = fixture.application.networkAccessPointRevision();
    TEST_ASSERT_TRUE(afterSsid != start);
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(fixture.step());

    // Password change.
    fixture.setCredentials(fixture.network.accessPointSsid(),
                           "rotated-password-0123456789");
    fixture.restartAccessPoint();
    const auto afterPassword = fixture.application.networkAccessPointRevision();
    TEST_ASSERT_TRUE(afterPassword != afterSsid);
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(fixture.step());

    // IPv4 change.
    fixture.network.setAccessPointAddress(0x0204A8C0U);
    fixture.restartAccessPoint();
    const auto afterAddress = fixture.application.networkAccessPointRevision();
    TEST_ASSERT_TRUE(afterAddress != afterPassword);
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(fixture.step());

    // Cleared (presence).
    static_cast<void>(fixture.network.stop());
    const auto afterClear = fixture.application.networkAccessPointRevision();
    TEST_ASSERT_TRUE(afterClear != afterAddress);
    TEST_ASSERT_FALSE(fixture.application.networkAccessPointInfo().has_value());
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(fixture.step());

    // Set again.
    fixture.restartAccessPoint();
    TEST_ASSERT_TRUE(fixture.application.networkAccessPointRevision() !=
                     afterClear);
    TEST_ASSERT_TRUE(fixture.step());
}

void test_redraw_after_access_point_change_still_shows_the_real_data() {
    NetworkFixture fixture;
    fixture.settle();
    fixture.setCredentials("Fermentation-Setup-ssid-changed",
                           "rotated-password-0123456789");
    fixture.restartAccessPoint();
    TEST_ASSERT_TRUE(fixture.step());
    // The redraw path fetches the actual data and the screen carries it,
    // including the Wi-Fi QR payload.
    const auto info = fixture.application.networkAccessPointInfo();
    TEST_ASSERT_TRUE(info.has_value());
    TEST_ASSERT_EQUAL_STRING("Fermentation-Setup-ssid-changed",
                             info->ssid.c_str());
    const auto packs = makeFermentationUiTextPacks();
    const auto screen = makeRepresentativeScreen(
        fixture.gate.snapshot(), fixture.workspace, packs,
        fixture.initialLocale, std::nullopt, nullptr,
        device_platform::DeviceUiNetworkStatus::Connected,
        {1'700'000'000LL, {}}, info);
    bool qrWithNewData = false;
    for (const auto& command : screen.commands) {
        if (command.kind == ScreenDrawKind::QrCode &&
            command.text.find("rotated-password-0123456789") !=
                std::string::npos &&
            command.text.find("Fermentation-Setup-ssid-changed") !=
                std::string::npos) {
            qrWithNewData = true;
        }
    }
    TEST_ASSERT_TRUE(qrWithNewData);
}

void test_active_run_with_long_run_id_allocates_nothing() {
    AppFixture fixture;
    auto& state =
        FermentationApplicationTestAccess::runtimeState(fixture.application);
    // R1 allows run ids up to 48 bytes, beyond the small-string buffer.
    state.activeRunId = std::string(48U, 'r');
    fixture.settle();
    TEST_ASSERT_EQUAL_UINT32(
        48U, static_cast<std::uint32_t>(
                 fixture.gate.snapshot().home.activeRunId.size()));

    startCounting();
    bool redraw = false;
    for (int loop = 0; loop < 100; ++loop) {
        redraw = redraw || fixture.step();
    }
    const auto allocations = stopCounting();
    TEST_ASSERT_FALSE(redraw);
    TEST_ASSERT_EQUAL_UINT32(0U, static_cast<std::uint32_t>(allocations));
    TEST_ASSERT_TRUE(equalFermentationUiSemanticSnapshot(
        fixture.gate.snapshot(), fixture.application.uiSnapshot()));

    // A different run id is a real visible-state change.
    state.activeRunId = std::string(40U, 's');
    state.runRevision += 1U;
    TEST_ASSERT_TRUE(fixture.step());
}

void test_recycled_snapshot_equals_a_fresh_snapshot() {
    AppFixture fixture;
    fixture.settle();
    const auto fresh = fixture.application.uiSnapshot();
    TEST_ASSERT_TRUE(
        equalFermentationUiSemanticSnapshot(fixture.gate.snapshot(), fresh));
    // Also after a state change and back-to-back refreshes.
    auto& state =
        FermentationApplicationTestAccess::runtimeState(fixture.application);
    state.messageCount = 1U;
    state.messages[0] = RuntimeMessage{};
    state.messages[0].id = 7U;
    state.messageRevision += 1U;
    fixture.gate.beginStep(fixture.application, false);
    fixture.gate.beginStep(fixture.application, false);
    TEST_ASSERT_TRUE(equalFermentationUiSemanticSnapshot(
        fixture.gate.snapshot(), fixture.application.uiSnapshot()));
    TEST_ASSERT_EQUAL_UINT32(1U, static_cast<std::uint32_t>(
                                     fixture.gate.snapshot().messages.size()));
}

// --- every visible input changes the key and requests a redraw ---

void test_application_state_change_requests_redraw() {
    AppFixture fixture;
    fixture.settle();
    auto& state =
        FermentationApplicationTestAccess::runtimeState(fixture.application);
    state.messageCount = 1U;
    state.messages[0] = RuntimeMessage{};
    state.messages[0].id = 9U;
    state.messageRevision += 1U;
    TEST_ASSERT_TRUE(fixture.step());
}

void test_workspace_page_change_requests_redraw() {
    AppFixture fixture;
    fixture.settle();
    fixture.workspace.setPage(FermentationUiPage::Messages);
    TEST_ASSERT_TRUE(fixture.step());
}

// D11: the language comes from the configuration, not from a boot-time copy.
// The HeaderNetwork eviction frees the catalog copy but must keep the language
// the user chose, so the network page is drawn in it.
void test_language_change_reaches_the_network_page_through_the_cache() {
    AppFixture fixture;
    fixture.settle();
    TEST_ASSERT_TRUE(fixture.gate.presentation().displayLocale().value() !=
                     "es");
    const auto revision =
        fixture.gate.snapshot().revisions.expectedUserConfigurationRevision;
    TEST_ASSERT_TRUE(revision.has_value());
    const auto changed =
        fixture.application.applyDisplayLanguage("es", revision);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(changed.commit));

    // The new revision refills the copy and requests a redraw.
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    TEST_ASSERT_EQUAL_STRING(
        "es", fixture.gate.presentation().displayLocale().value().c_str());

    // On HeaderNetwork the copy is evicted, the language stays German.
    fixture.workspace.setPage(FermentationUiPage::HeaderNetwork);
    TEST_ASSERT_TRUE(fixture.step());
    fixture.gate.markRendered();
    TEST_ASSERT_FALSE(fixture.gate.presentation().hasCopy());
    TEST_ASSERT_EQUAL_STRING(
        "es", fixture.gate.presentation().displayLocale().value().c_str());
    TEST_ASSERT_FALSE(fixture.step());
}

void test_pressed_target_change_requests_redraw() {
    AppFixture fixture;
    fixture.settle();
    TEST_ASSERT_TRUE(fixture.step(device_platform::DeviceUiTarget{
        device_platform::DeviceUiTargetKind::BottomSlot, 1U}));
    fixture.gate.markRendered();
    TEST_ASSERT_TRUE(fixture.step(device_platform::DeviceUiTarget{
        device_platform::DeviceUiTargetKind::BottomSlot, 2U}));
    fixture.gate.markRendered();
    TEST_ASSERT_TRUE(fixture.step(device_platform::DeviceUiTarget{
        device_platform::DeviceUiTargetKind::HeaderNetwork, 0U}));
    fixture.gate.markRendered();
    TEST_ASSERT_TRUE(fixture.step());  // release
}

void test_network_status_change_requests_redraw() {
    AppFixture fixture;
    fixture.settle();
    TEST_ASSERT_TRUE(fixture.step(
        std::nullopt, device_platform::DeviceUiNetworkStatus::Disconnected));
}

void test_clock_minute_change_requests_redraw_but_seconds_do_not() {
    AppFixture fixture;
    fixture.settle();
    TEST_ASSERT_FALSE(fixture.step(
        std::nullopt, device_platform::DeviceUiNetworkStatus::Connected,
        1'700'000'030LL));
    TEST_ASSERT_TRUE(fixture.step(
        std::nullopt, device_platform::DeviceUiNetworkStatus::Connected,
        1'700'000'000LL + 60));
    fixture.gate.markRendered();
    // Losing the trusted time changes the visible text ("--:--").
    TEST_ASSERT_TRUE(fixture.step(
        std::nullopt, device_platform::DeviceUiNetworkStatus::Connected,
        std::nullopt));
}

void test_catalog_revision_adoption_changes_the_key() {
    FermentationUiSnapshot snapshot;
    FermentationTouchWorkspace workspace;
    const device_platform::LocaleId locale{"en"};
    const auto fill = [] {
        return std::optional<FermentationUiPresentationSource>{
            FermentationUiPresentationSource{}};
    };
    FermentationUiPresentationCache first;
    FermentationUiExpectedRevisions revisionsOne;
    revisionsOne.expectedUserConfigurationRevision =
        UserConfigurationRevision{1U};
    revisionsOne.expectedProgramCatalogRevision = ProgramCatalogRevision{1U};
    first.update(false, revisionsOne, fill);
    FermentationUiPresentationCache second = first;
    auto revisionsTwo = revisionsOne;
    revisionsTwo.expectedProgramCatalogRevision = ProgramCatalogRevision{2U};
    second.update(false, revisionsTwo, fill);
    const auto keyOne = makeScreenRenderKey(
        snapshot, workspace, locale, std::nullopt, first,
        device_platform::DeviceUiNetworkStatus::Unavailable, std::nullopt, 0U);
    const auto keyTwo = makeScreenRenderKey(
        snapshot, workspace, locale, std::nullopt, second,
        device_platform::DeviceUiNetworkStatus::Unavailable, std::nullopt, 0U);
    TEST_ASSERT_FALSE(keyOne == keyTwo);
    // An evicted copy (HeaderNetwork) differs from any adopted catalog.
    FermentationUiPresentationCache evicted = first;
    evicted.evict();
    TEST_ASSERT_FALSE(
        keyOne ==
        makeScreenRenderKey(snapshot, workspace, locale, std::nullopt, evicted,
                            device_platform::DeviceUiNetworkStatus::Unavailable,
                            std::nullopt, 0U));
}

// --- every public workspace mutator bumps the render revision ---

void test_every_workspace_mutator_bumps_the_render_revision() {
    FermentationTouchWorkspace workspace;
    FermentationUiSnapshot snapshot;
    ProgramCatalog catalog;
    auto expectBump = [&workspace](auto&& mutate) {
        const auto before = workspace.renderRevision();
        mutate();
        TEST_ASSERT_NOT_EQUAL_UINT32(before, workspace.renderRevision());
    };
    expectBump([&] { workspace.setPage(FermentationUiPage::Messages); });
    expectBump([&] {
        static_cast<void>(workspace.press(
            snapshot,
            device_platform::DeviceUiTarget{
                device_platform::DeviceUiTargetKind::BottomSlot, 0U}));
    });
    expectBump(
        [&] { static_cast<void>(workspace.selectProgram("none", catalog)); });
    expectBump([&] { workspace.setStartCandidate({}); });
    expectBump([&] { workspace.setManualHoldingValues({}); });
    expectBump([&] { workspace.setManualTimedValues({}); });
    expectBump([&] { workspace.setCompletionCoolingPlan(std::nullopt); });
    expectBump([&] { workspace.setStopCoolingPlan(std::nullopt); });
    expectBump([&] { workspace.setSelectedMessage(3U); });
    expectBump([&] { workspace.setProgramEditCandidate(std::nullopt); });
    expectBump([&] {
        workspace.setProgramEditOperation(
            FermentationUiProgramEditOperation::Edit);
    });
    expectBump([&] { workspace.setSensorSelectionAction(std::nullopt); });
    expectBump([&] { workspace.setRecoveryTimeCorrectionSeconds(5U); });
    expectBump([&] { workspace.setProgramEditDirty(true); });
    expectBump([&] { static_cast<void>(workspace.movePagerDown()); });
    expectBump([&] { static_cast<void>(workspace.movePagerUp()); });
}

}  // namespace

void setUp() {}
void tearDown() {}

#include "../../main/fermentation_ui_press_dispatcher.cpp"
#include "../../main/fermentation_ui_renderer.cpp"

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_control_counter_detects_allocations_of_the_former_pipeline);
    RUN_TEST(test_unchanged_state_builds_no_render_and_allocates_nothing);
    RUN_TEST(test_header_network_with_real_access_point_data_allocates_nothing);
    RUN_TEST(test_control_by_value_access_point_fetch_allocates);
    RUN_TEST(test_access_point_changes_bump_the_revision_and_request_redraw);
    RUN_TEST(test_redraw_after_access_point_change_still_shows_the_real_data);
    RUN_TEST(test_active_run_with_long_run_id_allocates_nothing);
    RUN_TEST(test_recycled_snapshot_equals_a_fresh_snapshot);
    RUN_TEST(test_application_state_change_requests_redraw);
    RUN_TEST(test_workspace_page_change_requests_redraw);
    RUN_TEST(test_language_change_reaches_the_network_page_through_the_cache);
    RUN_TEST(test_pressed_target_change_requests_redraw);
    RUN_TEST(test_network_status_change_requests_redraw);
    RUN_TEST(test_clock_minute_change_requests_redraw_but_seconds_do_not);
    RUN_TEST(test_catalog_revision_adoption_changes_the_key);
    RUN_TEST(test_every_workspace_mutator_bumps_the_render_revision);
    RUN_TEST(test_web_access_page_steady_state_allocates_nothing);
    RUN_TEST(
        test_program_list_page_steady_state_allocates_nothing_and_rows_redraw);
    RUN_TEST(test_message_list_page_steady_state_allocates_nothing);
    RUN_TEST(test_clock_page_steady_state_and_local_time_path_allocate_nothing);
    RUN_TEST(test_language_page_steady_state_allocates_nothing);
    RUN_TEST(test_s7_content_pages_steady_state_allocate_nothing);
    RUN_TEST(test_value_edit_page_steady_state_allocates_nothing);
    RUN_TEST(test_web_access_page_keeps_the_presentation_copy_like_other_pages);
    return UNITY_END();
}
