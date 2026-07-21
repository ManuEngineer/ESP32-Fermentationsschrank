# Plan zur Softwarespezifikation

## Ziel

Die gewuenschte Firmware wird zuerst vollstaendig beschrieben und erst danach implementiert. Die Dokumentation soll so eindeutig sein, dass Codex oder ein anderer Entwickler keine wesentlichen Produktentscheidungen selbst erfinden muss.

## Regeln

- In dieser Phase wird keine Fermentationssteuerung programmiert.
- Entscheidungen werden im Repository festgehalten.
- Offene Fragen bleiben als `TBD`, `unknown` oder Checkliste sichtbar.
- Hardwaredaten und Softwareverhalten werden getrennt dokumentiert.
- Sicherheitsanforderungen haben Vorrang vor Komfortfunktionen.
- Unbestaetigte GPIOs und Pegel bleiben unbestaetigt.

## Bestehende Quellen

- [`HARDWARE.md`](HARDWARE.md): Hardware und elektrische Randbedingungen
- [`HARDWARE_REVISIONS.md`](HARDWARE_REVISIONS.md): akzeptierte Hardwareaenderungen aus der Spezifikation
- [`REQUIREMENTS.md`](REQUIREMENTS.md): bisherige Anforderungen
- [`ARCHITECTURE.md`](ARCHITECTURE.md): vorgesehene Modulgrenzen
- [`OPEN_POINTS.md`](OPEN_POINTS.md): offene Fragen
- [`DECISIONS.md`](DECISIONS.md): akzeptierte Entscheidungen
- [`PROGRAMS.md`](PROGRAMS.md): allgemeiner Programm- und Prozessablauf
- [`STANDARD_PROGRAMS.md`](STANDARD_PROGRAMS.md): Zweck und Voreinstellungen der Standardprogramme
- [`STATE_MACHINE.md`](STATE_MACHINE.md): Betriebszustaende und Uebergaenge
- [`RECOVERY_AND_INTERRUPTION.md`](RECOVERY_AND_INTERRUPTION.md): Unterbrechungen, Sensorersatz, Zeitquelle und automatischer Wiederanlauf
- [`RUNTIME_BEHAVIOR.md`](RUNTIME_BEHAVIOR.md): Luefter, Richtungswechsel, Meldungsprioritaeten und akustische Signale
- [`LOCAL_UI.md`](LOCAL_UI.md): lokale Touch-Grundlagen und Hauptbildschirme
- [`LOCAL_PROGRAMS.md`](LOCAL_PROGRAMS.md): lokale Programmauswahl und Programmverwaltung
- [`LOCAL_RUNTIME_UI.md`](LOCAL_RUNTIME_UI.md): Laufdetails, Meldungen, Stoppen und Wiederanlaufanzeige
- [`LOCAL_UI_SETTINGS_SERVICE.md`](LOCAL_UI_SETTINGS_SERVICE.md): Menue, Einstellungen, Diagnose, Service, Touchkalibrierung und Sprachen
- [`NETWORK.md`](NETWORK.md): WLAN-Einrichtung, QR-Assistent, Ersatz-WLAN, Geraetename und Zugriffsgrundlagen
- [`WEB_UI.md`](WEB_UI.md): responsive Weboberflaeche, Anmeldung, Sitzungen, Live-Daten und Konfliktschutz
- [`NETWORK_DIAGNOSTICS_INTEGRATION.md`](NETWORK_DIAGNOSTICS_INTEGRATION.md): Netzwerkdiagnose, VPN, Reverse Proxy, Proxy-Vertrauen und Lese-API
- [`SETTINGS_AND_STORAGE.md`](SETTINGS_AND_STORAGE.md): Konfigurationsebenen, Aenderungsrechte, Validierung und atomare Speicherung
- [`RUN_PERSISTENCE.md`](RUN_PERSISTENCE.md): Laufzustand, Kontrollpunkte, Messhistorie und Wiederherstellung
- [`../config/programs.example.yaml`](../config/programs.example.yaml): vorlaeufiges maschinenlesbares Programmschema

## Phasen und Ergebnisdokumente

1. **Produktvision und Nutzung** -> `PRODUCT_VISION.md`
2. **Programme und Prozessablauf** -> `PROGRAMS.md`, `STANDARD_PROGRAMS.md` und Programmschema
3. **Betriebszustaende und Uebergaenge** -> `STATE_MACHINE.md`, `RECOVERY_AND_INTERRUPTION.md`, `RUNTIME_BEHAVIOR.md`
4. **Lokale Touch-Bedienung** -> `LOCAL_UI.md`, `LOCAL_PROGRAMS.md`, `LOCAL_RUNTIME_UI.md`, `LOCAL_UI_SETTINGS_SERVICE.md`
5. **Weboberflaeche und Netzwerk** -> `WEB_UI.md`, `NETWORK.md`, `NETWORK_DIAGNOSTICS_INTEGRATION.md`
6. **Einstellungen und Persistenz** -> `SETTINGS_AND_STORAGE.md`, `RUN_PERSISTENCE.md`
7. **Temperaturregelung und Aktorlogik** -> `TEMPERATURE_CONTROL.md`
8. **Sicherheit und Fehlerbehandlung** -> `SAFETY_AND_FAULTS.md`
9. **Diagnose, Wartung und Updates** -> `DIAGNOSTICS_AND_MAINTENANCE.md`
10. **Akzeptanztests und Implementierungsplan** -> `ACCEPTANCE_TESTS.md` und spaetere GitHub-Issues

## Arbeitsweise

1. Eine Phase wird gemeinsam besprochen.
2. Ich fasse Entscheidungen und offene Punkte zusammen.
3. Das zugehoerige Dokument wird auf `docs/software-specification` erstellt oder aktualisiert.
4. Der Benutzer prueft und korrigiert den Inhalt.
5. Erst nach Freigabe beginnt die naechste Phase.
6. Am Ende wird ein Pull Request nach `main` erstellt.

## Stand

- [x] Bestehende Dokumentation gesichtet
- [x] Spezifikationsbranch erstellt
- [x] Phase 1: Produktvision und Nutzung
- [x] Phase 2: Programme und Prozessablauf
  - [x] Phase 2A: allgemeiner Programmablauf, Sensorbetrieb und Vorheizen
  - [x] Phase 2B: allgemeine Standardprogramme und ihre Voreinstellungen
  - [ ] Exakte Temperaturen, Zeiten und Grenzwerte werden nach Inbetriebnahme als Werkseinstellungen ergaenzt
- [x] Phase 3: Zustandsmaschine
  - [x] Phase 3A: Grundzustaende, Stoppen, Warnungen und manuelle Betriebsarten
  - [x] Phase 3B: Tuerkontakt, Produktfuehlerausfall, Wartezeit und Wiederanlaufgrundsaetze
  - [x] Phase 3C: Netzwerkzeit, spaetere RTC-Option und autonomer phasenbezogener Wiederanlauf
  - [x] Phase 3D: Luefternachlauf, Sensorersatz, Meldungsprioritaeten und akustische Signale
- [x] Phase 4: Lokale Bedienung
  - [x] Phase 4A: Displayausrichtung, Hauptbildschirm, Navigation, Temperaturen und Eingabeverhalten
  - [x] Phase 4B: Programmauswahl, Programmverwaltung, Loeschen und Startablauf
  - [x] Phase 4C: Laufdetails, Meldungen, Stoppen und Wiederanlaufanzeige
  - [x] Phase 4D: Menue, Einstellungen, Diagnose, PIN-Service, Wiederherstellung, Touchkalibrierung und Sprachen
- [x] Phase 5: Web und Netzwerk
  - [x] Phase 5A: WLAN-Ersteinrichtung, QR-Assistent, Ersatz-WLAN, Geraetename und Zugriffsgrundlagen
  - [x] Phase 5B: Weboberflaeche, Anmeldung, Sitzungen und gleichzeitige Bedienung
  - [x] Phase 5C: Netzwerkdiagnose, VPN, Reverse Proxy, Proxy-Vertrauen und lokale Lese-API
- [ ] Phase 6: Einstellungen und Persistenz
  - [x] Phase 6A: Einstellungsgruppen, Werkseinstellungen und Aenderungsrechte
  - [x] Phase 6B: Laufpersistenz, Speicherzeitpunkte, Messhistorie und Wiederherstellbarkeit
  - [ ] Phase 6C: Geheimnisse, Sicherung, Export, Reset und Datenaufbewahrung
- [ ] Phase 7: Temperaturregelung und Aktoren
- [ ] Phase 8: Sicherheit und Fehler
- [ ] Phase 9: Diagnose, Wartung und Updates
- [ ] Phase 10: Akzeptanztests und Implementierungsplan

## Aktuelle Phase

Als Naechstes wird Phase 6C bearbeitet: Geheimnisse, Sicherung, Export, Reset und Datenaufbewahrung.
