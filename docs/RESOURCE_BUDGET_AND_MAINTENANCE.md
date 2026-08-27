# Ressourcenbudget, Speicherpflege und Wartungsumfang

## Status

Dieses Dokument beschreibt die in Phase 9C akzeptierten Regeln fuer
Ressourcenueberwachung, Speicherbegrenzung, Flashverschleiss, optionale
Betriebszaehler und den Wartungsumfang des ersten Releases.

Die Zielhardware besitzt 4 MB Flash. PSRAM darf nicht vorausgesetzt werden.
Release 1 verwendet UART als verbindlichen Update- und Wiederherstellungsweg und
muss deshalb keine OTA-Slots reservieren.

Der allgemeine Engineering-Grundsatz fuer Ressourcenbudgets ist in
`docs/ENGINEERING_PRINCIPLES.md` kanonisch festgelegt. Dieses Dokument
konkretisiert nur die dort eingeordneten Detailregeln zu Speicherpflege,
Flashverschleiss und Wartungsumfang.

```text
EARLY_SUBSYSTEM_BUDGETS=PLANNING_AND_WARNING_VALUES
HARD_LIMIT_REQUIRES_PRODUCT_OR_HARDWARE_BASIS=YES
HARD_LIMIT_REQUIRES_MEASUREMENT_AND_OWNER_APPROVAL=YES
FINAL_SYSTEM_RESOURCE_QUALIFICATION_AFTER_INTEGRATION=YES
FRAMEWORK_DEFAULT_IS_PRODUCT_BUDGET=NO
```

## Grundsaetze

- Ressourcenknappheit soll durch begrenzte Strukturen, angemessene
  Planungs-/Warnwerte und automatische Bereinigung verhindert werden, nicht
  erst durch spaete Fehlermeldungen behandelt werden.
- Kritische Daten duerfen nie zugunsten von Komfortdaten geloescht werden.
- RAM-Knappheit und Flash-/Dateisystemknappheit werden getrennt bewertet.
- Regelung, Sicherheitslogik und kritische Laufpersistenz haben Vorrang vor
  Historie, Komfortstatistik und Wartungserinnerungen.
- Unbegrenzt wachsende Listen, Protokolle, Diagramme oder Nachrichtenpuffer sind
  unzulaessig.
- Ein vorhandener Zahlenwert darf nicht mit einer behaupteten Bauteillebensdauer
  verwechselt werden.

Der Release-1-SafetyCore hat eine endliche Compile-Time-FaultCode-Menge und
feste bzw. enum-indexierte aktive/acknowledged Faultdaten. Er besitzt keine
unbegrenzt wachsende Fault-Historie und keine dynamischen Faulttexte als
Safety-Wahrheit; Journaling bleibt beim vorhandenen Event-Journal. Kein Fault-
Ereignis darf im Safetykern Heapwachstum pro Ereignis verursachen.

## Ueberwachte Ressourcen

Das erste Release ueberwacht mindestens:

```text
freier Heap
niedrigster freier Heap seit Boot
groesster zusammenhaengender freier Heapblock
Firmwaregroesse
freier Flash-/Dateisystembereich
Belegung der Konfiguration
Belegung der kritischen Laufpersistenz
Belegung des Fehler- und Resetjournals
Belegung der Laufzusammenfassungen
Belegung der Diagrammhistorie
```

Zusaetzlich darf die Diagnose anzeigen:

- Anzahl aktiver Tasks und Warteschlangen, soweit sinnvoll verfuegbar
- groesste konfigurierte Ringpuffer
- Anzahl verworfener nichtkritischer Diagnoseeintraege
- letzte automatische Speicherbereinigung
- Grund einer Ressourcenwarnung
- Partitions- und Schemaversion

Darstellung:

- Das Touchdisplay zeigt nur relevante Warnungen und einen kompakten
  Ressourcenstatus.
- Die Weboberflaeche zeigt die technischen Einzelwerte, Minima und Belegungen.
- Diagnoseexporte enthalten die gleichen Werte in maschinenlesbarer Form.

## Pruefzeitpunkte

Ressourcen werden mindestens geprueft:

1. beim Boot und nach dem passiven Selbsttest
2. beim Start und Abschluss eines Laufes
3. vor und nach kritischen Persistenzvorgaengen
4. vor und nach Exporten oder Importen
5. vor einer gefuehrten Servicepruefung
6. nach einem Speicher-, Watchdog- oder Brownoutereignis
7. zusaetzlich periodisch ungefaehr einmal pro Minute

Die Ressourcenpruefung darf nicht im Zwei-Sekunden-Sensorzyklus schwere
Dateisystem-, JSON- oder Diagnoseoperationen ausloesen.

## Begrenzte Speicherstrukturen

### Regel- und Sicherheitskern

Im Regel-, Sensor-, Aktor- und Sicherheitskern werden feste oder streng
begrenzte Strukturen verwendet:

- begrenzte Sensorfilter
- begrenzter Impulsakkumulator
- feste Fehlerzustandsdaten
- vorab dimensionierte oder fest begrenzte Nachrichtenwarteschlangen
- begrenzte Ringpuffer fuer aktuelle Messwerte und Ereignisse
- keine unbegrenzt wachsenden dynamischen Strings
- keine unkontrollierte Speicherallokation im zeitkritischen Regelpfad
- definierte Fehlerreaktion, wenn ein Puffer seine Kapazitaet erreicht

### Web, JSON und Exporte

Dynamische Speicherverwendung ist dort erlaubt, wo sie praktisch notwendig ist,
aber nur innerhalb klarer Grenzen:

- maximale Programmgroesse
- maximale Anzahl Programme
- maximale Nachrichtengroesse
- maximale gleichzeitige Webanfragen
- maximale Exportgroesse
- streaming- oder blockweise Erzeugung grosser Exporte statt vollstaendiger
  Kopie im RAM
- Zeit- und Speicherlimit fuer Parsing und Validierung

Ein Web- oder Exportfehler darf die lokale Regelung und Sicherheitslogik nicht
beeintraechtigen.

## Proaktive Speicherbereinigung

Die bereits festgelegten Aufbewahrungsregeln werden aktiv durchgesetzt. Dadurch
soll ein voller Historienspeicher im normalen Betrieb gar nicht erst entstehen.

Vor Erreichen einer Warnschwelle werden automatisch und atomar:

1. abgelaufene temporaere Exportdaten entfernt
2. alte niedrig priorisierte Hinweise und Diagnoseereignisse verdichtet oder
   entfernt
3. die aeltesten nichtkritischen Diagrammdaten gemaess Aufbewahrungsgrenze
   entfernt
4. abgeschlossene Laufdetails auf die erhaltene Laufzusammenfassung reduziert,
   sofern deren Detailaufbewahrung abgelaufen ist
5. verwaiste oder nicht aktive Zwischenrevisionen nach erfolgreicher Validierung
   bereinigt

Nicht automatisch geloescht werden:

- aktuelle gueltige Konfiguration
- letzte gueltige Rueckfallrevision
- aktiver Laufschnappschuss
- notwendige aktive Laufkontrollpunkte
- aktive oder verriegelte Fehler
- fuer die Fehlerursache benoetigte kritische Reset- und Sicherheitsereignisse
- unveraenderliche Factory-Daten

Jede Bereinigung verwendet ein festes Speicherbudget und darf nicht selbst durch
wiederholtes Umschreiben unnoetigen Flashverschleiss erzeugen.

## Verhalten bei verbleibender Ressourcenknappheit

Da nichtkritische Daten vorher automatisch bereinigt werden, bedeutet eine
Ressourcenwarnung, dass die normale Speicherpflege die vorgesehene Reserve nicht
wiederherstellen konnte oder dass RAM-Fragmentierung beziehungsweise ein anderer
ungewoehnlicher Zustand vorliegt.

### Warnstufe

Bei einer Warnschwelle:

- automatische Bereinigung erneut gezielt ausfuehren
- Ursache und betroffenen Ressourcenbereich anzeigen
- neue nichtkritische Diagramm- und Detaildaten reduzieren oder aussetzen
- Exporte ablehnen oder kleiner erzeugen, wenn deren sicherer Speicherbedarf
  nicht vorhanden ist
- Prozess weiterlaufen lassen, solange Regelung und kritische Persistenz sicher
  bleiben

### Kritische Stufe vor einem neuen Lauf

Ein neuer Lauf darf nicht gestartet werden, wenn:

- kein sicherer aktiver Laufkontrollpunkt garantiert werden kann
- keine gueltige Konfigurations- und Rueckfallrevision erhalten werden kann
- das Fehlerjournal fuer verriegelte Ereignisse nicht verlaesslich ist
- der freie Heap oder groesste zusammenhaengende Block die nachgewiesene
  Mindestreserve unterschreitet

Die Oberflaeche zeigt den konkreten Grund und moegliche sichere Massnahmen.

### Kritische Stufe waehrend eines laufenden Prozesses

Ein laufender Prozess wird nicht allein wegen einer Komfort- oder
Historienknappheit gestoppt.

Er wird nur sicher gestoppt und verriegelt, wenn:

- Regel- oder Sicherheitsaufgaben nicht mehr verlaesslich ausgefuehrt werden
- kritische Laufpersistenz nicht mehr garantiert ist
- eine notwendige Fehlerrevision nicht mehr sicher gespeichert werden kann
- Speicherbeschaedigung die aktive Konfiguration oder Sicherheitsdaten betrifft

Damit hat die Fortsetzung eines sicher regelbaren Prozesses Vorrang vor alten
Diagrammen, aber nicht vor nachweisbarer Sicherheits- oder Wiederherstellbarkeit.

## Flashbudget und Prioritaeten

Vor der eigentlichen Implementierung wird fuer jede Hauptfunktion ein
angemessener Planungs-/Warnrahmen erstellt. Ein verbindliches Maximalbudget
wird erst aus realer Produkt-/Hardwarebasis, Messung und Ownerfreigabe
festgelegt.

Prioritaetsreihenfolge:

1. Bootloader, Partitionstabelle, Firmware und sichere Bootfaehigkeit
2. Factory-Daten und notwendige Hardwarekonfiguration
3. aktive Konfiguration und letzte gueltige Rueckfallrevision
4. aktiver Lauf, Kontrollpunkte und Wiederherstellungsdaten
5. Fehler-, Brownout-, Watchdog- und Resetjournal
6. notwendige lokale Bedienung und Weboberflaeche
7. abgeschlossene Laufzusammenfassungen
8. detaillierte alte Diagramm- und Komfortdaten
9. optionale Betriebs- und Wartungsstatistiken

Release 1 reserviert keine dualen OTA-Slots. Ein realer Build muss dennoch einen
freien Sicherheitsabstand behalten. Historie darf nicht den gesamten verbleibenden
Flash ausnutzen.

Vor einer Implementierungs- oder Freigabeentscheidung werden die fuer den
jeweiligen Integrationsstand bestimmbaren Ressourcen gemessen:

- Firmwaregroesse des Releasebuilds
- statischer RAM-Verbrauch
- freier Heap nach Boot
- niedrigster Heap bei gleichzeitiger Web-, Display- und Regellast
- groesster zusammenhaengender freier Block
- Flashbelegung bei maximal erlaubter Konfiguration
- Flashbelegung bei maximaler vorgesehener Lauf- und Journalhistorie
- Ressourcenbedarf eines Exports

Fuer noch nicht integrierte Subsysteme, insbesondere Web und Display, bleiben
die entsprechenden Werte bis zur Integration Planungs-/Warnwerte. Die
Messung der integrierten Gesamtlast und die finale Systemqualifikation
erfolgen nach der jeweiligen Integration. Nicht begruendete harte Grenzwerte
bleiben bis zu realer Produkt-/Hardwarebasis, Messung und Ownerfreigabe
`TBD_IMPLEMENTATION_BUDGET`; ein Planungs-/Warnwert ist keine solche harte
Grenze.

## Flashverschleiss

Verbindliche Regeln:

- Sensormessungen im Zwei-Sekunden-Zyklus werden nicht einzeln in den Flash
  geschrieben.
- Wichtige Zustandswechsel und Sicherheitsereignisse werden unmittelbar
  gespeichert.
- Periodische Laufkontrollpunkte werden gemaess dem festgelegten Intervall
  geschrieben.
- Messdaten werden vor dem Schreiben verdichtet.
- Konfiguration, Laufkontrollpunkte und Journal verwenden rotierende,
  versionierte oder wear-levelled Speicherverfahren.
- Wiederholte fehlgeschlagene Schreibversuche werden begrenzt.
- Ungewoehnlich hohe Schreibraten und haeufige Bereinigungen werden diagnostisch
  erfasst.
- Die Firmware behauptet keine exakt verbleibende Flashlebensdauer.

Ein einfacher interner Schreibzaehler darf zur Entwicklung und Diagnose verwendet
werden, ist aber kein kalibrierter Verschleissmesser.

## Betriebs- und Wartungszaehler

### Prioritaet im ersten Release

Umfangreiche Betriebszaehler sind **keine prioritaere Release-1-Funktion**.
Sie werden nur umgesetzt, wenn das gemessene Flash-, RAM- und Schreibbudget nach
Implementierung der Kernfunktionen ausreichende Reserve zeigt.

Moegliche Stufen:

### Stufe A: vollstaendige optionale Zaehler

- Peltier-Heizzeit
- Peltier-Kuehlzeit
- Anzahl Peltierimpulse
- Anzahl Richtungswechsel
- Innenluefterlaufzeit
- Aussenluefterlaufzeit
- maximale und minimale relevante Temperaturen
- Sensorfehler und Bus-Neuinitialisierungen
- Brownouts, Watchdogs und `SAFE_BOOT`
- Anzahl und Ergebnisse von Servicepruefungen

### Stufe B: reduzierte Zaehler

- Gesamtbetriebszeit
- gesamte Peltier-Heiz- und Kuehlzeit
- gesamte Luefterlaufzeit
- Brownout-, Watchdog- und schwere Fehleranzahl

### Stufe C: keine persistenten Komfortzaehler

Nur bereits sicherheits- und fehlerbedingt notwendige Ereignisse werden im
bestehenden Journal gespeichert.

Entscheidungsregel:

```text
Budget nach Kernfunktionen ausreichend
  -> Stufe A oder B

Budget knapp oder Schreibrate unguenstig
  -> Stufe B oder C
```

Keine Stufe darf eine exakte verbleibende Lebensdauer von Peltier, Luefter,
Sensor oder Flash behaupten.

## Wartungserinnerungen

Automatische Wartungserinnerungen sind **nicht Bestandteil des ersten
Releases**.

Release 1 darf Diagnosewerte und optionale Zaehler bereitstellen, zeigt aber
keine periodischen oder nutzungsabhaengigen Wartungsaufforderungen wie:

- Luefter reinigen
- Waermetauscher kontrollieren
- Kondensatwege pruefen
- Sensorkalibrierung erneuern
- Steckverbinder warten
- Temperatursicherung kontrollieren

Diese Funktion bleibt fuer ein Zukunftsrelease vorgemerkt. Vor einer spaeteren
Umsetzung werden Erinnerungskriterien, Quittierung, Verschiebung, Ruecksetzung
und Abgrenzung zu echten Sicherheitsfehlern separat spezifiziert.

Ein tatsaechlich erkannter Fehler oder eine bestehende Sicherheitsursache wird
weiterhin sofort gemeldet und ist keine Wartungserinnerung.

## Akzeptierte Entscheidungen aus Phase 9C

- [x] wesentliche Heap-, Flash-, Firmware- und Speicherbelegungswerte ueberwachen
- [x] Ressourcenpruefung beim Boot, bei wichtigen Ereignissen und ungefaehr einmal
      pro Minute
- [x] feste oder begrenzte Strukturen im Regel- und Sicherheitskern
- [x] dynamische Web- und JSON-Nutzung nur mit festen Grenzen
- [x] alte nichtkritische Protokolle und Diagrammdaten proaktiv automatisch
      bereinigen
- [x] Ressourcenwarnung erst, wenn normale Speicherpflege die Reserve nicht
      wiederherstellt
- [x] laufenden Prozess bei reiner Historienknappheit nicht stoppen
- [x] neuen Lauf bei gefaehrdeter kritischer Persistenz nicht starten
- [x] Firmware, aktive Konfiguration, Laufwiederherstellung und Fehlerjournal vor
      Komforthistorie priorisieren
- [x] keine OTA-Slots im Release-1-Flashbudget
- [x] keine Einzelmessung im Zwei-Sekunden-Zyklus in Flash schreiben
- [x] rotierende beziehungsweise wear-levelled Speicherung und begrenzte
      Schreibversuche
- [x] Betriebszaehler nur nach gemessenem Ressourcenbudget und gegebenenfalls auf
      reduzierte oder keine Komfortzaehler herabstufen
- [x] keine automatische Wartungserinnerung im ersten Release
- [x] Wartungserinnerungen als Zukunftsrelease vorgemerkt

## Noch offen fuer Phase 10 und Implementierung

- konkreter 4-MB-Partitionsplan fuer Release 1
- maximale Firmwaregroesse und freier Sicherheitsabstand
- konkrete RAM-Budgets je Hauptmodul
- Warn- und kritische Schwellen fuer Heap, groessten Block und Flash
- genaue Maximalgroessen fuer Programme, Nachrichten, Exporte und Ringpuffer
- konkrete Groesse von Laufpersistenz, Journal und Historienbereichen
- Bereinigungs- und Verdichtungsalgorithmen mit Stromausfalltests
- Wahl von Betriebszaehler-Stufe A, B oder C nach realer Ressourcenmessung
- Langzeit- und Fragmentierungstest mit Web, Display, Regelung und Export
- Schreiblast- und Wear-Leveling-Test
- spaetere Spezifikation der Wartungserinnerungen
