# Plan: Issue #90 Slice-7 Host-UTC-Seam (KISS, finale Fassung)

Diese Fassung ersetzt zwei vorangegangene Planrevisionen (native Parser-Tests,
ESP-IDF-Linux-Host-Testprojekt mit `settimeofday`-Wrapper,
`processCommand()`-Refactor, Detail-Preflight mit
`technical_reason`/`durability`-Assertions) vollständig. Der Owner hat diese
schwerere Fassung explizit als Scope-Creep zurückgewiesen: Ziel ist die reale
NVS-/Power-Cut-Kette, nicht der Bau einer neuen Testplattform. Diese Fassung
folgt exakt dem Owner-Dokument `KISS_KORREKTURAUFTRAG_ISSUE90_HOST_UTC_SEAM.md`.

## Context

Phase-A-Audit (NO HARDWARE) auf `integration/r1-development`
(`18fb96b79608914568b98d2ec06694d75ed0402e`) ergab
`SLICE7_CURRENT_BASELINE_COMPATIBILITY=CHANGES_REQUIRED`: Der Slice-7-
Harness (`main/issue_90_slice7_harness.*`, aus PR #123) stammt aus der Zeit
vor #121/#124/#126.

**Verifiziert (`run_persistence_coordinator.cpp:2224-2235`):** Jeder frische
produktive Run-Start (`StartManualHolding`) wird ohne `time.utcUnixSeconds`
mit `RunPersistenceResultStatus::Blocked` abgewiesen
(`TrustedAbsoluteTimeRequired`). Der Harness übergibt aktuell explizit
`RunCheckpointTime{timestamp, std::nullopt}`
(`main/issue_90_slice7_harness.cpp:489,522,537`) und besitzt eine eigene,
von der Applikation getrennte `EspTimerTimeSource timeSource_`
(`main/issue_90_slice7_harness.hpp:45`). Trusted UTC ist auf diesem
Harness-Build nie erreichbar (`kRtcConfig.present=false` mangels
#29-Board-Profil, keine WiFi/#89-Connectivity in `app_main.cpp`). Jeder
Run-Start schlägt daher heute unconditional fehl — der `run`-Power-Cut-Test
kann keinen persistierten Run erzeugen.

**Verifiziert (`scripts/issue_90_slice7_product_runner.py:763-770`):**
`KNOWN_DISPOSITIONS` enthält `RunLoadDisposition::RecoveryEvaluation`
(seit #121 real erreichbar, `boot_classification.hpp:23`) nicht.

**Verifiziert und vom Owner bestätigt:** `decideAutomatic()`
(`process_state_machine.cpp:555-579`) hat keinen Aufrufer im Repository —
der Regelkreis aus #25/#106 ist nicht komponiert. Ein Run kann daher real
nur bis `ReachingTarget` kommen; `ReachingTarget` ist nicht
resume-eligible (`isR1ResumeEligible`) und wird beim Reboot korrekt als
`NoActiveRun`→`Standby` klassifiziert. Owner-Entscheidung: keine
synthetische `decideAutomatic()`-Seam, kein künstlicher
Fermenting-Übergang. `FERMENTING`/`WaitingForTrustedTime` bleiben
dokumentiertes Restgate für den späteren Regelkreis, nicht Teil von #90.

Die Produktionslogik (`lib/fermentation_app/**`, #124/#126) ist bereits
digital vollständig umgesetzt und getestet; das #126-Startgate wird hier
**nicht** erneut hardwareseitig im Detail bewiesen — Ziel ist ausschliesslich
die reale NVS-Power-Cut-Kette (was passiert mit einem echten
NVS-Schreibvorgang, wenn währenddessen der Strom weg ist).

## Ziel

Den bestehenden Slice-7-Harness minimal an den seit #126 geltenden
Trusted-UTC-Vertrag anpassen, damit der reale `run`-Power-Cut-Test wieder
einen persistierten Run erzeugen kann — ohne neue Testplattform.

## Nicht-Ziele

```text
PRODUCT_CODE_CHANGE=NO            (lib/fermentation_app/**, device_platform*/** unveraendert)
ISSUE124_CONTRACT_CHANGE=NO
ISSUE126_CONTRACT_CHANGE=NO
SYNTHETIC_DECIDE_AUTOMATIC_SEAM=NO
NEW_TEST_PLATFORM=NO              (kein natives Parser-Testverzeichnis,
                                    kein ESP-IDF-Linux-Host-Testprojekt,
                                    kein settimeofday-Wrapper)
REAL_POWER_CUTS=NOT_RUN_UNTIL_OWNER_IMPLEMENTATION_REVIEW
```

## Betroffene Dateien

```text
main/issue_90_slice7_harness.hpp
main/issue_90_slice7_harness.cpp
main/app_main.cpp
scripts/issue_90_slice7_product_runner.py
```

Nicht ändern: `lib/fermentation_app/**`, `lib/device_platform/**`,
`lib/device_platform_esp_idf/**`, `test/test_issue90_product_recovery_oracle/**`,
`scripts/check_issue90_slice7_isolation.py` (bestehender Symbol-Scan auf
`issue_90_slice7::Harness` deckt den neuen, vollständig innerhalb der
Klasse liegenden Befehl bereits ab).

## Commit 1: Gemeinsame Zeitquelle + Host-UTC-Befehl im Harness

**`main/issue_90_slice7_harness.hpp` / `main/app_main.cpp`**

- Harness-eigene `EspTimerTimeSource timeSource_` entfernen
  (`SECOND_HARNESS_TIME_SOURCE=REMOVE`). Konstruktor:
  `Harness(FermentationApplication&, const device_platform_esp_idf::EspTimerTimeSource&)`,
  Member als Referenz.
- `app_main.cpp` übergibt an der bestehenden Konstruktionsstelle
  (`#if defined(APP_ISSUE_90_SLICE7_HARNESS)`, Zeile 219) die bereits
  vorhandene lokale `timeSource`-Instanz (Zeile 167). Keine Änderung am
  Nicht-Harness-Pfad, keine Änderung an `fermentation_app`/`device_platform`.

**`main/issue_90_slice7_harness.cpp`**

- Neuer UART-Befehl `SET_TRUSTED_UTC <unix_seconds>` in `processLine()`,
  Parser direkt im Harness (kein eigenes Parser-Modul, kein `lib/`-Modul):
  - gültige positive `int64`-Unixzeit → `EspTimerTimeSource::setSystemTimeUtc(value)`
    (statisch); nur bei Erfolg `timeSource_.markAbsoluteTimeTrusted()` und
    `ISSUE90_TRUSTED_UTC_SET result=PASS unix_seconds=<value>` loggen.
  - ungültige Eingabe oder `setSystemTimeUtc`-Fehlschlag →
    `ISSUE90_TRUSTED_UTC_SET result=FAIL`, keine Trust-Markierung.
- `emitStatus()`: neues Feld `trusted_utc=<unix_seconds>|NOT_AVAILABLE`,
  aus `application_.currentCheckpointTime().utcUnixSeconds` (privat, aber
  via bestehendem `friend class issue_90_slice7::Harness;`,
  `fermentation_application.hpp:78`, zugänglich).
- `writeRun()`, `stopActiveRun()`, `discardPendingRun()`: keine
  `RunCheckpointTime{..., std::nullopt}` mehr erzeugen. Stattdessen
  `const auto checkpointTime = application_.currentCheckpointTime();`
  verwenden. In `writeRun()`/`stopActiveRun()` zusätzlich
  `checkpointTime.monotonicMillis` für `CommandEnvelope.monotonicMillis`
  verwenden und **dasselbe** `checkpointTime` an `persistCommand()`
  übergeben — sonst lehnt `persistCommand()` mit `TimeMismatch` ab, da
  `time.monotonicMillis` exakt dem Envelope-Wert entsprechen muss
  (`run_persistence_coordinator.cpp:2215-2216`).
- `writeConfiguration()` bleibt unverändert (keine UTC-Abhängigkeit).

## Commit 2: Runner-Anpassung

**`scripts/issue_90_slice7_product_runner.py`**

- Nur für `scenario == "run"`, vor jedem `ARM_RUN_WRITE_LOAD` (Trust ist
  boot-lokal und übersteht keinen Reboot, daher vor **jedem** der drei
  realen Versuche erneut):
  1. `STATUS` → `trusted_utc == "NOT_AVAILABLE"` erwarten.
  2. `SET_TRUSTED_UTC <int(time.time())>` senden, `ISSUE90_TRUSTED_UTC_SET
     result=PASS` erzwingen.
  3. `STATUS` → `trusted_utc` numerisch vorhanden erwarten.
  4. `ARM_RUN_WRITE_LOAD` senden (bestehender Ablauf ab hier unverändert).
  `scenario == "config"` bleibt vollständig unverändert.
- `KNOWN_DISPOSITIONS` um `"RecoveryEvaluation"` ergänzen.
- Falls `run_load_disposition == "RecoveryEvaluation"` beobachtet wird:
  `classification="RECOVERY_EVALUATION_REQUIRES_OWNER_REVIEW"`,
  `result="BLOCKED"` — nicht PASS, nicht automatisch FAIL, keine Vermutung
  über die Ursache.
- Bestehende PASS-Klassifikation für den heute real erwarteten
  `ReachingTarget`-Power-Cut bleibt unverändert:
  `(Current, NoActiveRun, Standby)`. `ResumeOffer` nicht entfernen — bleibt
  ein gültiger, in diesem Harness aktuell nicht erreichbarer Zustand.

## Heute real testbar vs. nicht testbar (Abgrenzung, keine Code-Wirkung)

```text
heute real testbar:
  NVS-Power-Cut (config + run)
  trusted-UTC-Startgate (Blocked ohne UTC, Applied mit UTC)
  Current + ReachingTarget
  Boot-Recovery nach Standby

noch nicht real testbar (spaeteres, separates Gate):
  FERMENTING
  WaitingForTrustedTime -> CurrentRunRecoverable
```

## Gezielte digitale Nachweise (Draft-Phase)

```bash
python scripts/issue_90_slice7_product_runner.py --selftest
python scripts/check_issue90_partitions.py
python scripts/check_issue90_slice7_isolation.py --harness-elf <...> --release-elf <...>
pio run -e native
pio test -e native
python scripts/check_architecture_boundaries.py
git diff --check
```

Nach abgeschlossener Umsetzung, vor dem Owner Implementation Review, frisch
auf dem finalen `HEAD`:

```text
RUNNER_SELFTEST=PASS
ISSUE90_ISOLATION_GATE=PASS
ESP_IDF_BRINGUP_BUILD=PASS
ESP_IDF_RELEASE_BUILD=PASS
FULL_NATIVE_BUILD=PASS
FULL_NATIVE_TESTS=PASS
ARCHITECTURE_GATES=PASS
GIT_DIFF_CHECK=PASS
```

`REAL_POWER_CUTS=NOT_RUN` bis Owner Implementation Review abgeschlossen ist.

## Offene Entscheidungen / Risiken

- Keine. `ResumeOffer` bleibt bewusst als gültiger, aktuell unerreichbarer
  Zustand im Oracle stehen (kein Entfernen ohne Beleg). Kein ADR-, Schema-
  oder #124/#126-Vertragseffekt.

## Owner-Gate

Nach committetem Plan unter
`docs/tasks/issue-90-slice7-host-utc-seam-correction-plan.md` und
Draft-PR-Verweis auf die exakte Plan-SHA: **STOP** für Ownerfreigabe.
Umsetzung erst nach expliziter Freigabe der exakten Plan-SHA. Danach: echte
Power-Cut-Kampagne (3x config, 3x run), actor-free wie im bestehenden
Slice-7-Vertrag.
