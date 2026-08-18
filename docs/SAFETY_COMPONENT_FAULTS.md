# Temperatur-, Sensor-, Luefter- und Aktorfehler

## Status

Dieses Dokument beschreibt die in Phase 8B akzeptierten Fehlerreaktionen fuer
Temperatursensoren, widerspruechliche Messwerte, Temperaturgrenzen, begrenzte
Sicherheitsrueckfuehrung, Luefter, BTS7960 und Peltier.

Es ergaenzt [`SAFETY_AND_FAULTS.md`](SAFETY_AND_FAULTS.md),
[`TEMPERATURE_CONTROL.md`](TEMPERATURE_CONTROL.md),
[`ACTUATOR_TIMING_AND_FANS.md`](ACTUATOR_TIMING_AND_FANS.md) und
[`SENSOR_TUNING_COMMISSIONING.md`](SENSOR_TUNING_COMMISSIONING.md).

Konkrete Temperaturwerte, Zeitfenster, Wiederholungszahlen und thermische
Plausibilitaetsgrenzen bleiben bis zu den Versuchen am realen Aufbau
`TBD_COMMISSIONING`, soweit dieses Dokument keine firmwarefeste Obergrenze
festlegt.

## Issue #24 Release-1-Grenze

Der #24-Safety-Core konsumiert aus #20/#21 nur die bestehende
Sensorqualitaets- und Auswahlprojektion. Er fuehrt keine zweite Sensor-FSM
ein. `STALE`/`FAILED` sperrt die Peltierfreigabe beziehungsweise loest
`ImmediateStop` aus; die Rueckkehr erfolgt ausschliesslich ueber die frische
#20/#21-Evidenz.

Thermische Schutzgrenzen, Luefter-/H-Bruecken-/Hardwarefaults und
`SAFETY_RECOVERY` sind kein R1-Producer von #24. Ihre Commissioning- und
E5-Grenzen bleiben spaeter integrierbar; #24 erfindet dafuer weder Codes noch
Laufzeitwerte, keine automatische Gegenrichtung und keinen Service-PIN-Reset.

## E5/#35/Future-Kontext – nicht normativ fuer #24-R1

Die folgenden bestehenden Detailabschnitte dokumentieren spaetere
Commissioning-, Temperatur-, Luefter- und Hardwareentscheidungen. Sie sind
fuer #24-R1 nicht aktiv und erzeugen dort weder Fault-Codes noch Grenzwerte,
persistente Latches, Service-PIN-Clear, kontrollierte Neustarts,
`SAFETY_RECOVERY` oder automatische Gegenrichtung. Im #24-R1-Schnitt gelten
ausschliesslich die oben beschriebenen #20/#21-Sensorprojektionen:
`STALE`/`FAILED` sperrt beziehungsweise stoppt, und eine Freigabe erfolgt nur
nach frischer kanonischer Evidenz.

## Bestehende Sensor-, Temperatur- und Hardwarevertraege

Das erste Release verwendet drei Temperaturrollen:

1. **Schrankluftfuehler**
   - primaerer Regelsensor im luftgefuehrten Betrieb
   - fruehe Luftbegrenzung im produktgefuehrten Betrieb
   - absolute Temperatur-Sicherheitsueberwachung im Innenraum

2. **Abnehmbarer Produktfuehler**
   - primaerer Regelsensor im produktgefuehrten Betrieb
   - optional und nicht fuer die grundsaetzliche Hardware-Sicherheitsfreigabe
     erforderlich

3. **Aussenwaermetauscher- beziehungsweise Kuehlkoerperfuehler**
   - fest am thermisch relevanten aeusseren Waermetauscher beziehungsweise nahe
     der aeusseren Peltierseite montiert
   - ueberwacht Temperatur und Aenderungsrate der Leistungsbaugruppe
   - dient insbesondere zur Erkennung eines stark vermuteten Aussenluefter-,
     Waermeabfuhr- oder Peltierfehlers
   - wird in beiden Polaritaetsrichtungen ausgewertet; die aeussere Seite kann je
     nach Betriebsrichtung warm oder kalt werden

Eine Peltierfreigabe verlangt mindestens gueltige und ausreichend aktuelle
Messwerte von:

- Schrankluftfuehler
- Aussenwaermetauscher-/Kuehlkoerperfuehler

Der Produktfuehler wird nur dann zusaetzlich verlangt, wenn der aktive Lauf ihn
als primaeren Regelsensor verwendet und keine validierte Ersatzstrategie aktiv
ist.

## Sensorzustandsfolge

Alle Sensorrollen verwenden mindestens:

```text
VALID
  -> STALE
  -> FAILED
```

- `VALID`: aktueller, plausibler und ausreichend gefilterter Messwert
- `STALE`: einzelne oder kurzzeitig aufeinanderfolgende ungueltige Messungen;
  letzter gueltiger Wert ist nur fuer Anzeige und eng begrenzte Bewertung
  vorhanden
- `FAILED`: Fehleranzahl, Fehlerdauer oder Plausibilitaetsgrenze ueberschritten

Ein sicherheitsrelevanter Sensor im Zustand `STALE` sperrt neue
Peltierfreigaben sofort. Eine bereits aktive Peltierfreigabe wird sicher beendet.
Der Sensor erhaelt jedoch zuerst ein kurzes, begrenztes Wiedererkennungsfenster,
bevor ein verriegelter Fehler erzeugt wird.

## Ausfall des Schrankluftfuehlers

### Kurzer Wiedererkennungsversuch vor Verriegelung

Ein einzelner CRC-, Bus- oder Messfehler fuehrt nicht sofort zur permanenten
Verriegelung.

Vorgesehener Ablauf:

```text
Schrankluftmessung ungueltig
  -> Peltier sofort AUS
  -> notwendigen Aussenluefter-Nachlauf ausfuehren
  -> Sensorstatus STALE
  -> 1-Wire-Bus und Sensorerfassung begrenzt neu initialisieren
  -> mehrere neue Messzyklen pruefen
  -> bei stabiler gueltiger Rueckkehr automatisch wieder VALID
  -> andernfalls FAILED und verriegelter Sicherheitsfehler
```

Die automatische Rueckkehr aus `STALE` ist nur erlaubt, wenn:

- mehrere aufeinanderfolgende Messungen gueltig sind,
- CRC und Busstatus fehlerfrei sind,
- Wert und Aenderungsrate plausibel sind,
- der Sensor wieder eindeutig seiner gespeicherten ROM-Adresse entspricht,
- keine gleich- oder hoeherklassige Fehlerursache aktiv ist.

### Kein unkontrollierter Neustart als erste Reaktion

Ein kompletter ESP32-Neustart ist nicht der erste Wiedererkennungsversuch.
Zuerst werden Sensorzustandsmaschine, 1-Wire-Bus und Messablauf lokal neu
initialisiert. Ein normaler physischer Leitungs-, Steck- oder Sensorfehler wird
durch einen Neustart nicht behoben und darf nicht durch Neustartschleifen
verschleiert werden.

Ein **einmaliger kontrollierter Software-Neustart** darf nur dann als spaetere
Wiederherstellungsstufe verwendet werden, wenn interne Diagnose oder Watchdog
auf eine blockierte Sensoraufgabe beziehungsweise einen Treiberzustand und nicht
auf einen plausiblen physischen Sensorfehler hinweist.

Dabei gilt:

- Lauf- und Fehlerzustand werden vorher gesichert, soweit moeglich.
- Alle Ausgaenge bleiben waehrend Boot und Validierung sicher AUS.
- Nach dem Neustart ist eine vollstaendige stabile Sensorvalidierung erforderlich.
- Der Neustart selbst loescht keine bereits gesetzte Verriegelung.
- Derselbe Fehler darf nicht zu einer automatischen Neustartschleife fuehren.
- Eine Wiederholung innerhalb eines definierten Zeitfensters eskaliert zum
  verriegelten Sicherheits- oder Systemfehler.

Die genaue Abgrenzung eines Sensor- gegen einen Softwaretaskfehler wird in Phase
8C festgelegt.

### Verhalten bei `FAILED`

Ist der Schrankluftfuehler `FAILED`:

- Peltier und beide H-Brueckenrichtungen bleiben gesperrt.
- Eine Regelung nur mit dem traegeren Produktfuehler ist unzulaessig.
- Der Aussenluefter fuehrt den notwendigen Nachlauf aus.
- Der Fehler wird persistent als verriegelter Sicherheitsfehler gespeichert.
- Ein Service-PIN-Reset ist erst nach stabilen gueltigen Messungen und erneuter
  Sicherheitspruefung moeglich.

## Ausfall des Produktfuehlers

Ein Produktfuehlerausfall ist standardmaessig ein behebbarer Betriebsfehler und
verwendet die bereits pro Programm festgelegte Ersatzstrategie.

Ablauf:

```text
Produktfuehler STALE oder FAILED
  -> Peltier zunaechst AUS
  -> Schrankluft- und Kuehlkoerpersensor pruefen
  -> Sicherheitsfreigaben pruefen
  -> Benutzer sichtbar und akustisch informieren
  -> programmabhaengige Wartezeit
  -> falls erlaubt automatisch oder bewusst auf Luftregelung wechseln
  -> niedrigere Regel- und Zeitkonfidenz sichtbar halten
```

Nach stabiler Rueckkehr darf der Produktfuehler gemaess
`automatic_validated_return_to_product`, `manual_return_to_product` oder
`remain_on_air_until_end` behandelt werden.

Sind Schrankluft- oder Kuehlkoerpersensor gleichzeitig nicht sicher gueltig,
ist kein Luft-Ersatzbetrieb erlaubt und der Fehler eskaliert.

## Widerspruechliche Sensorwerte

Ein grosser Unterschied zwischen Produkt- und Schranklufttemperatur beweist fuer
sich allein keinen Sensorfehler.

Insbesondere bei rund 50 W Peltierleistung, grosser Produktmasse und einem
traegen Gefaess kann die Produkttemperatur erst nach langer Zeit sichtbar
reagieren. Die Firmware darf deshalb aus einer kurzfristig unveraenderten
Produkttemperatur keinen Peltier- oder Produktfuehlerfehler ableiten.

Die Plausibilisierung beruecksichtigt mindestens:

- Prozessphase und aktuelle Aktorrichtung
- Zeit seit Beginn der Heiz- oder Kuehlanforderung
- Produktmasse beziehungsweise verwendetes Tuningprofil, soweit bekannt
- Schrankluftverlauf
- Aussenwaermetauscher-/Kuehlkoerperverlauf
- Rohwert, Filterwert und Aenderungsrate jedes Sensors
- Sensoralter, CRC- und Busfehler
- zuvor beobachtete thermische Totzeiten

Bei einem anhaltenden Widerspruch gilt:

1. Kein Sensor wird allein aufgrund seiner Rolle automatisch fuer richtig erklaert.
2. Der offensichtlich unplausible Sensor wird zunaechst als verdaechtig
   gekennzeichnet.
3. Ein Ersatzbetrieb ist nur erlaubt, wenn die verbleibenden fuer Regelung und
   Sicherheit notwendigen Sensoren eindeutig vertrauenswuerdig sind.
4. Bei unklarer Sicherheitslage wird das Peltier ausgeschaltet.
5. Ein anhaltender oder sicherheitsrelevanter Widerspruch wird verriegelt.

Die Diagnose zeigt, dass eine Fehlerzuordnung vermutet und nicht bewiesen ist,
wenn keine eindeutige Ursache vorliegt.

## Temperaturgrenzen

### Funktionale Ebenen

Die Temperaturueberwachung besitzt folgende funktionale Ebenen:

1. **Fruehe Regelbegrenzung**
   - reduziert Leistung oder sperrt die aktuell unguenstige Richtung
   - normaler Bestandteil der Regelung

2. **Prozesswarnung**
   - meldet eine relevante Abweichung
   - Prozess darf weiterlaufen, solange alle Sicherheitsbedingungen erfuellt sind

3. **Sicherheitsbereich**
   - besitzt eine Eingriffsgrenze und eine weiter entfernte harte Notgrenze
   - beide Grenzen sind firmwarefest beziehungsweise nur innerhalb enger
     firmwarefester Grenzen parametrierbar

Die Aufteilung des Sicherheitsbereichs in Eingriffs- und harte Notgrenze ist
notwendig, damit ein grosses thermisches Ueberschwingen kontrolliert in einen
sicheren Bereich zurueckgefuehrt werden kann, ohne an der aeussersten Grenze noch
aktive Rettungsversuche zu starten.

### Sicherheits-Eingriffsgrenze

Beim Ueberschreiten einer oberen oder unteren Sicherheits-Eingriffsgrenze:

```text
aktuelle Peltierleistung sofort AUS
  -> ausloesende Richtung sperren
  -> Impulsakkumulator verwerfen
  -> Integralanteil sperren
  -> Mindest-Auszeit und Polaritaetstotzeit abwarten
  -> Sensoren, Luefter und Aktordiagnose neu pruefen
  -> gegebenenfalls begrenzte Gegenrichtung als SAFETY_RECOVERY
```

Das Ereignis wird als Sicherheitsfehler verriegelt. Eine begrenzte Gegenrichtung
ist eine kontrollierte Sicherheitsrueckfuehrung und keine normale automatische
Wiederfreigabe des Fermentationsprozesses.

### Harte Notgrenze

Wird die weiter entfernte harte obere oder untere Notgrenze erreicht oder
ueberschritten:

- beide Peltier-Richtungen bleiben sofort gesperrt,
- es erfolgt keine automatische Gegenrichtungs-Ansteuerung,
- Aussen- und Innenluefter werden gemaess sicherer Fehlerstrategie betrieben,
- der Fehler bleibt verriegelt,
- Service und Ursachenpruefung sind erforderlich.

Dasselbe gilt bereits vor der harten Notgrenze, wenn die Ursache nicht eindeutig
beherrschbar ist.

## Begrenzte Gegenrichtungs-Rueckfuehrung

Eine Gegenrichtung nach einer Sicherheits-Eingriffsgrenze ist nur erlaubt, wenn
alle folgenden Bedingungen erfuellt sind:

- Schrankluft- und Kuehlkoerpersensor sind aktuell, plausibel und stabil.
- Die Temperatur liegt noch innerhalb der harten Notgrenzen.
- Kein Sensorwiderspruch mit unklarer Sicherheitslage besteht.
- Kein Aussen- oder Innenluefterfehler ist aktiv.
- Kein BTS7960-, H-Bruecken-, Strom- oder Ausgangsfehler ist aktiv.
- Die ausloesende Richtung ist nachweislich deaktiviert.
- Falls ein brauchbares Stromsignal vorhanden ist, ist der Strom auf einen
  sicheren Aus-Zustand abgefallen.
- Mindest-Auszeit und Polaritaetswechsel-Totzeit sind beendet.
- Die Gegenrichtung ist physikalisch geeignet, die Temperatur zurueck in den
  sicheren Bereich zu fuehren.

Die Rueckfuehrung verwendet:

- begrenzte Leistung,
- begrenzte Pulsdauer,
- staendige Sensor- und Aktorueberwachung,
- eine erwartete Temperaturtrend-Pruefung nach jedem Versuch,
- firmwarefest maximal **zwei** Versuche,
- Werkseinstellung zunaechst **ein** Versuch, bis die Inbetriebnahme einen zweiten
  Versuch sicher rechtfertigt.

Die zulaessige Versuchszahl darf als PIN-geschuetzter Maschinenparameter nur im
Bereich `0..2` liegen.

Abbruch und vollstaendige Verriegelung erfolgen sofort, wenn:

- die Temperatur weiter in die falsche Richtung laeuft,
- keine ausreichende sichere Reaktion erkennbar ist,
- ein weiterer Sensor-, Luefter- oder Aktorfehler entsteht,
- die harte Notgrenze erreicht wird,
- die maximal erlaubte Versuchszahl erreicht ist.

Nach erfolgreicher Rueckkehr unter eine definierte Freigabeschwelle:

- wird die Gegenrichtung wieder ausgeschaltet,
- bleibt der Sicherheitsfehler verriegelt,
- wird der normale Lauf nicht automatisch fortgesetzt,
- sind Ursachenpruefung und bewusster Fehlerreset erforderlich,
- werden Verlauf, Versuche, Leistung und Temperaturreaktion protokolliert.

Damit kann ein reines thermisches Ueberschwingen begrenzt abgefangen werden,
ohne einen moeglicherweise defekten Aktor unkontrolliert als Rettungsfunktion zu
verwenden.

## Dritter DS18B20 am Aussenwaermetauscher

Der dritte Sensor ist Bestandteil des geplanten ersten Hardwareaufbaus.

Montageanforderungen:

- gute thermische Kopplung an den relevanten aeusseren Waermetauscher oder
  Kuehlkoerper
- elektrische Isolation und mechanische Fixierung gemaess Sondenaufbau
- Schutz vor Kondensat, Kabelzug und Kontakt mit beweglichen Luefterteilen
- Position muss reproduzierbar dokumentiert werden
- Sensor-ROM-Adresse und Rolle werden fest gespeichert

Eine Peltierfreigabe ohne gueltigen Kuehlkoerpersensor ist im ersten Release nicht
zulaessig.

Bei `STALE`:

- Peltier sofort AUS,
- Aussenluefter bleibt fuer Nachlauf beziehungsweise Diagnose aktiviert,
- begrenzter Wiedererkennungsversuch wie beim Schrankluftfuehler.

Bei `FAILED`:

- verriegelter Sicherheitsfehler,
- kein normaler oder Gegenrichtungs-Peltierbetrieb,
- Service-PIN-Reset erst nach stabiler Sensorvalidierung.

## 1-Wire-Bustopologie

Der abnehmbare Produktfuehler darf keinen fuer die Sicherheitsfreigabe
notwendigen festen Sensor beim Ein- oder Ausstecken stoeren.

Bevorzugte Reihenfolge bei der Pinplanung:

1. eigener 1-Wire-Bus fuer den Schrankluftfuehler,
2. eigener 1-Wire-Bus fuer den Aussenwaermetauscher-/Kuehlkoerperfuehler,
3. eigener 1-Wire-Bus fuer den abnehmbaren Produktfuehler.

Falls das GPIO-Budget drei getrennte Busse nicht erlaubt, ist als minimale
akzeptable Variante vorgesehen:

- ein geschuetzter interner Bus fuer die beiden fest eingebauten Sensoren,
- ein separater Bus fuer den abnehmbaren Produktfuehler.

Nicht zulaessig ist, den abnehmbaren Produktfuehler mit den festen
sicherheitsrelevanten Sensoren auf denselben extern zugaenglichen Bus zu legen.
Die verbindliche Variante wird erst nach finaler GPIO-Budgetpruefung festgelegt.

## Erkannter oder stark vermuteter Luefterfehler

Ohne Tachosignal wird nicht behauptet, dass ein eingeschalteter Ausgang einen
mechanisch laufenden Luefter beweist.

Moegliche Diagnosequellen sind:

- Ausgangsanforderung und tatsaechlicher Ausgangszustand, soweit messbar
- optionaler Ausgangsstrom
- Kuehlkoerpertemperatur und deren Aenderungsrate
- Schrankluftreaktion
- erwartete thermische Reaktion aus der Inbetriebnahme
- spaeteres Tachosignal, falls entsprechende Luefter nachgeruestet werden

### Aussenluefter

Bei nachgewiesenem oder stark vermutetem Aussenluefterfehler waehrend oder nach
Peltierbetrieb:

- Peltier sofort AUS,
- beide H-Brueckenrichtungen sperren,
- Impulsakkumulator verwerfen,
- Aussenluefterausgang zur moeglichen Restwaermeabfuhr eingeschaltet lassen,
  sofern kein elektrischer Ausgangsfehler dagegen spricht,
- Innenluefter gemaess thermischer Lage weiterbetreiben,
- Fehler persistent verriegeln,
- keine automatische Wiederfreigabe,
- Service-PIN-Reset erst nach Funktionspruefung.

### Innenluefter

Bei nachgewiesenem oder stark vermutetem Innenluefterfehler in einer
temperaturgeregelten Phase:

- Peltier sofort AUS,
- Regelung und Zielqualifikation anhalten,
- Fehler verriegeln,
- keine Annahme, dass der Luftfuehler weiterhin eine repraesentative
  Schranktemperatur misst,
- Service-PIN-Reset erst nach Funktionspruefung.

Ein bewusster zeitlich begrenzter Lueftertest bleibt im geschuetzten Servicemodus
moeglich. Direkte Tests sind waehrend eines aktiven Laufes gesperrt.

## BTS7960- und Peltierdiagnose

### R_IS und L_IS

Die Signale `R_IS` und `L_IS` werden am gelieferten BTS7960-Modul praktisch
geprueft.

Sie werden nur als Diagnose- oder Strominformation verwendet, wenn:

- die konkrete Modulbeschaltung dokumentiert ist,
- Signalpegel und Skalierung fuer den ESP32 sicher angepasst sind,
- Nullpunkt, Rauschen und Richtungszuordnung reproduzierbar sind,
- eine brauchbare Unterscheidung zwischen AUS, normalem Strom und Fehlerzustand
  moeglich ist.

Sind die Signale am gelieferten Modul nicht brauchbar, wird keine erfundene
Stromdiagnose implementiert. Ein externer Stromsensor ist fuer das erste Release
nicht zwingend, bleibt aber eine spaetere Option.

### Sofort verriegelnde Aktorfehler

Mindestens folgende Situationen gelten als verriegelter Sicherheitsfehler:

- Softwareanforderung beider H-Brueckenrichtungen gleichzeitig
- tatsaechlich festgestellte gleichzeitige Aktivierung beider Richtungen
- Stromfluss bei deaktivierter Peltierfreigabe, sofern messbar
- unzulaessig hoher Strom, sofern verlaesslich messbar
- unerwartete Ausgangs- oder Enable-Kombination
- wiederholte Aktor-Watchdogverletzung

Reaktion:

- Peltier und beide Richtungen sofort AUS,
- Aussenluefter-Nachlauf,
- keine automatische Wiederfreigabe,
- Service-PIN und technische Pruefung erforderlich.

### Fehlende Aktor- oder Temperaturreaktion

Fehlt trotz wiederholter gueltiger Aktorfreigabe die erwartete elektrische oder
thermische Reaktion:

1. Sensorqualitaet und thermische Totzeit pruefen.
2. Produkttraegheit nicht als sofortigen Fehler interpretieren.
3. Schrankluft- und Kuehlkoerperreaktion innerhalb ihrer deutlich schnelleren
   erwarteten Zeitfenster auswerten.
4. Aktor zunaechst sicher ausschalten und als behebbaren Betriebsfehler melden.
5. Bei Wiederholung, Stromwiderspruch oder unklarer Sicherheitslage zum
   verriegelten Aktor-/Peltierfehler eskalieren.

Ein fehlender schneller Temperaturanstieg des Produkts allein ist kein
hinreichender Peltierfehlernachweis.

## Fehlerreaktionsmatrix Phase 8B

| Ursache | Erste Reaktion | Klasse nach bestaetigtem Fehler | Automatische Wiederfreigabe |
|---|---|---:|---|
| Schrankluftfuehler kurz ungueltig | Peltier AUS, `STALE`, Bus/Sensor neu pruefen | 2 waehrend Wiedererkennung | ja, nur vor `FAILED` nach stabiler Validierung |
| Schrankluftfuehler `FAILED` | Peltier AUS, Luefternachlauf | 3 | nein |
| Produktfuehler `FAILED` | Peltier AUS, Ersatzstrategie pruefen | 2, bei weiterer Unsicherheit 3 | gemaess Programmstrategie |
| Kuehlkoerpersensor kurz ungueltig | Peltier AUS, Luefter weiter, neu pruefen | 2 waehrend Wiedererkennung | ja, nur vor `FAILED` nach stabiler Validierung |
| Kuehlkoerpersensor `FAILED` | Peltier AUS, Luefter weiter | 3 | nein |
| Sensorwiderspruch unklar | Peltier AUS | 2 oder 3 nach Bewertung | nur bei eindeutig beseitigter Ursache |
| Sicherheits-Eingriffsgrenze | ausloesende Richtung AUS, ggf. begrenzte Gegenrichtung | 3 verriegelt | nein |
| Harte Notgrenze | beide Richtungen AUS | 3 verriegelt | nein |
| Aussenluefterfehler | Peltier AUS, Luefterausgang soweit sicher EIN | 3 | nein |
| Innenluefterfehler | Peltier AUS | 3 | nein |
| H-Brueckenrichtungs- oder Stromfehler | beide Richtungen AUS | 3 | nein |
| Fehlende thermische Reaktion | Peltier AUS und Diagnose | zuerst 2, bei Wiederholung 3 | nur in Klasse 2 nach neuer Validierung |

## Akzeptierte Entscheidungen aus Phase 8B

- [x] Schrankluftfuehler erhaelt vor `FAILED` ein kurzes begrenztes
      Wiedererkennungsfenster
- [x] Peltier ist bereits waehrend `STALE` eines Sicherheitsfuehlers aus
- [x] lokaler Bus-/Sensormessneustart vor einem kompletten ESP32-Neustart
- [x] hoechstens einmaliger kontrollierter Software-Neustart nur bei begruendetem
      internen Sensoraufgaben- oder Treiberfehler
- [x] Schrankluftfuehler `FAILED` ist verriegelter Sicherheitsfehler
- [x] Produktfuehler verwendet die programmabhaengige Ersatzstrategie
- [x] langsame Produktreaktion bei rund 50 W wird nicht als schneller
      Fehlernachweis verwendet
- [x] Sensorwidersprueche werden rollen-, phasen- und zeitbezogen bewertet
- [x] fruehe Regelbegrenzung, Prozesswarnung und Sicherheitsbereich getrennt
- [x] Sicherheitsbereich besitzt Eingriffsgrenze und harte Notgrenze
- [x] an der Eingriffsgrenze maximal zwei begrenzte Gegenrichtungsversuche,
      Werkseinstellung zunaechst ein Versuch
- [x] nach erfolgreicher Sicherheitsrueckfuehrung bleibt der Fehler verriegelt
- [x] an der harten Notgrenze keine automatische Gegenrichtung
- [x] dritter fest eingebauter DS18B20 am Aussenwaermetauscher/Kuehlkoerper
- [x] Peltierfreigabe nur mit gueltigem Schrankluft- und Kuehlkoerpersensor
- [x] abnehmbarer Produktfuehler auf getrenntem 1-Wire-Bus
- [x] Aussen- und Innenluefterfehler verriegeln den Peltierbetrieb
- [x] ohne Tachometer keine falsche Behauptung eines laufenden Luefters
- [x] R_IS/L_IS nur nach praktischer Verifikation verwenden
- [x] keine externe Strommessung als Pflicht fuer das erste Release
- [x] gleichzeitige Richtungen, Strom ohne Freigabe und unzulaessige
      H-Brueckenzustaende verriegeln
- [x] fehlende Produktreaktion allein beweist keinen Peltierfehler

## Noch offen fuer Phase 8C und Inbetriebnahme

- konkrete Dauer und Fehleranzahl fuer `STALE -> FAILED`
- Kriterien fuer einen einmaligen kontrollierten Software-Neustart
- Zeitraum, in dem eine Fehlerwiederholung eskaliert
- konkrete obere und untere Regelbegrenzungen
- konkrete Sicherheits-Eingriffs- und harte Notgrenzen
- Leistungs-, Zeit- und Trendgrenzen fuer `SAFETY_RECOVERY`
- Werkseinstellung ein oder nach Nachweis zwei Rueckfuehrungsversuche
- konkrete Montageposition des Kuehlkoerpersensors
- finaler 1-Wire-Pinplan
- thermische Erkennungsschwellen fuer Aussen- und Innenluefterfehler
- Nutzbarkeit, Skalierung und sichere Pegelanpassung von R_IS/L_IS
- Fehlerreaktion bei unzuverlaessiger 12-V- oder 5-V-Versorgung
- Verhalten bei Softwaretask-, Watchdog-, Speicher- und Bootfehlern
