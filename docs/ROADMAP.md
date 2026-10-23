# Projekt-Roadmap

Stand: 2026-10-21 (Live-Abgleich gegen GitHub, `main` `086873a`)

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
| #19 – Journale, Aufbewahrung, Bereinigung, Backup, Import | `OPEN`; `PLAN=docs/tasks/issue-19-journals-retention-backup-import-plan.md` (Ownerfreigabe `6fbf130` fuer den R1-Werksreset; R0–R4 und Review-Fix B1 in PR #191 (gemergt, `df5a3ddf41889b46f9b4d3c64085092a88e25a0a`) umgesetzt fuer Ablauf B mit beiden Zugaengen, Hold 5000 ms; **nicht** umgesetzt: Ablauf A (O-R2 = B, mit dem produktiven Service-UI/PIN-Zugang, R1-Pflicht) und S1 (Reset im `ResetEligibleNoRuntime`) ist nach Ownerfreigabe von Plan Revision 8 (`aa665f1`) software-seitig umgesetzt (`SIM-19-S1-01..06`); K1 = A bedingt (Journal-Startbedingung bleibt offen), K2–K7 genehmigt, breitere SSOT-/#28-/#37-Anpassung separat; Implementierungsreview PASS / GO (`f2ab163`, `OPEN_BLOCKERS=0`), PR #191 gemergt; Hardware `NOT_RUN` im Folgeissue #192); **Ablauf A (PIN-geschuetzt) umgesetzt und gemergt (PR #202, `b903de8`):** `PLAN=docs/tasks/issue-19-factory-reset-a-plan.md` (Plan-SHA `adb0aa5` freigegeben, D-A1 = A), A1–A3 softwareseitig, `SIM-19-A-01..05`/`A-03b`; Hardware `NOT_RUN` (#192); **Ownerpriorisierung:** lokaler Vollreset `R1-PFLICHT` (PIN-unabhaengiger Zugang: Ownerentscheid O-R1 = B+, "PIN vergessen?" und `SAFE_BOOT`-Eintrag); Backup und Import `R1-ERWUENSCHT` nur bei nachgewiesener RAM-/Speichereignung (und #27-Pfaden); Journal, Laufhistorie, Bereinigung, Laufexport `DEFERRED_BY_OWNER_PENDING_R1_CONTRACT_RECONCILIATION` (nur Planung); R1-Vertragskonflikte K1–K7 im Plan (u. a. Startbedingung "Fehlerjournal verlaesslich", Abnahme #37); der fruehere Entwurf ist `REVIEW_DRAFT – PRESERVE, NOT APPROVED, NOT CANONICAL` und im Repository nicht enthalten | Reset-Umsetzung R0–R4/S1 gemergt (PR #191); Backup/Import erst nach Ressourcennachweis B0 und Ownerentscheid; Reconciliation der R1-Vertraege durch den Owner |
| #27 – Web-API, Weboberflaeche, Anmeldung | **OPEN**; #170 (Foundation), #201 (Web-Bedienroute), #203 (Dashboard/F1/F4/Single-Flight), [#205](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/205) (kanonischer Mutationsheader), [#206](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/206) (Laufinformationen), [#207](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/207) (Standbyinformationen) und [#208](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/208) (Sprachwahl) gemergt; R1-Web-Vertrag noch unvollstaendig | **Aktueller Stand (Implementierung, Draft-PR #209):** integrierter R1-Web-Restumfang in einem PR laut [Plan Revision 2.4 (Q3 freigegeben; Run-Delta nicht freigegeben)](tasks/issue-27-r1-web-remaining-scope-plan.md) (Ownerentscheide D1–D9 sowie Q1/Q2 bestaetigt: oeffentlicher versionierter Asset, Limits Shell 2048 B / Setup-Shell 4096 B / Asset 16384 B; C0 Asset-/Vertragsgrundlage, C1 Navigation/Meldungen, C2 Programme mit Detail-API, C3 Einstellungen nur Geraetename ueber geschlossenen Dispatch mit Herkunft `WebInterface` und Hinweis auf verzoegerte Netzwerkwirkung, Zeitzone nur lesbar (Schreiben zurueckgestellt), C5 Integration; C4 Service entfaellt). Q1/Q2 entschieden; **Plan-SHA `b7d5e2dbc1f5281c764d4a75f2c37f1c4b6397c2` und Implementierung C0 → C1 → C2 → C3 → C5 am 2026-10-21 vom Owner freigegeben**. Softwarekandidat `4f6f0c9`: C0–C3/C5 integriert, 437 native und 89 JS-Faelle sowie beide Profilbuilds PASS; **Ressourcen-HOLD: HTTPD-Settings-Stack mindestens 8528 B > 8192 B**, `OPEN_BLOCKERS=1` ([Evidence](audits/PR209_WEB_REMAINING_SCOPE_IMPLEMENTATION.md)). Q3 auf Plan-SHA `1b00baea77a6168f29172ffa91852e10fdfd0c7e` nach Review freigegeben (Owner am 2026-10-22 nochmals bestaetigt), begrenzt umgesetzt auf `095f393` ([Q3-Evidence](audits/PR209_Q3_IMPLEMENTATION_20261022.md)): Scratch 1856 B / Alignment 8 B, ausgewaehlte Settings-Frames jetzt 2928 B (keine Vollkettenqualifikation), 398 gezielte native Tests und beide Profile PASS; **unveraenderter Run-Frame `applyConfirmedPrepared` 10880 B >8192 B, HTTPD-Vollkettenstack weiter FAIL/HOLD**. Gezieltes [Run-Diagnose-/Plandelta](audits/PR209_RUN_STACK_DIAGNOSIS_20261022.md), Revision 2.4 §13: Decision10104 B, Visitor20240 B, StartInto8000 B; Root-only-Fix reicht nicht, kombinierte [Q4-Machbarkeitsprobe](audits/PR209_Q4_FEASIBILITY_20261022.md) **Q4_FEASIBILITY_HOLD**: alle Web-Visitor32 B, aber Program-Kern9648 B >8192 B wegen bestehender ActiveRun-Factory3136 B und unveraendertem Webframe2048 B; weitere Produktkorrektur nicht freigegeben. Begrenzter [Q4-Ownership-Prototyp](audits/PR209_Q4_OWNERSHIP_PROTOTYPE_20261023.md) jetzt in einer gemeinsamen temporaeren Sourcevariante fuer beide Profile kompiliert: instanzlokaler Borrow vor Candidate-Zugriff, Guard216 / View536 B, Coordinator unveraendert21352 B und neuer fester Heap0 B; keine globale Locks-/Value-Konsumenten-Umstellung. **IMPLEMENTATION_READY=HOLD / FULL_CHAIN_RESERVE=NOT_PROVEN**: Application1312 / Web1200 B, aber berechnete Run-Teilketten mit HTTPD-/Allocatorpfad7856 B, nur336 B Rest vor weiteren unaufgeloesten Pfaden; Settings-Teilpfad7616 B ebenfalls keine Obergrenze. Kein Semantik-/Heap-/HWM-/Ressourcen-PASS. Eine Owneralternative: zusammenhaengenden groesseren Persistenz-/Validierungskern-Scope entscheiden oder HOLD; keine automatische weitere Mikroprobe und kein neuer grosser Heapowner als Default. Weitere Produktkorrekturen STOP; keine Budgeterhoehung und kein Heap-/HWM-PASS, Independent Full Review offen; D9 Browser-/Geraetemessung nur nach gesonderter Freigabe. Abgetrennt/offen bleiben Manueller Betrieb (#35/Hardware), Diagnose/Diagramme/Exporte (#28), Journale/Protokolle/Backup/Werksreset (#19), Service-Funktionen, Programmeditor, Favicon (#204). **Gemergte Slices (aktuelle Zusammenfassung, juengster zuerst):** **#208** [MERGED 2026-10-20T15:13:41Z](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/208), Merge `086873a` (Plan `a671c33`): browserindividuelle Sprachwahl DE/EN/ES in Setup- und Dashboard-Shell (`localStorage.lang`, rein clientseitig), Shell 11312 B bei einmaligem Limit 11456 B (16 B ueber der Normalreserve, Kopf 1534/1536 B), `pre-ready/local=success` auf `26173ab`; [Offline-Ressourcenevidence](audits/PR208_WEB_LANGUAGE_RESOURCE_EVIDENCE_20261020.md): historische Offline-Nachweise JS 74/74, native Tests und beide ESP-Profile PASS; `.dram0.*`/`.iram0.*`/`.flash.text` ±0, `.flash.rodata` +656 B. **#207** [MERGED 2026-10-20T12:58:54Z](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/207), Merge `43f0338` (Plan `9656a18`): Standby-Informationen (Geraetename, getrennter Netzwerkmodus/-zustand, Lokalzeit), Shell 11016 B bei einmaligem Limit 11264 B, `pre-ready/local=success` auf `f9b0a84`; [Offline-Ressourcenevidence](audits/PR207_STANDBY_INFO_RESOURCE_EVIDENCE_20261020.md). **#206** MERGED 2026-10-19T19:55:15Z, Merge `45e7e63` (Plan-Revision 2 `3376a19`, D1-C = 10752 B, C1–C3, Independent Implementation Review `GO`, `OPEN_BLOCKERS=0`): Laufinformationen, Shell 10410 B bei Limit 10752 B ([Ressourcenevidence](audits/PR206_C1_C3_RESOURCE_EVIDENCE_20261019.md)). Browser-/Geraeteverhalten dieser Slices ist `NOT_PROVEN`. **Historische Hardware-Evidence (#206, H-RAM-1 am 2026-10-19, kein Stand der aktuellen Slices):** Mess-Overlay M, **begrenzter positiver Teilnachweis** ([Evidence](audits/PR206_H_RAM_1_EVIDENCE_20261019.md)): min. interner Heap 19 896 B, HTTPD-Stack 2 196 B frei, Main-Stack 11 272 B frei, keine beobachteten OOM-/UI-Skip-/Reset-Ereignisse; `RAM_PASS=NOT_PROVEN` fuer das vollstaendige R1-/Laufprofil; S4 (aktiver Lauf) `NOT_RUN`; Produktimage nicht hardwarequalifiziert; Mehrclient/Touch/Langzeit nicht geprueft; **FOLLOW-UP Transportstabilitaet:** ein ungeloester Client-Timeout im gescripteten Replay (117/118 Gruppen). Auf dem Geraet ist `a9a1156` installiert, nicht #206–#208. Weitere Dashboard-/Navigations-/Systemthemen getrennt gegen [WEB_UI](WEB_UI.md). [Headerfix-Evidence](audits/PR205_WEB_MUTATION_HEADER_20261019.md), [historische Ergebnisgrenzen](audits/PR203_G2_WEB_B0_B4_20261018/REVIEW_ADDENDUM.md), [RES-01/Ownerentscheid](tasks/issue-27-res01-heap-hardening-plan.md): G1′ storniert NOT_RUN/NOT_PROVEN, F4 PROVISIONAL 13312/16640 B mit akzeptiertem begrenztem D3-Restrisiko; B0–B4 nur funktionaler Teilnachweis, formale G2-/R1-U-/D9-/Aktorabnahme offen. Historische FAIL/HOLDs und archive/pr203-Tags erhalten; keine neue Hardwarefreigabe. `ACTUATOR_RELEASE=NO`.
| #28 – Diagnose, Diagramme, Serviceablauf, Exporte | `OPEN`; `PLANNED_SPEC_PENDING`; Issue-Abhaengigkeit von #19 (und #20, #22–#25) besteht formal weiter; da Journal/Historie/Export in #19 zurueckgestellt sind, ist die Abhaengigkeit vom Owner zu klaeren; wird nicht vorgezogen | Ownerklaerung der #19-Abhaengigkeit und des R1-Vertragsabgleichs (Plan #19, Abschnitt 7) |
| #30 – reale DS18B20-Sensoradapter | `OPEN`; **Software C1–C4 gemergt** (PR #189, `9beb68f`; Plan Rev. 5 `64f0d9395fc91b309b3ca9f1ad9cee686de7708e`; Independent Review und Fix Verification `PASS / GO`); Sampling-Task bleibt budget-gesperrt (`kApprovedDs18b20TaskBudget` leer); Hardware `NOT_RUN` | Owner-Bestaetigung des Kriterientransfers #30 -> #190, danach Schliessen von #30 |
| #192 – Hardwareverifikation lokaler Werksreset (Folge von #19) | `OPEN` (`HW-19-R01` `PASS` mit akzeptierter Abweichung nach P3c 2026-10-11; historisch P3 am 2026-10-10: Stack Overflow in task `main` und Bootloop, `FAIL`); neuer Draft-PR #200 auf aktuellem `main` (`21082de`, enthaelt PR #199); historische Evidence `df5a3dd` (2026-10-08, PR #194 geschlossen, nicht gemergt): Boot/Touchkalibrierung `PASS`, `HW-19-R01` `BLOCKED` (damals Service-Menue/PIN-Seite am Geraet nicht aufrufbar), `HW-19-R02` `NOT_RUN`, `HW-19-R03` `BLOCKED`; der damalige Zugangsblocker ist in `main` behoben (PR #199), die Ergebnisse werden nicht rueckwirkend geaendert; auf dem aktuellen `main` (Geraet `f859ef6`, quellgleich, kein Flash) am 2026-10-10: N1–N8 und P2b per Ownerbeobachtung belegt, **`HW-19-R01` damals `FAIL`** (G2 erteilt und verbraucht; voller Hold -> Stack Overflow in task `main`, danach Bootloop, Geraet blieb bis G5 unveraendert; Offline-Diagnose 2026-10-10 in Evidence 3b: Backtraces gegen das verifizierte `f859ef6`-ELF dekodiert, nur Erkennungsstelle im Kontextwechsel; Hypothese gemeinsames Stack-Budget-Problem, `prepareAuthorizedEpochHandoff` + `makeAuthorizedEpochHandoffTarget` = 29 680 B Frames > 24 576 B Main-Stack; Callsite und erreichter Persistenzzustand `NOT_RESOLVED`; Fix in PR #200 nach Plan-Freigabe umgesetzt: Heap-Scratch im autorisierten Handoff, Frames make 17 520 -> 5 680, prepare 12 160 -> 608, finalize 8 512 -> 656 B, Ketten K-HOLD/K-BOOT-P/K-BOOT-C statisch 15 568 / 12 352 / 12 400 B direkt benannt (Reserve >= 8 048 B, beide Profile; Evidence 3c), Host-Tests inkl. Golden-Bytes und Allokationsfehler gruen; G5 erteilt und ausgefuehrt am 2026-10-10: App-only-Flash `b9564e6` (SHA-256 `cc0f0fff...b38308`, `0x10000`, ohne Erase), Geraet bootet stabil, `application: ready`, `stack_hwm_bytes=11800`, kein Panic/WDT/Reset in ca. 150 s; Display normal und Einstellungen zurueckgestellt laut Ownerbeobachtung `PASS_OWNER_OBSERVED`; **G5/P3b `PASS`** = P3b-Recovery-Nachweis, interner Handoff-Pfad `NOT_RESOLVED`, Evidence 3d), `HW-19-R02` `NOT_RUN`, `HW-19-R03` `BLOCKED`; Evidence `docs/audits/ISSUE192_FACTORY_RESET_HW_20261010_EVIDENCE.md`; Plan `docs/tasks/issue-192-factory-reset-hardware-verification-plan.md` Revision 3 (Scope: Hardwareevidence plus begrenzter Stack-Fix im autorisierten Run-Epochen-Handoff, Software-Nachweise, Recovery G5 und Retest P3c ausgefuehrt, weiterer Retest nur nach neuem Gate; freigegebener Plan-Commit: `43a65d3`; Software-Fix C1–C4 umgesetzt und reviewt; Skript-Gate-Aufnahme und UI-Command-Stackpfad FOLLOW-UP), Evidence `docs/audits/ISSUE192_FACTORY_RESET_HW_20261008_EVIDENCE.md` (historisch); `ACTUATOR_RELEASE=NO` | Fixscope/-ort vom Owner entschieden (bestehender Draft-PR #200, kein neues Issue/PR); G2-R3/P3c ausgefuehrt am 2026-10-11 (Evidence 3e): Reset funktional ok, aber Stackreserve 5 432 B < 6 144 B und Heap-Minimum 4 084 B, Ownerentscheid 2026-10-11: Stackreserve als begruendete Abweichung akzeptiert, Heap-Minimum Messbefund ohne Fix, `HW-19-R01` `PASS` mit akzeptierter Abweichung; offen: unabhaengiger Review; G5, G2 und G2-R3 verbraucht, G3 vor Powercut-Cutpoints; R03 `BLOCKED`, solange kein sicherer `SAFE_BOOT`-/NoRuntime-Einstieg benannt ist |
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
- **#172 (Touch-UI, `CLOSED`)**: Hardware-Ownerabnahme S7-S10 inkl. Labels/
  Tastengroessen und Namenspersistenz am 2026-10-08 `PASS_OWNER_OBSERVED`
  (`docs/audits/PR187_HW_OWNER_RETEST_20261008_EVIDENCE.md`); D10-Commit-
  Ressourcen S3/S6/S10 gemessen, `PASS`
  (`docs/audits/PR187_D10_COMMIT_RESOURCES_20261008_EVIDENCE.md`). Offen:
  Pixelvermessung der Tasten nicht erfolgt. Die subjektiv langsamere Reaktion
  trat nur im Probe-Build auf (Exakt-Image: Owner „sehr smooth“).
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
| #19 PR #202 (`b903de8`) | lokaler Werksreset Ablauf A (PIN-geschuetzt, Issue bleibt offen, Hardware `NOT_RUN`) |
| #27 PR #201 (`93116d2`) | Web-Laufbedienung: Run-Mutationsroute, Shell-Aktionen, Programmliste (Issue bleibt offen, siehe oben) |
| #27 PR #205 (`79eb9b9`), PR #206 (`45e7e63`) | kanonischer Mutationsheader; Web-Laufinformationen (Programmname, Phase, Soll, Abschluss, phasenbezogene Zeit), H-RAM-1 nur Teilnachweis (Issue bleibt offen, siehe oben) |
| #27 PR #207 (`43f0338`) | Web-Standbyinformationen: Geraetename, Netzwerkmodus/-zustand, Lokalzeit; Shell-Limit 11264 B (Issue bleibt offen, siehe oben) |
| #27 PR #208 (`086873a`) | Web-Sprachwahl DE/EN/ES je Browser; Shell-Limit 11456 B ausgeschoepft (Issue bleibt offen, siehe oben) |
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
