# Agent-Auftrag fuer Issue #55

## Issue

**[E2.1b] Typisierte Konfigurationsdokumente implementieren**

Aktueller Snapshot-Status: `BLOCKED_DEPENDENCY`

Tracking-Issue: #16

Epic: #4

GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/55

> Das Live-Issue und GitHub sind die aktuelle Wahrheit. Dieser Auftrag darf erst
> ausgefuehrt werden, wenn #54 geschlossen und der Status von #55 live auf
> `READY` gesetzt wurde.

## Sperrregel

Solange #54 nicht gemergt und abgeschlossen oder #55 nicht `READY` ist:

- keinen Branch fuer #55 erstellen
- keine Produktionsdatei aendern
- keinen PR erstellen
- keine Plattformbausteine aus #54 parallel nachbauen
- Blocker berichten und vollstaendig anhalten

## Ziel nach Freigabe

Implementiere die drei typisierten Dokumente der ersten Schemageneration:

- UserConfiguration Schema 1
- ServiceConfiguration Schema 1
- ProgramCatalog Schema 1

Dazu gehoeren deterministische fachliche Payloadcodecs, vollstaendige
Grenzvalidierung und Copy-Migration. Manifeste, Roots, Preview und Aktivierung
bleiben #56.

## Vor jeder Arbeit lesen

- Live-Issue #55
- Tracking-Issue #16
- abgeschlossenes Issue #54 und dessen gemergten PR
- `AGENTS.md`
- `lib/fermentation_app/AGENTS.md`
- `lib/device_platform/AGENTS.md`
- `docs/CONFIGURATION_PERSISTENCE.md`
- `docs/SETTINGS_AND_STORAGE.md`
- `docs/BACKUP_SECURITY_RETENTION.md`
- `docs/PR38_REVIEW_CORRECTIONS.md`
- Programmmodell, Standardkatalog und Migrationen aus #12
- `docs/CI_AND_QUALITY_GATES.md`
- Agent-INDEX

Berichte vor der ersten Codeaenderung:

1. Dokumentmodelle und Schematypen
2. Feld- und Payloadgrenzen
3. Wiederverwendung des bestehenden ProgramDocument-Modells
4. fachliche Codecstruktur auf Basis von #54
5. Copy-Migrationsweg
6. Test- und Ressourcenplan

Nur bei echter fachlicher Ownerentscheidung, Sicherheitswiderspruch oder
widerspruechlicher Spezifikation rueckfragen.

## Git- und PR-Ablauf nach Freigabe

1. aktuellen `main` und Abschluss von #54 pruefen
2. Branch erstellen:
   `feat/issue-55-typisierte-konfigurationsdokumente`
3. INDEX zuerst gegen Live-Status pruefen und nur bei Abweichung als erste
   Branchaenderung synchronisieren
4. ausschliesslich #55 bearbeiten
5. PR mit `Closes #55` erstellen
6. nicht mergen, kein Auto-Merge, Branch nicht loeschen
7. danach anhalten

## Verbindlicher Scope

### UserConfiguration Schema 1

Ausschliesslich:

- DisplayLanguageId
- kanonischer IANA-Zeitzonenbezeichner
- sichtbarer Geraetename

Verbindlich:

- DisplayLanguageId 2–16 ASCII-Bytes
- Zeichen `a-z`, `0-9`, `-`
- Anfang und Ende alphanumerisch
- keine doppelten Bindestriche
- keine Normalisierung
- exakter Treffer im versionierten Firmwarekatalog
- Release 1 mindestens `de`, `es`, `en`; Factory-Standard `de`
- Zeitzone 1–64 ASCII-Bytes
- strukturelle IANA-Pruefung gemaess Spezifikation
- exakter Firmwarekatalogtreffer
- erfolgreiche Vorbereitung durch `ITimeZoneResolver`
- Factory-Standard `Europe/Zurich`
- Geraetename: gueltiges UTF-8, 1–48 Unicode-Skalarwerte, maximal 96 Bytes
- mindestens ein Nicht-Leerraumwert
- kein fuehrender/abschliessender Unicode-Leerraum
- keine NUL-, C0-, C1-, Zeilen- oder Absatztrennzeichen
- UserConfiguration-Payload maximal 256 Bytes

Keine Displayhelligkeit, Lautstaerke, Netzwerk-, Hardware-, Sensor-, Regel- oder
Sicherheitsfelder erfinden.

### ServiceConfiguration Schema 1

- typisiertes gueltiges Dokument
- exakt 0 Payloadbytes
- konkrete Revision in jedem spaeteren Manifest
- kein `fehlt`-Sondermodell
- keine Reservefelder
- kein freies Key/Value-Modell

### ProgramCatalog Schema 1

- exakt vier gespeicherte Factory-Arbeitskopien
- maximal zwoelf Benutzerprogramme
- maximal sechzehn Programme gesamt
- bestehende ProgramDocument-Typen und -Schemas wiederverwenden
- stabile katalogweit eindeutige IDs
- Position bildet die Anzeigereihenfolge
- Factory-, Ruecksetz-, Installations- und Aktivierungsmerkmale erhalten
- Factory-Vorlagen bleiben unveraenderlich in der Firmware
- Firmwareupdate ueberschreibt Arbeitskopien nicht
- Standardprogrammreset ersetzt nur die gewaehlte Arbeitskopie
- Programme mit TBD_COMMISSIONING duerfen strukturell gespeichert werden
- Start bleibt separat vollstaendig `Runnable`-validiert
- ProgramCatalog-Payload maximal 32.768 Bytes

Programm-ID:

- 1–48 ASCII-Bytes
- nur `a-z`, `0-9`, `-`
- Anfang/Ende alphanumerisch
- keine doppelten Bindestriche
- keine stille Normalisierung
- nach Erstellung unveraenderlich
- keine reservierte Factory-ID fuer Benutzerprogramme

Name und Notizen:

- Name: 1–48 Unicode-Skalarwerte, maximal 96 UTF-8-Bytes
- Notizen: leer erlaubt, maximal 512 Unicode-Skalarwerte und 1.024 UTF-8-Bytes
- Notizen erlauben LF, aber kein CR, NUL, C0/C1 ausser LF, U+2028 oder U+2029
- CRLF/CR duerfen nur vor Preview/Bestaetigung deterministisch zu LF werden

### Schema und Migration

- getrennte Schematypen fuer alle drei Dokumentarten
- Dokumentrevisionen `uint64_t`, Start 1, Wert 0 reserviert
- kein stiller Ueberlauf
- deterministische Big-Endian-Payloads auf Basis von #54
- explizite stabile Wire-IDs fuer fachliche Enums
- unbekannte Enumwerte ablehnen, sofern nicht anders spezifiziert
- exakte Laengenpruefung
- Anzahl-, Feld- und Payloadgrenzen unabhaengig pruefen
- keine Programme abschneiden oder still loeschen
- Copy-Migration, nie In-place
- unbekannte neuere Schemas ablehnen
- keine erfundene Schema-0-Migration
- generischen Ablauf mit Testschemas pruefen
- bestehende ProgramDocument-Migration im Katalog testen
- Active/Pending-Aktivierung nicht implementieren

## Ausdruecklicher Nicht-Scope

- ConfigurationManifest, Roots, Pending oder Aktivierungsintent
- Slotrotation und Schutzwurzeln
- Preview, Owner, Token oder Konfliktsemantik
- RuntimeConfigurationSnapshot und Publish
- Bootstrap, StorageEpoch-Ersteinrichtung oder Werksreset
- Connectivity-/Authentication-Manifeste oder Secrets
- Laufpersistenz aus #17
- Backup aus #19
- systemweite Fehlerpolitik aus #24
- Netzwerk und Authentifizierung aus #27

## Architekturgrenzen

- konkrete Dokumente, fachliche Codecs und Validierung in `fermentation_app`
- technische Wire-/Envelope-/Speicherbausteine aus #54 wiederverwenden
- keine konkreten Fermentationsdokumente in `device_platform`
- aktiver Lauf und Laufschnappschuss bleiben ausserhalb der Konfiguration
- keine zweite Programmschema-Definition

## Verbindliche Tests

Mindestens:

- feste Golden-Bytefolgen je Dokument
- Roundtrip und deterministisches Re-Encode
- fehlende, zusaetzliche und nichtkanonische Bytes
- jede ASCII-/UTF-8-/Skalar-/Leerraum-/Steuerzeichengrenze unter/auf/ueber Limit
- ungueltiges UTF-8
- Sprach-ID-Struktur und unbekannter Firmwarekatalogwert
- Zeitzonenstruktur, unbekannter Katalogwert und Resolverfehler
- ServiceConfiguration exakt 0 Bytes
- Factorykatalog mit exakt 4 Programmen
- 0 und 12 Benutzerprogramme
- 3/5 Factoryprogramme, falsche Reihenfolge, doppelte IDs, reservierte ID
- 17 Gesamtprogramme
- Feld-, Anzahl- und Payloadueberschreitung ohne Abschneiden
- Notiz-Zeilenendennormalisierung nur am erlaubten Eingangspunkt
- Copy-Migration Schritt fuer Schritt
- unbekanntes neueres Schema
- bestehende ProgramDocument-Migration im ProgramCatalog

Golden Tests duerfen Encoder und Decoder nicht nur gegeneinander testen.

## Qualitaets- und Ressourcenpruefung

- `pio test -e native`
- alle drei Buildprofile
- alle Quality-Gate-Skripte
- clang-format
- clang-tidy LLVM 18
- `git diff --check`
- Base-/Head-Vergleich fuer RAM, Flash, firmware.bin und firmware.elf
- reale Heap- oder Hardwarewerte nicht behaupten

## Definition of Done

- #54 gemergt und abgeschlossen
- kompletter Scope von #55 implementiert
- Golden-, Grenz-, Roundtrip- und Migrationsmatrix gruen
- keine Manifest-/Aktivierungsarbeit aus #56 vorweggenommen
- Dokumentation und CHANGELOG aktualisiert
- PR mit `Closes #55` erstellt
- nicht gemergt und Branch nicht geloescht

## Vorgeschlagener Branch

`feat/issue-55-typisierte-konfigurationsdokumente`
