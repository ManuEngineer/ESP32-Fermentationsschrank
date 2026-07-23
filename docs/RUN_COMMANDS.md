# Laufkommandos, Meldungen und Bedienaktionen

## Status

Dieses Dokument legt die fuer Issue #15 akzeptierte fachliche Semantik fuer
Laufkommandos, Meldungen und konkurrierende Bedienaktionen fest.

Es ergaenzt insbesondere:

- [`STATE_MACHINE.md`](STATE_MACHINE.md)
- [`PROGRAMS.md`](PROGRAMS.md)
- [`RUNTIME_BEHAVIOR.md`](RUNTIME_BEHAVIOR.md)
- [`LOCAL_RUNTIME_UI.md`](LOCAL_RUNTIME_UI.md)
- [`TEMPERATURE_CONTROL.md`](TEMPERATURE_CONTROL.md)
- [`SAFETY_AND_FAULTS.md`](SAFETY_AND_FAULTS.md)

## Architekturgrenze

Die Kommandoschicht ist Teil von `lib/fermentation_app/`, weil sie konkrete
Fermentationslaeufe, Prozesszustaende, Laufanpassungen und Bedienaktionen kennt.

Sie bleibt:

- hardwareunabhaengig
- persistenzfrei
- unabhaengig von Display, Web, WLAN und Arduino
- im Profil `native` deterministisch testbar

Display und Web sind spaeter nur Adapter derselben fachlichen Kommandoschicht.
Es entsteht kein allgemeines Command-Framework in `device_platform`.

Die Kommandoschicht erzeugt weder GPIO-Pegel noch direkte Aktorfreigaben. Sie
liefert ausschliesslich fachliche Entscheidungen, Meldungen und abstrakte
Wirkungsabsichten.

## Zweistufiges Entscheidungsmodell

Alle lauf-, meldungs- oder verriegelungswirksamen Kommandos werden zweistufig
verarbeitet:

```text
aktuellen fachlichen Stand und Kommando validieren
  -> noch nicht angewendete CommandDecision erzeugen
  -> spaeter in #17 Transaktionsabsicht und neue Revision atomar speichern
  -> Entscheidung bestaetigen und anwenden
  -> erst danach neue Aktoranforderungen ableiten
```

Die Entscheidungsfunktion veraendert weder Laufzustand, Laufrevisionen,
Meldungszustand noch Verriegelungen.

Eine Entscheidung darf nur angewendet werden, wenn:

- ihr erwarteter Ausgangszustand noch aktuell ist
- die erwarteten Sequenzen und Revisionen noch uebereinstimmen
- sie nicht bereits verworfen oder angewendet wurde
- der zugehoerige Lauf- oder Fehlerkontext weiterhin gueltig ist

Schlaegt die spaetere Persistenz fehl, bleibt der bisherige fachliche Zustand
wirksam. Es entsteht keine neue Aktorfreigabe.

Issue #15 implementiert noch keine produktive Persistenz. Native Tests duerfen
einen einfachen In-Memory-Treiber verwenden, der Entscheidungen bewusst
verwirft oder bestaetigt.

## Gemeinsamer Kommando-Umschlag

Ein fachliches Kommando besitzt mindestens:

- eine eindeutige Kommando-ID
- die Quelle, mindestens lokales Display oder Web
- einen monotonen Zeitbezug
- die erwartete Zustandssequenz
- soweit relevant die erwartete Laufrevision
- soweit relevant die erwartete Meldungs- oder Fehlerrevision
- die fachlichen Eingabedaten
- bei kritischen Benutzeraktionen eine ausdrueckliche Bestaetigung

Die konkrete C++-Benennung bleibt Implementierungsdetail. Die Semantik dieser
Felder ist verbindlich.

## Konflikte und Idempotenz

Lokales Display und Web sind fachlich gleichberechtigt. Es gibt keine pauschale
Quellenprioritaet.

Verbindliche Regeln:

1. Das erste gueltig angewendete Kommando gewinnt.
2. Ein weiteres Kommando, das auf einer veralteten Zustands-, Lauf-, Meldungs-
   oder Fehlerrevision basiert, wird eindeutig als Konflikt beziehungsweise
   veralteter Stand abgelehnt.
3. Ein abgelehntes Kommando veraendert keine fachlichen Daten.
4. Eine bereits verarbeitete Kommando-ID wird nicht nochmals ausgefuehrt.
5. Eine Wiederholung derselben Kommando-ID liefert dasselbe fachliche Ergebnis
   beziehungsweise einen eindeutig als Wiederholung erkennbaren Erfolg.
6. Sicherheitsereignisse sind keine konkurrierenden Komfortkommandos. Eine
   kritische Sicherheitsentscheidung hat immer Vorrang.
7. Transportwiederholungen, Anmeldung und konkrete Web-Protokolle folgen in
   Issue #27. Issue #15 definiert nur die fachliche Semantik.

Mindestens unterscheidbare Ergebnisarten sind:

- vorgeschlagen
- angewendet
- keine Aenderung
- nicht bestaetigt
- im aktuellen Zustand unzulaessig
- ungueltige Eingabe
- Konflikt oder veralteter Stand
- bereits verarbeitet
- Sicherheitsfreigabe fehlt
- Kapazitaet oder erforderlicher Kontext fehlt

## Bestaetigter Laufstart

Vor jedem normalen Start wird aus den validierten Eingaben eine
Startzusammenfassung erzeugt. Sie enthaelt mindestens die in
`LOCAL_UI.md` und `LOCAL_RUNTIME_UI.md` festgelegten Angaben.

Erst das bestaetigte Startkommando darf:

- einen unveraenderlichen Laufschnappschuss beziehungsweise manuellen Laufplan
  vorschlagen
- den fachlichen Startuebergang der Zustandsmaschine anfordern
- die fuer die spaetere Persistenz benoetigten Ereignisse liefern

Fehlende oder ungueltige Pflichtwerte, ein veralteter Katalogstand, ein
unzulaessiger Prozesszustand oder eine fehlende Sicherheitsfreigabe fuehren zu
einer Ablehnung ohne Teilwirkung.

## Manueller Laufplan

Manuelles Temperaturhalten und `Abbrechen und kuehlen` verwenden denselben
unveraenderlichen manuellen Laufplan.

Der Laufplan enthaelt mindestens:

- eindeutige Lauf-ID
- Zieltemperatur
- ausgewaehlten Sensorbetrieb
- Vorheizen EIN oder AUS
- maximale Produktwartezeit nur bei Vorheizen
- Zielband und Qualifikationsdauer
- Laufart `ManualHolding`
- Quelle und Erstellungszeit

Er enthaelt keine Fermentationsdauer. Der Lauf endet durch eine bewusste
Benutzeraktion oder einen Fehler.

Der kanonische Zustandsweg bleibt:

```text
mit Vorheizen:
  STANDBY -> PREHEATING -> WAITING_FOR_PRODUCT
          -> REACHING_TARGET -> QUALIFYING_TARGET -> MANUAL_HOLDING

ohne Vorheizen:
  STANDBY -> REACHING_TARGET -> QUALIFYING_TARGET -> MANUAL_HOLDING
```

Ob zur Zielerreichung geheizt oder gekuehlt wird, entscheidet spaeter die
Regel- und Sicherheitslogik aus aktuellem Messwert und Zielwert. Es wird kein
eigener Zustand `MANUAL_COOLING` eingefuehrt.

## Stoppen und Abbrechen

`STOP` selbst oeffnet nur den bereits spezifizierten Auswahl- und
Bestaetigungsdialog.

### Zurueck

`Zurueck` verwirft den Dialog. Lauf, Meldungen, Zustandsmaschine und
Aktoranforderungen bleiben unveraendert.

### Abbrechen und ausschalten

Die Entscheidung schlaegt atomar vor:

- den aktuellen Lauf als abgebrochen zu markieren
- den Benutzerabbruch als fachliches Ereignis zu erzeugen
- den vorgesehenen Uebergang nach `STANDBY` anzufordern
- eine abstrakte Absicht fuer sicheren Peltier-Stopp und erforderlichen
  Luefternachlauf bereitzustellen

Die konkrete Aktor- und Nachlaufsteuerung bleibt ausserhalb von Issue #15.

### Abbrechen und kuehlen

Der Benutzer bestaetigt vorab mindestens Kuehlziel und Halteverhalten.

Die CommandDecision bildet eine unteilbare fachliche Gesamtaktion:

1. bisherigen Programmlauf als abgebrochen markieren
2. Benutzerabbruch protokollierbar machen
3. neuen validierten manuellen Laufplan mit eigenem Schnappschuss erzeugen
4. den neuen Lauf ueber `REACHING_TARGET` beziehungsweise den kanonischen
   manuellen Zustandsweg starten

Der alte Lauf darf nicht bereits beendet werden, wenn der neue manuelle Laufplan
ungueltig ist oder nicht erzeugt werden kann. Eine spaetere Persistenz muss beide
Teile als zusammengehoerige atomare Revision behandeln.

## Laufanpassungen

Zieltemperatur und verbleibende Dauer werden nur ueber eine ausdrueckliche
Laufaktion mit Vorschau und Bestaetigung geaendert.

Die Vorschau zeigt mindestens:

- alten Wert
- neuen Wert
- betroffene Laufphase
- erkennbare fachliche Auswirkung
- Hinweis auf erneute Zielqualifikation, soweit erforderlich
- Hinweis, wenn keine automatische biologische Zeitkorrektur erfolgt

Der Programmschnappschuss und das gespeicherte Quellprogramm bleiben
unveraendert. Die bestaetigte Aenderung erzeugt eine protokollierbare
append-only Laufrevision mit Quelle und Zeitbezug.

### Zieltemperatur vor der Fermentationsphase

Bei einer Zieltemperaturaenderung in `PREHEATING`, `REACHING_TARGET` oder
`QUALIFYING_TARGET` wird die Zielerreichung beziehungsweise Zielqualifikation
mit dem neuen Zielwert neu bewertet. Bereits teilweise absolvierte
Qualifikationszeit wird nicht auf das neue Ziel uebertragen.

### Zieltemperatur waehrend `FERMENTING`

Eine bestaetigte Zieltemperaturaenderung waehrend `FERMENTING`:

- laesst den Prozesszustand in `FERMENTING`
- laesst die verbleibende Dauer ohne Unterbrechung weiterlaufen
- setzt den neuen Sollwert erst nach Anwendung der CommandDecision in Kraft
- erzeugt keine blockierende erneute Zielqualifikation
- berechnet keine automatische Laufzeitkorrektur

Die Vorschau weist ausdruecklich darauf hin, dass der biologische Prozess
weiterlaeuft und Release 1 aus der Sollwertaenderung keine erfundene
biologische Zeitkorrektur ableitet.

Eine spaetere temperaturgewichtete Fortschrittskorrektur bleibt Issue #18 und
der Inbetriebnahme vorbehalten.

### Reine Restdaueranpassung

Eine reine Aenderung der verbleibenden Dauer loest keine neue
Zielqualifikation aus. Die Restdauer darf auf null gesetzt werden, sofern der
aktuelle Zustand und die bestehende Laufsemantik einen unmittelbaren Abschluss
zulassen.

Bereits abgeschlossene Phasen werden nicht rueckwirkend umgedeutet.

### Nicht passende Phasen

Aenderungen von Fermentationsziel oder Fermentationsrestzeit werden in
`COOLING`, `COOL_HOLDING`, `MANUAL_HOLDING`, `COMPLETED`, `FAULT`,
`SAFE_BOOT`, `RECOVERY_EVALUATION` und `SERVICE_MODE` abgelehnt.

Eine spaetere eigene Zielwertanpassung fuer einen bereits laufenden manuellen
Haltebetrieb ist nicht Bestandteil von Issue #15. Der manuelle Lauf kann sicher
beendet und mit einem neuen bestaetigten Laufplan gestartet werden.

## Abschluss

`COMPLETED` bleibt ein fachlich abgeschlossener Lauf, der auf eine bewusste
Quittierung wartet.

`AcknowledgeCompletion` darf nur aus `COMPLETED` entschieden werden. Nach
spaeterer erfolgreicher Persistenz fuehrt seine Anwendung nach `STANDBY`.

`Jetzt kuehlen` erzeugt wie `Abbrechen und kuehlen` einen neuen manuellen Lauf
mit eigenem Laufplan. Der abgeschlossene Ursprungslauf bleibt unveraendert als
abgeschlossen protokolliert.

## Meldungsmodell

Issue #15 definiert fachliche Meldungen und akustische Absichten, keine
konkreten Displaytexte, Summerfrequenzen oder GPIO-Signale.

Eine Meldung besitzt mindestens:

- stabilen maschinenlesbaren Meldungscode
- Meldungsklasse und Prioritaet
- Quelle beziehungsweise fachlichen Ausloeser
- monotonen Zeitbezug
- Aktiv-, quittiert- und erledigt-Status
- Angabe, ob eine Benutzerentscheidung erforderlich ist
- abstrakte akustische Absicht
- soweit relevant eine Lauf-, Zustands- oder Fehlerreferenz

Die Prioritaetsreihenfolge aus `RUNTIME_BEHAVIOR.md` bleibt verbindlich.
Niedrigere Meldungen werden nicht geloescht, nur weil eine hoehere prominent
angezeigt wird.

## Quittieren und Stummschalten

`Quittieren` und `Stummschalten` sind getrennte Kommandos.

`Quittieren`:

- bestaetigt nur die Wahrnehmung
- veraendert die zugrunde liegende Ursache nicht
- loest keine Verriegelung
- entfernt keine weiterhin aktive Warnung oder Fehlerursache
- erzeugt eine protokollierbare Meldungsrevision

`Stummschalten`:

- beendet beziehungsweise unterdrueckt nur die aktuell zulaessige akustische
  Wiederholung
- veraendert sichtbare Meldung, Ursache und Verriegelung nicht
- kann durch eine neue Meldung hoeherer Prioritaet ueberstimmt werden

Eine Quittierung der prominentesten Meldung quittiert nicht automatisch andere
aktive Meldungen.

## Fehler zuruecksetzen

Issue #15 definiert das fachliche Kommando `Fehler zuruecksetzen` und dessen
Ergebnisstruktur, aber keine konkrete Fehlercode-, Sensor-, Hardware- oder
Berechtigungspolitik.

Das Kommando verwendet eine aktuelle, bereits qualifizierte
Resetfreigabebewertung der spaeteren Sicherheitslogik. Diese Bewertung drueckt
mindestens aus:

- ob der Reset erlaubt ist
- ob die Ursache weiterhin aktiv ist
- ob erforderliche Sicherheitspruefungen bestanden sind
- ob die erforderliche Berechtigung erfuellt ist
- auf welche Fehlerrevision sich die Bewertung bezieht
- einen stabilen Ablehnungsgrund

Ohne positive und noch aktuelle Resetfreigabe wird das Kommando abgelehnt.

Issue #15:

- liest keine realen Sensoren oder GPIOs
- kennt keine konkrete Service-PIN
- erfindet keine Fehlercode-Matrix
- loest niemals durch `Quittieren` eine Verriegelung

Die Erzeugung der Resetfreigabebewertung und die vollstaendige Resetpolitik
bleiben Issue #24. Die atomare Speicherung einer erfolgreichen Resetentscheidung
bleibt Issue #17.

## Tests fuer Issue #15

Native Tests decken mindestens ab:

- bestaetigten und nicht bestaetigten Laufstart
- ungueltige und veraltete Startdaten
- alle Stopoptionen einschliesslich vollstaendig atomarem `Abbrechen und kuehlen`
- manuellen Laufplan mit und ohne Vorheizen
- Berechnen, Verwerfen und Anwenden jeder laufwirksamen Entscheidung
- Konflikte zwischen Display und Web ohne Quellenprioritaet
- Wiederholung derselben Kommando-ID
- veraltete Zustands-, Lauf-, Meldungs- und Fehlerrevisionen
- Zieltemperaturaenderung vor und waehrend `FERMENTING`
- weiterlaufende Restdauer bei Zielaenderung waehrend `FERMENTING`
- Restdaueranpassung in zulaessigen und unzulaessigen Phasen
- Quittieren, Stummschalten und Fehlerreset als getrennte Aktionen
- erlaubte und abgelehnte deterministische Resetfreigabebewertungen
- keine direkte Persistenz, Hardwarewirkung oder Abhaengigkeit von Display/Web

## Abgrenzung zu Folge-Issues

- #17 implementiert atomare Laufpersistenz, Kontrollpunkte und
  Persistenz-vor-Anwendung.
- #18 implementiert Wiederanlauf und temperaturgewichteten Fortschritt.
- #24 implementiert Fehlerklassen, Verriegelungen und die konkrete
  Fehlerresetfreigabe.
- #25 und #26 implementieren gemeinsame UI-Modelle und die lokale Oberflaeche.
- #27 implementiert Webtransport, Anmeldung und konkrete Webkonfliktbehandlung.

Issue #15 darf diese Folge-Issues nicht mit provisorischen Hardware-,
Persistenz-, Sicherheits- oder Transportimplementierungen vorwegnehmen.
