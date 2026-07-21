# Unterbrechungen und Wiederanlauf

## Status

Dieses Dokument ergaenzt [`STATE_MACHINE.md`](STATE_MACHINE.md) um die in
Phase 3B und 3C akzeptierten Sonderfaelle. Exakte Zeitgrenzen,
Temperaturmodelle und Fehlerklassen werden spaeter in
`TEMPERATURE_CONTROL.md`, `SAFETY_AND_FAULTS.md` und
`SETTINGS_AND_STORAGE.md` vervollstaendigt.

## Grundsatz: keine blockierende Benutzerentscheidung

Ein laufender Prozess darf nach einem Stromausfall nicht unbegrenzt auf eine
Benutzereingabe warten. Der Benutzer steht unter Umstaenden erst Stunden spaeter
vor dem Geraet oder oeffnet die Weboberflaeche erst spaeter.

Deshalb gilt:

- Nach dem Neustart wird der Prozess automatisch und phasenbezogen in einer
  sinnvollen, sicheren Form weitergefuehrt.
- Eine fehlende Benutzerquittierung ist allein kein Grund, die Regelung zu
  stoppen.
- Die Software zeigt die getroffene Wiederanlaufentscheidung, die geschaetzte
  Unterbrechungsdauer und eine allfaellige Zeitkorrektur an.
- Der Benutzer kann die automatisch gewaehlte Fortsetzung spaeter pruefen und,
  soweit sicher zulaessig, anpassen oder den Lauf manuell beenden.
- Ein echter Sicherheitsfehler kann den Prozess weiterhin stoppen und hat
  Vorrang vor dem Grundsatz des automatischen Fortfahrens.

## Kein Tuerkontakt im ersten Release

Im ersten Release wird kein Tuerkontakt eingebaut.

Folgen:

- Die Software kann eine geoeffnete Tuer nicht direkt erkennen.
- Temperaturabweichungen durch eine geoeffnete Tuer werden wie andere
  Temperaturabweichungen behandelt.
- Es gibt keinen tuergesteuerten Timerstopp und keine tuergesteuerte
  Peltierabschaltung.
- Bedienungsanweisungen duerfen nicht behaupten, die Tuerstellung zu kennen.

Die Softwarearchitektur soll optional ein spaeteres Ereignis `door_open`
beziehungsweise `door_closed` aufnehmen koennen. Solange kein Tuerkontakt in
der bestaetigten Hardwarekonfiguration vorhanden ist, bleibt diese Funktion
deaktiviert und hat keinerlei Einfluss auf den Ablauf.

Falls spaeter ein Tuerkontakt nachgeruestet wird, ist als Standardverhalten
vorgesehen:

- Peltier bei offener Tuer AUS
- Fermentationstimer laeuft weiter
- sichtbare Meldung `Tuer offen`
- nach laengerer Tueröffnung Warnung und akustisches Signal
- nach dem Schliessen normale Temperaturregelung fortsetzen

## Ausfall oder Entfernen des Produktfuehlers

Ein produktgefuehrter Lauf darf nicht still und automatisch auf den
Luftfuehler wechseln.

Bei Ausfall, Entfernen oder ungueltigen Messwerten des Produktfuehlers:

1. Peltier vorlaeufig sicher AUS.
2. Der Luftfuehler und alle weiteren Sicherheitsbedingungen werden geprueft.
3. Die Bedienoberflaeche zeigt den Sensorfehler deutlich an.
4. Nur zulaessige Aktionen werden angeboten.

Moegliche Aktionen:

- `Mit Luftfuehler fortsetzen`, falls der Luftfuehler gueltig ist
- `Abbrechen und ausschalten`
- `Abbrechen und kuehlen`, falls Kuehlen sicher zulaessig ist

Anders als beim Stromausfall ist ein automatischer Wechsel des primaeren
Sensors nicht vorgesehen, weil der Benutzer beim Start bewusst einen
produktgefuehrten Lauf gewaehlt hat. Bis zur Entscheidung bleibt die
Leistungsstufe in einem sicheren, spaeter je Fehlerklasse festzulegenden
Zustand.

Bei einer Fortsetzung mit Luftfuehler:

- der Sensorwechsel wird deutlich angezeigt
- der Wechsel wird mit Zeit und Messwerten protokolliert
- der Lauf bleibt derselbe Programmschnappschuss, erhaelt aber einen
  dokumentierten Wechsel des Regelmodus
- die spaetere Bewertung der Temperaturfuehrung beruecksichtigt die geringere
  Aussagekraft gegenueber einer direkten Produktmessung

## Maximale Wartezeit nach dem Vorheizen

Der Zustand `WAITING_FOR_PRODUCT` besitzt eine pro Programm konfigurierbare
maximale Wartezeit.

Vorgesehener Ablauf:

1. Der leere Schrank haelt die vorbereitete Temperatur.
2. Vor Ablauf der Maximalzeit erfolgt eine sichtbare und akustische Warnung.
3. Wird die Maximalzeit erreicht, beginnt die Fermentation nicht automatisch.
4. Die Vorheizphase wird sicher beendet und der Lauf als nicht gestartet
   beziehungsweise abgebrochen protokolliert.
5. Der Benutzer kann den Programmlauf anschliessend neu starten.

Die konkreten Warte- und Vorwarnzeiten bleiben `TBD_COMMISSIONING`.

## Fehlerquittierung nach Fehlerklasse

Die Rueckkehr aus `FAULT` ist von der Fehlerklasse abhaengig.

### Potenziell fortsetzbare Fehler

Beispiele:

- Produktfuehler voruebergehend entfernt und danach wieder gueltig
- kurzzeitiger nichtkritischer Kommunikationsfehler
- voruebergehend ungueltiger Messwert, der sich eindeutig erholt hat

Eine Fortsetzung darf nur angeboten werden, wenn Sensoren, Aktoren und
Sicherheitsbedingungen erneut vollstaendig geprueft wurden.

### Laufbeendende Fehler

Beispiele:

- harte Ueber- oder Untertemperaturgrenze verletzt
- widerspruechliche H-Bruecken-Anforderung
- nicht gewaehrleisteter sicherer Ausgangszustand
- beschaedigte oder widerspruechliche Persistenzdaten
- kritischer interner Softwarefehler

Bei diesen Fehlern fuehrt eine Quittierung nur in einen sicheren Zustand. Der
urspruengliche Lauf wird nicht normal fortgesetzt.

Die verbindliche Fehlerklassifikation folgt in `SAFETY_AND_FAULTS.md`.

## Phasenbezogene automatische Wiederaufnahme

Nach einer Unterbrechung wird nicht blind der letzte elektrische Aktorzustand
wiederhergestellt. Stattdessen wird aus dem gespeicherten fachlichen Zustand
eine neue sichere Aktion berechnet.

### PREHEATING oder REACHING_TARGET

- Sensoren und Sicherheitsbedingungen pruefen
- Zieltemperatur erneut anfahren
- danach den normalen Ablauf fortsetzen
- keine Fermentationszeit anrechnen, solange sie noch nicht begonnen hatte

### WAITING_FOR_PRODUCT

- Vorheizzustand nur fortsetzen, wenn er noch innerhalb der konfigurierten
  maximalen Wartezeit liegt
- andernfalls Vorheizen beenden und als nicht gestarteten Lauf protokollieren
- niemals automatisch annehmen, dass das Produkt eingesetzt wurde

### QUALIFYING_TARGET

- Zieltemperatur erneut erreichen
- Zielqualifikation neu beginnen
- keine bereits teilweise absolvierte Qualifikationszeit blind uebernehmen

### FERMENTING

- Temperaturregelung nach gueltigem Sensorcheck sofort wieder aufnehmen
- Prozess nicht auf eine Benutzerentscheidung warten lassen
- Unterbrechungswirkung spaeter anhand der verfuegbaren Zeit- und
  Temperaturdaten bewerten
- verbleibende Fermentationszeit automatisch verlaengern, falls die
  Unterbrechung den wirksamen Prozessfortschritt reduziert hat

### COOLING oder COOL_HOLDING

- bei gueltigen Sensoren und ohne Sicherheitsfehler Kuehlung beziehungsweise
  Kuehlhalten wieder aufnehmen
- eine Benutzerquittierung ist fuer die Wiederaufnahme nicht erforderlich
- Dauer einer zeitlich begrenzten Haltephase anhand der wiederhergestellten
  Zeitbasis korrigieren

### MANUAL_HOLDING

- Zieltemperatur nach Sensor- und Sicherheitspruefung wieder halten
- den Benutzer ueber die Unterbrechung informieren

### COMPLETED

- keine Temperaturregelung neu starten
- abgeschlossenen Zustand und Meldung wiederherstellen

## Biologische Wirkung einer Stromunterbrechung

Eine Fermentation stoppt bei sinkender Temperatur nicht schlagartig. Sie laeuft
in der Regel langsamer weiter. Deshalb sind weder ein vollstaendiges Anrechnen
der Stromausfallzeit noch ein vollstaendiges Pausieren des Timers allgemein
korrekt.

Das Ziel ist eine **temperaturgewichtete Unterbrechungskompensation**:

- Zeit nahe der Solltemperatur zaehlt weitgehend als wirksame
  Fermentationszeit.
- Zeit deutlich unter der Solltemperatur zaehlt nur teilweise.
- Daraus wird eine erforderliche Verlaengerung der verbleibenden Laufzeit
  abgeleitet.

Konzeptionell wird die wirksame Fermentationszeit als temperaturabhaengige
Groesse behandelt:

```text
wirksamer Fortschritt = Summe aus Zeitabschnitten * Aktivitaetsfaktor(Temperatur)
```

Der Aktivitaetsfaktor ist bei der Solltemperatur ungefaehr `1`. Bei tieferen
Temperaturen liegt er darunter. Die konkrete Funktion darf nicht ohne
praktische Grundlage erfunden werden, weil sie von Kultur und Prozess abhaengt.

## Verfuegbare Temperaturgrundlage

### Produktgefuehrter Lauf

Ist nach dem Neustart ein gueltiger Produktfuehler vorhanden, hat dessen
Temperatur fuer die Bewertung Vorrang.

### Luftgefuehrter Lauf

Ohne Produktfuehler wird die **Schranklufttemperatur** verwendet. Damit ist die
Temperatur im Innenraum des Fermentationsschrankes gemeint, gemessen durch den
fest eingebauten Luftfuehler.

Die daraus berechnete Kompensation besitzt eine geringere Sicherheit, weil die
Produkttemperatur traeger reagieren kann als die Schranklufttemperatur.

Eine zusaetzliche Messung der Umgebungsluft ausserhalb des Schrankes ist fuer
das erste Release nicht vorgesehen.

## Fehlende Messwerte waehrend des Stromausfalls

Der ESP32 und seine Temperatursensoren sind waehrend des Stromausfalls ohne
Versorgung. Deshalb existiert fuer die Unterbrechungszeit keine kontinuierliche
Messkurve.

Verfuegbar sind hoechstens:

- letzte gueltige Produkt- oder Schranklufttemperatur vor dem Ausfall
- Zeitstempel des letzten gespeicherten Zustands
- erste gueltige Produkt- oder Schranklufttemperatur nach dem Neustart
- Dauer der Unterbrechung, sobald eine verlaessliche Zeitquelle verfuegbar ist
- spaeter eventuell ein kalibriertes thermisches Modell

Die Temperaturentwicklung waehrend des Ausfalls muss daher geschaetzt werden.
Eine spaetere Inbetriebnahme kann das Abkuehlverhalten des realen Schrankes mit
Wasser beziehungsweise Testlast vermessen und daraus ein konservatives
thermisches Modell ableiten.

## Wiederanlauf vor verfuegbarer Netzwerkzeit

Der ESP32 startet schneller als Router und Netzwerkzeit zur Verfuegung stehen.
Deshalb darf der fachliche Wiederanlauf nicht auf die NTP-Synchronisation
blockieren.

Vorgesehener Ablauf:

```text
BOOT
  -> gespeicherten Lauf und Sensoren pruefen
  -> sichere phasenbezogene Fortsetzung sofort beginnen
  -> Netzwerk und Zeitabgleich parallel starten
  -> nach erfolgreicher Zeitsynchronisation Unterbrechungsdauer berechnen
  -> Zeitkompensation und Protokoll nachtraeglich praezisieren
```

Bis die Zeit synchronisiert ist:

- wird der Zustand als `recovery_time_pending` markiert
- zeigt die Oberflaeche an, dass die Unterbrechungsdauer noch bestimmt wird
- wird keine ungesicherte exakte Restzeit behauptet
- laeuft die sichere phasenbezogene Regelung trotzdem weiter

## Zeitquelle fuer die Unterbrechungsdauer

### Erstes Release

Die primaere Zeitquelle ist Netzwerkzeit, beispielsweise ueber NTP.

Dafuer werden waehrend eines laufenden Prozesses ausreichend aktuelle
Zeitstempel und Zustandsdaten persistent gespeichert. Nach dem Neustart wird der
aktuelle Netzwerkzeitstempel mit dem letzten gueltigen gespeicherten
Zeitstempel verglichen.

Die Persistenzstrategie muss Schreibverschleiss und Genauigkeit gegeneinander
abwaegen. Das konkrete Speicherintervall wird in
`SETTINGS_AND_STORAGE.md` festgelegt.

### Netzwerk noch nicht verfuegbar

Fehlende Netzwerkzeit unmittelbar nach dem Boot ist kein Fehler. Das Geraet
arbeitet mit einer vorlaeufigen, phasenbezogenen Wiederanlaufstrategie weiter.
Sobald Netzwerkzeit verfuegbar ist, wird die Unterbrechungsdauer nachgetragen.

Bleibt Netzwerkzeit ueber eine spaeter festzulegende Maximaldauer nicht
verfuegbar, gilt die Unterbrechungsdauer als unsicher. Auch dann wird nicht
allein deshalb abgebrochen. Die Software verwendet eine konservative
programmspezifische Ersatzlogik und kennzeichnet die Schaetzung mit niedriger
Vertrauensstufe.

### Spaetere RTC-Option

Die Architektur soll eine spaetere batteriegepufferte Echtzeituhr, zum Beispiel
ein DS3231-Modul, als alternative oder zusaetzliche Zeitquelle ermoeglichen.
Ein RTC-Modul ist fuer das erste Release nicht erforderlich und seine spaetere
Montage ist nicht vorausgesetzt.

## Automatische Zeitkompensation

### Sicher bewertbare Unterbrechung

Wenn Unterbrechungsdauer und Temperaturschaetzung ausreichend verlaesslich sind:

- wird die Restzeit automatisch korrigiert
- wird die Korrektur angezeigt und protokolliert
- laeuft der Prozess ohne Benutzerbestaetigung weiter

### Lange oder unsichere Unterbrechung

Auch bei einer langen oder nur konservativ schaetzbaren Unterbrechung wartet das
Geraet nicht auf den Benutzer.

Vorgesehen ist:

- sofortige sichere Fortsetzung entsprechend der Prozessphase
- konservative automatische Verlaengerung der Fermentationszeit
- Fortsetzung von Kuehlung oder Kuehlhalten, falls dies die aktive Phase war
- sichtbare Kennzeichnung der niedrigen Vertrauensstufe
- spaetere Moeglichkeit fuer den Benutzer, die Verlaengerung zu pruefen und
  innerhalb sicherer Grenzen anzupassen

Die Oberflaeche zeigt mindestens:

- geschaetzte Unterbrechungsdauer
- letzte Temperatur vor dem Ausfall
- aktuelle Produkt- oder Schranklufttemperatur
- Vertrauensstufe der Schaetzung
- automatisch angewendete Verlaengerung
- automatisch gewaehlte Wiederanlaufaktion

Eine Schaltflaeche `Abbrechen` kann als bewusste spaetere Benutzeraktion
vorhanden sein, ist aber keine Voraussetzung fuer das automatische
Wiederanlaufen.

## Akzeptierte Entscheidungen

- [x] kein Tuerkontakt im ersten Release
- [x] optionale Tuerereignisse in der Architektur fuer eine spaetere Erweiterung
- [x] bei spaeterem Tuerkontakt Peltier AUS, Timer laeuft weiter
- [x] Produktfuehlerausfall fuehrt nicht zu stillem Wechsel auf Luftregelung
- [x] Fortsetzung mit Luftfuehler ist nach Benutzerentscheidung moeglich
- [x] maximale Wartezeit in `WAITING_FOR_PRODUCT` pro Programm
- [x] Fehlerquittierung und Fortsetzung richten sich nach Fehlerklasse
- [x] Stromausfallzeit wird weder pauschal voll angerechnet noch voll pausiert
- [x] Ziel ist eine temperaturgewichtete Verlaengerung der Fermentationszeit
- [x] Produktfuehler hat fuer die Kompensation Vorrang
- [x] ohne Produktfuehler wird die Schranklufttemperatur verwendet
- [x] Wiederanlauf wartet nicht blockierend auf den Benutzer
- [x] phasenbezogene sichere Aktion wird automatisch ausgefuehrt
- [x] Netzwerkzeit ist im ersten Release die primaere Zeitquelle
- [x] der Wiederanlauf beginnt bereits vor verfuegbarer Netzwerkzeit
- [x] spaetere RTC-Unterstuetzung wird architektonisch vorgesehen

## Noch offen

- Grenzwert fuer kurze und lange Unterbrechung
- konkrete Temperatur-Aktivitaetsfunktion oder konservative Ersatzlogik
- maximale automatisch zulaessige Zeitverlaengerung
- Speicherintervall fuer Zeitstempel und Laufzustand
- Maximaldauer fuer `recovery_time_pending`
- genaue programmspezifische Ersatzlogik bei dauerhaft unbekannter Zeit
- Behandlung der Entscheidungszeit bei Produktfuehlerausfall
- konkrete Fehlerklassen und Fortsetzungsbedingungen
