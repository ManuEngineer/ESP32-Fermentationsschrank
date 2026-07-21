# Sensorverarbeitung, Tuning und Inbetriebnahme

## Status

Dieses Dokument beschreibt die in Phase 7C akzeptierten Regeln fuer den
DS18B20-Messzyklus, die Filterung und Plausibilisierung der Temperaturwerte,
Sensorkalibrierung, PI-Parametersaetze, den strukturierten
Inbetriebnahmeablauf und die Voraussetzungen fuer spaetere Kaskaden- oder
PID-Strategien.

Es ergaenzt [`TEMPERATURE_CONTROL.md`](TEMPERATURE_CONTROL.md) und
[`ACTUATOR_TIMING_AND_FANS.md`](ACTUATOR_TIMING_AND_FANS.md).

Konkrete Filterparameter, Plausibilitaetsgrenzen, Kalibrierwerte und PI-Werte
bleiben bis zu den Messungen am realen Aufbau `TBD_COMMISSIONING`.

## Fester Temperatur-Messzyklus

Beide DS18B20-Sensoren werden im ersten Release mit 12-Bit-Aufloesung in einem
festen Zyklus gemessen.

Werkseinstellung und verbindlicher Release-Wert:

```text
Messzyklus: ungefaehr 2 Sekunden
```

Der Messzyklus ist im normalen Betrieb und im Servicebereich nicht frei
veraenderbar.

Gruende:

- Die Temperaturprozesse sind gegenueber zwei Sekunden sehr langsam.
- Filterung, Plausibilitaetspruefung und PI-Abstimmung erhalten eine konstante
  Zeitbasis.
- Zeitbezogene Fehlerregeln werden reproduzierbarer.
- Unterschiedliche Benutzerwerte muessen nicht separat getestet werden.
- Eine noch schnellere Messung bringt fuer diesen Aufbau keinen praktischen
  Regelvorteil.

Die Implementierung darf den Sensorbus nicht blockierend so verwenden, dass
Display, Regelaufgabe, Aktor-Watchdog oder Sicherheitsaufgaben verzoegert
werden. Die genaue nichtblockierende Ablaufsteuerung wird im technischen Entwurf
festgelegt.

## Verarbeitungskette eines Messwertes

Ein Rohwert darf nicht ungeprueft als Regelwert verwendet werden.

Vorgesehene Verarbeitung:

```text
Messung anfordern
  -> Messwert und CRC empfangen
  -> Bus- und Fehlerwerte pruefen
  -> physikalische und zeitliche Plausibilitaet pruefen
  -> kurzer Medianfilter gegen Einzelspitzen
  -> sensorbezogener Tiefpass
  -> gefilterten Regelwert und Qualitaetsstatus bereitstellen
```

Die Verarbeitung erzeugt mindestens getrennte Informationen fuer:

- letzten empfangenen Rohwert
- gefilterten Temperaturwert
- Alter des letzten gueltigen Rohwertes
- Sensorqualitaet
- Fehler- beziehungsweise Plausibilitaetsstatus
- verwendeten Kalibrier-Offset

Die technische Diagnose zeigt Rohwert, gefilterten Wert, Alter und Status an.
Die normale Bedienoberflaeche zeigt den fuer den Prozess relevanten gefilterten
Wert und warnt bei eingeschraenkter Qualitaet.

## Medianfilter gegen Einzelspitzen

Ein kurzer Medianfilter entfernt einzelne stark abweichende Werte, ohne eine
langfristige Temperaturverschiebung zu verstecken.

Verbindliche Anforderungen:

- feste, kleine Fensterlaenge
- begrenzter RAM-Verbrauch
- kein unbegrenzt wachsender Messwertpuffer
- ungueltige Messwerte werden nicht als normale Zahlen in den Median eingefuegt
- nach Sensorstart oder Wiederkehr wird der Filter kontrolliert initialisiert
- der Filter darf einen echten anhaltenden Temperaturwechsel nicht ueber lange
  Zeit unterdruecken

Die genaue Fensterlaenge bleibt `TBD_COMMISSIONING`.

## Sensorbezogener Tiefpass

Nach der Ausreisserunterdrueckung wird ein Tiefpass beziehungsweise eine
vergleichbare exponentielle Glaettung verwendet.

Luft- und Produktfuehler erhalten unterschiedliche Filterstaerken:

- **Schrankluft:** weniger traege Filterung, damit Luftbegrenzungen und
  Regelreaktionen zeitnah erfolgen
- **Produkt:** staerkere Glaettung zulaessig, passend zur groesseren thermischen
  Traegheit

Verbindliche Regeln:

- Filterparameter sind PIN-geschuetzte Maschinenparameter innerhalb
  firmwarefester Grenzen.
- Der Zeitbezug wird explizit beruecksichtigt; ein Messausfall darf nicht wie eine
  normale neue Probe behandelt werden.
- Ein Filterwert wird bei veraltetem Sensorstatus nicht unbegrenzt als aktuell
  ausgegeben.
- Sicherheitspruefungen duerfen bei Bedarf zusaetzlich den Rohwert oder einen
  weniger stark gefilterten Sicherheitswert auswerten.
- Die normale PI-Regelung arbeitet mit dem gefilterten Regelwert.

## Einzelne ungueltige Messung und veralteter Wert

Ein einzelner CRC-, Bus- oder Messfehler macht einen Sensor nicht zwingend
sofort dauerhaft ausgefallen.

Vorgesehener Ablauf:

```text
ein Messwert ungueltig
  -> letzten gueltigen Wert nur kurz als STALE kennzeichnen
  -> weitere Messungen abwarten
  -> bei rechtzeitiger gueltiger Rueckkehr normal fortsetzen
  -> bei Ueberschreitung der Fehleranzahl oder Zeitgrenze Sensor als FAILED
```

Verbindliche Regeln:

- `STALE` und `FAILED` sind getrennte maschinenlesbare Sensorzustaende.
- Der letzte gueltige Wert wird niemals unbegrenzt weiterverwendet.
- Die zulaessige Ueberbrueckungszeit ist eng begrenzt und wird aus Messzyklus,
  thermischer Dynamik und Sicherheitsanforderungen abgeleitet.
- Eine laufende Peltierfreigabe darf nicht minutenlang auf einem alten Wert
  beruhen.
- Der fuer eine Aktorfreigabe erforderliche Aktualitaetsstatus wird getrennt von
  der reinen Anzeige definiert.
- Mehrere aufeinanderfolgende Fehler oder eine ueberschrittene Altersgrenze
  fuehren zur Sensorfehlerlogik.
- Beim Produktfuehler kann danach die bereits spezifizierte Ersatzstrategie auf
  Luftregelung greifen.
- Beim fuer die Sicherheit erforderlichen Schrankluftfuehler ist kein blinder
  Weiterbetrieb erlaubt; das Verhalten wird in Phase 8 festgelegt.

Konkrete Anzahl und Zeitgrenze bleiben `TBD_COMMISSIONING` beziehungsweise fuer
Phase 8 offen.

## Kombinierte Plausibilitaetspruefung

Die Sensorbewertung verwendet nicht nur CRC und Busstatus, sondern kombiniert
mindestens:

1. CRC- und 1-Wire-Kommunikationsstatus
2. bekannte Sensor-Fehlerwerte und Startwerte
3. firmwarefesten physikalischen Messbereich
4. maximal plausible Aenderungsrate pro Zeit
5. Rollenbezogene Plausibilitaet fuer Luft und Produkt
6. Vergleich zwischen Produkt- und Schranklufttemperatur
7. Alter des letzten gueltigen Messwerts
8. Konsistenz nach Sensoranschluss, Sensorwechsel oder Neustart

Beispiele fuer auffaellige Situationen:

- unrealistisch grosser Temperatursprung in einem Messzyklus
- Produktfuehler aendert sich schneller als die Schrankluft und schneller als die
  definierte Produkttraegheit
- Luft- und Produkttemperatur widersprechen sich extrem und dauerhaft
- wiederholter typischer Einschalt- oder Fehlerwert
- Sensorwert bleibt unplausibel exakt konstant, obwohl ein deutlicher thermischer
  Prozess laeuft

Nicht jede Abweichung zwischen Luft und Produkt ist ein Fehler. Gerade beim
Aufheizen und Abkuehlen ist eine erhebliche Differenz normal. Deshalb werden
Grenzen zustands-, richtungs- und zeitbezogen ausgewertet.

Plausibilitaetsverletzungen besitzen unterschiedliche Schweregrade:

- Hinweis beziehungsweise Diagnoseauffaelligkeit
- voruebergehend eingeschraenkte Sensorqualitaet
- Sensorwarnung
- Sensorfehler mit Sperre oder Ersatzbetrieb

Die genaue Zuordnung erfolgt in `SAFETY_AND_FAULTS.md`.

## Individueller Kalibrier-Offset je Sensor

Jeder verwendete DS18B20 erhaelt einen eigenen PIN-geschuetzten
Temperatur-Offset.

Gespeichert werden mindestens:

- Sensor-ROM-Adresse
- Sensorrolle zum Zeitpunkt der Kalibrierung
- Offset in Grad Celsius
- verwendeter Referenzwert beziehungsweise Referenzgeraet
- Referenztemperatur oder Temperaturbereich
- Kalibrierdatum, sofern eine verlaessliche Zeit vorhanden ist
- Quelle der Aenderung
- optionaler Kommentar

Der korrigierte Wert wird logisch berechnet als:

```text
korrigierter Wert = plausibler Rohwert + Sensor-Offset
```

Verbindliche Regeln:

- Der Offset besitzt enge firmwarefeste Grenzen.
- Ein grosser benoetigter Offset wird als moeglicher Sensor- oder
  Kalibrierfehler gemeldet.
- Die Rohmessung bleibt in der Diagnose sichtbar.
- Filterung und Plausibilisierung verwenden klar dokumentiert rohe oder
  korrigierte Werte; es darf keine doppelte Offsetanwendung geben.
- Ein Sensortausch uebernimmt nicht still den Offset eines anderen Sensors.
- Die Zuordnung erfolgt ueber die eindeutige ROM-Adresse.

Eine Zweipunktkalibrierung mit Steigung und Offset ist im ersten Release nicht
vorgesehen. Sie kann spaeter ergaenzt werden, falls Messungen ueber den relevanten
Temperaturbereich eine reproduzierbare Steigungsabweichung zeigen.

## Vier Maschinen-Parametersaetze fuer die PI-Regelung

Das erste Release verwendet vier getrennte Parametersaetze:

```text
1. Luftregelung Heizen
2. Luftregelung Kuehlen
3. Produktregelung Heizen
4. Produktregelung Kuehlen
```

Jeder Parametersatz enthaelt mindestens:

- Proportionalparameter
- Integralparameter beziehungsweise Integrationszeit
- Ausgangsbegrenzungen
- Anti-Windup-relevante Parameter
- zugehoerige Filter- und Aktualisierungsannahmen
- Versions- beziehungsweise Tuningstand

Verbindliche Regeln:

- Die Parametersaetze sind Maschinen- und Serviceparameter, keine
  Rezeptparameter.
- Normale Fermentationsprogramme duerfen keine beliebigen PI-Werte enthalten.
- Programme legen Sollwerte, Zeiten, Sensorbetrieb und Prozessverhalten fest.
- Alle Parameter liegen innerhalb firmwarefester Grenzen.
- Aenderungen sind PIN-geschuetzt und waehrend eines aktiven Laufes gesperrt.
- Ein Lauf verwendet die beim Start in seinen Schnappschuss uebernommenen
  wirksamen Parameter.
- Werkseinstellungen werden erst nach den Inbetriebnahmetests festgelegt.
- Alte Tuningstaende bleiben in Export und Laufprotokoll identifizierbar.

## Strukturierter Inbetriebnahmeablauf

Die Regelparameter werden nicht allein durch normales Benutzen nach Gefuehl
festgelegt.

Der vorgesehene Serviceablauf umfasst mindestens:

### 1. Hardware- und Sensorpruefung

- Controllerboard, Versorgung und Ausgangspegel verifizieren
- Sensor-ROM-Adressen erfassen
- beide Sensoren bei stabiler gemeinsamer Temperatur vergleichen
- Offsets bestimmen und dokumentieren
- Messzyklus, CRC-Fehler und Busstabilitaet pruefen

### 2. Leerlaufversuch Heizen

- leeren beziehungsweise definiert vorbereiteten Schrank verwenden
- mit begrenzter Heizleistung einen dokumentierten Sollwertsprung ausfuehren
- Lufttemperatur, Anstiegsgeschwindigkeit, Totzeit und Ueberschwingen erfassen
- Aussen- und Innenluefterverhalten pruefen
- obere Luftbegrenzung vorbereiten

### 3. Leerlaufversuch Kuehlen

- entsprechend einen begrenzten Kuehlversuch durchfuehren
- Abkuehlgeschwindigkeit, Totzeit, Kondensation und Ueberschwingen erfassen
- untere Luftbegrenzung vorbereiten
- sicheren Richtungswechsel separat pruefen

### 4. Versuch mit definierter Referenzmasse

- dokumentierte Gefaessart und Produkt- beziehungsweise Wassermasse verwenden
- Produkt- und Luftfuehler reproduzierbar positionieren
- Heiz- und Kuehlversuche mit gleicher Ausgangslage wiederholen
- Produkttraegheit und Luft-Produkt-Differenz erfassen

### 5. Sprungantworten auswerten

Mindestens auswerten:

- erkennbare Totzeit
- Anstiegs- beziehungsweise Abfallgeschwindigkeit
- statische Abweichung
- Ueberschwingen
- Einschwingzeit
- Heiz-/Kuehlasymmetrie
- Einfluss der Produktmasse
- Einfluss der Umgebungstemperatur

### 6. Luftbegrenzungen bestimmen

- fruehe obere Luftbegrenzung bei Produktregelung
- fruehe untere Luftbegrenzung bei Produktregelung
- Leistungsreduktionsverhalten
- Abstand zu absoluten Sicherheitsgrenzen

### 7. PI-Parameter ableiten

- zuerst Luftregelung Heizen und Kuehlen abstimmen
- danach Produktregelung Heizen und Kuehlen abstimmen
- Ausgangsbegrenzung und Anti-Windup einbeziehen
- kleine Leistungen und Impulsakkumulator pruefen
- Parameter und Messgrundlage dokumentieren

### 8. Gesamtabnahme der Regelung

- Zielqualifikation pruefen
- Sollwertwechsel pruefen
- Tag-/Nachtwechsel zwischen Heizen und Kuehlen simulieren
- Sensorwechsel und Produktfuehler-Rueckkehr pruefen
- Stromunterbrechung und Wiederanlauf pruefen
- minimale und maximale vorgesehene Produktmasse pruefen
- lange Haltephase pruefen
- Temperaturkurven exportieren und auswerten

## Inbetriebnahmeprotokoll und Exporte

Jeder strukturierte Versuch besitzt mindestens:

- eindeutige Versuchs-ID
- Firmware- und Konfigurationsrevision
- Tuningstand
- verwendete Sensoren und Offsets
- Gefaess und Referenzmasse
- Startbedingungen und Umgebungshinweise
- Sollwertfolge
- Roh- und gefilterte Temperaturen
- Aktoranforderung und tatsaechliche Freigabe
- Heiz-/Kuehlrichtung
- Luftbegrenzungen und Sperren
- Luefterzustaende
- Ereignisse, Fehler und Benutzeraktionen

Die Daten sind als maschinenlesbarer Export verfuegbar. Die genaue
Speicherdichte und Dateigroesse muessen in das 4-MB-Budget passen; fuer
Inbetriebnahmeversuche darf eine laufende Webverbindung zusaetzlich dichtere
Live-Daten empfangen, ohne dass alle Rohwerte dauerhaft im Flash archiviert
werden.

## Vorbereitung spaeterer Regelstrategien

### Keine versteckte ungetestete Funktion

Kaskadenregelung und vollstaendiger PID mit Autotuning werden im ersten Release
nicht bereits funktionsfaehig implementiert und lediglich im Menue versteckt.

Nicht freigegebener Code, der trotzdem Aktoren ansteuern koennte, wuerde den
Testumfang und die Sicherheitsanalyse vergroessern.

### Vorbereitete Schnittstellen

Vorbereitet werden nur:

- austauschbare Regelstrategie-Schnittstelle
- eindeutige Strategie-ID im Laufschnappschuss und Protokoll
- getrennte Sensor-, Begrenzungs-, Sicherheits- und Aktormodule
- Diagnosefelder fuer Reglerausgang, Begrenzung und tatsaechliche Aktorfreigabe
- versionierbare Parametersaetze
- Datenmodell fuer einen spaeteren dynamischen Luftsollwert
- Akzeptanzteststruktur fuer neue Strategien

Aktiv implementiert und auswaehlbar ist im ersten Release nur die freigegebene
PI-Strategie aus `TEMPERATURE_CONTROL.md`.

## Freigabekriterien fuer eine spaetere Kaskadenregelung

Eine spaetere Strategie `cascade_product_air` darf erst freigegeben werden,
wenn mindestens:

- Luftregelung fuer Heizen und Kuehlen stabil und reproduzierbar ist
- Luftregelung bei relevanten Umgebungstemperaturen getestet wurde
- Produktverhalten mit mehreren definierten Produktmassen vermessen ist
- dynamische Luftsollwertgrenzen bestimmt und sicher begrenzt sind
- keine kritischen Luft- oder Produktueberschwinger auftreten
- innere und aeussere Integratoren nicht gegeneinander aufladen
- Sensorfehler- und Ersatzbetrieb fuer beide Regelkreise definiert sind
- die Kaskade gegenueber direkter Produktregelung eine reproduzierbare
  Verbesserung zeigt
- eine dokumentierte Rueckfallstrategie auf die freigegebene direkte
  PI-Regelung existiert
- eigene automatische und praktische Akzeptanztests bestanden sind

## Freigabekriterien fuer spaeteren PID oder Autotuning

Ein D-Anteil oder automatisches Tuning darf erst ergaenzt werden, wenn:

- die PI-Regelung als belastbare Vergleichsbasis dient
- Messrauschen, Filterung und Sensorquantisierung bekannt sind
- der D-Anteil nicht auf Einzelstoerungen oder Schaltvorgaenge ueberreagiert
- Tuningversuche durch sichere Sollwert-, Leistungs- und Luftgrenzen begrenzt sind
- Heizen und Kuehlen getrennt bewertet werden
- ein Abbruch des Tuningvorgangs jederzeit sicher moeglich ist
- die ermittelten Parameter vor Verwendung validiert und bestaetigt werden
- kein Autotuning waehrend eines Lebensmittelprozesses unbemerkt startet
- eine Rueckkehr zu den letzten gueltigen PI-Parametern jederzeit moeglich ist

## Akzeptierte Entscheidungen aus Phase 7C

- [x] fester 12-Bit-DS18B20-Messzyklus von ungefaehr zwei Sekunden
- [x] keine frei einstellbare Sensorabtastrate im ersten Release
- [x] CRC- und Plausibilitaetspruefung vor der Filterung
- [x] kurzer Medianfilter gegen Einzelspitzen
- [x] getrennte Tiefpassstaerke fuer Luft- und Produktfuehler
- [x] einzelne ungueltige Messung fuehrt zunaechst zu eng begrenztem `STALE`
- [x] `STALE` wird nach Zeit- oder Fehlergrenze zu `FAILED`
- [x] kombinierte Plausibilitaet aus CRC, Wertebereich, Aenderungsrate und
      Sensorrollenvergleich
- [x] individueller PIN-geschuetzter Offset je Sensor-ROM-Adresse
- [x] keine Zweipunktkalibrierung im ersten Release
- [x] vier Maschinen-PI-Parametersaetze fuer Luft/Produkt und Heizen/Kuehlen
- [x] strukturierter, exportierbar protokollierter Inbetriebnahmeablauf
- [x] Kaskadenregelung und PID nicht versteckt vorimplementieren
- [x] Schnittstellen, Datenmodell und Diagnose fuer spaetere Strategien vorbereiten
- [x] spaetere Strategien nur nach dokumentierten Freigabekriterien

## Noch offen fuer Phase 8, 9 und Inbetriebnahme

- konkrete Medianfensterlaenge
- konkrete Tiefpassparameter je Sensorrolle
- maximal zulaessiges Messwertalter fuer die Aktorfreigabe
- Fehleranzahl und Zeitgrenze fuer `STALE -> FAILED`
- physikalischer und prozessbezogener Plausibilitaetsbereich
- maximal plausible Aenderungsraten
- erlaubte Sensor-Offsetgrenzen
- genaues Referenzverfahren fuer die Sensorkalibrierung
- vier konkrete PI-Parametersaetze
- konkrete Luftbegrenzungen und Sicherheitsgrenzen
- Format und Umfang des Inbetriebnahmeexports
- Akzeptanzkriterien fuer Ueberschwingen, Einschwingzeit und Regelabweichung
