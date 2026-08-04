# Implementierungsplan fuer Issue #17

## Planstatus

- Issue: `#17 – [E2.2] Laufpersistenz und Kontrollpunkte implementieren`
- Ausgangsbranch: `main`
- Ausgangs-Commit: `6909b90f518190131eb41c1c707a5b8738d5ba3f`
- Planbranch: `plan/issue-17-run-persistence-checkpoints`
- Ersetzt den ersten Planstand: `d23a47e3b7451e37e755f670909acc5d90705a3f`
- Planstatus: `PLAN_DRAFT_REVIEW_REQUIRED`
- Harte Abhaengigkeiten: #13, #14, #15 und #54; alle abgeschlossen.

```text
IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

Dieser Plan autorisiert noch keine Implementierung. Sie darf erst nach einem
eindeutigen Ownerkommentar fuer genau den Commit mit dieser Planversion
beginnen:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

Jede materielle Abweichung von den unten festgelegten Issuegrenzen, dem
Schema-1-Vertrag, dem Zwei-Slot-/Head-Protokoll oder der
Anwendungsintegration macht die Freigabe ungueltig und verlangt einen neuen
Plancommit.

## Ziel

Issue #17 liefert die nativ testbare Laufpersistenz zwischen der bereits
vorhandenen Fachlogik aus #13/#14/#15 und dem anwendungsneutralen
`device_platform::IStateStore` aus #54.

Das Issue soll:

1. einen begrenzten, typisierten und versionierten Laufkontrollpunkt aus den
   bereits vorhandenen autoritativen Lauf-, Prozess- und Kommandomodellen
   bilden;
2. den Kontrollpunkt deterministisch und unabhaengig von C++-Layout, ABI,
   Padding, Arduino oder ESP-IDF kodieren;
3. genau zwei logische Kontrollpunktslots sowie einen kleinen fachlichen
   Head-/Transaktionsrecord verwenden;
4. den neuesten bestaetigten Kontrollpunkt und genau einen bestaetigten
   Rueckfallstand eindeutig referenzieren;
5. bereits fachlich entschiedene Laufmutationen erst nach bestaetigter
   Persistenz anwenden;
6. sofortige fachliche Speichertrigger und periodische Kontrollpunkte von
   einer bis 60 Minuten, Standard fuenf Minuten, ohne Schreiben im
   Sensorzyklus abbilden;
7. technische Speicher-, Integritaets- und Nichtrekonstruierbarkeitsbefunde
   typisiert melden, ohne eine Recovery-, Fehlerklassen- oder Aktorentscheidung
   vorwegzunehmen.

Das Ergebnis bleibt backend- und hardwareunabhaengig. #17 verwendet nur
`device_platform::IStateStore` und die bereits vorhandenen technischen
Wire-/Slotbausteine. Ein konkreter NVS-, ESP-IDF-, Preferences- oder
Hardwareadapter ist nicht Teil dieses Issues.

## Chronologie und heutige Architektur

#15 wurde vor der heutigen NVS- und ESP-IDF-Architektur umgesetzt. Es legte
bewusst nur die fachliche Grenze fest:

```text
Entscheidung vollstaendig und ohne Teilmutation berechnen
-> Persistenz vor Anwendung folgt mit #17
```

#15 entschied weder NVS noch ESP-IDF. Diese spaeteren Entscheidungen werden
heute wie folgt konsumiert:

- #54 stellt `IStateStore`, Envelope V1, CRC, begrenzte Bytecodecs,
  Slotkandidaten und `CommitOutcomeUnknown` bereit;
- ADR-016 legt NVS als spaeteres produktives ESP32-Backend fest;
- PR #79 legt ESP-IDF 6.0.2 als einzigen ESP32-Produktionspfad fest;
- #17 bleibt in `fermentation_app` und kennt weder NVS noch ESP-IDF;
- ein spaeterer Adapter in `device_platform_esp_idf` implementiert den
  bestehenden Port, ohne dass #17 geaendert werden muss.

## Nicht-Ziele und verbotene Vorwegnahmen

Nicht Bestandteil von #17 sind:

- Recoveryentscheidung, Ausfallintervall, Zeitkonfidenz,
  temperaturgewichteter Fortschritt oder Fortschrittskorrektur aus #18;
- Sensorqualitaet, primaere Sensorrolle, Ersatzbetrieb oder Rueckkehrlogik aus
  #20/#21;
- Fehlerklassen, persistente Verriegelung, `SAFE_BOOT`, Fehlerreset oder
  systemweite Aktorsperre aus #24;
- Journale, Messhistorie, Aufbewahrung, Bereinigung, Export, Backup oder Import
  aus #19;
- Konfigurationsgraphen, Pending-/Intent-Semantik, Bootstrap, Werksreset,
  Secret- oder Konfigurationspersistenz aus #16/#56/#57;
- konkrete NVS-, Preferences-, LittleFS-, Arduino- oder ESP-IDF-APIs;
- `device_platform_esp_idf`, GPIO, BTS7960, Sensor-, Display-, WLAN- oder
  Aktoradapter;
- Erweiterung von `IPlatformServices` zu einem breiten Sammelinterface;
- produktive Verkabelung in `FermentationApplication`, `src/main.cpp` oder
  `main/app_main.cpp`;
- eine allgemeine Datenbank-, Event-Sourcing-, Repository- oder
  Transaktionsplattform;
- lokale Ersatztypen fuer spaetere Sensor-, Recovery-, Journal- oder
  Sicherheitsmodelle;
- Dummyfelder, reservierte Erweiterungsblobs oder bedeutungslose
  Zukunftsplatzhalter;
- Issue-, Label-, Milestone- oder Statusaenderungen.

## Verbindliche Quellen

Bei Widerspruechen gilt die Prioritaet aus `docs/SPECIFICATION_REVIEW.md`.
Massgeblich sind:

- Live-Issue #17 und sein Owner-Synchronisationskommentar;
- Root-`AGENTS.md` und `lib/fermentation_app/AGENTS.md`;
- `docs/RUN_PERSISTENCE.md`;
- `docs/RECOVERY_AND_INTERRUPTION.md`;
- ADR-013, ADR-016 und ADR-018;
- `docs/ARCHITECTURE.md`;
- `docs/ESP_IDF_UPGRADE_CONTRACT.md`;
- die gemergten Implementierungen und Tests aus #13, #14, #15 und #54;
- der Ressourcen- und Integrationsbefund aus dem Nachreview PR #53.

## Issuegrenzen

### Issue #17 verantwortet

- Laufkontrollpunktmodell und Schema-1-Codec;
- zwei logische Kontrollpunktslots;
- einen kleinen, laufbezogenen Head-/Transaktionsrecord;
- Write-, Readback-, Commit-, Rueckfall- und Ladeprotokoll;
- fachliche Koordination `decide -> persist -> apply` innerhalb von
  `fermentation_app`;
- sofortige und periodische Kontrollpunkte;
- technische Diagnose, ob kein Lauf, ein rekonstruierbarer Lauf oder ein
  nicht eindeutig rekonstruierbarer Zustand vorliegt.

### Nachgelagerte Issues verantworten

- #18: Bedeutung eines geladenen Laufes fuer Recovery, Unterbrechungsdauer,
  Zeitkonfidenz und Fortschritt;
- #20/#21: Sensorrollen und Sensorqualitaet;
- #24: Zuordnung technischer Persistenzbefunde zu Fehlerklasse,
  Verriegelung, `SAFE_BOOT` und Aktorsperre;
- #19: Journal, Historie und Aufbewahrung;
- der spaetere Adapter-/Hardwarepfad: reales NVS, Flashverhalten,
  Stromunterbruch, Laufzeitjitter und Watchdogwirkung.

#17 darf fuer diese spaeteren Entscheidungen nur typisierte Befunde liefern.

## Aktuelle Ausgangslage und Wiederverwendung

### Vorhandene Fachlogik

- #13: `ActiveRun`, `RunProgramSnapshot`, `EffectiveRunValues`, bis zu 32
  `RunRevision`-Eintraege und `ActiveRun::restore()`.
- #14: `ProcessRunSnapshot`, `ProcessRuntimeState`,
  `TransitionDecision`, `decideProcessTransition()` und
  `applyProcessTransition()`.
- #15: `RunCommandState`, `CommandDecision`, die `decide*`-Funktionen und
  `applyRunCommand()`. Abgelehnte Entscheidungen veraendern keinen Zustand.
- #15 besitzt nur ein begrenztes In-Memory-Fenster fuer Kommando-IDs. Die
  persistierte Commit-Grenze wurde ausdruecklich an #17 delegiert.

### Vorhandene Plattformbausteine

- `IStateStore` mit atomarem Replace pro Schluessel;
- getrennte Lese- und Schreibstatus;
- `CommitOutcomeUnknown` mit verpflichtendem Readback;
- `StateStoreKey` mit 1 bis 15 Bytes aus `[A-Za-z0-9_.-]`;
- starke technische Typen und Ueberlaufpruefung;
- Envelope V1, CRC-32/ISO-HDLC und begrenzte Byte-Reader/-Writer;
- technischer Kandidatenscan und gezieltes Payloadladen;
- `SimulatedPersistentStateStore` mit Cut-Points, Korruption und Neustart.

Diese Bausteine werden wiederverwendet und nicht in #17 dupliziert.

## Stabile Speicherkennungen

Der Plan reserviert als naechste freie anwendungsspezifische Recordtypen:

```text
RecordTypeId 7: RunCheckpoint
RecordTypeId 8: RunPersistenceHead
SchemaVersion 1 fuer beide Recordtypen
```

Die bisherigen Konfigurationsrecordtypen verwenden 1 bis 6; die Werte 7 und 8
sind damit auf dem aktuellen `main` kollisionsfrei. Eine spaetere Aenderung
bedarf eines neuen Plancommits.

ADR-016-konforme Schluessel:

```text
rc0   RunCheckpoint Slot 0
rc1   RunCheckpoint Slot 1
rh0   RunPersistenceHead
```

Alle drei Schluessel liegen innerhalb des verbindlichen 1-bis-15-Byte-
Schluesselraums. `rh0` ist kein dritter Kontrollpunktslot.

## Genau zwei logische Kontrollpunktslots

Schema 1 verwendet genau zwei logische Kontrollpunktslots.

Begruendung:

- Beim Schreiben des inaktiven Slots bleibt der aktuell bestaetigte Slot
  unangetastet.
- `WriteError` und `CapacityError` veraendern den alten Slot nicht.
- `CommitOutcomeUnknown` wird durch exakten Readback aufgeloest.
- Nach einem erfolgreichen Commit wird der neue Slot aktuell und der bisher
  aktuelle Slot zum Rueckfall.
- Mehr als zwei logische Slots wuerden in #17 zusaetzliche Historie oder eine
  vermeintliche logische Verschleissstrategie einfuehren.
- Historie gehoert zu #19; physisches Wear-Leveling und reale Flashlebensdauer
  gehoeren zum NVS-/Hardwarepfad.

Findet die Umsetzung einen belegten Widerspruch, darf sie nicht selbst auf drei
oder mehr Slots wechseln. Sie muss mit dem konkreten Befund und einer
Ownerfrage anhalten.

## Laufkontrollpunkt Schema 1

### Fachlicher Zustand

Ein Kontrollpunkt besitzt genau eine der folgenden Varianten:

```text
ProgramRun
ManualRun
NoActiveRun
```

`NoActiveRun` ist ein typisierter Tombstone. Er verhindert, dass ein alter
aktiver Lauf nach einem bestaetigten Abbruch, einer bestaetigten Rueckkehr nach
`STANDBY` oder einem neuen Lebenszyklus faelschlich wieder erscheint.

Schema 1 verwendet nur heute vorhandene autoritative Modelle.

Gemeinsame Felder in kanonischer Reihenfolge:

1. Kontrollpunktvariante `uint8`;
2. Speichertrigger `uint8`;
3. monotone Kontrollpunktzeit `uint64`;
4. Kontrollpunktintervall in Minuten `uint16`;
5. Lauf-ID als `uint16`-Laenge plus begrenzte Bytes;
6. `runRevision` `uint32`;
7. `commandSequence` `uint32`;
8. optional letzte atomar uebernommene `CommandId` als Tag plus `uint64`;
9. `ProcessRunSnapshot` als typisierte optionale Struktur;
10. `ProcessRuntimeState` mit fester Feldreihenfolge und festen Breiten;
11. variantenabhaengiger Laufinhalt.

`ProgramRun` enthaelt:

- `RunProgramSnapshot`;
- den aktuellen `ActiveRun`-Zustand, rekonstruierbar ueber Programmschnappschuss
  und die begrenzte Folge vorhandener `RunRevision`-Eintraege;
- keine zweite frei widersprechende Kopie der effektiven Werte.

`ManualRun` enthaelt:

- den vorhandenen `ManualRunPlan`;
- den zugehoerigen `ProcessRunSnapshot` und `ProcessRuntimeState`;
- keine neu erfundene Programmdokument- oder Sensorsemantik.

`NoActiveRun` enthaelt ausser gemeinsamer Lebenszyklus-, Revisions- und
Zeitinformation keinen aktiven Programm- oder Manuallauf.

Nicht Teil von Schema 1 sind:

- `RuntimeMessage`-Historie oder Meldungsjournal;
- `criticalSafetyEventPending`, Fehlerklassen oder Verriegelungen;
- das gesamte `processedCommandIds`-Fenster;
- Sensorwerte, Sensorqualitaet oder primaere Sensorrolle;
- temperaturgewichteter Fortschritt und Recoverykonfidenz;
- direkte Hardware- oder Aktorzustaende.

### Zeit

Der optionale verlaessliche UTC-Wert wird ausschliesslich im vorhandenen
Envelope-V1-UTC-Feld gespeichert. Schema 1 erfindet keinen neuen
Zeitqualitaetstyp. Die Bewertung, ob der Wert fuer Recovery verwendbar ist,
bleibt #18.

### Programmdokument-Codec

Der Programmschnappschuss darf nicht mit einer zweiten, abweichenden
ProgramDocument-Kodierung dupliziert werden.

Der bestehende kanonische Programmkatalogcodec ist so klein zu refaktorieren,
dass die Kodierung und Dekodierung genau eines `ProgramDocument` intern von
Konfigurationskatalog und Laufkontrollpunkt wiederverwendet werden kann.

Dabei gilt:

- das bestehende ProgramCatalog-Wireformat bleibt bytegenau unveraendert;
- vorhandene Golden- und Migrationstests bleiben unveraendert gruen;
- keine neue oeffentliche Universalcodec-Schicht ohne zweiten realen
  Konsumenten;
- die gemeinsame Hilfe bleibt innerhalb von `fermentation_app`.

### Grenzen

Verbindliche statische Obergrenzen:

```text
maximaler RunCheckpoint-Payload: 8192 Byte
maximaler RunCheckpoint-Envelope mit UTC: 8237 Byte
maximaler RunPersistenceHead-Payload: 256 Byte
maximaler RunPersistenceHead-Envelope mit UTC: 301 Byte
logische Kontrollpunktslots: 2
maximale RunRevision-Anzahl: bestehende 32
Intervall: 1..60 Minuten, Standard 5 Minuten
```

Die Implementierung muss die tatsaechliche Worst-Case-Groesse aus den
bestehenden Feldgrenzen herleiten und als Test dokumentieren. Passt der
maximale gueltige Zustand nicht in 8192 Byte, wird die Grenze nicht still
angehoben. Die Umsetzung haelt mit Messwerten zur Ownerentscheidung an.

## Head- und Transaktionsvertrag

### Zweck

`rh0` ist ein kleiner laufbezogener Head-/Transaktionsrecord. Er ist weder ein
allgemeines Transaktionsframework noch die Konfigurations-`Pending`-/`Intent`-
Semantik aus #16/#56/#57.

Er besitzt zwei Zustandsformen:

```text
Committed
Prepared
```

`Committed` referenziert:

- den aktuellen bestaetigten Kontrollpunkt;
- optional den vorherigen bestaetigten Rueckfallkontrollpunkt.

`Prepared` referenziert:

- den bisher bestaetigten aktuellen und optionalen Rueckfallkontrollpunkt;
- den vollstaendig vorab kodierten Zielkontrollpunkt;
- Mutationsart;
- optionale `CommandId`;
- erwartete alte und neue Fachrevisionen;
- Zielslot, Zielrevision, Payloadlaenge und Payload-CRC.

Jede Referenz bindet mindestens:

- Slot-ID;
- Kontrollpunktrevision aus `Envelope.versionValue`;
- Schema-Version;
- StorageEpoch;
- Payloadlaenge;
- Payload-CRC.

### Mutationsprotokoll

Fuer eine bereits vollstaendig entschiedene laufwirksame Mutation gilt:

```text
1. Zielzustand aus bestehender CommandDecision oder TransitionDecision bilden.
2. Zielkontrollpunkt vollstaendig kodieren und validieren.
3. Prepared-Head mit altem bestaetigtem Stand und exakter Zielbindung schreiben.
4. Bei CommitOutcomeUnknown den Prepared-Head exakt zuruecklesen.
5. Zielkontrollpunkt in den inaktiven Slot schreiben.
6. Bei CommitOutcomeUnknown den Zielrecord exakt zuruecklesen.
7. Committed-Head mit Ziel als aktuell und bisher aktuell als Rueckfall schreiben.
8. Bei CommitOutcomeUnknown den Committed-Head exakt zuruecklesen.
9. Erst danach die vorhandene apply-Funktion auf den RAM-Zustand anwenden.
```

Kein spaeterer Schritt wird begonnen, solange der vorherige Persistenzstand
nicht eindeutig bestaetigt ist.

### Periodischer Kontrollpunkt

Ein periodischer Kontrollpunkt veraendert keine Fachentscheidung und braucht
keinen `Prepared`-Zustand:

```text
1. aktuellen gueltigen RAM-Zustand kodieren;
2. inaktiven Kontrollpunktslot bestaetigt schreiben;
3. Committed-Head bestaetigt auf neuen aktuellen und alten Rueckfall umstellen.
```

Scheitert der Headcommit, bleibt der neue Slot ein nicht referenzierter
technischer Kandidat und wird nicht als bestaetigter aktueller Stand verwendet.

### Unterbrechungsfaelle

- Unterbrechung vor bestaetigtem `Prepared`: alter Committed-Head bleibt
  massgeblich.
- `Prepared`, Ziel fehlt oder passt nicht: Mutation ist nicht bestaetigt; alter
  Stand bleibt referenziert, der Befund wird sichtbar geliefert.
- `Prepared`, Ziel passt, aber Committed-Head fehlt: Ziel ist technisch
  vorhanden, aber die Mutation ist nicht committed; keine stille Anwendung.
- Committed-Head passt zum Ziel: neuer Zustand ist bestaetigt, auch wenn der
  RAM-Apply vor dem Stromausfall nicht mehr ausgefuehrt wurde.
- Head unlesbar, korrupt oder widerspruechlich: kein Raten aus der hoechsten
  Slotrevision; Ergebnis ist nicht eindeutig rekonstruierbar.
- aktueller Kontrollpunkt ungueltig, Rueckfallreferenz vollstaendig gueltig:
  typisierter Rueckfall auf den referenzierten aelteren Stand.
- aktueller Tombstone ungueltig: kein Rueckfall auf einen alten aktiven Lauf;
  der Zustand ist nicht eindeutig rekonstruierbar.

Die Zuordnung dieser Befunde zu Recovery, `SAFE_BOOT`, Fault oder Aktorsperre
bleibt #18/#24.

## Anwendungsintegration innerhalb `fermentation_app`

### Erforderlicher Coordinator

#17 implementiert einen kleinen `RunPersistenceCoordinator` als
Produktionsbaustein in `fermentation_app`.

Seine Aufgabe ist ausschliesslich:

```text
bereits vorhandene Fachentscheidung entgegennehmen
-> Zielkontrollpunkt bilden
-> Head-/Slotprotokoll ausfuehren
-> erst nach bestaetigtem Commit vorhandene apply-Funktion aufrufen
-> typisiertes Ergebnis zurueckgeben
```

Er verwendet:

- `CommandDecision` und `applyRunCommand()` aus #15;
- `TransitionDecision` und `applyProcessTransition()` aus #14;
- den neuen Laufpersistenzstore;
- keinen konkreten Adapter und keine Hardware.

Er berechnet keine Kommando-, Prozess-, Recovery-, Sensor- oder
Sicherheitsentscheidung erneut. Die vorhandenen `decide*`-Funktionen bleiben
die einzige Quelle der Fachentscheidung.

Der Coordinator besitzt getrennte Pfade fuer:

- persistierte `CommandDecision`;
- persistierte `TransitionDecision`;
- periodischen Kontrollpunkt ohne neue Fachmutation;
- expliziten `NoActiveRun`-Tombstone.

Nicht vorgeschlagene oder fachlich abgelehnte Decisions werden nicht
persistiert und nicht angewendet.

### Noch keine Runtime-Verkabelung

#17 aendert nicht automatisch:

- `FermentationApplication`;
- `IPlatformServices`;
- `src/main.cpp`;
- `main/app_main.cpp`;
- `device_platform_esp_idf`.

Der Coordinator ist Produktionscode und vollstaendig nativ getestet, besitzt
aber in #17 noch keinen realen Composition-Root-, UI- oder NVS-Aufrufer.

Jeder spaetere Produktionspfad fuer Laufkommandos oder Prozessuebergaenge muss
den Coordinator verwenden. Ein spaeterer direkter Aufruf von
`applyRunCommand()` oder `applyProcessTransition()` in einem Runtime-, UI- oder
Adapterpfad waere eine Architekturverletzung. Reine Domain-Unit-Tests duerfen
die apply-Funktionen weiterhin direkt pruefen.

## Ressourcen- und Integrationsgate aus #15

PR #53 hat vor der ersten realen produktiven Nutzung ein verbindliches
ESP32-Messgate hinterlassen. Die damalige Messung erfolgte noch vor dem
heutigen ESP-IDF-Produktionspfad und belegte auf dem damaligen Xtensa-ABI etwa:

```text
RunCommandState: 4200 Byte
CommandDecision: 8608 Byte
```

Diese Zahlen sind Risikoindikatoren, kein heutiger Freigabenachweis.

Da #17 den Coordinator noch nicht in die Runtime-Composition einbindet, ist das
Hardware-Messgate kein stillschweigend erfuelltes Mergekriterium fuer den
reinen Bibliotheksbaustein. Es bleibt jedoch eine harte Sperre vor der ersten
spaeteren Runtime-, UI- oder Composition-Root-Aktivierung.

Der spaetere Aktivierungs-PR muss auf ESP-IDF 6.0.2 ohne PSRAM mit maximalem
48/96/1024-Programmkandidaten messen:

- Task-Stack-High-Water-Mark fuer jeden Kommandoweg;
- freien Heap und groessten zusammenhaengenden Block vor, waehrend und nach
  Decision-Erzeugung, Persistenz und Anwendung;
- Verhalten bei fehlgeschlagener Allokation ohne Teilmutation oder
  Aktorfreigabe.

Erst anhand dieser Messung entscheidet der Owner, ob die vollstaendigen
`CommandDecision`-Schnappschuesse bleiben oder ein separater Delta-Decision-
Umbau erforderlich ist. #17 darf diesen Umbau nicht vorsorglich einziehen.

In #17 selbst werden dokumentiert:

- `sizeof` der betroffenen Typen im aktuellen nativen und ESP-IDF-Build;
- maximale kodierte Payload- und Envelopegroesse;
- native Peakallokation des Codecs/Stores;
- statische RAM-/Flashdeltas beider ESP-IDF-Profile.

## Geplanter Modul- und Dateischnitt

### `lib/device_platform`

Keine Aenderung geplant. Ein neuer Port oder eine neue generische
Transaktionsschnittstelle waere eine materielle Planabweichung.

### `lib/device_platform_test_support`

Keine Produktionsaenderung geplant. Der bestehende
`SimulatedPersistentStateStore` wird nur aus Tests verwendet.

### `lib/fermentation_app`

Neue Dateien:

- `run_persistence_limits.hpp`
  - statische Groessen-, Slot-, Revisions- und Intervallgrenzen;
- `run_persistence_contract.hpp`
  - RecordTypeIds, Schemawerte, Keys und stabile Wirekennungen;
- `run_checkpoint.hpp/.cpp`
  - Kontrollpunktvarianten, Validierung und Gleichheit;
- `run_checkpoint_codec.hpp/.cpp`
  - Schema-1-Payloadcodec;
- `run_persistence_head.hpp/.cpp`
  - Committed-/Prepared-Modell und Validierung;
- `run_persistence_head_codec.hpp/.cpp`
  - kleiner Headpayloadcodec;
- `run_checkpoint_store.hpp/.cpp`
  - Slotscan, Referenzvalidierung, Write-/Readback- und Ladeprotokoll;
- `run_checkpoint_schedule.hpp/.cpp`
  - reine Ereignis-/Intervallfaelligkeit ohne Uhr-, Sensor- oder Aktorport;
- `run_persistence_coordinator.hpp/.cpp`
  - `decide -> persist -> apply`-Koordination.

Bestehende Dateien nur bei belegtem Bedarf:

- `configuration_document_codec.*`
  - kleine bytekompatible Extraktion der bereits vorhandenen
    Einzelprogrammkodierung;
- `run_snapshot.*`, `run_commands.*`, `process_state_machine.*`
  - ausschliesslich schmale read-only Validierungs-/Vergleichshilfen oder
    Zugriff auf bereits vorhandene Werte;
  - keine neue Fach-, Recovery- oder Hardwaresemantik.

`FermentationApplication`, die Composition Roots und Plattformadapter bleiben
unveraendert.

## Ergebnis- und Fehlervertraege

Die Implementierung verwendet getrennte typisierte Ergebnisse fuer:

- Encode-/Decode- und Validierungsfehler;
- Slot-/Head-Lese- und Schreibfehler;
- CapacityError;
- CommitOutcomeUnknown, exact-readback bestaetigt oder nicht bestaetigt;
- kein persistierter Lauf;
- gueltiger aktueller Lauf;
- gueltiger `NoActiveRun`-Tombstone;
- gueltiger Rueckfall;
- Prepared ohne bestaetigten Zielcommit;
- Head-/Referenz-/Schema-/Epoch-/CRC-Widerspruch;
- nicht eindeutig rekonstruierbarer Zustand;
- fachlich abgelehnte Decision;
- Persistenz bestaetigt und RAM-Apply erfolgreich;
- Persistenz bestaetigt, RAM-Apply wegen Vertragsverletzung abgelehnt.

Der letzte Fall darf bei gueltigen, unveraenderten Decisions nicht auftreten
und wird als interner Vertragsfehler sichtbar getestet. Er erzeugt in #17
selbst keine Aktor- oder `SAFE_BOOT`-Entscheidung.

## Teststrategie

### Codec und Wireformat

- Golden Bytes fuer Checkpoint, Head Committed und Head Prepared;
- beide Laufvarianten und Tombstone;
- maximale String-, Program-, Revisions- und Intervallgrenzen;
- unbekannte Varianten und Enums;
- Version 0, unbekannte Schema-Version und fremde RecordTypeId;
- Trunkierung, Zusatzbytes, falsche Laengen und CRC;
- negative Null, NaN und Unendlichkeit ueber bestehende Double-Codecs;
- ProgramDocument-Extraktion bleibt bytekompatibel zum ProgramCatalog-Codec;
- maximal gueltiger Payload bleibt <= 8192 Byte.

### Slot-, Head- und Stromunterbruchsmatrix

- leerer Speicher;
- erster Laufstart ohne bisherigen Kontrollpunkt;
- zwei aufeinanderfolgende Kontrollpunkte und Rotation;
- aktueller korrupt, Rueckfall gueltig;
- beide ungueltig;
- Head NotFound, ReadError, CapacityError und Korruption;
- fremde StorageEpoch oder Schema-Version;
- Prepared vor Zielschreiben;
- Stromausfall nach Prepared, vor Zielcommit;
- Zielcommit unbekannt, Readback alt beziehungsweise neu;
- Stromausfall nach Ziel, vor Committed-Head;
- Committed-Head unbekannt, Readback alt beziehungsweise neu;
- Stromausfall nach Committed-Head, vor RAM-Apply;
- periodischer orphaned Slot bei fehlgeschlagenem Headcommit;
- Tombstone verhindert Wiederbelebung eines alten aktiven Laufs;
- keine Teil- oder Mischrecords nach Neustart.

### Coordinator

Fuer jeden laufwirksamen Kommandopfad aus #15 mindestens:

- Programmstart;
- manueller Start;
- Abbruch und Ausschalten;
- Abbruch und Kuehlen;
- Abschlussquittierung;
- Kuehlen nach Abschluss;
- Laufanpassung.

Zusaetzlich relevante Prozessuebergaenge aus #14.

Je Pfad:

- fachliche Ablehnung erzeugt keinen Storewrite;
- Prepared-Fehler verhindert Zielwrite und Apply;
- Zielwrite-Fehler verhindert Headcommit und Apply;
- Headcommit-Fehler verhindert Apply;
- vollstaendig bestaetigter Commit wendet genau einmal an;
- Wiederholung nach Neustart liefert den bestaetigten Zustand und keine
  doppelte Laufmutation;
- Apply-Vertragsfehler nach Persistenz bleibt sichtbar und wird nicht
  geglaettet.

### Intervall und Schreiblast

- 1, 5 und 60 Minuten;
- 0 und >60 werden abgelehnt;
- rueckwaertige monotone Zeit wird abgelehnt;
- Ereigniswrite erfolgt sofort;
- wiederholte Aufrufe vor Faelligkeit schreiben nicht;
- kein Schreiben im Zwei-Sekunden-Sensorzyklus;
- Ereigniswrite setzt die naechste periodische Faelligkeit nachvollziehbar.

### Regression und Qualitaet

- kompletter nativer Testsatz;
- beide ESP-IDF-Profile mit ESP-IDF 6.0.2;
- Format, clang-tidy, Architektur-, Secret- und Quality-Gates;
- `git diff --check`;
- Ressourcenbericht fuer Base und Head;
- keine Abhaengigkeit von `fermentation_app` auf
  `device_platform_test_support`;
- keine Arduino-, NVS- oder ESP-IDF-Header in #17-Produktionsdateien.

## Dokumentation nach freigegebener Implementierung

Nur nach tatsaechlichem Diff anzupassen:

- `docs/RUN_PERSISTENCE.md` fuer den implementierten Schema-, Head-, Slot- und
  Fehlervertrag;
- `docs/IMPLEMENTATION_ISSUES.md` fuer den realen Umsetzungsstand;
- `CHANGELOG.md` fuer die gelieferte Funktion und Nachweise.

Keine dieser Dateien wird in der Planungsphase geaendert.

## Umsetzungsreihenfolge und Commitschnitt

Nach Ownerfreigabe bleibt die Umsetzung im selben Draft-PR:

1. Verträge, Grenzen, Kontrollpunkt-/Headmodelle und ProgramDocument-Codec-
   Wiederverwendung mit Regressionstests;
2. Checkpoint- und Headcodec mit Golden-, Grenz- und Negativtests;
3. Zwei-Slot-/Headstore mit Referenz-, Readback-, Cut-Point- und
   Rueckfalltests;
4. `RunPersistenceCoordinator` und persistierte Kommando-/Transitionsatomaritaet;
5. Ereignis-/Intervallscheduler und Schreiblasttests;
6. Dokumentation, Ressourcenbericht und vollstaendige Quality Gates.

Nach jedem Commit wird der exakte Scope gegen diesen Plan geprueft. Neue Ports,
Adapter, Abhaengigkeiten, Composition-Root-Arbeit, ein Delta-Decision-Umbau oder
mehr als zwei Kontrollpunktslots verlangen vorab einen neuen Plancommit und
eine neue Ownerfreigabe.

## Offene Mess- und Folgegates

```text
TBD_IMPLEMENTATION_BUDGET
```

- exakte tatsaechliche Payload-, Heap-, RAM- und Flashwerte waehrend der
  Implementierung messen;
- statische Obergrenzen dieses Plans nicht ohne Ownerentscheid anheben.

```text
MEASUREMENT_REQUIRED_BEFORE_RUNTIME_ACTIVATION
```

- reales Stack-/Heap-/Allokationsgate aus PR #53 unter ESP-IDF 6.0.2;
- muss der spaetere erste Runtime-/UI-/Composition-Root-Aufrufer erfuellen.

```text
MEASUREMENT_REQUIRED
```

- reale NVS-Kapazitaet, Atomizitaet, Schreibdauer, Wear-Leveling, Jitter und
  Watchdogwirkung im spaeteren Adapter-/Hardwarepfad.

```text
SPIKE_REQUIRED
```

- reale Stromunterbrueche mit produktivem NVS-Adapter.

```text
TBD_COMMISSIONING
```

- konkrete Serviceeinstellung innerhalb 1..60 Minuten;
- Zeit-/Thermikgrenzen aus #18.

Es verbleibt keine offene Slotanzahlentscheidung. Schema 1 verwendet zwei
logische Kontrollpunktslots.

## SOLID-, DRY- und KISS-Bewertung

| Prinzip | Planbewertung |
| --- | --- |
| Single Responsibility | Kontrollpunktmodell, Codecs, Storeprotokoll, Scheduler und Coordinator besitzen getrennte Aufgaben. |
| Open/Closed | Neue Backends und spaetere Recoveryregeln werden ueber bestehende Ports beziehungsweise neue Konsumenten ergaenzt, ohne Schema-1-Fachlogik an ESP-IDF zu koppeln. |
| Liskov Substitution | Jeder `IStateStore`, der den dokumentierten Replace-/Statusvertrag erfuellt, ist austauschbar. |
| Interface Segregation | Kein breites `IPlatformServices`- oder Persistenz-Sammelinterface wird eingefuehrt. |
| Dependency Inversion | `fermentation_app` haengt nur von `device_platform`-Abstraktionen ab; NVS und ESP-IDF bleiben Adapterdetails. |
| DRY | Envelope, CRC, Bytecodecs, Slotscan, ProgramDocument-Wirelogik sowie bestehende `decide*`, `restore()` und `apply*`-Funktionen werden wiederverwendet. |
| KISS | Zwei Kontrollpunktslots, ein kleiner Headrecord und ein Coordinator genuegen; keine Datenbank, kein Journal, kein Event-Sourcing und keine Zukunftsmodelle. |

Eine Vereinfachung darf den Persistenz-vor-Anwendung-Vertrag, den Tombstone,
die Rueckfallreferenz oder das Ressourcen-/Safetygate nicht entfernen.

## Abnahmekriterien

Der spaetere Implementierungsstand ist erst abnahmefaehig, wenn:

- ein aktiver Programm- und Manuallauf aus einem gueltigen Kontrollpunkt
  rekonstruiert werden kann;
- ein bestaetigter Tombstone keinen alten aktiven Lauf wiederbelebt;
- der aktuelle korrupte Kontrollpunkt nur auf die explizit referenzierte
  gueltige Rueckfallrevision faellt;
- ein Head-/Referenz-/Transaktionswiderspruch als nicht eindeutig
  rekonstruierbar gemeldet wird;
- keine fachliche Mutation vor bestaetigtem Committed-Head angewendet wird;
- `CommitOutcomeUnknown` durch exakten Readback aufgeloest wird;
- keine direkte GPIO-, Aktor-, Sensor-, NVS- oder ESP-IDF-Semantik in #17
  enthalten ist;
- periodische und sofortige Writes die Intervallregeln einhalten;
- Schema 1 keine Modelle aus #18/#20/#21/#24 vorwegnimmt;
- alle nativen Tests, beide ESP-IDF-Builds und Quality Gates bestanden sind;
- Ressourcenwirkung dokumentiert ist;
- der PR Draft bleibt und nicht durch den Agenten gemergt wird.

## Plan-Selbstpruefung

```text
ARCHITECTURE_ALIGNMENT: PASS
ISSUE_BOUNDARIES: PASS
RUN_TRANSACTION_CONTRACT: PASS
SCHEMA_1_SCOPE: PASS
APPLICATION_INTEGRATION: PASS
TWO_SLOT_CONTRACT: PASS
ESP_IDF_BOUNDARY: PASS
RESOURCE_GATE: PASS_AS_FUTURE_RUNTIME_BLOCKER
SOLID: PASS
DRY: PASS
KISS: PASS
IMPLEMENTATION: NOT_STARTED
OWNER_PLAN_APPROVAL: REQUIRED
```
