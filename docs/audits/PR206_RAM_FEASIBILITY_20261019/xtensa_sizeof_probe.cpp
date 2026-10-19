// Xtensa-sizeof-Probe (xtensa-esp32-elf-g++ -std=gnu++17 -c, ESP-IDF 6.1 Toolchain).
// Aufruf: g++ ... -I lib/fermentation_app/src -I lib/device_platform/src
//         -I <cJSON-Include> xtensa_sizeof_probe.cpp; Werte per objdump -s -j .rodata lesen.
// Reihenfolge der Werte = Reihenfolge der Q(...)-Zeilen (je 8 Byte, little endian).
// RunDetailScratch ist eine ABSCHAETZUNG des C2-Zielfelds nach Plan §3, kein Produktcode.
#include <cstdint>
#include <optional>
#include <string>
#include "cJSON.h"
#include "fermentation_ui_models.hpp"
#include "fermentation_ui_projector.hpp"
using namespace fermentation;
struct Completion { std::uint8_t mode; std::optional<double> cooling; std::optional<std::uint32_t> hold; };
struct TimeInfo { std::uint8_t kind; std::optional<std::uint64_t> elapsed; std::optional<std::uint64_t> remaining; };
struct RunDetailScratch { std::string programName; std::uint8_t sourceKind; std::optional<double> target; std::optional<Completion> completion; TimeInfo time; };
#define Q(N, T) extern const unsigned long long sz_##N; const unsigned long long sz_##N = sizeof(T);
Q(Snapshot, FermentationUiSnapshot) Q(HomeView, FermentationHomeView) Q(Tracker, FermentationUiRefreshRevisionTracker)
Q(String, std::string) Q(TempView, TemperatureView) Q(ProjInput, FermentationUiProjectionInput) Q(cJSONnode, cJSON)
Q(RunDetailScratch, RunDetailScratch) Q(OptRunDetailScratch, std::optional<RunDetailScratch>)
