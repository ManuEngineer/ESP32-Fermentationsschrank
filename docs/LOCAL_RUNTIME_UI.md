# Lokale Laufanzeige und Meldungsablaeufe

## Status

Dieses Dokument ergaenzt [`LOCAL_UI.md`](LOCAL_UI.md) um die in Phase 4C
akzeptierten Bedienablaeufe waehrend eines laufenden oder abgeschlossenen
Prozesses.

## Detailansicht waehrend eines Laufes

Die Detailansicht besteht aus zwei festen Seiten. Dadurch bleiben die Inhalte
auf dem 320-x-240-Pixel-Display uebersichtlich und es entsteht keine lange,
unvorhersehbare Scrollseite.

### Seite 1: Prozess

Mindestens anzuzeigen sind:

- Programmname
- benutzerverstaendlicher Phasenname
- Solltemperatur
- Produkt- und Schranklufttemperatur
- verstrichene und verbleibende Zeit, soweit verlaesslich verfuegbar
- Vorheiz-, Fermentations-, Kuehl- oder Haltezustand
- Abschlussverhalten
- aktuell wirksame Laufzeitanpassung, beispielsweise nach einem Stromausfall

Beispiel:

```text
Joghurt mild
Fermentation laeuft

Soll:       42,0 °C
Produkt:    41,8 °C
Schrank:    42,3 °C
Restzeit:   06:14
Danach:     Kuehlen und halten

[Technik]          [Zurueck]
```

### Seite 2: Technik

Mindestens anzuzeigen sind:

- aktiver Regelmodus
- primaerer Regelsensor
- Status beider Temperatursensoren
- aktuelle Aktoranforderung: Heizen, Kuehlen, Halten oder AUS
- Peltier-Totzeit, falls aktiv
- Status von Innen- und Aussenluefter
- aktive Warnungen und Fehler
- automatisch oder manuell erfolgter Sensorersatz
- Wiederanlaufstatus und Vertrauensstufe einer Zeitkorrektur
- letzte relevante Zustandsaenderung

Beispiel:

```text
Technische Details

Regelung:       Produkt
Peltier:        Heizen
Aussenluefter:  EIN
Innenluefter:   EIN
Sensorstatus:   gueltig
Meldungen:      1

[Prozess]           [Meldungen]
```

Die normale Laufansicht bleibt bewusst einfacher. Technische Begriffe gehoeren
primaer auf die Technikseite.

## STOP-Ablauf

`STOP` oeffnet unmittelbar einen Auswahl- und Bestaetigungsdialog. Ein zusaetzliches
langes Gedrueckthalten oder eine weitere allgemeine Ja/Nein-Abfrage ist nicht
erforderlich.

```text
Programm wirklich abbrechen?

[Abbrechen und ausschalten]
[Abbrechen und kuehlen]
[Zurueck]
```

### Abbrechen und ausschalten

- beendet den aktuellen Lauf als abgebrochen
- schaltet das Peltier sicher aus
- startet den definierten Luefternachlauf
- protokolliert den Benutzerabbruch
- fuehrt anschliessend zu `STANDBY`

### Abbrechen und kuehlen

Vor dem Start des neuen manuellen Kuehllaufes wird mindestens bestaetigt:

```text
Kuehlziel: 8,0 °C
Danach: bis manuell beendet halten

[START KUEHLEN]
[Zurueck]
```

Das Kuehlen ist ein neuer manueller Lauf mit eigenem Schnappschuss. Die
urspruengliche Fermentation bleibt als abgebrochen protokolliert.

## Bildschirm `Produkt einsetzen`

Nach abgeschlossenem Vorheizen zeigt das Geraet:

```text
Schrank vorbereitet

Schrank:    42,1 °C
Produkt:    nicht angeschlossen

Produkt einsetzen und Tuer schliessen.

[WEITER]
[Details]       [Abbrechen]
```

Regeln:

- `WEITER` bestaetigt nur, dass das Produkt eingesetzt wurde.
- `WEITER` startet nicht unmittelbar den Fermentationstimer.
- Danach folgen `REACHING_TARGET` und `QUALIFYING_TARGET`.
- Die Produktsensoranzeige zeigt den tatsaechlichen Status.
- Da im ersten Release kein Tuerkontakt vorhanden ist, behauptet die Software
  nicht, die Tuerstellung erkannt zu haben.
- `Details` zeigt insbesondere Sensorstatus, Sollwert und Vorheizstatus.
- `Abbrechen` verwendet den normalen Abbruchdialog.

## Mehrere gleichzeitig aktive Meldungen

Die Meldung mit der hoechsten Prioritaet wird als Banner prominent angezeigt.
Eine Zahl zeigt die Gesamtzahl der aktiven Meldungen.

```text
! Produktfuehler ausgefallen        [2]

Joghurt mild
Fermentation laeuft
...

[Meldungen]  [Details]  [STOP]
```

Die Meldungsliste:

- ist nach der in `RUNTIME_BEHAVIOR.md` festgelegten Prioritaet sortiert
- zeigt Meldungsklasse, Titel, Zeitpunkt und aktuellen Zustand
- verdeckt keine hoeher priorisierte Meldung durch eine niedrigere
- unterscheidet aktive, quittierte und erledigte Meldungen
- erlaubt das Oeffnen einer Detailansicht pro Meldung

## Quittieren und Stummschalten

Die Oberflaeche verwendet den technischen Begriff **`Quittieren`** und nicht
`Gesehen` oder `Gelesen`.

`Quittieren` bedeutet:

- Der Benutzer bestaetigt, dass er die Meldung zur Kenntnis genommen hat.
- Ein nicht mehr notwendiges grosses Banner darf danach verkleinert oder
  geschlossen werden.
- Die Ursache wird dadurch nicht beseitigt.
- Eine weiterhin aktive Warnung oder ein weiterhin aktiver Fehler bleibt in der
  Status- beziehungsweise Meldungsanzeige erhalten.
- Das Ereignis bleibt im Protokoll.

`Stummschalten` bedeutet ausschliesslich:

- das aktuelle akustische Signal beenden
- die sichtbare Meldung und ihre Ursache unveraendert lassen

Beispiel bei ausfallendem Produktfuehler:

```text
Produktfuehler ausgefallen

Regelung wechselt in 03:24 auf Luft.

[Mit Luft fortsetzen]
[Stummschalten]
[Details]
```

Eine spaetere Schaltflaeche `Quittieren` darf das Banner reduzieren, sobald keine
unmittelbare Benutzeraktion mehr erforderlich ist.

## Anzeige nach automatischem Wiederanlauf

Ein automatischer Wiederanlauf blockiert den bereits wieder laufenden Prozess
nicht.

Beispiel nach verfuegbarer Zeitbewertung:

```text
Nach Stromausfall automatisch fortgesetzt

Unterbrechung: 18 min
Verlaengerung: +11 min

[Details] [Quittieren]
```

Regeln:

- Der Prozess laeuft bereits weiter.
- `Quittieren` ist keine Freigabe zum Fortfahren.
- Vor verfuegbarer Netzwerkzeit wird statt einer erfundenen Dauer angezeigt,
  dass die Unterbrechungsdauer noch bestimmt wird.
- Nach der Zeitbestimmung werden Dauer, Vertrauensstufe, Wiederanlaufaktion und
  Laufzeitkorrektur sichtbar nachgetragen.
- Nach `Quittieren` bleibt der Wiederanlauf im Ereignisprotokoll und in den
  technischen Laufdetails nachvollziehbar.

## Anzeige nach automatischem Sensorwechsel

Nach dem automatischen Wechsel vom Produkt- zum Schrankluftfuehler wird eine
Warnung angezeigt:

```text
WARNUNG: Regelung ueber Schrankluftfuehler

Produktfuehler ausgefallen
Wechsel um 14:37 Uhr

[Details] [Quittieren]
```

Regeln:

- `Quittieren` reduziert das grosse Banner, beendet aber nicht die Kennzeichnung
  des geaenderten Regelmodus.
- Der aktive Regelmodus bleibt mindestens in der Laufdetailansicht sichtbar.
- Eine kompakte Warnkennzeichnung bleibt solange erhalten, wie der Lauf mit dem
  Ersatzsensor weitergefuehrt wird.
- Zeitpunkt, vorheriger Sensor, Ersatzsensor und Grund werden protokolliert.
- Kehrt der Produktfuehler spaeter zurueck, wird nicht unbemerkt automatisch
  zurueckgewechselt. Das Verhalten wird spaeter mit den Fehler- und
  Sensorwechselregeln festgelegt.

## Bildschirm nach Programmende

Ein Programm ohne laufendes Kuehlhalten zeigt:

```text
Programm beendet

Wasserkefir
Dauer: 48:00 h

[OK]
[Details]
[Jetzt kuehlen]
```

Regeln:

- `OK` quittiert den Abschluss und fuehrt zu `STANDBY`.
- `Details` zeigt Laufzusammenfassung, Warnungen und relevante Ereignisse.
- `Jetzt kuehlen` wird nur angeboten, wenn Kuehlung technisch und sicher
  zulaessig ist.
- Vor `Jetzt kuehlen` werden Kuehlziel und Halteverhalten bestaetigt.
- Die nachtraegliche Kuehlung ist ein neuer manueller Kuehllauf.
- Bei einem bereits aktiven `COOL_HOLDING` wird nicht zusaetzlich `Jetzt kuehlen`
  angeboten; dort endet der Lauf durch die vorgesehene manuelle Beendigung.

## Akzeptierte Entscheidungen aus Phase 4C

- [x] Detailansicht mit fester Prozess- und Technikseite
- [x] STOP oeffnet direkt den Auswahl- und Bestaetigungsdialog
- [x] `Produkt einsetzen` bietet `WEITER`, `Details` und `Abbrechen`
- [x] hoechste Meldung als Banner, weitere Meldungen in priorisierter Liste
- [x] akustisches Signal kann stummgeschaltet werden, Meldung bleibt erhalten
- [x] Bestaetigung von Meldungen wird als `Quittieren` bezeichnet
- [x] automatischer Wiederanlauf blockiert nicht auf eine Quittierung
- [x] automatischer Sensorwechsel bleibt auch nach Quittierung gekennzeichnet
- [x] Programmende bietet `OK`, `Details` und optional `Jetzt kuehlen`

## Noch offen fuer Phase 4D und spaeter

- genaue Menuestruktur
- Trennung normaler Einstellungen und geschuetzter Serviceeinstellungen
- Zugangsschutz zum Servicemodus
- Diagnosebildschirme
- Touchkalibrierung und Wiederherstellungsweg bei fehlerhafter Kalibrierung
- konkrete Texte, Schriftgroessen, Kontraste und Symbole
- maximale Anzahl gleichzeitig gespeicherter Meldungen
- Verhalten bei Rueckkehr eines zuvor ausgefallenen Produktfuehlers
