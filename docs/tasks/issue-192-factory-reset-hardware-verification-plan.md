# Issue #192 – Hardwareverifikation lokaler Werksreset und Handoff-Stack-Fix (Revision 3)

PR #200 (Draft) fuehrt #192 fort: Hardwareevidence **und** den durch die
Hardwareevidence nachgewiesenen, notwendigen Produktcode-Fix am autorisierten
Run-Epochen-Handoff (Abschnitt 4). `ACTUATOR_RELEASE=NO`; Aktoren bleiben
physisch getrennt/deaktiviert. PR #194 (Revision 1 auf Baseline `df5a3dd`)
wurde geschlossen und nicht gemergt. PR #193 (#172/D10) bleibt unberuehrt.
Die akzeptierte #188-B-Korrektur bleibt unveraendert; dafuer gibt es keine
neuen Tests.

Revision 3 ersetzt die Revisionen 1 und 2 vollstaendig und ist eigenstaendig
ausfuehrbar (kein paralleler Plan). Sie erhaelt den bisherigen Hardwareablauf
und die tatsaechlich erteilten/verbrauchten Gates und ergaenzt: den begrenzten
Software-Fix-Schnitt (Abschnitt 4), den Recovery-/Hardware-Retest mit neuen
Gates (Abschnitt 5) und die aktualisierten Gate-/Ownerentscheide (Abschnitt 6).

**Scope-Aenderung gegenueber Revision 2 (materiell):** Revision 2 war reine
Evidence/Dokumentation ohne Produktcodeaenderung. Revision 3 erweitert PR #200
um genau einen Produktcode-Fix in
`lib/fermentation_app/src/run_persistence_coordinator.cpp` (plus Tests und ein
Auswerteskript). Der Owner hat Fix und Weiterfuehrung in PR #200 (kein neuer PR,
kein neues Issue) angeordnet; der **Plan-Commit dieser Revision ist noch nicht
freigegeben**. Bis zur Freigabe der exakten Plan-SHA gilt: kein Produktcode,
kein Geraetezugriff.

**Ausfuehrungsstand (2026-10-10):** G1 erteilt und verbraucht (Flash selbst war
nicht noetig: das Geraet lief bereits mit `f859ef6`). G2 wurde fuer P2b + P3
erteilt und ist mit dem einen Vollreset-Versuch **verbraucht**. N1–N8 und P2b
ausgefuehrt (PASS); P3 (voller Hold) = **FAIL**: Stack Overflow in task `main`
beim Hold und reproduzierbarer Bootloop (92 `stack overflow`-Meldungen in der
Aufnahme). Das Geraet ist unveraendert im fail-closed Bootloop belassen; seither
kein Geraetezugriff. **G3 und G4 sind nicht erteilt**; HW-19-R01 = `FAIL`,
HW-19-R02 = `NOT_RUN`, HW-19-R03 = `BLOCKED`. Die Offline-Diagnose
(ELF-Hashes gegen HW-188-A01 verifiziert, Backtraces dekodiert, statische
Stackpfadanalyse) steht in
`docs/audits/ISSUE192_FACTORY_RESET_HW_20261010_EVIDENCE.md`, Abschnitt 3b:
belegt ist der Stack-Overflow und, dass `prepareAuthorizedEpochHandoff` plus
`makeAuthorizedEpochHandoffTarget` allein 29 680 B Frames benoetigen
(Main-Task 24 576 B). Ein gemeinsames Stack-Budget-Problem ist **Hypothese**,
kein Beweis; Callsite des Ueberlaufs und erreichter Persistenzzustand sind
`NOT_RESOLVED`. Abschnitt 1 beschreibt den Stand **vor** der Hardwareausfuehrung.

## 1. Ausgangslage (geprueft am 2026-10-10, vor der Hardwareausfuehrung)

```text
BASELINE=21082de766a0b7b52108448c85ae51ba8d84b3f1 (main, Merge von PR #199; enthaelt PR #191 inkl. S1 ResetEligibleNoRuntime und PR #199 Service-PIN-Einstieg)
PR_200_HEAD_BEI_REVISION_3=e5a3a38c5795c321ebd744fcf1ffbfc234da843a (Offline-Diagnose 3b; Produktquellen identisch zu main 21082de)
QUELLUNTERSCHIED_ZU_f859ef6=nur docs/ (4393d27 docs: HW-188-A01 Evidence + Merge-Commit); Produktquellen identisch zum in HW-188-A01 geflashten Stand f859ef6
GERAET_AKTUELL=f859ef6 im fail-closed Bootloop (Evidence 20261010, 3a/3b); kein Zugriff seit dem P3-Versuch
ISSUE_STAND(vor Hardwareausfuehrung)=#192 OPEN / BLOCKED_HARDWARE; HW-19-R01..R03 auf dem aktuellen main NOT_RUN  [heute: R01=FAIL, R02=NOT_RUN, R03=BLOCKED]
```

Build-Provenienz der Baseline (sauberer Build, `--require-clean-source-tree`,
Profil `release`, ESP-IDF v6.1 `fff9895c82d744c7237be8847347bdd1b07c6643`):

```text
SOURCE_HEAD=21082de766a0b7b52108448c85ae51ba8d84b3f1 (detached Worktree, git status sauber, SOURCE_TREE_CLEAN=YES)
BUILD=python3 scripts/build_esp_idf_profiles.py release --require-clean-source-tree  -> PASS (Profil esp32_release)
APP_VERSION_STRING=21082de (im Image enthalten)
APP_BIN_SHA256=9416e64013d37503257f1818614889767eae39cb9f51668f0f910d8ac5599ec7  (1 740 064 Byte)
APP_ELF_SHA256=d3196493f3b8f734a0d7c1fd24e6fbbaef0b34fc7e8c971921ad6823c7e2af10
PARTITION_TABLE_SHA256=d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d  (identisch zu df5a3dd und f859ef6 -> nur App flashbar, kein Erase)
BOOTLOADER_SHA256=d70a1e164a87b2950cb569fba92ab123fab45f770b8814010f2926968416902f  (weicht vom in HW-188-A01 genannten Wert 2c1b4979... ab; Ursache nicht untersucht; Bootloader wird nicht geflasht)
FLASH_ARGS=0x1000 bootloader, 0x8000 Partitionstabelle, 0x10000 App; geplant ist ausschliesslich 0x10000
GEFLASHT_AUF_DEM_GERAET=f859ef6 (ELF-SHA256 93535f2b...260b, == HW-188-A01); die Baseline-Hashes oben sind der main-Build und nicht das geflashte Image
```

Bisherige Hashes/Images von `df5a3dd` (App `06e08f0c...`) gelten **nicht** fuer
den aktuellen `main` und duerfen nicht fuer ihn herangezogen werden.

Hardwarebezogene Befunde aus dem Quellstand (nicht am Geraet nachgewiesen,
ausser wo die Evidence ausdruecklich belegt):

- Das Produkt verdrahtet den Ablauf `Warning -> Confirm -> Hold`
  (`FactoryResetStage`): Hold 5000 ms (`kApprovedFactoryResetHoldMillis`),
  Release-Pfad der Hauptschleife, Press-Dispatcher.
- **Zugang 1 "PIN vergessen?"** ist mit PR #199 regulaer erreichbar:
  `Einstellungen -> Service (PIN) -> PIN-Seite -> "PIN vergessen?"` (Slot 3,
  auch waehrend einer PIN-Sperre). Am Geraet belegt (N1–N8 am 2026-10-10).
  Die historischen Ergebnisse auf `df5a3dd` (R01 `BLOCKED`, R03 `BLOCKED`)
  werden nicht rueckwirkend geaendert.
- **Zugang 2 `SAFE_BOOT`-Eintrag und `ResetEligibleNoRuntime`:** erscheint nur in
  `SafeBoot` bzw. wenn der Recoverykern `ResetEligibleNoRuntime` zugelassen hat.
  Beides setzt einen defekten/unaufloesbaren Datenzustand voraus. Ohne rohe
  NVS-/Flash-Manipulation oder kuenstliche Korruption (ausgeschlossen) ist kein
  sicherer Eintrittspfad bekannt -> R03 bleibt `BLOCKED`, bis der Owner einen
  sicheren Pfad benennt (G4).
- **Persistenzrisiko beim Firmwarewechsel:** Ein Flash eines anderen Images auf
  ein Geraet mit aelterem Image kann Schema-Migrationen ausloesen; ein
  Zurueckflashen ist fuer diese Daten nicht gesichert. Fuer den Recovery-Flash
  (Abschnitt 5.1) relevant: der Fix aendert **kein** Schema und kein Wireformat
  (Abschnitt 4.3); die Produktquellen ausser dem Handoff-Pfad bleiben `f859ef6`.

## 2. Ablauf

Nichtdestruktive Schritte sind von destruktiven Schritten, Netzwerkstopp/
Powercut und `SAFE_BOOT`-/NoRuntime-Sonderfaellen getrennt.

| Phase | Inhalt | Destruktiv | Gate | Stand |
|---|---|---|---|---|
| P0 | Baseline `21082de`, sauberer Build, Hashes (Abschnitt 1) | nein | – | erledigt |
| P1 | Geraetestand per UART-Boot-Log; Flash nur falls Stand abweicht | nein | G1 | erledigt (kein Flash noetig) |
| P2 / R01-N | nichtdestruktive R01-Schritte N1–N8 (2.1) | nein | – | erledigt, PASS (Ownerbeobachtung) |
| P2b / R01-H | Hold-Fortschritt/-Abbruch vor 5000 ms | nein bei korrektem Loslassen | G2 | erledigt, PASS (Ownerbeobachtung) |
| P3 / R01 destruktiv | erster vollstaendiger 5000-ms-Hold | ja | G2 | **FAIL** (Stack Overflow, Bootloop); G2 verbraucht |
| **P3a** | **Software-Fix und Software-Nachweise (Abschnitt 4)** | nein (kein Geraet) | Freigabe der exakten Plan-SHA dieser Revision | offen |
| **P3b** | **Recovery: korrigierte App-only-Firmware auf das Bootloop-Geraet, Startbeobachtung (5.1)** | Flash (App), kein Erase | **G5 (neu)** | gesperrt bis Software-Review PASS |
| **P3c** | **Wiederholung des vollen 5000-ms-Hold (5.2)** | ja | **G2-R3 (neu)** | gesperrt bis P3b PASS |
| P3d / R02 | AP-/Client-Trennung, `esp_wifi_stop()`, `httpd_stop()` beim Reset; nur falls G2-R3 es einschliesst | ja | G2-R3 | gesperrt |
| P3e / R02 Powercut | Stromunterbrechung waehrend der Reset-Schreibschritte | ja | **G3** | nicht erteilt, `NOT_RUN` |
| P4 / R03 | `SAFE_BOOT`/`ResetEligibleNoRuntime` | nein / ja | **G4**, bei Ausfuehrung zusaetzlich G2 | nicht erteilt, `BLOCKED` |
| P5 | Evidence-Bericht, Roadmap/Issue #192 nur anhand tatsaechlich durchgefuehrter Tests | nein | – | offen |

Stromunterbrechung waehrend der Reset-Schreibschritte (R02-Cutpoints) wird
**nicht** durchgefuehrt, solange der Owner kein reproduzierbares Verfahren
freigegeben hat (G3). Ohne G3 bleibt dieser Teil `NOT_RUN`.

### 2.1 Nichtdestruktive R01-Schritte (ausgefuehrt, Ownerbeobachtung)

Alle Schritte liefen mit Aktoren physisch getrennt, ohne Druck auf das Halteziel
und endeten im `Abbrechen`-Pfad. Sie bleiben die Referenz fuer den Retest
(Abschnitt 5.2), werden aber nicht wiederholt.

| # | Schritt am Geraet | Erwartung / Beleg |
|---|---|---|
| N1 | Startseite -> `Einstellungen` -> `Service (PIN)` antippen | PIN-Seite oeffnet sich (Zeile nicht gesperrt) |
| N2 | PIN-Seite: "PIN vergessen?" sichtbar und antippbar | Wechsel in `Warning` |
| N3 | Warnung: Datenverlusttext vollstaendig lesbar, DE/EN/ES | keine abgeschnittenen Texte |
| N4 | Warnung -> `Abbrechen` | zurueck zur PIN-Seite, Zustand unveraendert |
| N5 | Warnung -> `Confirm`; Text pruefen; `Abbrechen` | wie N3/N4 |
| N6 | `Confirm` -> `Hold`; nur Anzeige, Halteziel nicht beruehrt; `Abbrechen` | Fortschritt 0, Texte DE/EN/ES |
| N7 | N2 waehrend aktiver PIN-Sperre wiederholen | "PIN vergessen?" bleibt erreichbar |
| N8 | Vorher/Nachher-Vergleich der Daten; UART ohne Reset-Header/Panic | Daten unveraendert |

## 3. Messung und Abbruch

- UART-Aufnahme mit Zeitstempeln durchgehend, **eine** Aufnahme (Port nicht
  mitten im Test neu oeffnen; ein Oeffnen kann das Board zuruecksetzen); Port
  bei `--before default-reset` nur zu Beginn; Marker ueber Datei. Client-/HTTP-
  Belege fuer R02 vom Host aus.
- Abbruch ohne Fix bei: Crash, WDT, OOM, nicht bestaetigtem Netzwerk-/HTTP-Stopp,
  unerwartetem Speicherzustand, nicht erreichbarem Einstieg. Beleg sichern,
  Befund dem Owner vorlegen, keine stille Korrektur.
- Rohlogs lokal; im Repository nur Hashes und bereinigte Werte (keine SSID, MAC,
  Passwoerter, keine Webpasswoerter im Chat).
- Ergebnis je HW-19-R01/R02/R03 getrennt `PASS`/`FAIL`/`NOT_RUN`/`BLOCKED` mit
  Ausgangszustand, exakter Firmware-/Build-SHA, Interaktion, UART-/Client-Belegen,
  Resetursache, Einschraenkungen. Ergebnisse frueherer Staende bleiben als
  historische Evidence erhalten und werden nicht uebernommen.
- Neu fuer P3b/P3c: `stack_hwm_bytes` und Heap (frei / groesster Block /
  Minimum) werden an den Messpunkten der Firmware-Diagnosezeilen
  (`after_platform_begin`, `after_application_begin`, `idle`) mitgelesen; fuer
  die Hold-Aufzeichnung gilt zusaetzlich 5.2.

## 4. Software-Fix: Stackverbrauch im autorisierten Run-Epochen-Handoff (P3a)

### 4.1 Ziel und Nicht-Ziele

**Ziel:** Der Stackbedarf der Reset-Hold-Kette und der Boot-Resume-Kette
(Handoff-Phase `Pending` und `Committed`) liegt am Release-ELF belegt unter
`24 576 B` abzueglich einer benannten Reserve (4.5), ohne dass sich Vertraege,
Persistenzformat oder Fail-Closed-Verhalten aendern.

**Nicht-Ziele:**

- Keine Erhoehung von `CONFIG_ESP_MAIN_TASK_STACK_SIZE`, kein Worker-Task
  (binden DRAM und verschieben das Problem; kein begruendeter Befund dafuer).
- Keine neue oder parallele Recoverylogik; keine Aenderung von State-Store-API,
  Epoch-Vertrag, `Pending`/`Committed`-Phasen, Idempotenz, Slot-/Head-Wireformat
  oder Reason-/Step-Vokabular (`RunPersistenceTechnicalReason`,
  `RunPersistenceStep`).
- Keine Aenderung des UI-Command-Stackpfads (`applyConfirmedPrepared` ->
  `decidePrepared` -> Visit-Lambda, ca. 41,6 kB direkte Frames, `UNVERIFIED_STATIC`,
  Evidence 3b «Nebenbefund»). Das ist ein nachvollziehbares **FOLLOW-UP**
  (Abschnitt 7): der Pfad liegt weder in der Hold-Kette noch in der
  Boot-Resume-Kette und blockiert diesen Fix nicht.
- Kein neues Stack-Gate in `run_pre_ready_gates.sh` bzw. CI (4.6).
- Keine Aenderung der bereits akzeptierten #188-B-Korrektur.

### 4.2 Belegter Ausgangsbefund (Evidence 3b, ELF `f859ef6`)

```text
makeAuthorizedEpochHandoffTarget  Frame 17520 B   (RunCommandState + RunPersistenceSnapshot 3904 B
                                                    + Target mit 2 x RunPersistenceRawRecord 3952 B)
prepareAuthorizedEpochHandoff     Frame 12160 B   (std::optional<Target> ca. 7,9 kB + RawRecord record 3952 B;
                                                    Lambda validateReferenced mit eigenem RawRecord)
finalizeAuthorizedEpochHandoff    Frame  8512 B   (make inline)
Reset-Hold  (Basis bis beginAuthorizedFactoryReset 9 248 B) + prepare + make = 38 928 B
Boot-Resume (Basis bis completeAuthorizedEpochHandoff 6 064 B) + prepare + make = 35 744 B
Boot-Resume Phase Committed (nur finalize) ca. 14,6 kB (unter Budget)
```

Die Basis der Hold-Kette bis `prepare` betraegt 9 248 B
(4928 + 816 + 2576 + 112 + 816), d. h. fuer `prepare` und alle Callees bleiben
15 328 B vor Reserve. Die Frame-Zahlen sind statische Summen aus dem ELF
(Hypothese zur Ursache des Ueberlaufs, nicht dessen Beweis; Callsite
`NOT_RESOLVED`).

Treiber der Groesse ist die **mehrfache Stack-Haltung desselben ca. 4-kB-Objekts**
(`RunPersistenceSnapshot`/`RunPersistenceRawRecord`): im Target (zweimal), als
Scratch in `make`, und in `prepare` (zweimal, davon einmal im Lambda). Die
tatsaechlich benoetigte Nutzlast ist klein: der leere Handoff-Snapshot kodiert
auf **63 B** Nutzlast, die beiden Slot-Records auf je **100 B**, der Head auf
**78 B** (Host-Messung mit dem unveraenderten Produktcode; der Byteinhalt der
Strings ist architekturunabhaengig).

### 4.3 Technisches Minimaldelta (Entwurf, am Quellcode `21082de` geprueft)

Nur `lib/fermentation_app/src/run_persistence_coordinator.cpp`; keine Aenderung
an Headern, Codec, State-Store oder Aufrufern.

1. **Schlankes Target.** `AuthorizedEpochHandoffTarget` haelt nur noch
   `std::array<std::string, 2U> slotBytes` und `std::string headBytes`. Die
   beiden `RunPersistenceRawRecord` (inkl. Snapshot-Kopie) entfallen: `prepare`
   und `finalize` verwenden aus dem Target ausschliesslich
   `records[slot].bytes` und `headBytes` (in C2 per Suche nach allen Zugriffen
   zu verifizieren; die Zuweisungen `snapshot`, `checkpointRevision`,
   `utcUnixSeconds` dienen nur der Referenzbildung des Heads in `make`).
2. **Ein Scratch statt vieler Grossobjekte.** `prepare` und `finalize` legen
   **einen** `std::unique_ptr<RunPersistenceRawRecord>` per `new (std::nothrow)`
   an (bestehendes Projektmuster, z. B. `RunPersistenceLoadResult` und
   `RunCommandState` in `fermentation_application.cpp`). `make...` erhaelt
   diesen Scratch als `RunPersistenceRawRecord&`: der leere Snapshot wird direkt
   in `scratch.snapshot` gebaut (kein zweiter `RunPersistenceSnapshot`),
   `scratch.bytes`/`checkpointRevision` werden fuer
   `makeRunCheckpointReference(0U, scratch, epoch)` gesetzt. In `prepare` dient
   derselbe Scratch danach der Validierung (`decodeRunPersistenceRecordInto`
   in der Slot-Schleife und im Lambda `validateReferenced`, sequentiell, nie
   gleichzeitig); die lokalen `RunPersistenceRawRecord record` entfallen. Das
   Ergebnis von `...Into` wird jeweils nur nach Erfolg gelesen, ein
   veralteter Scratchinhalt ist daher unerheblich (im Review zu bestaetigen).
3. **`RunCommandState emptyState` bleibt Stack-lokal** (ca. 5 kB, Lebensdauer
   endet nach dem Bauen des Snapshots). Das haelt den Heap-Spitzenbedarf klein
   (4.4); der Heap-Fallback ist vorab festgelegt (D2 in 4.4).
4. **Unveraenderte Vertraege.** Byteinhalt von Slot-Records und Head bleibt
   **identisch**. Das ist hart: ein im Bootloop-Geraet bereits geschriebener
   Zustand (Slots der alten Firmware, ggf. `Pending`) wird vom neuen Code nur
   dann als `Target` erkannt (`slotReads[slot].value == slotBytes[slot]`), wenn
   die Bytes gleich sind; andernfalls wuerde der Recovery-Flash (5.1) in den
   `Previous`/`InvalidProjection`-Zweig laufen. Nachweis: Charakterisierungstest
   mit festen Byte-Erwartungswerten (C1), der **vor** der Aenderung gegen den
   unveraenderten Code gruen ist und danach unveraendert gruen bleibt.
5. **Allokationsfehler fail-closed.** Liefert `new (std::nothrow)` `nullptr`,
   endet der Aufruf mit `reject(RunPersistenceStep::CandidateApply,
   RunPersistenceTechnicalReason::InvalidProjection)` – derselbe Step/Reason
   wie heute ein fehlgeschlagenes `make` (ein eigener OOM-Reason waere eine
   Vokabularaenderung und ist ausgeschlossen). Der Scratch wird an der heutigen
   Position der `make`-Aufrufstelle angelegt (in `prepare` nach den Head-/Slot-
   Reads und der Epochenpruefung, in `finalize` an der heutigen Stelle nach der
   Proof-Pruefung), damit vorgelagerte Reject-Gruende unveraendert bleiben. Bis
   dahin hat keine Schreiboperation stattgefunden: `durability` bleibt
   `Unchanged`, kein `enterBlockedIndeterminate`, kein Teilzustand, keine
   Freigabe. Der Aufrufer behandelt `Blocked` wie jeden bisherigen
   Blocked-Handoff (Reset nicht ausgefuehrt bzw. Boot-Resume blockiert); dafuer
   gibt es keine neue Logik.

### 4.4 Speicherbudget (Heap)

Der Ansatz ersetzt Stack durch kurzlebigen Heap und wird deshalb gegen die
vorhandene RAM-Evidence bewertet, nicht als gegeben angenommen:

```text
Neuer transienter Heap je Aufruf (prepare bzw. finalize), sequentiell, keine gleichzeitigen Grossobjekte:
  Scratch RunPersistenceRawRecord          3 952 B  (ein Block; groesster gleichzeitig benoetigter Block)
  Target-Strings (100 + 100 + 78 B) + payload (63 B)   < 0,5 kB (Allokatoroverhead zusaetzlich; Strings existieren heute schon)
  Spitze                                    ca. 4,4 kB, davon groesster Block 3 952 B
Entfaellt gegenueber heute: ca. 24 kB Stackframes in den Ketten (Abschnitt 4.2).

Vergleichswerte (belegt):
  Betrieb N1–N8 (Netzwerkmodus UNSELECTED): frei 43 184 B, groesster Block 40 960 B, Minimum 34 532 B / 39 324 B (Rohlog, Evidence 3b)
  R1-RAM-Evidenz Netzwerkmodi (docs/audits/R1_RAM_S8_EVALUATION.md): niedrigster gesampelter freier Heap 12 288 B,
    niedrigster gesampelter groesster Block 7 168 B, Low-Water-Mark 4 124 B
Bestehende Praezedenz: RunPersistenceLoadResult und RunCommandState (ca. 5 kB) liegen im Bootpfad bereits per nothrow-Heap.
```

Bewertung: Im gemessenen Betrieb ist die Spitze (4,4 kB, Block 3,95 kB) um mehr
als eine Groessenordnung kleiner als freier Heap und groesster Block. Gegenueber
dem Netzwerk-Worst-Case der R1-Evidenz (groesster Block 7 168 B) bleiben
ca. 3,2 kB Reserve im groessten Block. Der Low-Water-Wert 4 124 B ist **nicht**
fuer den Reset-Zeitpunkt belegt; ob der Reset-Handoff in einem solchen Fenster
laeuft, ist unverifiziert. Die Reihenfolge Netzwerkstopp/Handoff in
`beginAuthorizedFactoryReset` und die Heap-Lage zum Hold-Zeitpunkt sind in C3 zu
pruefen und im Retest (5.2) zu messen. Ein Heap-Fehlschlag ist kein stiller
Ersatz-Absturz, sondern ein sicherer `Blocked`-Abbruch (4.3 Punkt 5) und per
Test belegt (4.6).

**Vorab festgelegter Fallback D2 (nur wenn das Stackgate 4.5 mit dem Entwurf
verfehlt wird):** `RunCommandState emptyState` zusaetzlich in den Heap-Scratch
(ein zweiter Block ca. 5 kB; Spitze dann ca. 9,4 kB, groesster Block ca. 5 kB,
noch unter dem R1-Worst-Case-Block von 7 168 B). D2 wird nur umgesetzt, wenn
Stackgate und diese Heap-Grenzen danach erfuellt sind; verfehlen beide Varianten
die Gates, wird angehalten und dem Owner vorgelegt (materielle Abweichung, kein
stiller Ausweg).

### 4.5 Stack-Zielwerte und Bewertungsregel

Zielwerte sind **Warn- und Review-Werte** im Sinn von
`ENGINEERING_PRINCIPLES.md` (Ressourcenbudgets aus Messung), kein neues hartes
Produktlimit; Unterschreitung wird dem Owner vorgelegt statt still akzeptiert.

```text
STACK_BUDGET_MAIN=24 576 B (CONFIG_ESP_MAIN_TASK_STACK_SIZE)
RESERVE_ZIEL_STATISCH=6 144 B  (Begruendung: niedrigster im Betrieb gemessener Main-HWM 4 312 B in N1–N8 bzw. 5 936 B in der
                                R1-Serie; Ziel liegt darueber, auf 6 KiB gerundet; die Tiefe der ESP-IDF-NVS-/Flash-Callees
                                ist statisch nicht erfasst und wird durch die HWM-Messung am Geraet gegengeprueft)
KETTENGRENZE=24 576 - 6 144 = 18 432 B je Kette (Summe der Frames direkt geschachtelter Funktionen inkl. des tiefsten Callee-Pfads)
```

Zu bewertende Ketten (jeweils fuer `esp32_release` und `esp32_bringup`; die
Frames oberhalb des Handoffs entsprechen der Aufstellung in Evidence 3b):

```text
K-HOLD    app_main -> ... -> beginAuthorizedFactoryReset -> prepareAuthorizedEpochHandoff -> tiefster Callee-Pfad
K-BOOT-P  app_main -> ... -> completeAuthorizedEpochHandoff -> prepareAuthorizedEpochHandoff -> tiefster Callee-Pfad  (Phase Pending)
K-BOOT-C  app_main -> ... -> completeAuthorizedEpochHandoff -> finalizeAuthorizedEpochHandoff -> tiefster Callee-Pfad (Phase Committed)
```

Erwartung (Schaetzung vor Messung, **nicht** Nachweis): `prepare` < 2 kB,
`make` ca. 6 kB (`RunCommandState`), `finalize` ca. 6 kB; K-HOLD ca. 17 kB,
Reserve ca. 7,5 kB. Verbindlich ist ausschliesslich die Messung am ELF (4.6).

Virtuelle Aufrufe (State-Store-Port, `callx8`) und Interrupt-Frames fehlen in
der direkten Kantenanalyse. Das Skript (4.6) benennt diese Kanten in der Ausgabe
**explizit** (unaufgeloeste Kanten zaehlen und auflisten); die Kettensumme bleibt
eine Untergrenze und wird durch die HWM-Messung am Geraet (5.1/5.2)
gegengeprueft.

### 4.6 Nachweise (Akzeptanz der Softwarestufe)

| Nachweis | Befehl / Methode | Erwartung |
|---|---|---|
| Wire-Identitaet | neuer Test `test_issue192_handoff_target_bytes_are_stable` in `test/test_run_persistence_coordinator`: Epoche 61, `RunCheckpointSchedule{}`, feste Byte-Literale fuer Slot 0, Slot 1 und Head (aus dem **unveraenderten** Code in C1 gewonnen) | gruen vor und nach C2, Bytes identisch |
| Bestehende Handoff-/Recovery-Tests | `pio test -e native --filter test_run_persistence_coordinator` (Pending/Committed, Orphan, Teilzustaende/Write-Cutpoints, Idempotenz, ForeignEpoch, failed handoff) | unveraendert gruen, keine Anpassung bestehender Erwartungen |
| Direkte Konsumenten | `pio test -e native --filter` fuer `test_factory_reset_flow`, `test_issue144_run_identity`, `test_issue90_product_recovery_oracle`, `test_configuration_recovery_service` | gruen |
| Allokationsfehler | fokussierter Test im selben Verzeichnis: suite-lokales Ersatz-`operator new(size_t, const std::nothrow_t&)` mit armierbarem Fehler (nur fuer Allokationen >= 3 000 B), fuer `prepare` und `finalize`: Ergebnis `Blocked`/`CandidateApply`/`InvalidProjection`, `durability == Unchanged`, Store byteweise unveraendert, kein `Applied`; nach Freigabe des Fehlers gelingt derselbe Aufruf (Idempotenz) | gruen; keine weitere Test-Infrastruktur |
| Statisches Format/Architektur | `clang-format --dry-run --Werror` auf geaenderte Dateien, `python3 scripts/check_architecture_boundaries.py`, `git diff --check` | PASS |
| Beide ESP-IDF-Profile | `python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree` (Profile `esp32_bringup` und `esp32_release`, inkl. Profilvalidierung) | PASS beider Profile |
| **Release-ELF-Frame-/Call-Chain-Pruefung** | neues, build-spezifisches Skript `scripts/analyze_issue_192_handoff_stack.py` (Muster `scripts/analyze_issue_29_stack.py`: `entry a1, N` aus `objdump -d`, direkte `call*`-Kanten, kein allgemeines Framework): Kettensumme K-HOLD, K-BOOT-P, K-BOOT-C je Profil mit Einzelframes, tiefstem Callee-Pfad und Liste der unaufgeloesten (`callx`) Kanten; Kalibrierung: auf dem `f859ef6`-ELF muss es die belegten Werte 17520/12160/8512 und 38 928/35 744 B reproduzieren | alle drei Ketten je Profil <= 18 432 B; Kalibrierung reproduziert; Ergebnis mit Reserve in Evidence 3c |
| Heap-Spitze | aus 4.4 (Scratch 3 952 B) abgeleitet, Block-/Summenangabe in Evidence 3c; Messung am Geraet erst im Retest (5.2) | benannt, Grenzen 4.4 eingehalten |

Host-Tests beweisen den Stack **nicht**; sie belegen Verhalten, Wire-Identitaet
und Fail-Closed. Das Skript wird **nicht** in `run_pre_ready_gates.sh`/CI
verdrahtet (das waere eine Gate-/CI-Vertragsaenderung ausserhalb dieses
Scopes); eine Gate-Aufnahme ist FOLLOW-UP und Ownerentscheid.

### 4.7 Umsetzungs- und Commit-Schnitte (nach Planfreigabe; nach jedem Commit anhalten)

| Commit | Inhalt | Lokaler Nachweis |
|---|---|---|
| C1 | Charakterisierungstest `test_issue192_handoff_target_bytes_are_stable` (feste Bytes aus dem **unveraenderten** Code); kein Produktcode | gruen auf unveraendertem Code |
| C2 | Fix 4.3 in `run_persistence_coordinator.cpp` plus Allokationsfehlertest | `test_run_persistence_coordinator` komplett, direkte Konsumenten, Format |
| C3 | `scripts/analyze_issue_192_handoff_stack.py`, beide Profil-Builds, Evidence-Abschnitt 3c (Frame-Tabelle vorher/nachher, Kettensummen, Reserve, Heap-Spitze, Reihenfolge Netzwerkstopp/Handoff in `beginAuthorizedFactoryReset`) | Skript-Kalibrierung und Gate 4.5, Profil-PASS |
| C4 | Doku-/Statusabgleich: ROADMAP, ACCEPTANCE_TESTS (Retest-Voraussetzungen R01), OPEN_POINTS, Handover | `git diff --check`, Markdown-Konsistenz |

Danach: Builder-Self-Check, Anhalten fuer den Independent Review (Workflow);
**kein** Geraetezugriff in P3a. Der lokale Pre-Ready-Lauf folgt nur nach
Independent Review mit `OPEN_BLOCKERS=0` und ausdruecklicher Owner-Anweisung
(`AGENTS.md`). Weicht die Umsetzung materiell vom Entwurf 4.3 ab (z. B. Fallback
D2 ist noetig und nicht ausreichend, ein Reject-Grund muss sich aendern, ein
Wireformat-Byte aendert sich), wird angehalten und diese Revision erneut
vorgelegt.

## 5. Recovery und Hardware-Retest (erst nach Software-Review und Ownerfreigabe)

Alle Schritte dieses Abschnitts sind **nicht freigegeben** und beginnen nicht
vor: (a) Freigabe dieser Planrevision, (b) abgeschlossenem Software-Nachweis 4.6
und Independent Review der Softwarestufe, (c) ausdruecklicher Ownerfreigabe des
jeweiligen Gates.

### 5.1 P3b – Recovery per App-only-Flash (Gate G5)

- Bevorzugt: die korrigierte App-Firmware (`esp32_release`, sauberer Build mit
  `--require-clean-source-tree`, exakte SHA/Hashes in der G5-Vorlage) wird
  ausschliesslich bei `0x10000` auf das im Bootloop befindliche Geraet
  geflasht. **Kein Erase, kein NVS-/State-Erase, kein Bootloader-/
  Partitionstabellenflash.** Damit bleiben Konfiguration, Touchkalibrierung
  (`tc0`/`tc1`) und der persistierte Epochen-/Handoff-Zustand erhalten.
- Erwartung (Hypothese, nicht Beweis): Ist beim P3-Versuch ein offener
  `Pending`-Handoff persistiert worden, nimmt `application.begin()` ihn nun per
  `completeAuthorizedEpochHandoff` auf (Wire-Identitaet 4.3 Punkt 4 ist hierfuer
  Voraussetzung) und das Geraet startet; ist stattdessen ein anderer Zustand
  erreicht, wird das **beobachtet und dokumentiert**, nicht geraten.
- Dokumentiert wird: Boot-Version/SHA, UART-Bootfolge ohne `stack overflow`,
  `stack_hwm_bytes` und Heap an den Messpunkten, erreichte Anzeige
  (Ergebnisseite/Ersteinrichtung/Startseite), Aktoren AUS. Abbruch bei
  erneutem Crash/Bootloop: nicht erneut flashen, Befund und Optionen dem Owner
  vorlegen (Erase/Neueinrichtung nur nach separater Freigabe mit
  Datenverlustliste).
- Ein erfolgreicher Start belegt nur den Recovery-/Boot-Resume-Pfad, **nicht**
  HW-19-R01; R01 bleibt bis P3c `FAIL`.

### 5.2 P3c – Wiederholung des vollstaendigen 5000-ms-Reset (Gate G2-R3)

- Neue, **gesonderte** Ownerfreigabe G2-R3 vor dem ersten Druck auf das
  Halteziel, mit konkreter Liste der durch den Werksreset verlorenen Daten
  (Programme, Grenzen, Konfiguration, Netzwerk-/Zugangsdaten,
  Authentifizierungszustand der Epoche; Touchkalibrierung bleibt) und
  Wiederherstellbarkeit (nur Neueinrichtung). Das verbrauchte G2 gilt nicht.
- Ablauf: Weg wie P2b/P3 (N1–N6, Hold), UART-Aufnahme durchgehend. Zusaetzlich
  belegt werden: `stack_hwm_bytes` und Heap (frei/groesster Block/Minimum)
  unmittelbar vor dem Hold und nach dem Reset/Neustart; Ergebnisseite;
  Neustart ohne `stack overflow`; Ersteinrichtung; Erhalt der
  Touchkalibrierung; Aktoren AUS. Fehlt eine Hold-Start-Logzeile (siehe
  Evidence 3b), wird das ausdruecklich als Einschraenkung gefuehrt.
- Erfolgskriterium fuer die Stackreserve am Geraet: gemessener HWM nach dem
  Reset-Handoff und nach dem Folgeboot >= 6 144 B (4.5); darunter wird das
  Ergebnis dem Owner vorgelegt statt als PASS gefuehrt.
- Nach P3c: HW-19-R01 nur anhand des real belegten Umfangs `PASS`/`FAIL`;
  R02 nur fuer tatsaechlich beobachtete Teile; R03 bleibt `BLOCKED`.

## 6. Konkrete Ownerentscheide und Gates (Stand 2026-10-10)

```text
G1     erteilt, verbraucht
G2     erteilt (P2b + P3), verbraucht
G3     NICHT erteilt (Powercut-Cutpoints) – kein Powercut
G4     NICHT erteilt (SAFE_BOOT/NoRuntime) – R03 BLOCKED
G5     NEU, NICHT erteilt: App-only-Recovery-Flash (5.1), erst nach Planfreigabe + Software-Review PASS
G2-R3  NEU, NICHT erteilt: erneuter Vollreset (5.2), erst nach G5/P3b PASS und gesonderter Datenverlustliste
PLAN_REVISION_3_FREIGABE=OFFEN (exakte Plan-SHA wird im Draft-PR #200 ausgewiesen; danach Anhalten)
ACTUATOR_RELEASE=NO
```

- **Planfreigabe (diese Revision):** Umsetzung nach Abschnitt 4 nur nach Freigabe
  der exakten Plan-SHA. Mit der Freigabe ist der Software-Fix (P3a) in PR #200
  erlaubt – **nicht** G5, G2-R3, G3 oder G4.
- **G1 (verbraucht):** Flash nur der App ohne Erase, falls der Geraetestand vom
  Build abwich; Kenntnisnahme Schema-Migration.
- **G2 (verbraucht):** Bestaetigung der Datenliste (Programme, Grenzen,
  Konfiguration, Netzwerk-/Zugangsdaten, Authentifizierungszustand der Epoche
  werden auf Werkszustand zurueckgesetzt; Touchkalibrierung `tc0`/`tc1` bleibt;
  danach Ersteinrichtung).
- **G3:** Powercut-Verfahren (optional; sonst `NOT_RUN`).
- **G4:** R03: Umgang mit der fehlenden sicheren `SAFE_BOOT`-Vorbedingung
  (`BLOCKED` belassen / Owner nennt vorhandenen sicheren Pfad).
- **G5:** Flash der korrigierten App ohne Erase auf das Bootloop-Geraet (5.1);
  Kenntnisnahme: unvollstaendig beobachteter Persistenzzustand, moegliche
  erneute Bootloop-Beobachtung, Rueckweg nur ueber weitere Freigabe.
- **G2-R3:** wie die G2-Definition, neu zu erteilen (5.2).

Dieser Plan erteilt **keine** Flash-, NVS-Erase-, Werksreset-, 5000-ms-Hold-,
Powercut- oder `SAFE_BOOT`-Manipulationsfreigabe.

## 7. Offene Punkte, Risiken, FOLLOW-UP

| Punkt | Einordnung |
|---|---|
| Callsite des Ueberlaufs und erreichter Persistenzzustand beim P3-Versuch | `NOT_RESOLVED` (Evidence 3b); die Recovery (5.1) ist Beobachtung, kein Beweis der Hypothese |
| Hypothese «gemeinsames Stack-Budget-Problem» | durch den Fix nur gestuetzt, wenn P3b/P3c ohne Stack-Overflow laufen; sonst Befund an Owner |
| UI-Command-Stackpfad (`applyConfirmedPrepared`/Visit-Lambda, ca. 41,6 kB direkte Frames, `UNVERIFIED_STATIC`) | **FOLLOW-UP**, eigener Scope; nicht in der Hold-/Boot-Resume-Kette, blockiert diesen Fix nicht; Eintrag in ROADMAP/OPEN_POINTS in C4 |
| Normale Bedienung hatte nur 4 312 B Main-Stack-Reserve (PIN-Seiten/Sperre) | getrennt vom Handoff; Teil des FOLLOW-UP zur allgemeinen Stack-Absicherung |
| Aufnahme des Frame-Skripts in `run_pre_ready_gates.sh`/CI | FOLLOW-UP, Ownerentscheid (Gate-/CI-Vertrag) |
| Heap-Lage zum Reset-Zeitpunkt mit aktivem Netzwerk | in C3 pruefen, im Retest messen (4.4); Allokationsfehler ist per Test fail-closed |
| Fallback D2 | vorab festgelegt (4.4); jede weitere Abweichung ist materiell und fuehrt zu erneuter Planfreigabe |
| Reihenfolge nach dem Fix | Softwarestufe -> Independent Review -> Owner -> G5 -> G2-R3; G3/G4 bleiben gesperrt |
