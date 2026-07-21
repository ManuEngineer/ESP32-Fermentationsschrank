# Ausstehende Hardware-Revisionen aus der Softwarespezifikation

Dieses Dokument hat fuer die folgenden Punkte Vorrang vor widersprechenden Aussagen in `HARDWARE.md`, bis diese konsolidiert wird.

## Abnehmbarer Produktfuehler

Die bisherige Annahme eines dauerhaft verwendeten Produkt-/Referenzsensors in einer Wasserflasche ist nicht mehr verbindlich.

Geplant sind zwei Betriebsarten:

- produktgefuehrt mit angeschlossenem, abnehmbarem Produktfuehler
- luftgefuehrt ohne Produktfuehler

Der Produktfuehler soll ueber einen verpolungssicheren dreipoligen Anschluss abnehmbar sein. Der konkrete Steckverbinder ist noch offen.

Ein normaler 3,5-mm-Klinkenstecker wird vorerst nicht festgelegt, weil beim Stecken Kontakte kurzzeitig ueberbrueckt werden koennen. Zu pruefende Kandidaten sind M8 dreipolig, GX12-3 oder ein anderer verriegelbarer und verpolungssicherer Dreipolstecker.

Zu pruefen sind Feuchte- und Kondensatschutz, Reinigbarkeit, Zugentlastung, Lebensmitteleignung bei direktem Produktkontakt und das Verhalten des 1-Wire-Busses beim An- und Abstecken.

### Vorgeschlagene Bustrennung

Der fest eingebaute Luftfuehler und der abnehmbare Produktfuehler sollen nicht denselben extern zugaenglichen 1-Wire-Datenbus verwenden.

Nach der Entscheidung fuer einen dritten fest eingebauten Sicherheitssensor gilt folgende bevorzugte Reihenfolge:

1. eigener 1-Wire-Bus fuer den fest eingebauten Schrankluftfuehler
2. eigener 1-Wire-Bus fuer den fest eingebauten Aussenwaermetauscher-/Kuehlkoerperfuehler
3. eigener 1-Wire-Bus fuer den abnehmbaren Produktfuehler

Falls das GPIO-Budget drei getrennte Busse nicht erlaubt, ist als minimale akzeptable Variante vorgesehen:

- ein geschuetzter interner Bus fuer die beiden fest eingebauten Sensoren
- ein separater Bus fuer den abnehmbaren Produktfuehler

Damit kann das An- oder Abstecken des Produktfuehlers die fuer Regelung und Sicherheit wichtigen festen Sensoren nicht kurzzeitig stoeren. Die endgueltige Bustopologie wird erst nach der finalen Pinbudget-Pruefung verbindlich.

## Dritter fest eingebauter Sicherheitssensor

Ein dritter vorhandener DS18B20 wird am thermisch relevanten aeusseren Waermetauscher beziehungsweise Kuehlkoerper nahe der aeusseren Peltierseite montiert.

Aufgaben:

- Temperatur und Aenderungsrate der aeusseren Leistungsbaugruppe ueberwachen
- stark vermuteten Aussenluefter- oder Waermeabfuhrfehler erkennen
- unerwartete Peltier- oder Richtungsreaktionen diagnostizieren
- Sicherheitsfreigabe des Peltierbetriebs unterstuetzen

Der Sensor wird in Heiz- und Kuehlrichtung ausgewertet. Wegen der Polaritaetsumkehr kann die aeussere Peltierseite je nach Betriebsrichtung warm oder kalt werden.

Anforderungen:

- feste, reproduzierbar dokumentierte Montageposition
- gute thermische Kopplung
- Schutz vor Kondensat, Zug und beweglichen Luefterteilen
- eindeutige Zuordnung ueber ROM-Adresse
- Peltierfreigabe nur mit gueltigem Schrankluft- und Kuehlkoerpersensor
- bei Sensorausfall Peltier AUS und verriegelte Fehlerbehandlung gemaess `SAFETY_COMPONENT_FAULTS.md`

Die bisherige Angabe `2 geplant` in `HARDWARE.md` ist damit ueberholt. Fuer den ersten Aufbau sind drei DS18B20 vorgesehen:

1. Schrankluft
2. abnehmbares Produkt
3. Aussenwaermetauscher/Kuehlkoerper

## Akustischer Signalgeber

Fuer lokale Warnungen und Aufforderungen wird ein Summer vorgesehen.

Bevorzugte Richtung:

- aktiver 5-V- oder 12-V-Summer
- Schaltung ueber freien Onboard-MOSFET-Kanal oder separate Treiberstufe
- quittierbare kurze Signalmuster statt Dauerton

Noch zu pruefen sind Versorgungsspannung, Stromaufnahme, Lautstaerke, Kanalzuordnung, Bootverhalten und mechanische Position.

Der Summer ist eine Warnfunktion. Sicherheitsfehler muessen auch ohne funktionierenden Summer sichtbar behandelt werden.

## Kein Tuerkontakt im ersten Release

Im ersten Release wird kein Tuerkontakt eingebaut.

Die Software darf die Tuerstellung deshalb nicht als bekannt voraussetzen. Eine optionale spaetere Schnittstelle fuer einen Reed- oder Magnetschalter wird nur architektonisch vorgesehen und bleibt ohne bestaetigte Hardware deaktiviert.

Es wird vorerst kein GPIO fuer einen Tuerkontakt reserviert. Eine spaetere Nachruestung muss gegen das dann gueltige Pinbudget geprueft werden.

## Zeitquelle fuer Stromausfallbewertung

Fuer eine automatische Bewertung der Stromausfalldauer wird nach dem Neustart eine verlaessliche Zeitquelle benoetigt.

Noch offen sind:

- Netzwerkzeit mit zuvor gespeichertem Zeitstempel
- optionales batteriegepuffertes RTC-Modul
- Verhalten ohne verfuegbare verlaessliche Zeitquelle

Ohne verlaessliche Unterbrechungsdauer darf die Firmware einen unterbrochenen Lauf nicht automatisch fortsetzen. Ein RTC-Modul ist noch nicht als Pflicht-Hardware beschlossen.
