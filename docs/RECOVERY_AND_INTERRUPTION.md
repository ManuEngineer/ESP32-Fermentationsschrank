# Unterbrechungen und Wiederanlauf

## Status

Dieses Dokument definiert die Release-1-Regeln fuer Stromunterbrechungen,
Sensorersatz, phasenbezogenen Wiederanlauf und Zeitbewertung. Es ergaenzt
[`STATE_MACHINE.md`](STATE_MACHINE.md) und
[`SYSTEM_SAFETY_AND_RECOVERY.md`](SYSTEM_SAFETY_AND_RECOVERY.md).
Die persistierten Felder, die atomare Buchung und die Anzeigeprojektion sind
hier und in [`RUN_PERSISTENCE.md`](RUN_PERSISTENCE.md) konsistent beschrieben;
es gibt keine parallele Statusquelle.

## Grundsatz: autonom, aber nicht blind

Ein unterbrochener Lauf soll nicht unnoetig auf Benutzer oder Netzwerk warten.
Automatischer Wiederanlauf bedeutet jedoch nicht, unbekannte Ausfallzeit,
Temperaturverlauf oder biologischen Fortschritt zu erfinden.

Deshalb gilt:

- Jeder Wiederanlauf beginnt mit ausgeschalteten Aktoren.
- Bootschleifen, persistierte Verriegelungen, Speicherintegritaet und Sensoren
  werden vor jeder Aktorfreigabe geprueft.
- Der letzte elektrische Aktorzustand wird nie wiederhergestellt.
- Eine neue phasenbezogene Aktion wird aus validierten fachlichen Daten abgeleitet.
- Sichere aktuelle Temperaturregelung darf vor verfuegbarer NTP-Zeit wieder
  beginnen, sofern Phase und Freigaben eindeutig sind.
- Unbekannter Unterbrechung wird kein frei geschaetzter exakter Fortschritt
  gutgeschrieben.
- Ein Sicherheitsfehler oder eine nicht rekonstruierbare Phase stoppt die
  automatische Fortsetzung.
- Die Recoveryentscheidung wird persistiert, bevor Aktoren freigegeben werden.

## Kein Tuerkontakt in Release 1

Die Software besitzt keine direkte Tuerinformation.

Folgen:

- keine tuergesteuerte Peltierabschaltung
- kein tuergesteuerter Timerstopp
- Temperaturabweichungen durch eine offene Tuer werden wie andere Abweichungen
  behandelt
- UI und Diagnose duerfen nicht behaupten, die Tuerstellung zu kennen

Eine spaetere Erweiterung darf `door_open` und `door_closed` als optionale
Ereignisse einfuehren. Ohne bestaetigte Hardware bleibt sie wirkungslos.

## Ausfall oder Entfernen des Produktfuehlers

Ein produktgefuehrter Lauf wechselt nie still auf den Luftfuehler.

### Sofortreaktion

1. Peltier vorlaeufig AUS.
2. Schrankluftfuehler und alle Sicherheitsbedingungen pruefen.
3. Sensorfehler sichtbar und akustisch melden.
4. Zeitpunkt, letzte gueltige Werte und Prozessphase protokollieren.
5. Programmspezifische Ausfallstrategie anwenden.

### Strategien

```text
fallback_to_air_after_timeout
wait_for_user
stop_to_safe_state
```

Standard ist `fallback_to_air_after_timeout`.

Dabei gilt:

- Der Benutzer kann sofort mit Luftregelung fortsetzen, ausschalten oder einen
  separaten manuellen Kuehllauf starten, soweit sicher zulaessig.
- Nach der konfigurierten Wartezeit darf automatisch auf Luftregelung gewechselt
  werden, wenn der Luftfuehler gueltig ist und der Wechsel vollstaendig validiert
  wurde.
- Wechsel, Zeitpunkt, Werte und reduzierte Prozesskonfidenz werden protokolliert.
- Der aktive Programmschnappschuss bleibt bestehen; der Regelmoduswechsel wird als
  Laufrevision gespeichert.
- Ist der Luftfuehler ungueltig oder die Fortsetzung nicht sicher, erfolgt kein
  automatischer Wechsel.

Die Wartezeit bleibt `TBD_COMMISSIONING`.

Die vollstaendige Auswahl-, Ersatzbetriebs- und Rueckkehrlogik (alle drei
Strategien, gleichzeitiger Schrankluft-/Kuehlkoerperausfall, manuelle
Aktionen) ist Issue #21. Was davon nach einem Neustart bereits vorliegt und
was fuer die tatsaechliche Reaktivierung eines geladenen aktiven Laufs noch
fehlt, steht in `docs/RUN_PERSISTENCE.md`, Abschnitt "Uebergabe an ein
spaeteres Vorhaben: Regelsensorauswahl bei Reaktivierung".

## Maximale Wartezeit nach dem Vorheizen

`WAITING_FOR_PRODUCT` besitzt eine programmspezifische Maximalzeit.

- Beim Eintritt wird sichtbar und akustisch zum Einsetzen des Produkts
  aufgefordert. Diese Meldung ist zugleich die Warnung vor Ablauf; eine zweite
  Warnschwelle ist fuer Release 1 nicht vorgesehen.
- Das Produkt wird niemals automatisch als eingesetzt angenommen.
- Ist die Maximalzeit belastbar abgelaufen, endet das Vorheizen sicher und der
  Lauf wird als nicht gestartet beziehungsweise abgebrochen protokolliert.
- Ist nach einem Neustart unklar, ob die Maximalzeit abgelaufen ist, darf keine
  Fermentation automatisch starten. Die Regelung bleibt nur in einer eindeutig
  sicheren Warteaktion oder wird beendet.

## Fehlerquittierung und Fortsetzung

Quittierung bedeutet nur Kenntnisnahme. Eine Fortsetzung richtet sich nach
Fehlerklasse, beseitigter Ursache, erneuter Validierung und gegebenenfalls
bewusstem Fehlerreset.

Laufbeendende Beispiele:

- harte Ueber- oder Untertemperatur
- widerspruechliche H-Bruecken-Anforderung
- nicht nachweisbarer sicherer Ausgangszustand
- unbrauchbare Persistenz oder unvollstaendige Transaktion
- kritischer interner Softwarefehler

Ein Neustart setzt solche Fehler nicht zurueck.

## Phasenbezogene Wiederaufnahme

### PREHEATING und REACHING_TARGET

- Sensoren, Sicherheitsfreigaben und Zeitgrenzen pruefen
- Zieltemperatur erneut anfahren
- noch nicht gestartete Fermentationszeit nicht anrechnen

### WAITING_FOR_PRODUCT

- nur innerhalb einer belastbar gueltigen Wartezeit fortsetzen
- Produkt niemals automatisch als eingesetzt annehmen
- bei nicht sicher entscheidbarer Wartezeit keine Fermentation starten

### QUALIFYING_TARGET

- Zieltemperatur erneut erreichen
- Zielqualifikation vollständig neu beginnen

### FERMENTING

- sichere Temperaturregelung nach vollstaendiger Recoverypruefung neu ableiten
- bei fehlender Zeit `RECOVERY_TIME_PENDING` setzen
- unbekannter Unterbrechung keinen exakten Fortschritt gutschreiben
- keinen automatischen Abschluss aus einem einzelnen Schaetzwert ableiten
- spaetere Zeit- und Temperaturbewertung als Laufrevision nachtragen

### COOLING

- bei gueltigen Pflichtsensoren und ohne Sperre Kuehlung erneut ableiten
- keine alte BTS7960-Richtung blind wiederherstellen

### COOL_HOLDING

- sichere Kuehlregelung erneut ableiten
- zeitlich begrenztes Halten nur aus belastbarer Zeitbewertung beenden
- bei ueberlappender Unsicherheit keinen automatischen Abschluss ausloesen

### MANUAL_HOLDING

- Zieltemperatur nach vollstaendiger Recoverypruefung weiter halten
- Benutzer sichtbar ueber die Unterbrechung informieren

### COMPLETED

- keine Temperaturregelung neu starten
- `COMPLETED` und Ergebnisdaten wiederherstellen
- erst Benutzerquittierung fuehrt nach `STANDBY`

## Fehlende Messwerte waehrend des Stromausfalls

ESP32 und Sensoren liefern waehrend eines vollstaendigen Stromausfalls keine
Messkurve. Verfuegbar sind nur:

- letzter gueltiger Temperaturwert vor dem Ausfall
- Zeit und Sequenz des letzten gueltigen Kontrollpunkts
- erster gueltiger Temperaturwert nach dem Neustart
- programmspezifische Phase und Sollwerte
- spaeter eventuell eine vertrauenswuerdige aktuelle UTC-Zeit
- ein erst nach realer Vermessung freigegebenes thermisches Modell

Die Firmware darf keine dazwischenliegenden Messwerte vortaeuschen.

## Wiederanlauf vor NTP

Vorgesehener Ablauf:

```text
BOOT
  -> Sicherheits-, Persistenz- und Laufpruefung
  -> RECOVERY_EVALUATION
  -> sichere phasenbezogene Aktion bestimmen
  -> Recoveryentscheidung speichern
  -> Regelung freigeben, falls eindeutig zulaessig
  -> RECOVERY_TIME_PENDING setzen
  -> Netzwerk und NTP parallel aufbauen
```

Bis zur Zeitsynchronisation:

- keine scheinbar exakte Unterbrechungs- oder Restzeit anzeigen
- keinen unbekannten Fortschritt frei schaetzen
- keinen automatischen Phasenabschluss allein aus der unbekannten Zeit ausloesen
- aktuelle sichere Regelung fortsetzen, wenn Phase und Freigaben eindeutig sind
- bei nicht eindeutiger Aktion Peltier AUS und Meldung beziehungsweise Fehler

Fehlende NTP-Zeit allein beendet den Lauf nicht.

## Unterbrechungsdauer ist ein Intervall

Nach NTP gilt nicht automatisch:

```text
Ausfalldauer = aktuelle UTC - letzter Kontrollpunkt
```

Diese Differenz enthaelt auch die Zeit, in der das Geraet nach dem letzten
Kontrollpunkt noch lief.

Mindestens zu berechnen sind:

```text
obere Grenze = aktuelle UTC - letzter verlaesslicher UTC-Kontrollpunkt
untere Grenze = max(0, obere Grenze - maximaler moeglicher Abstand bis zum
                 naechsten planmaessigen oder ereignisbezogenen Kontrollpunkt)
```

Der maximale Abstand beruecksichtigt:

- konfiguriertes Kontrollpunktintervall
- den letzten bekannten monotonen Laufzeitstand
- ausgefallene oder verspaetete Kontrollpunkte
- Zeitqualitaet vor und nach der Unterbrechung
- letzte ereignisbezogene Speicherung

Je groesser oder schlechter bestimmt das Intervall, desto niedriger die
Konfidenz.

## Entscheidung mit dem Zeitintervall

### Eindeutiges Ergebnis

Fuehren Unter- und Obergrenze zur gleichen Phasen- und Fortschrittsentscheidung:

- darf die konservativere Variante automatisch angewendet werden
- werden beide Grenzen und die angewandte Korrektur protokolliert
- laeuft der Prozess ohne Benutzerbestaetigung weiter

### Intervall ueber einer Phasengrenze

Ueberschneidet das Intervall einen moeglichen Abschluss, eine Haltezeitgrenze oder
eine festgelegte Unsicherheitsgrenze:

- erfolgt kein automatischer Abschluss aufgrund der Zeit
- bleibt die sichere aktuelle Regelaktion erhalten, sofern sie eindeutig ist
- `WARNING_REQUIRES_ACTION` wird sichtbar
- der Benutzer kann Fortschritt beziehungsweise Abschluss innerhalb der
  erlaubten Grenzen bestaetigen oder anpassen

Damit wartet die Regelung nicht blockierend, waehrend die Firmware trotzdem
keinen fachlichen Abschluss erfindet.

## Temperaturgewichteter Fortschritt

Eine Fermentation stoppt bei sinkender Temperatur nicht schlagartig. Weder volle
Anrechnung noch vollstaendiges Pausieren der Ausfallzeit ist allgemein korrekt.

Konzeptionell:

```text
wirksamer Fortschritt = Summe aus Zeitabschnitten * Aktivitaetsfaktor(Temperatur)
```

Verbindliche Grenzen:

- Keine biologische Aktivitaetskurve wird ohne praktische Grundlage erfunden.
- Produktwerte haben Vorrang, wenn ein gueltiger Produktfuehler vorhanden ist.
- Schrankluftwerte besitzen geringere Konfidenz.
- Fehlende Messzeit wird nur mit einem waehrend der Inbetriebnahme validierten
  thermischen Modell bewertet.
- Ohne freigegebenes Modell wird das Zeitintervall konservativ behandelt und
  nicht durch eine scheinbar genaue Kurve ersetzt.

Die Produktionsgrenze ist `RecoveryProgressWeightingModel`. Gate C liefert in
Release 1 mit `UnavailableRecoveryProgressWeightingModel` stets `unavailable`,
weil kein Commissioning-Modell freigegeben ist. Ein Provider darf nur aus
`FERMENTING`, bekannten Ausfallgrenzen sowie gueltiger, gefilterter Vor-/
Nach-Ausfall-Evidenz einen Beitrag liefern. Produkt ist
`ProductPreferred`, Luft ist nur ausdruecklich und mit `AirReduced` zulaessig;
eine ungueltige Kuehlkoerper- oder Stale-Evidenz wird nicht umgedeutet.

Die Buchung bleibt separat von `observedRunSeconds` und nominaler Zeitkorrektur.
Sie verlangt Lauf-/Episodenrevision, eine passende nicht-null Segmentkennung,
aktuelle Sensorberechtigung, nicht fallende checked Bounds und eine positive
Modellrevision. Ein Segment ist idempotent; die atomare Persistenz erfolgt vor
der RAM-Anwendung. Ein abgeloestes, noch nicht gebuchtes Segment setzt
`PartialUnknown` und entfernt die Gesamt-Obergrenze. Unsichere Ausfallzeit
wird nicht automatisch als biologischer Fortschritt gutgeschrieben.

## Anzeige und Export

Die Recoveryanzeige zeigt mindestens:

- letzte gueltige Temperatur vor dem Ausfall
- erste gueltige Temperatur nach dem Neustart
- verwendete Sensorrolle
- Unter- und Obergrenze der moeglichen Ausfalldauer
- Alter und Intervall des letzten Kontrollpunkts
- Zeitqualitaet und Konfidenz
- automatisch gewaehlte sichere Wiederanlaufaktion
- angewandte oder noch ausstehende Fortschrittskorrektur
- Grund einer erforderlichen Benutzerentscheidung

Fuer die implementierte Anzeige-/Exportprojektion bleiben ausserdem getrennt
sichtbar: beobachtete Laufzeit, kumulative nominale Korrektur, gewichtete
Coverage (`Complete` oder `PartialUnknown`), Bounds, letzte Sensorrolle,
Vertrauensstufe, Modellrevision und zuletzt gebuchte Segmentkennung. Bei
`unavailable`, `not eligible`, `Stale`, `AlreadyProcessed` oder
`PartialUnknown` wird kein erfundener Einzelwert als exakter biologischer
Fortschritt ausgegeben; die Rohgrenzen und der Status werden exportiert.

## Spaetere RTC-Option

Eine batteriegepufferte RTC kann spaeter hinter derselben Zeitquellenschnittstelle
ergaenzt werden. Sie ist keine Voraussetzung fuer Release 1.

## Akzeptierte Entscheidungen

- [x] kein Tuerkontakt in Release 1
- [x] kein stiller Wechsel vom Produkt- zum Luftfuehler
- [x] Produktfuehlerstrategie pro Programm
- [x] maximale Wartezeit in `WAITING_FOR_PRODUCT`
- [x] phasenbezogene Recovery statt Wiederherstellung elektrischer Ausgaenge
- [x] sichere Recovery beginnt vor verfuegbarer NTP-Zeit
- [x] fehlende NTP-Zeit allein beendet den Lauf nicht
- [x] unbekannte Zeit erzeugt keinen erfundenen Fortschritt
- [x] NTP-Differenz zum Kontrollpunkt wird als Intervall, nicht als exakte
      Ausfalldauer behandelt
- [x] kein automatischer Phasenabschluss bei ueberlappendem Unsicherheitsintervall
- [x] spaetere RTC bleibt moeglich
- [x] Recovery-Zeitanker und nominale Korrektur bleiben von beobachteter Laufzeit
      und gewichteter Fortschrittsbasis getrennt
- [x] kein gewichteter Produktionsfortschritt ohne freigegebenes
      Commissioning-Modell
- [x] Recovery-/Progress-Buchungen sind revisioniert, bounds-geprueft,
      idempotent und Write-before-Apply

## Noch durch Inbetriebnahme festzulegen

- Wartezeit bis zum Produktfuehler-Fallback
- Kontrollpunktintervall innerhalb der spezifizierten Grenzen
- maximale automatisch akzeptierte Zeitunsicherheit
- thermisches Abkuehlmodell und dessen Gueltigkeitsbereich
- programmbezogene Grenzen fuer Fortschrittskorrekturen
