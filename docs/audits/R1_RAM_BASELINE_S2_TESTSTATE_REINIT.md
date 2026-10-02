# R1-RAM-Baseline S2 – kontrollierte Neuinitialisierung des Testzustands

Owner-Freigabe: `S2_TESTSTATE_REINITIALIZATION=OWNER_APPROVED`,
`RESTORE_OLD_BACKUP=NO`, `FULL_FLASH_ERASE=NO`, `DEFAULT_NVS_PHY_ERASE=NO`.
Owner-approved Testzustandsmaßnahme für die S2-Baseline, keine
Produktänderung. S3–S7 nicht begonnen.

```text
STATE_STORE_REINIT=DONE
APPLICATION_READY_AFTER_REINIT=YES
UnsupportedNewerConfigurationSchema_AFTER_REINIT=NOT_OBSERVED
TOUCH_CALIBRATION=NotFound_NOT_RESTORABLE_VIA_EXISTING_ISSUE31_PATH
O2_BASELINE_RUN=NOT_EXECUTED_BLOCKED
```

## 1. Ablauf und Belege

1. Rückfallevidenz unverändert: `state_store` SHA-256
   `f6c932be690bb6ef1d1e87e5659bc784d9cc1de5d10fae9fbefe039830cfbbad`
   (Sicherung außerhalb des Repositories, vor dem Eingriff erneut geprüft).
2. Nur die Produktivpartition geleert: `esptool erase-region 0x300000
   0x100000` (`state_store`). Kein Full-Erase, Default-NVS/PHY (0x9000)
   nicht berührt, keine Recordmanipulation.
3. Boot des finalen S2-Release-Stands (`dc593a9d…`, meldet `0de006a…`,
   Code `5bfc9bc`): `application: ready`, kein `service required`. Der leere
   `state_store` wurde regulär vom Produktpfad initialisiert. Rohlog:
   [R1_RAM_BASELINE_S2_REINIT_BOOT_20261002_RAW.txt](R1_RAM_BASELINE_S2_REINIT_BOOT_20261002_RAW.txt).
   Der initialisierte Stand (`state_store` gelesen) hat SHA-256
   `bf0b34557a20824fa1092191695950b560f83c36cf9bbdbd5947dcf40c979c6c`.
   Ein `UnsupportedNewerConfigurationSchema` trat nicht auf (die Prüfung
   erfolgte über `application: ready`; ein separater `STATUS`-Harnesslauf
   wurde nicht gemacht).
4. Touchkalibrierung: `active_status=NotFound fallback_status=NotFound`,
   `touch input held released until calibration is available` (Touch bleibt
   fail-closed).

## 2. Blocker: Issue-31-Provisionierungsweg kann den leeren Stand nicht füllen

Der vorhandene Provisionierer (`scripts/build_issue31_touch_calibration_provision.py`,
`main/issue_31_touch_calibration_provision.cpp`) wurde aus einem sauberen
Worktree auf HEAD gebaut und auf dem Gerät ausgeführt. Er akzeptiert nur zwei
Vorzustände des aktiven Slots: exakt das alte reviewte Modell Sequenz 1
(Migration auf Sequenz 2) oder bereits das komponierte Modell Sequenz 2
(idempotent). Bei leerem Slot meldet er:

```text
ISSUE31_CALIBRATION_ACTIVE_STATUS=NotFound
ISSUE31_CALIBRATION_PROVISION=FAILED reason=ACTIVE_RECORD_MIGRATION_PRECONDITION
```

Rohlog: [R1_RAM_BASELINE_S2_ISSUE31_PROVISION_ATTEMPT_20261002_RAW.txt](R1_RAM_BASELINE_S2_ISSUE31_PROVISION_ATTEMPT_20261002_RAW.txt).
Der Fehlerpfad schreibt nichts: `state_store` nach dem Lauf SHA-256
`bf0b3455…` (identisch zu vorher). Weitere Schreiber für Touch-
Kalibrierungsrecords gibt es im Repository nicht (`TouchCalibrationStore`
wird nur vom Provisionierer geschrieben; der Capture-Harness schreibt
ausdrücklich keine Records und berechnet keine Koeffizienten). Eine
Neukalibrierung bzw. ein Schreibpfad für `NotFound` wäre neue
Kalibrierungslogik und wurde auftragsgemäß nicht implementiert. Eine
Wiederherstellung aus der Sicherung (Record-Bytes) wäre manuelle
Recordmanipulation bzw. Restore des alten Stands und ist ausgeschlossen.

Der finale Release-Stand wurde danach wieder geflasht (Bootloader,
Partitionstabelle, App; App-BIN `dc593a9d…`) und bootet mit
`application: ready`, Touch `NotFound`:
[R1_RAM_BASELINE_S2_RELEASE_AFTER_REINIT_20261002_RAW.txt](R1_RAM_BASELINE_S2_RELEASE_AFTER_REINIT_20261002_RAW.txt).

## 3. Folge für die O2-Baseline

Der O2-Lastpfad verlangt Touchbedienung (Seitenwechsel, Netzwerkmodus-
Press, `network_page_press_*`). Ohne Kalibrierung bleibt Touch
fail-closed; die Baseline wurde daher nicht gestartet. Ob der
Netzwerkpfad nach der Neuinitialisierung ausführbar ist (Moduswahl,
QR/Access Point), ist **nicht geprüft**, weil er Touch voraussetzt.

## 4. Offene Owner-Entscheidung

Wie soll die Touchkalibrierung auf dem leeren Testgerät wiederhergestellt
werden? Der vorhandene Weg deckt nur die Migration Sequenz 1 → 2 ab. Mögliche
Wege (keine umgesetzt): (a) bestehende Provisionierung um einen
Erstbeschreibungs-Vorzustand erweitern (Änderung der Issue-31-
Provisionierlogik, eigener Plan/Review), (b) Rückspielen des alten
Kalibrierungsrecords aus der Sicherung als kontrollierte, dokumentierte
Testzustandsmaßnahme, (c) Neukalibrierung per Capture/Fit-Workflow, soweit
dieser vom Owner freigegeben wird.
