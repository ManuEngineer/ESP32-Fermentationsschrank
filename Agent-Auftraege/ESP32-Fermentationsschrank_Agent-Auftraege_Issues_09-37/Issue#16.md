# Agent-Auftrag fuer Issue #16

## Issue

**[E2.1] Konfigurationsebenen, Validierung und atomare Revisionen**

Aktueller Snapshot-Status: `PLANNED_SPEC_PENDING`

Epic: #4

GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/16

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das
> Live-Issue vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung,
> kein Ersatz fuer das Issue oder die Spezifikationsquellen.

## Aktueller Stand

Die fachliche und technische Spezifikation ist in
`docs/CONFIGURATION_PERSISTENCE.md` konsolidiert. Der Vertrag ist jedoch zu
gross fuer einen einzelnen Implementierungsbranch und einen kleinen PR.

Issue #16 bleibt deshalb ein Tracking-Issue. Vor der Umsetzung muessen vier
abhaengige Teilissues mit eigenen Agent-Auftraegen, Branches und PRs angelegt
und im INDEX verknuepft werden.

Es darf derzeit kein Implementierungsbranch fuer Issue #16 erstellt werden.

## Verbindliche Reviewkorrekturen

### Kanonische Schutzwurzeln

Nicht jeder physisch noch gueltige alte Root schuetzt seine nur dort
referenzierten Generationen dauerhaft.

Die Schutzmenge besteht ausschliesslich aus:

- Active und Fallback des aktuell kanonisch ausgewaehlten ConfigurationRoot
- Pending des aktuell kanonisch ausgewaehlten PendingRoot
- einem exakt passenden gueltigen Aktivierungsintent samt Pending-Graph
- der gerade ausgefuehrten serialisierten Mutation
- dem aktuell kanonischen committed AuthenticationRoot und der laufenden
  Authentication-Transaktion

Aeltere redundante Roots bleiben technische Bootkandidaten, sind aber keine
zusaetzlichen dauerhaft schuetzenden fachlichen Wurzeln. Dadurch koennen nur
noch von Altroots referenzierte Slots nach erfolgreichem neuen Root-Commit
wiederverwendet werden.

Verbindliche spaetere Tests umfassen mindestens:

- fuenf aufeinanderfolgende Active-Commits
- drei aufeinanderfolgende Pending-Ersetzungen
- Pending verwerfen und erneut erzeugen
- wiederholte Authentication-Rootwechsel
- kein vorzeitiges `NoUnreferencedSlotAvailable`

### Grenze zu Issue #17

Issue #16 beziehungsweise sein zustaendiges Teilissue definiert nur einen
schmalen externen `ConfigurationActivationRunAssessment`-Port mit mindestens:

- `Unknown`
- `NoActiveOrRecoverableRun`
- `ActiveRunPresent`
- `RecoverableRunPresent`

Nur `NoActiveOrRecoverableRun` erlaubt eine Pending-Aktivierung. `Unknown`
blockiert sicher.

Die reale Laufpersistenz und die Erkennung eines wiederherzustellenden Laufs
bleiben Issue #17.

### Grenze zu Issue #24

Issue #16 beziehungsweise sein zustaendiges Teilissue darf bei einem
Konfigurations- oder Publish-Vertragsfehler nur einen typisierten
`ConfigurationSafetyIntent` beziehungsweise
`ConfigurationRuntimeFailure` erzeugen und keine RuntimeConfiguration
freigeben.

Nicht vorwegnehmen:

- systemweite Fehlerklassen
- persistente Verriegelungen
- vollstaendige `SAFE_BOOT`-Politik
- reale Aktor- oder GPIO-Sperren

Diese Semantik bleibt Issue #24.

## Verbindliche Aufteilung vor READY

### Teilpaket A: Plattformpersistenz und Wireformat

- begrenztes binaersicheres `IStateStore`
- starke technische Typen
- Big-Endian-Codecs, CRC-32/ISO-HDLC und Envelope-Version 1
- generische feste Slot- und redundante Recordmechanik
- `ISecureRandomSource` und `ITimeZoneResolver`
- `SimulatedPersistentStateStore` und Golden Tests

### Teilpaket B: Typisierte Konfigurationsdokumente

- UserConfiguration Schema 1
- ServiceConfiguration Schema 1
- ProgramCatalog Schema 1
- ID-, Text-, Anzahl- und Payloadgrenzen
- fachliche Codecs, Validierung und Copy-Migration

### Teilpaket C: Manifeste, Roots, Preview und Runtimeaktivierung

- Active/Fallback/Pending
- korrigierte kanonische Schutzwurzeln und Slotrotation
- Preview, Owner-, Token- und Konfliktsemantik
- RuntimeConfigurationSnapshot und Prepare/Publish
- RunAssessment-Port zu #17
- ConfigurationSafetyIntent zu #24

### Teilpaket D: Bootstrap, Secret-Manifeste, Reset und End-to-End-Recovery

- Bootstrap und StorageEpoch
- Connectivity-/Authentication-Manifeste Schema 1 als `NotProvisioned`
- vorwaertsgerichtete Authentication-Roots
- wiederaufnehmbarer Werksreset
- vollstaendige Cut-Point-, Korruptions- und Ressourcenmatrix

## Auftrag bis zur Aufteilung

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` am Tracking-
Issue #16 nur, wenn der Owner ausdruecklich die Aufteilung in Teilissues
beauftragt hat.

Lies vor jeder Aenderung:
- aktuelles Live-Issue #16
- AGENTS.md und die Modul-AGENTS.md
- docs/SPECIFICATION_REVIEW.md
- docs/CONFIGURATION_PERSISTENCE.md
- docs/SETTINGS_AND_STORAGE.md
- docs/BACKUP_SECURITY_RETENTION.md
- Agent-INDEX

Solange die vier Teilissues nicht angelegt, verknuepft und im INDEX enthalten
sind:
- keinen Implementierungsbranch fuer #16 erstellen
- keinen Produktionscode aendern
- #16 nicht auf READY setzen
- keine weiteren Detailentscheidungsserien starten

Bei ausdruecklichem Auftrag zur Aufteilung:
1. vier kleine abhaengige Teilissues gemaess diesem Auftrag vorbereiten
2. jedem Teilissue klare Scope-, Nicht-Scope-, Tests- und DoD-Grenzen geben
3. Abhaengigkeiten A -> B -> C -> D festlegen, soweit technisch erforderlich
4. pro Teilissue einen eigenen Agent-Auftrag und vorgeschlagenen Branch anlegen
5. INDEX aktualisieren
6. nur das erste tatsaechlich ausfuehrbare Teilissue auf READY setzen
7. #16 als Tracking-Issue offen lassen
8. nach PR-Erstellung anhalten; nicht mergen und keinen Branch loeschen

Nur bei echter fachlicher Ownerentscheidung, Sicherheitswiderspruch,
widerspruechlicher Spezifikation oder nicht implementierbarer Grenze
rueckfragen. Klassennamen, private Datenstrukturen, Dateiaufteilung und
Test-Fixtures sind spaetere Implementierungsdetails.
```

## Spezifikationsquellen

- `docs/CONFIGURATION_PERSISTENCE.md`
- `docs/SETTINGS_AND_STORAGE.md`
- `docs/BACKUP_SECURITY_RETENTION.md`
- `docs/PR38_REVIEW_CORRECTIONS.md`

## Definition of Done fuer das Tracking-Issue

Issue #16 wird erst abgeschlossen, wenn:

- alle Teilissues angelegt und verknuepft wurden
- alle Teilimplementierungen gemergt sind
- die korrigierte Slotrotation und Schutzwurzeldefinition nachgewiesen ist
- die vollstaendige End-to-End-Cut-Point-Matrix besteht
- Produktionsprofile und Quality Gates bestehen
- Ressourcenwirkungen je Teil-PR und abschliessend dokumentiert sind
- reale Hardware-/Adaptermessungen weiterhin als spaetere Gates sichtbar sind

## Vorgeschlagener Branch

Kein Implementierungsbranch, bis die Teilissues angelegt sind.
