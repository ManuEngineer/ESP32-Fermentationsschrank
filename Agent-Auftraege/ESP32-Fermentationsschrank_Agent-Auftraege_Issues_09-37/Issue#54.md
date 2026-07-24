# Agent-Auftrag fuer Issue #54

## Issue

**[E2.1a] Plattformpersistenz und Wireformat implementieren**

Aktueller Snapshot-Status: `READY`

Tracking-Issue: #16

Epic: #4

GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/54

> Das Live-Issue und der aktuelle Stand auf GitHub sind die Wahrheit. Lies Issue
> #54, Tracking-Issue #16 und alle aktuellen Kommentare vor jeder Arbeit erneut.
> Dieser Auftrag ersetzt weder das Live-Issue noch die kanonische Spezifikation.

## Ziel

Implementiere die vollstaendig anwendungsneutralen Grundlagen fuer begrenzte,
binaersichere und stromausfallsichere Persistenz:

- `IStateStore`
- starke technische Typen
- begrenzte Byte-Reader und -Writer
- Big-Endian-Codecs
- IEEE-754-binary64-Codec
- CRC-32/ISO-HDLC
- Envelope Version 1
- generische feste Revisions- und redundante Recordslots
- sichere Zufalls- und Zeitzonenports
- `SimulatedPersistentStateStore` und technische Golden-/Cut-Point-Tests

Es werden in diesem Issue keine konkreten Konfigurationsdokumente, Manifeste,
Roots, Factorywerte oder Fermentationsregeln implementiert.

## Abhaengigkeiten

Abgeschlossen:

- #9
- #10
- #11
- #12

Issue #54 ist das erste ausfuehrbare Teilissue von #16. #55, #56 und #57 bleiben
bis zu ihren jeweiligen Abhaengigkeiten blockiert.

## Vor jeder Aenderung lesen

- Live-Issue #54
- Tracking-Issue #16
- `AGENTS.md`
- `lib/device_platform/AGENTS.md`
- `lib/device_platform_test_support/AGENTS.md`
- `lib/fermentation_app/AGENTS.md`
- `docs/CONFIGURATION_PERSISTENCE.md`
- `docs/SETTINGS_AND_STORAGE.md`
- `docs/BACKUP_SECURITY_RETENTION.md`
- `docs/PR38_REVIEW_CORRECTIONS.md`
- `docs/CI_AND_QUALITY_GATES.md`
- bestehenden Code in `lib/device_platform/`
- bestehenden Test-Support in `lib/device_platform_test_support/`
- Agent-INDEX

Berichte vor der ersten Codeaenderung:

1. geplante Dateien und Modulgrenzen
2. Portvertrag von `IStateStore`
3. Fehler- und Kapazitaetsmodell
4. kanonische Envelope-Bytefolge
5. Slot- und Kandidatenmechanik
6. Aufbau des simulierten Persistenzspeichers
7. Test- und Ressourcenplan

Nur bei echter fachlicher Ownerentscheidung, Sicherheitswiderspruch,
widerspruechlicher Spezifikation oder nicht implementierbarer Grenze anhalten.
Private Klassennamen, Helper, Dateiaufteilung und Test-Fixtures selbst entscheiden.

## Git- und PR-Ablauf

1. `main` aktualisieren und sauberen Arbeitsbaum nachweisen.
2. Branch von aktuellem `main` erstellen:
   `feat/issue-54-platformpersistenz-und-wireformat`
3. INDEX zuerst gegen den Live-Stand pruefen. Nur bei Abweichung ist die erste
   Branchaenderung eine reine INDEX-Synchronisierung; keine kuenstliche
   Aenderung erzeugen, wenn der INDEX bereits stimmt.
4. Ausschliesslich Issue #54 bearbeiten.
5. Kleine, nachvollziehbare Commits erstellen.
6. PR mit `Closes #54` erstellen.
7. Nicht mergen, kein Auto-Merge, Branch nicht loeschen.
8. Nach PR-Erstellung vollstaendig anhalten und berichten.

## Verbindlicher Scope

### `device_platform`

Implementiere anwendungsneutral:

- begrenztes binaersicheres `IStateStore`
- feste deterministische Schluessel als technischer Werttyp, ohne Kenntnis
  konkreter Konfigurationsschluessel
- caller- beziehungsweise schluesselspezifisches maximales Leselimit
- typisierte Ergebnisse fuer Erfolg, `NotFound`, Read-, Write-, Integritaets-,
  Sequenz- und Kapazitaetsfehler
- pro Schluessel atomares dauerhaftes Replace gemaess Portvertrag
- starke Typen mindestens fuer `StorageEpoch`, Revision, Generation,
  `RecordSequence`, `SlotId` und `RecordTypeId`
- reservierten Wert 0 und Ueberlauf explizit behandeln
- begrenzte Byte-Reader und -Writer
- Big-Endian-Codecs fuer alle benoetigten Integerbreiten und signierte Werte
- ausschliesslich `0x00`/`0x01` fuer Bool und Optionaltag
- IEEE-754-binary64 mit Compilezeitpruefungen, NaN-/Inf-Ablehnung,
  `-0.0`-Normalisierung beim Schreiben und Ablehnung negativer Null beim Lesen
- CRC-32/ISO-HDLC exakt gemaess Spezifikation
- Envelope Version 1 mit exakter Feldreihenfolge, Laenge und CRC
- generische feste Revision-/Recordslots und technische Kandidatensortierung
- Ports `ISecureRandomSource` und `ITimeZoneResolver`
- bestehendes `ITimeSource` verwenden und nicht duplizieren

### Envelope Version 1

Kanonische Reihenfolge:

1. Magic `DPRF`
2. Envelope-Version `uint16` Big Endian
3. Record-Type-ID `uint16` Big Endian
4. Schema-Version `uint32` Big Endian
5. StorageEpoch `uint64` Big Endian
6. VersionValue `uint64` Big Endian
7. Payloadlaenge `uint32` Big Endian
8. ChangeOrigin-Wire-ID `uint16` Big Endian
9. ChangeOperation-Wire-ID `uint16` Big Endian
10. UTC-Optionaltag
11. optional UTC-Unix-Sekunden `int64` Big Endian
12. CRC-32/ISO-HDLC `uint32` Big Endian
13. Payload

Verbindlich:

- 41 Bytes einschliesslich CRC ohne UTC
- 49 Bytes einschliesslich CRC mit UTC
- keine Padding-, Reserve-, Flags- oder ABI-abhaengigen Bytes
- CRC ueber Header vor CRC plus gesamte Payload; CRC-Feld ausgeschlossen
- unbekannte Envelope-Version ablehnen
- unbekannte rohe Origin-/Operation-IDs erhalten
- konkrete fachliche Record-Type-Bedeutung nicht implementieren

### `device_platform_test_support`

Implementiere anwendungsneutral:

- `SimulatedPersistentStateStore`
- committed Daten getrennt von laufender Schreiboperation und flüchtigem Zustand
- Fehler vor Beginn
- Stromausfall vor Commit
- Stromausfall nach Commit vor Rueckkehr
- erfolgreicher Write
- gezielte Read-, `NotFound`- und Korruptionsinjektion
- Neustart, bei dem nur committed Daten ueberleben
- kontrollierbare Zufalls- und Zeitzonenadapter

Abgeschnittene oder gemischte Daten sind nur gezielte Korruption, nie erlaubtes
Ergebnis eines erfolgreich zurueckgekehrten atomaren Writes.

## Ausdruecklicher Nicht-Scope

Nicht implementieren:

- UserConfiguration, ServiceConfiguration oder ProgramCatalog
- konkrete Dokument-, Manifest-, Root-, Pending-, Bootstrap- oder Secrettypen
- Factorywerte, Programme oder fachliche Konfigurationslimits
- Preview, Graphvalidierung, Aktivierung oder RuntimeConfiguration
- Laufpersistenz aus #17
- Backupformat aus #19
- Fehlerklassen, SAFE_BOOT oder Aktorsperren aus #24
- reale WLAN-/Passwort-/PIN-Daten aus #27
- produktiver ESP32-NVS-/Flashadapter, sofern nicht bereits gesondert spezifiziert
- reale Flashatomizitaets-, Lebensdauer- oder Heapgarantien

`device_platform` darf keine Fermentationsbegriffe enthalten.
`device_platform_test_support` darf von keinem Produktionsmodul oder
ESP32-Profil referenziert werden.

## Verbindliche Tests

Mindestens:

- Big-Endian-Goldenwerte aller Primitiven
- Bool-/Optionaltag nur 0 und 1
- signed Zweierkomplement-Grenzen
- binary64-Goldenwerte, `-0.0`, NaN und positive/negative Unendlichkeit
- CRC `123456789` = `0xCBF43926`
- Envelope-Goldenvektoren ohne und mit UTC
- falsches Magic
- Version, RecordType, Schema, StorageEpoch und VersionValue 0
- unbekannte Envelope-Version
- ungueltige UTC-Tags
- falsche, ueberlaufende, zu grosse, fehlende und zusaetzliche Laengen/Bytes
- CRC-Fehler in Header und Payload
- rohe unbekannte Origin-/Operation-IDs
- Slotgrenzen, Kandidatensortierung, doppelte Sequenzen und Sequenzueberlauf
- alle Write-Cuts und Fehler des simulierten Speichers
- Neustartverhalten und gezielte Korruption
- kein erfolgreicher Write erzeugt Teil- oder Mischdaten

Golden Tests muessen feste erwartete Bytefolgen enthalten und duerfen nicht nur
Encoder und Decoder gegeneinander testen.

## Qualitaets- und Ressourcenpruefung

Mindestens ausfuehren:

- `pio test -e native`
- `pio run -e native`
- `pio run -e esp32_bringup`
- `pio run -e esp32_release`
- `python3 scripts/check_platformio_config.py`
- `python3 scripts/check_architecture_boundaries.py`
- `python3 scripts/check_secrets.py`
- `python3 scripts/selftest_quality_gates.py`
- `git diff --check`
- clang-format gemaess Projektdokumentation
- clang-tidy mit LLVM 18 und dokumentierter Produktionsdateiliste

Dokumentiere Base-SHA und PR-Head mit identischer Toolchain fuer:

- statisches RAM
- Flash
- `firmware.bin`
- `firmware.elf`

Werte bleiben informativ; keine reale Heap-, Flashlebensdauer- oder
Atomizitaetsgarantie behaupten.

## Definition of Done

- kompletter Scope des Live-Issues #54 umgesetzt
- Plattform bleibt anwendungsneutral
- Test-Support bleibt produktionsfrei
- Golden-, Grenz-, Korruptions- und Cut-Point-Tests gruen
- alle drei Buildprofile und Quality Gates gruen
- Ressourcenvergleich dokumentiert
- Dokumentation und `CHANGELOG.md` aktualisiert
- PR mit `Closes #54` erstellt
- kein Scope aus #55 bis #57 vorweggenommen
- nicht gemergt und Branch nicht geloescht

## Vorgeschlagener Branch

`feat/issue-54-platformpersistenz-und-wireformat`
