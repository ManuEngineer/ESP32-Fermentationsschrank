# Implementierungs-Epics und GitHub-Issues

## Status

Dieses Dokument schliesst Phase 10B ab und bildet die verbindliche geplante
Implementierungsstruktur ab.

Die Issues wurden bereits in GitHub angelegt. Bis zum Merge des
Spezifikations-Pull-Requests besitzen softwareseitige Issues den Status
`PLANNED_SPEC_PENDING`. Es beginnt vorher keine eigentliche Implementierung.

## Statuskennzeichnungen

- `PLANNED_SPEC_PENDING`: geplant, Umsetzung erst nach Merge der Spezifikation
- `READY`: fachlich und technisch startbereit
- `BLOCKED_HARDWARE`: reale Hardware oder Messung fehlt
- `TBD_COMMISSIONING`: Wert oder Freigabe wird bei thermischer Inbetriebnahme bestimmt
- `TBD_IMPLEMENTATION_BUDGET`: Entscheidung benoetigt reale Build-/Ressourcenmessung
- `BLOCKED`: andere benannte Abhaengigkeit verhindert die Umsetzung

Diese Kennzeichnungen stehen zunaechst verbindlich im Issue-Text. GitHub-Labels
koennen spaeter ergaenzt werden, sind aber keine Voraussetzung fuer die Planung.

## Epic-Uebersicht

| Epic | GitHub | Zweck |
|---|---:|---|
| E0 | #2 | Projektgrundlage, Tests, Abstraktionen und Simulator |
| E1 | #3 | Programme und fachlicher Softwarekern |
| E2 | #4 | Konfiguration, Persistenz und Wiederanlauf |
| E3 | #5 | Sensor-, Regel- und Sicherheitskern |
| E4 | #6 | Lokale Bedienung, Web und Diagnose |
| E5 | #7 | ESP32- und Hardwareintegration |
| E6 | #8 | Inbetriebnahme und Release 1 |

## E0 – Projektgrundlage und Testbarkeit

- #9 `PlatformIO-Profile und Projektgrundlage einrichten`
- #10 `Native Tests, CI, virtuelle Zeit und Buildberichte`
- #11 `Hardwareabstraktionen, Mockadapter und Simulator`

### Abhaengigkeit

```text
#9 -> #10 -> #11
```

#9 ist nach Merge der Spezifikation das erste geplante Implementierungs-Issue.

## E1 – Programme und fachlicher Softwarekern

- #12 `Programmmodelle, Schema und Standardprogramme`
- #13 `Unveraenderlichen Laufschnappschuss und Laufrevisionen implementieren`
- #14 `Zustandsmaschine und Prozessablaeufe implementieren`
- #15 `Laufkommandos, Meldungen und Bedienaktionen implementieren`

### Abhaengigkeit

```text
#9/#10 -> #12 -> #13 -> #14 -> #15
           \----------------------/
             teilweise parallel
```

#14 benoetigt zusaetzlich virtuelle Zeit und Mockadapter aus #10/#11.

## E2 – Konfiguration, Persistenz und Wiederanlauf

- #16 `Konfigurationsebenen, Validierung und atomare Revisionen`
- #17 `Laufpersistenz und Kontrollpunkte implementieren`
- #18 `Wiederanlauf und temperaturgewichteten Fortschritt implementieren`
- #19 `Journale, Aufbewahrung, Bereinigung, Backup und Import`

### Abhaengigkeit

```text
#12 -> #16
#13/#14/#16 -> #17
#10/#14/#17/#20 -> #18
#16/#17 -> #19
```

Das native Testbackend wird zuerst umgesetzt. Der reale ESP32-Speicheradapter wird
bei der Hardwareintegration ergaenzt.

## E3 – Sensor-, Regel- und Sicherheitskern

- #20 `Sensorqualitaet, Filterung und Plausibilitaet implementieren`
- #21 `Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik`
- #22 `Zeitproportionale PI-Regelung und Luftbegrenzung`
- #23 `Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik`
- #24 `Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion`

### Abhaengigkeit

```text
#10/#11 -> #20 -> #21 -> #22 -> #23
#14/#15/#17/#20/#21/#23 -> #24
```

Alle Ausgaenge enden in dieser Phase bei abstrakten Aktorbefehlen und Mockadaptern.
Es werden keine unbestaetigten GPIOs verwendet.

## E4 – Lokale Bedienung, Web und Diagnose

- #25 `Gemeinsame UI-Modelle, Navigation und Mehrsprachigkeit`
- #26 `Lokale Touchoberflaeche fuer Programme, Lauf und Service`
- #27 `Web-API, Weboberflaeche, Anmeldung und Bedienkonflikte`
- #28 `Diagnose, Diagramme, Serviceablauf und Exporte`

### Abhaengigkeit

```text
#12/#14/#15/#20/#24 -> #25
#25/#12/#15 -> #26
#25/#12/#15/#16 -> #27
#19/#20/#22/#23/#24/#25 -> #28
```

Vor Ankunft der Hardware werden View-Modelle, Navigation, Texte, Weboberflaeche,
Programmeditor, Laufansichten, Diagnosemodelle und Serviceablaeufe gegen den
Simulator entwickelt.

Reale Displayinitialisierung, Touchcontroller, Rotation und Kalibrierung sind
separat in #31 enthalten.

## E5 – ESP32- und Hardwareintegration

- #29 `ESP32-Bring-up, Partition, Ressourcen und sichere Ausgangszustaende`
- #30 `DS18B20-Busse und reale Sensoradapter integrieren`
- #31 `Display- und Touchadapter integrieren und kalibrieren`
- #32 `Luefter, Summer und Onboard-MOSFET-Ausgaenge integrieren`
- #33 `BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen`

### Abhaengigkeit

```text
#9/#10/#11/#24 -> #29
#20/#21/#29 -> #30
#25/#26/#29 -> #31
#23/#24/#28/#29 -> #32
#23/#24/#29/#30/#32 -> #33
```

Diese Issues sind bis zur realen Hardwareverfuegbarkeit `BLOCKED_HARDWARE`.
Softwareadapter und Treiberschnittstellen duerfen vorher vorbereitet werden, aber
keine reale Hardwarefunktion gilt ohne Messung als bestaetigt.

### Verbindliche elektrische Reihenfolge

1. Controllerboard und Ausgaenge ohne angeschlossene Aktoren messen.
2. Sensoren, Display und Touch einzeln integrieren.
3. Luefter und Summer einzeln anschliessen und messen.
4. BTS7960 ohne angeschlossenes Peltier pruefen.
5. H-Brueckenausgang und Polaritaet mit Multimeter bestaetigen.
6. Peltier erst mit Sicherung, Lueftern, Kuehlkoerper und gueltigen
   Sicherheitssensoren anschliessen.
7. Nur begrenzte Servicepulse fuer Heizen und Kuehlen ausfuehren.

## E6 – Inbetriebnahme und Release 1

- #34 `Sensorvergleich, Offsets und thermische Grundvermessung`
- #35 `PI-Parameter, Luftbegrenzungen und Sicherheitsgrenzen festlegen`
- #36 `Hardwareabnahme, Fehlerinjektionen und Standardprogramme validieren`
- #37 `Siebentaegigen Belastungstest und Release-1-Abnahme durchfuehren`

### Abhaengigkeit

```text
#29-#33 -> #34 -> #35 -> #36 -> #37
```

Die Issues bleiben bis zur realen thermischen Inbetriebnahme
`TBD_COMMISSIONING`.

## Meilensteinzuordnung

| Meilenstein | Zugehoerige Issues |
|---|---|
| M0 – Softwaregrundlage und simuliertes System | #9–#11 |
| M1 – Getesteter Softwarekern | #12–#24 |
| M2 – Bedienbarer Simulator | #25–#28 |
| M3 – Hardware Bring-up | #29–#33 |
| M4 – Sichere Temperatursteuerung | #30, #32–#35 |
| M5 – Vollstaendige Integration | #26–#33, #36 |
| M6 – Release 1 | #34–#37 |

## Branch- und Pull-Request-Regeln

- Spezifikationsbranch zuerst als eigener Pull Request nach `main`.
- Danach ein Branch pro Implementierungs-Issue.
- Kleine, pruefbare Pull Requests.
- Keine umfangreiche direkte Implementierung auf `main`.
- Hardwareblockaden verhindern nicht die Entwicklung unabhaengiger Softwareteile.

Vorgeschlagener erster Branch nach Spezifikationsmerge:

```text
foundation/platformio-profiles
```

zu Issue #9.

## Definition of Done je Implementierungs-Issue

Ein Issue ist nur abgeschlossen, wenn alle zutreffenden Punkte erfuellt sind:

- Implementierung vollstaendig
- native, simulierte oder Hardwaretests vorhanden und bestanden
- ESP32-Zielbuild erfolgreich, soweit relevant
- Ressourcenwirkung geprueft oder sichtbar hardwareabhaengig markiert
- Fehlerfaelle behandelt
- Dokumentation aktualisiert
- keine Geheimnisse eingecheckt
- keine unbestaetigte Hardwareannahme als Tatsache implementiert
- Akzeptanzkriterien des Issues erfuellt

Ein hardwareunabhaengiges Software-Issue darf vor Hardwareankunft abgeschlossen
werden. Die verbleibende reale Validierung muss dann in einem verknuepften
`BLOCKED_HARDWARE`-Issue stehen.

## Freigaberegel

Die Issues sind angelegt, aber die Implementierung startet erst nach:

1. Gesamtreview in Phase 10C
2. geklaerten kritischen Widerspruechen
3. erstellt und geprueftem Spezifikations-Pull-Request
4. Merge des Spezifikations-Pull-Requests nach `main`

Danach wird #9 von `PLANNED_SPEC_PENDING` auf `READY` gesetzt.
