#include <unity.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "device_platform.hpp"
#include "ds18b20_sampling_engine.hpp"
#include "fake_ds18b20_bus.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_commands.hpp"
#include "mock_time_zone_resolver.hpp"
#include "onewire_rom_crc.hpp"
#include "sensor_commissioning.hpp"
#include "simulated_persistent_state_store.hpp"
#include "virtual_time_source.hpp"

// Issue #30, Softwarepfad C2 (Plan 5.3): Datenmodell, Kommando-Parser,
// Kanalabbildung und Application-Owner-Eintrag des Sensor-Inbetriebnahme-
// datensatzes. Alle Tests sind Simulationen (SIM-30-*), kein Hardwarenachweis.

namespace fermentation {

class FermentationApplicationTestAccess {
   public:
    static ApplicationCallSerializer::Guard enter(
        FermentationApplication& application) {
        return application.applicationCallSerializer_.enter();
    }
    // A manual run needs the technical run limits of an owner that does not
    // exist (O5, #172 S9); the private body behind that guard gives these
    // tests a real active run to prove the device name gate.
    static FermentationApplicationRequestResult prepareStartManualTimedBody(
        FermentationApplication& application,
        const FermentationUiCommandContext& context,
        const ManualTimedRunValues& values) {
        return application.prepareStartManualTimedUnguarded(context, values);
    }
};

}  // namespace fermentation

namespace {

using namespace fermentation;

CrossRolePlausibilityContext validEvidence() {
    CrossRolePlausibilityContext evidence;
    evidence.air.quality = device_platform::SensorQuality::Valid;
    evidence.cooling.quality = device_platform::SensorQuality::Valid;
    evidence.product.quality = device_platform::SensorQuality::Valid;
    return evidence;
}

struct Fixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    device_platform::VirtualTimeSource timeSource;
    FermentationApplication application;

    Fixture() {
        TEST_ASSERT_TRUE(platform.begin({true}));
        timeSource.setUnixTimeSeconds(1'700'000'000LL);
        TEST_ASSERT_TRUE(
            application.begin(platform, store, timeZoneResolver, timeSource));
        application.publishOwningRuntimeEvidence(validEvidence());
    }

    std::optional<ServiceConfigurationRevision> revision() {
        const auto snapshot = application.sensorCommissioning();
        TEST_ASSERT_TRUE(snapshot.has_value());
        return snapshot->revision;
    }
    std::optional<SensorCommissioningRecord> record() {
        const auto snapshot = application.sensorCommissioning();
        TEST_ASSERT_TRUE(snapshot.has_value());
        return snapshot->record;
    }
    ApplicationConfigurationChangeResult apply(
        const std::optional<SensorCommissioningRecord>& value) {
        return application.applySensorCommissioning(value, revision());
    }
    void startRun() {
        ManualTimedRunValues values;
        values.targetTemperatureCelsius = 30.0;
        values.durationMinutes = 60U;
        values.qualificationBandCelsius = 0.5;
        values.qualificationDurationMinutes = 10U;
        values.maximumTargetReachMinutes = 180U;
        FermentationUiCommandContext context;
        context.expected = application.uiSnapshot().revisions;
        context.monotonicMillis = timeSource.monotonicMillis();
        const auto prepared =
            FermentationApplicationTestAccess::prepareStartManualTimedBody(
                application, context, values);
        TEST_ASSERT_TRUE(prepared.request.has_value());
        const auto confirmed = application.confirmPrepared(prepared);
        TEST_ASSERT_TRUE(confirmed.request.has_value());
        const auto applied =
            application.applyConfirmedPrepared(*confirmed.request);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(FermentationUiCommandPhase::OwningOutcome),
            static_cast<int>(applied.phase));
        TEST_ASSERT_TRUE(application.uiSnapshot().home.mode ==
                         FermentationHomeMode::ActiveRun);
    }
    void stopRun() {
        FermentationUiCommandContext context;
        context.expected = application.uiSnapshot().revisions;
        context.monotonicMillis = timeSource.monotonicMillis();
        FermentationUiStopRunIntent stop;
        stop.option = StopOption::AbortAndTurnOff;
        const auto prepared = application.prepareStop(context, stop);
        TEST_ASSERT_TRUE(prepared.request.has_value());
        const auto confirmed = application.confirmPrepared(prepared);
        TEST_ASSERT_TRUE(confirmed.request.has_value());
        static_cast<void>(
            application.applyConfirmedPrepared(*confirmed.request));
        TEST_ASSERT_TRUE(application.uiSnapshot().home.mode ==
                         FermentationHomeMode::Standby);
    }
};

using device_platform::SensorOffset;

SensorOffset offsetOf(double celsius) {
    return SensorOffset::create(celsius).offset.value();
}

constexpr device_platform::OneWireRom kAir = 0x160100000000FF28ULL;
constexpr device_platform::OneWireRom kHeat = 0xF40200000000FF28ULL;
constexpr device_platform::OneWireRom kProbe1 = 0x8B1100000000FF28ULL;
constexpr device_platform::OneWireRom kProbe2 = 0x691200000000FF28ULL;

SensorCommissioningRecord validRecord(std::size_t probes = 0U) {
    SensorCommissioningRecord record;
    record.chamberAir = SensorRomOffset{kAir, offsetOf(0.5)};
    record.heatsink = SensorRomOffset{kHeat, offsetOf(-0.25)};
    if (probes >= 1U) {
        record.productProbes.push_back(
            SensorRomOffset{kProbe1, offsetOf(0.125)});
    }
    if (probes >= 2U) {
        record.productProbes.push_back(SensorRomOffset{kProbe2, offsetOf(0.0)});
    }
    return record;
}

void assertActivated(const ApplicationConfigurationChangeResult& result) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationPreviewStatus::Success),
                          static_cast<int>(result.preview));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationCommitStatus::Activated),
        static_cast<int>(result.commit));
}

// --- Datenmodell -----------------------------------------------------------

void test_model_validation_covers_every_invalid_shape() {
    TEST_ASSERT_TRUE(validateSensorCommissioning(validRecord(2U)) ==
                     SensorCommissioningStatus::Success);
    auto missingAir = validRecord();
    missingAir.chamberAir.reset();
    TEST_ASSERT_TRUE(validateSensorCommissioning(missingAir) ==
                     SensorCommissioningStatus::MissingFixedRole);
    auto missingHeat = validRecord();
    missingHeat.heatsink.reset();
    TEST_ASSERT_TRUE(validateSensorCommissioning(missingHeat) ==
                     SensorCommissioningStatus::MissingFixedRole);
    auto zero = validRecord();
    zero.chamberAir->rom = 0U;
    TEST_ASSERT_TRUE(validateSensorCommissioning(zero) ==
                     SensorCommissioningStatus::ZeroRom);
    // Manipulierte CRC (hoechstes Byte) an jeder Stelle: Luft, Kuehlkoerper,
    // bekannter Produktfuehler.
    auto badAir = validRecord(1U);
    badAir.chamberAir->rom ^= 0x0100000000000000ULL;
    TEST_ASSERT_TRUE(validateSensorCommissioning(badAir) ==
                     SensorCommissioningStatus::InvalidRomCrc);
    auto badHeat = validRecord(1U);
    badHeat.heatsink->rom ^= 0x0100000000000000ULL;
    TEST_ASSERT_TRUE(validateSensorCommissioning(badHeat) ==
                     SensorCommissioningStatus::InvalidRomCrc);
    auto badProbe = validRecord(1U);
    badProbe.productProbes[0].rom ^= 0x0100000000000000ULL;
    TEST_ASSERT_TRUE(validateSensorCommissioning(badProbe) ==
                     SensorCommissioningStatus::InvalidRomCrc);
    // Kuenstliches, von Null verschiedenes ROM ohne gueltige CRC.
    auto artificial = validRecord();
    artificial.chamberAir->rom = 0x28FF000000000001ULL;
    TEST_ASSERT_TRUE(validateSensorCommissioning(artificial) ==
                     SensorCommissioningStatus::InvalidRomCrc);
    auto duplicate = validRecord();
    duplicate.heatsink->rom = kAir;
    TEST_ASSERT_TRUE(validateSensorCommissioning(duplicate) ==
                     SensorCommissioningStatus::DuplicateRom);
    auto duplicateProbe = validRecord(1U);
    duplicateProbe.productProbes[0].rom = kHeat;
    TEST_ASSERT_TRUE(validateSensorCommissioning(duplicateProbe) ==
                     SensorCommissioningStatus::DuplicateRom);
    auto twoSameProbes = validRecord(2U);
    twoSameProbes.productProbes[1].rom = kProbe1;
    TEST_ASSERT_TRUE(validateSensorCommissioning(twoSameProbes) ==
                     SensorCommissioningStatus::DuplicateRom);
    auto tooMany = validRecord();
    constexpr device_platform::OneWireRom kExtraRoms[] = {
        0x6B2000000000FF28ULL, 0x352100000000FF28ULL, 0xD72200000000FF28ULL,
        0x892300000000FF28ULL, 0x0A2400000000FF28ULL};
    static_assert(sizeof(kExtraRoms) / sizeof(kExtraRoms[0]) ==
                  kMaximumKnownProductProbes + 1U);
    for (const auto rom : kExtraRoms) {
        tooMany.productProbes.push_back(SensorRomOffset{rom, offsetOf(0.0)});
    }
    TEST_ASSERT_TRUE(validateSensorCommissioning(tooMany) ==
                     SensorCommissioningStatus::TooManyProductProbes);
}

void test_rom_crc_check_matches_the_dallas_reference() {
    // Maxim-Referenz-ROM 02 1C B8 01 00 00 00 A2 (Byte 0 zuerst) und die
    // Testwerte; jede Einzelbit-Aenderung muss erkannt werden.
    constexpr uint64_t kReference = 0xA2000000'01B81C02ULL;
    TEST_ASSERT_TRUE(device_platform::hasValidOneWireRomCrc(kReference));
    TEST_ASSERT_TRUE(device_platform::hasValidOneWireRomCrc(kAir));
    TEST_ASSERT_TRUE(device_platform::hasValidOneWireRomCrc(kProbe2));
    for (unsigned bit = 0U; bit < 64U; ++bit) {
        TEST_ASSERT_FALSE(
            device_platform::hasValidOneWireRomCrc(kReference ^ (1ULL << bit)));
    }
    TEST_ASSERT_FALSE(device_platform::hasValidOneWireRomCrc(0x1ULL));
}

void test_calibration_lookup_is_per_rom_and_unknown_rom_has_none() {
    const auto record = validRecord(1U);
    const auto air = record.calibrationFor(kAir);
    TEST_ASSERT_TRUE(air.has_value());
    TEST_ASSERT_EQUAL_UINT64(kAir, air->identity().value());
    TEST_ASSERT_EQUAL_DOUBLE(0.5, air->offset().celsius());
    TEST_ASSERT_EQUAL_DOUBLE(-0.25,
                             record.calibrationFor(kHeat)->offset().celsius());
    TEST_ASSERT_EQUAL_DOUBLE(
        0.125, record.calibrationFor(kProbe1)->offset().celsius());
    TEST_ASSERT_FALSE(record.calibrationFor(0x7DFF00000000FF28ULL).has_value());
    TEST_ASSERT_FALSE(record.calibrationFor(0U).has_value());
}

void test_record_maps_roles_to_technical_channels_and_invalid_stays_unbound() {
    const auto bindings = toChannelBindings(validRecord(1U));
    TEST_ASSERT_TRUE(bindings[0].has_value());
    TEST_ASSERT_TRUE(bindings[1].has_value());
    TEST_ASSERT_EQUAL_UINT8(kChamberAirChannel, bindings[0]->channel);
    TEST_ASSERT_EQUAL_UINT64(kAir, bindings[0]->expectedRom);
    TEST_ASSERT_EQUAL_UINT8(kHeatsinkChannel, bindings[1]->channel);
    TEST_ASSERT_EQUAL_UINT64(kHeat, bindings[1]->expectedRom);
    TEST_ASSERT_EQUAL_UINT8(0U, kChamberAirChannel);
    TEST_ASSERT_EQUAL_UINT8(1U, kHeatsinkChannel);
    TEST_ASSERT_EQUAL_UINT8(2U, kProductChannel);

    for (const auto& unbound :
         {toChannelBindings(std::nullopt), toChannelBindings([] {
              auto invalid = validRecord();
              invalid.chamberAir->rom ^= 0x0100000000000000ULL;
              return std::optional<SensorCommissioningRecord>{invalid};
          }()),
          toChannelBindings([] {
              auto invalid = validRecord();
              invalid.heatsink->rom ^= 0x0100000000000000ULL;
              return std::optional<SensorCommissioningRecord>{invalid};
          }()),
          toChannelBindings([] {
              auto invalid = validRecord();
              invalid.heatsink.reset();
              return std::optional<SensorCommissioningRecord>{invalid};
          }()),
          toChannelBindings([] {
              auto invalid = validRecord();
              invalid.heatsink->rom = kAir;
              return std::optional<SensorCommissioningRecord>{invalid};
          }())}) {
        TEST_ASSERT_FALSE(unbound[0].has_value());
        TEST_ASSERT_FALSE(unbound[1].has_value());
    }
}

// Der Datensatz ist die einzige Bindungsquelle: der Plattform-Engine liefert
// ohne Datensatz nie Ok, mit Datensatz die Rollen ueber die Kanalabbildung.
void test_engine_is_fail_closed_without_record_and_bound_through_the_mapping() {
    device_platform::VirtualTimeSource time;
    device_platform_test_support::FakeDs18b20Bus bus0;
    device_platform_test_support::FakeDs18b20Bus bus1;
    device_platform::Ds18b20SamplingEngine engine(time, bus0, bus1);
    bus0.addDevice(kAir, 20.0);
    bus0.addDevice(kHeat, 5.0);
    const auto run = [&] {
        const uint64_t target = engine.completedCycles() + 1U;
        for (int guard = 0; guard < 8 && engine.completedCycles() < target;
             ++guard) {
            const uint64_t due = engine.nextDueMillis();
            if (due > time.monotonicMillis()) {
                time.advanceMonotonicMillis(due - time.monotonicMillis());
            }
            engine.step();
        }
    };
    engine.setBinding(toChannelBindings(std::nullopt));
    run();
    TEST_ASSERT_TRUE(engine.reading(kChamberAirChannel).status() ==
                     device_platform::TemperatureSampleStatus::MissingSample);
    TEST_ASSERT_TRUE(engine.reading(kHeatsinkChannel).status() ==
                     device_platform::TemperatureSampleStatus::MissingSample);
    engine.setBinding(toChannelBindings(validRecord()));
    run();
    TEST_ASSERT_EQUAL_DOUBLE(
        20.0, engine.reading(kChamberAirChannel).celsius().value());
    TEST_ASSERT_EQUAL_DOUBLE(
        5.0, engine.reading(kHeatsinkChannel).celsius().value());
}

// --- Kommando-Parser -------------------------------------------------------

void test_parser_accepts_exact_commands_only() {
    using Kind = SensorCommissioningCommandKind;
    TEST_ASSERT_TRUE(parseSensorCommissioningCommand("ds18b20 report").kind ==
                     Kind::Report);
    TEST_ASSERT_TRUE(
        parseSensorCommissioningCommand("  ds18b20   clear \r\n").kind ==
        Kind::Clear);
    const auto bind = parseSensorCommissioningCommand(
        "ds18b20 bind air=160100000000FF28 heatsink=F40200000000FF28");
    TEST_ASSERT_TRUE(bind.kind == Kind::Bind);
    TEST_ASSERT_EQUAL_UINT64(kAir, *bind.chamberAirRom);
    TEST_ASSERT_EQUAL_UINT64(kHeat, *bind.heatsinkRom);
    const auto air = parseSensorCommissioningCommand("ds18b20 offset air -250");
    TEST_ASSERT_TRUE(air.kind == Kind::Offset);
    TEST_ASSERT_TRUE(air.target == SensorCommissioningTarget::ChamberAir);
    TEST_ASSERT_EQUAL_INT32(-250, *air.offsetMilliCelsius);
    const auto product = parseSensorCommissioningCommand(
        "ds18b20 offset product=8B1100000000FF28 125");
    TEST_ASSERT_TRUE(product.target == SensorCommissioningTarget::Product);
    TEST_ASSERT_EQUAL_UINT64(kProbe1, *product.offsetRom);

    for (const char* bad :
         {"", "report", "ds18b20", "ds18b20 reset", "ds18b20 report x",
          "ds18b20 bind", "ds18b20 bind air=1 heatsink=2",
          "ds18b20 bind air=0000000000000000 heatsink=F40200000000FF28",
          "ds18b20 bind air=28FF00000000000G heatsink=F40200000000FF28",
          "ds18b20 bind heatsink=F40200000000FF28 air=160100000000FF28",
          "ds18b20 offset air", "ds18b20 offset air x",
          "ds18b20 offset air 12345678", "ds18b20 offset fan 1",
          "ds18b20 offset product=zz 1"}) {
        TEST_ASSERT_TRUE(parseSensorCommissioningCommand(bad).kind ==
                         Kind::Rejected);
    }
}

void test_command_application_builds_valid_records_only() {
    const auto bind = parseSensorCommissioningCommand(
        "ds18b20 bind air=160100000000FF28 heatsink=F40200000000FF28");
    auto applied = applySensorCommissioningCommand(std::nullopt, bind);
    TEST_ASSERT_TRUE(applied.accepted && applied.hasRecord);
    TEST_ASSERT_EQUAL_DOUBLE(
        0.0, applied.record.calibrationFor(kAir)->offset().celsius());
    // Bind mit ungueltiger ROM-CRC wird abgelehnt.
    TEST_ASSERT_FALSE(applySensorCommissioningCommand(
                          std::nullopt, parseSensorCommissioningCommand(
                                            "ds18b20 bind air=170100000000FF28 "
                                            "heatsink=F40200000000FF28"))
                          .accepted);
    // Offset ohne Datensatz oder fuer eine ungueltige Eingabe: abgelehnt.
    const auto offsetCommand =
        parseSensorCommissioningCommand("ds18b20 offset heatsink -125");
    TEST_ASSERT_FALSE(
        applySensorCommissioningCommand(std::nullopt, offsetCommand).accepted);
    auto withOffset =
        applySensorCommissioningCommand(applied.record, offsetCommand);
    TEST_ASSERT_TRUE(withOffset.accepted);
    TEST_ASSERT_EQUAL_DOUBLE(
        -0.125, withOffset.record.calibrationFor(kHeat)->offset().celsius());
    // Ausserhalb der Firmwaregrenze (10 Grad).
    TEST_ASSERT_FALSE(applySensorCommissioningCommand(
                          applied.record, parseSensorCommissioningCommand(
                                              "ds18b20 offset air 10001"))
                          .accepted);
    // Produkt-ROM wird angelegt, ein bereits fester ROM ist ein Duplikat.
    auto product = applySensorCommissioningCommand(
        applied.record, parseSensorCommissioningCommand(
                            "ds18b20 offset product=8B1100000000FF28 62"));
    TEST_ASSERT_TRUE(product.accepted);
    TEST_ASSERT_EQUAL_UINT32(1U, product.record.productProbes.size());
    TEST_ASSERT_FALSE(
        applySensorCommissioningCommand(
            applied.record, parseSensorCommissioningCommand(
                                "ds18b20 offset product=160100000000FF28 5"))
            .accepted);
    // Neubindung behaelt den Offset eines bekannten ROM.
    auto rebind = applySensorCommissioningCommand(
        withOffset.record,
        parseSensorCommissioningCommand(
            "ds18b20 bind air=F40200000000FF28 heatsink=160100000000FF28"));
    TEST_ASSERT_TRUE(rebind.accepted);
    TEST_ASSERT_EQUAL_DOUBLE(
        -0.125, rebind.record.calibrationFor(kHeat)->offset().celsius());
    // Clear und Report.
    const auto clear = applySensorCommissioningCommand(
        applied.record, parseSensorCommissioningCommand("ds18b20 clear"));
    TEST_ASSERT_TRUE(clear.accepted);
    TEST_ASSERT_FALSE(clear.hasRecord);
    TEST_ASSERT_FALSE(
        applySensorCommissioningCommand(
            applied.record, parseSensorCommissioningCommand("ds18b20 report"))
            .accepted);
}

// --- Application-Owner -----------------------------------------------------

void test_application_starts_unbound_and_commits_a_valid_record() {
    Fixture fixture;
    const auto before = fixture.revision();
    TEST_ASSERT_FALSE(fixture.record().has_value());
    assertActivated(fixture.apply(validRecord(1U)));
    TEST_ASSERT_TRUE(fixture.record() == validRecord(1U));
    TEST_ASSERT_TRUE(fixture.revision() != before);
    // Gleicher Datensatz nochmals: keine Aenderung.
    const auto same = fixture.apply(validRecord(1U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigurationCommitStatus::NoChange),
                          static_cast<int>(same.commit));
    // Loeschen macht die Sensoren wieder ungebunden.
    assertActivated(fixture.apply(std::nullopt));
    TEST_ASSERT_FALSE(fixture.record().has_value());
}

void test_application_rejects_invalid_records_without_a_preview() {
    Fixture fixture;
    const auto before = fixture.revision();
    auto partial = validRecord();
    partial.heatsink.reset();
    auto duplicate = validRecord();
    duplicate.heatsink->rom = kAir;
    auto zero = validRecord();
    zero.chamberAir->rom = 0U;
    for (const auto& invalid : {partial, duplicate, zero}) {
        const auto result = fixture.apply(invalid);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ConfigurationPreviewStatus::InvalidCandidate),
            static_cast<int>(result.preview));
        TEST_ASSERT_FALSE(fixture.record().has_value());
        TEST_ASSERT_TRUE(fixture.revision() == before);
    }
    // Es blieb kein Preview-Slot belegt.
    assertActivated(fixture.apply(validRecord()));
}

void test_application_missing_and_stale_revisions_are_rejected() {
    Fixture fixture;
    const auto first = fixture.revision();
    const auto missing = fixture.application.applySensorCommissioning(
        validRecord(), std::nullopt);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::StateChanged),
        static_cast<int>(missing.preview));
    TEST_ASSERT_FALSE(fixture.record().has_value());
    assertActivated(
        fixture.application.applySensorCommissioning(validRecord(), first));
    const auto stale =
        fixture.application.applySensorCommissioning(validRecord(1U), first);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::StateChanged),
        static_cast<int>(stale.preview));
    TEST_ASSERT_TRUE(fixture.record() == validRecord());
    assertActivated(fixture.apply(validRecord(1U)));
}

void test_application_refuses_changes_while_a_run_is_active() {
    Fixture fixture;
    fixture.startRun();
    const auto before = fixture.revision();
    const auto result = fixture.apply(validRecord());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigurationPreviewStatus::NotAllowed),
        static_cast<int>(result.preview));
    TEST_ASSERT_FALSE(fixture.record().has_value());
    TEST_ASSERT_TRUE(fixture.revision() == before);
    fixture.stopRun();
    assertActivated(fixture.apply(validRecord()));
}

void test_not_started_application_has_no_record_and_refuses() {
    FermentationApplication application;
    TEST_ASSERT_FALSE(application.sensorCommissioning().has_value());
    const auto result = application.applySensorCommissioning(
        validRecord(), std::optional<ServiceConfigurationRevision>{
                           ServiceConfigurationRevision{}});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable),
        static_cast<int>(result.preview));
}

void test_record_persists_across_a_restart() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    {
        device_platform::DevicePlatform platform;
        device_platform::VirtualTimeSource timeSource;
        FermentationApplication application;
        TEST_ASSERT_TRUE(platform.begin({true}));
        timeSource.setUnixTimeSeconds(1'700'000'000LL);
        TEST_ASSERT_TRUE(
            application.begin(platform, store, timeZoneResolver, timeSource));
        assertActivated(application.applySensorCommissioning(
            validRecord(2U), application.sensorCommissioning()->revision));
    }
    device_platform::DevicePlatform platform;
    device_platform::VirtualTimeSource timeSource;
    FermentationApplication restarted;
    TEST_ASSERT_TRUE(platform.begin({true}));
    timeSource.setUnixTimeSeconds(1'700'000'100LL);
    TEST_ASSERT_TRUE(
        restarted.begin(platform, store, timeZoneResolver, timeSource));
    const auto snapshot = restarted.sensorCommissioning();
    TEST_ASSERT_TRUE(snapshot.has_value());
    TEST_ASSERT_TRUE(snapshot->record == validRecord(2U));
    // Die Bindung nach dem Neustart entspricht dem gespeicherten Datensatz.
    const auto bindings = toChannelBindings(snapshot->record);
    TEST_ASSERT_EQUAL_UINT64(kAir, bindings[0]->expectedRom);
}

// Wie jeder oeffentliche Application-Eintrag wartet die Aenderung auf das
// gemeinsame Anwendungsgate (D5).
void test_apply_sensor_commissioning_waits_for_the_application_gate() {
    Fixture fixture;
    std::mutex synchronization;
    std::condition_variable changed;
    bool workerStarted = false;
    bool workerDone = false;
    const auto revision = fixture.revision();
    std::thread worker;
    {
        auto held =
            FermentationApplicationTestAccess::enter(fixture.application);
        worker = std::thread([&] {
            {
                std::lock_guard<std::mutex> lock(synchronization);
                workerStarted = true;
            }
            changed.notify_one();
            const auto result = fixture.application.applySensorCommissioning(
                validRecord(), revision);
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(ConfigurationCommitStatus::Activated),
                static_cast<int>(result.commit));
            {
                std::lock_guard<std::mutex> lock(synchronization);
                workerDone = true;
            }
            changed.notify_one();
        });
        {
            std::unique_lock<std::mutex> lock(synchronization);
            TEST_ASSERT_TRUE(changed.wait_for(lock, std::chrono::seconds(1),
                                              [&] { return workerStarted; }));
            changed.wait_for(lock, std::chrono::milliseconds(100),
                             [&] { return workerDone; });
            TEST_ASSERT_FALSE(workerDone);
        }
    }
    {
        std::unique_lock<std::mutex> lock(synchronization);
        TEST_ASSERT_TRUE(changed.wait_for(lock, std::chrono::seconds(2),
                                          [&] { return workerDone; }));
    }
    worker.join();
    TEST_ASSERT_TRUE(fixture.record() == validRecord());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_model_validation_covers_every_invalid_shape);
    RUN_TEST(test_rom_crc_check_matches_the_dallas_reference);
    RUN_TEST(test_calibration_lookup_is_per_rom_and_unknown_rom_has_none);
    RUN_TEST(
        test_record_maps_roles_to_technical_channels_and_invalid_stays_unbound);
    RUN_TEST(
        test_engine_is_fail_closed_without_record_and_bound_through_the_mapping);
    RUN_TEST(test_parser_accepts_exact_commands_only);
    RUN_TEST(test_command_application_builds_valid_records_only);
    RUN_TEST(test_application_starts_unbound_and_commits_a_valid_record);
    RUN_TEST(test_application_rejects_invalid_records_without_a_preview);
    RUN_TEST(test_application_missing_and_stale_revisions_are_rejected);
    RUN_TEST(test_application_refuses_changes_while_a_run_is_active);
    RUN_TEST(test_not_started_application_has_no_record_and_refuses);
    RUN_TEST(test_record_persists_across_a_restart);
    RUN_TEST(test_apply_sensor_commissioning_waits_for_the_application_gate);
    return UNITY_END();
}
