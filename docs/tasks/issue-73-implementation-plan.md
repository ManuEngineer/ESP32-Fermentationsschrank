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
- Ueberholter Planstand: `84b506f06953e68aba03e06b951d8b59fd8141e6` (vollstaendig
  ueberarbeitet nach dem PR-Review; jede vorherige Freigabe bezog sich auf
  diesen Stand nicht)

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
- Aenderung an `src/main.cpp` (siehe Abschnitt 8);
- eine fachliche Updateperiode fuer `update()` (siehe Abschnitt 12).

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
| FreeRTOS-Tickrate | `components/freertos/Kconfig:33-38` | `CONFIG_FREERTOS_HZ` Default `100` (10 ms/Tick); `pdMS_TO_TICKS(1)` kann bei 100 Hz auf `0` runden, deshalb Yield ueber einen festen Mindest-Tick, nicht ueber `pdMS_TO_TICKS(1)` |
| `esp_timer_get_time()` | `esp_timer.h` / v6.0.2-Doku | `int64_t` Mikrosekunden seit ESP-Timer-Init kurz vor `app_main()`; monoton, ISR-sicher, 64 Bit (kein praktischer Ueberlauf) |
| Logging | `esp_log.h` / v6.0.2-Doku | `ESP_LOGI/W/E(TAG, ...)`, Komponente `log` bereits Common-Requires, `CONFIG_LOG_DEFAULT_LEVEL` |
| `esp_reset_reason()`/Heap-APIs | `esp_system.h` / v6.0.2-Doku | Teil der Common-Komponente `esp_system`, keine zusaetzliche `REQUIRES` noetig |
| `uxTaskGetStackHighWaterMark()` | FreeRTOS-API, ESP-IDF-Portierung | liefert den Watermark in **Bytes** (nicht Words) auf der ESP-IDF-FreeRTOS-Portierung |
| `app_config.hpp`-Aktorguard | `include/app_config.hpp` | `#if APP_REAL_ACTUATORS_ENABLED != 0` / `#error "Issue #9 must not enable real actuators"` — ein **Praeprozessorfehler**, kein `static_assert`; zusaetzlich existiert weiter unten `static_assert(!kRealActuatorsEnabledByDefault)` als zweite, spaetere Absicherung des abgeleiteten `constexpr`-Werts |

Weitere Quellen: `docs/ARCHITECTURE.md`, `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`,
`docs/CI_AND_QUALITY_GATES.md`, `docs/tasks/issue-72-implementation-plan.md`,
`AGENTS.md`, `lib/device_platform/AGENTS.md`, `lib/fermentation_app/AGENTS.md`,
`lib/device_platform_test_support/AGENTS.md`.

## 6. Bestandsaufnahme

| Aspekt | Befund |
|---|---|
| `src/main.cpp` | Unconditional Kopf (`app_config.hpp`, `device_platform.hpp`, `fermentation_application.hpp`, globale Instanzen `platform`/`application`, `startApplication()`) plus drei Zweige: `#if defined(ARDUINO)` (`setup()`/`loop()`, `Serial`-Bootzusammenfassung, 1000-ms-Heartbeat via `millis()`), sonst `int main()` (nativ, ein `update()`-Durchlauf, Exit ueber `application.ready()`). Kein `#elif defined(ESP_PLATFORM)`-Zweig. |
| `DevicePlatform::begin/update` | `begin()` setzt `ready_` aus `PlatformStartupContext.configurationSafe`; `update()` ist ein reiner Kommentar-Stub, keine reale Wirkung. |
| `FermentationApplication::begin/update` | `begin()` prueft `platformServices.ready()`; `update()` ist ebenfalls ein reiner Kommentar-Stub ("Die Fermentationslogik wird issueweise... implementiert"). |
| `ITimeSource` | Port vorhanden (`monotonicMillis()`, `unixTimeSeconds()`), nur `VirtualTimeSource` (nativ) implementiert es. Kein Konsument in `DevicePlatform`/`FermentationApplication` heute. |
| Fachliche Updateperiode | Ausser dem 1000-ms-`Serial`-Heartbeat definiert der heutige Code **keine** fachliche Periode; `update()` beider Klassen ist wirkungslos. Arduinos `loop()` ruft `update()` bei **jeder** Iteration ungebremst auf, nur der Print ist gegated. |
| `#72`-Buildbasis | Root-`CMakeLists.txt` mit `EXTRA_COMPONENT_DIRS` (`device_platform`, `fermentation_app`), `MINIMAL_BUILD ON`, `IDF_VER == v6.0.2`-Guard; `main/CMakeLists.txt` + `main/issue72_build_stub.c` (`void app_main(void) {}`, reines C, keine Includes); `sdkconfig.defaults` (Ziel `esp32`, 4 MB Flash, kein PSRAM-Eintrag); Architekturguard mit IDF-/Arduino-/Adapter-Leak-Erkennung fuer `lib/device_platform/src` und `lib/fermentation_app/src` sowie `REQUIRES`-Allowlist fuer deren `CMakeLists.txt`. |
| `MINIMAL_BUILD ON`-Auswirkung | Baut nur `main`, Common-Komponenten und die von `main` tatsaechlich erreichbaren transitiven Abhaengigkeiten. Eine ueber `EXTRA_COMPONENT_DIRS` nur auffindbare, aber von `main` nicht referenzierte Komponente wird dadurch **nicht zuverlaessig** kompiliert — relevant fuer den Commit-Schnitt (Abschnitt 22). |
| `IResourceMonitor` | Laut `docs/ARCHITECTURE.md` vorgesehen, aber noch nicht implementiert; kein realer Konsument fuer eine volle Ressourcenport-Abstraktion in `#73`. |
| PlatformIO-LDF | Chain-Modus: nur ueber `#include` von `src/main.cpp` erreichte `lib/*`-Verzeichnisse werden gebaut (empirisch bestaetigt: `device_platform_test_support` erscheint nie im `#72`-Dependency-Graph, obwohl es in `lib/` liegt). Ein neues `lib/device_platform_esp_idf/`, das `src/main.cpp` nicht referenziert, wird von `native`/`esp32_bringup`/`esp32_release` **nicht** kompiliert. |

## 7. Paritaetsmatrix Arduino / nativ / ESP-IDF

| Aspekt | Arduino (`esp32_bringup`/`_release`) | nativ | ESP-IDF (`#73`, neu) |
|---|---|---|---|
| Einstieg | `setup()`/`loop()` | `int main()` | `extern "C" void app_main(void)` |
| Objekte | globale `platform`/`application` (aus `src/main.cpp`) | dieselben | eigene Instanzen in `main/app_main.cpp` (bewusste, begruendete Dopplung, siehe Abschnitt 8) |
| Start | `startApplication()`: `PlatformStartupContext{hasSafeDefaults(...)}`, `platform.begin()`, `application.begin(platform)` | identisch | aequivalente lokale Funktion, identischer `app_config`-Vertrag (Profil `esp32_bringup`) |
| Bootausgabe | `Serial.println(...)` mehrzeilig, immer vollstaendig (auch bei Fehlschlag) | keine | `ESP_LOGI`/`ESP_LOGE` mit denselben Feldern, **immer vollstaendig ausgegeben**, unabhaengig vom Ergebnis (Abschnitt 10) |
| Updatezyklus | jede `loop()`-Iteration ruft `update()` ungebremst auf; nur der Heartbeat-Print ist auf 1000 ms gegated | ein Durchlauf | eine kooperative Endlosschleife: `update()` bei jedem Durchlauf, Heartbeat separat zeitgesteuert, ein Ein-Tick-`vTaskDelay`-Yield pro Durchlauf (Abschnitt 12) — **keine** Frequenzaenderung an `update()` gegenueber Arduino |
| Zeitbasis | `millis()` | keine | `ITimeSource` via `EspTimerTimeSource` (`esp_timer_get_time()`) |
| Fehlerpfad | `applicationStarted=false`, `loop()` fuehrt danach nur noch `return` je Iteration aus (Task laeuft technisch weiter) | Exit-Code `1` | `app_main()` gibt die vollstaendige Zusammenfassung mit Fehlerstatus aus und kehrt danach zurueck (Abschnitt 10, offiziell unterstuetzt) |
| Task-Modell | Arduino-Loop-Task (Framework-verwaltet) | Prozess | ESP-IDF-Main-Task, keine Zusatz-Task |

## 8. Zielarchitektur und Abhaengigkeitsgraph

```text
main (app_main.cpp, ESP-IDF Composition Root)
  |
  +--> fermentation_app --> device_platform
  |
  +--> device_platform_esp_idf --> device_platform
  |                             --> ESP-IDF (esp_timer, privat)
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
#include <cinttypes>

#include "app_config.hpp"
#include "device_platform.hpp"
#include "esp_timer_time_source.hpp"
#include "fermentation_application.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void) {
    // 1. Objekte (analog src/main.cpp, siehe Abschnitt 8), EspTimerTimeSource
    // 2. startApplication()-Aequivalent
    // 3. Bootzusammenfassung IMMER vollstaendig ausgeben (Abschnitt 10)
    // 4. bei Fehlschlag: return (kein Loop-Eintritt)
    // 5. erste Ressourcenmessung nach erfolgreicher Initialisierung
    // 6. kooperative Endlosschleife: update() + zeitgesteuerter Heartbeat +
    //    Ein-Tick-Yield + zweite Ressourcenmessung nach ~30 s (Abschnitt 12/18)
}
```

`extern "C"` ist zwingend, da ESP-IDFs C-Startupcode `app_main` mit
C-Bindung aufruft; die Implementierung selbst ist C++. `<cinttypes>` wird fuer
`PRIu64` benoetigt (Abschnitt 16), damit `uint64_t`-Formatierung unter
`-Werror` typkorrekt bleibt statt eine plattformabhaengige `%llu`-Annahme zu
treffen.

## 10. Start-/Fehlerpfad

Identischer sicherer Vertrag wie Arduino: `hasSafeDefaults(kActiveProfilePolicy)`
wird geprueft (statischer `static_assert` in `app_config.hpp` bleibt
unveraendert die Quelle der Wahrheit), `platform.begin(context)` und
`application.begin(platform)` muessen beide erfolgreich sein.

Verbindlicher Ablauf (Parität zu Arduinos `printBootSummary()`, das nach
jedem Startversuch unabhaengig vom Ergebnis vollstaendig laeuft):

1. Startversuch (`platform.begin(...)`, `application.begin(...)`).
2. Vollstaendige Zusammenfassung **unabhaengig vom Ergebnis**: Projektname,
   Profil, Hardwarezustand, Aktorpolicy, „real actuators: disabled“.
3. Anwendungsstatus als letzte Zeile: Erfolg via `ESP_LOGI`
   ("application: ready"), Fehler via `ESP_LOGE`
   ("application: startup failed").
4. Bei Fehler: `app_main()` kehrt danach sofort zurueck (kein Schleifeneintritt,
   kein Update, kein Heartbeat, keine Ressourcenmessung) — offiziell
   unterstuetztes Verhalten (Abschnitt 5), sauberer Task-Abbau, kein Reboot.
5. Bei Erfolg: erste Ressourcenmessung, danach Eintritt in die kooperative
   Schleife (Abschnitt 12).

Keine Hardware wird in beiden Faellen beruehrt.

## 11. Schedulervertrag

KISS-Default: kein zusaetzlicher Task, keine Queue, keine Event Group, kein
Semaphore. `app_main()` selbst enthaelt die Update-/Heartbeat-Schleife und
laeuft im vorhandenen ESP-IDF-Main-Task. Ein Grund fuer eine Zusatz-Task ist
in `#73` nicht nachgewiesen ("ESP-IDF verwendet FreeRTOS" ist explizit kein
Grund, siehe Auftrag).

## 12. Updatekadenz

Arduino ruft `update()` bei jeder ungebremsten `loop()`-Iteration auf; nur
der Heartbeat-Print ist auf 1000 ms begrenzt. `#73` uebernimmt das exakt so
und erfindet keine neue Frequenzgarantie fuer `update()`:

```cpp
constexpr TickType_t kCooperativeYieldTicks = 1;  // mind. 1 Tick, siehe unten
uint64_t lastHeartbeatMs = timeSource.monotonicMillis();

for (;;) {
    platform.update();
    application.update();

    const uint64_t nowMs = timeSource.monotonicMillis();
    if (nowMs - lastHeartbeatMs >= 1000U) {
        lastHeartbeatMs = nowMs;
        logHeartbeat(nowMs);
    }

    vTaskDelay(kCooperativeYieldTicks);
}
```

Prinzipbeispiel; die tatsaechliche Implementierung integriert zusaetzlich die
zweite Ressourcenmessung nach ~30 s (Abschnitt 18) in denselben Schleifenkopf.

Verbindlich:

- genau eine Schleife, keine Zusatz-Task;
- `update()` bei **jedem** Durchlauf, wie im Arduino-Pfad — keine
  Frequenzaenderung;
- der Heartbeat bleibt separat zeitgesteuert ueber `monotonicMillis()`;
- `vTaskDelay(kCooperativeYieldTicks)` mit `kCooperativeYieldTicks = 1`
  (fester Mindest-Tick), **nicht** `pdMS_TO_TICKS(1)`: Bei
  `CONFIG_FREERTOS_HZ = 100` kann `pdMS_TO_TICKS(1)` auf `0` runden, was den
  Yield entfallen liesse;
- der Ein-Tick-Yield ist reine Schedulerkooperation (haelt den Idle-Task
  regelmaessig lauffaehig, Abschnitt 13), **keine** fachliche Updateperiode
  und **keine** Frequenzgarantie fuer `update()`;
- eine spaetere fachliche Periode wird erst mit dem ersten realen `update()`-
  Konsumenten geplant, nicht in `#73`.

Damit entfaellt die in einer frueheren Planfassung offene Frage nach einer
„feineren Updateperiode“ vollstaendig (siehe Abschnitt 28).

## 13. Task-/Stack-/Watchdogvertrag

| Wert | Status | Beleg |
|---|---|---|
| Main-Task-Prioritaet `1` | `VERIFIED_DEFAULT` | `esp_task.h`, nicht konfigurierbar, unveraendert uebernommen |
| Stackgroesse `3584` Byte (`CONFIG_ESP_MAIN_TASK_STACK_SIZE`) | `VERIFIED_DEFAULT` | Kconfig-Default; reale Ausnutzung erst durch `uxTaskGetStackHighWaterMark()` messbar -> `MEASUREMENT_REQUIRED` |
| Core-Affinitaet `CPU0` | `VERIFIED_DEFAULT` | Kconfig-Default, keine Aenderung geplant |
| Task-Watchdog | `VERIFIED_DEFAULT` | beobachtet Idle-Tasks, nicht `main`; der Ein-Tick-`vTaskDelay`-Yield bei jedem Schleifendurchlauf haelt den Idle-Task regelmaessig lauffaehig, kein `esp_task_wdt_reset()` noetig |
| Rueckkehr aus `app_main()` bei Fehler | `VERIFIED_DEFAULT` | offiziell unterstuetzt (Abschnitt 5), sauberer Task-Abbau, kein Reboot |

Keine Kconfig-Aenderung an Stack-/Prioritaets-/Affinitaetswerten in `#73`;
`sdkconfig.defaults` bleibt unveraendert. Reale Stack-/Heapmessung erfolgt an
genau zwei Punkten (Abschnitt 18), keine Grenzwerte ohne Messung.

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
dadurch kompilierbar: `APP_REAL_ACTUATORS_ENABLED != 0` loest bereits beim
Praeprozessorlauf den bestehenden `#error`-Sicherheitsguard in
`app_config.hpp` aus (Compile-Abbruch vor jeder C++-Auswertung), zusaetzlich
bestaetigt `static_assert(!kRealActuatorsEnabledByDefault)` denselben
Sachverhalt fuer den abgeleiteten `constexpr`-Wert (Abschnitt 5).

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
`EspTimerTimeSource`-Instanz und verwendet `monotonicMillis()` sowohl fuer
die Heartbeat-Zeitsteuerung als auch als geloggtes Uptime-Feld (Abschnitt 16).
Damit hat der Adapter einen konkreten Verbraucher im Composition Root, wie im
Auftrag als zulaessig benannt, ohne dass `DevicePlatform`/
`FermentationApplication` heute Zeit brauchen.

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

`device_platform` ist oeffentlich (`REQUIRES`), da `ITimeSource` im
oeffentlichen Header von `EspTimerTimeSource` erscheint. `esp_timer.h` wird
nur im `.cpp` includiert und bleibt privat (`PRIV_REQUIRES`); der oeffentliche
Header bleibt frei von ESP-IDF-Typen (nur `int64_t`).

## 16. Logging

Kein neuer `ILogger`-Port: kein portabler Konsument braucht ihn, und
`docs/ARCHITECTURE.md` sieht keinen vor. KISS-Default: eine kleine lokale
Loggingfunktion direkt in `main/app_main.cpp` (anonymer Namespace).

**Bootzusammenfassung** — eine Funktion, die bei Erfolg **und** Fehlschlag
identisch aufgerufen wird (Abschnitt 10), semantisch aequivalent zu Arduinos
`printBootSummary()`:

```text
ESP_LOGI(TAG, "%s", kProjectName);
ESP_LOGI(TAG, "profile: %s", profileName(policy.profile));
ESP_LOGI(TAG, "hardware state: %s", hardwareStateName(policy.startupHardwareState));
ESP_LOGI(TAG, "actuator policy: %s", actuatorPolicyName(policy.actuatorPolicy));
ESP_LOGI(TAG, "real actuators: disabled");
ESP_LOGI(TAG, "application: ready");   // oder:
ESP_LOGE(TAG, "application: startup failed");
```

**Heartbeat** (alle 1000 ms, zeitgesteuert ueber `monotonicMillis()`, siehe
Abschnitt 12) enthaelt **nur** Status und Uptime, keine Ressourcenwerte:

```text
ESP_LOGI(TAG, "heartbeat: safe test mode, uptime_ms=%" PRIu64, nowMs);
```

**Ressourcenlog** (genau zwei Zeitpunkte, siehe Abschnitt 18):

```text
ESP_LOGI(TAG, "resources: free_heap_bytes=%" PRIu32 " stack_hwm_bytes=%u", ...);
```

`PRIu64`/`<cinttypes>` statt einer ungesicherten `%llu`-Annahme haelt
Formatwarnungen unter dem projektweiten `-Werror` gruen. Kein `ILogger`, kein
Ringbuffer, keine Queue, kein Remote-Logging, keine Fremdbibliothek.

## 17. Resetursache

Entscheidung: **keine Resetursache in `#73`.** Es gibt keinen konkreten
Konsumenten (`SAFE_BOOT`/Fehlerklassen sind fachliche Recovery-Logik aus
Folgeissues, nicht Teil von `#73`); der heutige Arduino-Pfad meldet ebenfalls
keine Resetursache. `esp_reset_reason()` wird nicht eingebunden.

## 18. Ressourcenmessung

Nur lokale Messpunkte, keine Instrumentierung im Fachkern, keine feste
Baseline (die liefert `#74`). Genau **zwei** Messungen, kein
Dauerlogging (eine Messung pro Sekunde waere unnoetig laut, wuerde die
Messung selbst beeinflussen und widersprueche „einmalig“):

1. Direkt nach erfolgreicher Initialisierung (Abschnitt 10, Schritt 5):
   `esp_get_free_heap_size()` und `uxTaskGetStackHighWaterMark(NULL)`
   (liefert Bytes auf der ESP-IDF-FreeRTOS-Portierung, Abschnitt 5) via
   `ESP_LOGI`.
2. Genau ein weiteres Mal nach kontrollierter Laufzeit von rund 30 Sekunden,
   ueber einen kleinen Zaehler oder ein Bool im selben Schleifenkopf wie
   Abschnitt 12 (kein zweiter Scheduler, keine zweite Zeitquelle):

```cpp
bool secondResourceLogDone = false;
// im Schleifenkopf, nach der Heartbeat-Pruefung:
if (!secondResourceLogDone && nowMs - startMs >= 30000U) {
    secondResourceLogDone = true;
    logResources();
}
```

Dazwischen enthaelt der Heartbeat nur Status und Uptime (Abschnitt 16). Keine
Grenzwerte in `#73`; `#74` uebernimmt Baseline und Grenzwerte.

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
    PRIV_INCLUDE_DIRS "../include"
    PRIV_REQUIRES device_platform fermentation_app device_platform_esp_idf
)
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    APP_PROFILE_ESP32_BRINGUP=1 APP_TARGET_FLASH_MB=4 APP_REQUIRE_PSRAM=0
    APP_WEB_OTA_ENABLED=0 APP_REAL_ACTUATORS_ENABLED=0)
```

`app_config.hpp` wird ausschliesslich vom Composition Root selbst benoetigt
(kein anderer Konsument im ESP-IDF-Baum) und ist deshalb
`PRIV_INCLUDE_DIRS`, nicht `INCLUDE_DIRS`: Der `main`-Komponente besitzt
ohnehin keine eigenen Konsumenten, aber die private Sichtbarkeit macht die
Absicht explizit und vermeidet einen unnoetig oeffentlichen Includepfad.
`-std=gnu++17` ist jetzt auch fuer `main` noetig, da die Komponente von
reinem C (Stub) auf C++ (`app_main.cpp`) wechselt.

**PlatformIO-Auswirkung:** `lib/device_platform_esp_idf/` wird von
`src/main.cpp` nicht `#include`t und erscheint daher im Chain-Modus-LDF nicht
im Dependency-Graph von `native`/`esp32_bringup`/`esp32_release` (empirisch
belegtes Verhalten, Abschnitt 6). Als Gate wird `pio run`s
Dependency-Graph-Ausgabe fuer alle drei Envs manuell gegen diese Erwartung
geprueft (Abschnitt 24).

## 20. Architekturguard-Erweiterung

Erweitert `scripts/check_architecture_boundaries.py` (bestehende
Mechanismen aus `#72` wiederverwendet, keine neue Parserklasse):

1. `COMPONENT_REQUIRES_ALLOWLIST` wird von „eine Menge erlaubter Namen“ auf
   „getrennte oeffentliche und private Mengen“ umgestellt, da
   `device_platform_esp_idf` **oeffentlich** `device_platform`, aber **nur
   privat** `esp_timer` haben darf — eine gemeinsame Menge wuerde
   `REQUIRES esp_timer` faelschlich erlauben:

```python
COMPONENT_REQUIRES_ALLOWLIST = {
    "lib/device_platform/CMakeLists.txt": {
        "public": frozenset(), "private": frozenset()},
    "lib/fermentation_app/CMakeLists.txt": {
        "public": frozenset({"device_platform"}), "private": frozenset()},
    "lib/device_platform_esp_idf/CMakeLists.txt": {
        "public": frozenset({"device_platform"}),
        "private": frozenset({"esp_timer"})},
}
```

   Der bestehende Tokenparser aus `#72` liefert bereits `REQUIRES`- und
   `PRIV_REQUIRES`-Fundstellen getrennt (siehe dortige `mode`-Unterscheidung);
   die Auswertungsfunktion wird so angepasst, dass sie beide Mengen gegen
   ihre jeweils eigene Allowlist prueft, statt sie vor der Pruefung zu
   einer Menge zu vereinigen. Kein neuer Parser, nur eine geaenderte
   Vergleichslogik auf denselben bereits getrennt vorliegenden Ergebnissen.
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

Selftest-Erweiterung (tabellengesteuert, dem `#72`-Reviewfix-Muster
folgend), mindestens:

- `device_platform_esp_idf` mit `REQUIRES device_platform` und
  `PRIV_REQUIRES esp_timer`: `PASS`;
- `device_platform_esp_idf` mit `REQUIRES device_platform esp_timer` (also
  `esp_timer` faelschlich oeffentlich): `FAILED`;
- Referenz auf `fermentation_app` aus `device_platform_esp_idf`: `FAILED`;
- eine weitere, nicht erlaubte private IDF-Komponente in
  `PRIV_REQUIRES`: `FAILED`;
- eine dynamische `${...}`-Abhaengigkeit (Muster aus `#72`): `FAILED`.

Keine grosse Permutationsmatrix, kein neuer allgemeiner CMake-Parser.

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

`MINIMAL_BUILD ON` baut nur `main`, Common-Komponenten und die von `main`
tatsaechlich erreichbaren transitiven Abhaengigkeiten (Abschnitt 6). Ein
Zwischenstand, in dem der Adapter existiert, aber `main` noch den alten Stub
verwendet, waere sowohl ein unbenutzter Adapter als auch nicht zuverlaessig
buildbar. Deshalb bevorzugt **zwei** Implementierungscommits:

| # | Commit | Inhalt | Buildbar? |
|---|---|---|---|
| 1 | Vollstaendiger echter Laufzeitpfad | `lib/device_platform_esp_idf/` (`CMakeLists.txt`, `EspTimerTimeSource`), dritter `EXTRA_COMPONENT_DIRS`-Eintrag, `main/app_main.cpp`, `main/CMakeLists.txt` umgestellt, sichere Compile-Definitionen, Entfernung von `main/issue72_build_stub.c` — Adapter und realer Konsument entstehen atomar | Ja — `idf.py build` mit echtem `app_main()`, Adapter tatsaechlich ueber `main -> device_platform_esp_idf` gebaut und verlinkt |
| 2 | Guard, Dokumentation, Nachweise | Architekturguard-Erweiterung inkl. Selftests (Abschnitt 20), `docs/CI_AND_QUALITY_GATES.md`, `CHANGELOG.md`, Abschlussnachweise | Ja |

Kein Zwischencommit mit unbenutztem Adapter, kein Mischcommit mit CI-Aenderung.

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
wird gebaut und tatsaechlich in das ELF verlinkt (referenzierter, nicht nur
kompilierter Adapter); `device_platform`/`fermentation_app` weiterhin
gebaut; `gnu++17` fuer alle vier C++-Komponenten (`compile_commands.json`-
Pruefung wie in `#72`, jetzt inklusive `main`); ESP-IDF `v6.0.2`; kein
Arduino im IDF-Build; kein Test-Support im Komponentenbaum; gueltiges
BIN/ELF/Mapfile; `pio run`-Dependency-Graph fuer alle drei PlatformIO-Envs
enthaelt `device_platform_esp_idf` **nicht** (Abschnitt 19).

Negative Nachweise (manuell, dann zurueckgesetzt, wie im `#72`-Reviewfix
etabliert): IDF-Leak in `device_platform`/`fermentation_app` weiterhin
erkannt; `fermentation_app`-Referenz aus `device_platform_esp_idf` erkannt;
`REQUIRES esp_timer` (statt `PRIV_REQUIRES`) in
`lib/device_platform_esp_idf/CMakeLists.txt` wird als unerlaubte oeffentliche
Abhaengigkeit erkannt; unerlaubte private IDF-Komponente erkannt; dynamische
`${...}`-Abhaengigkeit erkannt; falscher `IDF_VER` bricht weiterhin
fail-fast ab; `APP_REAL_ACTUATORS_ENABLED=1` in `main/CMakeLists.txt`
erzeugt einen Compile-Abbruch durch den bestehenden
`app_config.hpp`-Sicherheitsguard (`#error`-Direktive, siehe Abschnitt 5);
die tatsaechliche Compilerdiagnose wird im Nachweis dokumentiert.

## 25. Hardware-Smoke-Test

Status: `TBD_HARDWARE` — keine ESP32-Hardware in dieser Planungsphase
angeschlossen oder geflasht.

Manuelle Owner-Prozedur nach Freigabe und Flashen:

```bash
idf.py -p <PORT> flash monitor
```

Erwartete serielle Ausgabe und Nachweise:

- Bootzusammenfassung (Projektname, Profil `esp32_bringup`,
  `HARDWARE_UNVERIFIED`, `LockedForBringup`, „real actuators: disabled“,
  „application: ready“);
- erste Ressourcenmessung direkt danach;
- periodische `heartbeat: safe test mode, uptime_ms=...`-Zeilen;
- die erste geloggte Uptime liegt plausibel nahe `0` (seit
  `EspTimerTimeSource`-Instanzerstellung, nicht seit Boot);
- die Uptime faellt ueber die gesamte Laufzeit nie zurueck;
- Heartbeatdifferenzen liegen ungefaehr bei `1000 ms`;
- eine zweite Ressourcenmessung nach rund 30 s;
- mindestens 30 Sekunden durchgehender Betrieb ohne Neustart, ohne
  Watchdog-Reset-Log und ohne jeden GPIO-/Sensor-/Display-/WLAN-/Webzugriff.

**Gate-Status:** Build-, Architektur- und statische Paritaet (Abschnitt 24)
koennen unabhaengig vom Hardware-Smoke-Test vollstaendig abgeschlossen
werden; `idf.py build` allein schliesst das Hardware-Gate **nicht** ab. Die
Laufzeitparitaet im engeren Sinn bleibt bis zum bestandenen manuellen
Smoke-Test offen. Ob der Hardware-Smoke-Test ein zwingendes Merge-Gate ist
oder dokumentiert nachgeholt werden darf, entscheidet der Owner vor dem
Merge; dieser Plan nimmt diese Entscheidung nicht vorweg.

## 26. Security-, Safety-, Recovery- und Persistenzgrenzen

Keine Aenderung an Fachmodellen, Wireformaten, Persistenz- oder
Recoverylogik. `APP_REAL_ACTUATORS_ENABLED != 0` bleibt bereits durch den
bestehenden `#error`-Praeprozessorguard in `app_config.hpp` blockiert, kein
Pfad in `#73` kann reale Aktoren kompilierbar machen (Abschnitt 14). Kein
Netzwerk-, Web- oder Auth-Code. Der einzige neue Fehlerpfad (Startfehler in
`app_main()`) beruehrt keine Persistenz und keine Hardware (Abschnitt 10).

## 27. SOLID, DRY, KISS (gebunden an den geplanten Diff)

**KISS:** ein Main-Task, eine kooperative Schleife mit Ein-Tick-Yield, ein
zeitgesteuerter Heartbeat ueber monotone Zeit, genau zwei gezielte
Ressourcensamples, kein Schedulerframework, kein unnoetiger Port
(`ILogger`, Resetursache, Ressourcenport).

**DRY:** Adapter und realer Konsument entstehen atomar in einem Commit
(Abschnitt 23); keine getrennten Heartbeat-/Update-Tasks; die
`APP_PROFILE_*`-Werte existieren nur an der einen Stelle
`main/CMakeLists.txt`; die CMake-Sichtbarkeitsregeln (`REQUIRES` vs.
`PRIV_REQUIRES`) sind zentral in einer erweiterten Allowlist-Struktur
geprueft statt verstreut; eine einzige Bootzusammenfassungsfunktion deckt
Erfolg und Fehlschlag ab (Abschnitt 10) statt zweier paralleler
Logikpfade. Die einzige bewusste Dopplung (Objektinstanzen in
`main/app_main.cpp` statt Extraktion aus `src/main.cpp`) ist in Abschnitt 8
einzeln begruendet.

**SOLID:**

- SRP: Root verdrahtet, `EspTimerTimeSource` liefert nur Zeit, die lokale
  Diagnosefunktion loggt nur.
- OCP: `device_platform`/`fermentation_app` bleiben unveraendert; die
  IDF-Unterstuetzung kommt additiv ueber eine neue Komponente hinzu.
- LSP: `EspTimerTimeSource` erfuellt exakt den bestehenden
  `ITimeSource`-Vertrag — monotone Zeit startet bei Instanzerstellung, UTC
  bleibt `std::nullopt`, keine Sonderfaelle.
- ISP: kein neuer Sammelport; `ITimeSource` bleibt so schmal wie heute.
- DIP: konkrete Adapter (`EspTimerTimeSource`) zeigen auf bestehende Ports,
  niemals umgekehrt; `fermentation_app` und `device_platform_esp_idf`
  haengen nur von `device_platform`-Ports ab.

## 28. Offene Ownerentscheidungen

Keine. Fuer `#73` ist bereits entschieden: keine Resetursache, kein
Loggingport, keine Zusatz-Task, keine fachliche Updateperiode (Abschnitt 12).
Eine moegliche spaetere Bootdiagnose via `esp_reset_reason()` in einem noch
nicht existierenden Folgeissue ist keine offene Ownerentscheidung dieses
Plans. Offen bleiben ausschliesslich die Nicht-Entscheidungs-Kategorien
`TBD_HARDWARE` und `MEASUREMENT_REQUIRED` (Abschnitt 29).

## 29. `SPIKE_REQUIRED` / `MEASUREMENT_REQUIRED` / `TBD_HARDWARE`

- `TBD_HARDWARE`: Hardware-Smoke-Test (Abschnitt 25).
- `MEASUREMENT_REQUIRED`: reale Stack-Watermark- und Heap-Auslastung an den
  zwei definierten Messpunkten (Abschnitt 18).
- Kein `SPIKE_REQUIRED` in `#73`: alle geplanten APIs sind bereits anhand
  der lokalen Primaerquellen verifiziert (Abschnitt 5).

## 30. Verbotene Vorwegnahmen

Siehe Abschnitt 4 (Nicht-Ziele); zusaetzlich ausdruecklich: keine
`esp_reset_reason()`-Einbindung, kein `ILogger`-Port, keine zweite
Composition-Root-Datei, keine Aenderung an `src/main.cpp`, keine
Kconfig-Overlay-Differenzierung Bring-up/CI/Release, keine
Fremdkomponente/kein Lockfile, keine Aenderung der `update()`-Aufruffrequenz
gegenueber Arduino, kein Dauerlogging von Ressourcenwerten.

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
  Adapter, tatsaechlich ueber `main` referenziert und verlinkt (nicht nur
  ueber `EXTRA_COMPONENT_DIRS` auffindbar).
- `esp_timer_get_time()` korrekt in Millisekunden konvertiert, seit
  Instanzerstellung; `unixTimeSeconds()` bleibt `std::nullopt`.
- Bestehender `DevicePlatform`-/`FermentationApplication`-Kern unveraendert
  wiederverwendet; `HARDWARE_UNVERIFIED`, Aktoren deaktiviert, Profil
  `esp32_bringup`.
- Bootzusammenfassung **immer vollstaendig**, unabhaengig von Erfolg oder
  Fehlschlag; sicherer Startfehler (kein Busy-Loop, keine automatische
  Reboot-Schleife, `app_main()`-Rueckkehr statt Endlosschleife, kein
  Schleifeneintritt nach Fehlschlag).
- `update()` wird bei jedem Schleifendurchlauf aufgerufen (keine
  Frequenzaenderung gegenueber Arduino); Heartbeat separat zeitgesteuert bei
  1000 ms; Ein-Tick-`vTaskDelay`-Yield pro Durchlauf; keine Zusatz-Task.
- Genau zwei Ressourcenmessungen (nach Init, nach ~30 s), kein
  Dauerlogging jede Sekunde.
- CMake-Sichtbarkeit korrekt: `PRIV_INCLUDE_DIRS` fuer `app_config.hpp` in
  `main`; `device_platform_esp_idf` hat `device_platform` oeffentlich und
  `esp_timer` ausschliesslich privat.
- Task-/Stack-/Watchdogannahmen dokumentiert (Abschnitt 13) und ueber die
  zwei Ressourcenmesspunkte belegt.
- Architekturguard unterscheidet oeffentliche/private `REQUIRES` je
  Komponente und erkennt weiterhin alle `#72`-Faelle plus die neuen
  `#73`-Faelle (Abschnitt 20); kein Test-Support im Produktbuild.
- `pio test -e native`, `pio run -e esp32_bringup`, `pio run -e esp32_release`,
  `idf.py build` und die bestehende GitHub-Actions-Pipeline gruen; **keine**
  CI-Aenderung.
- Aktor-Negativtest korrekt als Compile-Abbruch durch den bestehenden
  `app_config.hpp`-`#error`-Sicherheitsguard nachgewiesen und dokumentiert
  (nicht als `static_assert`-Fehlschlag beschrieben).
- Hardware-Smoke-Test bestanden oder weiterhin `TBD_HARDWARE` dokumentiert;
  `idf.py build` schliesst dieses Gate nicht ab; Owner entscheidet vor dem
  Merge ueber dessen Verbindlichkeit.
- Keine Fach-, Persistenz-, Wire-, Safety- oder Recoveryaenderung; keine
  Fremdkomponente.
- Tatsaechlicher Diff gegen `docs/ENGINEERING_PRINCIPLES.md` (SOLID, DRY,
  KISS) geprueft (Abschnitt 27).
