# PR #179 – Feature-Hardware-Smoke S1/S3/S4 (2026-10-06)

Feature-Smoke von Issue #172 mit Display und Touch (nicht der ESP-IDF-
Toolchain-Smoke ohne Display). Beobachtungen am Display stammen vom Owner
(Fotos, Video, Meldungen im Chat); UART-Werte stammen aus dem lokal
gesicherten Rohlog. Das Rohlog liegt bewusst nicht im Repository.

```text
TESTED_HEAD=2e81d09a82ac7a87ff8e7d9d9d1fefa5edbade77
BUILD_SOURCE_HEAD=2e81d09a82ac7a87ff8e7d9d9d1fefa5edbade77
BUILD_SOURCE_TREE_CLEAN=YES (separater Worktree, --require-clean-source-tree)
BUILD_PROVENANCE_UART=source git sha 2e81d09a82ac7a87ff8e7d9d9d1fefa5edbade77; app version 2e81d09
ESP_IDF=v6.1 fff9895c82d744c7237be8847347bdd1b07c6643
FLASH_PROFILE=esp32_release
FLASHED_IMAGES=0x1000 bootloader, 0x8000 partition table (byte-identisch zum bisherigen Release-Build), 0x10000 app
APP_BIN_SHA256=a6cafa23f0f826b0e1455fc63de27ffd98af1578412e12b662baad2e8532c132
SERIAL_PORT=/dev/ttyUSB0 (usb-FTDI_FT232R_USB_UART_A5069RR4-if00-port0), live ermittelt
FULL_ERASE=NO
NVS_ERASE=NO
STATE_STORE_ERASE=NO
ACTUATOR_RELEASE=NO

BOOT_STABILITY=PASS
S1_PROGRAM_LIST_TOUCH=BLOCKED_TEST_DATA
S2_HARDWARE=NOT_APPLICABLE
S3_LANGUAGE_TOUCH=PASS
S3_LANGUAGE_PERSISTENCE=PASS
S3_HARDWARE_RESOURCE_LOG=OPEN_EVIDENCE_ONLY
S4_HEADER_LOCAL_TIME=FAIL
S4_CLOCK_TOUCH=PASS
S4_CLOCK_SCREEN=PASS

PANIC=0
WATCHDOG=0
BROWNOUT=0
UNEXPECTED_RESET=0
HEAP_ALLOC_FAILED=0

PR179_FEATURE_HARDWARE_SMOKE=FAIL
```

`PR179_FEATURE_HARDWARE_SMOKE=FAIL` ergibt sich aus `S4_HEADER_LOCAL_TIME=FAIL`
(Ursache ausserhalb des S4-Codes, siehe unten) und aus den nicht voll belegten
Punkten S1 und S3-Ressourcenlog. Es gibt keinen Panic, keinen Watchdog, keinen
Brownout, keinen unerwarteten Reset und keinen `heap_alloc_failed`.

## 1. Provenienz und Flash

Release-Firmware mit `python3 scripts/build_esp_idf_profiles.py release
--require-clean-source-tree` in einem sauberen Worktree auf exakt `2e81d09`
gebaut. Nur die drei Images wurden mit esptool geschrieben und verifiziert
(`Hash of data verified`); kein Erase. Die Partitionstabelle hat denselben
SHA-256 wie der vorhandene Release-Build
(`d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d`), die
Datenpartitionen blieben unberuehrt.

## 2. Boot-/Stabilitaet

UART nach dem Flash und nach jedem Neustart: Build-SHA `2e81d09...`,
`application: ready`, ILI9341/LVGL gestartet, `touch calibration:
active_status=Available`, `actuator policy: REQUIRE_VERIFIED_HARDWARE`,
`real actuators: disabled`, Heim-WLAN verbunden. Kein Panic, Watchdog, Brownout
oder `heap_alloc_failed` in allen drei Aufzeichnungen.

Resets im Aufnahmezeitraum (alle durch das Aufnahmewerkzeug ausgeloest, keiner
unerwartet): (a) Reset beim Flash/Start der Aufzeichnung, (b) 17:22:41, weil
das erste Aufzeichnungsskript beim Oeffnen des Ports DTR/RTS gesetzt und den
ESP32 im Reset gehalten hatte (Display war schwarz, die 101 `rst:`-Zeilen in
`uart_02` stammen aus dieser Haltephase vor 17:22:41), (c) 18:28:19 beim
Oeffnen des Ports der zweiten Aufzeichnung (FTDI loest dabei EN aus),
(d) 18:33:08 absichtlicher EN-Reset fuer den Persistenztest.

Luecke: Zwischen 17:47:40 und 18:28:19 lief keine Aufzeichnung (Aufnahme war
auf 25 Minuten begrenzt). In dieser Zeit entstanden das S1-Video und Teile der
S3-/S4-Fotos. Ein Reset in dieser Zeit ist indirekt ausgeschlossen, weil die
im Header angezeigte Uhr ohne Sprung mit der Uptime weiterlief (01:59 → 02:02).

## 3. S1 – Programmliste (Video, Owner-Beobachtung)

Beobachtet: Home → `Start` zeigt die Drei-Zeilen-Liste (`Joghurt mild`,
`Joghurt stichfest`, `Milchkefir`, Pager `1/4`, Slots Home/Auf/Ab/Manuell).
Ein Zeilentap oeffnet den Folgescreen `Start` mit dem Grund `Programm
ungueltig` (Slots Zurueck/Bearbeite/Bestaetig/Status). Home → `Programme`
zeigt dieselbe Liste; ein Zeilentap oeffnet `Rezeptaktionen`
(Zurueck/Bearbeite/Kopieren/Neu), weiterer Wechsel nach `Rezept bearbeiten` und
zurueck. Ein Tap auf `Bestaetig` startete nichts (fail-closed).

Warum `BLOCKED_TEST_DATA`: Der Katalog enthaelt nur die Werksprogramme, deren
Zieltemperatur und Dauer als `TBD_COMMISSIONING` nicht Runnable-valide sind
(`docs/STANDARD_PROGRAMS.md`); `Programm ungueltig` ist der erwartete Grund.
Keiner der Folgescreens zeigt den Programmnamen, und der Finger verdeckt im
Video die angetippte Zeile. Dass **genau der angetippte Eintrag** gewaehlt
wurde, ist auf dem Geraet daher nicht belegt. Der Pager wurde nicht verschoben.
Kein Reset/Erase wurde ausgefuehrt, kein Lauf gestartet.

## 4. S3 – Sprache

Ausgangssprache Deutsch. Owner: Wechsel auf Englisch, Spanisch und zurueck auf
Deutsch; Header-Sprachcode und Seitentexte/Fusszeile wechselten jeweils sofort,
kein Whitescreen, Reset oder Panic, Navigation blieb bedienbar. Fotos belegen
Sprachseite und Header in DE, EN und ES. UART: je Sprachpress
`touch press dispatch: outcome=2`.

Persistenz: Sprache auf Spanisch gesetzt (Dispatch 18:31:09), EN-Pulse-Reset
18:33:08. Nach dem Neustart zeigte das Geraet weiter Spanisch (Owner:
„Immer noch spanisch“); zwischen Reset und Rueckstellung gab es in der UART
keinen Touch-Dispatch, die Sprache stammt also aus dem gespeicherten Zustand.
Rueckstellung auf Deutsch um 18:34:48 (Dispatch), Owner bestaetigt Deutsch.

Ressourcen-Nachweis vor/nach dem Sprach-Commit: `S3_HARDWARE_RESOURCE_LOG=
OPEN_EVIDENCE_ONLY`. Der Stand `2e81d09` hat keinen commitnahen
`logResources`-Messpunkt fuer den Sprach-Commit; keine Produktionsaenderung,
kein PASS erfunden. Ueber den fehlenden D10-Logpunkt wird separat entschieden.
Die periodischen Werte (Abschnitt 6) sind nur Zusatzinformation.

## 5. S4 – Header-Uhr

- **Touch:** Tap auf die Header-Uhr oeffnet den `HeaderClock`-Screen; Netzwerk-
  Symbol und Sprachcode bleiben separat bedienbar; mehrfacher Wechsel Uhr →
  Sprache → WLAN ohne Auffaelligkeit (Owner, Fotos). `S4_CLOCK_TOUCH=PASS`.
- **Screen:** Titel `Uhrzeit`, Vertrauensstatus `Zeit vertrauenswuerdig`,
  `Europe/Zurich` und dieselbe Zeit wie im Header (Foto: 01:59 in beiden).
  `S4_CLOCK_SCREEN=PASS`.
- **Lokalzeit: FAIL.** Die angezeigte Zeit ist falsch. Der Header zeigte
  01:02 bei 17:24 Uhr, 01:05 bei 17:27, nach dem Neustart um 18:33 wieder 01:01
  eine Minute nach dem Reset. Das ist die Uptime auf 1970-01-01 plus
  Zuericher Offset: Die Systemuhr bleibt nach dem Boot bei ca. Epoche 0, wird
  aber als vertrauenswuerdig publiziert. Die #178-Umrechnung selbst ist dabei
  korrekt (Epoche 120 s ist in Zuerich 01:02). Es gibt keine RTC am Geraet
  (Owner); Zeitquelle ist allein SNTP.

### Ursachenanalyse (Quelltext, nicht auf dem Geraet bewiesen)

`EspIdfSntpTimeCoordinator` nutzt `smooth_sync = true`. In ESP-IDF 6.1
(`components/lwip/apps/sntp/sntp.c`, `components/esp_libc/src/time.c`) laeuft
`sntp_sync_time` im Smooth-Modus ueber `adjtime()`, das
`tx.offset = delta->tv_sec * 1000000L` mit 32-Bit-`long` berechnet. Bei einem
Zeitsprung von rund 1,79 Mrd. Sekunden (Uhr bei 1970) laeuft das ueber. Die
Uhr wird so nicht auf die echte Zeit gestellt, der Status wird aber
`COMPLETED`, und der #126-Pfad (`promoteSntpSystemTrust`) markiert die Zeit als
trusted. Es ist ein vorhandener Defekt im absoluten Zeitpfad aus #126, nicht
im S4-Code. Er widerspricht der Fail-closed-Absicht von #126 und S4
(scheinbar gueltige, aber falsche Zeit) und braucht eine eigene Entscheidung;
in diesem Smoke wurde nichts geaendert.

## 6. Ressourcen und Beobachtung

Quelle: `uart_02` (Sitzung 1) und `uart_03` (Sitzung 2). Einheit Byte.

| Zeit | Punkt | free | min free | largest 8-bit | main stack HWM |
|---|---|---:|---:|---:|---:|
| 17:22:43 | stable_home_wifi | 20124 | 16232 | 19456 | 12744 |
| 17:23:13 | periodic_30s | 19996 | 14520 | 19456 | 12744 |
| 17:24:43 | idle_120s | 18868 | 4508 | 17408 | 12744 |
| 17:28:43 / 17:28:58 | network_page_press vor/nach | 19936 / 19940 | 4508 | 17408 | 12744 |
| 17:31:09 | idle_120s | 18868 | 4508 | 11264 | 5880 |
| 18:28:24 | stable_home_wifi | 19884 | 16556 | 7936 | 12744 |
| 18:28:52 | periodic_30s | 19912 | 14776 | 7936 | 12744 |
| 18:30:22 | idle_120s | 20080 | 10712 | 7936 | 12744 |
| 18:33:12 | stable_home_wifi (nach Reset) | 20096 | 16524 | 11776 | 12744 |
| 18:33:40 | periodic_30s | 20328 | 16524 | 11776 | 12744 |

LVGL-Pool: `total 46724–46972`, `free 33632–36256`, `largest free 33264–33864`,
`max used 13940–14060`, `used 23–29 %`, `frag 2–7 %`.

Beobachtungen ohne FAIL-Wirkung: Das minimale freie Heap fiel in Sitzung 1
bis `idle_120s` auf 4508 Byte, in Sitzung 2 auf 10712 Byte (jeweils vor dem
Sprach-Commit, also nicht dem Sprachwechsel zuordenbar, aber mit Touch im
Zeitraum nicht ausgeschlossen); der Main-Stack-HWM sank in Sitzung 1 nach den
Interaktionen auf 5880 Byte. `heap_alloc_failed` trat nie auf. Diese Werte sind
Ausgangspunkt fuer die separate Bewertung, keine neue Grenzwertaussage.

## 7. Lokale Artefakte (nicht im Repository)

Lokaler Ordner `pr179-hw-smoke-20261006` auf dem Testrechner (SHA-256):

```text
43b7b78dc9b9448b81aaa28cae72d8ab53d2e49edd9587112ea68b9f584feb46  uart_01_boot_after_flash.raw.txt
95c815947e6bee3ef2a424bfccb8878b8aabd1512a20e8dc5f0c69d9b21ccab8  uart_02_session1_1722-1747.raw.txt
1340f068c0fc5c74c41fde06522ae16f707f9b305f7dc2ad2c02e7bdcdece9b9  uart_03_session2_1828-1836.raw.txt
c14dc9ab1748ecbd055aaa43c9b59e550015f4d28835e074e2e73b54b0ec2b1e  flash.log.txt
281683f8e51699e2851dc2d5fedbcac5f2bc3234988c2f9fc59018fb931fc60b  build.log.txt
```

Fotos und Video des Owners liegen in dessen Chat-Uploads. Es wurden keine
Secrets, WLAN-Zugangsdaten oder Passwoerter erfasst oder committet.
