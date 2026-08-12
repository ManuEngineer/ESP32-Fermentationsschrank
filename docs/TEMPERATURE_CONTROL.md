# Temperaturregelung und Aktorlogik

## Status

Dieses Dokument beschreibt die in Phase 7A akzeptierten Grundregeln fuer
Regelverfahren, Heiz-/Kuehlbetrieb, Sensorrollen, Zielqualifikation,
Sensor-Rueckwechsel und Aenderungen waehrend eines aktiven Laufes.

Konkrete Regelparameter, Schaltfenster, Hysteresen, Luftbegrenzungen,
Qualifikationszeiten und Nachlaufzeiten bleiben bis zu den praktischen
Schranktests `TBD_COMMISSIONING`.

## Grundsaetze

- Die Regelung muss sowohl heizen als auch kuehlen koennen.
- Ein neutraler Bereich verhindert unnoetiges Umschalten nahe am Sollwert.
- Das Peltier wird nicht mit hochfrequentem direktem PWM betrieben.
- Leistungsdosierung erfolgt zeitproportional in ausreichend langen
  Schaltfenstern.
- Vor jedem Polaritaetswechsel wird das Peltier sicher ausgeschaltet und die
  vorgeschriebene Totzeit eingehalten.
- Die Temperaturregelung ist von Weboberflaeche, WLAN und Netzwerkzeit
  unabhaengig.
- Sicherheitsgrenzen und Sensorplausibilitaet haben immer Vorrang vor der
  normalen Regelanforderung.
- Alle Regelstrategien muessen ohne PSRAM und innerhalb des festgelegten
  4-MB-Flashbudgets funktionieren.

## Regelverfahren des ersten Releases

### Zeitproportionale PI-Regelung

Das erste Release verwendet eine zeitproportionale PI-Regelung mit:

- proportionalem Anteil
- integralem Anteil
- neutralem Bereich um den Sollwert
- begrenztem Reglerausgang
- Anti-Windup beziehungsweise einer vergleichbaren Begrenzung des
  Integralanteils
- getrennten Freigabe- und Begrenzungsregeln fuer Heizen und Kuehlen
- langen Schaltfenstern statt hochfrequentem Peltier-PWM

Der Regler erzeugt keine direkten GPIO-Pegel, sondern eine abstrakte
Aktoranforderung, beispielsweise:

```text
HEAT mit geforderter Zeitquote
OFF
COOL mit geforderter Zeitquote
```

Eine nachgelagerte Aktorlogik prueft Totzeiten, Mindestlaufzeiten,
Richtungswechsel, Sensorstatus und Sicherheitsgrenzen, bevor die H-Bruecke
angesteuert wird.

### Implementierter Issue-22-Fachkern

Der native Issue-22-Kern verwendet vier getrennte Parametersaetze fuer
`Air/Heating`, `Air/Cooling`, `Product/Heating` und `Product/Cooling`. Jeder
Satz validiert endliche, positive `Kp`-/`Ki`-Werte, eine positive einseitige
Neutralbandschwelle und eine Quote in `(0, 1]`; die konkrete Auswahl bleibt
`TBD_COMMISSIONING`. Die gemeinsame `maximumIntegrationStepMillis`-Grenze
wird vor der Richtungs- und Profilwahl geprueft.

Die aktive Richtung und Quote werden checked berechnet:

```text
rawError = target - measured
activeError = abs(rawError) - directionNeutralBand
rawP = Kp * activeError
I <= max(0, maximumQuote - rawP)
quote = min(rawP + I, maximumQuote)
```

`rawP` wird nicht vorab auf `maximumQuote` begrenzt. Ein endlicher `rawP` ab
`maximumQuote` ist `Saturated`; der Integralanteil bleibt dann null. Jede
gueltige Anforderung, einschliesslich `OFF`, traegt eine fluechtige Sequenz
und den monotonen Erzeugungszeitpunkt. Sequenz, Integral, Timestamp und
Feedbackfenster werden nicht persistiert.

Im Produktbetrieb muessen Produkt- und Luftsnapshot gleichzeitig verwendbar
sein. Ein fehlender, `STALE`- oder `FAILED`-Luftsnapshot fuehrt fail-closed zu
`Unavailable / SensorUnavailable`; ein vorhandener, aber nicht-finiter oder
strukturell ungueltiger Wert zu `InvalidInput / InvalidSample`. Im Luftbetrieb
ist ein Produktwert nicht erforderlich und `AirLimitState` ist `NotApplied`.
Die normale Produkt-Luftbegrenzung ist `Unrestricted`, `Reduced` oder
`Blocked`; sie ist keine Safety-Grenze und verwendet keine Safety-
Fehlerursache. `#24` bleibt fuer Safety und Aktorfreigabe zustaendig.

Der Kern liefert `HEAT`, `OFF` oder `COOL` als `ControlRequest` mit
`processTransitionSequence`, `runRevision` und `ControlSensorRole`. Ein
nachgelagerter Aktorplaner muss diese Identitaet und sein eigenes Watchdog-
Zeitfenster pruefen. Das PI-Feedback nennt nur
`NoIntegratorConstraint`, `DeferredOrLimited` oder `Rejected`; der Kern
behauptet keine physische Aktorquote.

### Warum im ersten Release kein vollstaendiger PID mit Autotuning

Ein vollstaendiger PID-Regler mit automatischer Einmessung bleibt technisch
moeglich, ist aber nicht Bestandteil des ersten Releases.

Der Grund ist nicht primaer Speicherplatz, sondern Validierung und
Inbetriebnahme:

- Der thermische Prozess ist langsam und besitzt unterschiedliche Traegheiten
  fuer Luft, Gefaess und Produkt.
- Der D-Anteil kann auf Messrauschen und Sensorquantisierung empfindlich reagieren.
- Automatisches Einmessen erfordert absichtliche Temperaturbewegungen und muss
  fuer Heizen und Kuehlen sicher begrenzt werden.
- Die Box, Produktmenge, Umgebungstemperatur und Luftfuehrung beeinflussen das
  Ergebnis stark.
- Ein unzureichend validiertes Autotuning darf keinen Lebensmittelprozess
  unkontrolliert ueber- oder unterschwingen lassen.

Die Softwarearchitektur trennt deshalb Regelstrategie, Sensoraufbereitung und
Aktorlogik. Eine spaetere Strategie wie `pid_autotuned` darf ergaenzt werden,
ohne Zustandsmaschine, Programme oder Sicherheitslogik neu zu entwerfen.

## Heizen, Neutralbereich und Kuehlen

Während temperaturgeregelter Phasen sind drei normale Anforderungen erlaubt:

```text
deutlich zu kalt  -> HEAT
nahe am Sollwert  -> OFF
deutlich zu warm  -> COOL
```

Dies gilt insbesondere auch waehrend der Fermentation. Das ist erforderlich,
weil die Box im Sommer tagsueber kuehlen und nachts heizen muessen kann.

Verbindliche Regeln:

- Heizen und Kuehlen duerfen niemals gleichzeitig angefordert oder angesteuert
  werden.
- Der Wechsel von Heizen zu Kuehlen oder umgekehrt erfolgt nicht direkt beim
  kleinsten Vorzeichenwechsel.
- Eine ausreichend grosse Umschaltschwelle beziehungsweise Richtungswechsel-
  Hysterese verhindert Pendeln.
- Vor einer Polaritaetsumkehr wird die Leistung deaktiviert.
- Die firmwarefeste Mindesttotzeit darf durch keine Einstellung unterschritten
  werden.
- Die konkrete Totzeit, das Schaltfenster, Mindest-Ein-/Auszeiten und die
  Umschaltschwellen werden in Phase 7B festgelegt beziehungsweise praktisch
  ermittelt.

## Sensorrollen im produktgefuehrten Betrieb

### Regelsensor

Ist ein gueltiger Produktfuehler ausgewaehlt, ist er der primaere Regelsensor fuer
Sollwert, Fortschritt und Zielqualifikation.

### Schrankluftfuehler als Begrenzungs- und Sicherheitssensor

Der feste Schrankluftfuehler besitzt zwei getrennte Aufgaben:

1. **Fruehe Regelbegrenzung**
   - verhindert bereits vor einer absoluten Sicherheitsgrenze unnoetig heisse
     oder kalte Schrankluft
   - darf die angeforderte Heiz- oder Kuehlleistung reduzieren oder voruebergehend
     sperren
   - ist ein normaler Teil der Produktregelung und keine Fehlerabschaltung

2. **Absolute Sicherheitsueberwachung**
   - verwendet firmwarefeste Grenzen
   - sperrt bei gefaehrlicher oder unplausibler Lufttemperatur zwingend die
     betroffene Aktorrichtung beziehungsweise den gesamten Peltierbetrieb
   - wird in `SAFETY_AND_FAULTS.md` genauer spezifiziert

Damit regelt der Produktfuehler den Prozess, waehrend der Luftfuehler verhindert,
dass die traegere Produkttemperatur zu extremen Lufttemperaturen fuehrt.

## Vorgemerkte spaetere Kaskadenregelung

Eine vollstaendige Kaskadenregelung ist als spaetere Erweiterung vorgemerkt:

```text
äusserer Produktregler
  -> erzeugt begrenzten dynamischen Luftsollwert
  -> innerer Luftregler regelt die Schrankluft
  -> Aktorlogik steuert das Peltier
```

Sie kann die Produkttemperatur schneller und mit geringerem Ueberschwingen
fuehren, benoetigt aber zwei abgestimmte Regelkreise und eine saubere thermische
Charakterisierung.

Entscheidung fuer das erste Release:

- aktive Strategie: `product_primary_with_air_limits`
- vorgemerkte spaetere Strategie: `cascade_product_air`
- keine vollstaendige Kaskadenregelung im ersten Release
- Datenmodell, Diagnose und Modulgrenzen werden so angelegt, dass die spaetere
  Strategie ergaenzt werden kann
- Programme duerfen im ersten Release keine nicht implementierte
  Kaskadenstrategie auswaehlen

Der Grund fuer die Verschiebung ist Inbetriebnahme- und Testaufwand, nicht ein
wesentlicher Flash- oder RAM-Verbrauch.

## Luftgefuehrter Betrieb

Ohne ausgewaehlten gueltigen Produktfuehler ist der feste Schrankluftfuehler der
primaere Regelsensor.

In diesem Modus:

- regelt die PI-Regelung direkt auf die Schranklufttemperatur
- wird keine nicht gemessene Produkttemperatur behauptet
- bleibt der Produktfuehler optional als reine Anzeige oder Plausibilitaetsquelle
  nutzbar, sofern angeschlossen und im Programm erlaubt
- gelten weiterhin absolute Lufttemperatur- und Aktorsicherheitsgrenzen

## Regelsensor beim Kuehlen und Halten

Der fuer den Lauf wirksame primaere Regelsensor wird beim anschliessenden Kuehlen
und Halten standardmaessig weiterverwendet:

- produktgefuehrter Lauf -> Produktfuehler bleibt primaer
- luftgefuehrter Lauf -> Schrankluftfuehler bleibt primaer

Der Schrankluftfuehler bleibt in beiden Faellen Begrenzungs- und
Sicherheitssensor.

Ein laufender Wechsel des primaeren Sensors erfolgt nur gemaess der
spezifizierten Sensorfehler- und Rueckkehrregeln. Der Uebergang in die Kuehlphase
allein ist kein Grund fuer einen stillen Sensorwechsel.

## Innenluefter

Der Innenluefter laeuft waehrend aller temperaturgeregelten Phasen dauerhaft,
insbesondere:

- Vorheizen
- Halten der Vorheiztemperatur waehrend `WAITING_FOR_PRODUCT`, sofern die
  Vorheiztemperatur weiterhin geregelt wird
- Zielerreichung
- Zielqualifikation
- Fermentation
- Kuehlen
- Kuehlhalten
- manuelles Temperaturhalten
- Peltier-Totzeit innerhalb einer temperaturgeregelten Phase

Im normalen Standby ohne Temperaturregelung ist der Innenluefter aus.

Ein Fehlerzustand kann ein abweichendes Luefterverhalten verlangen; dies wird in
`SAFETY_AND_FAULTS.md` festgelegt. Die konkrete Nachlaufzeit wird in Phase 7B
behandelt.

## Zielqualifikation und kurze Abweichungen

Die Zielqualifikation verlangt, dass der primaere Regelsensor fuer eine
definierte Zeit ausreichend im Zielband liegt.

Kurze Abweichungen duerfen innerhalb einer konfigurierten Gnadenzeit toleriert
werden. Dauert die Abweichung laenger als diese Gnadenzeit, beginnt die
Zielqualifikation wieder bei null.

Verbindliche Regeln:

- Ein einzelner Messausreisser setzt die Qualifikation nicht zwingend sofort
  zurueck.
- Sensorfehler oder ungueltige Messwerte gelten nicht als erfolgreicher Aufenthalt
  im Zielband.
- Eine Gnadenzeit darf nicht dazu fuehren, dass ueberwiegend ausserhalb des
  Zielbands liegende Temperaturen qualifiziert werden.
- `bandCelsius` ist eine einseitige Toleranz/Halbbreite; die Grenze ist
  inklusiv: `abs(measured - target) <= bandCelsius`. Der bestehende
  Wertebereich bleibt unveraendert.
- Der Evaluator unterscheidet `Unavailable`, `Invalid`, `OutsideBand`,
  `Grace`, `InBand` und `Complete`. Unavailable, Invalid, rueckwaerts laufende
  Zeit, eine zu grosse Luecke und ein abgelaufenes Grace-Fenster unterbrechen
  die aktuelle Episode und uebertragen keine Zeit.
- `Grace` mit `outsideElapsed == effectiveGraceMillis` ist bereits abgelaufen;
  eine direkte InBand-Rueckkehr vor dieser Grenze erhaelt den alten Kredit,
  schreibt aber keine Rueckkehrzeit gut.
- Effektive Gnaden- und Sample-Gap-Werte sowie die konkreten
  Qualifikationswerte bleiben `TBD_COMMISSIONING` beziehungsweise Eigentum
  des freigegebenen Sampling-/Commissioning-Vertrags.
- Der Start der Fermentationszeit erfolgt erst nach vollstaendig erfolgreicher
  Zielqualifikation.

## Rueckkehr des Produktfuehlers nach Ersatzbetrieb

Das Verhalten ist pro Programm konfigurierbar.

Zulaessige Strategien:

- `remain_on_air_until_end`
- `manual_return_to_product`
- `automatic_validated_return_to_product`

Werkseinstellung fuer produktgefuehrte Standardprogramme:

```text
automatic_validated_return_to_product
```

Vor einem automatischen Rueckwechsel muss der Produktfuehler:

- wiederholt gueltige Messungen liefern
- CRC- und Buspruefungen bestehen
- innerhalb plausibler Temperatur- und Aenderungsgrenzen liegen
- fuer eine definierte Stabilitaetszeit gueltig bleiben
- mit dem aktuellen Prozesszustand vereinbar sein

Der Rueckwechsel:

- erfolgt nicht nach einem einzelnen gueltigen Messwert
- wird sichtbar angezeigt
- erzeugt einen Protokolleintrag
- wird als neue Laufrevision gespeichert
- kann quittiert werden, blockiert den laufenden Prozess aber nicht
- darf bei unplausibler Differenz zwischen Produkt und Luft zunaechst ausgesetzt
  werden

Stabilitaetszeit und Plausibilitaetsgrenzen bleiben `TBD_COMMISSIONING`.

## Aenderung von Laufwerten waehrend eines aktiven Prozesses

Zieltemperatur und verbleibende Dauer duerfen ueber eine ausdrueckliche
Laufaktion innerhalb sicherer Grenzen geaendert werden.

Vorgesehener Ablauf:

```text
Aktuellen Lauf bearbeiten
  -> neuen Zielwert beziehungsweise neue Restdauer eingeben
  -> alte und neue Werte zusammenfassen
  -> Auswirkungen und Grenzen pruefen
  -> ausdruecklich bestaetigen
  -> neue Laufrevision atomar speichern
  -> Regelung mit den bestaetigten effektiven Laufwerten fortsetzen
```

Verbindliche Regeln:

- Das gespeicherte Quellprogramm wird nicht automatisch veraendert.
- Der urspruengliche Programmschnappschuss bleibt nachvollziehbar.
- Die Aenderung wird als append-only Laufanpassung beziehungsweise neue
  Laufrevision gespeichert.
- Aenderungszeitpunkt, Quelle, alter Wert, neuer Wert und Begruendungscode werden
  protokolliert.
- Die neue Zieltemperatur muss innerhalb der programmseitigen und firmwarefesten
  Grenzen liegen.
- Die Restdauer darf nicht negativ werden.
- Eine Verlaengerung oder Verkuerzung darf bereits abgeschlossene Phasen nicht
  rueckwirkend umdeuten.
- Eine Aenderung waehrend Vorheizen oder Zielqualifikation wird phasenbezogen
  validiert und kann eine erneute Zielqualifikation erfordern.
- Sicherheitslogik darf eine Aenderung ablehnen.
- Die genaue Benutzeroberflaeche verwendet Zusammenfassung und Bestaetigung vor
  der Aktivierung.

Weitere direkte Laufparameter werden nicht still ueber normale
Einstellungsseiten veraendert.

## Architekturgrenzen

Die Implementierung trennt mindestens:

1. Sensorerfassung und Plausibilisierung
2. Auswahl des primaeren Regelsensors
3. Regelstrategie
4. Luftbegrenzung
5. Sicherheitsfreigabe
6. Aktoranforderung
7. H-Bruecken- und Luefterlogik
8. Persistenz und Diagnose

Dadurch kann eine spaetere Kaskaden- oder PID-Strategie ergaenzt werden, ohne
Sicherheitslogik oder direkte Aktoransteuerung in den Regler selbst zu
vermischen.

## Akzeptierte Entscheidungen aus Phase 7A

- [x] zeitproportionale PI-Regelung im ersten Release
- [x] vollstaendiger PID mit Autotuning als spaetere Erweiterung vorbereitet
- [x] Heizen, neutraler Bereich und Kuehlen auch waehrend der Fermentation
- [x] Richtungswechsel nur mit Hysterese, Abschaltung und Totzeit
- [x] Produktfuehler primaer, Schrankluftfuehler als fruehe Begrenzung und
      absolute Sicherheit
- [x] vollstaendige Kaskadenregelung `cascade_product_air` vorgemerkt, aber nicht
      Bestandteil des ersten Releases
- [x] primaeren Regelsensor beim Kuehlen und Halten weiterverwenden
- [x] Innenluefter waehrend aller temperaturgeregelten Phasen und Totzeiten aktiv
- [x] kurze Zielbandabweichungen innerhalb einer Gnadenzeit tolerieren
- [x] Rueckkehrstrategie des Produktfuehlers pro Programm konfigurierbar
- [x] bei produktgefuehrten Standardprogrammen validierter automatischer
      Rueckwechsel
- [x] Zieltemperatur und verbleibende Dauer als ausdrueckliche protokollierte
      Laufanpassung aenderbar

## Noch offen fuer Phase 7B und 7C

- Schaltfenster fuer zeitproportionale Ansteuerung
- Mindest-Ein- und Mindest-Auszeit des Peltiers
- konkrete Polaritaetswechsel-Totzeit und Umschalthysterese
- Verhalten bei sehr kleinen Reglerausgaengen
- konkrete Produktionswahl der validierten Anti-Windup-/Integral-
  Transition-Policy je Phase
- PI-Parameter fuer Luft- und Produktbetrieb
- fruehe obere und untere Luftbegrenzungen
- absolute Temperatur-Sicherheitsgrenzen
- Innen- und Aussenluefter-Nachlaufzeiten
- Verhalten der Luefter bei Fehlern
- Filterung, Abtastrate und Plausibilisierung der Sensorwerte
- konkrete Zielband-, Qualifikationsdauer- und Gnadenwerte
- validierte Stabilitaetszeit fuer Sensor-Rueckwechsel
- Inbetriebnahme-, Sprungantwort- und Tuningverfahren
- Kriterien fuer eine spaetere Freigabe von `cascade_product_air`
- Kriterien fuer eine spaetere PID- oder Autotuning-Funktion
