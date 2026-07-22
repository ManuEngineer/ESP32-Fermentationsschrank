# AGENTS.md

Diese Datei gilt fuer das gesamte Repository.

## Projektziel

Eine sichere, lokal bedienbare ESP32-Firmware fuer einen Fermentationsschrank
entwickeln. Das Geraet regelt mit einem Peltier sowohl Heizen als auch Kuehlen,
bleibt ohne Netzwerk funktionsfaehig und darf bei Fehlern keine unbeabsichtigte
Aktorfreigabe erzeugen.

## Verbindlicher Entwicklungsstand

- Die Release-1-Spezifikation wurde mit PR #38 nach `main` uebernommen.
- Epics #2 bis #8 und Issues #9 bis #37 bilden die geplante Arbeitsstruktur.
- #9 ist das erste Implementierungs-Issue.
- Pro Implementierungs-Issue wird ein eigener Branch und ein kleiner PR verwendet.

## Zielhardware und Grenzen

- ESP32-32E mit 4 MB Flash
- keine PSRAM-Abhaengigkeit
- Peltier 12 V / etwa 60 W ueber BTS7960
- drei DS18B20: Schrankluft, abnehmbares Produkt und Kuehlkoerper
- Innen- und Aussenluefter
- 320-x-240-Touchdisplay
- einmalige Temperatursicherung und 7,5-A-Ueberstromsicherung
- UART/FT232RL als Update- und Wiederherstellungsweg fuer Release 1

Keine GPIO-Zuordnung, kein aktiver Pegel, kein Display-/Touchcontroller und keine
BTS7960-Diagnose darf vor realer Verifikation als bestaetigt behandelt werden.

## Dokumentationsprioritaet

Die verbindliche und vollstaendige Reihenfolge bei Dokumentationswiderspruechen
steht ausschliesslich in `docs/SPECIFICATION_REVIEW.md` im Abschnitt
`Dokumentationsprioritaet`. Kurzfassungen in Einstiegs- oder Agentendokumenten
duerfen diese Reihenfolge nicht ersetzen.

Akzeptierte ADRs werden im zentralen Register `docs/DECISIONS.md` gefuehrt.
Ausfuehrliche ADR-Einzeldokumente duerfen das Register ergaenzen, aber nicht
ersetzen.

Zentrale Einstiege:

- `docs/SPECIFICATION_PLAN.md`
- `docs/IMPLEMENTATION_PLAN.md`
- `docs/IMPLEMENTATION_ISSUES.md`
- `docs/ACCEPTANCE_TESTS.md`
- `docs/OPEN_POINTS.md`
- `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`
- `docs/CI_AND_QUALITY_GATES.md`
- `lib/README.md`

## Architekturregeln

Der fachliche Kern wird soweit sinnvoll ohne Arduino-Abhaengigkeit umgesetzt.
Direkte Hardwarezugriffe gehoeren in Adapter.

Der Kern darf insbesondere nicht direkt verwenden:

- `digitalWrite` oder `analogRead`
- 1-Wire-, Display- oder WLANbibliotheken
- konkrete Dateisystemaufrufe
- globale reale Zeit ohne abstrahierte Zeitquelle

Vorgesehene Profile:

```text
native
esp32_bringup
esp32_release
```

`esp32_bringup` startet mit gesperrten Aktoren und dem sichtbaren Zustand
`HARDWARE_UNVERIFIED`. Ein Wechsel auf `esp32_release` darf unbekannte Hardware
nicht automatisch freigeben.

## Modul- und Wiederverwendungsregeln

ADR-013 ist verbindlich. Die Firmware trennt:

```text
src/main.cpp            Composition Root
lib/device_platform/    anwendungsneutrale Geraetedienste
lib/fermentation_app/   konkrete Fermentationsanwendung
```

- `main.cpp` verbindet Module und enthaelt keine Prozess-, Regel-, Persistenz-
  oder Aktorlogik.
- `fermentation_app` darf nur schmale Plattform-Schnittstellen verwenden und
  kennt weder Arduino noch die konkrete Klasse `DevicePlatform`.
- `device_platform` darf keine Fermentationsbegriffe, Fermentationszustaende oder
  Abhaengigkeit auf `fermentation_app` enthalten.
- Die projektspezifische `app_config.hpp` bleibt ausserhalb der
  wiederverwendbaren Plattform.
- Allgemeine Module muessen im Profil `native` testbar sein.
- Keine vorschnelle Auslagerung oder Universalplattform: Ein separates
  Plattform-Repository entsteht erst bei einem zweiten realen Anwendungsfall
  oder einem klaren unabhaengigen Wartungsvorteil.

Fuer Dateien innerhalb der beiden Modulverzeichnisse gelten zusaetzlich die dort
liegenden `AGENTS.md`.

## Sicherheitsregeln

- Peltier und H-Bruecke bleiben bei Boot, Reset, Fehler und unklarer Lage AUS.
- Heizen und Kuehlen koennen nie gleichzeitig freigegeben werden.
- Richtungswechsel erzwingen Abschaltung, Mindest-Auszeit und Totzeit.
- Sicherheitsabschaltungen ueberstimmen Mindest-Einschaltzeiten.
- Schrankluft- und Kuehlkoerpersensor sind fuer jede Peltierfreigabe erforderlich.
- Direkte GPIO- oder Aktorzustaende werden nie als Wiederanlaufzustand gespeichert.
- Ein Neustart ist kein Fehlerreset.
- `Quittieren` und `Fehler zuruecksetzen` bleiben getrennt.
- Service-PIN oder Webzugang umgehen keine firmwarefesten Grenzen.
- Web, WLAN, Display und Exporte duerfen Regel- und Sicherheitsaufgaben nicht
  blockieren.

## Lauf- und Konfigurationsregeln

- Ein Lauf verwendet einen unveraenderlichen Programmschnappschuss.
- Das Quellprogramm wird durch einen laufenden Prozess nicht still veraendert.
- Zieltemperatur und verbleibende Dauer duerfen nur ueber die ausdruecklich
  spezifizierte Laufaktion mit Vorschau, Bestaetigung, Validierung und
  protokollierter Laufrevision geaendert werden.
- Andere normale Einstellungen veraendern einen aktiven Lauf nicht.
- Konfigurationen und Kontrollpunkte werden atomar, versioniert und mit
  Rueckfallrevision gespeichert.
- Es wird nicht in jedem Sensorzyklus in Flash geschrieben.
- Historien und Puffer sind fest begrenzt.

## Release-1-Abgrenzung

Nicht als Release-1-Funktion implementieren:

- Web-OTA oder duale OTA-Slots
- automatischer Firmwaredownload
- benutzeraktivierbare UART-Diagnose
- Cloud- oder Pushpflicht
- eigener WireGuard-Client
- Tuerkontakt
- verpflichtende RTC
- verpflichtende 12-V-ADC-Messung
- Kaskadenregelung oder PID-Autotuning
- automatische Wartungserinnerungen

Schnittstellen fuer spaetere Erweiterungen duerfen vorbereitet werden, solange
keine ungenutzten grossen Bibliotheken, Speicherpuffer oder aktorfaehigen
Zukunftsfunktionen eingebaut werden.

## Umgang mit offenen Werten

- `TBD_HARDWARE`: reale Komponente, Pin, Pegel oder Verdrahtung fehlt
- `TBD_COMMISSIONING`: thermischer, regelungstechnischer oder prozessbezogener
  Wert wird am realen Schrank bestimmt
- `TBD_IMPLEMENTATION_BUDGET`: reale Build-, Heap- oder Flashmessung fehlt
- `FUTURE_RELEASE`: bewusst nicht Bestandteil von Release 1

Kein solcher Platzhalter darf in der fertigen Firmware unbemerkt als gueltiger
Laufzeitwert verwendet werden.

## Tests und Definition of Done

Jedes Issue erfuellt alle zutreffenden Punkte:

- Implementierung vollstaendig
- native, simulierte oder Hardwaretests vorhanden und bestanden
- ESP32-Zielbuild erfolgreich, soweit relevant
- Ressourcenwirkung geprueft oder sichtbar als budgetabhaengig markiert
- Fehlerfaelle behandelt
- Dokumentation aktualisiert
- keine Geheimnisse eingecheckt
- keine unbestaetigte Hardwareannahme als Tatsache implementiert
- Akzeptanzkriterien des Issues erfuellt

Hardwareunabhaengige Logik darf vor Hardwareankunft abgeschlossen werden. Die
reale Verifikation bleibt in einem verknuepften `BLOCKED_HARDWARE`-Issue sichtbar.
