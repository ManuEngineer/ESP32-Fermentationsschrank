# Lokale Touch-Bedienung

## Status

Dieses Dokument beschreibt die lokale Bedienoberflaeche auf dem
320-x-240-Pixel-Touchdisplay. Die in Phase 4A akzeptierten Grundlagen sind
verbindlich. Detailablaeufe, einzelne Dialoge und die genaue visuelle Gestaltung
werden in den folgenden Teilphasen ergaenzt.

## Hardware- und Bedienannahmen

- Displayausrichtung: Querformat, 320 x 240 Pixel
- resistiver Touchscreen
- Bedienung muss ohne Wischgesten vollstaendig moeglich sein
- grosse, deutlich getrennte Schaltflaechen
- keine wichtigen Funktionen nur ueber kleine Symbole
- jeder Unterdialog besitzt einen klaren `Zurueck`- oder `Abbrechen`-Weg
- kritische Aktionen benoetigen eine bewusste Bestaetigung

Die genaue Touchkalibrierung und der konkrete Touchcontroller bleiben von der
Hardwareverifikation abhaengig.

## Grundprinzip: zustandsabhaengiger Hauptbildschirm

Der Hauptbildschirm passt sich dem aktuellen Betriebszustand an. Das Geraet
zeigt nicht immer zuerst ein starres Hauptmenue.

### Standby

Im Standby stehen Status und Startfunktionen im Vordergrund.

Beispiel:

```text
Fermentationsschrank

Luft:       22,4 °C
Produkt:    nicht angeschlossen

[Programm starten]
[Manueller Betrieb]
[Menue]
```

Verbindliche Inhalte:

- aktuelle Schranklufttemperatur
- Produkttemperatur oder klarer Status `nicht angeschlossen`
- sichtbarer Sensorfehler, falls ein erkannter Sensor ungueltig ist
- Start eines gespeicherten Programms
- Start eines manuellen Betriebs
- Zugang zu Menue, Einstellungen und Servicefunktionen

### Laufender Prozess

Waerend eines Laufes stehen Prozesszustand, Temperaturen und verbleibende Dauer
im Vordergrund.

Beispiel:

```text
Joghurt mild
FERMENTIERT

Soll:       42,0 °C
Produkt:    41,8 °C
Luft:       42,3 °C
Restzeit:   06:14

[Details]       [STOP]
```

Der dargestellte Phasenname muss fuer normale Benutzer verstaendlich sein. Die
internen Zustandsnamen wie `QUALIFYING_TARGET` duerfen in der normalen Ansicht
nicht zwingend unveraendert angezeigt werden.

Moegliche Benutzernamen:

| Interner Zustand | Normale Anzeige |
|---|---|
| `PREHEATING` | Schrank wird vorbereitet |
| `WAITING_FOR_PRODUCT` | Produkt einsetzen |
| `REACHING_TARGET` | Zieltemperatur wird erreicht |
| `QUALIFYING_TARGET` | Zieltemperatur wird geprüft |
| `FERMENTING` | Fermentation laeuft |
| `COOLING` | Wird heruntergekuehlt |
| `COOL_HOLDING` | Wird gekuehlt gehalten |
| `COMPLETED` | Programm beendet |

Die Anzeige fuer `QUALIFYING_TARGET` bezeichnet die Zielqualifikation und darf
nicht mehr mit dem frueher verwendeten Begriff `Stabilisierung` benannt werden.
Die endgueltigen Formulierungen werden spaeter sprachlich geprueft.

## Navigation

Die Bedienung erfolgt primaer ueber grosse Schaltflaechen.

Verbindliche Regeln:

- keine notwendige Wischgeste
- keine versteckten Randgesten
- `Zurueck` ist in Unterseiten immer eindeutig erreichbar
- `STOP` ist waehrend eines Laufes eindeutig sichtbar, aber gegen
  versehentliche Ausloesung geschuetzt
- Dialoge duerfen nicht mehrere unterschiedliche Aktionen unter identischen
  Symbolen verstecken
- bei mehr Inhalt als auf eine Seite passt, werden grosse Seiten- oder
  Scroll-Schaltflaechen verwendet

Eine feste Navigationsleiste wird nicht vorausgesetzt, weil sie auf 240 Pixel
Hoehe zu viel Platz beanspruchen kann.

## Temperaturanzeige

Produkt- und Schranklufttemperatur werden waehrend eines Laufes grundsaetzlich
gleichwertig und gleich gross angezeigt, sofern beide verfuegbar sind.

Beispiele:

```text
Produkt     41,8 °C
Luft        42,3 °C
```

```text
Produkt     --.- °C
Luft        24,1 °C
```

Die Hauptansicht muss nicht durch unterschiedliche Schriftgroessen erklaeren,
welcher Sensor die Regelung fuehrt. Diese technische Information wird unter
`Details` angezeigt.

Die Detailansicht zeigt mindestens:

- aktiver Regelmodus: Produkt oder Schrankluft
- primaerer Regelsensor
- Sensorstatus beider Fühler
- Sollwert
- Zielband beziehungsweise Regeltoleranz, sofern fuer Benutzer freigegeben
- aktuelle Prozessphase
- Heiz-, Kuehl- oder Halteanforderung
- Warnungen oder ein erfolgter Sensorersatz

Bei nur einem verfuegbaren Sensor bleibt die Position des fehlenden Sensors
sichtbar und wird mit einem klaren Status statt mit einem scheinbar gueltigen
Wert dargestellt.

## Displaybeleuchtung

Das Display bleibt nicht dauerhaft mit voller Helligkeit aktiv.

Vorgesehenes Verhalten:

1. Nach einer einstellbaren Zeit ohne Bedienung wird die Beleuchtung reduziert.
2. Eine Beruehrung stellt zuerst die normale Helligkeit wieder her.
3. Die erste Beruehrung zum Aufwecken darf keine darunterliegende Aktion
   versehentlich ausloesen.
4. Warnung, Fehler, Programmende oder erforderliche Benutzeraktion stellen die
   normale Helligkeit automatisch wieder her.
5. Ein Sicherheitsfehler darf nicht durch eine abgedunkelte Anzeige verborgen
   bleiben.

Die konkrete Wartezeit und Helligkeitsstufe werden in
`SETTINGS_AND_STORAGE.md` festgelegt.

## Programmstart und Startbestaetigung

Vor jedem Programmstart wird im normalen Betrieb eine Zusammenfassung angezeigt.

Beispiel:

```text
Joghurt mild

Ziel:          42,0 °C
Dauer:         8:00 h
Vorheizen:     Ja
Regelung:      Produkt, sonst Luft
Danach:        Kuehlen und halten

[START]       [Zurueck]
```

Die Zusammenfassung zeigt mindestens:

- Programmname
- Zieltemperatur
- Fermentationsdauer
- Vorheizen EIN/AUS
- gewaehlter Sensorbetrieb
- Status des Produktfuehlers
- Abschlussverhalten
- Kuehlziel, falls Kuehlung aktiviert ist
- wichtige programmspezifische Warnungen

### Standardverhalten

Die Startbestaetigung ist standardmaessig immer erforderlich.

### Spaetere Serviceeinstellung

Es soll spaeter eine geschuetzte Serviceeinstellung geben, mit der fuer
bestimmte gespeicherte Programme ein Direktstart ohne den zusaetzlichen
Bestaetigungsdialog erlaubt werden kann.

Rahmenbedingungen:

- nicht Bestandteil des einfachen normalen Einstellungsmenues
- werkseitig beziehungsweise standardmaessig AUS
- keine Umgehung von Sicherheitspruefungen
- Sensorstatus und Programmdaten werden auch beim Direktstart validiert
- Programme mit fehlenden oder ungueltigen Pflichtwerten starten niemals direkt
- Start aus einem Warn- oder Fehlerzustand bleibt unzulaessig

Die konkrete Einstellung und Berechtigung wird in
`SETTINGS_AND_STORAGE.md` festgelegt.

## Eingabe von Zahlenwerten

Fuer numerische Werte werden zwei Bedienarten kombiniert.

### Kleine Anpassung

Grosse Plus- und Minus-Schaltflaechen:

```text
Zieltemperatur

[-]     42,0 °C     [+]

[Direkt eingeben]
```

### Direkte Eingabe

Beim Antippen des Wertes oder von `Direkt eingeben` erscheint ein grosser
Ziffernblock.

Der Ziffernblock muss mindestens unterstuetzen:

- Ziffern 0 bis 9
- Dezimaltrennzeichen bei Temperaturwerten
- Rueckschritt
- Eingabe loeschen
- Abbrechen
- Uebernehmen

Verbindliche Validierungsregeln:

- ungueltige Eingaben koennen nicht uebernommen werden
- erlaubter Wertebereich wird sichtbar genannt
- das Dezimaltrennzeichen wird fuer die deutsche Oberflaeche als Komma
  dargestellt, intern aber eindeutig gespeichert
- Dauerwerte werden fuer normale Benutzer als Stunden und Minuten angezeigt
- Plus/Minus-Schrittweite richtet sich nach dem Feldtyp
- langes Gedrueckthalten darf spaeter eine schnellere Aenderung erlauben, ist
  aber keine notwendige Bedienfunktion

## Touch-Zielgroesse und Fehlbedienung

Wegen des resistiven Touchscreens gelten folgende Gestaltungsregeln:

- grosse Touchflaechen mit ausreichendem Abstand
- keine eng nebeneinanderliegenden kleinen Plus-, Minus- und Loeschsymbole
- gefaehrliche Aktionen nicht direkt neben haeufigen Komfortaktionen
- keine Aktion nur durch Farbe kennzeichnen; Text oder Symbol ergaenzt die Farbe
- deutliche Rueckmeldung nach jedem erkannten Tastendruck
- waehrend eines noch laufenden Speichervorgangs keine wiederholte Ausloesung
  derselben Aktion

## Akzeptierte Entscheidungen aus Phase 4A

- [x] Querformat 320 x 240 Pixel
- [x] zustandsabhaengiger Hauptbildschirm
- [x] Bedienung ueber grosse Schaltflaechen ohne notwendige Wischgesten
- [x] Produkt- und Schranklufttemperatur gleich gross anzeigen
- [x] aktiven Regelmodus in der Detailansicht darstellen
- [x] Display nach einstellbarer Zeit abdunkeln
- [x] erste Beruehrung nach Abdunkelung weckt nur die Anzeige
- [x] Startbestaetigung standardmaessig immer erforderlich
- [x] spaetere geschuetzte Serviceeinstellung fuer Direktstart vorsehen
- [x] Plus/Minus und Ziffernblock fuer numerische Eingaben kombinieren

## Noch offen fuer Phase 4B und spaeter

- Aufbau der Programmauswahl bei vielen Programmen
- Bearbeiten, Kopieren, Umbenennen und Loeschen von Programmen
- genaue Detailansicht waehrend eines Laufes
- STOP- und Abbruchdialoge
- Anzeige und Quittierung mehrerer Meldungen
- Bedienablauf fuer `WAITING_FOR_PRODUCT`
- Bedienablauf nach automatischem Wiederanlauf
- Menuestruktur fuer Einstellungen, Diagnose und Servicemodus
- Sprache, Symbole, Kontraste und konkrete Schriftgroessen
- Touchkalibrierung und Wiederherstellung bei fehlerhafter Kalibrierung
