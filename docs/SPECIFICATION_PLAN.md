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
- [`BACKUP_SECURITY_RETENTION.md`](BACKUP_SECURITY_RETENTION.md): Geheimnisse, Sicherung, Import, Aufbewahrung und Werksreset
- [`TEMPERATURE_CONTROL.md`](TEMPERATURE_CONTROL.md): Regelstrategie, Sensorrollen, Zielqualifikation und Laufanpassungen
- [`ACTUATOR_TIMING_AND_FANS.md`](ACTUATOR_TIMING_AND_FANS.md): Peltier-Schaltfenster, Mindestzeiten, Richtungswechsel und Luefterlogik
- [`SENSOR_TUNING_COMMISSIONING.md`](SENSOR_TUNING_COMMISSIONING.md): Sensorfilter, Kalibrierung, PI-Parametersaetze und Inbetriebnahme
- [`SAFETY_AND_FAULTS.md`](SAFETY_AND_FAULTS.md): Fehlerklassen, Verriegelung, Quittierung, Reset und Wiederfreigabe
- [`SAFETY_COMPONENT_FAULTS.md`](SAFETY_COMPONENT_FAULTS.md): Temperatur-, Sensor-, Luefter-, BTS7960- und Peltierfehler
- [`SYSTEM_SAFETY_AND_RECOVERY.md`](SYSTEM_SAFETY_AND_RECOVERY.md): Versorgung, Boot, Watchdogs, Datenintegritaet, SAFE_BOOT und Fehlerjournal
- [`DIAGNOSTICS_AND_MAINTENANCE.md`](DIAGNOSTICS_AND_MAINTENANCE.md): Diagnoseansichten, Boot-Selbsttest, Servicepruefungen, Exporte und UART-Umfang
- [`FIRMWARE_UPDATE_AND_ROLLBACK.md`](FIRMWARE_UPDATE_AND_ROLLBACK.md): UART-Update im ersten Release und vorbereitete spaetere Web-OTA-/Rollbackregeln
- [`RESOURCE_BUDGET_AND_MAINTENANCE.md`](RESOURCE_BUDGET_AND_MAINTENANCE.md): Ressourcenueberwachung, Speicherpflege, Flashbudget und Wartungsumfang
- [`ACCEPTANCE_TESTS.md`](ACCEPTANCE_TESTS.md): Testebenen, Fehlerinjektionen, Release-Gates und Abnahmenachweise
- [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md): software-first Entwicklungsreihenfolge, Simulation, Bring-up und Meilensteine
- [`../config/programs.example.yaml`](../config/programs.example.yaml): vorlaeufiges maschinenlesbares Programmschema

## Phasen und Ergebnisdokumente

1. **Produktvision und Nutzung** -> `PRODUCT_VISION.md`
2. **Programme und Prozessablauf** -> `PROGRAMS.md`, `STANDARD_PROGRAMS.md` und Programmschema
3. **Betriebszustaende und Uebergaenge** -> `STATE_MACHINE.md`, `RECOVERY_AND_INTERRUPTION.md`, `RUNTIME_BEHAVIOR.md`
4. **Lokale Touch-Bedienung** -> `LOCAL_UI.md`, `LOCAL_PROGRAMS.md`, `LOCAL_RUNTIME_UI.md`, `LOCAL_UI_SETTINGS_SERVICE.md`
5. **Weboberflaeche und Netzwerk** -> `WEB_UI.md`, `NETWORK.md`, `NETWORK_DIAGNOSTICS_INTEGRATION.md`
6. **Einstellungen und Persistenz** -> `SETTINGS_AND_STORAGE.md`, `RUN_PERSISTENCE.md`, `BACKUP_SECURITY_RETENTION.md`
7. **Temperaturregelung und Aktorlogik** -> `TEMPERATURE_CONTROL.md`, `ACTUATOR_TIMING_AND_FANS.md`, `SENSOR_TUNING_COMMISSIONING.md`
8. **Sicherheit und Fehlerbehandlung** -> `SAFETY_AND_FAULTS.md`, `SAFETY_COMPONENT_FAULTS.md`, `SYSTEM_SAFETY_AND_RECOVERY.md`
9. **Diagnose, Wartung und Updates** -> `DIAGNOSTICS_AND_MAINTENANCE.md`, `FIRMWARE_UPDATE_AND_ROLLBACK.md`, `RESOURCE_BUDGET_AND_MAINTENANCE.md`
10. **Akzeptanztests und Implementierungsplan** -> `ACCEPTANCE_TESTS.md`, `IMPLEMENTATION_PLAN.md` und spaetere GitHub-Issues

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
- [x] Phase 6: Einstellungen und Persistenz
  - [x] Phase 6A: Einstellungsgruppen, Werkseinstellungen und Aenderungsrechte
  - [x] Phase 6B: Laufpersistenz, Speicherzeitpunkte, Messhistorie und Wiederherstellbarkeit
  - [x] Phase 6C: Geheimnisse, Sicherung, Export, Reset und Datenaufbewahrung
- [x] Phase 7: Temperaturregelung und Aktoren
  - [x] Phase 7A: Regelstrategie, Heiz-/Kuehlbetrieb, Sensorrollen, Zielqualifikation und Laufanpassungen
  - [x] Phase 7B: Schaltfenster, Mindestzeiten, Richtungswechsel und Luefterlogik
  - [x] Phase 7C: Sensorfilter, Kalibrierung, Tuning, Inbetriebnahme und spaetere Regelstrategien
- [x] Phase 8: Sicherheit und Fehler
  - [x] Phase 8A: Fehlerklassen, unmittelbare Reaktionen, Quittierung und Wiederfreigabe
  - [x] Phase 8B: Temperatur-, Sensor-, Luefter- und Aktorfehler
  - [x] Phase 8C: Versorgung, Softwarefehler, sichere Zustaende und Fehlerprotokoll
- [x] Phase 9: Diagnose, Wartung und Updates
  - [x] Phase 9A: Diagnoseansichten, Servicepruefungen und Exporte
  - [x] Phase 9B: UART-Update im ersten Release sowie vorbereitete spaetere OTA-, Rollback- und Migrationsregeln
  - [x] Phase 9C: Ressourcenueberwachung, Speicherpflege, Flashbudget und Wartungsumfang
- [ ] Phase 10: Akzeptanztests und Implementierungsplan
  - [x] Phase 10A: Testebenen, Abnahmekriterien und Release-Gates
  - [ ] Phase 10B: Implementierungsreihenfolge und GitHub-Issues
    - [x] software-first Entwicklung vor Ankunft der Hardware
    - [x] native Simulation und Hardwareabstraktionen
    - [x] geschuetztes ESP32-Bring-up innerhalb derselben Codebasis
    - [x] Branch-, Pull-Request-, Definition-of-Done- und Meilensteinstrategie
    - [ ] konkrete Epic- und Issue-Liste erstellen
  - [ ] Phase 10C: Gesamtreview, offene Punkte, Pull Request und Uebergabe an die Implementierung

## Aktuelle Phase

Als Naechstes wird Phase 10B mit der konkreten Epic- und Issue-Liste abgeschlossen.
