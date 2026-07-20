# Hardware

## Statuskennzeichnung

| Kennzeichnung | Bedeutung |
|---|---|
| `confirmed` | Am realen Bauteil, Datenblatt oder Schaltplan bestaetigt |
| `candidate` | Vorgesehene Auswahl, noch nicht am realen Aufbau geprueft |
| `unconfirmed` | Projektspezifisch genannt, elektrische Details noch offen |
| `unknown` | Noch offen; darf im Code nicht als Fakt verwendet werden |

## Controller

| Eigenschaft | Wert | Status |
|---|---|---|
| PlatformIO-Ziel | `esp32dev` | candidate |
| Modul | generisches ESP32-WROOM-32E | candidate |
| Framework | Arduino | confirmed (Projektvorgabe) |
| Versorgung | unbekannt | unknown |
| Boardrevision | unbekannt | unknown |
| Flash/PSRAM | unbekannt | unknown |
| Programmierschnittstelle | unbekannt | unknown |

`esp32dev` ist das generische Build-Ziel und keine Bestaetigung einer konkreten
Boardrevision oder deren Pinbelegung.

## Sensoren

| Sensor | Menge | Schnittstelle | Versorgung | Zweck | Status |
|---|---:|---|---|---|---|
| DS18B20 | unbekannt | OneWire | extern/parasitisch unbekannt | Temperatur | unconfirmed |

ROM-Adressen, Bus-Pin, Pull-up, Aufloesung, Einbauorte und zulaessige
Plausibilitaetsgrenzen sind noch zu bestaetigen.

## Aktoren

| Aktor | Versorgung | Maximalstrom | Treiber | Sicherer Zustand | Status |
|---|---|---:|---|---|---|
| Peltier-Element | unbekannt | unbekannt | BTS7960-H-Bruecke | AUS, Bruecke deaktiviert | unconfirmed |
| Onboard-MOSFET 1-4 | unbekannt | unbekannt | Board-MOSFETs | AUS | unconfirmed |

Die vier MOSFET-Lasten und ihre Schutzbeschaltung sind unbekannt. Heizen und
Kuehlen verwenden dasselbe Peltier-Element mit umgekehrter Stromrichtung und
duerfen nie gleichzeitig angesteuert werden.

## Bedienung und Anzeige

| Bauteil | Schnittstelle | Zweck | Status |
|---|---|---|---|
| ILI9341 | SPI | Anzeige | unconfirmed |
| XPT2046 | SPI | Touch-Eingabe | unconfirmed |

Gemeinsame SPI-Nutzung ist ein Kandidat, jedoch nicht bestaetigt. Pinbelegung,
Chip-Select-Pegel, Rotation, Touchkalibrierung und Hintergrundbeleuchtung sind
offen.

## Stromversorgung

Die Versorgungstopologie, gemeinsame Massefuehrung, Peltier-Spannung,
Logikversorgung, Stromreserven und Trennung der Lastkreise sind `unknown`.

## Schutz

- [ ] Hauptsicherung dimensioniert
- [ ] Teilstromkreise abgesichert
- [ ] Verpolschutz geprueft
- [ ] sichere Bootzustaende aller Ausgaenge gemessen
- [ ] unabhaengiger Temperatur-/Stromschutz bewertet
- [ ] BTS7960-Kuehlung und Peltier-Waermeabfuhr ausgelegt
- [ ] Freilaufschutz fuer induktive MOSFET-Lasten bestaetigt
- [ ] Leitungsquerschnitte dokumentiert

## Quellen

- Herstellerdatenblaetter der konkret verbauten Komponenten: unknown
- Schaltplan und Boardrevision: unknown
- Fotos/Produktseite des konkreten Aufbaus: unknown

## Noch zu bestaetigen

Siehe `docs/OPEN_POINTS.md`. Bis zur Bestaetigung gilt jede GPIO-Zuordnung in
`config/pins.example.yaml` als `unknown` und bleibt `null`.
