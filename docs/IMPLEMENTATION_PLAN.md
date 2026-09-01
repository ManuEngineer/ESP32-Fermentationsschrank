# Implementierungsplan und Entwicklungsreihenfolge

## Zweck

Dieses Dokument beschreibt die stabile technische Reihenfolge bis Release 1.
Aktueller Arbeitsstand, laufende Pull Requests, Blocker und naechste Aufgaben
stehen ausschliesslich in [`ROADMAP.md`](ROADMAP.md) und den Live-Issues.

Die konkrete Issue- und Epic-Struktur steht in
[`IMPLEMENTATION_ISSUES.md`](IMPLEMENTATION_ISSUES.md). Produkt- und
Releasegrenzen stehen in [`SPECIFICATION_REVIEW.md`](SPECIFICATION_REVIEW.md).
Architektur, Safety und Tests werden nicht hier dupliziert, sondern durch die
zustaendigen ADRs und Fachvertraege festgelegt.

## Grundrichtung

```text
hardwareunabhaengigen Kern nativ entwickeln und testen
-> reale Plattform hinter schmalen Ports integrieren
-> aktorfreie Hardwarebaseline nachweisen
-> Hardware schrittweise und messbar freigeben
-> thermisch abstimmen und Release-Gates abnehmen
```

Es gibt kein separates Wegwerf-Testprojekt. Native Simulation,
ESP-IDF-Produktionspfad und reale Hardware verwenden dieselben fachlichen
Vertraege.

## Entwicklungsprofile

| Profil | Rolle |
|---|---|
| `native` | Fachlogik, Simulation, Mocks und deterministische Hosttests |
| `esp32_bringup` | aktorfreie beziehungsweise explizit gesperrte Hardwareintegration |
| `esp32_release` | Produktionsprofil; Hardwarefreigabe nur nach bestandenen Gates |

Konkrete Build-, Test- und CI-Regeln stehen in
[`CI_AND_QUALITY_GATES.md`](CI_AND_QUALITY_GATES.md). Ein Profilwechsel darf
unbestaetigte Hardware niemals freigeben.

## Softwarephasen

### SW0 – Plattformgrundlage

- Buildprofile und reproduzierbare Tests;
- portable Plattformports und native Testadapter;
- virtuelle monotone und optionale absolute Zeit;
- Architektur-, Secret- und Ressourcenpruefungen.

### SW1 – Fachlicher Laufkern

- Programmmodelle und Standardkatalog;
- unveraenderlicher Laufschnappschuss;
- Zustandsmaschine, Laufkommandos und Abschluss;
- Meldungen, Quittierung und Fehlerreset;
- Wiederherstellung eines persistierten `COMPLETED`.

### SW2 – Persistenz und Recovery

- typisierte Konfigurations- und Laufdaten;
- atomare Revisionen, Rueckfall und Kontrollpunkte;
- Transaktionsabsicht vor aktorwirksamer Zustandsaenderung;
- kritischer Persistenzfehler-Latch und Bootauswertung;
- sichere Unterbrechungs- und Wiederanlauflogik;
- Journale, Aufbewahrung, Backup und Import.

### SW3 – Sensor, Regelung und Safety

- Sensorqualitaet, Filterung und Plausibilitaet;
- Regelsensorauswahl, Ersatzbetrieb und Rueckkehr;
- PI-Regelung und Luftbegrenzung;
- Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik;
- Fehlerklassen, persistente Verriegelungen und `SAFE_BOOT`;
- reproduzierbare Fehlerinjektionen.

Alle Ausgaenge enden in dieser Phase bei abstrakten Aktorbefehlen und Mocks.

### SW4 – Bedienung und Netzwerk

- gemeinsame rendererunabhaengige UI-Modelle und Kommandos;
- lokale Touch-Shell gegen den Simulator;
- Web-API und Weboberflaeche;
- Anmeldung, Sitzungen und Bedienkonflikte;
- Mehrsprachigkeit, Branding und Themevertraege;
- PIN-unabhaengiger lokaler Vollreset als Recoveryablauf.

### SW5 – Diagnose, Exporte und Service

- Diagnosemodelle und Fehlerberichte;
- Lauf-, Diagnose- und Serviceexporte;
- gefuehrter Serviceablauf mit gesperrtem Mockbackend;
- passive `SAFE_BOOT`-Diagnose;
- Ressourcen- und Aufbewahrungsnachweise.

Softwarephasen werden in kleinen vertikalen Funktionsscheiben umgesetzt.
Parallelitaet ist nur zulaessig, wenn Abhaengigkeiten, Schnittstellen und
Owner-Gates nicht vorweggenommen werden.

## Hardwarephasen

### H0 – Sicht- und Aufbaupruefung

- reale Board- und Modulrevisionen erfassen;
- Verdrahtung, Versorgung, Masse, Stecker und Sicherungen dokumentieren;
- Kuehlkoerper, Waermetauscher und Temperatursicherung planen;
- unbekannte Ausgaenge physisch getrennt oder sicher inaktiv halten.

### H1 – Aktorfreie Controllerbaseline

- ESP-IDF-Firmware reproduzierbar flashen, booten und zuruecksetzen;
- ROM-Bootloader- und UART-Recovery nachweisen;
- Flash, Partition, Heap und Resetursachen erfassen;
- GPIO- und Businventar erstellen;
- Boot-, Reset- und Bootloaderverhalten fail-closed funktional pruefen; eine
  elektrische Pegelmessung ist fuer R1 nicht vorgeschrieben.

### H2 – Sensoren, Display und Touch

- DS18B20-Busse, ROM-Adressen und Produkt-Hot-Plug verifizieren;
- Display- und Touchcontroller praktisch bestaetigen;
- Rotation, Kalibrierung, Recovery und Ressourcen messen;
- Renderer- und Treiberauswahl nach Adopt-or-build und Espressif-first treffen.

### H3 – Luefter, Summer und MOSFET-Ausgaenge

- Kanaele und Verbraucherfunktion einzeln funktional pruefen;
- Verbraucher einzeln anschliessen;
- Nachlauf und Bootverhalten dokumentieren; Strom- und Anlaufdaten nur bei
  tatsaechlichem Gate und geeignetem Messmittel erfassen;
- Aussenluefter fuer spaetere Peltierpruefung freigeben.

### H4 – BTS7960 ohne Peltier

- Enable, Richtungen und Pulldowns/fail-low Beschaltung gegen die SSOT
  verifizieren;
- gleichzeitige Richtungsfreigabe ausschliessen;
- Reset und `SAFE_BOOT` mit sicher deaktivierter H-Bruecke pruefen;
- R_IS/L_IS in R1 nicht anschliessen, nicht messen und nicht implementieren;
  eine spaetere Nutzung ist `FUTURE_RELEASE` und benoetigt ein eigenes Issue,
  einen vollstaendigen Plan und ein eigenes Owner-Gate.

### H5 – Begrenzte Peltierpruefung

Vor dem ersten realen Peltierpuls muessen alle Gates aus
[`ACCEPTANCE_TESTS.md`](ACCEPTANCE_TESTS.md) erfuellt sein, insbesondere:

- geeignete Ueberstromsicherung;
- montierte und gepruefte einmalige Temperatursicherung;
- Kuehlkoerper und funktionsgepruefter Aussenluefter;
- gueltige Pflichtsensoren;
- SSOT-konforme BTS7960-Pinbelegung, fail-low AUS-Zustand und nachgewiesener
  Adapterinterlock; Richtung/Polaritaet wird im begrenzten Servicepuls
  funktional bestimmt;
- stabile Versorgung und jederzeitiger Abbruch;
- validiertes `STANDBY` und geschuetzter Serviceablauf.

Der Ablauf bleibt begrenzt und geordnet:

```text
Vorbedingungen pruefen
-> kurzer Heizpuls
-> AUS, Nachlauf, Mindest-Auszeit und Totzeit
-> kurzer Kuehlpuls
-> AUS und Nachlauf
-> Messwerte, Fehlerpfade und Abbruch dokumentieren
```

`SAFE_BOOT` kann keinen Aktortest oder Peltierpuls ausloesen.

## Inbetriebnahmephasen

### C0 – Thermische Grundvermessung

Sensorvergleich, Offsets, leerer Schrank, kleine und grosse Referenzmasse,
Temperaturverteilung sowie Heiz- und Kuehlreaktion messen.

### C1 – Regel- und Sicherheitsparameter

PI-Parameter, Luftbegrenzungen, Mindestzeiten, Totzeit, Nachlauf,
Zielqualifikation und Safetygrenzen anhand dokumentierter Messreihen festlegen.

### C2 – Hardware- und Prozessabnahme

Hardwarematrix, verpflichtende Fehlerinjektionen, Stromunterbrechungen,
Bedienung und Standardprogramme am realen Aufbau validieren.

### C3 – Dauer- und Releaseabnahme

Mindestens sieben zusammenhaengende Tage unter paralleler Regelungs-, UI-,
Web-, Speicher- und Exportlast testen und alle Release-Gates bewerten.

## Meilensteine

| Meilenstein | Ergebnis |
|---|---|
| M0 | Softwaregrundlage und simuliertes System |
| M1 | getesteter fachlicher, persistenter und sicherer Softwarekern |
| M2 | bedienbarer Simulator mit Diagnose und Exportschnittstellen |
| M3 | dokumentierte aktorfreie und schrittweise freigegebene Hardwarebaseline |
| M4 | sichere reale Temperatursteuerung |
| M5 | vollstaendige Integration und praktische Programmvalidierung |
| M6 | Release 1 nach Dauer- und Releaseabnahme |

## Verbindliche Gates

- Ein spaeterer Softwareblock darf einen fehlenden fachlichen oder Safetyvertrag
  nicht still ersetzen.
- Reale Adapter werden erst nach dem zugehoerigen portseitigen Vertrag gebaut.
- Produktive Aktorfreigabe verlangt den Fehler-/Safetykern sowie die owning
  SSOT-, funktionalen Hardware- und, für #33, Adapter-Safety-Gates. Eine
  generelle elektrische Pegelmessung ist fuer R1 nicht erforderlich.
- Thermische Parameter bleiben `TBD_COMMISSIONING`, bis Messnachweise vorliegen.
- Ressourcenwerte bleiben `TBD_IMPLEMENTATION_BUDGET`, bis reproduzierbare Builds
  und Belastungsmessungen vorliegen.
- Ein Neustart, Profilwechsel oder erfolgreicher Einzeltest hebt kein offenes Gate
  auf.

Planung, Branch-, Review- und Owner-Gates stehen ausschliesslich in
[`AGENT_WORKFLOW.md`](AGENT_WORKFLOW.md) und der Root-`AGENTS.md`.
