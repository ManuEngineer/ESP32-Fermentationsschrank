# R1-RAM-Baseline S2 – kontrollierte Neuinitialisierung des Testzustands

Owner-Freigabe: `S2_TESTSTATE_REINITIALIZATION=OWNER_APPROVED`,
`RESTORE_OLD_BACKUP=NO`, `FULL_FLASH_ERASE=NO`, `DEFAULT_NVS_PHY_ERASE=NO`.
Owner-approved Testzustandsmaßnahme für die S2-Baseline, keine
Produktänderung. S3–S7 nicht begonnen.

```text
STATE_STORE_REINIT=DONE
APPLICATION_READY_AFTER_REINIT=YES
UnsupportedNewerConfigurationSchema_AFTER_REINIT=NOT_OBSERVED
TOUCH_CALIBRATION=Available_AFTER_FIRST_PROVISION
FIRST_PROVISION_FROM_EMPTY=PASS
O2_BASELINE_RUN=PENDING_OWNER_AT_DEVICE
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

## 3. Erstprovisionierung (Owner-Freigabe `PR174_S2_Touch_FirstProvision`)

Der Provisionierer wurde minimal um genau einen Fall erweitert
(`main/issue_31_touch_calibration_provision.cpp`, Commit `2c71c4f5…`):
Sind `tc0` (Active) und `tc1` (Fallback) exakt `NotFound`, wird das bereits
reviewte `kComposedModel` direkt als Sequenz 2 geschrieben, dann exakter
Readback und unverändertes, leeres `tc1` geprüft. Migration (Sequenz 1 → 2),
Idempotenz (Sequenz 2) und alle übrigen vorhandenen Records bleiben
unverändert bzw. fail-closed. Keine Änderung an Store, Codec, Schema,
Koeffizienten, Thresholds, Rotation, Renderer oder Layout.

Build aus sauberem Worktree auf `2c71c4f5…` (`SOURCE_TREE_CLEAN`), Profil
`esp32_bringup`, App-BIN `a1868b7b97ccd5959b50e9482c15c0a6a30f2fbdde9c839951308aec95e9bf6f`,
nur Bootloader, Partitionstabelle und App geflasht. Auf dem
neuinitialisierten Gerät:

```text
ISSUE31_CALIBRATION_ACTIVE_STATUS=NotFound
ISSUE31_CALIBRATION_FALLBACK_STATUS=NotFound
ISSUE31_CALIBRATION_ACTIVE_PRESTATE=FIRST_PROVISION_FROM_EMPTY
ISSUE31_CALIBRATION_WRITE=COMMITTED
ISSUE31_CALIBRATION_FALLBACK_AFTER=NotFound
ISSUE31_CALIBRATION_READBACK=PASS
ISSUE31_CALIBRATION_PROVISION=COMPLETE ACTUATOR_RELEASE=NO FALLBACK_SLOT=UNCHANGED
```

Rohlog: [R1_RAM_BASELINE_S2_ISSUE31_FIRST_PROVISION_20261002_RAW.txt](R1_RAM_BASELINE_S2_ISSUE31_FIRST_PROVISION_20261002_RAW.txt).
Danach wurde der finale S2-Release-Stand (`dc593a9d…`, meldet `0de006a…`,
Code `5bfc9bc`) wieder geflasht; Boot: `application: ready`,
`touch calibration: active_status=Available fallback_status=NotFound`, kein
`UnsupportedNewerConfigurationSchema` im Log
([R1_RAM_BASELINE_S2_RELEASE_AFTER_PROVISION_20261002_RAW.txt](R1_RAM_BASELINE_S2_RELEASE_AFTER_PROVISION_20261002_RAW.txt)).
Der frühere Befund aus Abschnitt 2 (`ACTIVE_RECORD_MIGRATION_PRECONDITION` bei
`NotFound`) ist damit behoben und nur noch historisch.

Owner-Freigabe: `S2_TOUCH_FIRST_PROVISION_FROM_EMPTY=OWNER_APPROVED`
(`TC0_NOTFOUND_AND_TC1_NOTFOUND` -> `kComposedModel`, Sequenz 2; Commit
`2c71c4f5d2a0eb320c9fb75f1805af1ed6e950aa`; `NEW_CALIBRATION_CAPTURE=NO`,
`ACTUATOR_RELEASE=NO`). Die ausgeführte Hardware-Provisionierung und ihre
Evidenz gelten damit als freigegebene Testzustandsmaßnahme für die
S2-Baseline.

## 4. Offen

Product-Touch-Smoke zur Bestätigung der bekannten Zuordnung und der
O2-Baselinelauf benötigen den Owner am Gerät; sie sind noch nicht ausgeführt.
Ob der Netzwerkpfad (Moduswahl, QR/Access Point) ausführbar ist, ist noch
nicht geprüft.
