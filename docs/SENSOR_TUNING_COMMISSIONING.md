# Sensorverarbeitung, Tuning und Inbetriebnahme

## Umfang

Dieses Dokument spezifiziert Messzyklus, Qualitätszustände, Filterung,
Kalibrierung, PI-Parametersätze und thermische Inbetriebnahme für die drei
DS18B20-Rollen des ersten Aufbaus:

1. Schrankluft
2. abnehmbares Produkt
3. Kühlkörper beziehungsweise Aussenwärmetauscher

Konkrete numerische Werte bleiben bis zu den Messungen
`TBD_COMMISSIONING`.

## Messzyklus

- DS18B20-Auflösung: 12 Bit
- nominaler vollständiger Messzyklus: ungefähr 2 Sekunden
- feste, reproduzierbare Abtastrate im Regelkern
- Konvertierung und Buszugriff nicht blockierend
- jeder Sensor besitzt Zeitstempel, Alter und Qualitätsstatus
- ein langsamer Sensorbus darf Aktor-Watchdog und Sicherheitsaufgaben nicht
  blockieren

Der exakte Tasktakt kann technisch feiner sein; ein neuer Regelmesswert entsteht
nominal ungefähr alle zwei Sekunden.

## Verarbeitungskette

```text
Rohprobe
-> Bus- und CRC-Prüfung
-> DS18B20-Fehlerwertprüfung
-> physikalischer Wertebereich
-> maximale Änderungsrate und Sprungprüfung
-> Medianfilter gegen Einzelspitzen
-> individueller Kalibrier-Offset
-> sensorbezogener Tiefpass
-> Qualitätsstatus und gefilterter Regelwert
```

Diagnose zeigt getrennt:

- Rohwert
- korrigierten Wert
- gefilterten Wert
- Offset
- Messalter
- CRC- und Busfehler
- Trend
- Sensorrolle und Verwendung

## Qualitätszustände

### `VALID`

- aktuelle, wiederholt gültige Messung
- alle relevanten Plausibilitätsprüfungen bestanden
- darf entsprechend der Rolle für Regelung oder Sicherheit verwendet werden

### `STALE`

- einzelne Probe fehlt oder ist ungültig
- letzter gültiger Wert wird nur als sichtbar veraltet geführt
- enge, firmwarebegrenzte Überbrückungszeit
- keine minutenlange Aktorfreigabe auf altem Wert

### `FAILED`

- Fehleranzahl oder maximale Zeit ohne gültige Probe überschritten
- Sensor verliert seine Regel- beziehungsweise Sicherheitsfreigabe
- Wiedererkennungsablauf erforderlich

Grenzen und Anzahl notwendiger Proben bleiben `TBD_COMMISSIONING`, besitzen aber
firmwarefeste sichere Obergrenzen.

## Plausibilitätsprüfungen

Mindestens:

- CRC und Busantwort
- typische DS18B20-Fehler- und Einschaltwerte
- physikalisch zulässiger Temperaturbereich
- maximal plausible Änderung pro Zeit
- dauerhaft unveränderter Wert nur rollen- und prozessbezogen bewerten
- Widerspruch zwischen Sensorrollen
- Sensorkennung und erwartete ROM-Adresse fester Sensoren

Die Produkttemperatur darf wegen der ungefähr 50-W-Peltierleistung, grosser Masse
und träger Gefässe lange nahezu unverändert bleiben. Das allein ist kein
Sensorfehler. Für kurzfristige Aktordiagnose sind Schrankluft- und
Kühlkörpertrend aussagekräftiger.

## Filter

### Medianfilter

Ein kurzer Medianfilter entfernt einzelne Spitzen, ohne einen dauerhaften Trend
zu verstecken.

### Tiefpass

- Schrankluft: relativ schnelle Filterung
- Produkt: stärkere Glättung aufgrund hoher thermischer Trägheit
- Kühlkörper: ausreichend schnell für thermische Sicherheitsreaktion

Rohpfad und Sicherheitsprüfung bleiben getrennt vom gefilterten Regelwert. Ein
extremer Rohwert darf nicht durch einen langsamen Filter verdeckt werden.

Filterlängen und Zeitkonstanten: `TBD_COMMISSIONING`.

## Kalibrierung

Jeder Sensor erhält anhand seiner ROM-Adresse einen individuellen Offset.

Gespeichert werden:

- ROM-Adresse
- Rolle
- Offset
- Referenzmessgerät
- Referenztemperatur beziehungsweise Messbereich
- Datum oder monotone Revisionsinformation
- Bedienquelle

Der Offset ist PIN-geschützt und innerhalb firmwarefester Grenzen.

Release 1 verwendet zunächst eine Einpunkt-Offsetkorrektur im relevanten
Temperaturbereich. Eine Zweipunktkorrektur ist nur nötig, wenn Vergleichsmessungen
eine systematische Steigungsabweichung zeigen.

## Wiedererkennung eines Sensors

Nach `STALE` oder `FAILED`:

1. Bus und Messablauf neu initialisieren
2. mehrere gültige Proben verlangen
3. Wertebereich und Änderungsrate prüfen
4. Stabilitätszeit abwarten
5. Rolle und ROM-Adresse prüfen
6. Status sichtbar aktualisieren
7. Ereignis protokollieren

Ein automatischer Rückwechsel zum Produktfühler erfolgt nur, wenn das Programm
die Strategie `automatic_validated_return_to_product` erlaubt und die
Plausibilitätsprüfung bestanden ist.

Schrankluft- und Kühlkörpersensor sind Sicherheitssensoren. Ihr dauerhafter
Ausfall erlaubt keine normale Peltierfreigabe.

## PI-Parametersätze

Vier Maschinenparametersätze:

```text
Luftregelung Heizen
Luftregelung Kühlen
Produktregelung Heizen
Produktregelung Kühlen
```

Sie enthalten mindestens:

- proportionalen Faktor
- integralen Faktor beziehungsweise Integrationszeit
- Neutralbereich
- Ausgangsgrenzen
- Anti-Windup-Regeln
- Richtungsspezifische Leistungslimits

Die Parameter gehören zur Maschine, nicht zum einzelnen Rezept. Programme legen
Solltemperatur und Prozessverhalten fest, aber keine beliebigen
Reglerverstärkungen.

## Integratorregeln

Der Integrator wird begrenzt oder eingefroren bei:

- Sicherheits- oder Luftbegrenzung
- Mindest-Auszeit und Totzeit
- ungültigem Regelsensor
- Richtungswechsel
- gesperrter Aktorfreigabe
- Wechsel des primären Regelsensors
- Prozessphasen, in denen keine Temperaturregelung aktiv ist

Ein Sensor- oder Richtungswechsel darf keinen gespeicherten grossen
Integratorimpuls auf die neue Lage übertragen.

## Strukturierte Inbetriebnahme

### 1. Sensorvergleich

- alle drei DS18B20 nebeneinander messen
- externes Referenzmessgerät dokumentieren
- Offsets im relevanten Temperaturbereich bestimmen
- Wiederholbarkeit und Rauschen erfassen

### 2. Leerer Schrank

- Aufheizen
- Abkühlen
- Halten mehrerer Sollwerte
- Luftverteilung an mehreren Messpunkten
- Reaktion von Schrankluft und Kühlkörper

### 3. Kleine Referenzmasse

- definierte Masse und Gefäss
- Produkt- und Luftreaktion
- Überschwingen und Einschwingzeit
- Heiz- und Kühlrichtung

### 4. Grosse Referenzmasse

- gleiche Messungen mit höherer Trägheit
- Grenzen direkter Produktregelung beurteilen
- realistische maximale Zielerreichungszeit bestimmen

### 5. Sprungantworten

Für Luft/Produkt und Heizen/Kühlen:

- Ausgangszustand dokumentieren
- begrenzten definierten Aktorbetrieb anwenden
- Temperaturverläufe und Trends exportieren
- Totzeit, Verzögerung und Zeitkonstanten abschätzen
- keine automatische Aktoridentifikation ohne Sicherheitsgrenzen

### 6. PI-Abstimmung

- Luftregelung Heizen stabil abstimmen
- Luftregelung Kühlen stabil abstimmen
- Produktregelung Heizen mit Luftbegrenzung abstimmen
- Produktregelung Kühlen mit Luftbegrenzung abstimmen
- Überschwingen, Einschwingzeit und Richtungswechsel prüfen

### 7. Grenzwerte

- frühe Luftbegrenzung
- Prozesswarnungen
- Sicherheits-Eingriffsgrenze
- harte Notgrenze
- Kühlkörpergrenzen und Trendregeln
- begrenzte Gegenrichtungsversuche
- Temperatursicherung und Montageort

### 8. Prozessnahe Tests

- typische Tages-/Nachtumgebung mit Wechsel Heizen/Kühlen
- Vorheizen und Produkteinsatz
- Zielqualifikation
- Stromunterbrechung
- Sensorabzug und Rückkehr
- kleine und grosse Produktmasse

Die Messprotokolle werden den Issues #34 und #35 zugeordnet.

## Kaskadenregelung und PID

Release 1 implementiert keine aktive Kaskadenregelung und kein PID-Autotuning.
Vorbereitet werden lediglich:

- austauschbare Regelstrategie
- Diagnosefelder für Produkt- und Luftkreis
- getrennte Parameterrevisionen
- begrenzter dynamischer Luftsollwert als späterer Erweiterungspunkt

Spätere Freigabe benötigt:

- stabile Luftregelung Heizen und Kühlen
- Messungen mit mehreren Produktmassen
- nachweisbare Verbesserung gegenüber direkter Produktregelung
- validierte Luftsollwertgrenzen
- eigene Sicherheits- und Akzeptanztests
- dokumentierte Rückfallstrategie

Eine nicht validierte Zukunftsstrategie darf in Release 1 nicht auswählbar sein.
