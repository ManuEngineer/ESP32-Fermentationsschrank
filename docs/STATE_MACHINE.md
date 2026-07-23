# Betriebszustaende und Zustandsmaschine

## Status

Dieses Dokument definiert die kanonischen Betriebszustaende und Uebergaenge fuer
Release 1. Ergaenzende Detailregeln stehen in
[`RECOVERY_AND_INTERRUPTION.md`](RECOVERY_AND_INTERRUPTION.md),
[`SAFETY_AND_FAULTS.md`](SAFETY_AND_FAULTS.md) und
[`PR38_REVIEW_CORRECTIONS.md`](PR38_REVIEW_CORRECTIONS.md).

## Grundsaetze

- Jeder Zustand hat einen klar definierten Zweck.
- Aktoren duerfen nur in Zustaenden aktiv sein, in denen dies ausdruecklich
  vorgesehen ist.
- Ein Zustandswechsel wird validiert, atomar gespeichert und protokolliert,
  bevor daraus eine neue Aktorfreigabe entsteht.
- Direkte GPIO- oder H-Brueckenzustaende sind nie Teil des fachlichen
  Wiederanlaufzustands.
- Es gibt keine allgemeine Pausenfunktion fuer laufende Fermentationen.
- Warnungen, Fehler, Quittierung und Fehlerreset bleiben getrennt.
- Ein manueller Lauf verwendet dieselben Regel- und Sicherheitsmechanismen wie
  ein gespeichertes Programm.
- Direkte Aktortests sind ausschliesslich im geschuetzten `SERVICE_MODE` aus
  validiertem `STANDBY` zulaessig.
- Ein Wiederanlauf wartet nicht unnoetig auf Netzwerkzeit oder Benutzer, darf
  aber weder Sicherheitsfreigaben noch unbekannten Fortschritt erfinden.
- Ein echter Sicherheitsfehler hat immer Vorrang vor automatischem Fortfahren.

## Grenze des fachlichen Zustandsautomaten

Der fachliche Zustandsautomat ist deterministisch, hardwarefrei und
persistenzfrei. Er erhaelt:

- den aktuellen fachlichen Zustand
- einen unveraenderlichen Laufschnappschuss
- abstrahierte, bereits qualitaetsgepruefte Prozesssignale
- fachliche Ereignisse beziehungsweise Benutzerentscheidungen
- monotone virtuelle Zeit

Er liefert eine noch nicht angewendete Uebergangsentscheidung mit bisherigem und
vorgeschlagenem Zustand, Grund beziehungsweise Ereigniscode, relevantem
monotonem Zeitpunkt, aktualisierten fachlichen Phasendaten und gegebenenfalls
fachlichen Meldungen.

Die Berechnung veraendert den bisherigen Zustand nicht unumkehrbar. Erst der
aufrufende Anwendungsteil darf die Entscheidung nach erfolgreicher atomarer
Speicherung bestaetigen und anwenden. Schlaegt die Speicherung fehl, bleibt der
bisherige Zustand wirksam und es entsteht keine neue Aktorfreigabe. Speicherung,
Kontrollpunkte, Rueckfallrevisionen und kritische Schreibfehler sind nicht Teil
des fachlichen Zustandsautomaten.

Die Implementierung verwendet dafuer zwei getrennte Schritte:

1. `decideProcessTransition()` liefert eine vollstaendige, noch nicht angewendete
   Entscheidung mit Ausgangs- und Folgezustand.
2. `applyProcessTransition()` uebernimmt sie nur, wenn der vollstaendige
   Ausgangszustand weiterhin uebereinstimmt. Veraltete oder bereits angewendete
   Entscheidungen werden ohne Zustandsaenderung abgelehnt.

Kritische Fehler werden vor Bedienereignissen und normalen Phasenfortschritten
ausgewertet. In `WAITING_FOR_PRODUCT` hat der belastbar erreichte Zeitablauf
Vorrang vor einer gleichzeitig eintreffenden Produktbestaetigung.

## Kanonische Zustandsnamen

```text
BOOT
SAFE_BOOT
STANDBY
PREHEATING
WAITING_FOR_PRODUCT
REACHING_TARGET
QUALIFYING_TARGET
FERMENTING
COOLING
COOL_HOLDING
MANUAL_HOLDING
COMPLETED
RECOVERY_EVALUATION
FAULT
SERVICE_MODE
```

Zusaetzliche Kontexte:

```text
RECOVERY_TIME_PENDING
WARNING_REQUIRES_ACTION
```

`RECOVERY_TIME_PENDING` und `WARNING_REQUIRES_ACTION` koennen einem normalen
Prozesszustand ueberlagert sein und sind nicht zwingend eigene blockierende
Hauptzustaende.

## BOOT

### Zweck

Der ESP32 startet mit ausgeschalteten Aktoren, bewertet die gespeicherte
Sicherheits- und Laufhistorie und waehlt erst danach den naechsten Zustand.

### Verbindliche Reihenfolge

```text
BOOT
  -> Peltier und beide BTS7960-Richtungen AUS
  -> alle schaltbaren Ausgaenge zunaechst AUS
  -> Resetursache und abnormalen Neustartzaehler auswerten
  -> Konfiguration und kritischen Speicher validieren
  -> persistierte Verriegelungen und unvollstaendige Transaktionen auswerten
  -> Sensoren und Hardwarefreigaben grundlegend pruefen
  -> gespeicherten Laufzustand klassifizieren
  -> Netzwerk und Zeitabgleich parallel vorbereiten
```

### Uebergaenge

```text
Bootschleife, persistierte Sperre, unvollstaendige Transaktion,
kritischer Speicher- oder Initialisierungsfehler
  -> SAFE_BOOT oder FAULT gemaess Fehlerklasse

gueltiger persistierter Zustand COMPLETED
  -> COMPLETED

gueltiger unterbrochener aktiver Lauf
  -> RECOVERY_EVALUATION

kein aktiver oder abgeschlossener Lauf und alle Bootpruefungen bestanden
  -> STANDBY
```

`STANDBY`, `COMPLETED` und `RECOVERY_EVALUATION` sind vor Abschluss der
Bootpruefungen nicht erreichbar. Ein Neustart loescht keine Verriegelung und ist
kein Fehlerreset.

## SAFE_BOOT

### Zweck

Sicherer Wartungszustand nach wiederholtem abnormalem Neustart, persistierter
Sperre oder nicht ausreichend nachweisbarer Systemintegritaet.

### Aktoren

- Peltier AUS
- beide BTS7960-Richtungen AUS
- keine Heiz-, Kuehl-, Luefter- oder Summer-Aktortests
- nur ein sicherheitsbedingt erforderlicher, bereits laufender Luefternachlauf
  darf nach expliziter Fehlerregel beendet werden

### Erlaubte Funktionen

- passive Diagnose
- Fehler- und Resetjournal lesen
- Berichte exportieren
- Netzwerkwiederherstellung ohne Aktorwirkung
- PIN-unabhaengigen lokalen Vollreset ausloesen
- UART-Recovery beziehungsweise erneutes Flashen

Eine Rueckkehr zu `STANDBY` erfordert beseitigte Ursache, bestandene
Integritaetspruefung und den fuer die Fehlerklasse vorgesehenen bewussten Reset.

## STANDBY

### Zweck

Sicherer Ruhezustand ohne laufenden Prozess.

### Aktoren

- Peltier AUS
- Aussenluefter AUS
- Innenluefter standardmaessig AUS
- Summer AUS

### Verfuegbare Funktionen

- Temperaturen und Sensorstatus anzeigen
- Programme auswaehlen und bearbeiten
- neuen Programmlauf starten
- manuellen Zeit-/Temperaturlauf starten
- manuellen Temperatur-Haltebetrieb starten
- `SERVICE_MODE` oeffnen
- Einstellungen, Diagnose und Netzwerkfunktionen verwenden

Ein neuer Lauf startet nur nach vollstaendiger Validierung des
Programmschnappschusses, der Pflichtsensoren, der Sicherheitsfreigaben und der
kritischen Persistenz.

## PREHEATING

### Zweck

Leeren Schrank vor dem Einsetzen des Produkts auf die Programmsolltemperatur
bringen. Je nach Ausgangslage kann dies Heizen oder Kuehlen bedeuten.

```text
Ziel des leeren Schrankes erfolgreich qualifiziert
  -> WAITING_FOR_PRODUCT
```

Nach einer Unterbrechung wird die Vorheizphase nur wieder aufgenommen, wenn der
gespeicherte Lauf gueltig ist, keine Sperre aktiv ist und die maximale Wartezeit
noch nicht sicher abgelaufen ist.

## WAITING_FOR_PRODUCT

### Zweck

Vorbereitete Temperatur halten und auf das bewusste Einsetzen des Produkts
warten.

### Verhalten

- Anzeige `Produkt einsetzen`
- akustisches, quittierbares Signal
- Temperaturregelung bleibt aktiv, soweit sicher und programmgemaess
- Fermentationstimer laeuft nicht
- pro Programm konfigurierbare maximale Wartezeit

Die sichtbare und akustische Aufforderung beim Eintritt in
`WAITING_FOR_PRODUCT` ist zugleich die Warnung vor Ablauf der Wartezeit. Release
1 besitzt keine zweite, davon getrennte Warnschwelle.

### Uebergaenge

```text
Benutzer drueckt WEITER / START
  -> REACHING_TARGET

maximale Wartezeit sicher abgelaufen
  -> Vorheizen beenden
  -> Lauf als nicht gestartet oder abgebrochen protokollieren
  -> STANDBY
```

Auch nach einem Neustart darf nicht angenommen werden, dass das Produkt bereits
eingesetzt wurde.

## REACHING_TARGET

### Zweck

Die fuer den Lauf massgebende Temperatur auf den Sollwert bringen.

Moegliche Aktionsrichtung:

- Heizen
- Kuehlen
- Temperatur halten, falls bereits im Zielbereich

### Uebergaenge

```text
Zielbereich erreicht
  -> QUALIFYING_TARGET

maximale Zielerreichungszeit ueberschritten
  -> Warnung; Zustand laeuft weiter, solange sicher

kritischer Sensor- oder Sicherheitsfehler
  -> FAULT
```

## QUALIFYING_TARGET

### Zweck

Pruefen, ob der Sollwert fuer die festgelegte Zeit ausreichend im Zielband liegt.
Die Zielqualifikation ist nicht Teil der Fermentationszeit.

```text
Ziel erfolgreich qualifiziert
  -> FERMENTING fuer einen zeitgesteuerten Lauf
  -> MANUAL_HOLDING fuer einen manuellen Temperatur-Haltebetrieb

laengere oder deutliche Abweichung
  -> REACHING_TARGET oder Neustart der Qualifikation

kritischer Fehler
  -> FAULT
```

Nach einem Neustart beginnt eine zuvor nur teilweise absolvierte
Zielqualifikation neu.

## FERMENTING

### Zweck

Temperaturgefuehrte Fermentationsphase mit laufendem Fortschrittsmodell.

### Regeln

- keine allgemeine Pausenfunktion
- kurze oder moderate Temperaturabweichungen stoppen den Timer nicht automatisch
- Warnungen koennen den Lauf weiterlaufen lassen
- harte Fehler oder Sicherheitsgrenzen fuehren zu `FAULT`
- ein Phasenabschluss darf nur aus belastbaren Fortschrittsdaten abgeleitet werden

### Normale Uebergaenge

```text
Fermentationsziel erreicht, Abschluss ohne Kuehlung
  -> COMPLETED

Fermentationsziel erreicht, aktives Kuehlen vorgesehen
  -> COOLING
```

### Wiederanlauf

Nach validierter Recovery darf die Temperaturregelung automatisch neu abgeleitet
werden. Fehlt eine verlaessliche absolute Zeit, bleibt
`RECOVERY_TIME_PENDING` aktiv. Der unbekannten Unterbrechung wird kein erfundener
exakter Fortschritt gutgeschrieben und der Lauf wird nicht allein aufgrund einer
Schaetzung automatisch abgeschlossen.

## COOLING

### Zweck

Produkt beziehungsweise Schrank nach der Fermentation auf die konfigurierte
Kuehlzieltemperatur bringen.

```text
Kuehlziel erreicht, danach beenden
  -> COMPLETED

Kuehlziel erreicht, danach zeitlich begrenzt oder manuell halten
  -> COOL_HOLDING

kritischer Fehler
  -> FAULT
```

Nach einer Unterbrechung wird eine gueltige Kuehlphase nur nach vollstaendiger
Recoverypruefung automatisch wieder aufgenommen.

## COOL_HOLDING

### Zweck

Kuehltemperatur fuer eine festgelegte Dauer oder bis zur manuellen Beendigung
halten.

```text
belastbar bestimmte Haltezeit abgelaufen
  -> COMPLETED

Benutzer beendet Halten
  -> COMPLETED

kritischer Fehler
  -> FAULT
```

Bei unsicherer Ausfallzeit wird kein automatisches Ende aus einem einzelnen
Schaetzwert abgeleitet. Die Regelung darf sicher weiterlaufen, waehrend
`RECOVERY_TIME_PENDING` beziehungsweise `WARNING_REQUIRES_ACTION` sichtbar ist.

## MANUAL_HOLDING

### Zweck

Eine manuell gewaehlte Zieltemperatur ohne Timer halten.

Der Zustand nutzt dieselbe Regel- und Sicherheitslogik wie ein Programmlauf und
endet durch bewusste Benutzeraktion oder Fehler. Nach einer Unterbrechung wird er
nur nach erfolgreicher Recoverypruefung automatisch fortgesetzt.

Ein manueller Haltebetrieb wechselt nie direkt von `STANDBY` nach
`MANUAL_HOLDING`. Sein validierter, unveraenderlicher manueller Laufplan
durchlaeuft:

```text
mit Vorheizen:
  STANDBY -> PREHEATING -> WAITING_FOR_PRODUCT
          -> REACHING_TARGET -> QUALIFYING_TARGET -> MANUAL_HOLDING

ohne Vorheizen:
  STANDBY -> REACHING_TARGET -> QUALIFYING_TARGET -> MANUAL_HOLDING
```

Der Laufplan und das bestaetigte Startkommando werden von der Kommandoschicht
erstellt und validiert. Der Zustandsautomat definiert und prueft den vollstaendigen
Uebergangsweg.

## COMPLETED

### Zweck

Lauf ist fachlich abgeschlossen und wartet auf Benutzerquittierung.

### Verhalten

- Peltier AUS
- definierter Luefternachlauf
- optische Meldung `PROGRAMM BEENDET`
- kurzes akustisches Signalmuster
- abgeschlossener Lauf und Ergebnisdaten bleiben sichtbar

```text
Benutzer quittiert
  -> STANDBY
```

Ein gueltig persistiertes `COMPLETED` wird beim Boot wieder als `COMPLETED`
hergestellt. Es erfolgt weder ein automatischer Wechsel nach `STANDBY` noch der
Neustart einer Regelphase.

## Manuelles Stoppen

Es gibt keine Pause. Nach `STOP` zeigt die Bedienoberflaeche:

```text
[Abbrechen und ausschalten]
[Abbrechen und kuehlen]
[Zurueck]
```

### Abbrechen und ausschalten

- laufenden Prozess als abgebrochen markieren
- Peltier AUS
- definierten Luefternachlauf ausfuehren
- Abbruch protokollieren
- danach `STANDBY`

### Abbrechen und kuehlen

- urspruenglichen Prozess als abgebrochen markieren
- Benutzer bestaetigt Kuehlziel und Abschlussverhalten
- neuen expliziten manuellen Kuehllauf mit eigenem Schnappschuss anlegen
- Uebergang in `REACHING_TARGET` beziehungsweise `COOLING`

## Warnungen und WARNING_REQUIRES_ACTION

Warnungen lassen den Prozess grundsaetzlich weiterlaufen, sofern die
Sicherheitslogik dies erlaubt. Sie werden sichtbar angezeigt und protokolliert.

`WARNING_REQUIRES_ACTION` wird verwendet, wenn eine fachliche Entscheidung offen
ist, etwa bei nicht eindeutig aufloesbarer Recovery-Zeit oder einem ausgefallenen
optionalen Produktfuehler. Die Fehler- und Aktormatrix bestimmt, welche sichere
Regelaktion waehrenddessen zulaessig bleibt.

## FAULT

Fehler bedeuten, dass der Prozess nicht normal weiterlaufen darf.

Beispiele:

- primaerer Prozesssensor ungueltig und kein validierter Ersatzbetrieb
- harte Ueber- oder Untertemperaturgrenze
- widerspruechliche H-Bruecken-Anforderung
- kritischer interner Software- oder Persistenzfehler
- sichere Ausgangszustaende nicht gewaehrleistet

### Verhalten

- neue Aktoranforderungen sperren
- Peltier unverzueglich deaktivieren
- erforderlichen Luefternachlauf gemaess Fehlerstrategie ausfuehren
- stabilen Fehlercode anzeigen und protokollieren
- kein automatisches Zurueckkehren ohne ausdruecklich erlaubte Wiederfreigabe

## RECOVERY_EVALUATION

### Zweck

Nach `BOOT` unverzueglich bestimmen, wie ein unterbrochener Lauf sicher und
fachlich sinnvoll fortgesetzt wird.

Vor Eintritt wurden bereits Bootschleifen, persistierte Verriegelungen,
unvollstaendige Transaktionen und grundlegende Speicherintegritaet geprueft.
Zusaetzlich werden mindestens bewertet:

- Programmschnappschuss und Laufrevisionen
- Phase des unterbrochenen Laufes
- aktueller und letzter gueltiger Sensorstatus
- aktuelle und letzte bekannte Temperaturen
- Zeitqualitaet und moegliches Ausfallintervall
- aktive Warnungen und Fehler
- sichere phasenbezogene Aktoraktion

### Uebergaenge

```text
Fortsetzung technisch und sicher zulaessig
  -> geeigneten normalen Prozesszustand wieder aufnehmen

Zeit noch nicht belastbar
  -> geeigneten sicheren Prozesszustand wieder aufnehmen
  -> Kontext RECOVERY_TIME_PENDING setzen

Fortsetzung nicht sicher
  -> FAULT

Lauf nicht fachlich rekonstruierbar
  -> verriegelter Fehlerzustand; keine Aktorfreigabe
```

Die Recoveryentscheidung wird als neue Revision gespeichert, bevor Aktoren
freigegeben werden.

## RECOVERY_TIME_PENDING

### Zweck

Kennzeichnen, dass die sichere aktuelle Prozessaktion bestimmt ist, die
Unterbrechungsdauer und Fortschrittskorrektur aber noch nicht belastbar sind.

### Verhalten

- NTP-Zeitabgleich laeuft im Hintergrund
- UI zeigt die ausstehende Zeitbewertung
- keine scheinbar exakte Restzeit behaupten
- keinen frei geschaetzten Unterbrechungsfortschritt anrechnen
- keinen automatischen Phasenabschluss allein aus unbekannter Zeit ableiten
- sichere phasenbezogene Regelung fortsetzen, soweit eindeutig zulaessig

Nach verfuegbarer vertrauenswuerdiger UTC-Zeit wird die Ausfalldauer als
Unter-/Obergrenze aus Kontrollpunktzeit und maximalem Kontrollpunktabstand
berechnet. Ueberschneidet das Intervall eine Phasengrenze, bleibt eine sichtbare
Benutzerentscheidung erforderlich.

## SERVICE_MODE

### Eintritt

Nur aus validiertem `STANDBY`, nie aus `SAFE_BOOT`, `FAULT` oder einem laufenden
Prozess.

### Regeln

- Service-PIN erforderlich
- deutlicher Warnhinweis vor Aktortests
- Peltier- und Lueftertests zeitlich und leistungsmassig begrenzen
- Richtungswechsel, Mindest-Ausschaltzeit und Totzeit bleiben erzwungen
- Sensor- und Sicherheitspruefungen bleiben aktiv
- Fehler beendet den aktiven Test sofort
- beim Verlassen alle Ausgaenge AUS
- danach Rueckkehr zu `STANDBY`

## Manuelle Betriebsarten

### Manueller Zeit-/Temperaturlauf

Wird als temporaeres Programm mit Zieltemperatur, Dauer, Sensorbetrieb,
optionalem Vorheizen und Abschlussverhalten behandelt. Er nutzt dieselben
Zustaende und Sicherheitsregeln wie gespeicherte Programme.

### Manueller Temperatur-Haltebetrieb

Verwendet nach optionalem Vorheizen und Zielqualifikation `MANUAL_HOLDING`. Es
handelt sich nicht um direkte Aktorsteuerung.

## Tuerkontakt

Release 1 besitzt keinen Tuerkontakt. Die Zustandsmaschine darf keine
Tuerinformation als vorhandene Sicherheits- oder Prozessbedingung voraussetzen.

## Akzeptierte Entscheidungen

- [x] Bootpruefungen und persistierte Sperren haben Vorrang vor `STANDBY` und Recovery
- [x] `SAFE_BOOT` bleibt aktorfrei
- [x] `COMPLETED` wird nach Neustart explizit wiederhergestellt
- [x] keine allgemeine Pausenfunktion
- [x] Stop bietet Ausschalten oder einen neuen manuellen Kuehllauf
- [x] Warnungen und Fehler sind getrennt
- [x] `SERVICE_MODE` nur aus validiertem `STANDBY`
- [x] Produktfuehlerausfall fuehrt nicht zu stillem Sensorwechsel
- [x] sichere Recovery wartet nicht blockierend auf NTP
- [x] unbekannte Ausfallzeit erzeugt keinen erfundenen exakten Fortschritt
- [x] Ausfallzeit wird nach NTP als Unsicherheitsintervall behandelt
- [x] kein Tuerkontakt in Release 1
