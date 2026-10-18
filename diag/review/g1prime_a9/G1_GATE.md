# PR #203 – G1′ auf a9: konkretes Hardwaregate IMAGE01

**G1_PRIME_GATE_READY_FOR_OWNER_APPROVAL**
Gate-ID: `PR203_RES01_G1PRIME_A9_20261018_IMAGE01`.
**Vorbereitet, nicht freigegeben, nicht ausgeführt.** Dieser Auftrag hat keine
Geräteaktion ausgelöst. Nur eine spätere ausdrückliche Ownerfreigabe dieses
exakten Gate-/Quell-/Image-Stands erlaubt den unten begrenzten Ablauf.

## Bindende Basis und Artefakte

Live-Produkt-PR vor Vorbereitung offen/Draft auf
`2e37b0e3deee3f5770e3f0df3e174074c78711d6`; Laufzeitbasis und aktuell zuletzt
verifiziert installiertes Release aus `a9a1156a1d44d4679957526bbc34a30be6422e4e`.
B0–B4-Dokumentations-Fix-Verification laut neuem Ownerauftrag abgeschlossen;
kein erneuter Browsertest daraus abgeleitet. Kanonischer Plan
[§4 / §7.5](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/2e37b0e3deee3f5770e3f0df3e174074c78711d6/docs/tasks/issue-27-res01-heap-hardening-plan.md),
ursprünglich 6222efce976dc3bea3cf397c2dca56a5fb21859d, späteres R1-Profil
8ce57d78f1544728bf1868d965551d3de7c302f0 und Ownerentscheid 15.10. gelten.
D9 bleibt eine separate Entscheidung; keine Sensor-/Programm-Voraussetzung
für diesen Display-/Touch-Lauf.

| Gegenstand | Exakter Stand / SHA-256 |
|---|---|
| Diagnose-Firmwarequelle (separat, nie mergen) | [203d4d6ad45feb0081110d8e98bec48577da14bc](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/commit/203d4d6ad45feb0081110d8e98bec48577da14bc), Branch `agent/pr203-res01-g1prime-a9-nonmerge` |
| Overlay / Monitor-Herkunft | bb4f43a69740f234d26a951707991f792cc3ffde / 893f3bb73a50b484ce79b3b80722873452d998fc; Firmwaredateien bzw. Hosthelfer bytegleich wiederverwendet |
| App, 1760576 B, esp32_release | `69575dd82e4eda123d90e0b873eeec68ed48a29815e94de341c38c6a2268eaa1` |
| ELF, 36288496 B | `6a336ef8da29a9f11ab880d96187d9683d2ea870bde02463fc235670f49ff16a` |
| Partitionstabelle, 3072 B | `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d` |
| Aufgelöste sdkconfig, identisch zu a9-Build | `9ab3490da37da6bd5a9689eb067c9739276974c0b3cfe20d8c40def2d925c3ea` |
| Rückfall: unverändertes vorhandenes a9-Release, 1756544 B | `2d069d2db793359177379f492559c69fa949cb470351d57f60dd095889619305` |
| Rückfall-ELF | `dca3cc664f3c143797f1caf29ca51bd3b9afe88eb081f914bec595f065523601` |
| UART-Monitor `diag/g1_capture.py` | `955f1c3b1cc12587a012b6ecae0740657cd991152090de801b0458944f9571c5` |
| Probe `main/g1_ui_probe.hpp` | `34a64108e3d19357985c60155f33c303d32f04d8d7bae61f279644e499fa20ee` |

Vollständige Dateigrößen/Runner-/Analyzerhashes:
[Artefaktmanifest](G1_ARTIFACT_MANIFEST.json); [Offline-Verifikation](OFFLINE_VERIFICATION.json),
[Testblatt](G1_TEST_SHEET.md), [Evidence-/Coverage-Matrix](COVERAGE.md), [SHA256SUMS](SHA256SUMS).
Review-Dokumentationscommit ist ausdrücklich **nicht** Firmware-Quellcommit.
App/ELF/Rückfall liegen privat unter
`hw-evidence-pr203/diag/g1prime/prepare_20261018_image01/artifacts/`,
Verzeichnis 0700, Dateien 0600. Rückfall ist kein Ersatz-Neubuild und löst
keinen automatischen Rückflash aus. C5 bleibt separat historisch gesichert;
für dieses Gate ist das aktuelle a9-Image der einzige vorgesehene Rückfall.

## Offline-PASS und unveränderte Produktgrenzen

- 55 bestehende Monitor-/Lasthelfer-/Analyzer-Pythonregressionen PASS; UART/Clock/
  Socketzugriffe dabei nur Fakes. Native Probe mit C++17, -Wall -Wextra -Werror
  PASS: keine C++-Heapallokation, verschachtelte Minima, verzögertes Logging, Overflow.
- Kanonischer sauberer Quellbuild `python3 scripts/build_esp_idf_profiles.py
  release --require-clean-source-tree` PASS, ESP-IDF v6.1 gepinnt auf
  fff9895c82d744c7237be8847347bdd1b07c6643; Profil-/Provenienzguard PASS.
  Ein vorheriger falscher CLI-Profilname `esp32_release` wurde vor dem Build
  abgewiesen und zu `release` korrigiert; kein Quell-/Toolchainfix.
- Offline esptool 5.4.0 image-info PASS (Checksum, Validation hash, ELF-Hash,
  App-Version 203d4d6, DIO/40 MHz/4 MiB). Nur Release benötigt: Bringup,
  unveränderte native Produktregressionen und vollständiges Pre-Ready hier NOT_RUN.
- Keine Änderung an Auth, Single-Flight-Polling, F4/Schwellen, Touchentscheidungen,
  Stackgrößen, Netzwerk-/HTTPD-/TCP-Konfiguration, sdkconfig, Partition oder Safety.
  Keine neue Messinfrastruktur; kein NETPROBE1. Importiertes c0_load ist nur
  unveränderte Monitorabhängigkeit; **kein --load, kein START_LOAD, keine Lastsessions**.

## Zielboard und konkret beabsichtigte Aktionen nach Freigabe

Vorhandenes dauerhaft angeschlossenes ESP32-WROOM-32E / ESP32-D0WD-V3 Rev3.1,
4 MiB, ILI9341 + XPT2046, keine Aktoren/Temperatursensoren. Owner bestätigt
physischen Aufbau im selben zusammenhängenden Gate. UART115200, FT232R:
`/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A5069RR4-if00-port0`.
Zuordnung und erwartete private MAC frisch gegen die gepinnten privaten
16.10.-Preflightreferenzen im Manifest abgleichen. Heute nur historische
Referenz, **kein frischer Board-PASS**. Bei Abweichung HOLD, keine Portsuche
mit Probeboots oder improvisierte Reparatur.

Vor Portöffnung prüfen: keine laufende Aufzeichnung beschädigen, kein
ESP-Browser/anderer bekannter Client, lokale Artefakthashes erneut identisch,
Privatablage und Hostaufzeichnung vorhanden. Genau drei erforderliche
esptool-Sitzungen plus eine Monitoröffnung; je esptool-Sitzung bekannte
Reset-vor/Hard-reset-nach-Effekte (bis zu sechs), plus ein kontrollierter
UART-Portöffnungsboot. Alle tatsächlich beobachteten Effekte protokollieren;
keine weitere Reset- oder Identitäts-Probesitzung und kein Retry.

| Schritt | Exakte Begrenzung | Nachweis/Stop |
|---|---|---|
| PRE | Eine esptool-Read-Sitzung `read-flash 0x0 0x400000`, ausschließlich privat | Chip/MAC aus derselben Sitzung, Partition 0x8000 Länge 0xc00 wie Manifest, App ab 0x10000 exakt 1756544 B mit a9-Hash; sonst HOLD ohne Schreiben |
| FLASH | Genau ein `python3 -m esptool --chip esp32 --port <obiger Port> --baud 460800 --before default-reset --after hard-reset write-flash --flash-mode keep --flash-size keep --flash-freq keep 0x10000 <g1prime_a9_app.bin>` | Nur App; esptool-Hashprüfung muss PASS. Kein idf.py flash, kein @flash_args (diese würden andere Bereiche schreiben), kein erase/erase-all |
| VERIFY | Eine weitere private `read-flash 0x0 0x400000`-Sitzung mit denselben Reset-Flags | App exakt 1760576 B mit Gatehash; Nicht-App-Präfix 0x000000–0x00ffff und state_store 0x300000–0x3fffff bytegleich zu PRE. Partition unverändert. Jegliche Abweichung STOP/HOLD |
| BOOT | Eine Öffnung mit unverändertem gepinntem g1_capture.py; **--main-stack-policy strict**, --run, --allow-port-open-boot, --gate-id PR203_RES01_G1PRIME_A9_20261018_IMAGE01, --expected-source 203d4d6ad45feb0081110d8e98bec48577da14bc; neue private --output-Ablage; kein --load | Erster vollständiger rst:0x1 (POWERON_RESET), zulässiges ROM-Suffix, ≤5 s. Vorherige gepufferte Zeilen nur Rohdaten. Danach vollständiger Live-Bootnachweis ≤30 s und frische Heartbeats; kein weiterer Reset |
| UI | Testblatt: 60 s vergleichbarer Eine-Session-Idle, 18 min reale gezielte Touch-/Locale-Coverage bei einem Browser mit normalem 5-s-Polling, 120 s Erholung | Schritte einzeln anweisen und Ownerantwort mit Rohdaten korrelieren. Keine neue B0–B4- oder R1-U-Abnahme |
| END | Capture via Host-STOP beenden, gesicherte Browserdateien/NetLog schließen, Evidence versiegeln | Kein weiterer Flash, Rollback, Reset, Portöffnen, G2 oder Pre-Ready. Unabhängiger Ergebnisreview |

Für PRE/VERIFY dieselben esptool-Chip-/Port-/Baud-/Resetflags wie FLASH.
Keine Byte-Manipulation an Bootloader, Partitionen, Hersteller-NVS/PHY,
Touchkalibrierung oder state_store. **Einzige geplante fachliche
Persistenzänderung während UI:** normale Sprachwahl über den vorhandenen
User-Konfigurationsowner (es/en/de und Rückkehr zur Ausgangssprache). Sie
verändert state_store regulär nach abgeschlossener Flashverifikation. Das
Gate legt diese Änderung ausdrücklich zur Ownerentscheidung vor; keine
Behauptung, state_store bleibe nach dem ganzen UI-Lauf bytegleich. Wird
diese begrenzte Sprachwahl nicht mitfreigegeben, bleiben Locale-Nachweise
offen; keine temporäre Diagnose-Locale oder Persistenzumgehung einbauen.
Programmeinstellungen/Netzkonfiguration/Namen/Kalibrierung/PIN unverändert.

## Stack-Policy, harte Stops, Grenzen

**G1′ bleibt strict:** Main-HWM <11160 B oder HTTPD-HWM <2080 B führt STOP/HOLD.
Der Ownerentscheid 15.10. akzeptiert den historischen Touch-Stackbefund
10984 B/24576 B als Befund, nimmt die relative Maingrenze aber ausdrücklich
nur im **G2-Web-Teiltest** aus dem Hard-Stop. Die unveränderte Monitoroption
g2-trend ist an dessen exakte alte Gate-ID gebunden und wird hier nicht
verwendet. Der neue Lauf könnte bei erweiterter Navigation erneut an dieser
strengen Reservegrenze stoppen; das Gate versteckt dieses Risiko nicht.
Historischer G1P-2-/IMAGE01-FAIL/HOLD bleibt unverändert.

Sofort stoppen bei erstem heap_alloc_failed, Panic/Abort/WDT/Brownout,
Stackoverflow, unerwartetem Reset/Uptime-Rückgang, fehlender Identität,
Heartbeat-Lücke >5 s, Monitor-/Probe-/Transport-/Timeoutfehler,
unerwartetem HTTP-Fehler, unerklärter Hardware-/Safety-Abweichung. Keine
neue HTTP-Timeoutschwelle aus alten Dauern ableiten; normale Browser-
Single-Flight-Deadline bleibt 30 s, kein Abnahmekriterium. Nur bekannte
/favicon.ico→404 sowie erwartete anonyme/abgelaufene Session-401 sind
ausdrücklich als vorgeschlagene Ausnahmen enthalten. Falsches Passwort
oder sonstige unerwartete Antworten nicht absichtlich erzeugen.

Beim Browserfehler vorhandenen Trace/HAR/volle Fehlerklasse sichern, Tab
schließen, Host-ABORT/STOP; keine Reloads oder zweiten Versuche. Monitor
erkennt einen manuell gesehenen Browserfehler nicht automatisch: Owner/
Builder muss den Abbruch unverzüglich setzen. Vorhandener begrenzter
passiver UART-Nachlauf ≤5 s auf schon offenem Port bleibt nur Evidence,
keine neue Geräteabfrage. Danach keine weitere Hardwareaktion.

Kein tatsächlicher Werksreset, kein ununterbrochenes 5-s-Halten. Nur
geführter kurzer Kontakt ≤1 s mit sicherem Loslassen und Abbrechen; bei
unklarer Touchreaktion nicht weiter Richtung Hold navigieren. Fehlende
Kalibrierung/Touchfunktion wird NOT_PROVEN, nicht repariert. Keine
Sensor-/Commissioning-Simulation, keine Programm-/Aktorfreigabe.

## Messentscheidung und Evidence

[Coverage](COVERAGE.md) trennt historische/native, neu benötigte und am
Standbyboard nicht erreichbare Nachweise. Vor/nach- und zusätzliche
Checkpoint-Minima sind **gemeinsame** interne 8-Bit-Heap-/Blockwerte,
keine exakten transienten Peaks und kein kausal isoliertes UI-b.
16768 B historisch ist keine Formelbasis, **keine Schwelle 33536 B**.
Faktor 2 und Marge 3328 B werden dokumentiert, nicht neu angewendet,
solange b und relevante Pfade nicht hinreichend belegt sind.

F4_SKIP=13312 B / F4_RESUME=16640 B unverändert,
`F4_THRESHOLD=PROVISIONAL_PENDING_G1`. Ohne echte sichere Skip-/Resume-
Hold-A/B-Ereignisse oder mit offenen Spitzen/Varianten **Schwelle nicht
qualifiziert / HOLD**. Kein Gate-Bypass, keine OOM-Erzeugung. Die sichere
Coverage kann den Nachweis erweitern, kann mit diesem Overlay unter den
genannten Grenzen jedoch keine vollständige G1′-Qualifikation versprechen.

Privat: vollständige UART-Bytes/Chunkgrenzen/Hostzeiten, Readbacks, Boot-/
Flashlogs, MAC, HAR inklusive Antwortinhalten, NetLog, etwaige Fotos mit
Netzwerkdetails, unveränderte Originale und Hashmanifeste. Öffentlich:
bereinigte Phasen-/HTTP-/Fehler-/UI-/Heap-/Stackmetadaten, Coverage und
Checksummen; keine Credentials, Cookies, CSRF/Replay-Token oder privaten
Readbackbytes. Bereinigungen/Auslassungen und Uhrenunsicherheit kenntlich.
PASS/FAIL/NOT_PROVEN je Nachweis; keine hypothetischen Belege.

Nach diesem einen Gate unabhängiger Ergebnisreview; keine automatische
Schwellenänderung, kein Rückflash, kein G2 oder Pre-Ready.

`HARDWARE_EXECUTION=NOT_AUTHORIZED` · `G1_QUALIFICATION=NOT_PROVEN` ·
`RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`.
