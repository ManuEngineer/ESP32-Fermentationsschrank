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
- Touch und Web verwenden dieselben rendererunabhaengigen View-Modelle,
  typisierten Commands, erwarteten Revisionen und strukturierten
  Command-Ergebnisse. Layout, Navigation, Browser-Sprache, Transport- und
  Sitzungsgrenzen bleiben oberflaechenspezifisch.
- Plattform- und App-Texte besitzen getrennte Namensraeume. Renderer und
  Transportadapter veraendern Fachzustaende nie direkt.

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
- Produkt- und Schranktemperatur; die technische Rolle bleibt
  Schrankluftfuehler
- Produktfuehlerstatus
- aktive Meldungen
- Netzwerk- und Zeitstatus in kompakter Form
- `Programm starten`
- `Manueller Betrieb`

Beispiel:

```text
Fermentationsschrank

Produkt       nicht angeschlossen
Schrank       22,4 °C

[Programm starten]
[Manueller Betrieb]
```

### Laufender Prozess

Mindestens sichtbar:

- Programmname
- benutzerverstaendliche Prozessphase
- Produkt-, Schrank- und Solltemperatur
- verstrichene und verbleibende Zeit
- Abschlussverhalten
- hoechste aktive Meldung
- `Details`, `Meldungen` und `STOP`

Beispiel:

```text
Joghurt mild
Fermentation laeuft

Produkt       41,8 °C
Schrank       42,3 °C
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
- Fehlende Uebersetzungen verwenden zuerst Englisch und danach den sichtbaren
  technischen Schluessel. Die Regel gilt gleichermassen fuer Touch und Web,
  ohne die browserindividuelle Sprachwahl zu veraendern.

## Normale Webanmeldung

Diese Regeln gelten, wenn der normale Webpasswortschutz aktiviert ist.

### Anmeldung

- gemeinsames normales Webpasswort
- getrennt von der vierstelligen Service-PIN
- kein Benutzerkonten- oder Rollenmodell im ersten Release
- Passwort wird bei der Eingabe verdeckt dargestellt
- Fehlversuche werden global pro Credential gezaehlt: nach fuenf falschen
  Passwortpruefungen 30 Sekunden Sperre, weitere Fehlversuchsbloecke
  verdoppeln bis hoechstens 15 Minuten
- Vor-Sperr-Zaehler, Sperrstufe und aktiver Sperrzustand werden atomar und
  neustartfest gefuehrt; ein beantworteter Fehlversuch verschwindet nicht
  durch Neustart und Persistenzfehler ergeben niemals Freigabe

### Keine dauerhafte Anmeldung in Release 1

Gemaess [ADR-017](DECISIONS.md#adr-017-keine-dauerhafte-webanmeldung-in-release-1)
gibt es in Release 1 keine Option `Angemeldet bleiben`.
Normale Sessions bleiben serverseitig, fluechtig und begrenzt. Sie enden nach
30 Minuten Inaktivitaet, spaetestens nach 12 Stunden absolut, und werden bei
einem Geraeteneustart verworfen. Persistente Login-, Refresh- oder
Browsergeraete-Tokens sind nicht vorgesehen. Ein spaeterer Ausbau benoetigt
eine neue Entscheidung und einen eigenen Securitynachweis.

Ein reiner Browserneustart ist kein garantiertes Logout- oder
Widerrufsereignis. Stellt der Browser das Sitzungscookie wieder her, bleibt die
serverseitige Session hoechstens bis zum Inaktivitaets-/Absolutlimit, Logout,
Credential-/Moduswechsel, Werksreset oder einem anderen serverseitigen
Widerruf gueltig. Browser-Fingerprints, `beforeunload`-Garantien oder andere
Lifecycle-Hacks sind dafuer nicht vorgesehen.

### Betrieb ohne normales Webpasswort

Ist der normale Webpasswortschutz bewusst deaktiviert:

- entfaellt die Passwortpruefung, nicht aber die Sitzung;
- der Server erzeugt eine begrenzte fluechtige anonyme lokale Session mit
  mindestens 128 Bit kryptografisch zufaelliger Kennung, derselben Cookiepolicy,
  einem sitzungsgebundenen CSRF-Token sowie denselben Session- und
  Ressourcenlimits; sie enthaelt keine Benutzeridentitaet oder Rolle und wird
  beim Neustart verworfen;
- jede Person im erreichbaren lokalen Netz kann normale Funktionen bedienen;
  eine dauerhafte sichtbare Warnung weist darauf hin;
- normale Mutationen verlangen weiterhin Session, CSRF, vorgesehene Methode
  und Content-Type, Origin-/Referer-/Fetch-Metadata-Pruefung, erwartete
  Revision, Konflikt-/Wiederholungsschutz sowie Fach- und Safetypruefung;
- der Servicebereich bleibt zusaetzlich durch Service-PIN, PIN-KDF,
  neustartfeste Sperrlogik, sitzungsgebundene Servicefreigabe und
  Hardware-/Safetygates geschuetzt.

Der Moduswechsel ist selbst eine geschuetzte Mutation. Aktiviert zu deaktiviert
verlangt eine gueltige Session, CSRF, erwartete Revision sowie ausdrueckliche
Warnung und Bestaetigung; danach werden alle bisherigen Sessions widerrufen.
Deaktiviert zu aktiviert setzt das neue Passwort atomar und widerruft alle
anonymen Sessions. Bei der Ersteinrichtung ist Passwortschutz empfohlen und
vorausgewaehlt; Deaktivierung ist nur bewusst nach Warnung moeglich.

## Sitzungstechnische Mindestregeln

Unabhaengig von der spaeteren konkreten Bibliothek gelten:

- Sitzungskennungen duerfen nicht als dauerhaft sichtbare URL-Parameter verwendet
  werden.
- Sitzungskennungen enthalten mindestens 128 Bit kryptografisch zufaelligen
  Inhalt und sind nicht erratbar.
- Das Sessioncookie verwendet `HttpOnly`, `SameSite=Strict`, `Path=/` und kein
  `Domain`-Attribut.
- Unter direktem lokalem HTTP kann `Secure` technisch nicht erzwungen werden;
  bei spaeterem TLS-Betrieb muss es aktiviert werden koennen.
- Zustandsveraendernde Webanfragen benoetigen einen sitzungsgebundenen
  CSRF-Token mit mindestens 128 Bit Zufall im Header `X-CSRF-Token`, die
  vorgesehene Methode und den vorgesehenen Content-Type sowie Origin-,
  ersatzweise Referer-, und soweit vorhanden Fetch-Metadata-Pruefung. Eine
  erwartete fachliche Revision bleibt ein separates Gate.
- Abmeldung und Passwortaenderung machen die betroffene Sitzung ungueltig.
- Sitzungs- und Servicefreigaben werden nicht in normale Diagnoseexporte
  aufgenommen.

## Servicebereich in der Weboberflaeche

Der Web-Servicebereich wird durch dieselbe vierstellige Service-PIN geschuetzt
wie der lokale Servicebereich.

### Freigabe

Nach korrekter PIN-Eingabe wird der Servicebereich fuer genau diese fluechtige
normale oder anonyme lokale Websession freigegeben: 5 Minuten Inaktivitaet,
hoechstens 15 Minuten absolut. Diese bewusst strengere netzwerkseitige
Securitygrenze bleibt von der lokalen Touch-Servicefreigabe getrennt und wird
nicht auf deren 10-Minuten-Inaktivitaet vereinheitlicht.

Verbindliche Regeln:

- Servicefreigabe ist getrennt von der normalen Webanmeldung beziehungsweise
  anonymen lokalen Session.
- Die Freigabe gilt nur fuer die jeweilige Websitzung.
- Sie wird nach Inaktivitaet automatisch verworfen.
- Abmeldung verwirft auch die Servicefreigabe.
- Schliessen oder Ablauf einer normalen Sitzung verwirft die Servicefreigabe.
- Ein anderes angemeldetes Endgeraet wird dadurch nicht ebenfalls entsperrt.
- PIN-Fehlversuche werden global gezaehlt: nach drei Fehlern 30 Sekunden
  Sperre, weitere Bloecke verdoppeln bis hoechstens 30 Minuten. Der
  sicherheitsrelevante Fehlversuchs-/Sperrzustand ist atomar neustartfest.

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
- Schranktemperatur
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
- [x] gemaess ADR-017 keine dauerhafte Anmeldung in Release 1
- [x] fluechtige serverseitige Sessions mit 30 Minuten Inaktivitaet,
      12 Stunden absoluter Dauer und Widerruf beim Geraeteneustart; ein reiner
      Browserneustart ist kein garantiertes Widerrufsereignis
- [x] Service-PIN entsperrt nur die jeweilige Sitzung fuer begrenzte Zeit
- [x] kritische Serviceaktionen verlangen eine eigene Bestaetigung
- [x] Live-Aktualisierung ohne manuelles Neuladen
- [x] Revisionsschutz gegen stilles Ueberschreiben konkurrierender Aenderungen
- [x] Temperaturdiagramm fuer den aktuellen Lauf

## Noch offen fuer Phase 5C und spaeter

- konkrete Passwortmindestanforderungen
- technische Auswahl fuer Live-Aktualisierung
- weitere HTTP-Schutzheader ueber den verbindlichen Cookie-/CSRF-Vertrag hinaus
- maximale Anzahl paralleler Webverbindungen
- Abtastrate und Reduktion der Diagrammdaten
- Speicherung und Export vergangener Laufdiagramme
- genaue Darstellung von Vergleich und Konfliktaufloesung
- Verhalten hinter Reverse Proxy und VPN
