# Verbindliche Korrekturen aus den Reviews von PR #38

## Status und Prioritaet

Dieses Dokument verarbeitet die beiden Reviews von PR #38. Die hier festgelegten
Regeln sind verbindliche Release-1-Spezifikation und ersetzen widersprechende
Aussagen in aelteren Abschnitten der thematischen Dokumente.

Die Korrekturen werden vor dem Merge soweit moeglich direkt in die betroffenen
Dokumente uebernommen. Wo historische Phasenabschnitte verbleiben, gilt dieses
Dokument zusammen mit `SPECIFICATION_REVIEW.md` als spaetere Klarstellung.

## 1. Verbindliche Boot-Reihenfolge

`BOOT` darf weder direkt nach `STANDBY` noch direkt in eine Lauf-Recovery wechseln,
bevor Sicherheits- und Persistenzinformationen ausgewertet wurden.

Verbindliche Reihenfolge:

```text
BOOT
  -> alle Aktoren und beide BTS7960-Richtungen sicher AUS
  -> Resetursache und abnormalen Neustartzaehler auswerten
  -> Konfiguration und kritischen Speicherbereich validieren
  -> persistierte Verriegelungen und unvollstaendige Transaktionen auswerten
  -> Sensoren und Hardwarefreigaben pruefen
  -> bei Bootschleife, Verriegelung oder Integritaetsfehler: SAFE_BOOT
  -> gueltiger persistierter Zustand COMPLETED: COMPLETED wieder anzeigen
  -> gueltiger unterbrochener aktiver Lauf: RECOVERY_EVALUATION
  -> sonst: STANDBY
```

Keine Aktorfreigabe ist vor Abschluss dieser Reihenfolge moeglich. Ein Neustart
loescht keine persistierte Verriegelung und gilt nicht als Fehlerreset.

## 2. Unbekannte Ausfalldauer ohne blindes Raten

Fehlende NTP-Zeit allein beendet einen Lauf nicht. Sie erlaubt aber auch keine
frei erfundene Ausfalldauer und keinen automatischen Phasenabschluss aufgrund
eines einzelnen Schaetzwerts.

Vor einer vertrauenswuerdigen absoluten Zeit gilt:

- Aktoren bleiben zunaechst AUS.
- Lauf, Phase, Sensoren, Verriegelungen und aktuelle Temperatur werden validiert.
- Eine fuer die aktuelle Phase sichere Temperaturregelung darf automatisch neu
  abgeleitet werden, wenn alle normalen Freigabebedingungen erfuellt sind.
- Der unbekannten Unterbrechung wird bis zur Klaerung kein frei geschaetzter
  Fermentationsfortschritt gutgeschrieben.
- Der Kontext `RECOVERY_TIME_PENDING` bleibt sichtbar und wird protokolliert.
- Die Firmware darf den Lauf nicht allein aufgrund unbekannter Zeit automatisch
  als abgeschlossen erklaeren.
- Kann die aktuelle Phase oder die sichere Regelaktion nicht eindeutig bestimmt
  werden, bleibt das Peltier AUS und es folgt `WARNING_REQUIRES_ACTION` oder eine
  passende Fehlerreaktion.

Damit bleibt die frueher akzeptierte nicht blockierende Recovery erhalten, ohne
biologischen Fortschritt oder Unterbrechungszeit zu erfinden.

## 3. Unterbrechungsdauer als Intervall

Nach spaeterem NTP-Abgleich ist

```text
aktuelle UTC - UTC des letzten Kontrollpunkts
```

nicht automatisch die Ausfalldauer. Der Stromausfall kann erst nach dem letzten
Kontrollpunkt eingetreten sein.

Die Recovery berechnet deshalb mindestens ein Intervall:

```text
obere Grenze = aktuelle UTC - letzter verlaesslicher UTC-Kontrollpunkt
untere Grenze = max(0, obere Grenze - maximal moeglicher Abstand bis zum
                 naechsten planmaessigen oder ereignisbezogenen Kontrollpunkt)
```

Ausgelassene Kontrollpunkte, unklare Schreibzeitpunkte oder eine bereits zuvor
unsichere Zeitquelle vergroessern dieses Intervall und senken die Konfidenz.

Regeln:

- Fuehren Unter- und Obergrenze zur gleichen Phasen- und Fortschrittsentscheidung,
  darf diese konservativ angewendet werden.
- Ueberschneidet das Intervall einen Phasenabschluss oder eine konfigurierte
  Unsicherheitsgrenze, erfolgt kein automatischer Abschluss aufgrund der Zeit.
- Die sichere aktuelle Temperaturregelung darf weiterlaufen, waehrend die UI eine
  Benutzerentscheidung oder spaetere Zeitkorrektur anfordert.
- Angezeigt und exportiert werden beide Grenzen, Kontrollpunktalter,
  Checkpointintervall und Konfidenz; kein scheinbar exakter Wert.

## 4. Aktortests niemals aus SAFE_BOOT

`SAFE_BOOT` erlaubt in Release 1 ausschliesslich:

- passive Diagnose
- Lesen und Exportieren von Berichten
- Netzwerkwiederherstellung ohne Aktorwirkung
- PIN-unabhaengigen lokalen Werksreset
- UART-Recovery beziehungsweise erneutes Flashen

Heiz-, Kuehl-, Luefter- und andere leistungsbezogene Aktortests sind aus
`SAFE_BOOT` gesperrt. Sie werden erst nach Beseitigung der Ursache, bestandener
Integritaetspruefung, bewusstem Fehlerreset und Rueckkehr in ein validiertes
`STANDBY` im PIN-geschuetzten Servicemodus freigegeben.

## 5. Voraussetzungen fuer den ersten Peltier-Puls

Vor jeder erstmaligen Bestromung des realen Peltiers muessen nachweislich vorhanden
und geprueft sein:

- geeignete 7,5-A-Ueberstromsicherung
- montierte einmalige Temperatursicherung als von der Firmware unabhaengige
  thermische Abschaltung
- dokumentierter Montageort und Durchgangspruefung der Temperatursicherung
- korrekt montierter Kuehlkoerper
- funktionsgepruefter Aussenluefter
- gueltiger Schrankluft- und Kuehlkoerpersensor
- verifizierte BTS7960-Pinbelegung, AUS-Pegel und Polaritaet
- strom- und zeitbegrenzter Servicepuls mit jederzeitiger Abschaltmoeglichkeit

Rating und genauer Montageort der Temperatursicherung bleiben
`TBD_COMMISSIONING`, ihre Installation vor dem ersten Peltier-Puls ist jedoch
kein optionaler Inbetriebnahmeschritt.

## 6. Kritische Persistenzfehler und Neustart

Ein Fehler beim Schreiben eines kritischen Lauf-, Sperr- oder Recoverydatensatzes
fuehrt sofort zu:

1. Sperren neuer Aktoranforderungen
2. Peltier AUS und erforderlichem Luefternachlauf
3. RAM-seitiger Verriegelung
4. Versuch, einen minimalen Fehler-Latch in einem reservierten, redundanten
   Speicherbereich zu setzen
5. Wechsel in einen verriegelten Fehlerzustand

Zusaetzlich gilt ein transaktionales Freigabeprinzip:

- Ein zustandsaendernder Schritt, der spaeter Aktoren freigeben kann, wird erst
  angewendet, nachdem Transaktionsabsicht und neue Revision erfolgreich
  persistiert wurden.
- Ein unvollstaendiger Transaktionsmarker fuehrt beim Boot zu `SAFE_BOOT`.
- Vor jeder Recovery-Aktorfreigabe muss der kritische Speicher eine
  Lesen-Schreiben-Integritaetspruefung bestehen und die Recoveryentscheidung als
  neue Revision erfolgreich gespeichert sein.
- Ein gesetzter Persistenzfehler-Latch darf nur im Serviceablauf nach bestandener
  Speicherpruefung zurueckgesetzt werden.

Der reservierte Latch liegt getrennt vom normalen Laufjournal, aber weiterhin im
selben physischen ESP32-Flash. Ein vollstaendiger physischer Flashdefekt kann ohne
unabhaengigen externen Speicher nicht redundant ueberlebt werden. Die Firmware
darf deshalb keine hoehere Ausfallsicherheit behaupten, als die Hardware bietet.
Ein aktuell nicht les- oder schreibbarer kritischer Speicher verhindert in jedem
Fall die Lauf-Recovery und Aktorfreigabe.

## 7. PIN-unabhaengiger lokaler Werksreset

Der vorgesehene Recoveryweg bei vergessener Service-PIN darf die vergessene PIN
nicht selbst voraussetzen.

Release-1-Anforderung:

- nur lokal bei physischer Anwesenheit
- nur waehrend Boot beziehungsweise `SAFE_BOOT`
- Peltier und alle leistungsbezogenen Aktoren bleiben AUS
- nicht ueber Web oder Netzwerk ausloesbar
- mehrstufige Warnung mit langer bewusster Bestaetigung
- Ausloesung ueber einen bei der Hardwareabnahme verifizierten physischen Weg,
  vorzugsweise rohe Touchgeste beim Boot; UART-Loeschen/Neu-Flashen bleibt letzter
  physischer Recoveryweg
- loescht Benutzerprogramme, Einstellungen, WLAN-Zugang, Webpasswort,
  Service-PIN und Historien
- stellt den Factory-Katalog wieder her
- behaelt die Touchkalibrierung, soweit der Resetpfad nicht gerade wegen
  unbrauchbarer Touchdaten eine separate Kalibrierungswiederherstellung verlangt

Die konkrete Geste beziehungsweise Taste bleibt bis zur Hardwareverifikation
`TBD_HARDWARE`; das Vorhandensein eines PIN-unabhaengigen Wegs ist verbindlich.

## 8. Wiederherstellung von COMPLETED

Ein gueltig persistierter Zustand `COMPLETED` wird nach dem Boot wieder als
`COMPLETED` angezeigt. Es wird weder nach `STANDBY` weitergeschaltet noch eine
Regelphase neu gestartet. Erst die bewusste Benutzerquittierung fuehrt nach
`STANDBY`.

## 9. Terminologie und Dateinamen

Verbindliche Zustandsnamen stammen aus `STATE_MACHINE.md`, insbesondere:

```text
QUALIFYING_TARGET
COOL_HOLDING
RECOVERY_EVALUATION
SERVICE_MODE
```

Verbindliche Begriffe sind `Mindest-Einschaltzeit` und
`Mindest-Ausschaltzeit`.

Der kanonische Dateiname ist `ACTUATOR_TIMING.md`. Ein eventueller
Kompatibilitaetshinweis unter `ACTUATOR_TIMING_AND_FANS.md` darf nur auf das
kanonische Dokument verweisen und keine zweite Spezifikation enthalten.

## Abnahmeanforderungen

Die Implementierung muss die Korrekturen mindestens durch folgende Tests
nachweisen:

- persistierte Sicherheitsverriegelung plus Neustart fuehrt zu `SAFE_BOOT`
- Bootschleife kann weder `STANDBY` noch Recovery-Aktoren erreichen
- `COMPLETED` bleibt nach Neustart `COMPLETED`
- fehlende NTP-Zeit erzeugt keinen erfundenen exakten Fortschritt
- Ausfallzeit wird als Unter-/Obergrenze berechnet
- Unsicherheitsintervall ueber Phasengrenze verhindert automatischen Abschluss
- kein Aktortest ist aus `SAFE_BOOT` erreichbar
- Peltier-Test ohne bestaetigte Temperatursicherung wird blockiert
- kritischer Persistenzschreibfehler schaltet sicher ab
- unvollstaendige Persistenztransaktion fuehrt beim Boot zu `SAFE_BOOT`
- vergessene Service-PIN kann nur ueber den lokalen PIN-unabhaengigen
  Vollresetweg behandelt werden
