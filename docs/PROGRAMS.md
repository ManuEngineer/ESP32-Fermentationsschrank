# Programme und Prozessablauf

## Status

Dieses Dokument beschreibt den akzeptierten Grundaufbau der Programme. Die
konkreten Temperaturen, Zeiten und Grenzwerte der vier Standardprogramme werden
im naechsten Schritt festgelegt.

## Grundsaetze

- Das erste Release enthaelt vier vorbereitete Standardprogramme.
- Weitere Programme koennen dynamisch angelegt werden.
- Standardprogramme sind bearbeitbar und auf ihre Werkseinstellungen
  zuruecksetzbar.
- Das erste Release zeigt eine einfache Bedienoberflaeche mit einer
  Fermentationstemperatur.
- Das interne Modell wird bereits als Folge von Prozessphasen gedacht, damit
  spaetere Programme mit mehreren Temperaturstufen nicht verhindert werden.
- Ein gestartetes Programm verwendet einen unveraenderlichen Schnappschuss
  seiner Startparameter.

## Temperaturfuehrung und Sensorwahl

Es gibt zwei zulaessige Betriebsarten.

### Produktgefuehrter Betrieb

Wenn ein Produktfuehler angeschlossen und fuer den Lauf ausgewaehlt ist:

- Der Produktfuehler ist der primaere Prozesssensor.
- Der Luftfuehler unterstuetzt die Regelung und dient fuer schnelle Reaktionen,
  Plausibilitaetskontrolle sowie Temperaturgrenzen im Schrank.
- Die Zielqualifikation und der Start der Fermentationszeit richten sich nach
  dem Produktfuehler.
- Ein Ausfall des Produktfuehlers waehrend eines produktgefuehrten Laufs darf
  nicht unbemerkt auf Luftregelung umschalten. Die konkrete Fehlerreaktion wird
  in `SAFETY_AND_FAULTS.md` festgelegt.

### Luftgefuehrter Betrieb

Wenn kein Produktfuehler verwendet wird:

- Der Luftfuehler ist der primaere Prozesssensor.
- Die Zielqualifikation und der Start der Fermentationszeit richten sich nach
  der Lufttemperatur.
- Die Software zeigt deutlich an, dass die Produkttemperatur nicht direkt
  gemessen wird.
- Luftgefuehrter Betrieb ist ein normaler, unterstuetzter Betriebsmodus und kein
  Fehlerzustand.

### Auswahl beim Programmstart

Vor dem Start zeigt die Bedienoberflaeche den erkannten Sensorstatus und den
vorgesehenen Regelmodus an.

- Ist ein Produktfuehler vorhanden, wird produktgefuehrter Betrieb angeboten
  beziehungsweise vorausgewaehlt.
- Ist kein Produktfuehler vorhanden, wird luftgefuehrter Betrieb verwendet.
- Ein Wechsel des Regelmodus erfolgt nicht still oder unbemerkt.
- Der Benutzer bestaetigt den Regelmodus zusammen mit den Startparametern.

## Abnehmbarer Produktfuehler

Der Produktfuehler soll abnehmbar ausgefuehrt werden, damit der Schrank je nach
Anwendung mit oder ohne Produktmessung betrieben werden kann.

Der konkrete Steckverbinder ist noch offen. Ein normaler 3,5-mm-Klinkenstecker
wird nicht als Standard festgelegt, weil beim Ein- und Ausstecken Kontakte
kurzzeitig ueberbrueckt werden koennen. Zu pruefen sind insbesondere:

- verriegelbarer dreipoliger Steckverbinder
- Schutz gegen Feuchtigkeit und Kondensat
- eindeutige Polung
- Reinigbarkeit
- sichere elektrische Trennung beim Ein- und Ausstecken

Moegliche Kandidaten sind ein dreipoliger M8-Steckverbinder, ein GX12-3 oder
ein anderer verpolungssicherer dreipoliger Steckverbinder. Die endgueltige
Auswahl ist eine Hardwareentscheidung.

Ein Produktfuehler darf nur direkt mit Lebensmitteln in Kontakt kommen, wenn
Sonde, Kabeluebergang und Reinigungsverfahren fuer diesen Einsatz geeignet
sind. Andernfalls wird er nur in einer geeigneten geschlossenen Referenz oder
ueber eine andere definierte thermische Kopplung verwendet.

## Vorheizen

Vorheizen ist pro Programmlauf ein- oder ausschaltbar.

### Lauf ohne Vorheizen

```text
Programm auswaehlen
  -> Startparameter und Sensorbetrieb bestaetigen
  -> Zieltemperatur erreichen
  -> Zieltemperatur qualifizieren
  -> Fermentationszeit
  -> Abschlussphase
```

Das Produkt befindet sich bereits beim ersten Start im Schrank.

### Lauf mit Vorheizen

```text
Programm auswaehlen
  -> Vorheizen aktivieren
  -> erster Start
  -> Schrank anhand des Luftfuehlers vorheizen oder vorkuehlen
  -> Zieltemperatur des leeren Schranks qualifizieren
  -> Temperatur halten
  -> Anzeige und Signal: Produkt einsetzen
  -> Benutzer setzt Produkt ein
  -> zweiter Start / Weiter
  -> Zieltemperatur fuer den gewaehlten Regelmodus qualifizieren
  -> Fermentationszeit
  -> Abschlussphase
```

Der zweite Tastendruck ist eine bewusste Bestaetigung, dass das Produkt
eingesetzt wurde. Ohne diese Bestaetigung beginnt die Fermentationszeit nicht.

Nach dem Einsetzen kann die Temperatur durch geoeffnete Tuer und Produktmasse
abweichen. Deshalb wird die Zieltemperatur nach dem zweiten Start erneut
qualifiziert.

## Zieltemperatur erreichen

`Zieltemperatur erreichen` kann je nach Ausgangslage bedeuten:

- heizen
- kuehlen
- Temperatur bereits im Zielbereich halten

Die Regelung entscheidet aus Messwerten und Zielwert, welche Richtung benoetigt
wird. Heizen und Kuehlen bleiben elektrisch und logisch gegenseitig verriegelt.

## Zielqualifikation

Die bisher verwendete Bezeichnung `Stabilisierung` wird genauer als
**Zielqualifikation** bezeichnet.

Die Zielqualifikation ist **nicht** Teil der Fermentationszeit. Sie ist eine
kurze Pruefphase unmittelbar davor und verhindert, dass der Timer bereits bei
einem einmaligen kurzen Durchqueren des Zielbereichs startet.

Ein Ziel gilt als qualifiziert, wenn:

- der fuer den Lauf massgebende Sensor im Zielband liegt
- dieser Zustand fuer eine festgelegte Qualifikationsdauer ausreichend stabil
  war
- kein relevanter Sensor- oder Sicherheitsfehler vorliegt

Kurze einzelne Abweichungen sollen die Qualifikation nicht zwingend komplett
zuruecksetzen. Dafuer werden spaeter eine kleine Ausreisser- oder Gnadenzeit
und Plausibilitaetsregeln definiert. Laengere oder deutliche Abweichungen
unterbrechen oder starten die Qualifikation neu.

Die genaue Zielbandbreite, Qualifikationsdauer und Gnadenzeit werden pro
Programm oder als validierte Standardwerte festgelegt.

## Start der Fermentationszeit

Die Fermentationszeit startet erst, nachdem die Zielqualifikation erfolgreich
abgeschlossen wurde.

Nicht zur Fermentationszeit zaehlen:

- Vorheizen oder Vorkuehlen des leeren Schranks
- Warten auf das Einsetzen des Produkts
- erneutes Erreichen der Zieltemperatur nach dem Einsetzen
- Zielqualifikation

## Temperaturabweichungen waehrend der Fermentation

Die Regelung soll die Temperatur im vorgesehenen Arbeitsbereich halten.
Trotzdem koennen kurze Abweichungen durch Regeltraegheit, Tueröffnung oder
Messrauschen auftreten.

Standardverhalten im ersten Release:

- Die Fermentationszeit laeuft bei kurzen oder moderaten Abweichungen weiter.
- Abweichungen werden protokolliert.
- Eine deutliche oder laenger andauernde Abweichung erzeugt eine Warnung.
- Harte Sicherheitsgrenzen fuehren unabhaengig vom Programm zur sicheren
  Abschaltung oder zum Fehlerzustand.

Das Datenmodell darf pro Programm spaeter unterschiedliche Reaktionen erlauben,
beispielsweise:

- Timer weiterlaufen lassen
- Timer nach einer Gnadenzeit pausieren
- Benutzerentscheidung verlangen
- Programm wegen Prozessabweichung beenden

Fuer die vier ersten Standardprogramme wird im naechsten Schritt festgelegt,
ob sie vom Standardverhalten abweichen.

## Maximale Zeit zum Erreichen der Zieltemperatur

Jedes Programm besitzt eine maximale erwartete Zeit fuer das Erreichen und
Qualifizieren der Zieltemperatur.

Wird diese Zeit ueberschritten:

- das Programm erzeugt eine sichtbare und akustische Warnung
- die Regelung versucht standardmaessig weiter, sofern kein Sicherheitsfehler
  vorliegt
- der Benutzer kann den Lauf fortsetzen beobachten oder abbrechen
- das Ereignis wird mit Zeit und Temperaturen protokolliert

Das Ueberschreiten ist damit zunaechst eine Prozesswarnung und nicht automatisch
ein harter Fehler. Harte Grenzen, Sensorfehler oder unplausibles Verhalten
werden separat behandelt.

## Akustische Signale

Fuer wichtige lokale Meldungen wird ein Pieper beziehungsweise Summer als
zusaetzliche Hardware vorgesehen.

Mindestens zu signalisierende Ereignisse:

- Vorheizen beendet; Produkt einsetzen
- maximale Zielerreichungszeit ueberschritten
- Benutzerentscheidung erforderlich
- Programm beendet
- Fehler oder Sicherheitsabschaltung

Akustische Signale ersetzen nie die Anzeige. Sie muessen quittierbar sein und
duerfen bei einer laenger anstehenden Meldung nicht ununterbrochen laufen.

Bevorzugte Hardware fuer die erste Ausbaustufe ist ein aktiver 5-V- oder
12-V-Summer, der ueber einen noch zu bestaetigenden freien MOSFET-Kanal oder
eine geeignete Treiberstufe geschaltet wird. Spannung, Stromaufnahme,
Lautstaerke und Kanalzuordnung sind noch offen.

## Abschlussphase

Das Verhalten nach der Fermentation wird pro Programm festgelegt. Zulaessige
Varianten sind:

1. ohne aktive Kuehlung beenden
2. bis zur Kuehlzieltemperatur kuehlen und beenden
3. bis zur Kuehlzieltemperatur kuehlen und fuer eine festgelegte Dauer halten
4. bis zur Kuehlzieltemperatur kuehlen und bis zur manuellen Beendigung halten

Der Status muss klar zwischen diesen Phasen unterscheiden:

- Fermentation abgeschlossen
- aktives Herunterkuehlen
- Kuehlhalten
- vollstaendig beendet

## Phasenmodell

Ein Programm wird konzeptionell als geordnete Folge von Phasen betrachtet.
Die erste Oberflaeche bildet diese Phasen mit einfachen Feldern ab.

```text
Programm
├── optionale Vorheizphase
├── Zieltemperatur erreichen
├── Zielqualifikation
├── Fermentationsphase
├── optionale Kuehlphase
└── optionale Haltephase
```

Spaetere Releases koennen mehrere temperaturgefuehrte Fermentationsphasen
ermoeglichen, beispielsweise:

```text
6 Stunden bei Temperatur A
  -> 2 Stunden bei Temperatur B
  -> auf Temperatur C herunterkuehlen
```

Das erste Release implementiert jedoch nur eine Fermentationstemperatur pro
Programm.

## Standardprogramme und Werkseinstellungen

Die vier Standardprogramme sind:

1. Joghurt mild
2. Joghurt stichfest
3. Milchkefir
4. Wasserkefir

Sie duerfen bearbeitet werden. Fuer jedes Standardprogramm bleibt eine
unveraenderliche Werkseinstellung verfuegbar, auf die der Benutzer es
zuruecksetzen kann.

Benutzerprogramme koennen angelegt, kopiert, umbenannt und geloescht werden.
Die genaue Bedienung und maximale Anzahl werden spaeter festgelegt.

## Bereits akzeptierte Entscheidungen

- [x] Produktfuehler vorhanden: Produktfuehler primaer, Luftfuehler fuer
      Unterstuetzung und Sicherheit
- [x] Produktfuehler nicht vorhanden: Luftfuehler primaer
- [x] Regelmodus wird vor dem Start sichtbar bestaetigt
- [x] abnehmbarer Produktfuehler vorgesehen; Steckverbinder noch offen
- [x] Zielqualifikation ist getrennt von der Fermentationszeit
- [x] kurze Ausreisser duerfen innerhalb definierter Grenzen ignoriert werden
- [x] Fermentationszeit startet erst nach Zielqualifikation
- [x] Timer laeuft bei normalen Abweichungen standardmaessig weiter
- [x] abweichende Reaktion kann spaeter pro Programm konfigurierbar sein
- [x] maximale Zielerreichungszeit pro Programm
- [x] Zeitueberschreitung erzeugt Warnung und versucht standardmaessig weiter
- [x] lokaler akustischer Signalgeber vorgesehen
- [x] Vorheizen pro Lauf ein- oder ausschaltbar
- [x] nach Vorheizen zweiter Start zur Bestaetigung des eingesetzten Produkts
- [x] Standardprogramme bearbeitbar und auf Werkseinstellung ruecksetzbar
- [x] internes Phasenmodell, erste Oberflaeche bleibt einfach

## Offene Entscheidungen fuer Phase 2B

- Zieltemperatur und Standarddauer jedes Standardprogramms
- Standardregelmodus jedes Programms
- Standardwert fuer Vorheizen je Programm
- Zielband fuer Zielqualifikation und Regelung
- Qualifikationsdauer und erlaubte kurze Ausreisser
- maximale Zeit zum Erreichen der Zieltemperatur je Programm
- Warnschwellen bei Temperaturabweichungen waehrend der Fermentation
- Kuehlziel und Abschlussverhalten je Programm
- Standardlautstaerke beziehungsweise Signalmuster des Summers
- endgueltiger Steckverbinder fuer den abnehmbaren Produktfuehler
