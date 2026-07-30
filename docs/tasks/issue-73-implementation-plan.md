# Implementierungsplan fuer Issue #73

## 1. Planstatus und Basis-SHA

- Issue: `#73 – [Platform B] ESP-IDF-Laufzeitadapter und Composition-Root-Paritaet herstellen`
- Tracking: `#71`
- Abhaengigkeit: `#72` / PR `#77` (gemergt)
- Planbranch: `plan/issue-73-esp-idf-runtime-parity`
- Basisbranch: `main`
- Basis-Commit: `bf3b1a8b008ce6169494fb2e444cedeadb456d39`
  (Merge-Commit von PR #77, zugleich aktueller `origin/main`-Stand)
- Planstatus: `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`
- Implementierung: nicht begonnen

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

Diese Freigabe gilt ausschliesslich fuer den genannten Plan-Commit.

### Metadatenabweichung (nur Beobachtung)

Issue `#73` traegt weiterhin `Status: BLOCKED_DEPENDENCY`. Da PR #77 gemergt
ist, ist die fachliche Startabhaengigkeit erfuellt. Keine Issue-Aenderung in
dieser Phase.

## 2. Umgebungsnachweis (read-only, native ESP-IDF-Installation)

| Pruefung | Ergebnis |
|---|---|
| `idf.py --version` | `ESP-IDF v6.0.2` |
| `IDF_PATH` | `/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.0.2` |
| `git describe --tags --exact-match` | `v6.0.2` |
| ESP-IDF-Commit-ID | `7101770dc6db2667b3c477cc31365dd1acd6db4e` |
| `git status --short` | leer (sauber) |
| Submodule-Status | 25 Submodule, alle sauber |

Ausschliesslich gelesen; keine Installation, kein Update, kein Checkout
geaendert.

## 3. Ziel

Den bestehenden portablen Kern (`device_platform`, `fermentation_app`) unter
ESP-IDF 6.0.2 mit einem echten `app_main()`-Composition-Root betreiben, mit
demselben sicheren Start- und Testmodusverhalten wie der bestehende
Arduino-/native-Pfad, ohne Fachlogik umzuschreiben. Der temporaere Stub aus
`#72` wird entfernt. `device_platform_esp_idf` entsteht erstmals, aber nur mit
Adaptern, die einen realen Konsumenten im Composition Root haben.

## 4. Nicht-Ziele

- Arduino-/PlatformIO-Entfernung; CI-Migration; IDF in GitHub Actions;
  finale Ressourcenbaseline/-grenzwerte; Upgradevertrag (alles `#74`);
- NVS, WLAN, Web, OTA, GPIO, LEDC, Sensoren, Display, Touch, Aktoren,
  Peltier, Luefter, jede reale Hardwarefreigabe;
- persistente Reset-/Crashhistorie, NTP, Zeitzonenkatalog;
- allgemeines Loggingframework, allgemeiner Scheduler, zusaetzliche Task
  ohne nachgewiesenen Grund;
- neue Fachzustaende, Wire-/Persistenzaenderungen;
- neue ADRs oder Folgeissues ohne Freigabe;
- Aenderung an `src/main.cpp` (siehe Abschnitt 8).

## 5. Verbindliche Quellen

Lokal im Checkout `/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.0.2`
direkt verifiziert (kein Websearch-Ersatz):

| Fakt | Fundstelle | Wert |
|---|---|---|
| Main-Task-Prioritaet | `components/esp_system/include/esp_task.h:56` | `ESP_TASK_MAIN_PRIO = ESP_TASK_PRIO_MIN + 1` = fix `1`, nicht konfigurierbar |
| Main-Task-Stackgroesse | `components/esp_system/Kconfig:231-236` | `CONFIG_ESP_MAIN_TASK_STACK_SIZE`, Default `3584` Byte |
| Main-Task-Affinitaet | `components/esp_system/Kconfig:240-261` | `CONFIG_ESP_MAIN_TASK_AFFINITY`, Default `CPU0` |
| Rueckkehr aus `app_main()` | offizielle Get-Started-/Startup-Doku v6.0.2 | erlaubt: Task wird sauber beendet, Stack freigegeben, System laeuft mit uebrigen Tasks weiter |
| Task-Watchdog-Default | `components/esp_system/Kconfig:295-357` | `ESP_TASK_WDT_EN`/`_INIT` an, beobachtet standardmaessig nur die Idle-Tasks je Core (nicht `main`), Timeout `5 s`, `ESP_TASK_WDT_PANIC` aus (Default: Log statt Panic) |
| FreeRTOS-Tickrate | `components/freertos/Kconfig:33-38` | `CONFIG_FREERTOS_HZ` Default `100` (10 ms/Tick); `pdMS_TO_TICKS(1000)` = 100 Ticks exakt, keine Rundung |
| `esp_timer_get_time()` | `esp_timer.h` / v6.0.2-Doku | `int64_t` Mikrosekunden seit ESP-Timer-Init kurz vor `app_main()`; monoton, ISR-sicher, 64 Bit (kein praktischer Ueberlauf) |
| Logging | `esp_log.h` / v6.0.2-Doku | `ESP_LOGI/W/E(TAG, ...)`, Komponente `log` bereits Common-Requires, `CONFIG_LOG_DEFAULT_LEVEL` |
| `esp_reset_reason()`/Heap-APIs | `esp_system.h` / v6.0.2-Doku | Teil der Common-Komponente `esp_system`, keine zusaetzliche `REQUIRES` noetig |

Weitere Quellen: `docs/ARCHITECTURE.md`, `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`,
`docs/CI_AND_QUALITY_GATES.md`, `docs/tasks/issue-72-implementation-plan.md`,
`AGENTS.md`, `lib/device_platform/AGENTS.md`, `lib/fermentation_app/AGENTS.md`,
`lib/device_platform_test_support/AGENTS.md`.

## 6. Bestandsaufnahme

| Aspekt | Befund |
|---|---|
| `src/main.cpp` | Unconditional Kopf (`app_config.hpp`, `device_platform.hpp`, `fermentation_application.hpp`, globale Instanzen `platform`/`application`, `startApplication()`) plus drei Zweige: `#if defined(ARDUINO)` (`setup()`/`loop()`, `Serial`-Bootzusammenfassung, 1000-ms-Heartbeat via `millis()`), sonst `int main()` (nativ, ein `update()`-Durchlauf, Exit ueber `application.ready()`). Kein `#elif defined(ESP_PLATFORM)`-Zweig mehr (mit `#72` entfernt/nie hinzugefuegt). |
| `DevicePlatform::begin/update` | `begin()` setzt `ready_` aus `PlatformStartupContext.configurationSafe`; `update()` ist ein reiner Kommentar-Stub, keine reale Wirkung. |
| `FermentationApplication::begin/update` | `begin()` prueft `platformServices.ready()`; `update()` ist ebenfalls ein reiner Kommentar-Stub ("Die Fermentationslogik wird issueweise... implementiert"). |
| `ITimeSource` | Port vorhanden (`monotonicMillis()`, `unixTimeSeconds()`), nur `VirtualTimeSource` (nativ) implementiert es. Kein Konsument in `DevicePlatform`/`FermentationApplication` heute. |
| Fachliche Updateperiode | Ausser dem 1000-ms-`Serial`-Heartbeat definiert der heutige Code **keine** fachliche Periode; `update()` beider Klassen ist wirkungslos. Keine neue Periode wird hier als Fachvertrag erfunden. |
| `#72`-Buildbasis | Root-`CMakeLists.txt` mit `EXTRA_COMPONENT_DIRS` (`device_platform`, `fermentation_app`), `MINIMAL_BUILD ON`, `IDF_VER == v6.0.2`-Guard; `main/CMakeLists.txt` + `main/issue72_build_stub.c` (`void app_main(void) {}`, reines C, keine Includes); `sdkconfig.defaults` (Ziel `esp32`, 4 MB Flash, kein PSRAM-Eintrag); Architekturguard mit IDF-/Arduino-/Adapter-Leak-Erkennung fuer `lib/device_platform/src` und `lib/fermentation_app/src` sowie `REQUIRES`-Allowlist fuer deren `CMakeLists.txt`. |
| `IResourceMonitor` | Laut `docs/ARCHITECTURE.md` vorgesehen, aber noch nicht implementiert; kein realer Konsument fuer eine volle Ressourcenport-Abstraktion in `#73`. |
| PlatformIO-LDF | Chain-Modus: nur ueber `#include` von `src/main.cpp` erreichte `lib/*`-Verzeichnisse werden gebaut (empirisch bestaetigt: `device_platform_test_support` erscheint nie im `#72`-Dependency-Graph, obwohl es in `lib/` liegt). Ein neues `lib/device_platform_esp_idf/`, das `src/main.cpp` nicht referenziert, wird von `native`/`esp32_bringup`/`esp32_release` **nicht** kompiliert. |

## 7. Paritaetsmatrix Arduino / nativ / ESP-IDF

| Aspekt | Arduino (`esp32_bringup`/`_release`) | nativ | ESP-IDF (`#73`, neu) |
|---|---|---|---|
| Einstieg | `setup()`/`loop()` | `int main()` | `extern "C" void app_main(void)` |
| Objekte | globale `platform`/`application` (aus `src/main.cpp`) | dieselben | eigene Instanzen in `main/app_main.cpp` (bewusste, begruendete Dopplung, siehe Abschnitt 8) |
| Start | `startApplication()`: `PlatformStartupContext{hasSafeDefaults(...)}`, `platform.begin()`, `application.begin(platform)` | identisch | aequivalente lokale Funktion, identischer `app_config`-Vertrag (Profil `esp32_bringup`) |
| Bootausgabe | `Serial.println(...)` mehrzeilig | keine | `ESP_LOGI` mit denselben semantischen Feldern (Abschnitt 16) |
| Updatezyklus | jede `loop()`-Iteration ungebremst, nur Heartbeat-Print alle 1000 ms gegated | ein Durchlauf | ein `vTaskDelayUntil`-getakteter 1000-ms-Zyklus fuer `update()` **und** Heartbeat gemeinsam (Abweichung, siehe Abschnitt 12) |
| Zeitbasis | `millis()` | keine | `ITimeSource` via `EspTimerTimeSource` (`esp_timer_get_time()`) |
| Fehlerpfad | `applicationStarted=false`, `loop()` fuehrt danach nur noch `return` aus | Exit-Code `1` | `app_main()` loggt Fehler und kehrt zurueck (offiziell unterstuetzt, Abschnitt 13) |
| Task-Modell | Arduino-Loop-Task (Framework-verwaltet) | Prozess | ESP-IDF-Main-Task, keine Zusatz-Task |

Abweichung ist bewusst und einzeln begruendet (Updatezyklus-Taktung); kein
stiller neuer Fachvertrag.

## 8. Zielarchitektur und Abhaengigkeitsgraph

```text
main (app_main.cpp, ESP-IDF Composition Root)
  |
  +--> fermentation_app --> device_platform
  |
  +--> device_platform_esp_idf --> device_platform
  |                             --> ESP-IDF (esp_timer)
  |
  +--> device_platform (direkt, fuer Typen wie PlatformStartupContext)
```

Verboten (unveraendert aus `#72`, durch Guard erzwungen):
`device_platform -> device_platform_esp_idf`,
`fermentation_app -> device_platform_esp_idf`,
`device_platform -> ESP-IDF`, `fermentation_app -> ESP-IDF`.

### `src/main.cpp` bleibt unveraendert

Ein dritter Praeprozessorzweig oder eine Extraktion des gemeinsamen
`platform`/`application`/`startApplication()`-Musters in eine gemeinsam
genutzte Datei wird **nicht** vorgezogen: Beides wuerde `src/main.cpp`
anfassen, was dieser Plan vermeidet. Die dadurch entstehende kleine Dopplung
(zwei Objektinstanzen plus eine kurze Startfunktion, ca. 15 Zeilen) in
`main/app_main.cpp` ist eine bewusste, begrenzte Abweichung von strikter DRY:
Arduino-/nativer Pfad und ESP-IDF-Pfad haben unterschiedliche Lebenszyklen
und Build-Systeme; eine gemeinsame Abstraktion nur fuer diese 15 Zeilen wuerde
mehr Kopplung erzeugen als sie einspart (KISS-Ausnahme gemaess
`docs/ENGINEERING_PRINCIPLES.md`).

## 9. `app_main()`-Lebenszyklus

Neue Datei `main/app_main.cpp` (ersetzt `main/issue72_build_stub.c`):

```cpp
#include "app_config.hpp"
#include "device_platform.hpp"
#include "esp_timer_time_source.hpp"
#include "fermentation_application.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

extern "C" void app_main(void) {
    // 1. Objekte (analog src/main.cpp, siehe Abschnitt 8)
    // 2. startApplication()-Aequivalent, sicherer Profilcheck
    // 3. bei Fehlschlag: Fehlerlog, return (Abschnitt 13)
    // 4. Bootzusammenfassung loggen (Abschnitt 16)
    // 5. vTaskDelayUntil-Schleife: update() + Heartbeat + Ressourcenlog
}
```

`extern "C"` ist zwingend, da ESP-IDFs C-Startupcode `app_main` mit
C-Bindung aufruft; die Implementierung selbst ist C++.

## 10. Start-/Fehlerpfad

Identischer sicherer Vertrag wie Arduino: `hasSafeDefaults(kActiveProfilePolicy)`
wird geprueft (statischer `static_assert` in `app_config.hpp` bleibt
unveraendert die Quelle der Wahrheit), `platform.begin(context)` und
`application.begin(platform)` muessen beide erfolgreich sein. Bei Erfolg:
Bootzusammenfassung, dann Updateschleife. Bei Fehlschlag: ein `ESP_LOGE`
mit demselben Statustext wie Arduino ("application: startup failed") und
sofortige Rueckkehr aus `app_main()` (siehe Abschnitt 13). Keine Hardware
wird in beiden Faellen beruehrt.

## 11. Schedulervertrag

KISS-Default: kein zusaetzlicher Task, keine Queue, keine Event Group, kein
Semaphore. `app_main()` selbst enthaelt die Update-/Heartbeat-Schleife und
laeuft im vorhandenen ESP-IDF-Main-Task. Ein Grund fuer eine Zusatz-Task ist
in `#73` nicht nachgewiesen ("ESP-IDF verwendet FreeRTOS" ist explizit kein
Grund, siehe Auftrag).

## 12. Updatekadenz

Der bestehende Code definiert ausser dem 1000-ms-Heartbeat keine fachliche
Periode (Abschnitt 6). Fuer `#73` gilt deshalb: `platform.update()`,
`application.update()` und der Heartbeat-Log laufen gemeinsam alle 1000 ms.

Bewusste Abweichung von der Arduino-Taktung: Arduino ruft `update()` bei
jeder ungebremsten `loop()`-Iteration auf und gated nur den Print. Eine
unbegrenzt schnelle ESP-IDF-Schleife ohne Delay waere ein Busy-Loop, das den
Idle-Task verhungern liesse und nach dem Default-Timeout von 5 s den
Task-Watchdog ausloesen wuerde (Abschnitt 5). Da `update()` heute ohnehin
wirkungslos ist, gibt es keinen fachlichen Nachteil, `update()` an dieselbe
1000-ms-Kadenz wie den Heartbeat zu binden. Eine feinere Periode ist eine
`OWNER_DECISION_REQUIRED`, sobald `update()` erstmals real etwas tut.

Technisch: `vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000))` verhindert
Drift durch Ausfuehrungszeit (im Gegensatz zu verketteten `vTaskDelay()`).
Bei `CONFIG_FREERTOS_HZ = 100` ist `pdMS_TO_TICKS(1000) = 100` exakt, keine
Rundung. `TickType_t`-Ueberlauf ist bei einer 1000-ms-Periode praktisch
irrelevant (FreeRTOS behandelt den Tickzaehler-Ueberlauf von
`vTaskDelayUntil` intern korrekt).

## 13. Task-/Stack-/Watchdogvertrag

| Wert | Status | Beleg |
|---|---|---|
| Main-Task-Prioritaet `1` | `VERIFIED_DEFAULT` | `esp_task.h`, nicht konfigurierbar, unveraendert uebernommen |
| Stackgroesse `3584` Byte (`CONFIG_ESP_MAIN_TASK_STACK_SIZE`) | `VERIFIED_DEFAULT` | Kconfig-Default; reale Ausnutzung erst durch `uxTaskGetStackHighWaterMark()` messbar -> `MEASUREMENT_REQUIRED` |
| Core-Affinitaet `CPU0` | `VERIFIED_DEFAULT` | Kconfig-Default, keine Aenderung geplant |
| Task-Watchdog | `VERIFIED_DEFAULT` | beobachtet Idle-Tasks, nicht `main`; `vTaskDelayUntil` haelt den Idle-Task regelmaessig lauffaehig, kein `esp_task_wdt_reset()` noetig |
| Rueckkehr aus `app_main()` bei Fehler | `VERIFIED_DEFAULT` | offiziell unterstuetzt (Abschnitt 5), sauberer Task-Abbau, kein Reboot |

Keine Kconfig-Aenderung an Stack-/Prioritaets-/Affinitaetswerten in `#73`;
`sdkconfig.defaults` bleibt unveraendert. Eine reale Stack-/Heapmessung
erfolgt einmalig im laufenden Betrieb (Abschnitt 15), keine Grenzwerte ohne
Messung.

## 14. Sichere Profilkonfiguration

`app_config.hpp` verlangt unveraendert genau ein `APP_PROFILE_*` sowie
`APP_TARGET_FLASH_MB`, `APP_REQUIRE_PSRAM`, `APP_WEB_OTA_ENABLED`,
`APP_REAL_ACTUATORS_ENABLED`. `#73` waehlt `esp32_bringup`-Semantik
(`HARDWARE_UNVERIFIED`, `LockedForBringup`), textidentisch mit
`platformio.ini`s `[env:esp32_bringup]`:

```cmake
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    APP_PROFILE_ESP32_BRINGUP=1
    APP_TARGET_FLASH_MB=4
    APP_REQUIRE_PSRAM=0
    APP_WEB_OTA_ENABLED=0
    APP_REAL_ACTUATORS_ENABLED=0
)
```

Nur auf dem tatsaechlich konsumierenden `main`-Target, keine neue
Konfigurationsplattform, kein Kconfig-Menu fuer diese fuenf Konstanten,
keine Verteilung auf mehrere IDF-Komponenten. Diese Dopplung zwischen
`platformio.ini` und CMake ist dieselbe, in `#72` bewusst vermiedene
Konstruktion, die jetzt notwendig wird, weil `#73` echte Sicherheitsparitaet
statt eines reinen Compile-Ankers liefert; sie bleibt auf genau diese eine
CMake-Stelle begrenzt und entfaellt mit `#74`. Keine reale Aktorfreigabe wird
dadurch kompilierbar (`APP_REAL_ACTUATORS_ENABLED=0`, durch `static_assert`
in `app_config.hpp` erzwungen).

## 15. Zeitadapter

Neues Modul `lib/device_platform_esp_idf/src/esp_timer_time_source.hpp`/`.cpp`,
Namespace `device_platform_esp_idf`, Klasse `EspTimerTimeSource final :
public device_platform::ITimeSource`:

- `monotonicMillis()`: Konstruktor speichert `baselineMicros_ =
  esp_timer_get_time()`; `monotonicMillis()` liefert
  `(esp_timer_get_time() - baselineMicros_) / 1000`. Dadurch gilt exakt der
  bestehende Vertrag „seit Erstellung dieser Instanz“ (wie
  `VirtualTimeSource`), nicht „seit Boot“ — unabhaengig vom genauen
  Konstruktionszeitpunkt in `app_main()`.
- `unixTimeSeconds()`: liefert immer `std::nullopt`. `#73` bindet keine
  NTP-/RTC-Quelle an; keine erfundene UTC aus Uptime oder Buildzeit.
- Negativwerte/Ueberlauf: `esp_timer_get_time()` ist laut Primaerquelle
  monoton mit 64-Bit-Bereich; die Differenz `aktuell - baseline` ist per
  Konstruktion nie negativ. Kein zusaetzlicher Guard noetig.
- Kein Vertragswechsel an `ITimeSource` selbst.

Realer Konsument: `main/app_main.cpp` konstruiert genau eine
`EspTimerTimeSource`-Instanz und loggt `monotonicMillis()` als Uptime-Feld
im periodischen Heartbeat (Abschnitt 16). Damit hat der Adapter einen
konkreten Verbraucher im Composition Root, wie im Auftrag als zulaessig
benannt, ohne dass `DevicePlatform`/`FermentationApplication` heute Zeit
brauchen.

`lib/device_platform_esp_idf/CMakeLists.txt`:

```cmake
idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "src"
    REQUIRES device_platform
    PRIV_REQUIRES esp_timer
)
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
```

`esp_timer.h` wird nur im `.cpp` includiert; der oeffentliche Header bleibt
frei von ESP-IDF-Typen (nur `int64_t`), obwohl das fuer diese Komponente
architektonisch erlaubt waere.

## 16. Logging

Kein neuer `ILogger`-Port: kein portabler Konsument braucht ihn, und
`docs/ARCHITECTURE.md` sieht keinen vor. KISS-Default: eine kleine lokale
Loggingfunktion direkt in `main/app_main.cpp` (anonymer Namespace),
semantisch aequivalent zu Arduinos `printBootSummary()`:

```text
ESP_LOGI(TAG, "%s", kProjectName);
ESP_LOGI(TAG, "profile: %s", profileName(policy.profile));
ESP_LOGI(TAG, "hardware state: %s", hardwareStateName(policy.startupHardwareState));
ESP_LOGI(TAG, "actuator policy: %s", actuatorPolicyName(policy.actuatorPolicy));
ESP_LOGI(TAG, "real actuators: disabled");
ESP_LOGI(TAG, "application: ready" | "application: startup failed");
```

Heartbeat alle 1000 ms: `ESP_LOGI(TAG, "heartbeat: safe test mode, uptime_ms=%llu, free_heap=%u, stack_hwm=%u", ...)`
(Ressourcenwerte siehe Abschnitt 18). Kein `ILogger`, kein Ringbuffer, keine
Queue, kein Remote-Logging, keine Fremdbibliothek.

## 17. Resetursache

Entscheidung: **keine Resetursache in `#73`.** Es gibt keinen konkreten
Konsumenten (`SAFE_BOOT`/Fehlerklassen sind fachliche Recovery-Logik aus
Folgeissues, nicht Teil von `#73`); der heutige Arduino-Pfad meldet ebenfalls
keine Resetursache. `esp_reset_reason()` wird nicht eingebunden. Eine
spaetere kleine Bootdiagnose bleibt moeglich, sobald ein realer Verbraucher
existiert (`SPIKE_REQUIRED` fuer ein Folgeissue, nicht `#73`).

## 18. Ressourcenmessung

Nur lokale Messpunkte, keine Instrumentierung im Fachkern, keine feste
Baseline (die liefert `#74`): `esp_get_free_heap_size()` und
`uxTaskGetStackHighWaterMark(NULL)` werden direkt in der 1000-ms-Schleife in
den Heartbeat-Log integriert (Abschnitt 16) — derselbe Takt, keine
zusaetzliche Schedulinglogik. Das liefert einen kontinuierlichen, aber
absichtlich nicht persistierten Laufzeitnachweis fuer den Hardware-Smoke-Test
(Abschnitt 23). Keine Grenzwerte ohne Messung.

## 19. `device_platform_esp_idf` — Struktur

```text
lib/device_platform_esp_idf/
├── CMakeLists.txt
└── src/
    ├── esp_timer_time_source.hpp
    └── esp_timer_time_source.cpp
```

Kein leeres Modul, keine generische Wrapperplattform, keine Schatten-API,
kein GPIO-/NVS-/WLAN-/Web-/Display-/Sensor-/Aktoradapter. Genau ein realer
Adapter mit realem Konsumenten (Abschnitt 15). `EXTRA_COMPONENT_DIRS` im
Root-`CMakeLists.txt` erhaelt einen dritten Eintrag:

```cmake
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/lib/device_platform"
    "${CMAKE_CURRENT_LIST_DIR}/lib/fermentation_app"
    "${CMAKE_CURRENT_LIST_DIR}/lib/device_platform_esp_idf"
)
```

`main/CMakeLists.txt` wird auf den echten Composition Root umgestellt:

```cmake
idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "../include"
    PRIV_REQUIRES device_platform fermentation_app device_platform_esp_idf
)
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    APP_PROFILE_ESP32_BRINGUP=1 APP_TARGET_FLASH_MB=4 APP_REQUIRE_PSRAM=0
    APP_WEB_OTA_ENABLED=0 APP_REAL_ACTUATORS_ENABLED=0)
```

`-std=gnu++17` ist jetzt auch fuer `main` noetig, da die Komponente von
reinem C (Stub) auf C++ (`app_main.cpp`) wechselt.

**PlatformIO-Auswirkung:** `lib/device_platform_esp_idf/` wird von
`src/main.cpp` nicht `#include`t und erscheint daher im Chain-Modus-LDF nicht
im Dependency-Graph von `native`/`esp32_bringup`/`esp32_release` (empirisch
belegtes Verhalten, Abschnitt 6). Als Gate wird `pio run`s
Dependency-Graph-Ausgabe fuer alle drei Envs manuell gegen diese Erwartung
geprueft (Abschnitt 22).

## 20. Architekturguard-Erweiterung

Erweitert `scripts/check_architecture_boundaries.py` (bestehende
Mechanismen aus `#72` wiederverwendet, keine neue Parserklasse):

1. `COMPONENT_REQUIRES_ALLOWLIST` erhaelt einen dritten Eintrag:
   `"lib/device_platform_esp_idf/CMakeLists.txt": frozenset({"device_platform", "esp_timer"})`.
   Derselbe Tokenparser aus `#72` prueft ihn automatisch mit.
2. Neuer `add_reference_violations(...)`-Aufruf (bestehende Funktion) fuer
   `lib/device_platform_esp_idf/`: verbietet `fermentation_app` und
   `device_platform_test_support` als Referenz — dieselbe Technik wie die
   bestehende Rueckwaertspruefung fuer `device_platform`.
3. Neuer `add_reference_violations(...)`-Aufruf fuer das Verzeichnis `main/`
   (bisher nicht geprueft, da es in `#72` nur einen leeren C-Stub enthielt):
   verbietet `device_platform_test_support`, analog zur bereits bestehenden
   Pruefung fuer `src/main.cpp`.
4. `IDF_LEAK_PORTABLE_ROOTS` bleibt unveraendert bei genau
   `lib/device_platform/src` und `lib/fermentation_app/src` —
   `device_platform_esp_idf` und `main/` duerfen ESP-IDF-Header enthalten
   (architektonisch vorgesehen) und werden bewusst **nicht** in diese Liste
   aufgenommen.

Selftest-Erweiterung (tabellengesteuert, dem `#72`-Muster aus dem
Reviewfix folgend): ein sauberer Fall (`device_platform_esp_idf` mit
erlaubtem `#include "esp_timer.h"` und `REQUIRES device_platform` bleibt
`PASS`) sowie mindestens zwei Verstossfaelle (Referenz auf
`fermentation_app` aus `device_platform_esp_idf` wird erkannt; unerlaubte
`REQUIRES` in dessen `CMakeLists.txt` wird erkannt). Keine grosse
Permutationsmatrix, kein neuer allgemeiner Parser.

## 21. Adopt-or-build und Lizenzen

1. Bestehender Projektport (`ITimeSource`) wiederverwendet.
2. Offizielle ESP-IDF-Kernfunktionen genutzt: `esp_timer_get_time()`
   (Komponente `esp_timer`, Apache-2.0, Teil des ESP-IDF-Kerns),
   `esp_log.h`/`esp_system.h`/FreeRTOS-Delay (Common-Komponenten, keine
   zusaetzliche Einbindung).
3. Keine externe Komponente, kein `idf_component.yml`, kein
   `dependencies.lock`, kein Arduino-as-component.
4. Eigene Logik ausschliesslich fuer den projektspezifischen
   `EspTimerTimeSource`-Adapter und den Composition Root.

Kein unerwarteter Bedarf identifiziert; falls waehrend der Umsetzung doch
einer auftritt, gilt `SPIKE_REQUIRED` statt eigenmaechtiger Auswahl.

## 22. Voraussichtliche Dateiliste

**Neu:** `lib/device_platform_esp_idf/CMakeLists.txt`,
`lib/device_platform_esp_idf/src/esp_timer_time_source.hpp`,
`lib/device_platform_esp_idf/src/esp_timer_time_source.cpp`,
`main/app_main.cpp`.

**Entfernt:** `main/issue72_build_stub.c`.

**Geaendert:** `CMakeLists.txt` (dritter `EXTRA_COMPONENT_DIRS`-Eintrag),
`main/CMakeLists.txt` (echter Composition Root statt Stub),
`scripts/check_architecture_boundaries.py` (Abschnitt 20),
`docs/CI_AND_QUALITY_GATES.md` (kurze Ergaenzung: Stub durch echten
Composition Root ersetzt, neue Adapterkomponente erwaehnt), `CHANGELOG.md`
(ein Eintrag).

**Unveraendert:** `src/main.cpp`, `platformio.ini`,
`lib/device_platform_test_support/`, Fachmodelle, Persistenz, Wireformate,
Safetylogik, Hardwareadapter, `.github/workflows/build.yml`,
`sdkconfig.defaults` (kein neuer Kconfig-Bedarf in `#73`),
`include/app_config.hpp` (kein nachgewiesener Vertragsmangel).

## 23. Commit-Schnitt

| # | Commit | Buildbar? |
|---|---|---|
| 1 | Reale Adapterkomponente: `lib/device_platform_esp_idf/` (`CMakeLists.txt`, `EspTimerTimeSource`), dritter `EXTRA_COMPONENT_DIRS`-Eintrag | Ja — kompiliert und linkt weiterhin mit dem alten Stub, da der Adapter noch von niemandem referenziert wird |
| 2 | Echter Composition Root: `main/app_main.cpp`, `main/CMakeLists.txt` umgestellt, `main/issue72_build_stub.c` entfernt | Ja — vollstaendiger `idf.py build` mit echtem `app_main()` |
| 3 | Guards und Dokumentation: Architekturguard-Erweiterung, `docs/CI_AND_QUALITY_GATES.md`, `CHANGELOG.md` | Ja |

Kein Mischcommit, kein nicht buildbarer Zwischenstand, kein CI-Commit, kein
Adapter ohne Konsumenten in Commit 1 (der Konsument entsteht planmaessig in
Commit 2 — Zwischenstand bleibt trotzdem buildbar, da ESP-IDF unreferenzierte
Komponenten kompiliert, aber nicht zwingend verlinkt).

## 24. Tests und Nachweise

```bash
pio test -e native
pio run -e esp32_bringup
pio run -e esp32_release
python scripts/check_architecture_boundaries.py
python scripts/check_architecture_boundaries.py --selftest
python scripts/check_platformio_config.py
python scripts/check_secrets.py
python scripts/selftest_quality_gates.py
git diff --check

. "$IDF_PATH/export.sh"
idf.py build
```

Positive Nachweise: echter `app_main()` statt Stub; `device_platform_esp_idf`
wird gebaut; `device_platform`/`fermentation_app` weiterhin gebaut; `gnu++17`
fuer alle drei C++-Komponenten (`compile_commands.json`-Pruefung wie in
`#72`); ESP-IDF `v6.0.2`; kein Arduino im IDF-Build; kein Test-Support im
Komponentenbaum; gueltiges BIN/ELF/Mapfile.

Negative Nachweise (manuell, dann zurueckgesetzt, wie im `#72`-Reviewfix
etabliert): IDF-Leak in `device_platform`/`fermentation_app` weiterhin
erkannt; `fermentation_app`-Referenz aus `device_platform_esp_idf` erkannt;
unerlaubte `REQUIRES` in dessen `CMakeLists.txt` erkannt; falscher `IDF_VER`
bricht weiterhin fail-fast ab; `APP_REAL_ACTUATORS_ENABLED=1` versucht ->
`static_assert`-Fehlschlag in `app_config.hpp` bestaetigt weiterhin
Kompilierabbruch.

## 25. Hardware-Smoke-Test

Status: `TBD_HARDWARE` — keine ESP32-Hardware in dieser Planungsphase
angeschlossen oder geflasht.

Manuelle Owner-Prozedur nach Freigabe und Flashen:

```bash
idf.py -p <PORT> flash monitor
```

Erwartete serielle Ausgabe: Bootzusammenfassung (Projektname, Profil
`esp32_bringup`, `HARDWARE_UNVERIFIED`, `LockedForBringup`, „real actuators:
disabled“, „application: ready“), danach periodische
`heartbeat: safe test mode, uptime_ms=..., free_heap=..., stack_hwm=...`-Zeilen
im 1000-ms-Abstand ueber mindestens 30 s ohne Neustart, ohne
Watchdog-Reset-Log und ohne jeden GPIO-/Sensor-/Display-/WLAN-/Webzugriff.
Kein automatischer Owner-Test in dieser Phase; Build- und statische Nachweise
(Abschnitt 24) sind vollstaendig unabhaengig davon. Keine vollstaendige
Laufzeitparitaet wird ohne diesen Test behauptet.

## 26. Security-, Safety-, Recovery- und Persistenzgrenzen

Keine Aenderung an Fachmodellen, Wireformaten, Persistenz- oder
Recoverylogik. `APP_REAL_ACTUATORS_ENABLED=0` bleibt durch `static_assert`
erzwungen; kein Pfad in `#73` kann reale Aktoren freigeben. Kein Netzwerk-,
Web- oder Auth-Code. Der einzige neue Fehlerpfad (Startfehler in
`app_main()`) beruehrt keine Persistenz und keine Hardware (Abschnitt 10).

## 27. SOLID, DRY, KISS (gebunden an den geplanten Diff)

- **SRP:** `app_main.cpp` verdrahtet nur; `EspTimerTimeSource` liefert nur
  Zeit; die lokale Loggingfunktion loggt nur.
- **OCP:** `device_platform`/`fermentation_app` bleiben unveraendert; die
  IDF-Unterstuetzung kommt rein additiv ueber eine neue Komponente hinzu.
- **LSP:** `EspTimerTimeSource` erfuellt exakt den bestehenden
  `ITimeSource`-Vertrag (siehe Abschnitt 15), keine Sonderfaelle.
- **ISP:** kein neuer Sammelport; `ITimeSource` bleibt so schmal wie heute.
- **DIP:** `fermentation_app` und `device_platform_esp_idf` haengen nur von
  `device_platform`-Ports ab; `main` kennt konkrete Adapter und Anwendung.
- **DRY:** ein Composition Root, eine Zeitadapterklasse, eine lokale
  Logginglogik, kein zweiter Scheduler, keine doppelte Quellliste (`SRC_DIRS`
  wie in `#72`). Die einzige bewusste Dopplung (Objektinstanzen in
  `main/app_main.cpp` statt Extraktion aus `src/main.cpp`) ist in Abschnitt 8
  einzeln begruendet. Die `APP_PROFILE_*`-Makro-Dopplung ist auf die eine
  Stelle `main/CMakeLists.txt` begrenzt und in Abschnitt 14 begruendet.
- **KISS:** keine Zusatz-Task, kein Resetursache-Port ohne Konsument, keine
  Ressourcenport-Abstraktion, kein `ILogger`, kein Kconfig-Menu fuer fuenf
  Konstanten.

## 28. Offene Ownerentscheidungen

1. Feinere Updateperiode, sobald `DevicePlatform::update()`/
   `FermentationApplication::update()` erstmals real etwas tun (heute nicht
   entscheidungsrelevant, da beide wirkungslos sind).
2. Ob eine kuenftige Bootdiagnose via `esp_reset_reason()` in einem
   Folgeissue sinnvoll ist (in `#73` bewusst nicht eingebunden).

Keine dieser Fragen blockiert die Umsetzung von `#73`.

## 29. `SPIKE_REQUIRED` / `MEASUREMENT_REQUIRED` / `TBD_HARDWARE`

- `TBD_HARDWARE`: Hardware-Smoke-Test (Abschnitt 25).
- `MEASUREMENT_REQUIRED`: reale Stack-Watermark- und Heap-Auslastung nach
  kontrollierter Laufzeit (Abschnitt 13, gemessen ueber den Heartbeat-Log,
  Abschnitt 18).
- Kein `SPIKE_REQUIRED` in `#73`: alle geplanten APIs sind bereits anhand
  der lokalen Primaerquellen verifiziert (Abschnitt 5).

## 30. Verbotene Vorwegnahmen

Siehe Abschnitt 4 (Nicht-Ziele); zusaetzlich ausdruecklich: keine
`esp_reset_reason()`-Einbindung, kein `ILogger`-Port, keine zweite
Composition-Root-Datei, keine Aenderung an `src/main.cpp`, keine
Kconfig-Overlay-Differenzierung Bring-up/CI/Release, keine
Fremdkomponente/kein Lockfile.

## 31. Abgrenzung zu `#74`

`#74` bleibt zustaendig fuer: CI-Migration (`idf.py build` in GitHub
Actions), reproduzierbare IDF-Installation in Actions, finale
Ressourcenbaseline und -grenzwerte, Entfernung des Arduino-Hauptframework-
Pfads, Upgradevertrag, endgueltigen Uebergang. `#73` liefert ausschliesslich
den lokal nachgewiesenen Laufzeitpfad, auf dem `#74` aufsetzt.

## Abnahmekriterien

- `idf.py build` baut mit echtem `app_main()`; `main/issue72_build_stub.c`
  entfernt.
- `device_platform_esp_idf` real, mit `EspTimerTimeSource` als einzigem
  Adapter und einem echten Konsumenten im Composition Root.
- `esp_timer_get_time()` korrekt in Millisekunden konvertiert, seit
  Instanzerstellung; `unixTimeSeconds()` bleibt `std::nullopt`.
- Bestehender `DevicePlatform`-/`FermentationApplication`-Kern unveraendert
  wiederverwendet; `HARDWARE_UNVERIFIED`, Aktoren deaktiviert, Profil
  `esp32_bringup`.
- Bootzusammenfassung und sicherer Startfehler (kein Busy-Loop, keine
  automatische Reboot-Schleife, `app_main()`-Rueckkehr statt Endlosschleife).
- Kontrollierter 1000-ms-Heartbeat via `vTaskDelayUntil`, keine
  Zusatz-Task ohne nachgewiesenen Grund.
- Task-/Stack-/Watchdogannahmen dokumentiert (Abschnitt 13) und ueber
  Heartbeat-Log messbar.
- Architekturguard erkennt weiterhin alle `#72`-Faelle plus die drei neuen
  `#73`-Faelle (Abschnitt 20); kein Test-Support im Produktbuild.
- `pio test -e native`, `pio run -e esp32_bringup`, `pio run -e esp32_release`,
  `idf.py build` und die bestehende GitHub-Actions-Pipeline gruen; **keine**
  CI-Aenderung.
- Hardware-Smoke-Test bestanden oder weiterhin `TBD_HARDWARE` dokumentiert.
- Keine Fach-, Persistenz-, Wire-, Safety- oder Recoveryaenderung; keine
  Fremdkomponente.
- Tatsaechlicher Diff gegen `docs/ENGINEERING_PRINCIPLES.md` (SOLID, DRY,
  KISS) geprueft (Abschnitt 27).
