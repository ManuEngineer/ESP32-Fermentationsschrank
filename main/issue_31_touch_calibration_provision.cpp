#include "issue_31_touch_calibration_provision.hpp"

#if defined(APP_ISSUE_31_TOUCH_CALIBRATION_PROVISIONER)

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs_state_store.hpp"
#include "touch_calibration.hpp"

namespace fermentation::issue_31_touch_calibration_provision {
namespace {

constexpr char kTag[] = "issue31_provision";
constexpr char kStateStorePartitionLabel[] = "state_store";
constexpr char kStateStoreNamespace[] = "fermentation";
constexpr char kBoardControllerId[] =
    "esp32_32e_quad_mosfet_r1+ili9341+xpt2046";

// These are the reviewed affine coefficients derived from the committed
// Rotate90 capture. They intentionally exist only in this explicit,
// bring-up-only provisioning source; TouchCalibrationModel has no defaults.
const device_platform::TouchCalibrationModel kReviewedModel{
    0.00009043639686374949, -0.08753007024919984,  344.1462734722118,
    0.06559259340326797,    0.0007485020274466806, -15.832052617145878,
    kBoardControllerId};

class NvsPartitionLifetime final {
   public:
    explicit NvsPartitionLifetime(const char* partitionLabel)
        : partitionLabel_(partitionLabel) {}

    ~NvsPartitionLifetime() {
        if (initialized_) {
            const auto status = nvs_flash_deinit_partition(partitionLabel_);
            if (status != ESP_OK) {
                ESP_LOGE(kTag, "NVS_DEINIT=FAIL status=%s",
                         esp_err_to_name(status));
            }
        }
    }

    NvsPartitionLifetime(const NvsPartitionLifetime&) = delete;
    NvsPartitionLifetime& operator=(const NvsPartitionLifetime&) = delete;

    void markInitialized() noexcept { initialized_ = true; }

   private:
    const char* partitionLabel_;
    bool initialized_{false};
};

[[nodiscard]] const char* loadStatusName(
    device_platform::TouchCalibrationLoadStatus status) noexcept {
    using device_platform::TouchCalibrationLoadStatus;
    switch (status) {
        case TouchCalibrationLoadStatus::Available:
            return "Available";
        case TouchCalibrationLoadStatus::NotFound:
            return "NotFound";
        case TouchCalibrationLoadStatus::OtherEpoch:
            return "OtherEpoch";
        case TouchCalibrationLoadStatus::UnsupportedSchema:
            return "UnsupportedSchema";
        case TouchCalibrationLoadStatus::InvalidRecord:
            return "InvalidRecord";
        case TouchCalibrationLoadStatus::ReadError:
            return "ReadError";
        case TouchCalibrationLoadStatus::CapacityError:
            return "CapacityError";
    }
    return "Unknown";
}

[[nodiscard]] bool isExactReviewedRecord(
    const device_platform::TouchCalibrationLoadResult& loaded) noexcept {
    return loaded.status ==
               device_platform::TouchCalibrationLoadStatus::Available &&
           loaded.record.has_value() && loaded.record->recordSequence == 1U &&
           loaded.record->model == kReviewedModel;
}

void logFailure(const char* reason) noexcept {
    ESP_LOGE(kTag,
             "ISSUE31_CALIBRATION_PROVISION=FAILED reason=%s "
             "ACTUATOR_RELEASE=NO",
             reason);
}

}  // namespace

void run() noexcept {
    ESP_LOGI(kTag,
             "ISSUE31_CALIBRATION_PROVISION=READY "
             "ACTUATOR_RELEASE=NO FALLBACK_SLOT=UNCHANGED_BY_DESIGN");

    const auto config = device_platform_esp_idf::NvsStateStoreConfig::create(
        kStateStorePartitionLabel, kStateStoreNamespace);
    if (!config.has_value()) {
        logFailure("INVALID_NVS_CONFIGURATION");
        return;
    }

    NvsPartitionLifetime partitionLifetime(kStateStorePartitionLabel);
    const auto initStatus =
        nvs_flash_init_partition(config->partitionLabel().c_str());
    if (initStatus != ESP_OK) {
        logFailure("NVS_PARTITION_INIT");
        return;
    }
    partitionLifetime.markInitialized();

    auto opened = device_platform_esp_idf::NvsStateStore::open(*config);
    if (opened.status != ESP_OK || opened.store == nullptr) {
        logFailure("NVS_STORE_OPEN");
        return;
    }

    device_platform::TouchCalibrationStore calibration(*opened.store);
    const auto active = calibration.load(
        device_platform::TouchCalibrationSlot::Active, kBoardControllerId);
    ESP_LOGI(kTag, "ISSUE31_CALIBRATION_ACTIVE_STATUS=%s",
             loadStatusName(active.status));

    if (active.status ==
        device_platform::TouchCalibrationLoadStatus::NotFound) {
        ESP_LOGI(kTag, "ISSUE31_CALIBRATION_ACTIVE_PRESTATE=NOT_FOUND");
        const auto write = calibration.write(
            device_platform::TouchCalibrationSlot::Active, kReviewedModel, 1U);
        if (write.status !=
            device_platform::TouchCalibrationWriteStatus::Committed) {
            logFailure("ACTIVE_WRITE_NOT_COMMITTED");
            return;
        }
        ESP_LOGI(kTag, "ISSUE31_CALIBRATION_WRITE=COMMITTED");
    } else if (active.status ==
                   device_platform::TouchCalibrationLoadStatus::Available &&
               isExactReviewedRecord(active)) {
        ESP_LOGI(kTag, "ISSUE31_CALIBRATION_ACTIVE_PRESTATE=MATCHING");
        ESP_LOGI(kTag, "ISSUE31_CALIBRATION_WRITE=NOT_NEEDED");
    } else if (active.status ==
               device_platform::TouchCalibrationLoadStatus::Available) {
        logFailure("ACTIVE_RECORD_CONFLICT_NOT_OVERWRITTEN");
        return;
    } else {
        logFailure("ACTIVE_RECORD_STATUS_NOT_WRITABLE");
        return;
    }

    const auto readback = calibration.load(
        device_platform::TouchCalibrationSlot::Active, kBoardControllerId);
    if (!isExactReviewedRecord(readback)) {
        logFailure("ACTIVE_READBACK_MISMATCH");
        return;
    }
    ESP_LOGI(kTag, "ISSUE31_CALIBRATION_READBACK=PASS");
    ESP_LOGI(kTag,
             "ISSUE31_CALIBRATION_PROVISION=COMPLETE "
             "ACTUATOR_RELEASE=NO FALLBACK_SLOT=UNCHANGED");
}

}  // namespace fermentation::issue_31_touch_calibration_provision

#endif
