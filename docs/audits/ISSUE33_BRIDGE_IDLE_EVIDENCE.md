# Issue #33 – BTS7960-Leerlauf-Bruecke: DEV-Flash und Multimeter-Test (2026-11-19)

Scope: Plan `docs/tasks/issue-33-bts7960-idle-bringup-plan.md` (Plan-SHA
`6d3b8c2f976bb80c543ec6968037e79a14fbceff`, Ownerfreigabe D1–D6), PR #217, Code-HEAD
`0fb91d155ca5cb742cd78d3f958347f66e40019b` (unabhaengiger C1-Review PASS, 0 Blocker).
Ownergates: G1 (12-V-Leistungsversorgung vom Owner selbst angeschlossen und freigegeben —
**Ownerangabe, keine von Codex durchgefuehrte elektrische Abnahme**), G2 (DEV-App-only-Flash),
G3 (ein unbelasteter Vorwaerts-/Rueckwaertstest, Owner liest das Multimeter). **Peltier und
alle anderen Verbraucher blieben an den BTS7960-Ausgangsklemmen abgeklemmt.**
`ACTUATOR_RELEASE=NO`, `REAL_PELTIER_TEST=NOT_RUN`.

## Vorpruefung

`test/esp_idf_shared_enable_bridge_host` (nicht Teil von `run_pre_ready_gates.sh`): Build
(`idf.py -C test/esp_idf_shared_enable_bridge_host -B /tmp/build_issue33_bridge_host build`,
Linux-Target) und Lauf des ELF: **13 Tests, 0 Failures, 0 Ignored, `OK`**, Exitcode 0, auf
HEAD `0fb91d1`. Weitere C1-Tests und der bestandene Review wurden nicht wiederholt.

## Image und Flash (G2)

| Feld | Wert |
|---|---|
| Source-HEAD | `0fb91d155ca5cb742cd78d3f958347f66e40019b` (Arbeitsbaum sauber, `SOURCE_TREE_CLEAN=YES`) |
| Build | `scripts/build_issue33_bridge_idle_test.py --clean`, **ohne** `--allow-dirty-source-tree`; Profil `esp32_bringup` (`CONFIG_APP_PROFILE_ESP32_BRINGUP=y`), Testflag `APP_ISSUE_33_BRIDGE_IDLE_TEST=1` in den Compile-Befehlen; das fruehere Dirty-Testimage wurde nicht verwendet |
| Datei | `build/esp32_bringup_issue33/esp32_fermentationsschrank.bin`, 1 796 320 B |
| SHA-256 | `ae4f2ca0807825af6fa50ee332fb25cce01f688a4e275f4029fdd11fe4493ef2` |
| ELF-SHA (Boot-Log) | `325f806c3...` |

Zielgeraet `ESP32-D0WD-V3 (revision v3.1)`; installierte Partitionstabelle byteidentisch zum
Build (`factory app 0x10000/3008K`, `nvs 0x9000`, `state_store 0x300000/1M`). Nur die App bei
`0x10000` geschrieben: `Hash of data verified.`, unabhaengig `verify-flash`: `Verification
successful (digest matched).` Kein Backup, kein Restore, kein `erase_flash`, kein NVS-/State-
Store-/Bootloader-/Partitionstabellen-/OTA-Zugriff.

## Boot

```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
I boot: Loaded app from partition at offset 0x10000
I app_main: onboard outputs: inner_fan=ready_inactive outer_fan=ready_inactive buzzer=ready_inactive reserve=ready_inactive
I app_main: peltier bridge: ready_all_off_not_connected_to_planner
I issue33_bridge_idle: ISSUE33_UART_RX_READY result=PASS
I issue33_bridge_idle: ISSUE33_READY bring_up_only=YES real_actuators_enabled=NO bridge_ready=YES commands="bridge forward|bridge reverse|bridge off" direction_ms=10000 note=no_automatic_on_at_boot_no_load
```

Keine automatische Richtung, kein Panic/WDT/`E`-Zeile. Owner-Ablesung im Ruhezustand nach
dem Boot: Multimeter etwa 0 V (ein 0-V-Wert im unbelasteten Ruhezustand ist wegen des
hochohmigen Ausgangs kein Abschaltnachweis). Das Board lief danach ca. 54 min ohne Reset bis
zum Test.

## G3 – Multimeter im DC-V-Modus zwischen den beiden Ausgangsklemmen (beaufsichtigt)

UART-Log (Port durchgehend offen, ein EN-Reset nur beim Boot):

```
#SEND bridge forward
I issue33_bridge_idle: ISSUE33_BRIDGE direction=forward state=ON_COMMANDED
I issue33_bridge_idle: ISSUE33_BRIDGE direction=forward state=OFF_COMMANDED reason=TIMEOUT      (+9,95 s)
#SEND bridge off
I issue33_bridge_idle: ISSUE33_OFF result=PASS reason=ALREADY_OFF
#SEND bridge reverse
I issue33_bridge_idle: ISSUE33_BRIDGE direction=reverse state=ON_COMMANDED
I issue33_bridge_idle: ISSUE33_BRIDGE direction=reverse state=OFF_COMMANDED reason=TIMEOUT      (+9,95 s)
#SEND bridge off
I issue33_bridge_idle: ISSUE33_OFF result=PASS reason=ALREADY_OFF
```

Das Timeout wurde fuer beide Richtungen selbstaendig von der Software befohlen (Fristablauf
nach ca. 10 s). Kein Reset/Panic/WDT, keine `E`-Zeilen, kein weiteres Kommando, keine
Wiederholung, keine weitere Messreihe, keine GPIO-/Pulldown-Messung.

**Owner-Beobachtung (massgeblich):** `bridge forward`: **≈ +12 V**, danach ≈ 0 V.
`bridge reverse`: **≈ −12 V**, danach ≈ 0 V (Vorzeichen bei unveraenderter Messleitungs-
Polung umgekehrt gegenueber forward). Keine unerwartete Schaltung, keine Spannung nach
`off`/Timeout im Sinne einer Abweichung.

## Bewertung

`HW-33-06=PASS` fuer genau den unbelasteten Leerlauf-Brueckentest: beide Richtungen liefern
unter manueller Software-Aktivierung naeherungsweise ±12 V zwischen den beiden
Ausgangsklemmen, die Software befiehlt nach der 10-s-Frist selbstaendig AUS (Log).

**Grenzen:** `ON_COMMANDED`/`OFF_COMMANDED` sind Befehlslogs, keine Bestaetigung einer
physischen Abschaltung. Ein Multimeterwert nahe 0 V im abgeschalteten, unbelasteten Zustand
ist wegen des hochohmigen/floatenden Ausgangs **kein** belastbarer Nachweis einer sicheren
Abschaltung. ±12 V im Leerlauf ersetzt keinen Belastungs-, Sensor- oder Peltier-Safety-
Nachweis; das 10-s-Timeout ist kein Hardware-Watchdog. `HW-33-01` bis `HW-33-05`,
`REAL_PELTIER_TEST=NOT_RUN` und `ACTUATOR_RELEASE=NO` bleiben unberuehrt.

## Endzustand Board

Das DEV-Board laeuft das **Bring-up-Image `0fb91d1`** (SHA-256 `ae4f2ca0…3ef2`, Profil
`esp32_bringup`), **nicht** die Release-Firmware; Partitionen/NVS/State-Store unveraendert,
Bruecke im Zustand All-off (Software), Peltier und Verbraucher abgeklemmt. Die 12-V-
Versorgung liegt laut Owner weiterhin an (Trennung durch den Owner). Kein Rueckflash.
