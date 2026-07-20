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

Der fest eingebaute Luftfuehler und der abnehmbare Produktfuehler sollen nach Moeglichkeit nicht denselben 1-Wire-Datenpin verwenden.

Bevorzugter Kandidat:

- eigener 1-Wire-Bus fuer den fest eingebauten Luftfuehler
- eigener 1-Wire-Bus fuer den abnehmbaren Produktfuehler

Damit kann das An- oder Abstecken des Produktfuehlers den fuer Regelung und Sicherheit wichtigen Luftfuehler nicht kurzzeitig stoeren. Diese Bustrennung kostet einen zusaetzlichen GPIO und wird erst nach der finalen Pinbudget-Pruefung verbindlich.

## Akustischer Signalgeber

Fuer lokale Warnungen und Aufforderungen wird ein Summer vorgesehen.

Bevorzugte Richtung:

- aktiver 5-V- oder 12-V-Summer
- Schaltung ueber freien Onboard-MOSFET-Kanal oder separate Treiberstufe
- quittierbare kurze Signalmuster statt Dauerton

Noch zu pruefen sind Versorgungsspannung, Stromaufnahme, Lautstaerke, Kanalzuordnung, Bootverhalten und mechanische Position.

Der Summer ist eine Warnfunktion. Sicherheitsfehler muessen auch ohne funktionierenden Summer sichtbar behandelt werden.
