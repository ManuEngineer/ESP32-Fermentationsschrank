# Laufzeitverhalten von Lueftern und Meldungen

## Status

Dieses Dokument ergaenzt [`STATE_MACHINE.md`](STATE_MACHINE.md) um die in
Phase 3D akzeptierten Regeln fuer Luefternachlauf, Richtungswechsel,
Meldungsprioritaet und akustische Signale.

Die exakten Nachlaufzeiten, Wiederholungsintervalle und Grenzwerte bleiben bis
zur Inbetriebnahme beziehungsweise bis zu den Phasen
`TEMPERATURE_CONTROL.md` und `SAFETY_AND_FAULTS.md` offen.

## Issue #24 Release-1-Meldungs- und Safety-Grenze

Die stabile R1-FaultCode- und Lifecycle-Matrix wird durch SafetyCore und
`SAFETY_AND_FAULTS.md` vorgegeben. Dieses Dokument liefert keine zweite
Fehlerklassifikation. Ack quittiert nur Anzeige/Akustik; Safety-Gate, Fault-
Clear und `Allowed` bleiben davon unberuehrt. Journal- oder
Notificationfehler duerfen eine Abschaltung nie verhindern.

## Rollen der Luefter

### Aussenluefter

Der Aussenluefter dient der thermischen Beherrschung der Peltier-Aussenseite und
des Kuehlkoerpers.

Verbindliche Regeln:

- Er laeuft waehrend jedes Heiz- oder Kuehlbetriebs des Peltiers.
- Er laeuft nach jeder Peltier-Aktivierung zwingend fuer eine definierte Zeit
  nach.
- Er darf nicht gleichzeitig mit dem Peltier abgeschaltet werden, wenn am
  Kuehlkoerper noch relevante Restwaerme vorhanden sein kann.
- Ein Fehler des Aussenluefters kann je nach Betriebszustand ein harter Fehler
  sein und muss spaeter in `SAFETY_AND_FAULTS.md` klassifiziert werden.

### Innenluefter

Der Innenluefter sorgt fuer eine gleichmaessige Schranklufttemperatur.

Verbindliche Regeln:

- Sein Betrieb ist zustandsabhaengig.
- Er laeuft waehrend aktiver Temperaturregelung normalerweise mit.
- Er kann nach dem Peltierbetrieb fuer eine eigene, zustandsabhaengige Zeit
  nachlaufen.
- In `STANDBY` ist er standardmaessig AUS.
- In `WAITING_FOR_PRODUCT`, `FERMENTING`, `COOLING`, `COOL_HOLDING` und
  `MANUAL_HOLDING` darf sein Verhalten unterschiedlich parametriert werden.

Die exakten Nachlaufzeiten beider Luefter bleiben `TBD_COMMISSIONING`.

## Richtungswechsel zwischen Heizen und Kuehlen

Ein Wechsel der Peltier-Richtung erfolgt niemals direkt.

Vorgesehene Sequenz:

```text
aktuelle Peltier-Richtung deaktivieren
  -> Peltierleistung sicher AUS
  -> vorgeschriebene Totzeit abwarten
  -> Gegenrichtung freigeben
```

Waehrend der Totzeit:

- bleibt das Peltier AUS
- laeuft der Aussenluefter weiter
- laeuft der Innenluefter weiter, sofern der aktuelle Zustand ihn vorsieht
- Sicherheits- und Temperatursensoren bleiben aktiv

Die bereits dokumentierte Mindesttotzeit wird nicht durch Komfort- oder
Regelungslogik verkuerzt.

## Prioritaet gleichzeitiger Meldungen

Mehrere Meldungen koennen gleichzeitig aktiv sein. Die Bedienoberflaeche zeigt
die hoechste aktive Prioritaet prominent an; weitere Meldungen bleiben in einer
Meldungsliste sichtbar.

Prioritaetsreihenfolge:

1. **Sicherheitsfehler**
2. **Zwingend erforderliche Entscheidung**
3. **Wiederanlauf- oder Unterbrechungsmeldung**
4. **Prozesswarnung**
5. **Information**

Regeln:

- Eine niedrigere Meldung darf eine hoehere Meldung nicht verdecken.
- Neue Sicherheitsfehler unterbrechen jede weniger wichtige Anzeige.
- Alle Meldungen erhalten Zeitstempel, Meldungsklasse und eindeutigen Code.
- Quittieren entfernt nur die akustische oder sichtbare Bestaetigungsanforderung,
  nicht automatisch die zugrunde liegende Ursache.
- Eine weiterhin aktive Ursache bleibt als aktive Meldung sichtbar.

## Akustische Signale nach Meldungsklasse

Akustische Signale sind von der Wichtigkeit abhaengig.

### Information

Beispiele:

- Vorheizen beendet
- Programm beendet
- Kuehlziel erreicht

Verhalten:

- einmaliges kurzes Signalmuster
- keine regelmaessige Wiederholung

### Prozesswarnung

Beispiele:

- Zieltemperatur wird langsamer als erwartet erreicht
- Temperatur war laenger ausserhalb des normalen Arbeitsbereichs
- Produktfuehler ist ausgefallen und die automatische Ersatzstrategie laeuft

Verhalten:

- deutliches Warnmuster
- Wiederholung in laengeren Abstaenden, solange die Warnung aktiv und nicht
  quittiert ist
- kein ununterbrochener Dauerton

### Zwingend erforderliche Entscheidung

Verhalten:

- wiederkehrendes Signalmuster mit hoeherer Dringlichkeit als eine normale
  Prozesswarnung
- lokal stummschaltbar
- sichtbare Meldung bleibt bis zur Entscheidung oder bis eine definierte
  automatische Ersatzstrategie greift bestehen

### Sicherheitsfehler

Verhalten:

- deutlich unterscheidbares Fehlermuster
- regelmaessige Wiederholung bis zur Quittierung oder bis zur definierten
  Stummschaltung
- kein Dauerton
- Stummschaltung beeinflusst weder Fehlerzustand noch Sicherheitsabschaltung

## Stummschaltung und Quittierung

- Jede akustische Meldung muss lokal stummschaltbar sein.
- Die Stummschaltung entfernt keine sichtbare Meldung.
- Eine neue Meldung hoeherer Prioritaet darf trotz zuvor erfolgter
  Stummschaltung erneut akustisch signalisieren.
- Eine globale dauerhafte Deaktivierung aller Sicherheits- und Fehlersignale ist
  im normalen Benutzerbetrieb nicht vorgesehen.
- Einstellbare Lautstaerke oder reduzierte Signalmuster koennen spaeter
  vorgesehen werden, sofern die verwendete Summer-Hardware dies ermoeglicht.

## Akzeptierte Entscheidungen

- [x] Aussenluefter laeuft nach jedem Peltierbetrieb zwingend nach
- [x] Innenluefter besitzt zustandsabhaengiges Verhalten und Nachlauf
- [x] Luefter laufen waehrend der Peltier-Totzeit weiter
- [x] Meldungen besitzen eine feste Prioritaetsreihenfolge
- [x] nur die hoechste Meldung wird prominent angezeigt
- [x] weitere Meldungen bleiben in einer Meldungsliste erhalten
- [x] akustische Wiederholung richtet sich nach der Meldungsklasse
- [x] kein ununterbrochener Dauerton
- [x] Stummschaltung entfernt keine sichtbare Meldung und keine Fehlerursache

## Noch offen fuer spaetere Phasen

- konkrete Nachlaufzeit des Aussenluefters
- zustandsabhaengige Lauf- und Nachlaufzeiten des Innenluefters
- Erkennung und Reaktion bei Luefterausfall
- konkrete Summermuster und Wiederholungsintervalle
- technische Moeglichkeit einer Lautstaerkeregelung
- UI-Meldungstexte und Prioritaetsdarstellung; die stabilen #24-FaultCodes
  kommen aus dem SafetyCore-Vertrag
