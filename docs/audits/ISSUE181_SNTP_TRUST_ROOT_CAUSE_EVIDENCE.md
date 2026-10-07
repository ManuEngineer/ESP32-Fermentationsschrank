# Issue #181 – Root-Cause-Evidence (C0: N1, N2, N3)

```text
ISSUE=181
PLAN=docs/tasks/issue-181-sntp-trust-ntp-only-cold-boot-plan.md
OWNER_APPROVED_PLAN_SHA=e24abd207a54f7869475f68b9872a9b6f75de758
N1_TARGET_TYPES=PASS
N1_BUILD_CONFIG=PASS
N2_UPSTREAM_COMPARISON=PASS
N3_HARDWARE_PROBE=PASS
H1_TRUNCATION=PROVEN
H2_BOOT_TIME_ZERO_SLEW_DISCARD=PROVEN
ROOT_CAUSE=PROVEN
ACTUATOR_RELEASE=NO
```

## N1 – Target-Typen und Build-Konfiguration

- Compiler `xtensa-esp32-elf-gcc` (crosstool-NG esp-15.2.0_20251204) 15.2.0,
  Target esp32: `__SIZEOF_LONG__=4`; `_Static_assert(sizeof(long)==4)` und
  `_Static_assert(sizeof(time_t)==8)` kompilieren.
- Erzeugte Profil-`sdkconfig` (`build/esp32_bringup`, `build/esp32_release`,
  gebaut mit `scripts/build_esp_idf_profiles.py all`, PASS): in beiden
  `CONFIG_ESP_TIME_FUNCS_USE_RTC_TIMER=y`, `CONFIG_ESP_TIME_FUNCS_USE_ESP_TIMER=y`,
  `CONFIG_LIBC_TIME_SYSCALL_USE_RTC_HRT=y`, `CONFIG_LWIP_SNTP_UPDATE_DELAY=3600000`.
  Keine neue Kconfig-Einstellung.

## N2 – Gepinnter ESP-IDF-Stand gegen Upstream

- Pin: `v6.1` @ `fff9895c82d744c7237be8847347bdd1b07c6643`.
- H1: `components/esp_libc/src/time.c:193` rechnet
  `tx.offset = delta->tv_sec * 1000000L + delta->tv_usec;` in `long`
  (32 Bit). Upstream-Fix `bdf98cf0ddadc02fbd413be4992353e5a1d60070`
  (2026-09-09, „reject out-of-range adjtime() deltas instead of overflowing",
  esp-idf #19051) liegt auf `master`, nicht im Pin und nicht auf `release/v6.1`.
- H2: `components/esp_libc/src/timekeeping.c:33`
  (`if ((boot_time == 0) || ...) s_adjtime_start_us = 0;`) ist auf `master`
  unveraendert.
- Kein Pin-Bump in diesem PR (Plan A3).

## N3 – Hardware-Probe (Baseline-Modus SMOOTH, nur Diagnose)

Probe: Patch `docs/audits/ISSUE181_N3_PROBE.patch` gegen `b935be9`, gebaut
ausschliesslich in einem Scratch-Worktree, **nicht** Teil des Fix-Codes.
Sie ersetzt `sntp_sync_time` durch eine logging-Kopie des ESP-IDF-Smooth-Pfads
und erzwingt `SMOOTH` (altes #126-Verhalten). Profil `esp32_release`
(das Bring-up-Profil bricht unabhaengig von #181 am `issue29_probe` vor der
Hauptschleife ab). Flash ohne Erase (Bootloader, Partitionstabelle, App),
NVS unberuehrt.

Cold-Boot-Beweis: zwei echte Spannungsunterbrechungen durch den Owner
(`POWER_CYCLE=YES`). `ESP_RST_POWERON` allein belegt den Cold Boot nicht
(auch ein EN-Reset meldet `rst:0x1`); massgeblich sind die physische
Unterbrechung und `raw_time=0`. Ein EN-/DTR-Lauf nach dem Flashen und der
Reset durch den Agenten sind **nicht** als Beweis gewertet.

Auszug `n3_uart_pc2.log` (Zeitstempel = Uptime in ms):

```text
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
W (796) N3_PROBE: BOOT reset_reason=1 (ESP_RST_POWERON=1) raw_time=0
W (32386) N3_PROBE: sync_cb mode=1 time_before=31 server=1791312481 delta_us=1791312449509143
W (32386) N3_PROBE: SMOOTH adjtime ret=0 errno=0 tv_sec_arg=1791312449 readback ret=0 remaining=0.000000
W (32396) N3_PROBE: time_after=31
W (32406) N3_PROBE: poll status=1 mode=1 time=31 trusted=0
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
W (796) N3_PROBE: BOOT reset_reason=1 (ESP_RST_POWERON=1) raw_time=0
W (5856) N3_PROBE: sync_cb mode=1 time_before=5 server=1791312613 delta_us=1791312608655515
W (5856) N3_PROBE: SMOOTH adjtime ret=0 errno=0 tv_sec_arg=1791312608 readback ret=0 remaining=0.000000
W (5866) N3_PROBE: time_after=5
W (5876) N3_PROBE: poll status=1 mode=1 time=5 trusted=0
```

Auswertung je Power-Cycle (beide identisch):

| Groesse | Cut 1 | Cut 2 |
|---|---|---|
| Reset | `ESP_RST_POWERON` nach Spannungsunterbrechung | wie Cut 1 |
| `time(nullptr)` vor Sync | `raw_time=0` | `raw_time=0` |
| Sync-Modus | `SMOOTH` (`mode=1`) | `SMOOTH` |
| Delta | 1 791 312 449 509 143 us | 1 791 312 608 655 515 us |
| `adjtime()` | `ret=0 errno=0` (Argument-`tv_sec` 1 791 312 449) | `ret=0 errno=0` (1 791 312 608) |
| Readback direkt danach | Restkorrektur `0.000000` | `0.000000` |
| `time(nullptr)` nach Sync | 31 (Uptime, nicht UTC) | 5 (Uptime) |
| Status | `COMPLETED` sofort | `COMPLETED` sofort |

## Bewertung

- **H1 (Truncation) bewiesen:** Mit korrektem 64-Bit-Vergleich waere
  `|delta|` ueber der Grenze von 2 146 000 000 us und `adjtime()` muesste `-1`
  liefern (Fallback `settimeofday`). Es liefert `0`: der 32-Bit-gewrappte Wert
  passiert die Pruefung. Gestuetzt durch N1 (`long` = 4 Byte).
- **H2 (verworfener Slew auf nie gesetzter Uhr) bewiesen:** Das Readback
  direkt nach angenommenem `adjtime()` ist `0`, die Uhr bewegt sich nicht,
  und `sntp_get_sync_status()` meldet sofort `COMPLETED`. `RTC_BOOT_TIME`
  war 0 (`raw_time=0` nach echtem Power-on; N1: RTC-Timekeeping aktiv).
  H2 reicht fuer den beobachteten Fehler allein aus und ist deltaunabhaengig.
- Damit ist die Annahme des #126-Plans
  (`ESP_IDF_DOCUMENTED_LARGE_DELTA_IMMEDIATE_STEP`) widerlegt.
- Die Probe belegt zusaetzlich das Verhalten des neuen Gates: Trotz
  `COMPLETED` blieb `trusted=0` (Promote nur unter `IMMED`), der Header
  zeigte keine Zeit (fail-closed).

## Nicht belegt / offen

- Das Verhalten der Fix-Firmware (`IMMED` im untrusted Zustand, Promote,
  anschliessender Wechsel auf `SMOOTH`, echte Lokalzeit im Header) ist
  Gegenstand des spaeteren Hardware-Retests nach dem Independent Review
  (zwei echte Power-Cycles) und hier nicht belegt.
- RTC-seeded Pfad bei Abweichung ueber ca. 35,8 min: bekanntes Restrisiko
  (O1=a), nicht behoben.
