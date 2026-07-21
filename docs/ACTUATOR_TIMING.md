# Peltier-Schaltzeiten und Luefterlogik

## Status

Dieses Dokument beschreibt die in Phase 7B akzeptierten Regeln fuer
zeitproportionale Peltier-Ansteuerung, Mindestzeiten, Richtungswechsel,
Integratorverhalten, Luefterbetrieb und die Ueberwachung veralteter
Regelanforderungen.

Es ergaenzt [`TEMPERATURE_CONTROL.md`](TEMPERATURE_CONTROL.md) und
[`RUNTIME_BEHAVIOR.md`](RUNTIME_BEHAVIOR.md).

Konkrete Sekundenwerte bleiben bis zur Inbetriebnahme `TBD_COMMISSIONING`,
sofern sie nicht als firmwarefeste Mindestgrenze aus Sicherheitsgruenden bereits
vorher definiert werden muessen.

## Gemeinsames zeitproportionales Schaltfenster

Heizen und Kuehlen verwenden im ersten Release ein gemeinsames
zeitproportionales Schaltfenster.

Der Wert ist eine PIN-geschuetzte Serviceeinstellung innerhalb firmwarefester
Grenzen.

Verbindliche Regeln:

- Dasselbe Schaltfenster gilt fuer produkt- und luftgefuehrte Regelung.
- Unterschiedliches thermisches Verhalten wird primaer ueber getrennte
  PI-Parameter, Luftbegrenzungen und Sollwertlogik abgebildet.
- Ein Programm darf nicht eigenmaechtig ein beliebiges Schaltfenster ausserhalb
  des freigegebenen Bereichs setzen.
- Ein laufender Prozess verwendet den im Laufschnappschuss festgelegten
  wirksamen Wert.
- Aenderungen waehrend eines Laufes sind gesperrt und gelten erst fuer
  zukuenftige Laeufe.
- Das Schaltfenster darf nicht so kurz gewaehlt werden, dass es faktisch zu
  hochfrequentem Peltier-PWM wird.

Der Werkspunkt wird anhand der realen Box, des Peltiers und der thermischen
Traegheit bei der Inbetriebnahme festgelegt.

## Kleine Regleranforderungen und Impulsakkumulator

Eine Regleranforderung unterhalb der Mindest-Einschaltzeit wird nicht als extrem
kurzer Impuls ausgefuehrt und nicht einfach verworfen.

Stattdessen wird die geforderte Einschaltzeit in einem begrenzten
Impulsakkumulator gesammelt.

Beispiel:

```text
Schaltfenster:          30 s
Mindest-Einschaltzeit:   3 s
Regleranforderung:        3 % = 0,9 s je Fenster

Fenster 1: +0,9 s -> noch nicht schalten
Fenster 2: +0,9 s -> noch nicht schalten
Fenster 3: +0,9 s -> noch nicht schalten
Fenster 4: +0,9 s -> Mindestimpuls ausfuehren
```

Verbindliche Regeln:

- Der Akkumulator wird getrennt fuer die aktuell zulaessige Aktorrichtung
  behandelt.
- Es gibt keine gleichzeitigen Akkumulatoren, die spaeter unkontrolliert Heizen
  und Kuehlen nachholen.
- Bei bestaetigtem Richtungswechsel wird eine Restanforderung der alten Richtung
  verworfen.
- Bei Verlassen der temperaturgeregelten Phase, Stop, Fehler oder ungueltigem
  Sensorzustand wird die angesammelte Anforderung verworfen.
- Der Akkumulator besitzt eine feste Obergrenze und darf nicht unbegrenzt
  wachsen.
- Eine Luftbegrenzung oder Sicherheitsabschaltung darf nicht dadurch umgangen
  werden, dass zuvor gesammelte Energie spaeter nachgeholt wird.
- Nach laengerer Sperrung wird die Regelanforderung neu berechnet; ein alter
  Akkumulator wird nicht blind ausgefuehrt.

Damit bleibt eine kleine mittlere Heiz- oder Kuehlleistung moeglich, ohne das
Peltier durch sehr kurze Schaltimpulse zu belasten.

## Mindest-Ein- und Mindest-Auszeit

Fuer das Peltier werden eine Mindest-Einschaltzeit und eine Mindest-Auszeit
vorgesehen.

Beide Werte sind im PIN-geschuetzten Servicebereich innerhalb firmwarefester
Grenzen einstellbar.

### Mindest-Einschaltzeit

Nach einer normalen Freigabe bleibt die aktuelle Richtung mindestens fuer die
konfigurierte Mindest-Einschaltzeit aktiv, sofern nicht eine hoeherrangige
Bedingung ein sofortiges Abschalten verlangt.

Sofortige Abschaltung hat Vorrang bei mindestens:

- Sicherheitsfehler
- ungueltigem oder ausgefallenem fuer die Freigabe erforderlichem Sensor
- unzulaessiger H-Bruecken-Kombination
- fehlender aktueller Regelanforderung
- explizitem Stop oder Abbruch

Die Mindest-Einschaltzeit ist keine Erlaubnis, trotz eines Fehlers weiter zu
heizen oder zu kuehlen.

### Mindest-Auszeit

Nach dem Abschalten bleibt das Peltier mindestens fuer die konfigurierte
Mindest-Auszeit deaktiviert, bevor es erneut freigegeben werden darf.

Bei einem Richtungswechsel muessen sowohl Mindest-Auszeit als auch
Polaritaetswechsel-Totzeit erfuellt sein. Sind beide Zeiten gleichzeitig aktiv,
gilt die jeweils spaeter endende Sperre; sie werden nicht zwingend addiert.

## Bestaetigter Richtungswechsel

Eine einzelne oder kurzzeitige Gegenanforderung fuehrt nicht sofort zu einem
Wechsel zwischen Heizen und Kuehlen.

Vor einem Richtungswechsel muss die Gegenanforderung:

- eine definierte Mindeststaerke beziehungsweise Umschaltschwelle erreichen,
- fuer eine definierte Mindestdauer bestehen,
- auf gueltigen und plausiblen Sensorwerten beruhen,
- trotz Luftbegrenzung und Sicherheitslogik weiterhin zulaessig sein.

Erst danach beginnt der Richtungswechsel:

```text
HEAT aktiv
  -> bestaetigte COOL-Gegenanforderung
  -> Peltier AUS
  -> alte Richtung sicher deaktivieren
  -> Mindest-Auszeit und Polaritaetswechsel-Totzeit abwarten
  -> Sensoren und Freigaben erneut pruefen
  -> COOL kontrolliert freigeben
```

Dasselbe gilt spiegelbildlich fuer `COOL -> HEAT`.

Wird die Gegenanforderung waehrend der Bestaetigungs- oder Totzeit wieder
unzureichend, darf die neue Richtung nicht allein wegen der vorherigen
Anforderung eingeschaltet werden. Nach Ende der Sperrzeit wird die aktuelle
Regellage neu bewertet.

Umschaltschwelle, Bestaetigungszeit und Totzeit bleiben
`TBD_COMMISSIONING`, wobei eine firmwarefeste Mindesttotzeit niemals
unterschritten werden darf.

## Integralanteil bei Sperren und Totzeiten

Der PI-Integralanteil darf waehrend einer Aktorsperre nicht unkontrolliert
anwachsen.

Verbindliches Verhalten:

- Bei Mindest-Auszeit, Polaritaetswechsel-Totzeit, Luftbegrenzung oder sonstiger
  temporaerer Aktorsperre wird der Integralanteil eingefroren oder durch
  geeignetes Anti-Windup begrenzt.
- Die Differenz zwischen Regleranforderung und tatsaechlich freigegebener
  Aktorleistung wird fuer das Anti-Windup beruecksichtigt.
- Ein Richtungswechsel setzt den Integralanteil nicht automatisch und pauschal
  auf null; die genaue Rueckfuehrungs- oder Begrenzungsstrategie wird beim
  Tuning festgelegt.
- Bei Phasenwechsel, Sollwertsprung, Sensorwechsel oder manueller Laufanpassung
  kann eine kontrollierte Anpassung beziehungsweise Teilruecksetzung notwendig
  sein.
- Nach einem Fehler wird kein alter aufgeladener Integralwert blind wieder
  freigegeben.

## Aussenluefter

Der Aussen- beziehungsweise Kuehlkoerperluefter wird immer gemeinsam mit einer
Peltierfreigabe eingeschaltet und besitzt einen zwingenden Nachlauf.

### Kein Vorlauf

Es gibt im normalen Betrieb keine absichtliche Vorlaufzeit, waehrend der der
Aussenluefter bereits laeuft und das Peltier noch wartet.

Begruendung:

- Der Luefter muss nicht erst einen thermischen Zustand aufbauen.
- Eine zusaetzliche Vorlaufverzoegerung bringt fuer den vorgesehenen Aufbau
  keinen erkennbaren Nutzen.
- Unnoetige Verzoegerungen der Regelreaktion werden vermieden.

Die Aktorlogik setzt den Luefterbefehl im selben Steuerzyklus vor oder gemeinsam
mit der Peltierfreigabe. Das bedeutet keine konfigurierbare Wartezeit, aber eine
sichere programmtechnische Reihenfolge.

### Betrieb und Nachlauf

Verbindliche Regeln:

- Bei jeder aktiven Heiz- oder Kuehlrichtung ist der Aussenluefter eingeschaltet.
- Beim Abschalten des Peltiers beginnt ein konfigurierbarer Nachlauf.
- Eine erneute Peltierfreigabe waehrend des Nachlaufs laesst den Luefter ohne
  Unterbrechung weiterlaufen.
- Der Nachlauf gilt auch nach normalem Stop, Richtungswechsel und veralteter
  Regelanforderung.
- Ein Fehler darf den Aussenluefter nicht automatisch gleichzeitig mit dem
  Peltier abschalten, wenn fuer die Abfuhr vorhandener Waerme ein Nachlauf
  erforderlich ist.
- Die genaue Reaktion bei Luefterfehler, Versorgungsausfall oder kritischem
  Temperaturfehler wird in `SAFETY_AND_FAULTS.md` festgelegt.

Die Nachlaufzeit ist im Servicebereich innerhalb firmwarefester Mindest- und
Maximalgrenzen einstellbar. Eine separate Vorlaufzeit wird nicht angeboten.

## Innenluefter und Nachlauf

Der Innenluefter laeuft gemaess `TEMPERATURE_CONTROL.md` waehrend aller
aktiven temperaturgeregelten Phasen dauerhaft, auch wenn das Peltier innerhalb
eines Schaltfensters oder wegen einer Totzeit ausgeschaltet ist.

Nach dem Ende der Temperaturregelung besitzt der Innenluefter einen
konfigurierbaren Nachlauf.

Verbindliche Regeln:

- Der Nachlauf beginnt erst, wenn die temperaturgeregelte Phase tatsaechlich
  verlassen wurde.
- Kurze Peltier-Auszeiten innerhalb einer geregelten Phase beenden den
  Dauerbetrieb des Innenluefters nicht.
- Ein erneuter geregelter Zustand waehrend des Nachlaufs laesst den Innenluefter
  ohne Unterbrechung weiterlaufen.
- Im normalen Standby ist der Innenluefter nach beendetem Nachlauf aus.
- Nachlaufzeit und zulaessiger Einstellbereich sind PIN-geschuetzt und bleiben
  `TBD_COMMISSIONING`.
- Fehlerzustaende koennen einen laengeren, kuerzeren oder gesperrten Betrieb
  verlangen; dies wird in Phase 8 festgelegt.

## Aktualisierungs-Watchdog fuer Regelanforderungen

Die Aktorlogik darf eine alte Heiz- oder Kuehlanforderung nicht unbegrenzt
weiter ausfuehren.

Jede gueltige Regelanforderung besitzt deshalb:

- eine Revisions- oder Sequenznummer,
- einen monotonen Erzeugungszeitpunkt,
- einen begrenzten Gueltigkeitszeitraum,
- den zugehoerigen Prozess- und Sensorzustand.

Wird innerhalb des festgelegten Watchdog-Zeitraums keine neue gueltige
Regelanforderung geliefert:

```text
Peltier sicher AUS
  -> H-Bruecke deaktivieren
  -> Aussenluefter-Nachlauf ausfuehren
  -> Innenluefter gemaess Fehlerstrategie behandeln
  -> Impulsakkumulator verwerfen
  -> Integralanteil sperren beziehungsweise sichern
  -> Fehler erzeugen und protokollieren
```

Verbindliche Regeln:

- Die letzte Anforderung wird nicht als dauerhafte Ersatzanforderung verwendet.
- Ein automatischer Neustart ist nicht die erste Reaktion.
- Eine neue Regelanforderung allein darf den Fehler nicht still loeschen; die
  Fehlerklasse bestimmt Wiederfreigabe und Quittierung.
- Der Watchdog wird in einer von der eigentlichen PI-Berechnung getrennten
  Aktorlogik ausgewertet.
- Ein blockierter Webserver oder fehlendes WLAN darf den Watchdog nicht
  ausloesen, solange die lokale Regelaufgabe korrekt weiterarbeitet.

Der genaue Watchdog-Zeitraum wird aus Reglerzyklus, Sensorzyklus und maximaler
zulaessiger Softwarelatenz abgeleitet.

## Reihenfolge einer normalen Peltierfreigabe

Eine normale Freigabe folgt logisch mindestens diesem Ablauf:

```text
aktuelle Sensorwerte gueltig
  -> Regelanforderung aktuell
  -> Luftbegrenzung und Sicherheitsfreigabe erteilt
  -> Mindest-Auszeit und Totzeit beendet
  -> gewuenschte Richtung exklusiv festlegen
  -> Aussenluefter einschalten
  -> H-Bruecke kontrolliert freigeben
  -> Mindest-Einschaltzeit ueberwachen
```

Es gibt dabei keine absichtliche Lueftervorlaufzeit. Der Luefter wird jedoch in
der sicheren Ansteuerreihenfolge vor beziehungsweise gleichzeitig mit der
Leistungsfreigabe gesetzt.

## Akzeptierte Entscheidungen aus Phase 7B

- [x] gemeinsames PIN-geschuetzt einstellbares Schaltfenster fuer Heizen und
      Kuehlen
- [x] kleine Regleranforderungen in begrenztem Impulsakkumulator sammeln
- [x] Mindest-Ein- und Mindest-Auszeit innerhalb firmwarefester Grenzen
- [x] Richtungswechsel erst nach ausreichend starker und anhaltender
      Gegenanforderung
- [x] Mindest-Auszeit und Totzeit muessen vor neuer Richtung beide erfuellt sein
- [x] Integralanteil waehrend Sperren und Totzeiten einfrieren beziehungsweise
      durch Anti-Windup begrenzen
- [x] Aussenluefter ohne absichtliche Vorlaufzeit gemeinsam mit dem Peltier
      einschalten
- [x] zwingender konfigurierbarer Aussenluefter-Nachlauf
- [x] Innenluefter waehrend geregelter Phasen dauerhaft und danach mit
      konfigurierbarem Nachlauf
- [x] veraltete Regelanforderung schaltet das Peltier sicher aus und erzeugt
      einen Fehler
- [x] notwendiger Luefternachlauf wird auch bei veralteter Anforderung
      durchgefuehrt

## Noch offen fuer Phase 7C und Phase 8

- konkretes Schaltfenster und freigegebener Einstellbereich
- konkrete Mindest-Ein- und Mindest-Auszeit
- konkrete Polaritaetswechsel-Totzeit
- Gegenanforderungsschwelle und Bestaetigungsdauer
- konkrete Impulsakkumulatorgrenze und Rundungsregeln
- genaue Anti-Windup-Strategie
- Sensor- und Reglerzyklus
- Aktualisierungs-Watchdog-Zeitraum
- Aussen- und Innenluefter-Nachlaufzeiten
- Luefterverhalten je Fehlerklasse
- Erkennung eines ausgefallenen oder blockierten Luefters
- sichere H-Bruecken-Sequenz fuer die bestaetigte Hardware
- Verhalten bei Versorgungseinbruch waehrend einer Schaltsequenz
