# Plan zur Softwarespezifikation

## Ziel

Die Firmware wird vor der Implementierung so beschrieben, dass keine wesentliche
Produkt-, Sicherheits- oder Hardwareentscheidung durch Codex oder einen
Entwickler erfunden werden muss.

## Regeln

- Sicherheitsanforderungen haben Vorrang vor Komfortfunktionen.
- Unbestaetigte GPIOs, Pegel und Moduldetails bleiben `TBD_HARDWARE`.
- Thermische und regelungstechnische Werte bleiben `TBD_COMMISSIONING`.
- Reale Flash-, Heap- und Puffergrenzen bleiben `TBD_IMPLEMENTATION_BUDGET`.
- Bewusst spaetere Funktionen werden `FUTURE_RELEASE` zugeordnet.
- Kein Platzhalter darf als produktiver Laufzeitwert verwendet werden.
- Implementierung beginnt erst nach Merge des Spezifikations-Pull-Requests.

## Dokumentationsprioritaet

1. spaetere akzeptierte ADRs in `DECISIONS.md`
2. `PR38_REVIEW_CORRECTIONS.md`
3. `SPECIFICATION_REVIEW.md`
4. thematisch spezialisierte Spezifikationsdokumente
5. `REQUIREMENTS.md`, `ARCHITECTURE.md` und `HARDWARE.md`
6. Beispielkonfigurationen
7. historische Phasen- und Revisionsnotizen

Die aktuelle Liste echter offener Punkte steht in `OPEN_POINTS.md`. Verbindliche
Korrekturen aus den Reviews von PR #38 stehen in
`PR38_REVIEW_CORRECTIONS.md`.

## Zentrale Quellen

### Einstieg und Review

- [`PR38_REVIEW_CORRECTIONS.md`](PR38_REVIEW_CORRECTIONS.md): verbindliche
  Sicherheits- und Konsistenzkorrekturen aus beiden Reviews von PR #38
- [`SPECIFICATION_REVIEW.md`](SPECIFICATION_REVIEW.md): Ergebnis von Phase 10C,
  Prioritaeten und verbleibende offene Kategorien
- [`REQUIREMENTS.md`](REQUIREMENTS.md): konsolidierte Release-1-Muss-Anforderungen
- [`ARCHITECTURE.md`](ARCHITECTURE.md): Softwarestruktur, Ports und Adapter
- [`HARDWARE.md`](HARDWARE.md): konsolidierter Hardwarestand
- [`OPEN_POINTS.md`](OPEN_POINTS.md): reale Hardware-, Inbetriebnahme- und
  Ressourcenpunkte
- [`DECISIONS.md`](DECISIONS.md): Architekturentscheidungen

### Prozess und Bedienung

- [`PRODUCT_VISION.md`](PRODUCT_VISION.md)
- [`PROGRAMS.md`](PROGRAMS.md)
- [`STANDARD_PROGRAMS.md`](STANDARD_PROGRAMS.md)
- [`STATE_MACHINE.md`](STATE_MACHINE.md)
- [`RECOVERY_AND_INTERRUPTION.md`](RECOVERY_AND_INTERRUPTION.md)
- [`RUNTIME_BEHAVIOR.md`](RUNTIME_BEHAVIOR.md)
- [`LOCAL_UI.md`](LOCAL_UI.md)
- [`LOCAL_UI_PROGRAMS.md`](LOCAL_UI_PROGRAMS.md)
- [`LOCAL_RUNTIME_UI.md`](LOCAL_RUNTIME_UI.md)
- [`LOCAL_UI_SETTINGS_SERVICE.md`](LOCAL_UI_SETTINGS_SERVICE.md)
- [`WEB_UI.md`](WEB_UI.md)
- [`NETWORK.md`](NETWORK.md)
- [`NETWORK_DIAGNOSTICS_INTEGRATION.md`](NETWORK_DIAGNOSTICS_INTEGRATION.md)

### Persistenz, Regelung und Sicherheit

- [`SETTINGS_AND_STORAGE.md`](SETTINGS_AND_STORAGE.md)
- [`CONFIGURATION_PERSISTENCE.md`](CONFIGURATION_PERSISTENCE.md)
- [`RUN_PERSISTENCE.md`](RUN_PERSISTENCE.md)
- [`BACKUP_SECURITY_RETENTION.md`](BACKUP_SECURITY_RETENTION.md)
- [`TEMPERATURE_CONTROL.md`](TEMPERATURE_CONTROL.md)
- [`ACTUATOR_TIMING.md`](ACTUATOR_TIMING.md)
- [`SENSOR_TUNING_COMMISSIONING.md`](SENSOR_TUNING_COMMISSIONING.md)
- [`SAFETY_AND_FAULTS.md`](SAFETY_AND_FAULTS.md)
- [`SAFETY_COMPONENT_FAULTS.md`](SAFETY_COMPONENT_FAULTS.md)
- [`SYSTEM_SAFETY_AND_RECOVERY.md`](SYSTEM_SAFETY_AND_RECOVERY.md)

### Diagnose, Ressourcen und Umsetzung

- [`DIAGNOSTICS_AND_MAINTENANCE.md`](DIAGNOSTICS_AND_MAINTENANCE.md)
- [`FIRMWARE_UPDATE_AND_ROLLBACK.md`](FIRMWARE_UPDATE_AND_ROLLBACK.md)
- [`RESOURCE_BUDGET_AND_MAINTENANCE.md`](RESOURCE_BUDGET_AND_MAINTENANCE.md)
- [`ACCEPTANCE_TESTS.md`](ACCEPTANCE_TESTS.md)
- [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md)
- [`IMPLEMENTATION_ISSUES.md`](IMPLEMENTATION_ISSUES.md)
- [`CODEX_HANDOFF.md`](CODEX_HANDOFF.md)

### Maschinenlesbare Beispiele

- [`../config/programs.example.yaml`](../config/programs.example.yaml)
- [`../config/hardware.example.yaml`](../config/hardware.example.yaml)
- [`../config/pins.example.yaml`](../config/pins.example.yaml)

## Phasen

1. **Produktvision und Nutzung** -> `PRODUCT_VISION.md`
2. **Programme und Prozessablauf** -> `PROGRAMS.md`, `STANDARD_PROGRAMS.md`
3. **Betriebszustaende und Uebergaenge** -> `STATE_MACHINE.md`,
   `RECOVERY_AND_INTERRUPTION.md`, `RUNTIME_BEHAVIOR.md`
4. **Lokale Touch-Bedienung** -> `LOCAL_UI.md`, `LOCAL_UI_PROGRAMS.md`,
   `LOCAL_RUNTIME_UI.md`, `LOCAL_UI_SETTINGS_SERVICE.md`
5. **Weboberflaeche und Netzwerk** -> `WEB_UI.md`, `NETWORK.md`,
   `NETWORK_DIAGNOSTICS_INTEGRATION.md`
6. **Einstellungen und Persistenz** -> `SETTINGS_AND_STORAGE.md`,
   `RUN_PERSISTENCE.md`, `BACKUP_SECURITY_RETENTION.md`
7. **Temperaturregelung und Aktorlogik** -> `TEMPERATURE_CONTROL.md`,
   `ACTUATOR_TIMING.md`, `SENSOR_TUNING_COMMISSIONING.md`
8. **Sicherheit und Fehlerbehandlung** -> `SAFETY_AND_FAULTS.md`,
   `SAFETY_COMPONENT_FAULTS.md`, `SYSTEM_SAFETY_AND_RECOVERY.md`
9. **Diagnose, Wartung und Updates** -> `DIAGNOSTICS_AND_MAINTENANCE.md`,
   `FIRMWARE_UPDATE_AND_ROLLBACK.md`, `RESOURCE_BUDGET_AND_MAINTENANCE.md`
10. **Akzeptanztests und Implementierungsplan** -> `ACCEPTANCE_TESTS.md`,
    `IMPLEMENTATION_PLAN.md`, `IMPLEMENTATION_ISSUES.md`, GitHub-Issues,
    `SPECIFICATION_REVIEW.md` und `PR38_REVIEW_CORRECTIONS.md`

## Abschlussstand

- [x] Phase 1: Produktvision und Nutzung
- [x] Phase 2: Programme und Prozessablauf
- [x] Phase 3: Zustandsmaschine und Wiederanlauf
- [x] Phase 4: Lokale Bedienung
- [x] Phase 5: Web und Netzwerk
- [x] Phase 6: Einstellungen und Persistenz
- [x] Phase 7: Temperaturregelung und Aktoren
- [x] Phase 8: Sicherheit und Fehler
- [x] Phase 9: Diagnose, Ressourcen und Updates
- [x] Phase 10A: Testebenen, Abnahmekriterien und Release-Gates
- [x] Phase 10B: Implementierungsreihenfolge, Epics und Issues
  - [x] software-first Entwicklung
  - [x] native Simulation und Hardwareabstraktionen
  - [x] `esp32_bringup` innerhalb derselben Codebasis
  - [x] sieben Epics #2 bis #8
  - [x] 29 Arbeits- und Abnahme-Issues #9 bis #37
  - [x] erstes Implementierungs-Issue #9 festgelegt
- [x] Phase 10C: Gesamtreview und Uebergabe
  - [x] zentrale Alt-Dokumente konsolidiert
  - [x] falsche zentrale Dateiverweise korrigiert
  - [x] offene Punkte nach Hardware, Inbetriebnahme, Budget und Zukunft getrennt
  - [x] Draft-PR #38 erstellt
  - [x] Issue #1 und Uebergabedokumente aktualisiert
  - [x] beide PR-Reviews ausgewertet
  - [x] verbindliche Reviewkorrekturen dokumentiert
  - [x] alle Inline-Reviewthreads beantwortet und geschlossen
  - [ ] Owner-Review und Merge abgeschlossen

## Ergebnis

Die Dokumentationsphase ist inhaltlich abgeschlossen. Pull Request #38 bleibt bis
zum Owner-Review und Merge offen. Nach dem Merge wird Issue #9 auf `READY`
gesetzt und die Implementierung auf einem eigenen Branch begonnen.

Exakte Temperaturen, Zeiten, Regelparameter, GPIOs und Ressourcenbudgets werden
nicht als fehlende Spezifikation gewertet, weil sie durch die realen Hardware-,
Inbetriebnahme- und Belastungstests #29 bis #37 bestimmt werden muessen.
