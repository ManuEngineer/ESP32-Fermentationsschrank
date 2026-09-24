#include <unity.h>

#include <cmath>
#include <limits>
#include <string>

#include "simulated_persistent_state_store.hpp"
#include "storage_envelope.hpp"
#include "touch_calibration.hpp"

namespace {

using device_platform::CalibratedTouchPoint;
using device_platform::TouchCalibrationLoadStatus;
using device_platform::TouchCalibrationModel;
using device_platform::TouchCalibrationSlot;
using device_platform::TouchCalibrationStore;
using device_platform::TouchCalibrationWriteStatus;
using device_platform_test_support::SimulatedPersistentStateStore;

constexpr char kBoardId[] = "esp32_32e_quad_mosfet_r1+ili9341+xpt2046";

// A fully, explicitly measured-form fixture that happens to be the
// identity transform. It must set every coefficient itself: a
// default-constructed TouchCalibrationModel is deliberately unmeasured
// (NaN) and must never be mistaken for a valid identity calibration.
TouchCalibrationModel measuredFixtureModel() {
    TouchCalibrationModel model;
    model.a = 1.0;
    model.b = 0.0;
    model.c = 0.0;
    model.d = 0.0;
    model.e = 1.0;
    model.f = 0.0;
    model.boardControllerId = kBoardId;
    return model;
}

TouchCalibrationModel affineModel() {
    TouchCalibrationModel model;
    model.a = 0.5;
    model.b = 0.0;
    model.c = 10.0;
    model.d = 0.0;
    model.e = 0.25;
    model.f = -3.0;
    model.boardControllerId = kBoardId;
    return model;
}

void test_apply_identity_model_returns_raw_coordinates() {
    const auto model = measuredFixtureModel();
    const CalibratedTouchPoint point =
        device_platform::applyTouchCalibration(model, 100U, 200U);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, point.x);
    TEST_ASSERT_EQUAL_DOUBLE(200.0, point.y);
}

void test_apply_affine_model_scales_and_offsets() {
    const auto model = affineModel();
    const CalibratedTouchPoint point =
        device_platform::applyTouchCalibration(model, 100U, 40U);
    TEST_ASSERT_EQUAL_DOUBLE(60.0, point.x);  // 0.5*100 + 10
    TEST_ASSERT_EQUAL_DOUBLE(7.0, point.y);   // 0.25*40 - 3
}

void test_well_formed_requires_matching_board_controller_id() {
    const auto model = measuredFixtureModel();
    TEST_ASSERT_TRUE(
        device_platform::touchCalibrationModelIsWellFormed(model, kBoardId));
    TEST_ASSERT_FALSE(device_platform::touchCalibrationModelIsWellFormed(
        model, "some-other-board"));
    TEST_ASSERT_FALSE(
        device_platform::touchCalibrationModelIsWellFormed(model, ""));
}

void test_well_formed_rejects_non_finite_coefficients() {
    auto model = measuredFixtureModel();
    model.a = std::numeric_limits<double>::quiet_NaN();
    TEST_ASSERT_FALSE(
        device_platform::touchCalibrationModelIsWellFormed(model, kBoardId));
    model.a = std::numeric_limits<double>::infinity();
    TEST_ASSERT_FALSE(
        device_platform::touchCalibrationModelIsWellFormed(model, kBoardId));
}

void test_codec_roundtrip_preserves_model() {
    const auto model = affineModel();
    std::string payload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::TouchCalibrationCodecStatus::Success),
        static_cast<int>(
            device_platform::encodeTouchCalibrationPayload(model, payload)));

    const auto decoded =
        device_platform::decodeTouchCalibrationPayload(payload);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::TouchCalibrationCodecStatus::Success),
        static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.model.has_value());
    TEST_ASSERT_TRUE(*decoded.model == model);
}

void test_codec_rejects_oversized_board_controller_id() {
    auto model = measuredFixtureModel();
    model.boardControllerId = std::string(
        device_platform::kMaximumTouchCalibrationBoardControllerIdBytes + 1U,
        'x');
    std::string payload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::TouchCalibrationCodecStatus::InvalidWireValue),
        static_cast<int>(
            device_platform::encodeTouchCalibrationPayload(model, payload)));
}

void test_store_load_without_prior_write_is_not_found() {
    SimulatedPersistentStateStore store;
    TouchCalibrationStore calibration(store);
    const auto result =
        calibration.load(TouchCalibrationSlot::Active, kBoardId);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationLoadStatus::NotFound),
        static_cast<int>(result.status));
}

void test_store_write_then_load_active_returns_committed_model() {
    SimulatedPersistentStateStore store;
    TouchCalibrationStore calibration(store);
    const auto model = affineModel();
    const auto written = calibration.write(TouchCalibrationSlot::Active, model,
                                           /*recordSequence=*/1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationWriteStatus::Committed),
        static_cast<int>(written.status));

    const auto loaded =
        calibration.load(TouchCalibrationSlot::Active, kBoardId);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationLoadStatus::Available),
        static_cast<int>(loaded.status));
    TEST_ASSERT_TRUE(loaded.record.has_value());
    TEST_ASSERT_TRUE(loaded.record->model == model);
    TEST_ASSERT_EQUAL_UINT64(1U, loaded.record->recordSequence);
}

void test_store_active_and_fallback_slots_are_independent() {
    SimulatedPersistentStateStore store;
    TouchCalibrationStore calibration(store);
    auto active = measuredFixtureModel();
    auto fallback = affineModel();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationWriteStatus::Committed),
        static_cast<int>(
            calibration.write(TouchCalibrationSlot::Active, active, 1U)
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationWriteStatus::Committed),
        static_cast<int>(
            calibration.write(TouchCalibrationSlot::Fallback, fallback, 1U)
                .status));

    const auto loadedActive =
        calibration.load(TouchCalibrationSlot::Active, kBoardId);
    const auto loadedFallback =
        calibration.load(TouchCalibrationSlot::Fallback, kBoardId);
    TEST_ASSERT_TRUE(loadedActive.record.has_value());
    TEST_ASSERT_TRUE(loadedFallback.record.has_value());
    TEST_ASSERT_TRUE(loadedActive.record->model == active);
    TEST_ASSERT_TRUE(loadedFallback.record->model == fallback);
}

void test_store_load_rejects_mismatched_board_controller_id() {
    SimulatedPersistentStateStore store;
    TouchCalibrationStore calibration(store);
    const auto model = measuredFixtureModel();
    static_cast<void>(
        calibration.write(TouchCalibrationSlot::Active, model, 1U));

    const auto loaded =
        calibration.load(TouchCalibrationSlot::Active, "a-different-board");
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationLoadStatus::InvalidRecord),
        static_cast<int>(loaded.status));
    TEST_ASSERT_FALSE(loaded.record.has_value());
}

void test_store_load_detects_corruption_as_invalid_record() {
    SimulatedPersistentStateStore store;
    TouchCalibrationStore calibration(store);
    static_cast<void>(calibration.write(TouchCalibrationSlot::Active,
                                        measuredFixtureModel(), 1U));
    store.injectCorruption(
        TouchCalibrationStore::key(TouchCalibrationSlot::Active),
        std::string("not-a-valid-envelope"));

    const auto loaded =
        calibration.load(TouchCalibrationSlot::Active, kBoardId);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationLoadStatus::InvalidRecord),
        static_cast<int>(loaded.status));
}

void test_store_load_classifies_other_epoch_and_unsupported_schema() {
    SimulatedPersistentStateStore store;
    TouchCalibrationStore calibration(store);
    std::string payload;
    static_cast<void>(device_platform::encodeTouchCalibrationPayload(
        measuredFixtureModel(), payload));

    std::string wrongEpochEnvelope;
    static_cast<void>(device_platform::encodeEnvelope(
        {device_platform::kTouchCalibrationRecordType,
         device_platform::kTouchCalibrationSchemaVersion,
         device_platform::StorageEpoch{2U}, 1U, std::nullopt, payload},
        wrongEpochEnvelope,
        device_platform::kMaximumTouchCalibrationEnvelopeBytes));
    store.injectCorruption(
        TouchCalibrationStore::key(TouchCalibrationSlot::Active),
        wrongEpochEnvelope);
    const auto wrongEpoch =
        calibration.load(TouchCalibrationSlot::Active, kBoardId);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationLoadStatus::OtherEpoch),
        static_cast<int>(wrongEpoch.status));

    std::string wrongSchemaEnvelope;
    static_cast<void>(device_platform::encodeEnvelope(
        {device_platform::kTouchCalibrationRecordType, 2U,
         device_platform::kTouchCalibrationStorageEpoch, 1U, std::nullopt,
         payload},
        wrongSchemaEnvelope,
        device_platform::kMaximumTouchCalibrationEnvelopeBytes));
    store.injectCorruption(
        TouchCalibrationStore::key(TouchCalibrationSlot::Active),
        wrongSchemaEnvelope);
    const auto wrongSchema =
        calibration.load(TouchCalibrationSlot::Active, kBoardId);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationLoadStatus::UnsupportedSchema),
        static_cast<int>(wrongSchema.status));
}

void test_store_write_rejects_zero_record_sequence() {
    SimulatedPersistentStateStore store;
    TouchCalibrationStore calibration(store);
    const auto written =
        calibration.write(TouchCalibrationSlot::Active, measuredFixtureModel(),
                          /*recordSequence=*/0U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationWriteStatus::WriteFailure),
        static_cast<int>(written.status));
}

void test_default_constructed_model_is_unmeasured_and_not_well_formed() {
    TouchCalibrationModel model;
    model.boardControllerId = kBoardId;
    TEST_ASSERT_FALSE(std::isfinite(model.a));
    TEST_ASSERT_FALSE(std::isfinite(model.b));
    TEST_ASSERT_FALSE(std::isfinite(model.c));
    TEST_ASSERT_FALSE(std::isfinite(model.d));
    TEST_ASSERT_FALSE(std::isfinite(model.e));
    TEST_ASSERT_FALSE(std::isfinite(model.f));
    TEST_ASSERT_FALSE(
        device_platform::touchCalibrationModelIsWellFormed(model, kBoardId));
}

void test_default_constructed_model_is_not_encodable_or_writable() {
    TouchCalibrationModel model;
    model.boardControllerId = kBoardId;

    std::string payload;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            device_platform::TouchCalibrationCodecStatus::InvalidWireValue),
        static_cast<int>(
            device_platform::encodeTouchCalibrationPayload(model, payload)));

    SimulatedPersistentStateStore store;
    TouchCalibrationStore calibration(store);
    const auto written =
        calibration.write(TouchCalibrationSlot::Active, model, 1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationWriteStatus::CapacityFailure),
        static_cast<int>(written.status));
    // No record was ever committed for this unmeasured model.
    const auto loaded =
        calibration.load(TouchCalibrationSlot::Active, kBoardId);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TouchCalibrationLoadStatus::NotFound),
        static_cast<int>(loaded.status));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_apply_identity_model_returns_raw_coordinates);
    RUN_TEST(test_apply_affine_model_scales_and_offsets);
    RUN_TEST(test_well_formed_requires_matching_board_controller_id);
    RUN_TEST(test_well_formed_rejects_non_finite_coefficients);
    RUN_TEST(test_codec_roundtrip_preserves_model);
    RUN_TEST(test_codec_rejects_oversized_board_controller_id);
    RUN_TEST(test_store_load_without_prior_write_is_not_found);
    RUN_TEST(test_store_write_then_load_active_returns_committed_model);
    RUN_TEST(test_store_active_and_fallback_slots_are_independent);
    RUN_TEST(test_store_load_rejects_mismatched_board_controller_id);
    RUN_TEST(test_store_load_detects_corruption_as_invalid_record);
    RUN_TEST(test_store_load_classifies_other_epoch_and_unsupported_schema);
    RUN_TEST(test_store_write_rejects_zero_record_sequence);
    RUN_TEST(test_default_constructed_model_is_unmeasured_and_not_well_formed);
    RUN_TEST(test_default_constructed_model_is_not_encodable_or_writable);
    return UNITY_END();
}
