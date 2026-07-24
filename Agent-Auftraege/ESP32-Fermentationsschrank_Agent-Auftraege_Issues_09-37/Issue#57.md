# Agent-Auftrag fuer Issue #57

## Issue

**[E2.1d] Bootstrap, Secret-Manifeste und Recovery integrieren**

Aktueller Snapshot-Status: `BLOCKED_DEPENDENCY`

Tracking-Issue: #16

Epic: #4

GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/57

> Dieser Auftrag darf erst ausgefuehrt werden, wenn #54, #55 und #56 gemergt und
> abgeschlossen sind und das Live-Issue #57 auf `READY` steht.

## Sperrregel

Solange eine Abhaengigkeit offen oder #57 nicht `READY` ist:

- keinen Implementierungsbranch erstellen
- keine Produktionsdatei aendern
- keinen PR erstellen
- keine provisorischen Bootstrap-, Secret- oder Resetmodelle parallel einfuehren
- Blocker berichten und anhalten

## Ziel nach Freigabe

Integriere die bisherigen Teilpakete zu einem wiederanlaufbaren Gesamtworkflow:

- BootstrapRecord und automatische Ersteinrichtung
- StorageEpoch
- Connectivity-Manifest Schema 1 als NotProvisioned
- Authentication-Manifest Schema 1 als NotProvisioned
- vorwaertsgerichtete Authentication-Roots
- wiederaufnehmbarer Werksreset
- vollstaendige End-to-End-Cut-Point-, Korruptions- und Ressourcenmatrix

Reale WLAN-, Passwort-, PIN- und Authentifizierungsdaten bleiben #27.

## Vor jeder Arbeit lesen

- Live-Issue #57
- Tracking-Issue #16
- gemergte Ergebnisse von #54, #55 und #56
- `AGENTS.md`
- Modul-AGENTS.md fuer `device_platform`, `device_platform_test_support` und
  `fermentation_app`
- `docs/CONFIGURATION_PERSISTENCE.md`
- `docs/SETTINGS_AND_STORAGE.md`
- `docs/BACKUP_SECURITY_RETENTION.md`
- `docs/PR38_REVIEW_CORRECTIONS.md`
- `docs/CI_AND_QUALITY_GATES.md`
- Agent-INDEX

Berichte vor Codeaenderungen:

1. Bootstrap- und Bootzustandsmodell
2. StorageEpoch-Regeln
3. Connectivity-/Authentication-Schema-1-Modelle
4. Prepared-/Committed-Authentication-Rootablauf
5. Werksreset-Zustandsmaschine
6. Gesamt-Recoverydienst
7. Cut-Point-/Korruptionsorakel
8. Ressourcen- und Buildplan

Nur bei echter fachlicher Ownerentscheidung, Sicherheitswiderspruch oder
widerspruechlicher Spezifikation anhalten.

## Git- und PR-Ablauf nach Freigabe

1. aktuellen `main` und Abschluss von #54/#55/#56 pruefen
2. Branch erstellen:
   `feat/issue-57-bootstrap-secret-manifeste-recovery`
3. INDEX zuerst gegen Live-Status pruefen; nur bei Abweichung synchronisieren
4. ausschliesslich #57 bearbeiten
5. PR mit `Closes #57` erstellen
6. nicht mergen, kein Auto-Merge, Branch nicht loeschen
7. danach anhalten

## Verbindlicher Scope

### BootstrapRecord

- zwei redundante ConfigurationBootstrapRecord-Slots
- mindestens BootstrapSequence, Speicherformatversion, StorageEpoch und Zustand
- gespeicherte Zustaende:
  - Initializing
  - Initialized
  - Resetting
- NotFound ist kein gespeicherter Zustand
- Readfehler niemals als NotFound behandeln

Automatische Initialisierung nur, wenn:

- alle erforderlichen Reads erfolgreich waren
- kein gueltiger Root existiert
- keine beschaedigt vorhandenen Root-/Bootstrapdaten erkannt wurden
- kein BootstrapRecord existiert

Vor der ersten Dokumentrevision Initializing persistieren.

Unter StorageEpoch 1 erzeugen:

- UserConfiguration Revision 1: de, Europe/Zurich, Fermentationsschrank
- ServiceConfiguration Revision 1 mit 0 Payloadbytes
- ProgramCatalog Revision 1 mit vier Factory-Arbeitskopien
- ConfigurationManifest Generation 1
- ConfigurationRootRecord Sequenz 1 ohne Fallback
- Connectivity- und Authentication-Manifeste Generation 1 als NotProvisioned

Erst nach Ruecklesen und Vollvalidierung des gesamten Graphen Bootstrap auf
Initialized setzen.

### StorageEpoch

- gesamte Konfigurations- und Secretpersistenz an aktuelle StorageEpoch binden
- alle Referenzen muessen dieselbe aktuelle Epoche besitzen
- Startwert 1, Wert 0 reserviert
- Ueberlauf sicher ablehnen
- alte Epochen nach Reset logisch unerreichbar
- keine sichere physische Flashloeschung behaupten

### Connectivity Schema 1

Vorbereitete physische Struktur:

- 4 WLAN-Secret-Revisionsslots
- 4 ConnectivitySecretSetManifest-Slots
- keine eigenen Connectivity-Roots

Schema 1 enthaelt ausschliesslich:

- StorageEpoch
- Manifestgeneration
- Zustand NotProvisioned

Keine Secret-Referenzen und keine freie/opake Payload. Wirksamkeit nur durch
einen vollstaendig gueltigen Active-/Fallback-/Pending-Konfigurationsgraphen.

### Authentication Schema 1

Physische Struktur:

- 3 vorbereitete Webpasswort-Nachweisslots
- 3 vorbereitete Service-PIN-Nachweisslots
- 3 AuthenticationManifest-Slots
- 2 AuthenticationRootRecord-Slots

Schema 1 enthaelt:

- StorageEpoch
- Manifestgeneration
- CredentialEpoch
- NotProvisioned fuer Webpasswort
- NotProvisioned fuer Service-PIN

Keine reale Nachweispayload.

Rootregeln:

- Status Prepared oder Committed
- nur kanonischer vollstaendig gueltiger Committed-Root ist wirksam
- Prepared allein nie wirksam
- Credentials/Manifest vorbereiten
- einen Root Prepared schreiben
- zweiten Root Committed schreiben
- neuen committed Graph vollstaendig pruefen
- Prepared-Root auf dieselbe committed Generation nachziehen
- danach darf kein committed Root der aelteren CredentialEpoch verbleiben
- CredentialEpoch beginnt bei 1, ist von MutationSequence getrennt und laeuft
  nie still ueber

Teste Auswahl, Widerruf, Rootwechsel und Recovery, aber keine Anmeldung.

### Beschaedigte Daten

- ungueltige, beschaedigte oder unlesbare Daten sind nie fabrikneuer Speicher
- ohne nutzbaren Active/Fallback keine Factory-Neuanlage
- nichts still loeschen
- unbekanntes oder nicht migrierbares Schema erzeugt keinen Factory-Fallback
- typisierten ConfigurationUnavailable beziehungsweise
  ConfigurationIntegrityFailure liefern
- keine RuntimeConfiguration freigeben
- systemweite Fehlerklasse und SAFE_BOOT bleiben #24

### Wiederaufnehmbarer Werksreset

Ein bestaetigter Vollreset:

1. StorageEpoch ohne Ueberlauf erhoehen
2. Resetting unter neuer Epoche persistieren
3. Referenzen alter Epochen logisch ungueltig machen
4. Connectivity und Authentication als NotProvisioned erzeugen
5. neue Initialkonfiguration aktivieren
6. Bootstrap als Initialized abschliessen

Verbindlich:

- Active, Pending und Fallback der alten Epoche invalidieren
- alte Secrets logisch unerreichbar machen
- nie fruehere CredentialEpoch reaktivieren
- Touchkalibrierung erhalten
- Lauf-, Journal- und Historiendaten anderer Issues nicht still loeschen
- Korruption startet niemals automatisch einen Reset
- keine sichere physische Flashloeschung behaupten

### End-to-End-Recovery

- Bootdienst aus Bootstrap, ConfigurationRoot, Pending/Intent, Connectivity und
  Authentication zusammensetzen
- nach jedem simulierten Neustart alle Dienste und externen Quellen neu aufbauen
- Zeit, Zufall und RunAssessment kontrollierbar
- jeder Cut mit konkretem Recovery-Orakel
- halbfertige Initialisierung, Authentication-Transaktion und Reset idempotent
  fortsetzen oder sicher blockieren

## Ausdruecklicher Nicht-Scope

- reale WLAN-SSID oder Passwoerter
- reale Passwort-/PIN-Pruefnachweise
- KDF-, Algorithmus-, Salt-, Work-Factor- oder Pepperdaten
- Anmeldung, Sitzungen, Tokens, CSRF oder Sperrzeiten
- Netzwerkadapter oder WLAN-Verbindung
- Laufpersistenz aus #17
- Backup-/Importformat aus #19
- systemweite Fehlerklassen, Verriegelung, SAFE_BOOT und Aktorsperren aus #24
- Bedienablauf oder physische Resetgeste aus #25/#26
- Webtransport und Authentifizierung aus #27
- reale Flashatomizitaet, Flashlebensdauer oder physische sichere Loeschung

## Architekturgrenzen

- Bootstrap-, Secret-Manifest-, Authentication-Root-, Epoch- und Resetbedeutung
  in `fermentation_app`
- technische Persistenzbausteine aus #54 nicht duplizieren
- Test-Support nur anwendungsneutral; fachliche Recovery-Orakel in App-Tests
- keine produktive Secret-Payload in freien oder opaken Strukturen
- `src/main.cpp` bleibt Composition Root

## Verbindliche Tests

### Bootstrap

- fabrikneuer simulierter Speicher
- Initializing vor erster Dokumentrevision
- Cut vor und nach jedem Write bis Initialized
- Wiederaufnahme aus jedem Cut
- Readfehler verhindert Initialisierung
- beschaedigte Altbytes verhindern Initialisierung
- unbekannte Speicherformat-/Schemageneration verhindert Factory-Fallback

### Connectivity/Authentication

- NotProvisioned nur ueber gueltigen Konfigurationsgraph wirksam
- gemeinsame Connectivity-Generation fuer Active/Fallback/Pending
- wiederholte Authentication-Rootwechsel
- Cuts vor/nach jedem Credential-/Manifest-/Prepared-/Committed-Write
- Prepared allein nie wirksam
- nach Erfolg kein committed Root der alten CredentialEpoch
- gemischte Epoch/Generation/CRC/Referenz ablehnen
- Widerruf reaktiviert nie alte CredentialEpoch

### Werksreset

- Cut vor und nach jedem der sechs Schritte
- Resetting idempotent wiederaufnehmen
- alte Active/Pending/Fallback/Connectivity/Authentication unerreichbar
- Touchkalibrierung erhalten
- keine alte CredentialEpoch reaktivieren
- StorageEpoch-/CredentialEpoch-Ueberlauf ablehnen
- Korruption startet keinen Reset

### Vollstaendige Matrix von #16

- Bootstrap
- mindestens 5 Active-Commits
- mindestens 3 Pending-Ersetzungen
- Pending verwerfen und erneut erzeugen
- Anwenden und neu starten
- kombinierte Connectivity-/Konfigurationstransaktion
- wiederholte Authentication-Rootwechsel und Widerruf
- Active-/Pending-Migration
- Werksreset
- kein vorzeitiges NoUnreferencedSlotAvailable
- Korruption jedes Recordtyps und jeder Referenzkante
- konkretes Fallback- oder Unavailable-Orakel fuer jeden Fall

### Ressourcen

- hoechstens ein vollstaendiger kodierter Recordpuffer waehrend Commit
- Preview erst nach Ressourcenbereitstellung sichtbar
- Ressourcenfehler vor Root-Commit ohne Teilaktivierung
- Publish nach Root-Commit ohne Allokation, Serialisierung oder Reservierung
- Base-/Head-Vergleich fuer RAM, Flash, firmware.bin und firmware.elf
- keine reale Heap-/Flashgarantie ohne Hardwaremessung

## Qualitaetspruefung

- `pio test -e native`
- alle drei Buildprofile
- alle Quality-Gate-Skripte
- clang-format
- clang-tidy LLVM 18
- `git diff --check`
- vollstaendiger Ressourcenbericht

## Definition of Done

- #54, #55 und #56 gemergt und abgeschlossen
- kompletter Scope von #57 umgesetzt
- Bootstrap, Secret-Manifeste, Authentication-Roots und Reset wiederanlaufbar
- vollstaendige Cut-Point-, Korruptions-, Slotrotations- und Ressourcenmatrix gruen
- keine reale Secret-/Authlogik aus #27 vorweggenommen
- Dokumentation und CHANGELOG aktualisiert
- PR mit `Closes #57` erstellt
- nicht gemergt und Branch nicht geloescht
- #16 bleibt bis Merge und finaler Gesamtpruefung offen

## Vorgeschlagener Branch

`feat/issue-57-bootstrap-secret-manifeste-recovery`
