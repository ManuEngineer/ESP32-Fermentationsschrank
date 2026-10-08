# Projekt-Roadmap

Stand: 2026-10-08 (Live-Abgleich gegen GitHub, `main` `9beb68f`)

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt. Die vollstaendige fruehere Statushistorie
(inklusive aller damaligen Evidence-Kennungen) steht unveraendert in
[`audits/ROADMAP_ARCHIVE_20261008.md`](audits/ROADMAP_ARCHIVE_20261008.md);
sie ist nicht normativ und kann veraltete Statusangaben enthalten.

Durchgaengig gilt: `ACTUATOR_RELEASE=NO`. Keine Zeile dieser Datei behauptet
eine Hardware-, Safety- oder Aktorfreigabe.

## Aktuelle Reihenfolge

```text
abgeschlossene Basis: #29 -> #90 -> #121 -> #124 -> #126 -> #25 -> #144 -> #152
                      -> #26 -> #31 -> #172 -> #164/#174 -> #178/#181 -> #27 (Foundation)
                      -> #30 (Software)
hardwareunabhaengig:  #19 (Plan) -> #28 -> weitere #27-Mutationspfade
reale Hardware:       #190 (DS18B20) -> #32 -> #33
                      -> erste real bedienbare Fermenter-Hardwareintegration
                      -> #106 strukturell (erledigt) -> #34 -> #35 -> #106 produktiv
                      -> #36 -> #37 (Abnahme-/Releasegates)
```

Es entsteht keine separate Wegwerf-Testsoftware und kein zweiter temporaerer
Bedien- oder Diagnosevertrag. Schmale Low-Level-Nachweise bleiben zulaessig,
wenn sie fuer Treiber, Pegel, Boot-/Reset-Sicherheit oder Fehlersuche
notwendig sind.

## Offene Arbeit

| Issue | Status | Naechstes Gate |
|---|---|---|
| #19 – Journale, Aufbewahrung, Bereinigung, Backup, Import | `OPEN`; `PLAN=docs/tasks/issue-19-journals-retention-backup-import-plan.md`; `PLAN_STATUS=DRAFT_AWAITING_INDEPENDENT_REVIEW_AND_OWNER_APPROVAL`; keine Implementierung; der fruehere Entwurf ist `REVIEW_DRAFT – PRESERVE, NOT APPROVED, NOT CANONICAL` und im Repository nicht enthalten | Unabhaengiger Plan-Review, Ownerentscheide, Freigabe der exakten Plan-SHA |
| #27 – Web-API, Weboberflaeche, Anmeldung | `OPEN`; PR #170 gemergt (Foundation: Login, Sessions, CSRF/Replay, read-only API, lokaler Web-Setup- und Auth-Provisioning-Pfad); der produktive Run-Mutationspfad `POST /internal/ui/run` ist im Composition Root **nicht registriert**; das Hardware-Ressourcengate (Vier-Sessions, Polling) ist bestanden (`docs/audits/PR170_HW_GATE_20261005/`) | Rest gegen `docs/WEB_UI.md` und den bestaetigten R1-Scope abarbeiten oder per Ownerentscheid neu zuschneiden |
| #28 – Diagnose, Diagramme, Serviceablauf, Exporte | `OPEN`; `PLANNED_SPEC_PENDING`; hart abhaengig von #19 (und #20, #22–#25); wird nicht vorgezogen | Nach freigegebenem und umgesetztem #19 |
| #30 – reale DS18B20-Sensoradapter | `OPEN`; **Software C1–C4 gemergt** (PR #189, `9beb68f`; Plan Rev. 5 `64f0d9395fc91b309b3ca9f1ad9cee686de7708e`; Independent Review und Fix Verification `PASS / GO`); Sampling-Task bleibt budget-gesperrt (`kApprovedDs18b20TaskBudget` leer); Hardware `NOT_RUN` | Owner-Bestaetigung des Kriterientransfers #30 -> #190, danach Schliessen von #30 |
| #190 – DS18B20-Hardwareverifikation (Folge von #30) | `OPEN / BLOCKED_HARDWARE`; H1–H7 `NOT_RUN` | Reale Bus-, ROM-, CRC-, Hot-Plug-, Fehler- und Ressourcenpruefung; Task-Budget aus Hardwaremessung |
| #32 – Luefter, Summer, Onboard-MOSFET-Ausgaenge | `OPEN / BLOCKED_HARDWARE`; eigener Hardware-/Adapterscope; #28/#35/#106 sind keine Abschlussvoraussetzung | Funktionale Hardwareverifikation ueber den produktionsnahen Adapter-/Treiberpfad; keine produktive `Allowed`-Freigabe |
| #33 – BTS7960, R_IS/L_IS, begrenzte Peltierpruefungen | `OPEN / BLOCKED_HARDWARE`; folgt auf #32; R_IS/L_IS fuer R1 bewusst unbeschaltet (`FUTURE_RELEASE`) | H-Bruecken-Adapter-Safety und begrenzte sichere Serviceprüfung |
| #106 – Aktorplaner Per-Run-Snapshot und Recovery-Bindung | `OPEN`; strukturell erledigt (PR #157, `2c010e8`); produktiver Abschluss an #35 gebunden | Produktive Werte nach #35 |
| #34 / #35 – Sensorvergleich, PI-/Luft-/Aktor-/Sicherheitsparameter | `OPEN / TBD_COMMISSIONING`; nach #30/#31/#32/#33 | Reale Messreihen, Werte- und Safetyfreigabe |
| #36 / #37 – Hardwareabnahme, 7-Tage-Belastungstest | `OPEN`; spaetere Releasegates | Nach den Hardware- und Commissioningissues |
| #188 – Doku: SIM-26-21/65 verweisen auf nicht existierenden Testpfad | `OPEN`; vorbestehender Doku-Befund | Eigener Scope, nicht Teil anderer PRs |
| #176 / #177 – Device-Platform-Architekturziel, Projektlizenz | `OPEN`; Governance-/Zielbild | Ownerentscheid |
| #114 / #163 – Future-Scope (Advanced Safety v2, Produkt-Enhancements) | `OPEN`; `FUTURE_SCOPE_REFERENCE_NON_NORMATIVE`, kein Release-1-Gate | Vollstaendige Neuplanung auf dann aktuellem Stand |

Epics bleiben offen, weil verbleibende Reste offen sind: **#4** (Rest: #19),
**#5** (Rest: produktives #106), **#6** (Rest: #27, #28), **#7** (Rest: #30
Abschluss, #32, #33, #190), **#8** (E6-Commissioning und Abnahme). #16 ist
abgeschlossen (2026-10-08).

## Offene Akzeptanz-/Hardware-Gates aus abgeschlossenen Arbeiten

Diese Punkte sind nicht erledigt, auch wenn das zugehoerige Issue geschlossen
ist; Details stehen in den verlinkten Dokumenten.

- **#164 (WLAN, `CLOSED`)**: physische Akzeptanz (Browser-, Test-before-Commit-,
  Neustart-, Persistenz- und Reconnect-Tests) ist `DEFERRED` und `NOT_RUN`.
  Evidence: `docs/tasks/issue-164-owner-hardware-test-evidence-2026-09-29.md`.
- **#174/#27 (RAM)**: die einmalig beobachtete nicht-fatale Idle-Allokation
  (`HEAP_ALLOC_FAILED` 1696 B) bleibt als nicht blockierende Nachverfolgung
  offen und wurde nicht reproduziert; Evidence
  `docs/audits/R1_RAM_LVGL48_HW_EVIDENCE.md`, `docs/audits/PR170_HW_GATE_20261005/`.
  Endgueltige harte Systemgrenzen bleiben der finalen R1-Integrationsqualifikation
  vorbehalten.
- **#172 (Touch-UI, `CLOSED`)**: Hardware nur teilweise Ownerabnahme; nicht
  ausgefuehrt: Layout S7, S8/S9, Geraetename-Persistenz, Labelbreiten,
  Tastengroesse/Detail, D10-Commit-Logs.
- **#126 (Zeit, `CLOSED`)**: reale RTC-Hardware (`BLOCKED_OWNER_HARDWARE_PENDING`)
  und reale NTP-Netzwerklaeufe (`NOT_RUN`).
- **#29/#130 (Bring-up, GPIO-SSOT)**: elektrische Pegelmessung
  `NOT_REQUIRED_WAIVED`; GPIO-Matrix `PLANNED_NOT_CONFIRMED`; funktionale
  Hardwareverifikation `PENDING`.
- **Thermische Parameter und Releaseabnahme** bleiben bis zu den realen
  Messungen und Belastungstests blockiert (`docs/OPEN_POINTS.md`).
- **OTA** ist fuer ein spaeteres Release vorgesehen, kein Release-1-Scope.

## Abgeschlossen (Kurzliste, Merge-Commits verifiziert)

Details: Live-Issue/PR, freigegebener Plan in `docs/tasks/`, Audits in
`docs/audits/`, Detailhistorie im [Archiv](audits/ROADMAP_ARCHIVE_20261008.md).

| Arbeit | Ergebnis |
|---|---|
| #17 PR #84 (`8d65b50`), #18 PR #102 (`10ff98e`), #20 PR #95 (`306bda5`), #21 PR #99 (`082fb3f`), #22 PR #104 (`2986dca`), #23 PR #105 (`b8eae5f`), #24 PR #110 (`a802b1f`) | Laufpersistenz, Wiederanlauf, Sensorqualitaet, Regelsensorauswahl, PI-Regelung, Aktorplaner, Release-1-Fehler-/Interlock-Vertrag |
| #29 PR #129 (`1fc22d6`), #90 PR #128 (`c1f5fbb`), #121, #124 PR #125 (`5b8b86b`), #126 PR #127 (`18fb96b`) | Bring-up-Baseline, ESP-IDF-NVS-Adapter mit realen Power-Cuts, Lifecycle-/Safety-Vereinfachung, Stromausfall-Recovery, absolute Zeitplattform |
| #119 / PR #120 | `CLOSED/SUPERSEDED` (PR nicht gemergt) – fehlgeschlagener Zwischenansatz, ersetzt durch #121 |
| #130 PR #131 (`1fd8f6a`), #134 PR #135 (`86e5549`), #136 PR #137 (`c347875`), #148 PR #149 (`e84dfa8`) | GPIO-SSOT, Integrationscheckpoint nach `main`, Korrekturen, `main` wieder Entwicklungsbasis |
| #25 PR #142 (`87bd668`), #144 PR #147 (`0b8b4cc`), #152 PR #153 (`5d838f4`), #26 PR #143 (`253f613`) | Device-UI-Shell, Run-Identity, manueller Zeit-/Temperaturlauf, Fermentations-Workspace |
| #31 PR #156 (`b871375`), #172 PR #179/#183/#186/#187 (`d644a31`/`dc938b2`/`0718ef9`/`7b16dbe`), #168 PR #169 (`b8d963e`) | Display/Touch real, lokale Touch-UI vollstaendig, Runtime-Evidence-Projektion |
| #89 PR #158 (`c5aa9ca`), #164 PR #165 (`1f1755e`) / Plan-PR #171 (`cafacb2`), PR #174 (`8a734f6`, R1-RAM-Stabilisierung), #178 PR #180 (`9cfffdf`), #181 PR #182 (`2d9fc52`) | WLAN-Evaluation und -Integration, R1-RAM-Stabilisierung, lokale Zeit/IANA-Zeitzonen, SNTP-Trust-Bugfix |
| #27 PR #170 (`02b7523`) | Web-Foundation (Issue bleibt offen, siehe oben) |
| #30 PR #189 (`9beb68f`) | DS18B20-Software C1–C4 (Issue bleibt offen, siehe oben) |
| #106 PR #157 (`2c010e8`) | Per-Run-Producer-/Schema-/Snapshotmechanismus strukturell |
| #145 PR #146 (`f5aca94`), #150 PR #151 (`913f4c9`), #154 PR #155 (`54c80d2`), #184 PR #185 (`271b510`), #111 PR #113 (`bab3bcc`), PR #161 (`0f37004`) | Governance, Pre-Ready-Gates, lokaler `pre-ready/local`-Status als Merge-Gate, Plan Mode, CI-Beschleunigung |
| #159 PR #160 (`dc017f4`) | ESP-IDF 6.1 als Produktionsbasis |

## Pflege

Aktualisierung ist erforderlich:

- zu Beginn jedes neuen Pull Requests;
- nach jedem Merge;
- bei materieller Reihenfolgeaenderung;
- bei neuem Blocker oder Ownerentscheid.

Details bleiben in Live-Issue, Pull Request, freigegebenem Plan, ADR oder
Fachvertrag.
