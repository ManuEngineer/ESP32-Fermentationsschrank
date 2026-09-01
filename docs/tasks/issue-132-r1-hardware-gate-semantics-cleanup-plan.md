# Issue #132 – R1-Hardwaregate-Semantik vor Integration bereinigen

## Ziel

Die aktuelle R1-Hardware-Verifikationssemantik wird vor einer späteren
Promotion von `integration/r1-development` nach `main` kanonisch und
widerspruchsfrei gemacht. Nicht ausgeführte elektrische Pegelmessungen werden
nicht als PASS behauptet. Die Nachweisarten bleiben je Hardware-Scope getrennt:

```text
ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED
SSOT_CONFORMANCE=<PASS/PENDING/NOT_APPLICABLE>
FUNCTIONAL_HARDWARE_VERIFICATION=<PASS/PENDING/NOT_APPLICABLE>
ADAPTER_SAFETY_VERIFICATION=<PASS/PENDING/NOT_APPLICABLE>
THERMAL_COMMISSIONING=<PASS/PENDING/NOT_APPLICABLE>
```

Verbindliche Ownerentscheidungen:

```text
MULTIMETER_REQUIRED_FOR_R1_ACCEPTANCE=NO
BOOT_LEVEL_MEASUREMENT_REQUIRED=NO
GPIO_VOLTAGE_MEASUREMENT_REQUIRED=NO
ELECTRICAL_LEVEL_MEASUREMENT_REQUIRED=NO
R_IS_L_IS_R1=DISABLED
R_IS_L_IS_LEVEL_MEASUREMENT_REQUIRED=NO
```

## Scope und Nicht-Ziele

### Im Scope

- `docs/ROADMAP.md` nach dem Abschluss von Issue #29 / PR #129 und Issue
  #130 / PR #131 synchronisieren. Die nächste fachliche Phase bleibt exakt
  `#25 -> #26 -> #31 -> #30 -> #32 -> #33`.
- Den generischen aktuellen Sammelstatus
  `ELECTRICAL_VERIFICATION=PENDING` und die abgeleitete
  `pending_electrical_verification`-Sprache in normativen aktuellen Stellen
  durch die passenden getrennten Statusfelder ersetzen.
- Das Boardprofil
  `config/board_profiles/esp32_32e_quad_mosfet_r1.yaml` nur semantisch
  aktualisieren: keine GPIO-Neuzuordnung, keine Änderung von Pulls, Netzen,
  Widerständen, Designwerten oder aktiven Pegelwerten.
- Das Beispielprofil sowie die kanonischen Hardware-/Akzeptanz-/Entscheidungs-
  dokumente synchronisieren, soweit sie den aktuellen R1-Gatevertrag
  beschreiben.
- Die Live-Bodies von Issue #29, #31, #32, #33 und #130 auf denselben Vertrag
  synchronisieren. Abgeschlossene technische Evidenz wird nicht neu
  interpretiert.
- R_IS/L_IS für R1 explizit deaktiviert lassen; eine spätere Nutzung erhält
  ausschließlich den Status `FUTURE_RELEASE` und einen eigenen
  Plan-/Issue-/Owner-Gate-Scope.
- Repositoryweite Such-, Diff- und gegebenenfalls YAML-/Dokumentationsprüfungen
  ausführen und ihre Ergebnisse im Draft-PR dokumentieren.

### Nicht-Ziele

Keine #25- oder #26-Arbeit, keine Produktlogik, keine Firmware-, Adapter-,
Schema-, Safety- oder Bibliotheksänderung, keine realen Display-, Sensor-,
Lüfter-, Summer-, BTS7960- oder Peltierarbeiten, keine GPIO-Neuzuordnung, keine
neue Hardwaremessung, keine Aktorfreigabe und kein Integrations-PR nach
`main`.

## Verifizierte Live-Baseline

Die Planung basiert auf dem am 2026-09-01 live verifizierten Stand:

```text
REPOSITORY=ManuEngineer/ESP32-Fermentationsschrank
BASE_BRANCH=integration/r1-development
BASE_SHA=1fc22d693bae8572144bf61d242a2fe6d0b093bc
MAIN_SHA=87dd593fcdc8d26831873a4163b174340b4347c0
INTEGRATION_AHEAD_OF_MAIN=257
INTEGRATION_BEHIND_MAIN=0
ISSUE29=CLOSED_COMPLETED
PR129=MERGED
PR129_MERGE_COMMIT=1fc22d693bae8572144bf61d242a2fe6d0b093bc
PR131=MERGED
PR131_MERGE_COMMIT=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
ISSUE130=CLOSED
ISSUE132=OPEN
```

Der aktuelle Arbeitszweig wird von `origin/integration/r1-development`
abgeleitet. Der bestehende lokale Checkout `agent/issue-29-requalification-r1`
war veraltet und wurde nicht als Basis verwendet.

## Betroffene Quellen und Konsumenten

Primäre Quellen sind `docs/ROADMAP.md`,
`config/board_profiles/esp32_32e_quad_mosfet_r1.yaml`,
`docs/HARDWARE.md`, `docs/DECISIONS.md`, `docs/ACCEPTANCE_TESTS.md` sowie die
Live-Issue-Bodies #29, #31, #32, #33 und #130. Das Beispiel
`config/hardware.example.yaml` führt einen abgeleiteten
`hardware_release.state`-Status und wird konsistent angepasst.

Die Repositorysuche der Baseline zeigt keine Produktionscode-, Test- oder
Generatorauswertung der Statusfelder. Die Änderungen bleiben deshalb auf
Konfiguration, Dokumentation und Live-Metadaten begrenzt. Falls die laufende
Konsumentenprüfung einen bisher nicht sichtbaren Schema-/Generatorpfad findet,
wird die Umsetzung an dieser materiellen Abweichung angehalten und der Plan
aktualisiert.

Historische Abschnitte in `docs/ISSUE_29_BUILD_REPORT.md` und
`docs/ISSUE_29_MEASUREMENTS.md` bleiben als Evidenz erhalten. Nur aktuelle
Statusdarstellungen werden umgestellt; alte Sammelstatus werden in
historischen/superseded Kontexten nicht rückwirkend umgedeutet.

## Umsetzungsschnitte nach Planfreigabe

1. Aktuelle Roadmap- und kanonische Dokumentstatus auf Issue #29 / PR #129,
   Issue #130 / PR #131 und die neue Statusmatrix umstellen.
2. Boardprofil und Beispielkonfiguration minimal synchronisieren. Vorher und
   nachher werden GPIO-, Pull-, Netz- und Designwerte strukturell verglichen.
3. Issue #29, #31, #32, #33 und #130 mit den getrennten Statusfeldern,
   funktionalen Nachweisgrenzen und der R1-R_IS/L_IS-Deaktivierung
   synchronisieren.
4. Den vollständigen Diff sowie die repositoryweite Statussuche gegen diesen
   Plan prüfen und den Draft-PR mit exakter HEAD-SHA und Nachweisen
   aktualisieren.

## Gezielte Nachweise

Nach Planfreigabe:

- `git diff --check` – PASS/FAILED;
- repositoryweite Suche nach den im Auftrag genannten alten Status- und
  Messbegriffen – PASS mit bewerteten historischen Treffern oder FAILED;
- YAML-/Konfigurationssyntax und direkt betroffene Dokumentations-/Generator-
  prüfungen, nur falls die Konsumentenprüfung einen solchen Pfad bestätigt;
- strukturierter Vergleich des Boardprofils vor/nach der Statusänderung –
  GPIO-Zuordnung, Pull-Konfiguration, Netze und Designwerte unverändert;
- keine Firmware-, ESP-IDF-, Hardware- oder vollständigen Gesamttests.

Nicht ausgeführte Nachweise werden als `NOT_RUN` oder `BLOCKED` bezeichnet und
nicht als PASS ausgegeben. Der PR bleibt Draft; der Owner reviewt und mergt
allein. Erst nach Owner-Review und Merge dieses PRs darf ein separater
Integrations-PR `integration/r1-development -> main` angelegt werden.

## Owner-Gate

```text
APPROVED_PLAN_SHA=PENDING
OWNER_PLAN_REVIEW_REQUIRED=YES
IMPLEMENTATION_ALLOWED=NO
```

Die Umsetzung beginnt erst nach ausdrücklicher Freigabe dieser exakten
Plan-Commit-SHA.
