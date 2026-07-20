# Standardprogramme

## Status

Dieses Dokument definiert Zweck und Grundcharakter der mitgelieferten
Standardprogramme. Die exakten Temperaturen, Zeiten, Zielbaender und
Kuehlparameter werden erst waehrend der Inbetriebnahme und praktischen Erprobung
festgelegt.

Die Standardprogramme sind **nicht an eine bestimmte Kultur oder einen
bestimmten Hersteller gebunden**.

## Grundsatz fuer Werkseinstellungen

Die vier Standardprogramme dienen als allgemeine, bearbeitbare Ausgangspunkte:

1. Joghurt mild
2. Joghurt stichfest
3. Milchkefir
4. Wasserkefir

Fuer jedes Standardprogramm gilt:

- allgemeiner Prozess statt herstellerspezifischer Kulturvorgabe
- bearbeitbare Zieltemperatur
- bearbeitbare Fermentationsdauer
- bearbeitbarer Standardwert fuer Vorheizen
- bearbeitbarer Regelmodus beziehungsweise Sensorvorschlag
- bearbeitbare Abschluss- und Kuehloption
- Ruecksetzen auf eine unveraenderliche Werkseinstellung moeglich
- vor jedem Start sichtbare Zusammenfassung der tatsaechlich verwendeten Werte

Die Software darf nicht voraussetzen, dass ein bestimmtes Kulturprodukt
verwendet wird. Hinweise auf Kulturen oder Hersteller gehoeren spaeter hoechstens
in optionale Notizen eines Programms.

## Umgang mit noch offenen Prozesswerten

Die konkrete Firmware benoetigt schliesslich gueltige Werkseinstellungen. Diese
werden aber nicht aufgrund von Annahmen vor der praktischen Erprobung
festgeschrieben.

Vorgehen:

1. Datenmodell, Eingabefelder, Validierung und Prozessablauf werden zuerst
   implementiert.
2. Hardware und Temperaturregelung werden mit Wasser beziehungsweise einer
   geeigneten Testlast erprobt.
3. Aufheiz-, Abkuehl- und Regelverhalten des realen Schrankes werden gemessen.
4. Danach werden vorlaeufige Werkseinstellungen fuer die vier Programme
   eingetragen.
5. Diese Werte werden mit den tatsaechlich verwendeten Kulturen weiter
   optimiert.

Bis zu diesem Schritt bleiben exakte Werkseinstellungen in der Spezifikation
als `TBD_COMMISSIONING` gekennzeichnet.

Die Benutzeroberflaeche muss verhindern, dass ein Programm mit fehlenden oder
ungueltigen Pflichtwerten gestartet wird. `TBD_COMMISSIONING` ist nur ein
Dokumentationszustand und kein zulaessiger Laufzeitwert der fertigen Firmware.

## Joghurt mild

### Zweck

Allgemeines Programm fuer eher mild fermentierten Joghurt. Es ist nicht auf die
FAIE-Kultur MILD oder eine andere konkrete Kultur festgelegt.

### Vorgesehener Grundcharakter

- Vorheizen des leeren Schrankes standardmaessig sinnvoll
- luftgefuehrter Betrieb muss voll unterstuetzt werden
- produktgefuehrter Betrieb ist bei geeignetem angeschlossenem Produktfuehler
  moeglich
- nach Ablauf soll aktive Kuehlung als konfigurierbare Option vorhanden sein
- konkrete Zieltemperatur, Dauer und Kuehlwerte: `TBD_COMMISSIONING`

### Nicht festgelegt

- Kulturhersteller
- konkrete Bakterienstaemme
- Milchtyp oder Fettgehalt
- erforderliche Vorbehandlung der Milch
- Garantie fuer ein bestimmtes Geschmacks- oder Konsistenzergebnis

## Joghurt stichfest

### Zweck

Allgemeines Programm fuer Joghurt, bei dem eine festere beziehungsweise
stichfeste Konsistenz angestrebt wird.

### Vorgesehener Grundcharakter

- Vorheizen des leeren Schrankes standardmaessig sinnvoll
- gleichmaessige Temperaturfuehrung und moeglichst geringe Stoerungen waehrend
  der Fermentation
- luftgefuehrter und produktgefuehrter Betrieb moeglich
- aktive Kuehlung nach Ablauf als konfigurierbare Option
- konkrete Zieltemperatur, Dauer und Kuehlwerte: `TBD_COMMISSIONING`

Das Programm allein garantiert keine Stichfestigkeit. Kultur, Milch,
Vorbehandlung, Feststoffgehalt und ruhige Aufstellung beeinflussen das Ergebnis.

## Milchkefir

### Zweck

Allgemeines Programm fuer Milchkefir mit Kefirknollen oder einer geeigneten
Kultur.

### Vorgesehener Grundcharakter

- Vorheizen standardmaessig eher optional
- Zieltemperatur kann je nach Ausgangslage durch Heizen oder Kuehlen erreicht
  werden
- luftgefuehrter Betrieb muss voll unterstuetzt werden
- produktgefuehrter Betrieb optional
- Abschluss ohne Kuehlung oder mit anschliessender Kuehlung konfigurierbar
- konkrete Zieltemperatur, Dauer und Kuehlwerte: `TBD_COMMISSIONING`

## Wasserkefir

### Zweck

Allgemeines Programm fuer Wasserkefir.

### Vorgesehener Grundcharakter

- Vorheizen standardmaessig eher optional
- luftgefuehrter Betrieb ist der normale Ausgangspunkt
- produktgefuehrter Betrieb bleibt technisch moeglich
- aktive Kuehlung nach Ablauf ist optional und nicht zwingender Standard
- konkrete Zieltemperatur, Dauer und Kuehlwerte: `TBD_COMMISSIONING`

## Benutzerprogramme

Benutzerprogramme koennen aus einem Standardprogramm kopiert oder leer neu
angelegt werden. Sie koennen eigene Namen und Notizen erhalten, beispielsweise
fuer:

- eine konkrete Kultur
- eine bestimmte Milch
- Kombucha
- individuelle Versuchsreihen
- spaetere mehrstufige Prozesse

Ein Benutzerprogramm speichert alle fuer den Lauf erforderlichen Parameter und
ist nicht von spaeteren Aenderungen am urspruenglichen Standardprogramm
abhaengig.

## Akzeptierte Entscheidungen

- [x] `Joghurt mild` ist ein allgemeines Programm
- [x] kein Standardprogramm ist an eine konkrete Kultur oder Marke gebunden
- [x] Feinabstimmung der Werkseinstellungen erfolgt nach funktionsfaehiger
      Hardware und praktischer Erprobung
- [x] Standardprogramme bleiben bearbeitbar und ruecksetzbar
- [x] konkrete Laufzeitwerte duerfen in der fertigen Firmware nicht fehlen
- [x] Kultur- und Rezeptdetails koennen spaeter als optionale Notizen oder
      Benutzerprogramme abgebildet werden

## Noch offen

- vorlaeufige Werkseinstellungen nach Inbetriebnahme
- globale oder programmspezifische Zielbaender
- Qualifikationsdauer
- maximale Zielerreichungszeit
- Standard-Abschlussverhalten je Programm
- Kuehlziel und Kuehlhalteverhalten
