# Projekt-Roadmap

Stand: 2026-10-10 (Live-Abgleich gegen GitHub, `main` `21082de`)

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
hardwareunabhaengig:  #19 Werksreset (R1-Pflicht, Plan) | #19 Backup/Import (bedingt)
                      -> weitere #27-Mutationspfade; #28 (formale #19-Abhaengigkeit: Ownerklaerung offen)
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
| #19 – Journale, Aufbewahrung, Bereinigung, Backup, Import | `OPEN`; `PLAN=docs/tasks/issue-19-journals-retention-backup-import-plan.md` (Ownerfreigabe `6fbf130` fuer den R1-Werksreset; R0–R4 und Review-Fix B1 in PR #191 (gemergt, `df5a3ddf41889b46f9b4d3c64085092a88e25a0a`) umgesetzt fuer Ablauf B mit beiden Zugaengen, Hold 5000 ms; **nicht** umgesetzt: Ablauf A (O-R2 = B, mit dem produktiven Service-UI/PIN-Zugang, R1-Pflicht) und S1 (Reset im `ResetEligibleNoRuntime`) ist nach Ownerfreigabe von Plan Revision 8 (`aa665f1`) software-seitig umgesetzt (`SIM-19-S1-01..06`); K1 = A bedingt (Journal-Startbedingung bleibt offen), K2–K7 genehmigt, breitere SSOT-/#28-/#37-Anpassung separat; Implementierungsreview PASS / GO (`f2ab163`, `OPEN_BLOCKERS=0`), PR #191 gemergt; Hardware `NOT_RUN` im Folgeissue #192); **Ownerpriorisierung:** lokaler Vollreset `R1-PFLICHT` (PIN-unabhaengiger Zugang: Ownerentscheid O-R1 = B+, "PIN vergessen?" und `SAFE_BOOT`-Eintrag); Backup und Import `R1-ERWUENSCHT` nur bei nachgewiesener RAM-/Speichereignung (und #27-Pfaden); Journal, Laufhistorie, Bereinigung, Laufexport `DEFERRED_BY_OWNER_PENDING_R1_CONTRACT_RECONCILIATION` (nur Planung); R1-Vertragskonflikte K1–K7 im Plan (u. a. Startbedingung "Fehlerjournal verlaesslich", Abnahme #37); der fruehere Entwurf ist `REVIEW_DRAFT – PRESERVE, NOT APPROVED, NOT CANONICAL` und im Repository nicht enthalten | Reset-Umsetzung R0–R4/S1 gemergt (PR #191); Backup/Import erst nach Ressourcennachweis B0 und Ownerentscheid; Reconciliation der R1-Vertraege durch den Owner |
| #27 – Web-API, Weboberflaeche, Anmeldung | `OPEN`; PR #170 gemergt (Foundation: Login, Sessions, CSRF/Replay, read-only API, lokaler Web-Setup- und Auth-Provisioning-Pfad); der produktive Run-Mutationspfad `POST /internal/ui/run` ist im Composition Root **nicht registriert**; das Hardware-Ressourcengate (Vier-Sessions, Polling) ist bestanden (`docs/audits/PR170_HW_GATE_20261005/`) | Rest gegen `docs/WEB_UI.md` und den bestaetigten R1-Scope abarbeiten oder per Ownerentscheid neu zuschneiden |
| #28 – Diagnose, Diagramme, Serviceablauf, Exporte | `OPEN`; `PLANNED_SPEC_PENDING`; Issue-Abhaengigkeit von #19 (und #20, #22–#25) besteht formal weiter; da Journal/Historie/Export in #19 zurueckgestellt sind, ist die Abhaengigkeit vom Owner zu klaeren; wird nicht vorgezogen | Ownerklaerung der #19-Abhaengigkeit und des R1-Vertragsabgleichs (Plan #19, Abschnitt 7) |
| #30 – reale DS18B20-Sensoradapter | `OPEN`; **Software C1–C4 gemergt** (PR #189, `9beb68f`; Plan Rev. 5 `64f0d9395fc91b309b3ca9f1ad9cee686de7708e`; Independent Review und Fix Verification `PASS / GO`); Sampling-Task bleibt budget-gesperrt (`kApprovedDs18b20TaskBudget` leer); Hardware `NOT_RUN` | Owner-Bestaetigung des Kriterientransfers #30 -> #190, danach Schliessen von #30 |
| #192 – Hardwareverifikation lokaler Werksreset (Folge von #19) | `OPEN / FAIL_HARDWARE` (P3: Stack Overflow in task `main` und Bootloop auf dem Geraet, Befund 2026-10-10); neuer Draft-PR #200 auf aktuellem `main` (`21082de`, enthaelt PR #199); historische Evidence `df5a3dd` (2026-10-08, PR #194 geschlossen, nicht gemergt): Boot/Touchkalibrierung `PASS`, `HW-19-R01` `BLOCKED` (damals Service-Menue/PIN-Seite am Geraet nicht aufrufbar), `HW-19-R02` `NOT_RUN`, `HW-19-R03` `BLOCKED`; der damalige Zugangsblocker ist in `main` behoben (PR #199), die Ergebnisse werden nicht rueckwirkend geaendert; auf dem aktuellen `main` (Geraet `f859ef6`, quellgleich, kein Flash) am 2026-10-10: N1–N8 und P2b per Ownerbeobachtung belegt, **`HW-19-R01` `FAIL`** (G2 erteilt; voller Hold -> Stack Overflow in task `main`, danach Bootloop, Geraet bleibt zur Analyse unveraendert), `HW-19-R02` `NOT_RUN`, `HW-19-R03` `BLOCKED`; Evidence `docs/audits/ISSUE192_FACTORY_RESET_HW_20261010_EVIDENCE.md`; Plan `docs/tasks/issue-192-factory-reset-hardware-verification-plan.md` Revision 2, Evidence `docs/audits/ISSUE192_FACTORY_RESET_HW_20261008_EVIDENCE.md` (historisch); `ACTUATOR_RELEASE=NO` | Ownerentscheid zum Befund (Analyse, Wiederherstellung des Geraets = neue Freigabe noetig, Fix-Issue); G2 verbraucht, G3 vor Powercut-Cutpoints; R03 `BLOCKED`, solange kein sicherer `SAFE_BOOT`-/NoRuntime-Einstieg benannt ist |
| #190 – DS18B20-Hardwareverifikation (Folge von #30) | `OPEN / BLOCKED_HARDWARE`; Software-Voraussetzung PR #189 gemergt (`9beb68f`); H1–H7 `NOT_RUN` | Reale Bus-, ROM-, CRC-, Hot-Plug-, Fehler- und Ressourcenpruefung; Task-Budget aus Hardwaremessung |
| #32 – Luefter, Summer, Onboard-MOSFET-Ausgaenge | `OPEN / BLOCKED_HARDWARE`; eigener Hardware-/Adapterscope; #28/#35/#106 sind keine Abschlussvoraussetzung; Softwareplan `docs/tasks/issue-32-onboard-mosfet-outputs-plan.md` (Revision 1, freigegeben `2c1d32b`); S1–S3 softwareseitig in PR #195 gemergt (`92d6b823d566443226952917ec36c6b4f4125ef8`; Adapter, SSOT-Generator, Composition Root, ohne Planner-Anbindung), Independent Software Review PASS / GO auf `515386b1cdee3d25ded1eacc9545fc96af2be18b`, `pre-ready/local=success` am PR-HEAD `1c15da6`; Hardware `NOT_RUN`, Polaritaet `Unconfirmed`, `ACTUATOR_RELEASE=NO` | Funktionale Hardwareverifikation ueber den produktionsnahen Adapter-/Treiberpfad; keine produktive `Allowed`-Freigabe |
| #33 – BTS7960, R_IS/L_IS, begrenzte Peltierpruefungen | `OPEN / BLOCKED_HARDWARE`; folgt auf #32; R_IS/L_IS fuer R1 bewusst unbeschaltet (`FUTURE_RELEASE`); Softwareplan `docs/tasks/issue-33-bts7960-hbridge-plan.md` (Revision 2, Plan-SHA `d55d2db` freigegeben); S1–S5 softwareseitig in PR #196 gemergt (`f33e8593c79920aca9084528547a220f284b9f42`; Port `bool`, Aussenluefter-Sperre, Bruecke, ESP-Adapter, Generator/Composition Root ohne Planner-Anbindung), Independent Review PASS (Ownerangabe), `pre-ready/local=success` am PR-HEAD `1a52521`; `REAL_PELTIER_TEST=NOT_RUN`, `ACTUATOR_RELEASE=NO` | H-Bruecken-Adapter-Safety und begrenzte sichere Serviceprüfung |
| #106 – Aktorplaner Per-Run-Snapshot und Recovery-Bindung | `OPEN`; strukturell erledigt (PR #157, `2c010e8`); produktiver Abschluss an #35 gebunden | Produktive Werte nach #35 |
| #34 / #35 – Sensorvergleich, PI-/Luft-/Aktor-/Sicherheitsparameter | `OPEN / TBD_COMMISSIONING`; nach #30/#31/#32/#33 | Reale Messreihen, Werte- und Safetyfreigabe |
| #36 / #37 – Hardwareabnahme, 7-Tage-Belastungstest | `OPEN`; spaetere Releasegates | Nach den Hardware- und Commissioningissues |
| #188 – A: Service (PIN) aus Einstellungen gesperrt; B: Doku SIM-26-21/65 | `OPEN` (administrativer Abschluss nach Merge des #192-Doku-PRs #200); **A** erledigt: PR #199 gemergt (`21082de`; C1 `c7b6887`, C2 `876a33e`, C3 `f859ef6`, Hardware-Evidence `HW-188-A01` `4393d27`, `docs/audits/ISSUE_188_A_HW_G2_20261009_EVIDENCE.md`; regulaerer Zugang `Einstellungen -> Service (PIN) -> PIN-Seite -> PIN vergessen?` in `main`; Inaktivitaet auf Hardware `NOT_RUN`, Owner akzeptiert); **B** durch Ownerentscheid (2026-10-10) ohne zusaetzliche Tests oder Nachtests akzeptiert; die Korrektur der Trace-Referenzen `SIM-26-21`/`SIM-26-65` in `docs/ACCEPTANCE_TESTS.md` erfolgt im Zuge des #192-Doku-PRs #200 (kein separates Issue/PR); `ACTUATOR_RELEASE=NO` | Merge von PR #200, danach #188 administrativ schliessen |
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
| #19 PR #191 (`df5a3dd`) | lokaler Werksreset Ablauf B inkl. S1 (Issue bleibt offen, siehe oben) |
| #32 PR #195 (`92d6b82`) | Onboard-MOSFET-Ausgabeadapter, Software (Issue bleibt offen, Hardware `NOT_RUN`) |
| #33 PR #196 (`f33e859`) | BTS7960-Brueckenlogik und -Adapter, Software (Issue bleibt offen, Hardware `NOT_RUN`) |
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
