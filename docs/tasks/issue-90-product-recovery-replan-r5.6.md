# Issue #90 – Persistenz auf den Release-1-Produktvertrag zurückführen (R5.6)

## Status, Zweck und Owner-Gate

Diese Datei ist eine vollständige eigenständige Planrevision R5.6 für PR #117.
Sie setzt die verbindliche Ownerentscheidung zum realen Release-1-
Produktziel um. Sie setzt weder R5.5 um noch setzt sie deren LittleFS-
Untersuchungspfad fort.

Bis zur Freigabe der exakten Plan-Commit-SHA gilt:

`PRODUCT_RECOVERY_REPLAN_PENDING_R5_6_PLAN_APPROVAL`

Verifizierte Baseline dieser Planrevision:

- PR: #117, OPEN und Draft;
- Branch: `agent/issue-90-nvs-adapter-plan`;
- aktueller PR-Head vor R5.6: `1dfa7eb390ededec4dc58efee7138d3fffdd39ff`;
- Base-Branch: `agent/issue-29-esp32-bringup-plan`;
- Base-SHA: `30fa0a8264e2c4564d324340c6bebc204147f477`;
- R5.5, nicht ownerfreigegeben und nicht umzusetzen:
  `docs/tasks/issue-90-architecture-replan-r5.5.md @
  1dfa7eb390ededec4dc58efee7138d3fffdd39ff`;
- vorheriger ownerfreigegebener und am definierten Herstellerblocker
  angelangter R5.4-Plan:
  `docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md @
  c35ce0342898f0e19d3cce5e6a7eaa077f73bad6`;
- Phase-A.1-Herstellerbericht:
  `docs/ISSUE_90_MANUFACTURER_ANALYSIS.md @
  440180f5b712a1eda427292207ebc7e9861cd284`;
- produktive ESP-IDF-Baseline: `v6.0.2 @
  7101770dc6db2667b3c477cc31365dd1acd6db4e`.

R5.4 bleibt unverändert als historische ownerfreigegebene Blockerrevision.
R5.5 bleibt unverändert als nicht ownerfreigegebene, nicht normative
Architekturrevision. R5.6 ist eine neue Planwahrheit und wird erst nach
seinem eigenen Owner-Gate normativ.

## Ziel und Nichtziele

R5.6 plant die Rückführung von Issue #90 auf den tatsächlichen Release-1-
Produktvertrag:

- zuverlässig booten nach Stromausfall;
- beschädigte, unvollständige, widersprüchliche oder nicht rekonstruierbare
  Records niemals still aktivieren;
- gültige Konfigurationsgenerationen und Run-Checkpoints best effort nutzen;
- bei fehlender eindeutiger Rekonstruktion sicher, diagnostizierbar und
  bedienbar in einen Recovery-/Setup-Zustand gelangen;
- Aktoren aus unklarer Persistenzlage gesperrt halten;
- NVS als Default behalten, sofern dieser reale Produktvertrag mit der
  vorhandenen Recoveryarchitektur erfüllt wird;
- nur tatsächlich rote Produkt-Recoveryfälle als spätere Implementierung
  zulassen.

R5.6 ist plan-only. In dieser Runde werden nicht ausgeführt:

- Produktionscode-, `IStateStore`-API- oder NVS-Adapteränderungen;
- Test-, Oracle-, Runner-, Harness- oder Fault-Injection-Änderungen;
- Dependency-, Backend-, Partition- oder ESP-IDF-Pin-Änderungen;
- LittleFS-, FlashDB-, FatFs-, SPIFFS- oder eigener Flash-Store-Vergleich;
- ADR-016 oder kanonische Persistenzdokumente materiell ändern;
- Host-, ESP-IDF-, Hardware- oder Power-Cut-Tests;
- R5.5 umsetzen;
- PR Ready setzen, mergen, Auto-Merge aktivieren, Issue schließen,
  Branch löschen oder Force-Push.

## Verbindliche Release-1-Produktentscheidung

Release 1 ist kein hochverfügbarer Transaktionscontroller. Das Gerät ist ein
eigener Fermenter des Owners, kein Medizinprodukt, kein Safety-PLC und kein
System mit einer Forderung nach unterbrechungsfreier oder 100-prozentiger
Verfügbarkeit.

Nach einem Stromausfall muss die Firmware zuverlässig booten und danach in
einen benutzbaren und diagnostizierbaren Zustand gelangen. Persistente Daten
werden nur verwendet, wenn sie auf der jeweils zuständigen Ebene vollständig
validiert sind. Beschädigte oder nicht eindeutig rekonstruierbare Daten werden
nie still aktiviert. Aktoren bleiben aus beziehungsweise gesperrt, bis die
produktiven Voraussetzungen und das Safety-Gate wieder gültig sind.

Ein laufendes Programm soll nach Möglichkeit fortgesetzt werden. Automatisches
Resume ist Best Effort und keine Safety- oder Hochverfügbarkeitsgarantie. Ein
nicht eindeutig und plausibel wiederherstellbarer Lauf darf kontrolliert nicht
automatisch fortgesetzt, verworfen oder als Recovery-required gemeldet werden.
Der Benutzer muss diesen Zustand erkennen und das Gerät sicher bedienen,
reparieren oder neu konfigurieren können.

Zulässig sind:

- Verlust des gerade laufenden Schreibvorgangs;
- Verlust des neuesten noch nicht sicher aktivierten Konfigurationsstands;
- Verlust des jüngsten Checkpoints;
- Abbruch eines einzelnen Fermentations-/Joghurtlaufs nach einem ungünstigen
  Stromausfall.

Nicht zulässig sind:

- Brick oder Bootloop durch beschädigte Nutzdaten;
- stille Aktivierung beschädigter Konfiguration;
- stille Verwendung eines Teil-, Misch- oder Fremdrecords;
- unkontrollierte Aktorfreigabe aus unklarer Recoverylage;
- falsche Behauptung eines erfolgreich wiederhergestellten Laufs;
- heimlicher Reset oder Factory-New-Annahme, nur um einen unklaren Zustand zu
  verdecken.

Die folgende Forderung ist ausdrücklich **keine** Release-1-Produkt-
anforderung:

```text
existing key
-> same-key overwrite
-> power cut at any internal backend callback
-> key must always be exactly OLD or NEW
```

Der bestätigte Callback-12-/`NotFound`-Befund bleibt technisch gültige
Backendcharakterisierung. Ein nach Power-Cut verlorener oder `NotFound`
gewordener gerade geschriebener Record ist ein möglicher Backend-/Record-
verlust, aber nach dieser Ownerentscheidung nicht allein ein Produktblocker.

## Drei Vertragsebenen

### Ebene A – physisches Backend

Das Backend, voraussichtlich weiterhin ESP-IDF NVS, muss:

- Fehler und `CommitOutcomeUnknown` sauber melden;
- nach erfolgreichem Write/Commit den geschriebenen Wert bei nachfolgendem
  Lesen vollständig liefern;
- keine vom Adapter erfundenen Erfolgsmeldungen erzeugen;
- vorhandene Integritäts-, Flash- und Wear-Mechanismen nutzen;
- keine unnötige Eigenimplementierung von NVS-internem Flash-/GC-Verhalten
  hinzufügen.

Nach einem echten Stromausfall darf ein in Bearbeitung befindlicher Record
alt, neu, fehlend oder unlesbar/fehlerhaft sein. Keiner dieser Zustände ist
für sich ein erfolgreicher Record. Die Backendebene muss den Zustand nur
korrekt und unterscheidbar an die Persistenzebene weitergeben.

### Ebene B – Persistenz- und Recoveryarchitektur

Diese Ebene validiert vollständige Records mit vorhandenen Envelope-, CRC-,
Schema- und `StorageEpoch`-Verträgen. Sie verwendet Konfigurations-
generationen, Manifest, Root/Fallback sowie `rc0`/`rc1`/`rh0`. Sie erkennt
verlorene oder ungültige Records, aktiviert keinen beschädigten Record und
entscheidet zwischen gültigem neuem Zustand, gültigem altem/Fallbackzustand
und Recovery-required.

### Ebene C – Produktverhalten

Das Produkt bootet nach Power-Cut, hält Aktoren sicher, macht lokale Bedienung
und Diagnose erreichbar, fällt bei Bedarf auf die letzte vollständig gültige
Konfiguration zurück und bietet ein Resume nur für einen eindeutig gültigen
Run-Zustand an. Andernfalls wird der Run sicher nicht automatisch fortgesetzt;
das Gerät bleibt trotzdem für Diagnose, Reparatur, Reset und neue Bedienung
verfügbar, soweit die Konfigurationsebene dies sicher zulässt.

## Neuer generischer `IStateStore`-Vertrag

### Beizubehaltende öffentliche Typen und Grenzen

Die vorhandenen getrennten Statusfamilien bleiben zunächst bestehen:

- `StateStoreWriteStatus::Success`;
- `WriteError`;
- `CapacityError`;
- `CommitOutcomeUnknown`;
- `StateStoreReadStatus::Success`;
- `NotFound`;
- `ReadError`;
- `CapacityError`.

`Success` bedeutet weiterhin einen vom Backend bestätigten erfolgreichen
Write/Commit, nicht einen geratenen Erfolg. Der Adapter muss den Wert danach
vollständig lesbar liefern; die konkrete fachliche Gültigkeit wird nicht in
der generischen Portschicht erfunden.

`WriteError` und `CapacityError` bleiben von `CommitOutcomeUnknown` getrennt.
`CommitOutcomeUnknown` bleibt unbekannt und wird niemals als Erfolg geraten.
`read()` liefert weiterhin vollständige Bytes nur bei `Success`; alle anderen
Read-Status liefern keinen verwendbaren Wert.

Keys, `maxBytes`, Binärsicherheit, Anwendungsneutralität und die Trennung von
`device_platform` und ESP-IDF-Adapter bleiben unverändert. Es gibt keine
NVS-, ESP-IDF- oder Fermentationsdetails in der generischen Port-API.

### Präzisierung nach unbekanntem Commit

Nach `CommitOutcomeUnknown` darf Readback bei einem zuvor vorhandenen Record
folgende Ergebnisse liefern:

| Readback | Generische Bedeutung | Zulässige Fachverwendung |
|---|---|---|
| vollständige Bytes, auf höherer Ebene gültig | Record eindeutig verwendbar | neue oder bestehende Generation/Slotreferenz nur nach vollständiger Validierung |
| `NotFound` | `RECORD_OUTCOME_INDETERMINATE_OR_LOST` | kein erfolgreicher alter/neuer Record; höhere Ebene sucht gültigen Fallback |
| `ReadError` | `RECORD_OUTCOME_INDETERMINATE_OR_LOST` | kein erfolgreicher Record; Recovery-required oder fail-closed |
| `CapacityError` | `RECORD_OUTCOME_INDETERMINATE_OR_LOST` | kein erfolgreicher Record; Recovery-required oder fail-closed |
| vollständige Bytes, aber Envelope/CRC/Schema/Epoch/Fachprüfung ungültig | `RECORD_OUTCOME_INDETERMINATE_OR_LOST` | niemals aktivieren; höhere Ebene sucht gültigen Fallback |

Der Ausdruck `RECORD_OUTCOME_INDETERMINATE_OR_LOST` ist zunächst eine
Vertragsklassifikation, kein automatisch neues öffentliches Enum. Vor einer
API-Erweiterung ist zu prüfen, ob die bestehenden Consumerstatus bereits
ausreichen:

- Konfiguration besitzt bereits
  `ConfigurationRecordOutcomeIndeterminate`,
  `ConfigurationCommitIndeterminate` und typisierte Ursachen;
- Run besitzt bereits `RunPersistenceStoreWriteResult::Indeterminate`,
  `RunPersistenceResultStatus::PersistenceIndeterminate`,
  `RunPersistenceLoadStatus::NotReconstructible` und
  `NotReconstructibleOrphanedState`;
- `StateStoreReadStatus` kann die physischen Read-Ergebnisse ohne neue
  Portsemantik ausdrücken.

Die R5.6-Empfehlung ist deshalb: zuerst Dokumentation und vorhandene
Mapping-/Consumerstatus schärfen; nur wenn ein echter Consumerzustand nicht
ausgedrückt werden kann, eine kleine generische Präzisierung planen. Keine
API-Erweiterung nur für sprachliche Schönheit und keine fachliche Bedeutung
im Port.

### Adapter- und Recordgrenze

Der Port entscheidet nicht, ob nach einem verlorenen Record ein alter
Gesamtzustand verfügbar ist. Das entscheidet die zuständige Record-
/Generations-/Slotarchitektur. Ein `Success`-Read ist nur eine Bytefolge;
Envelope, CRC, Schema, Epoch, Referenzen und fachliche Invarianten werden
darüber validiert.

Damit wird die technische Garantie gezielt neu geschnitten, ohne
`CommitOutcomeUnknown` zu verschleiern, Backendfehler zu Erfolgen zu machen
oder die fachliche Fail-Closed-Grenze zu schwächen.

## NVS bleibt die Default-Hypothese

R5.6 ersetzt NVS nicht vorsorglich. Die verbindliche Reihenfolge ist:

1. NVS und die vorhandene Konfigurations-/Run-Recoveryarchitektur gegen den
   neuen Release-1-Produktvertrag prüfen.
2. Bei PASS NVS behalten und keinen alternativen Storage-Stack einführen.
3. Keine generische A/B-, Journal- oder Selector-Schicht ergänzen, solange
   kein konkreter Produktfehler sie verlangt.
4. Nur bei einem konkreten Produkt-FAIL eine neue Adopt-or-build-Planrevision
   öffnen.

LittleFS aus R5.5 ist kein übernommener Untersuchungspfad. FlashDB,
LittleFS, FatFs, SPIFFS und andere Backends werden in R5.6 nicht weiter
verglichen oder eingeführt. Eigenentwicklung bleibt letzte Option. Der
Callback-12-/`NotFound`-Befund bleibt offen sichtbar als
`BACKEND_POWER_CUT_CHARACTERIZATION` beziehungsweise
`KNOWN_BACKEND_LIMITATION`, ist aber nicht alleiniger Release-Blocker.

## Bestandsanalyse der Konfigurationspersistenz

### Vorhandene Records und höhere Mechanik

| Recordfamilie | Keys | Maximaler Storewert | Höhere Produktlogik |
|---|---|---:|---|
| User-Dokument | `uc0..uc3` | 301 B | Dokumentrevision, Manifestreferenz, aktive Generation, Fallback, `StorageEpoch` |
| Service-Dokument | `sc0..sc3` | 45 B | wie User-Dokument; leeres Schema-1-Payload bleibt gültiger Record |
| Programm-Katalog | `pc0..pc3` | 32813 B | vollständig validierter Katalog, Manifestreferenz, Generation, Fallback, `StorageEpoch` |
| Active Manifest | `cm0..cm2` | 149 B | exakte Kombination aller Dokumentrevisionen und gemeinsame Epoch |
| Root | `cr0/cr1` | 114 B | aktiver Root mit genau einer vorherigen Fallbackgeneration |
| Bootstrap | `cb0/cb1` | 42 B | Sequenz/Succession, Status und `StorageEpoch` |

Erhalten bleiben Dokumentrevisionen, `ActiveConfigurationManifest`, RootRecord,
aktive Generation, genau eine vorherige Fallbackgeneration, `StorageEpoch`,
Copy-Migrationen und der bestehende Wire-/Schema-Vertrag.

### Zulässige Konfigurations-Outcome-Menge

Nach jedem relevanten Write-/Commit-/Power-Cut-Fall prüft die höhere Schicht
nicht nur einen Key, sondern den vollständigen fachlich benötigten Graphen.
Die zulässige Produktmenge ist:

```text
A) neueste vollständig gültige Konfiguration
B) letzte vollständig gültige Fallbackgeneration
C) kein sicherer Graph rekonstruierbar
   -> Recovery/SAFE_BOOT/Setup, keine stille Aktoraktivierung
```

Für eine vorbereitete Dokument-, Manifest- oder Bootstrapänderung gilt:

- bleibt der alte kanonische Root unverändert und vollständig gültig, bleibt
  die alte Konfiguration produktiv verwendbar;
- ist der neue Root vollständig gültig und referenziert einen vollständig
  validierten Graphen, darf die neue Generation aktiv werden;
- ist der Zielrecord verloren, fehlt oder ungültig, darf er nicht als Erfolg
  gelten; die alte kanonische Generation oder Fallbackgeneration wird nur
  verwendet, wenn sie vollständig validiert ist;
- sind weder alte noch neue Graphsicht vollständig validierbar, entsteht
  eindeutig Recovery-required/ConfigurationUnavailable beziehungsweise
  ConfigurationCommitIndeterminate;
- teilweise neue plus teilweise alte Dokumente werden nie als Generation
  aktiviert;
- ein beschädigter Programmkatalog wird nie geladen;
- fehlende referenzierte Daten werden nie als Factory-New behandelt;
- ein heimlicher Reset wird nicht als Recovery ausgegeben.

### Root-/Manifest-Unknown

Der bestehende Root-Resolver prüft beide Rootslots und alle für alte/neue
Graphen erforderlichen Records. R5.6 plant zu verifizieren, nicht neu zu
erfinden:

- vollständiger neuer Root plus vollständiger neuer Graph -> neue Generation;
- vollständig alter Root plus vollständiger alter Graph -> alter Zustand;
- alter kanonischer Root trotz verlorenem unreferenziertem Zielrecord -> alte
  Generation bleibt gültig;
- fehlender/fehlerhafter referenzierter Record -> Zielgraph ungültig;
- unvollständiger Scan oder widersprüchliche Sicht ->
  `ConfigurationCommitIndeterminate` beziehungsweise Recovery-required.

Die bestehende `ConfigurationRecoveryStatus::ConfigurationRecordOutcomeIndeterminate`
und die bestehenden Commit-Indeterminate-Ursachen werden wiederverwendet.
Nur wenn die Produktprüfung eine Lücke findet, wird ein kleiner Status- oder
Mappingfix geplant.

### Bootstrap- und Factory-New-Grenze

`cb0/cb1` werden weiter gescannt und über Status, Sequenz und Epoch validiert.
Ein verlorener Zielslot darf die vorherige vollständig gültige Bootstrap-
Sicht nicht still entwerten. `NotFound` bedeutet Factory-New/absent nur,
wenn der fachliche Vertrag den Zustand zuvor als absent belegt. Vorhandene,
aber beschädigte Konfigurationsdaten werden nicht als fabrikneu behandelt.

Die R5.6-Orakel müssen Factory-Empty, Bootstrap-Recovery, aktive Generation,
Fallbackgeneration und beschädigte/fehlende referenzierte Dokumente getrennt
prüfen.

## Bestandsanalyse der Run-Persistenz

### Vorhandene Records und Status

| Recordfamilie | Keys / Größe | Bestehende Mechanik | R5.6-Produktbedeutung |
|---|---|---|---|
| Checkpointslots | `rc0`, `rc1`, je 8240 B; Nutzlast bis 8192 B | Revisionen, alterierende Slots, `rh0`-Referenz, Schema 3, Orphan-/High-Water-Prüfung | jüngster Checkpoint darf verloren gehen; nur vollständig validierter Checkpoint ist nutzbar |
| Persistenzkopf | `rh0`, 256 B | Current-/Fallbackreferenzen, Prepared/Committed-Reihenfolge | kein Kopf oder unklare Referenz errät keinen Run; Recovery/Abort ist zulässig |
| RAM-/Safety-Projektion | kein eigener Storekey | `RunPersistenceLoadStatus`, Coordinator State, `SafetyCore` | Aktoren bleiben gesperrt, Run-Resume nur nach gültiger Produktprojektion |

### Zulässige Run-Outcome-Menge

```text
Power-Cut -> reboot -> letzte eindeutig valide Run-Sicht suchen

wenn eindeutig valide Current-Sicht:
    Best-Effort Resume-Angebot

wenn eindeutig älterer valider Checkpoint:
    Resume von diesem älteren Checkpoint ist zulässig

wenn nicht eindeutig rekonstruierbar:
    kein automatisches Resume
    Aktoren sicher
    Recovery-/Abort-Zustand sichtbar
    Gerät bedienbar, sofern Konfiguration gültig
```

Die zulässigen Ergebnisse sind:

- `NEW_VALID_RESUME` beziehungsweise bestehendes gültiges Current-Resume;
- `OLDER_VALID_CHECKPOINT_RESUME` nur bei explizit validierter Referenz und
  vollständiger Envelope-/CRC-/Schema-/Epoch-/Fachprüfung;
- `RUN_RECOVERY_REQUIRED` oder `RUN_ABORT_REQUIRED` bei unklarer Sicht.

Nie zulässig sind höchste-Revision-gewinnt, Orphan-Activation, Prepared-
Activation, Teilrecord-Akzeptanz oder ein Resume aus einer bloßen
`rc0`/`rc1`-Existenz ohne gültige Kopf-/Referenzsemantik.

### Konkrete bestehende Fälle, die R5.6 prüfen muss

- `RunPersistenceStoreWriteResult::Indeterminate` nach Unknown bleibt kein
  Write-Erfolg.
- `RunPersistenceLoadStatus::Current` darf nur bei vollständiger fachlicher
  Resume-Eignung zu einem Resume-Angebot führen.
- `FallbackRecovered` ist eine vorhandene explizite Statusklasse; aktuell
  muss geprüft werden, ob ihre Projektion über `SafetyCore::classifyRunLoad`
  bereits korrekt best effort resumed oder heute vorsorglich `SafeBoot`
  ergibt.
- `PreparedInterrupted`, `NotReconstructible` und
  `NotReconstructibleOrphanedState` dürfen keinen falschen Resume erzeugen.
- Ein fehlendes `rh0` mit orphanierten, wenn auch technisch gültigen `rc`-
  Records darf nicht durch höchste Revision erraten werden.
- Wenn gültige Konfiguration und UI/Diagnose verfügbar sind, darf ein
  unrekonstruierbarer Run nicht unnötig das gesamte Gerät unbenutzbar machen;
  die aktuelle `SafetyCore`-/`RunLoadDisposition::SafeBoot`-Projektion wird
  gegen dieses Produktziel geprüft.

Die R5.6-Empfehlung ist: vorhandene explizite Fallbackreferenzen nutzen,
keine neue Rekonstruktionsheuristik einführen und bei fehlender Referenz den
Run kontrolliert abbrechen beziehungsweise Recovery-required melden. Eine
kleine Änderung ist nur zulässig, wenn der Bestandstest zeigt, dass heute die
UI/Diagnose unnötig global blockiert wird.

## Safe Boot, Bedienbarkeit und Aktorgrenze

Persistenzfehler dürfen den Fermentationslauf blockieren, aber nicht ohne
Beweis das gesamte Gerät brick- oder bootloop-artig unbenutzbar machen.

R5.6 prüft die bestehende Semantik in dieser Reihenfolge:

1. Firmware und lokale UI/Diagnose starten nach Reboot.
2. Ausgänge bleiben AUS; Safety-Gate bleibt `Unresolved` beziehungsweise
   geschlossen.
3. Configuration-/Run-Status und Fehlergrund sind sichtbar und
   diagnostizierbar.
4. Gültige Konfiguration ermöglicht Setup, Reparatur, Reset oder weitere
   Bedienung über definierte sichere Wege.
5. Ein nicht rekonstruierbarer Run wird nicht automatisch fortgesetzt und
   kann als `NoActiveRun`/Recovery-required kontrolliert beendet werden.
6. Erst nach frischer gültiger Konfigurations-, Sensor-, Persistenz- und
   Safety-Evidenz kann ein expliziter produktiver Start-/Resume-Gate öffnen.

Wenn der heutige globale `SAFE_BOOT` genau diese sichere, weiterhin
bedienbare Recovery bedeutet, bleibt er erhalten. Wenn er nur wegen eines
nicht rekonstruierbaren Runs auch gültige Konfiguration, UI und Setup sperrt,
plant R5.6 eine kleine run-spezifische Produktprojektion. Eine zweite
Recoveryplattform, ein neuer persistenter Safety-Latch oder eine neue
Aktorfreigabelogik sind ausgeschlossen.

## Backend-Charakterisierung und Produktorakel

### Erhaltene Backend-Evidenz

Der vollständige NVS-BDL-Cut bleibt als
`BACKEND_POWER_CUT_CHARACTERIZATION` erhalten. Callback 12 und
`NotFound` bleiben sichtbar und werden als `KNOWN_BACKEND_LIMITATION` oder
gleichwertig klar klassifiziert.

Nicht zulässig ist:

- Testdaten löschen;
- Callback 12 filtern;
- `NotFound` zu „Old“ umetikettieren;
- den bisherigen Backendbefund als exhaustive PASS darstellen.

Der Charakterisierungstest ist aber nicht mehr alleiniger Release-Blocker,
wenn der Produkt-Recoveryvertrag vollständig PASS ist.

### Neues Produkt-Power-Cut-Gate

Für relevante Konfigurations- und Run-Schreibpfade schneiden spätere
owner-gatete Fault-Injection-Tests nach jedem sinnvoll erreichbaren mutierenden
Backend-Cut beziehungsweise nach der bestehenden vollständigen Cutmatrix,
führen einen simulierten Reboot/Reload aus und bewerten die höhere Ebene.

Die technische Detailabdeckung bleibt erhalten; das Orakel bewertet zusätzlich
die reale Produkt-Outcome-Menge.

Konfiguration erlaubt ausschließlich:

- `NEW_VALID_CONFIGURATION`;
- `OLD_VALID_CONFIGURATION` beziehungsweise `FALLBACK_VALID_CONFIGURATION`;
- `CONFIGURATION_RECOVERY_REQUIRED`.

Run erlaubt ausschließlich:

- `NEW_VALID_RESUME`;
- `OLDER_VALID_CHECKPOINT_RESUME`;
- `RUN_RECOVERY_REQUIRED` beziehungsweise `RUN_ABORT_REQUIRED`.

Zusätzlich muss jeder Fall bestätigen:

- kein Partial-/Mixed-/Corrupt-Record akzeptiert;
- kein fehlender referenzierter Record als Factory-New akzeptiert;
- kein Prepared-/Orphan-Record still aktiviert;
- keine unsichere Aktorfreigabe;
- kein falsches Resume-Erfolgssignal;
- Boot und Diagnose erreichbar, soweit die Konfiguration selbst gültig ist.

### Orakelklassifikation

Die Orakel sollen Backend- und Produktergebnis getrennt ausgeben:

```text
backend: SUCCESS | UNKNOWN | NOT_FOUND | READ_ERROR | CAPACITY_ERROR
record:  VALID_NEW | VALID_OLD | VALID_FALLBACK | INVALID | LOST
product: CONFIG_NEW | CONFIG_OLD_OR_FALLBACK | CONFIG_RECOVERY_REQUIRED
         RUN_NEW_RESUME | RUN_OLDER_RESUME | RUN_RECOVERY_REQUIRED
         ACTUATORS_BLOCKED | UI_DIAGNOSTICS_AVAILABLE
```

`backend=NOT_FOUND` nach zuvor vorhandenem Record ist nicht `record=VALID_OLD`.
Die höhere Ebene darf nur aus vollständig validierten Records ein Produkt-
Outcome erzeugen.

## Gezielte reale Release-1-Hardwareverifikation

Nach einem grünen Host-/Softwarevertrag werden keine medizinische oder
interne Callback-Hardwarevollmatrix und keine willkürliche zehnfache
Wiederholung je Backendfenster geplant.

Vorgesehen sind gezielte Szenarien:

1. sauberer Power-Off/Power-On mit gültiger Konfiguration;
2. Stromunterbrechung während eines Konfigurations-Save-Pfads;
3. Stromunterbrechung während eines laufenden Programms beziehungsweise im
   Checkpointzeitraum;
4. Reboot-/Reloadprüfung auf Boot, Aktorzustand, Konfiguration, Fallback oder
   Recovery, Run-Resume oder klaren Nicht-Resume und Bedienbarkeit.

Als Planminimum gilt je Szenario ein sauberer Kontrolllauf und mindestens
drei gezielte Wiederholungen; der konkrete Owner-verifizierte Aufbau und die
letzte Wiederholungszahl werden vor dem Hardware-Slice bestätigt. Eine
präzise externe Power-Cut-Fixture ist kein Release-1-Muss, wenn manuelle
Power-Unterbrechung am sicheren realen Aufbau zusammen mit der Host-Fault-
Injection den Produktvertrag ausreichend belegt. Host-Fault-Injection
ersetzt die reale Boot-/Bedienbarkeitsprüfung nicht.

## Keine alternative Backend-Evaluation in R5.6

Die LittleFS-Bewertung aus R5.5 wird nicht übernommen. Die R5.5-Marktübersicht war
nicht vollständig genug, um LittleFS als einzigen plausiblen Pfad zu
bezeichnen; Embedded-KV-Ansätze wie FlashDB wären in einem späteren Vergleich
ebenfalls zu berücksichtigen. Das ist aber kein Auftrag, FlashDB jetzt zu
untersuchen.

R5.6 entscheidet ausdrücklich:

- NVS zuerst am realen Produktvertrag beweisen;
- solange NVS plus bestehende Recovery PASS ist, keine weitere Evaluation;
- bei einem konkreten Produkt-FAIL eine eigene Adopt-or-build-Revision mit
  vollständigem Komponenten-/Lizenz-/Wartungs-/Power-Loss-/Ressourcenvergleich
  eröffnen;
- Eigenentwicklung bleibt letzte Option.

## Kanonische Dokument- und ADR-Folge

R5.6 ändert diese Dokumente noch nicht materiell. Nach R5.6-Ownerfreigabe und
vor dem jeweiligen Umsetzungsslice sind mindestens folgende Folgeschritte zu
planen:

- `docs/ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md`:
  NVS bleibt Default beziehungsweise gewählt, wenn der Produkt-Recoverybeweis
  PASS ist; NVS-Eintragsatomizität wird nicht als Multi-Page-Same-Key-
  Power-Cut-Garantie behauptet; Backendverlust und Produktrecovery werden
  getrennt.
- generischer `IStateStore`-Vertrag und Statusdokumente: `Success`, Fehler,
  Unknown, Readback und die höhere `RECORD_OUTCOME_INDETERMINATE_OR_LOST`-
  Semantik werden ohne Fermentationsdetails beschrieben.
- `docs/CONFIGURATION_PERSISTENCE.md`: vollständig gültiger neuer Graph,
  gültiger alter/Fallbackgraph oder Recovery-required; keine Mischgeneration,
  kein beschädigter Record, kein stilles Factory-New.
- `docs/RUN_PERSISTENCE.md`: Best-Effort-Resume, explizit validierter älterer
  Checkpoint und kontrollierter Nicht-Resume; keine höchste-Revision-
  Heuristik.
- `docs/SETTINGS_AND_STORAGE.md`: Release-1-Verfügbarkeits- und
  Recoverygrenze ohne Einzel-Key-Hochverfügbarkeitsgarantie.
- `docs/CI_AND_QUALITY_GATES.md`: Backendcharakterisierung als offene
  Limitation und Produkt-Power-Cut-Gate als eigentliches Release-Gate.

Diese Folge darf keine falsche Herstellerbehauptung und keine überzogene
Einzel-Key-Garantie einführen. ADR-016 und die kanonischen Dokumente bleiben
bis zu diesem eigenen Dokumentations-/Owner-Gate unverändert.

## Owner-gatete Umsetzungsslices nach R5.6-Freigabe

Jeder Slice endet mit PASS/FAIL/BLOCKED/NOT_RUN, einem exakten Head und STOP
für den Owner. Kein Slice autorisiert den nächsten automatisch.

### Slice 1 – Vertrags- und ADR-Abgleich

Nur die geplante Dokumentation und eine Prüfung, ob die bestehenden Status
ausreichen. Falls ein kleiner generischer Statusabgleich tatsächlich nötig
ist, wird er in einem eigenen explizit benannten Scope ausgewiesen. Keine
Produktions- oder Teständerung in dieser aktuellen R5.6-Planrunde.

Gate: klarer Vertrag für Backend, Record, Konfiguration, Run, Safety und
Produktverhalten; Entscheidung über etwaige minimale Statuspräzisierung.

### Slice 2 – Produkt-Recovery-Orakel

Backendcharakterisierung behalten, Callback 12 offen sichtbar lassen und die
Konfigurations-/Run-Outcome-Orakel mit simuliertem Reboot definieren und
implementieren. Kein künstlich gelockertes Backendorakel.

Gate: alle zulässigen/unerlaubten Produktoutcomes sind pro Schreibpfad und
Recordfamilie reproduzierbar klassifiziert.

### Slice 3 – Bestehende Recoverylogik gegen das neue Orakel

Zuerst wird der vorhandene Produktionscode gegen die neuen Produktorakel
geprüft. Besonders zu untersuchen sind Konfigurationsfallback, Root-/Graph-
Auflösung, Bootstrap, `FallbackRecovered`, `PreparedInterrupted`, fehlendes
`rh0`, Orphans und die aktuelle `SafetyCore`-Projektion.

Gate: echte rote Produktfälle getrennt von akzeptierten Backendlimitationen;
keine prophylaktische neue Schicht.

### Slice 4 – minimale notwendige Produktionskorrektur

Nur für durch Slice 3 belegte Produkt-Fails:

1. bestehende Generation-/Fallback-/Slotlogik korrigieren;
2. kleine bestehende Status-/Recoveryprojektion korrigieren;
3. erst danach eine neue Abstraktion prüfen.

Kein Backendwechsel, keine neue generische A/B-/Journalarchitektur und keine
Vertragsausweitung ohne neues materielles Owner-Gate.

### Slice 5 – Final Software Verification

Nach Review der Änderungen: relevante Native-Suite, Produkt-Fault-Matrix,
beibehaltene NVS-Backendcharakterisierung, ESP-IDF-Builds, Static Analysis,
Capacity/4-MB, RAM/Stack, Release-Isolation, Architektur-, Lizenz- und
Artefaktgates. Ein Backend-Charakterisierungs-FAIL wird nicht als
Produktvertrag-FAIL und umgekehrt verschleiert.

### Slice 6 – gezieltes reales Board

Gezielte reale NVS-/Reboot-/manuelle Power-Unterbrechungs- und Recoverytests
nach dem Hardwareabschnitt. Nachweise: Boot, sichere Aktoren, gültige
Konfiguration/Fallback/Recovery, Resume oder klarer Nicht-Resume,
Bedienbarkeit und Diagnose.

## Neues Akzeptanzziel für Issue #90

Issue #90 ist erfolgreich, wenn:

1. der ESP-IDF-NVS-Adapter korrekt, generisch und anwendungsneutral integriert
   ist;
2. Backendfehler, Unknown und `NotFound` nicht als falsche Erfolge
   interpretiert werden;
3. Konfigurations-Fault-Injection ausschließlich neue gültige Konfiguration,
   gültigen alten/Fallbackgraph oder Recovery-required erzeugt;
4. Run-Fault-Injection ausschließlich eindeutiges Resume, eindeutig älteres
   gültiges Checkpoint-Resume oder kontrollierten Nicht-Resume erzeugt;
5. kein beschädigter, gemischter, teilweiser, vorbereiteter oder orphanierter
   Record produktiv aktiviert wird;
6. keine unsichere Aktorfreigabe entsteht;
7. Firmware nach realem Power-Cycle bootet und bedienbar/diagnostizierbar
   bleibt;
8. 4-MB-, RAM-/Stack-, Wear-/Write- und Buildbudgets erfüllt sind;
9. Callback 12/`NotFound` als bekannte Backendlimitation offen dokumentiert
   bleibt.

## R5.6-Abnahmekriterien und aktueller Nachweisstatus

Die Planrevision ist vollständig, wenn sie:

- den verbindlichen Release-1-Produktvertrag statt Einzel-Key-Hochverfügbarkeits-
  garantie als Abnahmeziel definiert;
- Backend-, Record-/Recovery- und Produktverträge trennt;
- die bestehende `IStateStore`-Statusfamilie und die minimal erforderliche
  `RECORD_OUTCOME_INDETERMINATE_OR_LOST`-Semantik konkret beschreibt;
- NVS als Default beibehält und alternative Backends nicht vorsorglich öffnet;
- Konfigurations- und Run-Outcome-Mengen mit sicheren Recoverypfaden nennt;
- Bedienbarkeit, Safe Boot und Aktorgrenze prüft;
- Backendcharakterisierung und Produkt-Power-Cut-Orakel trennt;
- gezielte reale Hardwareverifikation statt Callback-Hardwarevollmatrix plant;
- kanonische ADR-/Dokumentationsfolge und kleine Owner-Slices definiert;
- keine Umsetzung in dieser Runde vornimmt.

Für diese Planrunde gilt:

- Live-Kontextanalyse: `PASS`;
- R5.5-Freigabe: `NOT_RUN` / nicht erteilt;
- Implementierung: `NOT_RUN` / nicht freigegeben;
- Testcode-, Oracle-, Runner- und Harnessänderung: `NOT_RUN`;
- Backend-, Dependency-, ESP-IDF-, Partition- und Hardwareänderung:
  `NOT_RUN`, keine Änderung;
- Callback-12-/`NotFound`-Befund: bestehendes `FAIL`, nicht neu ausgeführt;
- R5.6-Planfreigabe: `BLOCKED` bis zur ausdrücklichen Ownerentscheidung über
  die exakte neue Plan-SHA.

## Referenzen

Interne Primärquellen:

- `docs/tasks/issue-90-architecture-replan-r5.5.md`;
- `docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md`;
- `docs/ISSUE_90_MANUFACTURER_ANALYSIS.md`;
- `docs/ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md`;
- `docs/CONFIGURATION_PERSISTENCE.md`;
- `docs/RUN_PERSISTENCE.md`;
- `docs/SETTINGS_AND_STORAGE.md`;
- `docs/CI_AND_QUALITY_GATES.md`;
- `lib/device_platform/src/state_store.hpp`;
- `lib/fermentation_app/src/configuration_recovery_service.hpp`;
- `lib/fermentation_app/src/configuration_service.hpp`;
- `lib/fermentation_app/src/run_persistence_store.hpp`;
- `lib/fermentation_app/src/run_persistence_coordinator.hpp`;
- `lib/fermentation_app/src/safety_core.cpp`.

R5.6 führt keine neue externe Backendquelle als Entscheidungsgrundlage ein.
Die produktive ESP-IDF-Baseline bleibt `v6.0.2 @
7101770dc6db2667b3c477cc31365dd1acd6db4e`.
