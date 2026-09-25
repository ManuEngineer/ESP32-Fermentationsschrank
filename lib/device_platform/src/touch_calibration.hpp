#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "state_store.hpp"
#include "state_store_key.hpp"
#include "storage_types.hpp"

// Anwendungsneutraler, technischer Touchkalibrierungs-Record ueber den
// bestehenden IStateStore. Siehe
// docs/tasks/issue-31-renderer-display-touch-calibration-plan.md,
// Abschnitt 8 ("Persistenzvertrag und StorageEpoch"). Dieser Port kennt
// weder die konkrete Fachbedeutung der Kalibrierungswerte noch das
// Wiederherstellungs-/Recoveryverhalten der Anwendung; er ist die einzige
// technische Quelle fuer den Record selbst.
namespace device_platform {

// Eingefrorene Wire-/Record-Identitaet (siehe Plan Abschnitt 8): Wert 10 ist
// gegen die produktiven Recordtypen 1-9 kollisionsfrei geprueft. Diese
// Werte werden nie mit der allgemeinen Konfigurations-StorageEpoch
// vermischt; ein normaler Factory Reset erhoeht die
// Konfigurations-StorageEpoch, laesst kTouchCalibrationStorageEpoch aber
// unveraendert.
inline constexpr RecordTypeId kTouchCalibrationRecordType{10U};
inline constexpr std::uint32_t kTouchCalibrationSchemaVersion = 1U;
inline constexpr StorageEpoch kTouchCalibrationStorageEpoch{1U};
inline constexpr char kTouchCalibrationActiveKeyBytes[] = "tc0";
inline constexpr char kTouchCalibrationFallbackKeyBytes[] = "tc1";
inline constexpr std::size_t kMaximumTouchCalibrationBoardControllerIdBytes =
    64U;
inline constexpr std::size_t kMaximumTouchCalibrationPayloadBytes = 128U;
inline constexpr std::size_t kMaximumTouchCalibrationEnvelopeBytes = 256U;

// A general affine transform from controller-native raw touch coordinates
// to calibrated display coordinates:
//   calibratedX = a*rawX + b*rawY + c
//   calibratedY = d*rawX + e*rawY + f
// This is the transform *form* only. No coefficient is ever invented here
// or defaulted to a plausible-looking value by this port; every value
// this type ever carries at runtime comes from a record an owner-approved
// calibration workflow actually wrote.
//
// A default-constructed model is deliberately NOT a valid identity
// calibration: every coefficient defaults to NaN, so it fails
// touchCalibrationModelIsWellFormed() and is rejected by
// encodeTouchCalibrationPayload()/TouchCalibrationStore::write() until all
// six coefficients are explicitly set from a real measurement. There is no
// public "valid by default" state this type can silently fall back to.
struct TouchCalibrationModel {
    static constexpr double kUnmeasured =
        std::numeric_limits<double>::quiet_NaN();

    double a{kUnmeasured};
    double b{kUnmeasured};
    double c{kUnmeasured};
    double d{kUnmeasured};
    double e{kUnmeasured};
    double f{kUnmeasured};
    // Identifies the board/controller combination this model was measured
    // against. This port never hardcodes a concrete board or controller
    // name; the caller supplies the identity it expects and compares
    // against.
    std::string boardControllerId;

    friend bool operator==(const TouchCalibrationModel& left,
                           const TouchCalibrationModel& right) noexcept {
        return left.a == right.a && left.b == right.b && left.c == right.c &&
               left.d == right.d && left.e == right.e && left.f == right.f &&
               left.boardControllerId == right.boardControllerId;
    }
    friend bool operator!=(const TouchCalibrationModel& left,
                           const TouchCalibrationModel& right) noexcept {
        return !(left == right);
    }
};

struct CalibratedTouchPoint {
    double x{0.0};
    double y{0.0};
};

// Applies the model form only; this function never rejects a model, it
// only computes the transform. Structural validity is
// touchCalibrationModelIsWellFormed()'s responsibility.
[[nodiscard]] CalibratedTouchPoint applyTouchCalibration(
    const TouchCalibrationModel& model, std::uint16_t rawX,
    std::uint16_t rawY) noexcept;

// A model is well-formed only if every coefficient is finite (protects
// against a corrupted/garbage record) and its boardControllerId matches
// the caller-supplied expected identity exactly. This is a structural and
// identity check, not a plausibility judgement about specific coefficient
// values or coordinate ranges - this renderer-/hardware-independent port
// cannot know those.
[[nodiscard]] bool touchCalibrationModelIsWellFormed(
    const TouchCalibrationModel& model,
    const std::string& expectedBoardControllerId) noexcept;

enum class TouchCalibrationCodecStatus : std::uint8_t {
    Success,
    CapacityExceeded,
    Truncated,
    TrailingBytes,
    InvalidWireValue,
};

// Encodes only the payload (not the envelope); TouchCalibrationStore wraps
// this with encodeEnvelope()/decodeEnvelope() for the technical record
// identity (record type, schema version, storage epoch, record sequence).
[[nodiscard]] TouchCalibrationCodecStatus encodeTouchCalibrationPayload(
    const TouchCalibrationModel& model, std::string& out);

struct TouchCalibrationPayloadDecodeResult {
    TouchCalibrationCodecStatus status{TouchCalibrationCodecStatus::Truncated};
    std::optional<TouchCalibrationModel> model;
};

[[nodiscard]] TouchCalibrationPayloadDecodeResult decodeTouchCalibrationPayload(
    const std::string& payload);

struct TouchCalibrationRecord {
    TouchCalibrationModel model;
    std::uint64_t recordSequence{0U};
};

enum class TouchCalibrationSlot : std::uint8_t {
    Active,
    Fallback,
};

enum class TouchCalibrationLoadStatus : std::uint8_t {
    Available,
    NotFound,
    OtherEpoch,
    UnsupportedSchema,
    InvalidRecord,
    ReadError,
    CapacityError,
};

struct TouchCalibrationLoadResult {
    TouchCalibrationLoadStatus status{TouchCalibrationLoadStatus::ReadError};
    std::optional<TouchCalibrationRecord> record;
};

enum class TouchCalibrationWriteStatus : std::uint8_t {
    Committed,
    WriteFailure,
    CapacityFailure,
    Indeterminate,
};

struct TouchCalibrationWriteResult {
    TouchCalibrationWriteStatus status{
        TouchCalibrationWriteStatus::WriteFailure};
    std::uint64_t recordSequence{0U};
};

// The single technical owner of the two dedicated tc0 (active) / tc1
// (fallback) record slots. It checks only the fixed, independent
// kTouchCalibrationStorageEpoch namespace and the caller-supplied expected
// board/controller identity - never the general configuration
// StorageEpoch, which a normal factory reset legitimately advances. This
// store draws no conclusion about whether the fallback slot should ever
// replace the active one; that policy question belongs to the calling
// application layer, not this port.
class TouchCalibrationStore final {
   public:
    explicit TouchCalibrationStore(IStateStore& store) : store_(store) {}

    [[nodiscard]] TouchCalibrationLoadResult load(
        TouchCalibrationSlot slot,
        const std::string& expectedBoardControllerId) const;

    [[nodiscard]] TouchCalibrationWriteResult write(
        TouchCalibrationSlot slot, const TouchCalibrationModel& model,
        std::uint64_t recordSequence);

    [[nodiscard]] static const StateStoreKey& key(
        TouchCalibrationSlot slot) noexcept;

   private:
    IStateStore& store_;
};

}  // namespace device_platform
