# Einstellungen und Persistenz

## Status

Dieses Dokument definiert Konfigurationsebenen, Aenderungsrechte, Vorschau,
Neustartbedarf, Zeitdarstellung und atomare Speicherung. Laufpersistenz,
Sicherungen und Recovery werden in `RUN_PERSISTENCE.md`,
`BACKUP_SECURITY_RETENTION.md` und `PR38_REVIEW_CORRECTIONS.md` ergaenzt.

## Grundsaetze

- Werkseinstellungen, Benutzereinstellungen und aktive Laufdaten sind getrennt.
- Ein Lauf verwendet einen unveraenderlichen Programmschnappschuss.
- Neue Werte werden vor der Aktivierung vollstaendig validiert.
- Keine teilweise geschriebene Konfiguration gilt als gueltig.
- Die letzte nachweislich gueltige Revision bleibt als Rueckfall erhalten.
- Eine Service-PIN hebt keine firmwarefesten Sicherheitsgrenzen auf.
- Normale Einstellungen starten einen laufenden Prozess nie automatisch neu.
- Ein vergessener Service-PIN kann nur durch einen vollstaendigen lokalen
  Werksreset behandelt werden; dieser besondere Recoveryweg darf die vergessene
  PIN nicht selbst voraussetzen.

## Konfigurationsebenen

```text
unveraenderliche Werkseinstellungen
        ↓
gespeicherte validierte Benutzereinstellungen
        ↓
unveraenderlicher Schnappschuss des aktiven Laufes
```

### Werkseinstellungen

Enthalten mindestens:

- allgemeine Factory-Werte
- vier Standardprogramme
- Schema- und Migrationsgrundlagen
- firmwarefeste Sicherheitsgrenzen
- verbotene Aktorkombinationen
- nicht unterschreitbare Schutzzeiten

Sie werden im normalen Betrieb nicht ueberschrieben. Ein Werksreset erzeugt aus
ihnen eine neue gueltige Benutzerkonfiguration.

### Benutzereinstellungen

Beispiele:

- Sprache, Geraetename, Display und Ton
- WLAN und Webzugang
- Programme und Benutzervoreinstellungen
- freigegebene Serviceparameter
- Touchkalibrierung
- IANA-Zeitzone

Jede Revision besitzt mindestens Schema, Generation, Integritaetsinformation,
Aenderungsquelle und – falls verfuegbar – UTC-Zeit.

### Laufschnappschuss

Beim Start werden mindestens festgehalten:

- Programm-ID und Programmrevision
- sichtbarer Name
- Zieltemperatur und Dauer
- Vorheizen
- Sensorbetrieb und Sensorfehlerstrategie
- Zielqualifikation
- maximale Zielerreichungszeit
- Abschluss- und Kuehlverhalten
- laufbezogene freigegebene Regelparameter
- sprachunabhaengige Codes

Aenderungen am Quellprogramm veraendern den aktiven Lauf nicht. Zieltemperatur
und Restdauer koennen nur ueber die separat spezifizierte, bestaetigte und
protokollierte Laufanpassung geaendert werden.

Das implementierte Laufmodell bildet diese Atomizitaet im Arbeitsspeicher ab:
Es validiert eine kombinierte Anpassung vollstaendig, bevor es wirksame Werte
und den naechsten Eintrag der begrenzten Revisionshistorie gemeinsam
fortschreibt. Ein Neustartzustand wird deterministisch durch Pruefen und
Wiederabspielen des unveraenderlichen Schnappschusses und seiner Revisionen
hergestellt. Der physische Commit, Integritaetsschutz und Rueckfall im
Geraetespeicher folgen mit Issue #17; Issue #13 fuehrt keinen konkreten
Dateisystem- oder Flashzugriff ein.

## Berechtigungsebenen

### Normale Einstellungen

Ohne Service-PIN mindestens:

- Sprache
- Geraetename
- Displayhelligkeit und Abdunkeln
- zulaessige Toneinstellungen
- Zeitzone
- WLAN-Einrichtung
- Webpasswort
- Benutzerprogramme und freigegebene Programmwerte

Weitreichende normale Aktionen wie WLAN-Ersatz, Deaktivierung des Webschutzes
oder Programmloeschung benoetigen eine deutliche Bestaetigung.

### PIN-geschuetzter Service

Die Service-PIN ist im normalen Betrieb mindestens erforderlich fuer:

- Sensorzuordnung und technische Sensoroptionen
- statische IPv4- und Proxy-Vertrauenseinstellungen
- Direktstart-Freigaben
- normale Touchkalibrierung
- technische Regel-, Luefter-, Nachlauf- und Totzeitparameter innerhalb
  firmwarefester Grenzen
- gefuehrte Aktor- und Hardwaretests aus validiertem `STANDBY`
- normale Wiederherstellungsfunktionen
- bewusst aus dem normalen Menue gestarteten vollstaendigen Werksreset

### Ausnahme bei vergessener Service-PIN

Der besondere lokale Recovery-Werksreset ist **nicht** PIN-geschuetzt, weil die
PIN gerade nicht mehr bekannt ist.

Verbindliche Bedingungen:

- nur waehrend Boot oder `SAFE_BOOT`
- nur bei physischer Anwesenheit
- nicht ueber Web oder Netzwerk
- Peltier und alle leistungsbezogenen Aktoren AUS
- mehrstufige Warnung und lange bewusste Bestaetigung
- verifizierter physischer Ausloeseweg, vorzugsweise rohe Touchgeste;
  UART-Loeschen beziehungsweise Neu-Flashen bleibt letzter Recoveryweg
- vollstaendiger Werksreset, kein isolierter PIN-Reset

Die konkrete Geste beziehungsweise Taste bleibt `TBD_HARDWARE`.

### Firmwarefeste Grenzen

Nicht durch Benutzer oder Service-PIN aenderbar:

- absolute Temperatur-Sicherheitsgrenzen
- verbotene gleichzeitige Aktoransteuerung
- firmwarefeste Mindesttotzeit
- zwingende Heiz-/Kuehlsperren
- sichere Ausgaenge bei Boot, Reset und schwerem Fehler
- grundlegende Sensorplausibilitaet
- Datenintegritaets- und Validierungsregeln

## Serviceparameter innerhalb harter Grenzen

Technische Werte sind nur innerhalb firmwarefester Bereiche speicherbar.
Manipulierte Webanfragen werden serverseitig gleich validiert wie lokale Eingaben.
Ein fehlender oder ungueltiger Wert erzeugt keine unsichere Annahme; die Funktion
wird gesperrt, wenn kein eindeutig sicherer Ersatzwert existiert.

Konkrete thermische Werte bleiben `TBD_COMMISSIONING`.

## Vorschau, Speichern und Abbrechen

Aenderungen sind zuerst Entwurf. Dauerhaft wirksam werden sie erst mit
`Speichern` oder gleichwertiger Bestaetigung.

Als reversible Vorschau geeignet:

- Displayhelligkeit
- Sprache der aktuellen Oberflaeche
- ungefaehrliche Darstellungs- und Toneinstellungen

Nicht als ungespeicherte Vorschau aktivieren:

- WLAN-Zugangsdaten
- statische IP und Proxyvertrauen
- Sensorzuordnung
- Regel- und Sicherheitsparameter
- Programmaenderungen
- Wiederherstellungsaktionen

`Abbrechen` stellt Vorschauwerte wieder her. Verlassen mit ungespeicherten
Aenderungen warnt. Browserabbruch speichert nicht. Revisionsschutz verhindert
das stille Ueberschreiben neuerer Daten.

## Verhalten waehrend eines Laufes

Sofort zulaessig, ohne Schnappschusswirkung:

- Oberflaechensprache
- Displayhelligkeit und Abdunkeln
- zulaessige Toneinstellungen
- Stummschalten gemaess Meldungsregeln
- WLAN-Neuverbindung

Programme duerfen bearbeitet werden, wenn klar angezeigt wird, dass dies nur
zukuenftige Laeufe betrifft.

Gesperrt bleiben:

- Sensorzuordnung
- Regel-, Sicherheits-, Luefter- und Peltierparameter
- Touchkalibrierung
- Aktortests
- Import, Wiederherstellung und Werksreset
- neustartpflichtige Systemaenderungen

## Neustartpflichtige Einstellungen

- Speichern loest keinen automatischen Neustart aus.
- Waehrend eines Laufes wird nie automatisch neu gestartet.
- Ausstehende Aenderungen bleiben sichtbar.
- `Jetzt neu starten` erscheint nur in sicherem Zustand ohne Lauf.
- Bis dahin gilt die zuletzt aktivierte gueltige Konfiguration.
- Ein unerwarteter Neustart darf keine halb angewendete Konfiguration erzeugen.

## Zeitbasis und Zeitzone

- absolute Zeit intern in UTC, sofern verlaesslich
- Lauf- und Schutzzeiten mit monotoner Zeit
- konfigurierbare IANA-Zeitzone
- Werkseinstellung `Europe/Zurich`
- lokale Darstellung auf Touch und Web ohne Aenderung gespeicherter UTC-Werte

Ohne NTP wird keine erfundene absolute Zeit ausgegeben. Relative Reihenfolge und
monotone Laufzeit bleiben erhalten. Recovery-Zeit wird gemaess
`RECOVERY_AND_INTERRUPTION.md` als Unsicherheitsintervall behandelt.

## Validierung und atomare Speicherung

Logischer Ablauf:

```text
Entwurf entgegennehmen
-> Datentypen und Pflichtfelder pruefen
-> Wertebereiche und Abhaengigkeiten pruefen
-> firmwarefeste Sicherheitsgrenzen pruefen
-> neue vollstaendige Revision getrennt schreiben
-> Integritaet pruefen
-> Revision atomar aktivieren
-> vorherige gueltige Revision als Rueckfall behalten
```

Anforderungen:

- niemals eine Mischung aus alter und neuer Revision laden
- Integritaet auch beim Laden pruefen
- unbekannte Schema-Version nicht blind interpretieren
- Migration auf Kopie ausfuehren und erst danach aktivieren
- Rueckfall sichtbar protokollieren
- ohne gueltige aktuelle oder Rueckfallrevision sicherer Konfigurationsfehlerzustand
- aktorwirksame Recoveryentscheidung zuerst erfolgreich persistieren
- kritischer Schreibfehler gemaess `RUN_PERSISTENCE.md` verriegeln

## Akzeptierte Entscheidungen

- [x] getrennte Factory-, Benutzer- und Laufebene
- [x] unveraenderlicher Laufschnappschuss
- [x] reversible Vorschau und bewusstes Speichern
- [x] normal, PIN-Service und firmwarefest
- [x] technische Werte nur innerhalb harter Grenzen
- [x] feldbezogenes Verhalten waehrend eines Laufes
- [x] kein automatischer Neustart
- [x] UTC und IANA-Zeitzone `Europe/Zurich`
- [x] vollstaendige Validierung und atomare Revisionen
- [x] normale Vollresetfunktion PIN-geschuetzt
- [x] PIN-unabhaengiger physischer Vollreset ausschliesslich als Recovery bei
      vergessener Service-PIN
