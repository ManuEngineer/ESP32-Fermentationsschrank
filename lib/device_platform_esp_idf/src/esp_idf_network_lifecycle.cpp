#include "esp_idf_network_lifecycle.hpp"

#include <cstring>
#include <limits>
#include <utility>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"

namespace device_platform_esp_idf {
namespace {

constexpr std::size_t kMaximumSsidBytes = 32U;
constexpr std::size_t kMinimumPasswordBytes = 8U;
constexpr std::size_t kMaximumPasswordBytes = 63U;
constexpr TickType_t kCandidatePollTicks = pdMS_TO_TICKS(100U);
constexpr TickType_t kCandidateTimeoutTicks = pdMS_TO_TICKS(10000U);

bool copyBounded(std::uint8_t* destination, std::size_t capacity,
                 const std::string& value) {
    if (value.empty() || value.size() > capacity) {
        return false;
    }
    std::memcpy(destination, value.data(), value.size());
    if (value.size() < capacity) {
        destination[value.size()] = 0U;
    }
    return true;
}

}  // namespace

bool EspIdfNetworkLifecycle::validAccessPointConfig(
    const EspIdfNetworkLifecycleConfig& config) {
    return !config.softApSsid.empty() &&
           config.softApSsid.size() <= kMaximumSsidBytes &&
           config.softApPassword.size() >= kMinimumPasswordBytes &&
           config.softApPassword.size() <= kMaximumPasswordBytes &&
           !config.hostname.empty();
}

EspIdfNetworkLifecycle::~EspIdfNetworkLifecycle() {
    std::lock_guard<std::mutex> operationLock(operationMutex_);
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        reconnectAllowed_ = false;
        reconnectRequested_ = false;
        candidateTesting_ = false;
    }
    cleanupInitialization();
}

void EspIdfNetworkLifecycle::destroyDefaultNetifs() noexcept {
    if (accessPointNetif_ != nullptr) {
        esp_netif_destroy_default_wifi(accessPointNetif_);
        accessPointNetif_ = nullptr;
    }
    if (stationNetif_ != nullptr) {
        esp_netif_destroy_default_wifi(stationNetif_);
        stationNetif_ = nullptr;
    }
}

void EspIdfNetworkLifecycle::cleanupInitialization() noexcept {
    stopWifi();
    unregisterEventHandlers();
    if (wifiInitialized_) {
        static_cast<void>(esp_wifi_deinit());
        wifiInitialized_ = false;
    }
    if (mdnsInitialized_) {
        mdns_free();
        mdnsInitialized_ = false;
    }
    destroyDefaultNetifs();
    initialized_ = false;
    std::lock_guard<std::mutex> stateLock(stateMutex_);
    accessPointInfo_.reset();
    intentionalDisconnectsPending_ = 0U;
}

bool EspIdfNetworkLifecycle::ensureInitialized() {
    if (initialized_) {
        return true;
    }
    if (wifiInitialized_ || wifiStarted_ || mdnsInitialized_ ||
        stationNetif_ != nullptr || accessPointNetif_ != nullptr ||
        wifiEventHandler_ != nullptr || ipEventHandler_ != nullptr) {
        cleanupInitialization();
    }
    if (!validAccessPointConfig(config_)) {
        return false;
    }
    const esp_err_t netifStatus = esp_netif_init();
    if (netifStatus != ESP_OK && netifStatus != ESP_ERR_INVALID_STATE) {
        return false;
    }
    const esp_err_t eventStatus = esp_event_loop_create_default();
    if (eventStatus != ESP_OK && eventStatus != ESP_ERR_INVALID_STATE) {
        return false;
    }
    stationNetif_ = esp_netif_create_default_wifi_sta();
    accessPointNetif_ = esp_netif_create_default_wifi_ap();
    if (stationNetif_ == nullptr || accessPointNetif_ == nullptr) {
        cleanupInitialization();
        return false;
    }
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK) {
        cleanupInitialization();
        return false;
    }
    wifiInitialized_ = true;
    // The ESP-IDF adapter must never create a second persistent credential
    // store. cc0/record type 9 in IStateStore is the only durable truth.
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
        cleanupInitialization();
        return false;
    }
    if (mdns_init() != ESP_OK) {
        cleanupInitialization();
        return false;
    }
    mdnsInitialized_ = true;
    if (mdns_hostname_set(config_.hostname.c_str()) != ESP_OK) {
        cleanupInitialization();
        return false;
    }
    if (esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &EspIdfNetworkLifecycle::handleEvent,
            this, &wifiEventHandler_) != ESP_OK ||
        esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &EspIdfNetworkLifecycle::handleEvent,
            this, &ipEventHandler_) != ESP_OK) {
        cleanupInitialization();
        return false;
    }
    initialized_ = true;
    return true;
}

bool EspIdfNetworkLifecycle::configureAccessPoint() {
    wifi_config_t ap{};
    if (!copyBounded(ap.ap.ssid, sizeof(ap.ap.ssid), config_.softApSsid) ||
        !copyBounded(ap.ap.password, sizeof(ap.ap.password),
                     config_.softApPassword)) {
        return false;
    }
    ap.ap.ssid_len = static_cast<std::uint8_t>(config_.softApSsid.size());
    ap.ap.channel = 1U;
    ap.ap.max_connection = 4U;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    return esp_wifi_set_config(WIFI_IF_AP, &ap) == ESP_OK;
}

bool EspIdfNetworkLifecycle::configureStation(
    const device_platform::NetworkCredentials& credentials) {
    if (credentials.ssid.empty() ||
        credentials.ssid.size() > kMaximumSsidBytes ||
        credentials.password.size() < kMinimumPasswordBytes ||
        credentials.password.size() > kMaximumPasswordBytes) {
        return false;
    }
    wifi_config_t station{};
    if (!copyBounded(station.sta.ssid, sizeof(station.sta.ssid),
                     credentials.ssid) ||
        !copyBounded(station.sta.password, sizeof(station.sta.password),
                     credentials.password)) {
        return false;
    }
    return esp_wifi_set_config(WIFI_IF_STA, &station) == ESP_OK;
}

bool EspIdfNetworkLifecycle::startWifi() {
    if (wifiStarted_) {
        return true;
    }
    if (esp_wifi_start() != ESP_OK) {
        return false;
    }
    wifiStarted_ = true;
    return true;
}

bool EspIdfNetworkLifecycle::connectStationOnce() {
    return esp_wifi_connect() == ESP_OK;
}

bool EspIdfNetworkLifecycle::requestIntentionalDisconnect() noexcept {
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        if (intentionalDisconnectsPending_ ==
            std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        ++intentionalDisconnectsPending_;
    }
    if (esp_wifi_disconnect() == ESP_OK) {
        return true;
    }
    std::lock_guard<std::mutex> stateLock(stateMutex_);
    if (intentionalDisconnectsPending_ != 0U) {
        --intentionalDisconnectsPending_;
    }
    return false;
}

void EspIdfNetworkLifecycle::stopWifi() noexcept {
    if (!wifiStarted_) {
        return;
    }
    static_cast<void>(esp_wifi_stop());
    wifiStarted_ = false;
}

void EspIdfNetworkLifecycle::unregisterEventHandlers() noexcept {
    if (wifiEventHandler_ != nullptr) {
        static_cast<void>(esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEventHandler_));
        wifiEventHandler_ = nullptr;
    }
    if (ipEventHandler_ != nullptr) {
        static_cast<void>(esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, ipEventHandler_));
        ipEventHandler_ = nullptr;
    }
}

void EspIdfNetworkLifecycle::handleEvent(void* context,
                                         esp_event_base_t eventBase,
                                         std::int32_t eventId,
                                         void* eventData) {
    auto* self = static_cast<EspIdfNetworkLifecycle*>(context);
    if (self == nullptr) {
        return;
    }
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        std::lock_guard<std::mutex> stateLock(self->stateMutex_);
        if (self->intentionalDisconnectsPending_ != 0U) {
            --self->intentionalDisconnectsPending_;
            return;
        }
        self->status_.ipv4Address.reset();
        if (self->candidateTesting_) {
            self->candidateTestOutcome_ = CandidateTestOutcome::Failed;
            self->status_.state =
                device_platform::NetworkLifecycleState::Failed;
        } else if (self->reconnectAllowed_ &&
                   self->status_.selectedMode ==
                       device_platform::NetworkMode::HOME_WIFI &&
                   self->activeHomeCredentials_.has_value()) {
            self->reconnectRequested_ = true;
            self->status_.state =
                device_platform::NetworkLifecycleState::ConnectingHome;
        } else if (self->status_.state !=
                   device_platform::NetworkLifecycleState::Stopped) {
            self->status_.state =
                device_platform::NetworkLifecycleState::Failed;
        }
        return;
    }
    if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP &&
        eventData != nullptr) {
        const auto* event = static_cast<const ip_event_got_ip_t*>(eventData);
        std::lock_guard<std::mutex> stateLock(self->stateMutex_);
        self->status_.ipv4Address = event->ip_info.ip.addr;
        if (self->candidateTesting_) {
            self->candidateTestOutcome_ = CandidateTestOutcome::Connected;
        } else {
            self->status_.state =
                device_platform::NetworkLifecycleState::HomeConnected;
        }
    }
}

device_platform::NetworkOperationResult EspIdfNetworkLifecycle::start(
    device_platform::NetworkMode mode,
    const std::optional<device_platform::NetworkCredentials>& credentials) {
    std::lock_guard<std::mutex> operationLock(operationMutex_);
    if (!device_platform::isSelectableNetworkMode(mode) ||
        (mode == device_platform::NetworkMode::HOME_WIFI &&
         credentials.has_value() &&
         (credentials->ssid.empty() || credentials->password.empty()))) {
        return {device_platform::NetworkOperationStatus::InvalidInput};
    }
    if (!ensureInitialized()) {
        return {device_platform::NetworkOperationStatus::Failed};
    }
    bool stationWasActive = false;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        stationWasActive =
            activeHomeCredentials_.has_value() || candidateTesting_ ||
            status_.state ==
                device_platform::NetworkLifecycleState::ConnectingHome ||
            status_.state ==
                device_platform::NetworkLifecycleState::HomeConnected ||
            status_.state ==
                device_platform::NetworkLifecycleState::CandidateTesting;
        reconnectAllowed_ = false;
        reconnectRequested_ = false;
        candidateTesting_ = false;
        candidateTestOutcome_ = CandidateTestOutcome::None;
        status_.ipv4Address.reset();
        accessPointInfo_.reset();
    }
    if (stationWasActive && wifiStarted_) {
        static_cast<void>(requestIntentionalDisconnect());
    }
    if (mode == device_platform::NetworkMode::AP_ONLY ||
        !credentials.has_value()) {
        if (esp_wifi_set_mode(mode == device_platform::NetworkMode::AP_ONLY
                                  ? WIFI_MODE_AP
                                  : WIFI_MODE_APSTA) != ESP_OK ||
            !configureAccessPoint() || !startWifi()) {
            return {device_platform::NetworkOperationStatus::Failed};
        }
        esp_netif_ip_info_t ipInfo{};
        if (accessPointNetif_ == nullptr ||
            esp_netif_get_ip_info(accessPointNetif_, &ipInfo) != ESP_OK) {
            return {device_platform::NetworkOperationStatus::Failed};
        }
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        activeHomeCredentials_.reset();
        status_.selectedMode = mode;
        status_.state =
            mode == device_platform::NetworkMode::AP_ONLY
                ? device_platform::NetworkLifecycleState::AccessPointOnly
                : device_platform::NetworkLifecycleState::SetupAccessPoint;
        status_.ipv4Address = ipInfo.ip.addr;
        accessPointInfo_ = device_platform::NetworkAccessPointInfo{
            config_.softApSsid, config_.softApPassword, ipInfo.ip.addr};
        return {device_platform::NetworkOperationStatus::Applied};
    }
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
        !configureStation(*credentials) || !startWifi() ||
        !connectStationOnce()) {
        return {device_platform::NetworkOperationStatus::Failed};
    }
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        activeHomeCredentials_ = *credentials;
        reconnectAllowed_ = true;
        status_.selectedMode = mode;
        status_.state = device_platform::NetworkLifecycleState::ConnectingHome;
        accessPointInfo_.reset();
    }
    return {device_platform::NetworkOperationStatus::Applied};
}

device_platform::NetworkOperationResult EspIdfNetworkLifecycle::stop() {
    std::lock_guard<std::mutex> operationLock(operationMutex_);
    bool stationWasActive = false;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        stationWasActive =
            activeHomeCredentials_.has_value() || candidateTesting_ ||
            status_.state ==
                device_platform::NetworkLifecycleState::ConnectingHome ||
            status_.state ==
                device_platform::NetworkLifecycleState::HomeConnected ||
            status_.state ==
                device_platform::NetworkLifecycleState::CandidateTesting;
        reconnectAllowed_ = false;
        reconnectRequested_ = false;
        candidateTesting_ = false;
        candidateTestOutcome_ = CandidateTestOutcome::None;
        activeHomeCredentials_.reset();
        status_.state = device_platform::NetworkLifecycleState::Stopped;
        status_.httpReady = false;
        status_.ipv4Address.reset();
        accessPointInfo_.reset();
    }
    if (stationWasActive && wifiStarted_) {
        static_cast<void>(requestIntentionalDisconnect());
    }
    if (!initialized_) {
        cleanupInitialization();
    } else {
        // Keep the initialized ESP-IDF Wi-Fi/netif/event objects alive so a
        // later start reuses them and delayed intentional-disconnect events
        // still reach the same lifecycle generation.
        stopWifi();
    }
    return {device_platform::NetworkOperationStatus::Applied};
}

device_platform::NetworkScanResult EspIdfNetworkLifecycle::scan() {
    std::lock_guard<std::mutex> operationLock(operationMutex_);
    if (!initialized_ || !wifiStarted_) {
        return {device_platform::NetworkOperationStatus::Failed, {}};
    }
    wifi_scan_config_t config{};
    if (esp_wifi_scan_start(&config, true) != ESP_OK) {
        return {device_platform::NetworkOperationStatus::Failed, {}};
    }
    std::uint16_t count = 0U;
    if (esp_wifi_scan_get_ap_num(&count) != ESP_OK) {
        return {device_platform::NetworkOperationStatus::Failed, {}};
    }
    std::vector<wifi_ap_record_t> records(count);
    if (count != 0U &&
        esp_wifi_scan_get_ap_records(&count, records.data()) != ESP_OK) {
        return {device_platform::NetworkOperationStatus::Failed, {}};
    }
    std::vector<device_platform::NetworkScanEntry> entries;
    entries.reserve(count);
    for (const auto& record : records) {
        entries.push_back({reinterpret_cast<const char*>(record.ssid),
                           record.rssi, record.authmode != WIFI_AUTH_OPEN});
    }
    return {device_platform::NetworkOperationStatus::Applied,
            std::move(entries)};
}

device_platform::NetworkOperationResult EspIdfNetworkLifecycle::testCandidate(
    const device_platform::NetworkCredentials& candidate) {
    std::lock_guard<std::mutex> operationLock(operationMutex_);
    bool stationWasActive = false;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        stationWasActive =
            activeHomeCredentials_.has_value() || candidateTesting_ ||
            status_.state ==
                device_platform::NetworkLifecycleState::ConnectingHome ||
            status_.state ==
                device_platform::NetworkLifecycleState::HomeConnected ||
            status_.state ==
                device_platform::NetworkLifecycleState::CandidateTesting;
    }
    if (stationWasActive && wifiStarted_) {
        static_cast<void>(requestIntentionalDisconnect());
    }
    if (!initialized_ || esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK ||
        !configureAccessPoint() || !configureStation(candidate) ||
        !startWifi()) {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        status_.state = device_platform::NetworkLifecycleState::Failed;
        return {device_platform::NetworkOperationStatus::Failed};
    }
    esp_netif_ip_info_t ipInfo{};
    if (accessPointNetif_ == nullptr ||
        esp_netif_get_ip_info(accessPointNetif_, &ipInfo) != ESP_OK) {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        status_.state = device_platform::NetworkLifecycleState::Failed;
        return {device_platform::NetworkOperationStatus::Failed};
    }
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        accessPointInfo_ = device_platform::NetworkAccessPointInfo{
            config_.softApSsid, config_.softApPassword, ipInfo.ip.addr};
        reconnectAllowed_ = false;
        reconnectRequested_ = false;
        candidateTesting_ = true;
        candidateTestOutcome_ = CandidateTestOutcome::None;
        status_.state =
            device_platform::NetworkLifecycleState::CandidateTesting;
        status_.ipv4Address.reset();
    }
    if (!connectStationOnce()) {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        candidateTesting_ = false;
        candidateTestOutcome_ = CandidateTestOutcome::Failed;
        status_.state = device_platform::NetworkLifecycleState::Failed;
        return {device_platform::NetworkOperationStatus::Failed};
    }

    TickType_t waited = 0U;
    while (waited < kCandidateTimeoutTicks) {
        {
            std::lock_guard<std::mutex> stateLock(stateMutex_);
            if (candidateTestOutcome_ == CandidateTestOutcome::Connected) {
                candidateTesting_ = false;
                status_.state =
                    device_platform::NetworkLifecycleState::HomeConnected;
                // APSTA remains active until the app has resolved the commit.
                return {device_platform::NetworkOperationStatus::Applied};
            }
            if (candidateTestOutcome_ == CandidateTestOutcome::Failed) {
                candidateTesting_ = false;
                return {device_platform::NetworkOperationStatus::Failed};
            }
        }
        vTaskDelay(kCandidatePollTicks);
        waited += kCandidatePollTicks;
    }
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        candidateTesting_ = false;
        candidateTestOutcome_ = CandidateTestOutcome::Failed;
        status_.state = device_platform::NetworkLifecycleState::Failed;
    }
    static_cast<void>(requestIntentionalDisconnect());
    return {device_platform::NetworkOperationStatus::Failed};
}

device_platform::NetworkOperationResult EspIdfNetworkLifecycle::setHostname(
    const std::string& hostname) {
    std::lock_guard<std::mutex> operationLock(operationMutex_);
    if (hostname.empty() || hostname.size() > 63U) {
        return {device_platform::NetworkOperationStatus::InvalidInput};
    }
    if (initialized_) {
        return {device_platform::NetworkOperationStatus::Busy};
    }
    config_.hostname = hostname;
    return {device_platform::NetworkOperationStatus::Applied};
}

device_platform::NetworkStatus EspIdfNetworkLifecycle::status() const {
    std::lock_guard<std::mutex> stateLock(stateMutex_);
    return status_;
}

std::optional<device_platform::NetworkAccessPointInfo>
EspIdfNetworkLifecycle::accessPointInfo() const {
    std::lock_guard<std::mutex> stateLock(stateMutex_);
    return accessPointInfo_;
}

void EspIdfNetworkLifecycle::poll() {
    std::lock_guard<std::mutex> operationLock(operationMutex_);
    std::optional<device_platform::NetworkCredentials> reconnectCredentials;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        if (!reconnectRequested_ || !reconnectAllowed_ || candidateTesting_ ||
            !activeHomeCredentials_.has_value() || !wifiStarted_) {
            return;
        }
        reconnectRequested_ = false;
        reconnectCredentials = activeHomeCredentials_;
    }
    // The event callback only schedules this action. The app task performs
    // the one reconnect call under the lifecycle operation lock.
    if (!reconnectCredentials.has_value() || !connectStationOnce()) {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        status_.state = device_platform::NetworkLifecycleState::Failed;
    }
}

}  // namespace device_platform_esp_idf
