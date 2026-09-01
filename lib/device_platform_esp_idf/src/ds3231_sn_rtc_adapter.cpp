#include "ds3231_sn_rtc_adapter.hpp"

#include <ctime>
#include <new>

#include "ds3231.h"
#include "i2cdev.h"

namespace device_platform_esp_idf {
namespace {

constexpr std::uint8_t kRtcAddress = 0x68U;
constexpr std::uint8_t kTimeRegister = 0x00U;
constexpr std::uint8_t kControlRegister = 0x0EU;
constexpr std::uint8_t kStatusRegister = 0x0FU;

}  // namespace

struct Ds3231SnRtcAdapter::RtcDevice {
    i2c_dev_t value{};
};

Ds3231SnRtcAdapter::Ds3231SnRtcAdapter(EspIdfI2cSubsystem& i2c) noexcept
    : i2c_(i2c) {}

Ds3231SnRtcAdapter::~Ds3231SnRtcAdapter() {
    // The object cannot retry after destruction.  If i2cdev still owns the
    // descriptor after a failed cleanup, deliberately leak/quarantine its
    // backing storage rather than let unique_ptr create a dangling registry
    // entry.  The retained project claim still blocks i2cdev_done().
    static_cast<void>(shutdownImpl(true));
}

esp_err_t Ds3231SnRtcAdapter::initialize(
    const Ds3231SnRtcConfig& config) noexcept {
    if (initialized_ || device_ != nullptr || portClaimed_)
        return ESP_ERR_INVALID_STATE;
    config_ = config;
    if (!config_.present) return ESP_OK;
    if (config_.address != kRtcAddress || !i2c_.initialized() ||
        config_.sdaPin < 0 || config_.sclPin < 0 || config_.bus < I2C_NUM_0 ||
        config_.bus >= I2C_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!i2c_.claimPort(config_.bus, config_.sdaPin, config_.sclPin))
        return ESP_ERR_INVALID_ARG;
    portClaimed_ = true;

    auto descriptor =
        std::unique_ptr<RtcDevice>(new (std::nothrow) RtcDevice());
    if (descriptor == nullptr) {
        i2c_.releasePort(config_.bus, config_.sdaPin, config_.sclPin);
        portClaimed_ = false;
        return ESP_ERR_NO_MEM;
    }
    const auto initStatus = ds3231_init_desc(
        &descriptor->value, static_cast<i2c_port_t>(config_.bus),
        static_cast<gpio_num_t>(config_.sdaPin),
        static_cast<gpio_num_t>(config_.sclPin));
    if (initStatus != ESP_OK) {
        i2c_.releasePort(config_.bus, config_.sdaPin, config_.sclPin);
        portClaimed_ = false;
        return initStatus;
    }
    device_ = std::move(descriptor);
    if (i2c_dev_check_present(&device_->value) != ESP_OK) {
        const auto cleanupStatus = shutdownImpl(false);
        return cleanupStatus == ESP_OK ? ESP_ERR_NOT_FOUND : cleanupStatus;
    }
    initialized_ = true;
    if (!ensureHealthControls()) {
        const auto cleanupStatus = shutdownImpl(false);
        return cleanupStatus == ESP_OK ? ESP_FAIL : cleanupStatus;
    }
    return ESP_OK;
}

esp_err_t Ds3231SnRtcAdapter::shutdown() noexcept {
    return shutdownImpl(false);
}

esp_err_t Ds3231SnRtcAdapter::shutdownImpl(
    const bool quarantineOnFailure) noexcept {
    if (device_ == nullptr)
        return portClaimed_ ? ESP_ERR_INVALID_STATE : ESP_OK;

    // A requested shutdown makes normal adapter operations unavailable even
    // when upstream cleanup fails.  The descriptor and claim remain owned for
    // an explicit retry, or are quarantined by the destructor path.
    initialized_ = false;
    internal::DescriptorCleanupBackend backend{
        this, &Ds3231SnRtcAdapter::backendFreeDescriptor,
        &Ds3231SnRtcAdapter::backendDestroyDescriptor,
        &Ds3231SnRtcAdapter::backendReleasePortClaim,
        &Ds3231SnRtcAdapter::backendQuarantineDescriptor};
    return static_cast<esp_err_t>(
        internal::cleanupDescriptor(backend, quarantineOnFailure));
}

esp_err_t Ds3231SnRtcAdapter::readRaw(RawRegisters& registers) noexcept {
    if (!initialized_ || device_ == nullptr) return ESP_ERR_INVALID_STATE;
    auto status = i2c_dev_take_mutex(&device_->value);
    if (status != ESP_OK) return status;
    status =
        i2c_dev_read_reg(&device_->value, kTimeRegister,
                         registers.calendar.data(), registers.calendar.size());
    if (status == ESP_OK)
        status = i2c_dev_read_reg(&device_->value, kControlRegister,
                                  &registers.control, 1U);
    if (status == ESP_OK)
        status = i2c_dev_read_reg(&device_->value, kStatusRegister,
                                  &registers.status, 1U);
    const auto giveStatus = i2c_dev_give_mutex(&device_->value);
    return status == ESP_OK ? giveStatus : status;
}

esp_err_t Ds3231SnRtcAdapter::writeControl(const std::uint8_t value) noexcept {
    if (!initialized_ || device_ == nullptr) return ESP_ERR_INVALID_STATE;
    internal::I2cMutexWriteBackend backend{
        &device_->value,
        [](void* context) {
            return static_cast<int>(
                i2c_dev_take_mutex(static_cast<i2c_dev_t*>(context)));
        },
        [](void* context, const std::uint8_t controlValue) {
            return static_cast<int>(
                i2c_dev_write_reg(static_cast<i2c_dev_t*>(context),
                                  kControlRegister, &controlValue, 1U));
        },
        [](void* context) {
            return static_cast<int>(
                i2c_dev_give_mutex(static_cast<i2c_dev_t*>(context)));
        }};
    return static_cast<esp_err_t>(
        internal::writeRegisterWithMutex(backend, value));
}

bool Ds3231SnRtcAdapter::ensureHealthControls() noexcept {
    RawRegisters registers;
    if (readRaw(registers) != ESP_OK) return false;
    const auto originalRs = registers.control & internal::kDs3231RsMask;
    if ((registers.control & internal::kDs3231EoscMask) != 0U) {
        if (writeControl(internal::clearEoscPreservingControlBits(
                registers.control)) != ESP_OK)
            return false;
        if (readRaw(registers) != ESP_OK ||
            (registers.control & internal::kDs3231EoscMask) != 0U ||
            (registers.control & internal::kDs3231RsMask) != originalRs)
            return false;
    }
    // Use the adopted library operation for the public R1 feature, then use
    // the narrow raw shim only to prove the exact DS3231SN register contract.
    if (ds3231_disable_32khz(&device_->value) != ESP_OK) return false;
    if (readRaw(registers) != ESP_OK) return false;
    return (registers.control & internal::kDs3231EoscMask) == 0U &&
           (registers.status & internal::kDs3231En32khzMask) == 0U &&
           (registers.control & internal::kDs3231RsMask) == originalRs;
}

std::optional<std::int64_t> Ds3231SnRtcAdapter::readTrustedUtc() noexcept {
    if (!initialized_) return std::nullopt;
    RawRegisters registers;
    // The raw burst is the authoritative coherent trust sample.  The
    // adopted library API is checked separately by its compatibility gate;
    // a second library read here could cross a one-second RTC boundary.
    if (readRaw(registers) != ESP_OK || !internal::validateRaw(registers))
        return std::nullopt;
    return internal::rawCalendarToUnix(registers.calendar);
}

bool Ds3231SnRtcAdapter::backendSetTime(void* context,
                                        const ::tm& value) noexcept {
    auto* adapter = static_cast<Ds3231SnRtcAdapter*>(context);
    if (adapter == nullptr || adapter->device_ == nullptr) return false;
    ::tm writableValue = value;
    return ds3231_set_time(&adapter->device_->value, &writableValue) == ESP_OK;
}

bool Ds3231SnRtcAdapter::backendReadRaw(
    void* context, internal::Ds3231SnRawRegisters& registers) noexcept {
    auto* adapter = static_cast<Ds3231SnRtcAdapter*>(context);
    return adapter != nullptr && adapter->readRaw(registers) == ESP_OK;
}

bool Ds3231SnRtcAdapter::backendWriteControl(
    void* context, const std::uint8_t value) noexcept {
    auto* adapter = static_cast<Ds3231SnRtcAdapter*>(context);
    return adapter != nullptr && adapter->writeControl(value) == ESP_OK;
}

bool Ds3231SnRtcAdapter::backendDisable32khz(void* context) noexcept {
    auto* adapter = static_cast<Ds3231SnRtcAdapter*>(context);
    return adapter != nullptr && adapter->device_ != nullptr &&
           ds3231_disable_32khz(&adapter->device_->value) == ESP_OK;
}

bool Ds3231SnRtcAdapter::backendReadOsf(void* context, bool& osf) noexcept {
    auto* adapter = static_cast<Ds3231SnRtcAdapter*>(context);
    return adapter != nullptr && adapter->device_ != nullptr &&
           ds3231_get_oscillator_stop_flag(&adapter->device_->value, &osf) ==
               ESP_OK;
}

bool Ds3231SnRtcAdapter::backendClearOsf(void* context) noexcept {
    auto* adapter = static_cast<Ds3231SnRtcAdapter*>(context);
    return adapter != nullptr && adapter->device_ != nullptr &&
           ds3231_clear_oscillator_stop_flag(&adapter->device_->value) ==
               ESP_OK;
}

int Ds3231SnRtcAdapter::backendFreeDescriptor(void* context) noexcept {
    auto* adapter = static_cast<Ds3231SnRtcAdapter*>(context);
    if (adapter == nullptr || adapter->device_ == nullptr) return -1;
    return static_cast<int>(ds3231_free_desc(&adapter->device_->value));
}

void Ds3231SnRtcAdapter::backendDestroyDescriptor(void* context) noexcept {
    auto* adapter = static_cast<Ds3231SnRtcAdapter*>(context);
    if (adapter != nullptr) adapter->device_.reset();
}

void Ds3231SnRtcAdapter::backendReleasePortClaim(void* context) noexcept {
    auto* adapter = static_cast<Ds3231SnRtcAdapter*>(context);
    if (adapter == nullptr || !adapter->portClaimed_) return;
    adapter->i2c_.releasePort(adapter->config_.bus, adapter->config_.sdaPin,
                              adapter->config_.sclPin);
    adapter->portClaimed_ = false;
}

void Ds3231SnRtcAdapter::backendQuarantineDescriptor(void* context) noexcept {
    auto* adapter = static_cast<Ds3231SnRtcAdapter*>(context);
    if (adapter != nullptr) {
        // No owner may destroy this storage while i2cdev's registry is
        // unresolved.  The retained port claim prevents composition shutdown.
        static_cast<void>(adapter->device_.release());
    }
}

bool Ds3231SnRtcAdapter::synchronizeFromSystemUtc(
    const std::int64_t utcUnixSeconds) noexcept {
    if (!initialized_) return false;
    internal::Ds3231SnSyncBackend backend{
        this,
        &Ds3231SnRtcAdapter::backendSetTime,
        &Ds3231SnRtcAdapter::backendReadRaw,
        &Ds3231SnRtcAdapter::backendWriteControl,
        &Ds3231SnRtcAdapter::backendDisable32khz,
        &Ds3231SnRtcAdapter::backendReadOsf,
        &Ds3231SnRtcAdapter::backendClearOsf};
    return internal::synchronizeDs3231FromSystemUtc(backend, utcUnixSeconds);
}

}  // namespace device_platform_esp_idf
