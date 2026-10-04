# R1-RAM-Baseline S2 – Diagnose des Baseline-Blockers

Auftrag: `PR174_S2_Baseline_Blocker_Diagnose` (Owner). Nur Diagnose, kein Fix,
keine Änderung am persistenten Zustand, keine S3–S7-Arbeit, O2 unverändert
freigegeben.

```text
DIAGNOSIS=DONE_READ_ONLY_STATUS
BLOCKER_STATUS=configuration_recovery_status=UnsupportedNewerConfigurationSchema
PERSISTENCE_CHANGED=NO_STATE_STORE_BITIDENTICAL_BEFORE_AFTER
FIX_IMPLEMENTED=NO
```

## 1. Vorgehen

1. `state_store` (0x300000, 1 MiB) vor der Diagnose nur lesend gesichert
   (`esptool read-flash`), SHA-256
   `f6c932be690bb6ef1d1e87e5659bc784d9cc1de5d10fae9fbefe039830cfbbad`.
2. Der vorhandene Issue-90-Slice-7-Harness (`scripts/build_issue90_slice7_harness.py`,
   Bring-up-Profil, Label `state_store_test` auf demselben Flashbereich)
   wurde aus einem sauberen Git-Worktree auf HEAD `1a8d06fe…` gebaut
   (`SOURCE_TREE_CLEAN=YES`; der Builder verlangt einen sauberen Baum, die
   unbeteiligte `.codex/config.toml` im Arbeitsbaum blieb unberührt). Keine
   Änderung am Harness.
3. Geflasht wurden nur Bootloader, Partitionstabelle und App
   (`write-flash @flash_args`); keine Datenpartition, kein Erase.
4. Ausgeführt wurde ausschließlich der read-only `STATUS`-Pfad: Der Harness
   emittiert ihn beim Start (`start()` → `emitStatus()`); zusätzlich wurde
   einmal `STATUS` per UART gesendet. Keiner der schreibenden Befehle
   (`CONFIG_CONTROL_WRITE`, `RUN_CONTROL_WRITE`, `*_DISCARD_PENDING`,
   `ARM_*`, `STOP`, `SET_TRUSTED_UTC`) wurde gesendet.
5. Danach wurde der `esp32_release`-Stand aus dem unveränderten Build
   (`build/esp32_release`, App-BIN
   `dc593a9de2457820bb87c96ab0bab318af3535ba89800a71baf522e7d9a51fc3`,
   meldet `source git sha: 0de006a7…`) ohne Neubau zurückgeflasht und der
   `state_store` erneut gelesen.

## 2. Ergebnis

Rohmitschnitt: [ISSUE_90_SLICE7_STATUS_20261002_1A8D06F_RAW.txt](ISSUE_90_SLICE7_STATUS_20261002_1A8D06F_RAW.txt)
(Harness-Quell-SHA `1a8d06fe…`, Boot und `STATUS` zweimal identisch).

| Statusfeld | Wert |
|---|---|
| `application_started` | `YES` |
| `application_lifecycle` | `ServiceRequired` |
| `configuration_recovery_status` | `UnsupportedNewerConfigurationSchema` |
| `configuration_runtime_available` | `NO` |
| `run_persistence_load_status` | `NOT_AVAILABLE` |
| `run_load_disposition` | `SafeBoot` |
| `published_process_state` | `NONE` |
| `trusted_utc` | `NOT_AVAILABLE` |
| `actuator_release` | `false` |

Weitere Presentation-/Fault-Felder liefert der vorhandene `STATUS`-Pfad nicht;
es wurde keine API ergänzt.

**Befund:** Der Blocker liegt in der **Configuration-Recovery**. Die
persistierte Konfiguration wird als `UnsupportedNewerConfigurationSchema`
klassifiziert; damit ist die Configuration-Runtime nicht verfügbar
(`configuration_runtime_available=NO`), die Anwendung steht fail-closed auf
`ServiceRequired`, und der Run-Persistence-Pfad wird gar nicht erst geladen
(`NOT_AVAILABLE`, `SafeBoot`). Das passt zu den Beobachtungen am Gerät
(Netzwerkmodus bleibt `UNSELECTED`, leere Rezeptseite). Aus dem Status folgt
nicht, wer oder welcher Firmwarestand den Datensatz geschrieben hat; das ist
hier nicht untersucht.

## 3. Unveränderter Persistenzzustand

`state_store` (1 MiB) vor der Diagnose und nach dem Rückflash des
Release-Stands: SHA-256 jeweils
`f6c932be690bb6ef1d1e87e5659bc784d9cc1de5d10fae9fbefe039830cfbbad`,
`cmp` bitgleich. Es wurde kein NVS-/`state_store`-Erase, kein Full-Erase und
kein Testwrite ausgeführt. Nach dem Rückflash meldet das Gerät wieder
`profile: esp32_release`, `application: service required`.

## 4. Entscheidungsgrenze

Es wurde kein Fix implementiert und kein Issue angelegt. Der Owner-/
Reviewentscheid legt fest, ob nur ein gültiger Testzustand hergestellt werden
muss oder ein Produktfehler im bestehenden Issue-#164-Scope vorliegt. Die
S2-Baseline bleibt bis dahin unvollständig; O2 und der
Netzwerkmoduswechsel bleiben Bestandteil der Baseline.
