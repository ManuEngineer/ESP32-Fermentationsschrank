# Implementierungsplan fuer Issue #17

## Planstatus

- Issue: `#17 – [E2.2] Laufpersistenz und Kontrollpunkte implementieren`
- Ausgangsbasis: `main@6909b90f518190131eb41c1c707a5b8738d5ba3f`
- Planbranch: `plan/issue-17-run-persistence-checkpoints`
- Ersetzt den ersten Planstand: `d23a47e3b7451e37e755f670909acc5d90705a3f`
- Harte Abhaengigkeiten: #13, #14, #15 und #54; abgeschlossen
- Planstatus: `PLAN_DRAFT_REVIEW_REQUIRED`

```text
IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

Implementierung erst nach:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

Jede materielle Abweichung von Issuegrenzen, Schema 1, dem
Zwei-Slot-/Head-Protokoll, der Integrationsgrenze oder den Ressourcen-Gates
verlangt einen neuen Plancommit und eine neue Ownerfreigabe.

## Ziel

#17 verbindet die vorhandene Fachlogik aus #13/#14/#15 mit dem
anwendungsneutralen `device_platform::IStateStore` aus #54.

Geliefert werden:

1. ein begrenzter und versionierter Laufkontrollpunkt;
2. ein deterministischer Schema-1-Codec;
3. genau zwei logische Kontrollpunktslots;
4. ein kleiner laufbezogener Head-/Transaktionsrecord;
5. persistierte Kommando- und Zustandsatomaritaet nach
   `decide -> persist -> apply`;
6. sofortige und periodische Kontrollpunkte;
7. typisierte Lade-, Rueckfall- und Nichtrekonstruierbarkeitsbefunde.

#17 bleibt nativ testbar und kennt weder NVS noch ESP-IDF. Das spaetere
ESP32-Backend implementiert nur den bestehenden `IStateStore`-Port.

## Chronologie und Architektur

#15 entstand vor der heutigen NVS- und ESP-IDF-Architektur. Es legte bewusst
nur die fachliche Commit-Grenze fest:

```text
Fachentscheidung ohne Teilmutation berechnen
-> Persistenz vor Anwendung folgt mit #17
```

Die spaeteren Entscheidungen werden heute so verwendet:

- #54: `IStateStore`, Envelope V1, CRC, Bytecodecs, Status- und
  Readbackvertrag;
- ADR-016: NVS als spaeteres produktives ESP32-Backend;
- PR #79: ESP-IDF 6.0.2 als einziger ESP32-Produktionspfad;
- #17: Fach- und Persistenzkoordination in `fermentation_app`;
- spaeter: konkreter NVS-Adapter in `device_platform_esp_idf`.

Die Abhaengigkeitsrichtung bleibt:

```text
fermentation_app -> device_platform::IStateStore
device_platform_esp_idf -> device_platform
```

## Verbindliche Quellen

- Live-Issue #17 und Owner-Synchronisationskommentar;
- Root-`AGENTS.md` und `lib/fermentation_app/AGENTS.md`;
- `docs/RUN_PERSISTENCE.md`;
- `docs/RECOVERY_AND_INTERRUPTION.md`;
- ADR-013, ADR-016 und ADR-018;
- `docs/ARCHITECTURE.md`;
- `docs/ESP_IDF_UPGRADE_CONTRACT.md`;
- Implementierungen und Tests aus #13, #14, #15 und #54;
- Ressourcen-/Integrationsbefund aus PR #53.

Bei Widerspruechen gilt `docs/SPECIFICATION_REVIEW.md`.

## Scope

### #17 verantwortet

- Kontrollpunktmodell und Schema-1-Codec;
- zwei logische Kontrollpunktslots;
- laufbezogenen Head-/Transaktionsrecord;
- Write-, Readback-, Commit-, Lade- und Rueckfallprotokoll;
- `RunPersistenceCoordinator` innerhalb `fermentation_app`;
- sofortige und periodische Kontrollpunkte;
- technische Klassifikation:
  - kein persistierter Lauf;
  - aktueller Lauf;
  - `NoActiveRun`;
  - Rueckfall;
  - Prepared/unterbrochen;
  - nicht eindeutig rekonstruierbar.

### Nicht-Scope

- Recoveryentscheidung, Ausfallintervall, Zeitkonfidenz und
  temperaturgewichteter Fortschritt: #18;
- Sensorrollen, Sensorqualitaet und Ersatzbetrieb: #20/#21;
- Fehlerklassen, Verriegelung, `SAFE_BOOT`, Fehlerreset und Aktorsperre: #24;
- Journal, Meldungshistorie, Messhistorie, Aufbewahrung und Export: #19;
- Konfigurations-Pending/Intent, Bootstrap, Werksreset und Secrets:
  #16/#56/#57;
- konkretes NVS, Preferences, LittleFS, Arduino oder ESP-IDF;
- `device_platform_esp_idf`, GPIO, Sensor-, Display-, WLAN- oder Aktoradapter;
- Erweiterung von `IPlatformServices`;
- Verkabelung in `FermentationApplication`, `src/main.cpp` oder
  `main/app_main.cpp`;
- allgemeine Datenbank-, Event-Sourcing-, Repository- oder
  Transaktionsplattform;
- lokale Ersatzmodelle fuer spaetere Issues;
- Dummyfelder oder untypisierte Erweiterungsblobs.

## Vorhandene Bausteine

### Fachlogik

- `ActiveRun`, `RunProgramSnapshot`, `RunRevision`,
  `ActiveRun::restore()` aus #13;
- `ProcessRunSnapshot`, `ProcessRuntimeState`, `TransitionDecision`,
  `decideProcessTransition()` und `applyProcessTransition()` aus #14;
- `RunCommandState`, `CommandDecision`, `decide*()` und
  `applyRunCommand()` aus #15.

Abgelehnte Decisions mutieren keinen Zustand. Das begrenzte
In-Memory-Kommando-ID-Fenster aus #15 ist keine persistente
Wiederholungserkennung.

### Plattform

- atomarer Replace pro `IStateStore`-Schluessel;
- `Success`, `WriteError`, `CapacityError`,
  `CommitOutcomeUnknown`;
- exakter Readback bei unbekanntem Commit-Ausgang;
- `StateStoreKey` mit 1..15 Bytes aus `[A-Za-z0-9_.-]`;
- Envelope V1, CRC-32/ISO-HDLC, Big-Endian-Codecs;
- starke technische Typen und Ueberlaufpruefung;
- `loadSlotPayload()` und begrenzte technische Slotbausteine;
- `SimulatedPersistentStateStore` fuer Tests.

#17 dupliziert keinen dieser Vertraege.

## Stabile Kennungen

Naechste freie Anwendungs-Recordtypen:

```text
RecordTypeId 7: RunCheckpoint
RecordTypeId 8: RunPersistenceHead
SchemaVersion 1 fuer beide
```

Schluessel:

```text
rc0   RunCheckpoint Slot 0
rc1   RunCheckpoint Slot 1
rh0   RunPersistenceHead
```

Die bisherigen Konfigurationsrecordtypen verwenden 1..6. Die neuen IDs und
Schluessel sind auf der Ausgangsbasis kollisionsfrei.

`Envelope.versionValue` bedeutet:

- bei `RunCheckpoint`: Kontrollpunktrevision `uint64`, Start bei 1;
- bei `RunPersistenceHead`: Headrevision `uint64`, Start bei 1 und Erhoehung
  bei jedem bestaetigten Headwrite.

Revision 0 und Ueberlauf werden typisiert abgelehnt.

## Eingespeiste Werte ohne Konfigurationskopplung

#17 erhaelt als bereits validierte Werte vom spaeteren Aufrufer:

- `StorageEpoch`, ungleich 0;
- Kontrollpunktintervall 1..60 Minuten;
- monotone Zeit;
- optionalen verlaesslichen UTC-Wert.

#17 liest oder mutiert keinen ConfigurationRoot und erzeugt keinen
StorageEpoch. Native Tests verwenden explizite Testwerte. Die spaetere
Runtime-Composition liefert die Werte ueber schmale Konstruktor-/Methodenwerte,
nicht ueber ein breites `IPlatformServices`.

## Genau zwei logische Kontrollpunktslots

Schema 1 verwendet genau zwei logische Slots.

- Der inaktive Slot wird geschrieben; der aktuelle bleibt unangetastet.
- `WriteError` und `CapacityError` veraendern den alten Slot nicht.
- `CommitOutcomeUnknown` wird exakt zurueckgelesen.
- Nach bestaetigtem Headcommit wird neu zu aktuell und alt zu Rueckfall.
- Mehr Slots waeren zusaetzliche Historie oder vermeintliches Wear-Leveling.
- Historie gehoert zu #19; physisches Wear-Leveling zum NVS-/Hardwarepfad.

`rh0` ist kein dritter Kontrollpunktslot.

Ein belegter Widerspruch stoppt die Umsetzung zur Ownerentscheidung; die
Slotzahl wird nicht eigenmaechtig geaendert.

## Laufkontrollpunkt Schema 1

### Varianten

```text
ProgramRun = 1
ManualRun = 2
NoActiveRun = 3
```

`NoActiveRun` ist ein Tombstone. Er verhindert die Wiederbelebung eines alten
Laufs nach bestaetigtem Abbruch oder bestaetigter Rueckkehr nach `STANDBY`.

### Speichertrigger

```text
Periodic = 1
RunCommand = 2
ProcessTransition = 3
LifecycleTombstone = 4
```

Spaetere Sensor- oder Recoveryproduzenten erweitern den Vertrag nicht
stillschweigend. Eine neue fachliche Bedeutung verlangt eine neue
Schemaversion oder einen neuen freigegebenen Plan.

### Kanonische Top-Level-Reihenfolge

1. Variante `uint8`;
2. Trigger `uint8`;
3. monotone Kontrollpunktzeit `uint64`;
4. Intervall-Minuten `uint16`;
5. Lauf-ID: `uint16`-Laenge plus maximal 48 Bytes;
6. `runRevision` `uint32`;
7. `commandSequence` `uint32`;
8. optionale letzte atomar uebernommene `CommandId`:
   Tag `uint8`, danach optional `uint64`;
9. optionaler `ProcessRunSnapshot`;
10. `ProcessRuntimeState`;
11. variantenabhaengiger Inhalt.

Der optionale UTC-Wert liegt ausschliesslich im vorhandenen Envelope-Feld.

### ProgramRun

- `RunProgramSnapshot`;
- vorhandene, begrenzte `RunRevision`-Folge;
- Wiederherstellung ueber `ActiveRun::restore()`;
- keine zweite, frei widersprechende Kopie der effektiven Werte.

### ManualRun

- vorhandener `ManualRunPlan`;
- `ProcessRunSnapshot`;
- `ProcessRuntimeState`.

### NoActiveRun

- keine aktive Programm- oder Manuallaufstruktur;
- nur notwendige Lebenszyklus-, Revisions- und Zeitinformation.

### Ausgeschlossen aus Schema 1

- `RuntimeMessage`-Historie und Meldungsjournal;
- `criticalSafetyEventPending`, Fehlerklassen und Verriegelungen;
- gesamtes `processedCommandIds`-Fenster;
- Sensorwerte, Sensorqualitaet und Sensorrolle;
- temperaturgewichteter Fortschritt und Recoverykonfidenz;
- GPIO- oder Aktorzustaende.

Acknowledge-/Mute-Kommandos und FaultReset werden in #17 nicht dauerhaft
koordiniert, weil ihre persistente Zustaendigkeit bei #19 beziehungsweise #24
liegt. Sie duerfen spaeter nicht ohne den Vertrag ihres owning Issues als
persistiert gelten.

## ProgramDocument-Wirelogik

Der Programmschnappschuss erhaelt keine zweite abweichende Kodierung.

Die bereits im ProgramCatalog-Codec vorhandene Einzelprogrammkodierung wird
klein und bytekompatibel als interne gemeinsame Hilfe in
`fermentation_app` extrahiert.

- bestehendes ProgramCatalog-Wireformat bleibt bytegenau unveraendert;
- vorhandene Golden- und Migrationstests bleiben gruen;
- genau zwei reale Konsumenten: ProgramCatalog und RunCheckpoint;
- keine allgemeine oeffentliche Universalcodec-Plattform.

## Groessengrenzen

```text
RunCheckpoint-Payload maximal: 8192 Byte
RunCheckpoint-Envelope mit UTC maximal: 8237 Byte
RunPersistenceHead-Payload maximal: 256 Byte
RunPersistenceHead-Envelope mit UTC maximal: 301 Byte
RunRevision-Anzahl: bestehende 32
Intervall: 1..60 Minuten, Standard 5
```

Die Implementierung leitet die echte Worst-Case-Groesse aus den vorhandenen
Feldgrenzen ab und testet sie. Passt ein maximal gueltiger Zustand nicht in
8192 Byte, wird die Grenze nicht angehoben; die Umsetzung stoppt mit Messwerten
zur Ownerentscheidung.

## Head- und Transaktionsrecord

### Zustaende

```text
Committed = 1
Prepared = 2
```

### CheckpointReference

Jede Referenz bindet:

- Slot-ID `uint8`;
- Checkpointrevision `uint64`;
- Schema-Version `uint32`;
- StorageEpoch `uint64`;
- Payloadlaenge `uint32`;
- Payload-CRC `uint32`;
- Kontrollpunktvariante `uint8`.

### Committed

Enthaelt:

- aktuelle `CheckpointReference`;
- optionale Rueckfallreferenz.

### Prepared

Enthaelt:

- bisherige aktuelle Referenz;
- optionale bisherige Rueckfallreferenz;
- Zielreferenz;
- MutationKind:
  - `RunCommand = 1`;
  - `ProcessTransition = 2`;
- optionale `CommandId`;
- erwartete alte und neue `runRevision`;
- erwartete alte und neue `transitionSequence`.

Der Zielkontrollpunkt wird vor dem Prepared-Write vollstaendig in RAM kodiert,
validiert und per Laenge/CRC gebunden.

## Mutationsprotokoll

Fuer eine bereits fachlich vorgeschlagene Mutation:

```text
1. Zielzustand aus CommandDecision oder TransitionDecision verwenden.
2. Zielkontrollpunkt vollstaendig kodieren und validieren.
3. Prepared-Head bestaetigt schreiben.
4. Zielkontrollpunkt in den inaktiven Slot bestaetigt schreiben.
5. Committed-Head mit Ziel als aktuell und alt als Rueckfall bestaetigt schreiben.
6. Erst danach bestehende apply-Funktion auf den RAM-Zustand anwenden.
```

Bei jedem `CommitOutcomeUnknown` wird exakt derselbe erwartete Record
zurueckgelesen. Kein Folgeschritt startet ohne eindeutige Bestaetigung.

## Periodischer Kontrollpunkt

Periodisch wird keine neue Fachmutation erzeugt:

```text
1. aktuellen gueltigen RAM-Zustand kodieren;
2. inaktiven Slot bestaetigt schreiben;
3. Committed-Head bestaetigt aktualisieren.
```

Scheitert der Headcommit, bleibt der neue Slot unreferenziert und ist nicht
massgeblich.

## Ladevertrag

Laden ist head-first, nicht highest-revision-first:

1. `rh0` lesen und Envelope/Schema/Epoch validieren;
2. Headpayload validieren;
3. exakt referenzierten aktuellen Slot laden;
4. bei ungueltigem aktuellen Slot exakt referenzierten Rueckfall pruefen;
5. nicht referenzierte oder hoehere Slots nie still als aktuell waehlen.

`scanTechnicalSlotCandidates()` darf fuer begrenzte Diagnose verwendet werden,
ist aber keine alternative Recovery-Wahrheit. Ein fehlender oder ungueltiger
Head wird nicht aus den Slots erraten.

### Unterbrechungsfaelle

- vor bestaetigtem Prepared: alter Committed-Head bleibt massgeblich;
- Prepared, Ziel fehlt/abweichend: Mutation nicht committed; alten Stand und
  Befund liefern;
- Prepared, Ziel gueltig, Committed fehlt: Ziel technisch vorhanden, aber
  nicht committed; keine stille Anwendung;
- Committed passt: neuer Zustand bestaetigt, auch bei Stromausfall vor RAM-Apply;
- aktueller ungueltig, Rueckfall gueltig: typisierter Rueckfall;
- aktueller Tombstone ungueltig: keinen alten aktiven Lauf wiederbeleben;
- Head unlesbar/korrupt/widerspruechlich: nicht eindeutig rekonstruierbar.

#18/#24 entscheiden spaeter ueber Recovery, `SAFE_BOOT`, Fault und
Aktorfreigabe.

## Anwendungsintegration

### RunPersistenceCoordinator

Neuer Produktionsbaustein in `fermentation_app`:

```text
vorhandene Fachentscheidung
-> Kontrollpunkt bauen
-> Head-/Slotprotokoll
-> bestaetigter Commit
-> vorhandene apply-Funktion
```

Er verwendet:

- `CommandDecision` und `applyRunCommand()`;
- `TransitionDecision` und `applyProcessTransition()`;
- RunCheckpointStore;
- keinen konkreten Adapter.

Er berechnet keine Fachentscheidung erneut und dupliziert keine
`decide*`-Logik.

Pfade:

- persistierte laufwirksame `CommandDecision`;
- persistierte laufwirksame `TransitionDecision`;
- periodischer Kontrollpunkt;
- `NoActiveRun`-Tombstone.

Nicht vorgeschlagene/abgelehnte Decisions erzeugen keinen Write und keinen
Apply.

### Noch keine Runtime-Verkabelung

#17 aendert nicht:

- `FermentationApplication`;
- `IPlatformServices`;
- `src/main.cpp`;
- `main/app_main.cpp`;
- `device_platform_esp_idf`.

Der Coordinator ist Produktionscode und nativ vollstaendig getestet, besitzt
aber noch keinen realen UI-, Composition-Root- oder NVS-Aufrufer.

Ein spaeterer Produktionspfad muss den Coordinator verwenden. Direkte
Runtime-Aufrufe von `applyRunCommand()` oder `applyProcessTransition()` waeren
eine Architekturverletzung. Reine Domain-Unit-Tests duerfen weiterhin direkt
testen.

## Ressourcen-Gate aus PR #53

Der damalige Risikoindikator lag ungefaehr bei:

```text
RunCommandState: 4200 Byte
CommandDecision: 8608 Byte
```

Er entstand vor dem heutigen ESP-IDF-Produktionspfad und ist kein aktueller
Freigabenachweis.

Da #17 noch keinen Runtime-Aufrufer einfuehrt, bleibt das reale Messgate eine
harte Sperre vor der ersten spaeteren Runtime-/UI-/Composition-Root-Aktivierung.

Der spaetere Aktivierungs-PR misst unter ESP-IDF 6.0.2 ohne PSRAM mit maximalem
48/96/1024-Programmkandidaten:

- Task-Stack-High-Water-Mark jedes Kommandowegs;
- freien Heap und groessten zusammenhaengenden Block vor, waehrend und nach
  Decision, Persistenz und Apply;
- Allokationsfehler ohne Teilmutation oder Aktorfreigabe.

Danach entscheidet der Owner ueber Beibehaltung der Vollschnappschuesse oder
einen separaten Delta-Decision-Umbau. #17 zieht diesen Umbau nicht vor.

#17 dokumentiert bereits:

- `sizeof` der Typen im nativen und ESP-IDF-Build;
- maximale Payload-/Envelopegroessen;
- native Peakallokation;
- statische RAM-/Flashdeltas beider ESP-IDF-Profile.

## Geplanter Dateischnitt

### Keine Aenderung

- `lib/device_platform`;
- Produktionscode in `lib/device_platform_test_support`;
- `FermentationApplication`;
- Composition Roots;
- ESP-IDF-Adapter.

### Neue Dateien in `lib/fermentation_app`

- `run_persistence_limits.hpp`;
- `run_persistence_contract.hpp`;
- `run_checkpoint.hpp/.cpp`;
- `run_checkpoint_codec.hpp/.cpp`;
- `run_persistence_head.hpp/.cpp`;
- `run_persistence_head_codec.hpp/.cpp`;
- `run_checkpoint_store.hpp/.cpp`;
- `run_checkpoint_schedule.hpp/.cpp`;
- `run_persistence_coordinator.hpp/.cpp`.

### Bestehende Dateien nur bei belegtem Bedarf

- `configuration_document_codec.*`:
  bytekompatible Extraktion der Einzelprogrammkodierung;
- `run_snapshot.*`, `run_commands.*`, `process_state_machine.*`:
  nur schmale read-only Hilfen oder vorhandene Werte.

Neue Fach-, Recovery-, Hardware- oder Plattformsemantik in diesen Dateien ist
nicht geplant.

## Ergebnisvertraege

Getrennte typisierte Ergebnisse mindestens fuer:

- Encode-/Decode-/Validierungsfehler;
- ReadError, WriteError, CapacityError;
- `CommitOutcomeUnknown` bestaetigt/nicht bestaetigt;
- kein persistierter Lauf;
- aktueller Lauf;
- `NoActiveRun`;
- Rueckfall;
- Prepared/unterbrochen;
- Head-/Referenz-/Schema-/Epoch-/CRC-Widerspruch;
- nicht eindeutig rekonstruierbar;
- fachlich abgelehnte Decision;
- Persistenz und Apply erfolgreich;
- Persistenz committed, Apply wegen interner Vertragsverletzung abgelehnt.

Der letzte Fall ist ein sichtbarer interner Vertragsfehler, aber #17 erzeugt
daraus selbst keinen Fault-, `SAFE_BOOT`- oder Aktorbefehl.

## Teststrategie

### Wire und Modell

- Golden Bytes: ProgramRun, ManualRun, Tombstone, Committed, Prepared;
- maximale Strings, Programmdokumente und 32 RunRevisions;
- unbekannte Varianten/Enums;
- Schema/RecordType/Epoch/Revision 0 oder falsch;
- Trunkierung, Zusatzbytes, Laengen- und CRC-Fehler;
- NaN, Unendlichkeit und negative Null ueber bestehende Double-Codecs;
- ProgramCatalog-Wireformat nach Codec-Extraktion byteidentisch;
- Worst-Case-Payload <= 8192 Byte.

### Store und Cut-Points

- leerer Speicher;
- erster Laufstart;
- Rotation zwischen zwei Slots;
- aktueller korrupt, Rueckfall gueltig;
- beide ungueltig;
- Head fehlt, ReadError, CapacityError, korrupt;
- fremde Epoch/Schema;
- Prepared vor Ziel;
- Cut nach Prepared;
- Zielwrite `CommitOutcomeUnknown`, alt/neu;
- Cut nach Ziel vor Committed;
- Headwrite `CommitOutcomeUnknown`, alt/neu;
- Cut nach Committed vor Apply;
- periodischer orphaned Slot;
- Tombstone verhindert Wiederbelebung;
- keine Teil-/Mischrecords.

### Coordinator

Laufwirksame Pfade:

- Programmstart;
- manueller Start;
- Abbruch/Ausschalten;
- Abbruch/Kuehlen;
- Abschlussquittierung;
- Kuehlen nach Abschluss;
- Laufanpassung;
- relevante Prozessuebergaenge.

Je Pfad:

- Ablehnung: kein Write, kein Apply;
- Prepared-Fehler: kein Zielwrite, kein Apply;
- Ziel-Fehler: kein Commit, kein Apply;
- Head-Fehler: kein Apply;
- Erfolg: genau ein Apply;
- Neustart: keine doppelte Mutation;
- interner Apply-Vertragsfehler bleibt sichtbar.

### Intervall

- 1, 5, 60 Minuten;
- 0 und >60 abgelehnt;
- rueckwaertige monotone Zeit abgelehnt;
- Ereignis sofort;
- keine Wiederholung vor Faelligkeit;
- kein Write im Zwei-Sekunden-Sensorzyklus;
- Ereignis setzt naechste Faelligkeit nachvollziehbar.

### Regression und Quality Gates

- kompletter nativer Testsatz;
- beide ESP-IDF-Profile mit ESP-IDF 6.0.2;
- Format, clang-tidy, Architektur, Secrets und Quality-Selftests;
- `git diff --check`;
- Base-/Head-Ressourcenbericht;
- keine Produktionsabhaengigkeit auf `device_platform_test_support`;
- keine NVS-, Arduino- oder ESP-IDF-Header in #17-Dateien.

## Commitreihenfolge

1. Vertraege, Grenzen, Modelle und bytekompatible ProgramDocument-Hilfe;
2. Checkpoint-/Headcodecs und Golden-/Negativtests;
3. Zwei-Slot-/Headstore, Ladevertrag und Cut-Point-Tests;
4. Coordinator und persistierte Laufmutationen;
5. Ereignis-/Intervallscheduler;
6. Dokumentation, Ressourcenbericht und vollstaendige Gates.

Neue Ports, Adapter, Abhaengigkeiten, Composition-Root-Arbeit,
Delta-Decision-Umbau oder mehr als zwei Slots verlangen einen neuen Plancommit.

## Dokumentation nach Implementierung

- `docs/RUN_PERSISTENCE.md`;
- `docs/IMPLEMENTATION_ISSUES.md`;
- `CHANGELOG.md`.

Nur anhand des realen Diffs; keine Aenderung in der Planungsphase.

## Offene Folgegates

```text
TBD_IMPLEMENTATION_BUDGET
```

Tatsaechliche Payload-, Heap-, RAM- und Flashwerte messen. Grenzen nicht
stillschweigend anheben.

```text
MEASUREMENT_REQUIRED_BEFORE_RUNTIME_ACTIVATION
```

Reales Stack-/Heap-/Allokationsgate aus PR #53 unter ESP-IDF 6.0.2 durch den
ersten spaeteren Runtime-/UI-/Composition-Root-Aufrufer.

```text
MEASUREMENT_REQUIRED
```

Reale NVS-Kapazitaet, Atomizitaet, Schreibdauer, Wear-Leveling, Jitter und
Watchdogwirkung im Adapter-/Hardwarepfad.

```text
SPIKE_REQUIRED
```

Reale Stromunterbrueche mit produktivem NVS-Adapter.

```text
TBD_COMMISSIONING
```

Konkrete Einstellung innerhalb 1..60 Minuten sowie Zeit-/Thermikgrenzen aus
#18.

## SOLID, DRY und KISS

| Prinzip | Bewertung |
| --- | --- |
| SRP | Modell, Codecs, Store, Scheduler und Coordinator bleiben getrennt. |
| OCP | Backend und Recovery werden ueber bestehende Abstraktionen erweitert. |
| LSP | Jeder vertragskonforme `IStateStore` bleibt austauschbar. |
| ISP | Kein breites Plattform- oder Persistenz-Sammelinterface. |
| DIP | Fachlogik kennt nur `device_platform`-Abstraktionen. |
| DRY | Envelope, CRC, Bytecodecs, ProgramDocument-Wirelogik, `decide*`, `restore()` und `apply*` werden wiederverwendet. |
| KISS | Zwei Slots, ein kleiner Head und ein Coordinator; keine Datenbank, kein Journal und keine Zukunftsmodelle. |

## Abnahmekriterien

- Programm- und Manuallauf rekonstruierbar;
- Tombstone verhindert Wiederbelebung;
- Rueckfall nur auf explizit referenzierten gueltigen Stand;
- Head-/Transaktionswiderspruch wird nicht geraten;
- keine Mutation vor bestaetigtem Committed-Head;
- `CommitOutcomeUnknown` immer per Readback aufgeloest;
- keine Hardware-, NVS-, Sensor- oder Recoveryvorwegnahme;
- Intervall-/Schreiblastvertrag eingehalten;
- Schema 1 verwendet keine Modelle aus #18/#20/#21/#24;
- native Tests, beide ESP-IDF-Builds und Quality Gates gruen;
- Ressourcenwirkung dokumentiert;
- PR bleibt Draft; Merge ausschliesslich durch den Owner.

## Plan-Selbstpruefung

```text
ARCHITECTURE_ALIGNMENT: PASS
ISSUE_BOUNDARIES: PASS
RUN_TRANSACTION_CONTRACT: PASS
SCHEMA_1_SCOPE: PASS
STORAGE_EPOCH_BOUNDARY: PASS
APPLICATION_INTEGRATION: PASS
TWO_SLOT_CONTRACT: PASS
HEAD_FIRST_LOAD: PASS
ESP_IDF_BOUNDARY: PASS
RESOURCE_GATE: PASS_AS_FUTURE_RUNTIME_BLOCKER
SOLID: PASS
DRY: PASS
KISS: PASS
IMPLEMENTATION: NOT_STARTED
OWNER_PLAN_APPROVAL: REQUIRED
```
