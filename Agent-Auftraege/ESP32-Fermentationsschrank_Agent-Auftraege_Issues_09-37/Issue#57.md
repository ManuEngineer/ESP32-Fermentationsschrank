# Agent-Auftrag fuer Issue #57

## Issue

**[E2.1d] Bootstrap, StorageEpoch und Recovery implementieren**

Snapshot-Status: `BLOCKED_DEPENDENCY`

Tracking-Issue: #16

Epic: #4

GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/57

## Abhaengigkeiten und Sperrregel

- #54 – `COMPLETED`
- #55 – `COMPLETED`
- #56 – nach Variante-B-Neuschnitt zu planen, umzusetzen und zu mergen

Solange #56 nicht gemergt und das Live-Issue #57 nicht fuer seinen eigenen
Plan-first-Schritt freigegeben ist: keinen Implementierungsbranch, keinen Code
und keinen PR fuer #57 erstellen. Nach #56 folgt zuerst ein eigener
Plan-Draft-PR und eine commitgebundene Ownerfreigabe.

## Verbindliche Architekturentscheidung

Release 1 implementiert sicheren Bootstrap, `StorageEpoch`, Korruptionssperre
und wiederaufnehmbaren Werksreset auf dem Variante-B-Active-/Fallback-Kern.
Connectivity- und Authentication-Domaenen entstehen erst mit ihrem ersten
realen Konsumenten.

## Ziel

Den Variante-B-Konfigurationsgraphen bei fabrikneuem Speicher sicher
initialisieren, bei Boot eindeutig laden oder fail closed sperren und einen
ausdruecklich ausgeloesten Werksreset nach jedem Stromunterbruch idempotent
fortsetzen. Geraetespezifische Touchkalibrierung bleibt erhalten.

## Verbindlicher Scope

### BootstrapRecord

- zwei redundante `ConfigurationBootstrapRecord`-Slots
- Felder mindestens: `BootstrapSequence`, Speicherformatversion,
  `StorageEpoch` und Zustand
- gespeicherte Zustaende:
  - `Initializing`
  - `Initialized`
  - `Resetting`
- `NotFound` ist kein gespeicherter Zustand
- Lese-, Kapazitaets- und Integritaetsfehler werden nie wie `NotFound`
  behandelt
- BootstrapSequence und `StorageEpoch` beginnen bei 1; 0 ist reserviert und
  Ueberlauf wird vor Writes abgelehnt

### Nachweislich fabrikneuer Speicher

Automatische Initialisierung ist nur erlaubt, wenn:

- alle erforderlichen Reads technisch erfolgreich abgeschlossen wurden;
- alle Bootstrap-, Root-, Manifest- und Konfigurationsslots eindeutig
  `NotFound` melden;
- keine gueltigen, unbekannten, beschaedigten oder unlesbaren Altbytes erkannt
  wurden;
- kein BootstrapRecord existiert.

Readfehler, CRC-Fehler, unbekannte Schemas, ungueltige Roots oder
widerspruechliche Slotzustaende sind niemals fabrikneu.

### Initialisierung unter StorageEpoch 1

1. `Initializing` dauerhaft schreiben und ruecklesen;
2. UserConfiguration Revision 1 mit bestaetigten Factory-Startwerten erzeugen;
3. ServiceConfiguration Revision 1 erzeugen;
4. ProgramCatalog Revision 1 mit vier Factory-Arbeitskopien erzeugen;
5. ConfigurationManifest Generation 1 schreiben und vollstaendig validieren;
6. ConfigurationRootRecord rootSequence 1 mit Active Generation 1 und ohne
   Fallback schreiben, ruecklesen und als Graph validieren;
7. vorbereiteten Factory-`RuntimeConfigurationSnapshot` nach #56 publizieren;
8. Bootstrap auf `Initialized` fortschreiben.

Nach Stromausfall wird aus `Initializing` anhand persistierter Records
idempotent fortgesetzt. Es entstehen keine Connectivity-/Authentication-
Manifeste, Secret-Roots oder Dummyrecords.

### Normaler Boot

- kanonischen BootstrapRecord technisch und fachlich bestimmen
- `StorageEpoch` und Speicherformat validieren
- bei `Initialized` den vollstaendigen Variante-B-Rootgraphen aus #56 laden
- Active, ersatzweise genau einen gueltigen Fallback verwenden
- Fallbacknutzung sichtbar diagnostizieren
- Runtime-Snapshot vor Freigabe vollstaendig vorbereiten
- bei `Initializing` Initialisierung idempotent fortsetzen
- bei `Resetting` Werksreset idempotent fortsetzen
- bei Korruption, unbekanntem Schema, unlesbarem Speicher oder fehlendem
  nutzbarem Graphen keine Runtime freigeben

### StorageEpoch

- jeder R1-Konfigurations- und Bootstraprecord ist an die aktuelle Epoche
  gebunden
- Referenzen ueber Epochen hinweg sind ungueltig
- ein abgeschlossener Werksreset macht alte Epochen logisch unerreichbar
- ohne Plattformnachweis keine sichere physische Loeschung alter Flashbytes
- spaetere reale Connectivity-/Authentication-Domaenen muessen ihre Records
  beim ersten Konsumenten an dieselbe Resetgrenze binden

### Beschaedigte oder unbekannte Daten

- ungueltige, beschaedigte oder unlesbare Daten loesen keinen Factory-
  Bootstrap und keinen automatischen Werksreset aus
- unbekannte oder nicht migrierbare Schemas werden ohne Teilwirkung abgelehnt
- ohne nutzbaren Active/Fallback entsteht ein typisierter
  `ConfigurationUnavailable` beziehungsweise `ConfigurationIntegrityFailure`
- keine Runtime und keine Aktorfreigabe
- systemweite Fehlerklasse, persistente Verriegelung und `SAFE_BOOT` bleiben #24

### Wiederaufnehmbarer Werksreset

Der Reset wird nur durch einen bereits fachlich autorisierten, ausdruecklichen
lokalen Resetauftrag gestartet. UI, PIN-Pruefung, Raw-Touch-Geste, Laufgate,
Historienloeschung und Safetyfreigabe liegen bei ihren zustaendigen Issues.

Persistenter Ablauf:

1. naechste `StorageEpoch` und BootstrapSequence ohne Ueberlauf bestimmen;
2. `Resetting` unter der neuen Epoche dauerhaft schreiben und ruecklesen;
3. damit alle Records alter Epochen logisch unerreichbar machen;
4. neue UserConfiguration, ServiceConfiguration und ProgramCatalog-
   Anfangsrevisionen schreiben und pruefen;
5. neues ConfigurationManifest und neuen Root ohne Fallback schreiben und den
   gesamten Graphen validieren;
6. vorbereiteten Factory-Runtime-Snapshot nach #56 publizieren;
7. Bootstrap als `Initialized` abschliessen.

Nach jedem Cut wird aus `Resetting` idempotent fortgesetzt. Vor dem
Linearisierungspunkt bleibt kein teilweise neuer Graph wirksam; nach dem
Root-Commit bleibt die neue Epoche kanonisch.

Der Reset:

- erhaelt gemaess ADR-010 die geraetespezifische Touchkalibrierung;
- schreibt, loescht oder ueberschreibt keine Touchkalibrierungsrecords;
- erzeugt keine leeren Connectivity-/Authentication-Manifeste oder Secret-
  Roots;
- reaktiviert keine Records einer alten `StorageEpoch`;
- behauptet keine physische Flashloeschung;
- wird nie automatisch durch Korruption ausgeloest.

Ein gesonderter Recoveryfall fuer unbrauchbare Touchkalibrierung bleibt vom
normalen Werksreset getrennt.

### Spaetere reale Connectivity-/Authentication-Domaenen

#57 reserviert keine Keys, Slots, Records, Manifeste, Roots, CredentialEpoch
oder Ports fuer Secrets. Der erste produktive WLAN-, Webpasswort- oder
Service-PIN-Konsument muss in einem eigenen ownerfreigegebenen Plan:

- sein typisiertes Schema und seine Schluessel definieren;
- Records an die aktuelle `StorageEpoch` binden;
- atomaren Commit, Widerruf und fail-closed Recovery festlegen;
- Werksreset- und Cut-Point-Verhalten ergaenzen;
- verhindern, dass alte Epochen oder Credentials reaktiviert werden;
- Secret-, Redaction-, Backup- und Plattformschutzvertraege nachweisen.

### Grenze zu #24

#57 liefert bei nicht verfuegbarer oder beschaedigter Konfiguration nur stabile
typisierte Fehlerdaten und keine Runtimefreigabe. Fehlerklasse, persistente
Verriegelung, Bootprioritaet, `SAFE_BOOT` und reale Aktorsperren bleiben #24.

## Ausdruecklicher Nicht-Scope

- persistentes Pending, PendingRoot oder Aktivierungsintent
- `ConfigurationActivationRunAssessment`
- persistente Previewdaten
- Connectivity-/Authentication-Manifeste oder Secretslots
- Prepared-/Committed-Authentication-Roots
- `CredentialEpoch`, Credentialwechsel, KDF, Sessions oder CSRF
- kombinierte Konfigurations-/Secret-Transaktionen
- Lauf-, Journal- und Historienpersistenz
- physische Resetgeste oder Touchkalibrierungsmathematik
- systemweite Fehler-/Safety-/Aktorlogik
- produktiver NVS-Adapter, Partitionierung oder physische Loeschgarantie

## Architekturgrenzen

- Bootstrap-, Epoch-, Reset- und Konfigurations-Recoverysemantik liegt in
  `fermentation_app`.
- technische Store-, Envelope-, Slot- und Cut-Point-Bausteine bleiben in
  `device_platform`.
- Testadapter bleiben in `device_platform_test_support`.
- `src/main.cpp` bleibt Composition Root.
- es entsteht kein allgemeines Domaenen-, Reset-Plugin- oder Providerframework.

## Verbindliche Tests

### Bootstrap

- vollstaendig leerer simulierter Speicher mit erfolgreichen Reads
- `Initializing` vor erster Dokumentrevision
- Cut vor und nach jedem Write bis `Initialized`
- Wiederaufnahme ohne doppelte oder gemischte aktive Generation
- Readfehler, Altbytes, unbekanntes Schema oder ungueltiger Root verhindern
  automatische Initialisierung
- BootstrapSequence-/StorageEpoch-Ueberlauf wird vor Write abgelehnt

### Boot und Korruption

- normaler `Initialized`-Boot mit Active
- unbrauchbares Active mit gueltigem Fallback
- kein nutzbarer Graph: typisiertes Unavailable und keine Runtime
- rohe Bytekorruption ohne passende CRC
- semantisch ungueltiger, CRC-korrekt kodierter Record
- falsche `StorageEpoch` an jeder Referenzkante
- unbekannte Bootstrap-, Root-, Manifest- und Dokumentversion
- Lese-/Kapazitaetsfehler niemals als `NotFound`
- Korruption startet nie Reset oder Factoryinitialisierung

### Werksreset

- Cut vor und nach jedem persistenten Resetschritt
- Wiederaufnahme von `Resetting` ist idempotent
- alte Active-/Fallback-/Dokumentrecords bleiben logisch unerreichbar
- neue Anfangskonfiguration ist vollstaendig oder noch nicht aktiv, nie gemischt
- Touchkalibrierungs-Sentinel bleibt an jedem Cut und nach Abschluss
  unveraendert
- keine Connectivity-/Authentication-/Secretrecords werden erzeugt
- `StorageEpoch`-Ueberlauf wird fail closed abgelehnt
- Neustart nach Root-Commit und vor `Initialized` behaelt die neue Epoche
- Korruption waehrend Reset fuehrt zu sicherem Unavailable

### Additiver Ausbauvertrag

- unbekannte spaetere Recordtypen oder Schemas ohne Teilwirkung ablehnen
- Schema-1-Variante-B-Daten bleiben deterministisch lesbar
- Copy-Migration veraendert Quelldaten nicht in place
- Testmodell kann spaeter einen neuen epochengebundenen Recordtyp hinzufuegen,
  ohne bestehende Schema-1-Bytes umzudeuten
- keine produktiven Dummy-Secretrecords fuer diesen Nachweis

### Ressourcen und Builds

- vollstaendige Cut-Point-Matrix fuer Bootstrap, Boot und Reset
- begrenzte Record-/Scanpuffer gemaess Vertrag
- Base-/Head-Vergleich fuer Flash, statisches RAM, `firmware.bin` und
  `firmware.elf`
- native Tests und alle drei Buildprofile
- keine reale Heap-, NVS-, Flashatomizitaets- oder
  Flashlebensdauergarantie ohne Messung

## Akzeptanzkriterien

- fabrikneu, `Initializing`, `Initialized`, `Resetting`, korrupt und unbekannt
  sind eindeutig unterscheidbar.
- Nur nachweislich leerer, fehlerfrei lesbarer Speicher wird initialisiert.
- Jeder Cut besitzt ein konkretes fachliches Recovery-Orakel.
- Korruption erzeugt keinen stillen Factory-Fallback oder automatischen Reset.
- Werksreset ist wiederaufnehmbar und behaelt Touchkalibrierung.
- Alte Epochen werden nie reaktiviert.
- Es existieren keine leeren Connectivity-/Authentication-Manifeste,
  Secret-Roots oder vorbereiteten Credentialstrukturen.
- Tests, Buildprofile, Quality Gates und Ressourcenvergleich sind bestanden.

## Git- und PR-Regeln

Nach Merge von #56 fuer #57 zuerst einen eigenen Plan-first-Draft-PR erstellen.
Nach commitgebundener Planfreigabe ausschliesslich #57 bearbeiten. Kleiner
Draft-PR, Dokumentation und `CHANGELOG.md` aktualisieren, `Closes #57` erst im
Umsetzungs-PR. Nicht selbst auf Ready setzen, mergen, Auto-Merge aktivieren,
force-pushen oder Branch loeschen.

## Vorgeschlagener Branch

`feat/issue-57-bootstrap-storageepoch-recovery`
