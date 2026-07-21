# Lokale Weboberflaeche

## Status

Dieses Dokument beschreibt die in Phase 5B akzeptierten Regeln fuer die lokale
Weboberflaeche, Anmeldung, Sitzungen, Servicefreigabe, Live-Aktualisierung und
gleichzeitige Bedienung.

Die Netzwerkbereitstellung und grundlegenden Sicherheitsgrenzen stehen in
[`NETWORK.md`](NETWORK.md).

## Grundsaetze

- Die Weboberflaeche bietet auf Mobiltelefon, Tablet und Computer denselben
  fachlichen Funktionsumfang.
- Das Layout passt sich der Bildschirmgroesse an.
- Die mobile Bedienung ist kein nachtraeglicher Sonderfall, sondern ein
  gleichwertiges Zielgeraet.
- Die Weboberflaeche verwendet denselben fachlichen Zustand und dieselben
  Sicherheitsregeln wie das Touchdisplay.
- Keine Webaktion darf Sensor-, Aktor- oder Sicherheitspruefungen umgehen.
- Ein Ausfall der Weboberflaeche beeinflusst den laufenden Prozess nicht.

## Responsive Darstellung

Die Oberflaeche wird responsiv aufgebaut.

### Mobiltelefon

- einspaltige Darstellung
- grosse Touchflaechen
- kompakte, eindeutig erreichbare Navigation
- wichtige Laufdaten ohne horizontales Scrollen
- Diagramme passen sich der verfuegbaren Breite an

### Tablet und Computer

- breitere Tabellen und Diagramme
- Seitenmenue oder vergleichbare dauerhafte Navigation moeglich
- Prozessdaten und Detailinformationen duerfen nebeneinander dargestellt werden

Die fachlichen Funktionen unterscheiden sich nicht nach Geraeteklasse.

## Zustandsabhaengige Startseite

Die Startseite richtet sich wie das lokale Display nach dem aktuellen
Geraetezustand.

### Standby

Mindestens sichtbar:

- Geraetename
- Produkt- und Schranklufttemperatur
- Produktfuehlerstatus
- aktive Meldungen
- Netzwerk- und Zeitstatus in kompakter Form
- `Programm starten`
- `Manueller Betrieb`

Beispiel:

```text
Fermentationsschrank

Produkt       nicht angeschlossen
Luft          22,4 °C

[Programm starten]
[Manueller Betrieb]
```

### Laufender Prozess

Mindestens sichtbar:

- Programmname
- benutzerverstaendliche Prozessphase
- Produkt-, Schrankluft- und Solltemperatur
- verstrichene und verbleibende Zeit
- Abschlussverhalten
- hoechste aktive Meldung
- `Details`, `Meldungen` und `STOP`

Beispiel:

```text
Joghurt mild
Fermentation laeuft

Produkt       41,8 °C
Luft          42,3 °C
Soll          42,0 °C
Restzeit      06:14

[Details] [Meldungen] [STOP]
```

### Abgeschlossener Prozess

Die Weboberflaeche bietet dieselben fachlichen Aktionen wie das Display:

- Abschluss quittieren
- Laufdetails anzeigen
- optional neuen manuellen Kuehllauf starten

## Navigation

Die fachlichen Hauptbereiche sind:

1. Uebersicht
2. Programme
3. Manueller Betrieb
4. Meldungen und Protokolle
5. Einstellungen
6. Diagnose
7. Service
8. System

Auf breiten Bildschirmen kann dies als Seitenmenue dargestellt werden. Auf
Mobiltelefonen wird eine kompakte, klar beschriftete Navigation verwendet.
Wichtige Funktionen duerfen nicht ausschliesslich hinter schwer erkennbaren
Symbolen versteckt werden.

Eine spaetere visuelle Zusammenfassung einzelner Technikpunkte bleibt moeglich,
solange Zugriffsrechte und fachliche Trennung erhalten bleiben.

## Sprache je Browser

Jeder Browser beziehungsweise jedes Endgeraet kann seine eigene Sprache fuer die
Weboberflaeche speichern.

Unterstuetzt werden:

- Deutsch
- Spanisch
- Englisch

Die Sprache des Touchdisplays bleibt davon unabhaengig.

Beispiel:

- lokales Display: Deutsch
- Browser des Benutzers: Deutsch
- Browser der Partnerin: Spanisch

Verbindliche Regeln:

- Beim ersten Aufruf darf die Browsersprache als Vorschlag verwendet werden.
- Der Benutzer kann die Sprache jederzeit manuell aendern.
- Die Auswahl wird lokal im Browser oder in der Websitzung gespeichert.
- Eine Sprachwahl veraendert keine Prozessdaten oder Geraeteeinstellungen.
- Benutzerdefinierte Programmnamen und Notizen werden nicht automatisch
  uebersetzt.
- Fehlende Uebersetzungen verwenden Deutsch als Fallback.

## Normale Webanmeldung

Diese Regeln gelten, wenn der normale Webpasswortschutz aktiviert ist.

### Anmeldung

- gemeinsames normales Webpasswort
- getrennt von der vierstelligen Service-PIN
- kein Benutzerkonten- oder Rollenmodell im ersten Release
- Passwort wird bei der Eingabe verdeckt dargestellt
- Fehlversuche werden begrenzt beziehungsweise zeitlich verzoegert

### Option `Angemeldet bleiben`

Der Anmeldedialog bietet:

```text
[ ] Angemeldet bleiben
```

Ohne aktivierte Option:

- Sitzung endet nach einer konfigurierbaren Inaktivitaetszeit
- Sitzung soll spaetestens beim Ende der Browsersitzung ihre Gueltigkeit verlieren
- erneute Anmeldung ist danach erforderlich

Mit aktivierter Option:

- ein sicher gespeichertes Anmeldetoken darf ueber das Schliessen des Browsers
  hinaus gueltig bleiben
- die Gueltigkeit ist zeitlich begrenzt
- der Benutzer kann sich aktiv abmelden
- Passwortaenderung oder Zuruecksetzen der Webanmeldung widerruft bestehende
  dauerhafte Sitzungen

Die konkreten Zeitspannen werden in `SETTINGS_AND_STORAGE.md` festgelegt.

### Betrieb ohne normales Webpasswort

Ist der normale Webpasswortschutz bewusst deaktiviert:

- entfällt die normale Anmeldung
- jede Person im erreichbaren lokalen Netz kann die normalen Webfunktionen
  aufrufen und bedienen
- die Oberflaeche zeigt einen sichtbaren Hinweis auf den ungeschuetzten normalen
  Webzugang
- der Servicebereich bleibt weiterhin PIN-geschuetzt

## Sitzungstechnische Mindestregeln

Unabhaengig von der spaeteren konkreten Bibliothek gelten:

- Sitzungskennungen duerfen nicht als dauerhaft sichtbare URL-Parameter verwendet
  werden.
- Sitzungskennungen muessen ausreichend zufaellig und nicht erratbar sein.
- Bei cookiebasierter Anmeldung werden mindestens `HttpOnly` und ein sinnvoller
  `SameSite`-Schutz verwendet.
- Unter direktem lokalem HTTP kann `Secure` technisch nicht erzwungen werden;
  bei spaeterem TLS-Betrieb muss es aktiviert werden koennen.
- Zustandsveraendernde Webanfragen benoetigen Schutz gegen unbeabsichtigte oder
  fremde Formularanforderungen, beispielsweise CSRF-Schutz.
- Abmeldung und Passwortaenderung machen die betroffene Sitzung ungueltig.
- Sitzungs- und Servicefreigaben werden nicht in normale Diagnoseexporte
  aufgenommen.

## Servicebereich in der Weboberflaeche

Der Web-Servicebereich wird durch dieselbe vierstellige Service-PIN geschuetzt
wie der lokale Servicebereich.

### Freigabe

Nach korrekter PIN-Eingabe wird der Servicebereich fuer eine begrenzte
Inaktivitaetszeit entsperrt.

Verbindliche Regeln:

- Servicefreigabe ist getrennt von der normalen Webanmeldung.
- Die Freigabe gilt nur fuer die jeweilige Websitzung.
- Sie wird nach Inaktivitaet automatisch verworfen.
- Abmeldung verwirft auch die Servicefreigabe.
- Schliessen oder Ablauf einer normalen Sitzung verwirft die Servicefreigabe.
- Ein anderes angemeldetes Endgeraet wird dadurch nicht ebenfalls entsperrt.
- PIN-Fehlversuche werden begrenzt beziehungsweise verzoegert.

### Kritische Aktionen

Auch bei entsperrtem Servicebereich benoetigen kritische Aktionen eine eigene
Bestaetigung, beispielsweise:

- vollstaendiger Werksreset
- Netzwerkeinstellungen loeschen
- Touchkalibrierung zuruecksetzen
- Sensorzuordnung aendern
- direkte Aktortests starten

Ein Werksreset oder eine vergleichbar weitreichende Aktion darf bei laengerer
Inaktivitaet eine erneute PIN-Eingabe verlangen.

## Live-Aktualisierung

Temperaturen, Restzeit, Prozessphase, Aktorstatus und Meldungen aktualisieren sich
ohne manuelles Neuladen.

Die technische Umsetzung kann je nach Ressourcen und Bibliotheken erfolgen ueber:

- Server-Sent Events
- WebSocket
- vergleichbaren Push-Mechanismus
- kontrolliertes periodisches Nachladen als Fallback

Die Spezifikation legt nicht vorzeitig ein bestimmtes Protokoll fest. Das
sichtbare Verhalten ist verbindlich:

- neue Messwerte erscheinen laufend
- Zustandswechsel werden zeitnah dargestellt
- Warnungen und Fehler werden ohne komplettes Neuladen sichtbar
- Verbindungsabbruch wird klar angezeigt
- veraltete Werte werden nicht als aktuell dargestellt
- nach Wiederverbindung wird ein vollstaendiger aktueller Zustandsabzug geladen

Die Weboberflaeche zeigt den Zeitpunkt beziehungsweise das Alter des letzten
gueltigen Status, falls die Live-Verbindung unterbrochen ist.

## Gleichzeitige Bedienung und Revisionsschutz

Display, mehrere Browser und mehrere Browser-Tabs duerfen gleichzeitig lesend
verbunden sein.

Jeder bearbeitbare Datensatz besitzt eine Revisionskennung beziehungsweise eine
vergleichbare Version.

Vorgesehener Ablauf:

```text
Browser A laedt Programmrevision 12
Browser B speichert Programmrevision 13
Browser A versucht seine alte Revision 12 zu speichern
  -> Speichern wird nicht still ausgefuehrt
  -> Konflikt anzeigen
  -> aktuellen Stand laden oder Aenderungen vergleichen
```

Verbindliche Regeln:

- Veraltete Daten duerfen neuere Aenderungen niemals still ueberschreiben.
- Jede veraendernde Aktion enthaelt die erwartete Revision.
- Bei Abweichung wird die Aktion abgelehnt oder bewusst neu bestaetigt.
- Start, Stop, Quittieren und vergleichbare Laufaktionen werden atomar
  verarbeitet.
- Eine bereits durch eine andere Quelle ausgefuehrte Aktion wird nicht doppelt
  ausgefuehrt.
- Die Quelle wird mindestens als `display`, `web` oder `service_web`
  protokolliert.
- Sicherheitslogik darf eine grundsaetzlich gueltige Webaktion jederzeit
  ablehnen.

Beispielmeldung:

```text
Programm wurde inzwischen an einer anderen Oberflaeche geaendert.

[Aktuellen Stand laden]
[Aenderungen vergleichen]
```

## Temperaturdiagramm des aktuellen Laufes

Die Weboberflaeche zeigt fuer den aktuellen Lauf ein Diagramm mit mindestens:

- Produkttemperatur, sofern verfuegbar
- Schranklufttemperatur
- Solltemperatur
- Phasenwechseln
- Warnungs- und Unterbrechungsmarkierungen, soweit sinnvoll

Anforderungen:

- mobil und am Computer lesbar
- Achsen, Einheiten und Zeitbasis eindeutig
- fehlende Messwerte als Luecke und nicht als erfundene Verbindung darstellen
- Sensorwechsel sichtbar kennzeichnen
- Laufzeitkorrektur nach Stromausfall nachvollziehbar machen
- Diagramm darf den Regelprozess nicht durch uebermaessige Ressourcenlast
  beeintraechtigen

Vollstaendige Diagramme vergangener Laeufe sind nicht verbindlicher Bestandteil
von Phase 5B. Umfang, Aufbewahrung und Export historischer Messdaten werden in
Phase 6 und Phase 9 festgelegt.

## Akzeptierte Entscheidungen aus Phase 5B

- [x] gleicher Funktionsumfang auf Handy, Tablet und Computer
- [x] responsive, mobil optimierte Darstellung
- [x] zustandsabhaengige Web-Startseite
- [x] getrennte Navigation fuer Uebersicht, Programme, manuellen Betrieb,
      Meldungen, Einstellungen, Diagnose, Service und System
- [x] Sprache je Browser unabhaengig von der Displaysprache
- [x] Deutsch, Spanisch und Englisch auch im Web
- [x] Option `Angemeldet bleiben`
- [x] ohne diese Option Sitzungsende nach Browserende oder Inaktivitaet
- [x] Service-PIN entsperrt nur die jeweilige Sitzung fuer begrenzte Zeit
- [x] kritische Serviceaktionen verlangen eine eigene Bestaetigung
- [x] Live-Aktualisierung ohne manuelles Neuladen
- [x] Revisionsschutz gegen stilles Ueberschreiben konkurrierender Aenderungen
- [x] Temperaturdiagramm fuer den aktuellen Lauf

## Noch offen fuer Phase 5C und spaeter

- genaue Zeitspannen fuer normale und dauerhafte Sitzungen
- genaue Service-Inaktivitaetszeit
- konkrete Passwortmindestanforderungen
- technische Auswahl fuer Live-Aktualisierung
- konkretes CSRF-Verfahren und weitere HTTP-Schutzheader
- maximale Anzahl paralleler Webverbindungen
- Abtastrate und Reduktion der Diagrammdaten
- Speicherung und Export vergangener Laufdiagramme
- genaue Darstellung von Vergleich und Konfliktaufloesung
- Verhalten hinter Reverse Proxy und VPN
