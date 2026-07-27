# Hardware-Spike-Plan

Zur Auditnavigation: [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).

## Zweck und Sicherheitsgrenze

Die Spikes entscheiden nur ueber Hardwaretreiber und technische Adapter. Sie
implementieren weder fachliche UI, Sensor-Safety, Regelung noch produktive
Aktorfreigaben. Alle Peltier-, H-Bruecken-, Luefter- und MOSFET-Ausgaenge
bleiben waehrend dieser Spikes elektrisch getrennt oder nachweislich AUS. Der
Summer bleibt ebenfalls getrennt und wird nicht angesteuert.

Die Spikes muessen nicht auf den Abschluss von #20–#24 warten. Nach der Audit-
und Planungsbereinigung genuegt die folgende minimale sichere Hardwarebaseline;
die hardwareunabhaengige Safety-Kette kann parallel weiterlaufen.

## Minimale sichere Hardwarebaseline vor den Spikes

Vor jedem Display-, Touch- oder Sensorspike werden dokumentiert und
nachgewiesen:

- reale ESP32-Boardrevision;
- erfolgreiche Verbindung ueber UART beziehungsweise FT232RL;
- reproduzierbarer Flash-, Boot- und Resetablauf;
- reale Flashgroesse;
- Versorgungsspannungen und sichere Einspeisung;
- verwendete PlatformIO- und Arduino-ESP32-Version;
- Betrieb ohne PSRAM;
- Baselinewerte fuer Firmwaregroesse, statisches RAM, freien Heap und groessten
  freien Heapblock;
- verfuegbare GPIOs und grundsaetzlich moegliche Busse, ohne eine produktive
  Belegung festzulegen;
- physische Trennung oder nachweisliche Inaktivitaet aller Aktorpfade.

Mindestens Peltier, BTS7960, Innenluefter, Aussenluefter, alle
MOSFET-Verbraucher und der Summer bleiben getrennt oder nachweislich inaktiv.
Der Summer wird in keinem Display- oder Sensorspike angesteuert.

Diese Baseline ist der kleinste vorziehbare Anteil von #29. Sie ist ein sicheres
Mess-, Build- und Flashfundament, kein vollstaendiges Hardware-Bring-up. Der
Audit empfiehlt eine spaetere Aufteilung von #29, aendert das Issue aber nicht.

### Nicht Bestandteil der minimalen Baseline

Die Baseline legt noch nicht fest:

- endgueltige produktive Pinbelegung;
- finale Display-, Touch- oder DS18B20-Bibliothek;
- endgueltige Sensorbustopologie;
- Luefter-, Summer- oder BTS7960-Adapter;
- Peltierbetrieb oder andere produktive Aktorfreigaben;
- Safety-Grenzwerte oder PI-Parameter;
- finale produktive Partitionierung.

## Drei verbindliche Gates pro Kandidat

Jeder Kandidat durchlaeuft die folgenden Gates in dieser Reihenfolge. Ein
negatives Ergebnis wird dokumentiert; es wird weder durch einen Toolchainwechsel
noch durch einen verfruehten Hardwaretest umgangen.

### Gate 1 – Quelle, Lizenz und Kompatibilitaetsvertrag

Zu dokumentieren sind:

- offizielle Projektquelle oder lokale Herstellerquelle;
- konkrete Version beziehungsweise Commit;
- Lizenz und betroffene Dateien;
- transitive Abhaengigkeiten;
- deklarierte ESP32-Unterstuetzung;
- Unterstuetzung der benoetigten Controller beziehungsweise Sensoren;
- erwartete Kompatibilitaet mit der fixierten Toolchain;
- erforderliche Konfigurationsdateien oder Buildflags;
- moegliche Veroeffentlichungseinschraenkungen.

Fuer das LCDWiki-Paket bleibt die interne technische Evaluation freigegeben.
Fehlende paketweite Lizenzklarheit blockiert die Untersuchung nicht. Direkte
Codeuebernahme, abgeleitete Eigenimplementierung und reine Referenznutzung
werden getrennt dokumentiert. Vor einer oeffentlichen Veroeffentlichung direkt
uebernommener Dateien ist eine konkrete Dateipruefung erforderlich; der Spike
erteilt keine allgemeine Publikationsfreigabe.

### Gate 2 – Reproduzierbarer Build ohne reale Aktoren

Der Kandidat wird in einem isolierten Spike-Build mit der bestehenden
PlatformIO-/Arduino-ESP32-Toolchain eingebunden. Konfiguration, Buildflags und
transitive Abhaengigkeiten muessen reproduzierbar sein. Ein Toolchain- oder
Frameworkwechsel ist nicht zulaessig. Der Build und jeder Lauf bleiben ohne
Peltier-, BTS7960-, Innen-/Aussenluefter-, MOSFET- oder Summeraktivierung und
ermoeglichen einen Base-/Kandidaten-Ressourcenvergleich.

Ein Kandidat, der Gate 2 nicht besteht, erreicht Gate 3 nicht. Zulaessige
typisierte Ergebnisse sind insbesondere:

```text
INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN
BUILD_CONFIGURATION_NOT_REPRODUCIBLE
UNRESOLVED_TRANSITIVE_DEPENDENCY
REQUIRES_UNAPPROVED_TOOLCHAIN_CHANGE
```

### Gate 3 – Identischer realer Hardwaretest

Nur Kandidaten, die Gate 1 und Gate 2 ausreichend bestehen, durchlaufen die
vollstaendige Hardwarematrix. Zwischen den Kandidaten bleiben gleich:

- dasselbe ESP32-Board;
- dasselbe Display beziehungsweise dieselben Sensoren;
- dieselbe Versorgung und dieselben Leitungen;
- dieselbe gemessene Pinbelegung und Buskonfiguration;
- dieselben Compilerflags und Testinhalte;
- dieselbe Messmethode und Anzahl Wiederholungen.

## Gemeinsame Spikebedingungen

Zusaetzlich gilt:

- PlatformIO `espressif32@7.0.1`, Arduino-ESP32 `2.0.17` (`dcc1105b`), C++17;
- 4 MB Flash, keine PSRAM-Nutzung;
- je Kandidat eigener wegwerfbarer Spike-Branch oder isolierter Build, keine
  Vermischung der Kandidaten;
- identische Compilerflags, Displayinhalte, Sensorablaeufe und Messmethode;
- Version/Commit, Konfiguration und Buildartefakte archivieren;
- keine Kandidatenaussage als bestaetigte Produktverdrahtung uebernehmen.

## Spike A: Display und Touch

### Ziel

Den kleinsten stabilen Treiber fuer das tatsaechliche MSP2807-Modul bestimmen
und ILI9341, Touchcontroller, Rotation, Reset, Shared-SPI-Verhalten sowie
Ressourcen auf der Zieltoolchain belegen. Das Touchdisplay ist die einzige
lokale Bedien- und Anzeigeoberflaeche; der Spike plant keine parallelen lokalen
Eingaben oder Anzeigen.

### Kandidaten

1. LovyanGFX `1.2.26` (`3f78b705`)
2. TFT_eSPI Manifest `2.5.44` (`16e37595`)
3. LCDWiki-Paket aus
   `references/datasheets/Display/2.8inch_SPI_Module_ILI9341_MSP2807_V1.1.zip`

Arduino_GFX plus XPT2046_Touchscreen und Adafruit GFX plus ILI9341 plus
XPT2046_Touchscreen sind Reservekandidaten. Sie werden nur einbezogen, wenn
weniger als zwei Hauptkandidaten Gate 3 erreichen, alle Hauptkandidaten ein
wesentliches technisches, Ressourcen-, Wartungs- oder Lizenzproblem besitzen
oder der Vergleich keine belastbare Ownerentscheidung erlaubt.

### Hardwareaufbau und Buskonfiguration

| Punkt | Festlegung |
|---|---|
| Display | genau das gelieferte TZT/LCDWiki MSP2807 |
| Controller | vor Test praktisch identifizieren; ILI9341/XPT2046 nicht allein aus Lieferantenangabe als bestaetigt markieren |
| Versorgung | gemaess gemessener Modulvariante; Spannung und Strom protokollieren |
| SPI | derselbe Hardware-SPI-Controller und dieselbe gemessene Pinbelegung fuer alle Kandidaten |
| Chip Select | getrennte Display-/Touch-CS nur nach Boardpruefung; inaktiv sichere Pegel messen |
| Reset/DC/Backlight | reale Pins, aktive Pegel und Bootzustand messen und als `TBD_HARDWARE` bis dahin offen lassen |
| Weitere Verbraucher | Peltier, BTS7960, Innen-/Aussenluefter, MOSFET-Verbraucher und Summer getrennt/gesperrt |

Es werden keine Pinzahlen aus einem aehnlichen Board uebernommen. Die
verwendeten Pins und Busfrequenzen gehoeren in das Spikeprotokoll und erst nach
Messung in ein spaeteres Hardwareprofil.

### Identische Testfaelle

Jeder Kandidat durchlaeuft in derselben Reihenfolge:

1. Kaltstart und Initialisierung mit schwarzem, aktorfreiem Startbild.
2. Querformat 320 x 240 und eindeutige Eckmarkierungen.
3. je 100 Vollbildfuellungen in Schwarz, Weiss, Rot, Gruen und Blau.
4. ASCII sowie deutsche, spanische und englische Beispieltexte mit identischem
   Fontumfang.
5. identische einfache UI-Elemente: Titel, zwei Temperaturwerte, Statuszeile,
   vier grosse Schaltflaechen und Meldungsdialog.
6. Touch-Rohwerte an Ecken, Kanten und Mitte; Druck-/Kontaktstatus erfassen.
7. Fuenfpunktkalibrierung, Neustart und erneute Pruefung.
8. abwechselnde Display- und Touchtransaktionen auf dem gemeinsamen SPI-Bus.
9. 1.000 Zyklen aus Touch lesen, Schaltflaeche zeichnen und Status aktualisieren.
10. kontrollierter Reset waehrend Leerlauf, Zeichnen und Touchabfrage.
11. Betrieb ohne PSRAM und ohne Vollbild-Framebuffer.
12. Fehlen beziehungsweise Stoeren des Touchcontrollers: Anzeige und
    Regelkern duerfen nicht blockieren.

### Messwerte

Pro Kandidat und Baseline werden erfasst:

- erfolgreiche/fehlgeschlagene Initialisierungen und Resetdauer;
- Rotation, Farbfolge, Textdarstellung und Touchkoordinaten;
- Touch-Latenz, Fehl-/Doppelausloesungen und Busfehler;
- Flash und statisches RAM aus dem identischen Build;
- `firmware.bin` und `firmware.elf`;
- freier Heap nach Boot, niedrigster freier Heap und groesster freier
  Heapblock;
- groesster konfigurierter Display-/Sprite-/Transferpuffer;
- Laufzeit und Fehlerzahl der 1.000 Zyklen;
- Bibliothekskonfiguration und eingeschlossene Treiber/Fonts.

### Erfolgskriterien

- reale Controller und 320-x-240-Querformat funktionieren reproduzierbar;
- alle Testelemente sind lesbar, Touchkalibrierung bleibt nach Neustart
  reproduzierbar und der erste Aufweckkontakt kann getrennt werden;
- Shared-SPI verursacht in 1.000 Zyklen keinen Haenger, Watchdog oder
  unerklaerten Reset;
- kein PSRAM und kein Vollbildpuffer werden vorausgesetzt;
- Build passt in die gemessenen Release-1-Budgets mit dokumentiertem Abstand;
- der Adapter kann Bibliothekstypen aus der Anwendung fernhalten;
- Herkunfts- und Lizenzpruefung ist fuer den konkreten Dateisatz abschliessbar.

### Abbruchkriterien

- unkontrollierte GPIO- oder Aktorwirkung;
- wiederholter Haenger, Watchdog oder Buszustand, der nur durch Power-Cycle
  geloest wird;
- falscher oder nicht reproduzierbar identifizierbarer Controller;
- notwendige PSRAM-Abhaengigkeit oder nicht begrenzbarer grosser Puffer;
- nicht aufloesbare Lizenz-/Herkunftsfrage fuer den konkret benoetigten Code;
- Kandidat erfordert einen Toolchainwechsel ohne separaten Ownerentscheid.

### Nicht-Scope und Artefakte

Nicht-Scope: komplette Release-UI, finale Pins, Touchgehause, echte
Serviceaktionen, Aktorbedienung und LVGL-Auswahl. Encoder,
Programmwahlschalter, Start-/Stop-Taster und Status-LED werden auch nicht als
spaetere Spikevarianten untersucht, weil sie kein Bestandteil dieses Projekts
sind. Der 230-V-AC-Hauptschalter ist kein Firmwareeingang. Ergebnisartefakte:

- Schaltplan/Fotos des Testaufbaus und gemessene Pin-/Pegelmatrix;
- je Kandidat fixierte Abhaengigkeits- und Konfigurationsdatei;
- Quellcommit, Lizenzpaket und Buildlog;
- Screenshots/Fotos, Touchroh- und Kalibrierdaten;
- CSV oder Markdown mit allen Messwerten;
- Base-/Kandidaten-Ressourcenvergleich;
- begruendete Ownerentscheidung mit Rueckfallkandidat.

Notwendige Owner-/Hardwareaktion: reale Boardrevision und Modul bereitstellen,
Verdrahtung nach Messung freigeben und den aus dem identischen Vergleich
hervorgehenden Kandidaten auswaehlen.

## Spike B: DS18B20 und 1-Wire

### Ziel

Den kleinsten stabilen Sensorstack fuer die reale Topologie bestimmen und
Adressierung, asynchrone 12-Bit-Konvertierung, mehrere Sensoren, Hot-Plug,
Fehleruebersetzung und Ressourcen belegen.

### Kandidaten

1. DallasTemperature `4.0.6` (`dadbbf7d`) plus OneWire `2.3.8`
   (`800f26f3`)
2. Espressif `onewire_bus 1.1.1` (`a269e1fe`) plus `ds18b20 0.4.0`
   (`bf92b0b3`)

Der Espressif-Kandidat darf nur getestet werden, wenn er mit der bestehenden
Arduino-ESP32-2.0.17-/PlatformIO-Toolchain ohne verdeckten Frameworkwechsel
gebaut werden kann. Andernfalls lautet das Ergebnis reproduzierbar
`INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN`, nicht "Treiber ungeeignet".

### Hardwareaufbau und Buskonfiguration

| Punkt | Festlegung |
|---|---|
| Sensoren | genau drei reale DS18B20: optionaler Produktfuehler, Raum-/Luftsensor und verpflichtender Kuehlkoerper-/Peltier-Schutzsensor |
| Betrieb | 3-Leiter, keine Parasitspeisung |
| Adressen | 64-Bit-ROM jedes Sensors vor Rollenbindung erfassen |
| Topologie A | je Sensor separater Bus, sofern GPIO-Budget nach #29 bestaetigt |
| Topologie B | beide festen Sensoren gemeinsam, Produkt auf getrenntem externem Bus |
| Pull-ups/Leitungen | reale Werte, Laengen und Steckverbindung messen und protokollieren; nicht vorgeben |
| Aktoren | Peltier, BTS7960, Innen-/Aussenluefter, MOSFET-Verbraucher und Summer getrennt/gesperrt; Spike erzeugt nur Messereignisse |

### Identische Testfaelle

1. Ein Sensor: ROM lesen, 9/10/11/12 Bit konfigurieren, Temperatur und CRC
   wiederholt lesen.
2. Drei Sensoren einzeln auf getrennten Bussen.
3. Zwei feste Sensoren auf einem Bus und Produktfuehler separat.
4. Enumeration und stabile 64-Bit-Adressen ueber zehn Neustarts.
5. asynchrone 12-Bit-Konvertierung: Trigger, andere Arbeit, fruehester gueltiger
   Read; keine blockierende Wartezeit im Anwendungspfad.
6. optionalen Produktfuehler vor Boot fehlend, waehrend Betrieb entfernen und
   wieder anschliessen; nur Anwesenheits-, Adress- und Fehlerstatus erfassen.
7. Raum-/Luftsensor sowie Kuehlkoerper-/Peltier-Schutzsensor jeweils entfernen
   und wieder anschliessen; nur Treiberstatus erfassen, keine Rollenprioritaet
   oder Safety-Regel im Spike implementieren.
8. Bus kurzschliessen/unterbrechen nur mit sicherer strombegrenzter Methode;
   Timeout und Wiederherstellung messen.
9. CRC- beziehungsweise Datenfehler soweit reproduzierbar injizieren.
10. 1.000 Messzyklen bei ungefaehr zwei Sekunden mit Zeitstempeln.
11. absichtlich ungueltige Rolle/Adresse: Adapter lehnt eindeutig ab.
12. Reset waehrend Konvertierung und anschliessende Neuinitialisierung.

### Messwerte

- Erkennungs-, Konvertierungs- und Lesedauer pro Aufloesung;
- Anzahl erfolgreicher Reads, CRC-/Bus-/Timeoutfehler und Wiederanschlusszeit;
- ROM-Stabilitaet und Reihenfolge der Enumeration;
- blockierte CPU-Zeit und maximaler Adapterpuffer;
- Flash, statisches RAM, `firmware.bin`, `firmware.elf`;
- freier/niedrigster Heap und groesster freier Heapblock;
- zusaetzliche Framework-/Buildkomplexitaet und transitive Abhaengigkeiten;
- Verhalten auf beiden zulaessigen Topologien.

### Erfolgskriterien

- alle realen Sensoren sind ueber stabile 64-Bit-Adressen unterscheidbar;
- mehrere Sensoren und beide Topologien funktionieren;
- 12-Bit-Konvertierung ist ohne blockierende Wartephase integrierbar;
- fehlender Sensor, Busfehler, CRC-Fehler und Wiederanschluss sind typisiert und
  verursachen keine veraltete Messung als neuen gueltigen Wert;
- 1.000 Zyklen ohne Haenger, Watchdog oder unerklaerten Reset;
- Adapter bleibt schmal und liefert den benoetigten Bus-, Adress- und
  Fehlerstatus; Rollenprioritaet, Ersatzregelung und Peltierfreigabe bleiben
  ausserhalb;
- Ressourcen und Toolchainkomplexitaet sind gemessen.

### Abbruchkriterien

- Bibliothek blockiert laenger als der dokumentierte und begrenzte Messvertrag
  ohne unterbrechbare Alternative;
- Fehler oder Hot-Plug erfordern einen Geraetereset;
- stabile ROM-Adressierung oder Mehrsensorbetrieb ist nicht moeglich;
- Toolchainwechsel, ungebundene Task-/Heapnutzung oder nicht aufloesbare
  transitive Abhaengigkeiten;
- unkontrollierte elektrische oder thermische Situation.

### Nicht-Scope und Artefakte

Nicht-Scope: `VALID`/`STALE`/`FAILED`, Filter, Offsets, Produktfuehler als
primaerer Regelsensor, Raum-/Luftsensor als regulaerer Ersatz,
Kuehlkoerper-/Peltier-Schutzsensor als verpflichtende Sicherheitsgrundlage,
PI-Regelung, Aktorfreigabe und finale Sensorposition. Diese Vertraege bleiben
im Fermentations-/Safety-Kern. Ergebnisartefakte:

- Aufbau-/Topologiefotos, Pull-up-/Leitungsdaten und ROM-Liste;
- identischer Testcode je Kandidat hinter demselben Adaptervertrag;
- Messdaten und Fehlerprotokoll;
- Base-/Kandidaten-Ressourcenvergleich;
- Toolchain-/Abhaengigkeitsbericht;
- begruendete Ownerentscheidung und Rueckfallkandidat.

Notwendige Owner-/Hardwareaktion: alle drei realen Sensoren, Produktstecker und
Leitungen bereitstellen; zulaessige Stoer-/Hot-Plug-Tests bestaetigen; Auswahl
erst anhand des identischen Messprotokolls treffen.

## Reihenfolge und Entscheidungsprotokoll

1. Audit- und Planungsbereinigung abschliessen.
2. Den minimalen Baseline-Anteil von #29 nachweisen; der vollstaendige Abschluss
   von #24 oder #29 ist keine Voraussetzung fuer die aktorfreien Spikes.
3. Jeden Kandidaten durch Gate 1 und Gate 2 fuehren.
4. Nur ausreichend erfolgreiche Kandidaten in Gate 3 identisch vergleichen.
5. Herkunfts-/Lizenzpruefung fuer die technisch geeigneten Kandidaten
   aktualisieren.
6. Owner waehlt je Gruppe genau einen Produktivkandidaten und einen
   dokumentierten Rueckfallkandidaten.
7. Erst danach implementieren #30 und #31 die schmalen Adapter.

Parallel dazu darf die hardwareunabhaengige Kette #20 Sensorqualitaet, #21
Regelsensorauswahl, #22 PI/Luftbegrenzung, #23 Aktorplaner und #24 Fehlerkern/
`SAFE_BOOT` weiterlaufen. Bibliothekstypen und reale GPIOs gelangen nicht in
diesen Kern; Treiber- und Fachstatus bleiben getrennt, und ungemessene
Hardwarewerte bleiben `TBD_COMMISSIONING`.

Produktive Aktoradapter und reale Aktortests bleiben von ihren Safety-Gates
abhaengig. Ein bestandener aktorfreier Spike ist keine Aktorfreigabe.
