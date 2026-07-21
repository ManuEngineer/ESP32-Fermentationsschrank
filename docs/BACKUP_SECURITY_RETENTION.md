# Geheimnisse, Sicherungen und Datenaufbewahrung

## Status

Dieses Dokument beschreibt die in Phase 6C akzeptierten Regeln fuer
Zugangsdaten, Sicherung und Import, Laufexporte, lokale Aufbewahrung,
Speicherbereinigung, Werksreset und den Umgang mit einer vergessenen
Service-PIN.

Es ergaenzt [`SETTINGS_AND_STORAGE.md`](SETTINGS_AND_STORAGE.md) und
[`RUN_PERSISTENCE.md`](RUN_PERSISTENCE.md).

## Bestaetigte Speicherbasis

Fuer das bestellte Controllerboard ist laut vorliegender Produktbeschreibung ein
ESP32-32E-Modul mit **4 MB Flash** vorgesehen.

Verbindliche Planungsannahmen:

- Das erste Release muss in 4 MB Flash funktionieren.
- PSRAM wird nicht vorausgesetzt.
- Die tatsaechlich erkannte Flashgroesse und der wirksame Partitionsplan werden
  trotzdem bei der Hardwareinbetriebnahme und in einer Test-Firmware geprueft.
- OTA-Reserve, Firmware, Webressourcen, Konfiguration, aktiver Laufzustand und
  Historie erhalten feste getrennte Budgets.
- Eine Funktion darf nicht unbegrenzt wachsen oder den fuer den aktiven Lauf
  reservierten Speicher verbrauchen.

## Speicherung von Webpasswort und Service-PIN

Das normale Webpasswort und die vierstellige Service-PIN werden nicht im
Klartext gespeichert.

Gespeichert werden mindestens:

- eine gesalzene, nicht umkehrbare Pruefinformation
- eine eindeutige Versionskennung des verwendeten Pruefverfahrens
- die fuer eine spaetere Migration notwendigen nicht geheimen Parameter

Verbindliche Regeln:

- Webpasswort und Service-PIN besitzen getrennte Pruefinformationen und Salze.
- Die Anwendung muss die Eingabe pruefen koennen, ohne das urspruengliche
  Passwort oder die PIN wieder auszulesen.
- Vergleiche und Fehlermeldungen duerfen keine verwertbaren Informationen ueber
  Teiltreffer preisgeben.
- Fehlversuche werden begrenzt beziehungsweise zeitlich verzoegert.
- Die geringe Entropie einer vierstelligen PIN wird nicht durch Hashing allein
  sicher; physischer Zugangsschutz, Rate-Limiting und die beschraenkten
  Servicefunktionen bleiben notwendig.
- Das genaue ressourcengerechte Passwort-Pruefverfahren wird im technischen
  Entwurf festgelegt und versioniert.

## Speicherung des WLAN-Passworts

Das WLAN-Passwort muss im Gegensatz zum Webpasswort wiederverwendbar sein, weil
der ESP32 es fuer einen erneuten Verbindungsaufbau benoetigt.

Deshalb gilt:

- Das WLAN-Passwort wird getrennt von normalen Einstellungen behandelt.
- Es wird nie in normalen Anzeigen, Protokollen, Diagnoseexporten oder
  Sicherungsdateien ausgegeben.
- Die Speicherabstraktion wird so aufgebaut, dass eine plattformseitige
  Verschluesselung beziehungsweise spaetere Flash-Verschluesselung verwendet
  werden kann.
- Flash-Verschluesselung und Secure Boot sind keine zwingende Voraussetzung fuer
  das erste Release.
- Ohne aktivierte Hardware- oder Flash-Verschluesselung besteht bei physischem
  Auslesen des Flashs eine verbleibende Sicherheitsgrenze. Die Firmware darf
  keine Scheinsicherheit durch blosse reversible Verschleierung behaupten.

## Normaler Sicherungsexport

Die normale Geraetesicherung enthaelt ausschliesslich wiederherstellbare,
nicht geheime Anwendungsdaten.

Mindestens exportierbar sind:

- Benutzerprogramme
- veraenderte Standardprogramme
- freigegebene normale Einstellungen
- freigegebene Serviceparameter ohne Geheimnisse
- Geraetename und Zeitzone
- Display-, Ton- und Sprachkonfiguration
- nicht geheime Netzwerkparameter, soweit fuer eine Wiederherstellung sinnvoll
- Schema- und Exportversion

Nicht enthalten sind:

- WLAN-Passwort
- Passwort des Einrichtungs- oder Ersatz-WLANs
- Webpasswort oder dessen direkt wiederverwendbare Darstellung
- Service-PIN
- Sitzungskennungen und Anmeldetokens
- CSRF-Tokens
- private Schluessel oder spaetere Integrationstokens
- aktive Servicefreigaben

Das Fehlen der Geheimnisse wird in der Exportzusammenfassung sichtbar erklaert.
Nach einem Import muessen betroffene Zugangsdaten bei Bedarf neu eingerichtet
werden.

### Entwicklerzugang ueber UART

Eine vollstaendige Sicherung mit Geheimnissen ist kein Bestandteil der normalen
Oberflaeche und keine Anforderung an das erste Release.

Fuer spaetere Entwicklung oder Reparatur darf ein physisch lokaler
Entwicklerweg ueber UART beziehungsweise externe Flash-Werkzeuge vorgesehen
werden. Dabei gilt:

- kein Menuepunkt fuer normale Benutzer
- keine automatische oder entfernte Uebertragung
- keine Verpflichtung, ein anwendungsseitiges Klartext-Geheimnisbackup zu bauen
- physischer Zugriff und ausdruecklicher Entwicklungs- oder Wartungsmodus
- eine rohe Flashkopie gilt nicht als portabler Anwendungsimport

Falls dieser Entwicklerweg nicht sicher und ressourcenschonend umgesetzt werden
kann, wird er im ersten Release weggelassen.

## Export einzelner Laufhistorien

Einzelne abgeschlossene Laeufe koennen separat exportiert werden.

Unterstuetzte Formate:

- JSON als vollstaendiges maschinenlesbares Format
- CSV fuer tabellarische Temperaturdaten, soweit sinnvoll

Ein Laufexport enthaelt mindestens:

- Lauf-ID und Programmschnappschuss
- Start-, Abschluss- und Abbruchinformationen
- Temperaturaggregate
- Phasenwechsel
- Warnungen und Fehler
- Stromunterbrechungen und Wiederanlaufaktionen
- Sensorwechsel
- Laufzeitkorrekturen
- Zeitqualitaet und Zeitzone

Geheimnisse und aktuelle Sitzungsdaten werden auch in Laufexporten niemals
enthalten.

## Import einer Sicherung

Eine Sicherung wird niemals direkt ueber die aktive Konfiguration geschrieben.

Verbindlicher Ablauf:

```text
Datei empfangen
  -> Dateityp und Groesse pruefen
  -> Export- und Schema-Version pruefen
  -> Integritaet und Pflichtfelder pruefen
  -> Wertebereiche und firmwarefeste Grenzen pruefen
  -> erforderliche Migration auf einer Arbeitskopie ausfuehren
  -> Unterschiede und betroffene Daten anzeigen
  -> ausdrueckliche Bestaetigung verlangen
  -> neue vollstaendige Revision getrennt schreiben
  -> Integritaet pruefen
  -> atomar aktivieren
  -> bisherige gueltige Revision als Rueckfall behalten
```

Regeln:

- Ein Import startet niemals automatisch einen Lauf.
- Ein laufender Prozess wird durch einen Import nicht veraendert.
- Lauf- oder regelungsrelevante Importe sind waehrend eines aktiven Laufes
  gesperrt beziehungsweise nur fuer spaetere Aktivierung zulaessig.
- Unbekannte neuere Schema-Versionen werden nicht blind interpretiert.
- Eine fehlgeschlagene Migration veraendert die aktive Konfiguration nicht.
- Vor der Bestaetigung wird angezeigt, welche Programme, Einstellungen und
  Daten ersetzt, ergaenzt oder ignoriert werden.
- Da normale Sicherungen keine Geheimnisse enthalten, bleiben vorhandene
  Geheimnisse unveraendert oder muessen bewusst neu eingerichtet werden; das
  genaue feldbezogene Verhalten wird im Importschema dokumentiert.

## Lokale Aufbewahrung abgeschlossener Laeufe

Die Aufbewahrung ist im PIN-geschuetzten Servicebereich konfigurierbar, aber nur
innerhalb eines firmwarefesten maximalen Speicherbudgets.

Werkseinstellung:

```text
Aktiver Lauf:                         vollstaendig
Letzte abgeschlossene Laeufe:         5 mit Diagrammdaten
Abgeschlossene Laufzusammenfassungen: 50
```

Die Begriffe bedeuten:

- **Diagrammdaten:** verdichtete Temperaturaggregate und exakte
  Ereignismarkierungen
- **Zusammenfassung:** Programm, Zeiten, Ergebnis, wesentliche Warnungen,
  Unterbrechungen und Abschlussgrund ohne vollstaendige Temperaturkurve

Verbindliche Regeln:

- Der aktive Lauf und seine Wiederherstellungsrevisionen besitzen hoechste
  Speicherprioritaet.
- Die konfigurierbaren Zahlen duerfen das feste Historienbudget nicht
  ueberschreiten.
- Die Oberflaeche zeigt den ungefaehren Speicherbedarf beziehungsweise die
  daraus resultierende Aufbewahrung an.
- Eine hoehere Anzahl Zusammenfassungen darf nicht unbemerkt den Platz fuer
  detaillierte aktive Laufdaten reduzieren.
- Die tatsaechlichen Datensatzgroessen werden vor Implementierung mit dem
  4-MB-Partitionsplan gemessen.

## Verhalten bei vollem Historienspeicher

Ein voller Historienspeicher darf weder einen neuen Lauf verhindern noch die
Temperaturregelung oder Wiederherstellung gefaehrden.

Bereinigungsreihenfolge:

1. veraltete temporaere Exportdateien beziehungsweise unvollstaendige
   nichtkritische Arbeitsdaten entfernen
2. aelteste nicht mehr geschuetzte Diagrammdaten abgeschlossener Laeufe
   entfernen
3. betroffene Laeufe als Zusammenfassung erhalten
4. erst gemaess definierter Aufbewahrungsgrenze die aeltesten
   Zusammenfassungen entfernen

Dabei gilt:

- Vor beziehungsweise bei einer automatischen Bereinigung wird eine sichtbare
  Meldung und ein Ereigniseintrag erzeugt.
- Die Bereinigung erfolgt nicht still.
- Wichtige Fehler-, Reset- und Sicherheitsereignisse werden laenger als normale
  Detailmessdaten aufbewahrt, soweit das feste Budget dies erlaubt.
- Der aktive Wiederherstellungsdatensatz, seine Rueckfallrevision und
  firmwarekritische Konfiguration werden nie zugunsten alter Diagramme
  geloescht.
- Ist auch der reservierte kritische Speicher beschaedigt oder erschoepft,
  erfolgt kein blindes Weiterarbeiten; die Fehlerbehandlung wird in
  `SAFETY_AND_FAULTS.md` festgelegt.

## Vollstaendiger Werksreset

Der vollstaendige Werksreset ist eine lokale, PIN-geschuetzte und mindestens
zweistufig bestaetigte Aktion, solange die Service-PIN bekannt ist.

Er loescht beziehungsweise setzt zurueck:

- Benutzerprogramme
- veraenderte und geloeschte Standardprogramme
- normale und Serviceeinstellungen
- WLAN-Zugangsdaten
- Einrichtungs- und Ersatz-WLAN-Passwort
- Webpasswort und Websitzungen
- Service-PIN und Servicefreigaben
- Proxy- und Netzwerkkonfiguration
- Laufhistorien, Diagrammdaten und Ereignisprotokolle
- aktiven beziehungsweise wiederherstellbaren Laufzustand

Er stellt wieder her:

- firmwarefeste Werkseinstellungen
- die vier Standardprogramme aus dem Factory-Katalog
- sicheren Erst-Einrichtungszustand

### Touchkalibrierung bleibt erhalten

Die gespeicherte Touchkalibrierung bleibt beim normalen vollstaendigen
Werksreset erhalten, weil sie geraetespezifisch ist und nicht zur
Benutzerkonfiguration gehoert.

Sie kann weiterhin separat ueber:

- `Touchkalibrierung zuruecksetzen`
- den bereits festgelegten Boot-Wiederherstellungsweg

entfernt beziehungsweise neu aufgebaut werden.

Wenn die Kalibrierungsdaten selbst ungueltig oder beschaedigt sind, werden sie
nicht blind weiterverwendet.

## Vergessene Service-PIN

Es gibt keinen Wiederherstellungsweg, der lediglich die Service-PIN entfernt und
dabei die bestehende geschuetzte Konfiguration vollstaendig erhaelt.

Bei vergessener Service-PIN ist nur ein **vollstaendiger lokaler Werksreset**
erlaubt.

Anforderungen an diesen Wiederherstellungsweg:

- physische Anwesenheit am Geraet
- keine Ausloesung ueber die normale Weboberflaeche oder Lese-API
- klarer Hinweis, dass Benutzerprogramme, Zugangsdaten und Laufhistorien
  geloescht werden
- mehrstufige lokale Bestaetigung
- keine Moeglichkeit, vorher auf bestehende Serviceeinstellungen oder
  Geheimnisse zuzugreifen
- alle Aktoren bleiben waehrend des Wiederherstellungsablaufs sicher AUS
- Touchkalibrierung bleibt gemaess normalem Werksreset erhalten

Der genaue physische Boot-Ablauf wird in der Diagnose- und Wartungsphase
festgelegt. Er darf nicht mit dem zehnsekundigen reinen
Touchkalibrierungs-Wiederherstellungsweg verwechselt werden.

## Akzeptierte Entscheidungen aus Phase 6C

- [x] Zielhardware mit 4 MB Flash; keine PSRAM-Abhaengigkeit
- [x] Webpasswort und Service-PIN nur als gesalzene Pruefinformation speichern
- [x] WLAN-Passwort getrennt als wiederverwendbares Geheimnis behandeln
- [x] Architektur fuer spaetere plattformseitige beziehungsweise
      Flash-Verschluesselung vorbereiten
- [x] normaler Sicherungsexport ohne Passwoerter, PINs, Sitzungen oder Tokens
- [x] optionaler physischer Entwicklerweg ueber UART bleibt spaeter moeglich,
      ist aber keine Release-Anforderung
- [x] einzelne Laufhistorien als JSON und bei Bedarf CSV exportierbar
- [x] Import erst validieren, migrieren, anzeigen, bestaetigen und atomar aktivieren
- [x] konfigurierbare Aufbewahrung innerhalb eines festen Speicherbudgets
- [x] Werkseinstellung: 5 detaillierte Laeufe und 50 Zusammenfassungen
- [x] kontrollierte automatische Bereinigung alter nichtkritischer Historie
- [x] vollstaendiger Werksreset behaelt die Touchkalibrierung
- [x] vergessene Service-PIN nur durch vollstaendigen lokalen Werksreset

## Noch offen fuer Phase 7, 8 und 9

- konkreter 4-MB-Partitionsplan und OTA-Aufteilung
- gemessene Firmware-, Web-, RAM- und Flashbudgets
- genaues Passwort-Pruefverfahren und Migrationsstrategie
- technische Speicherung des wiederverwendbaren WLAN-Geheimnisses
- konkretes Sicherungs-, Laufexport- und Import-JSON-Schema
- maximale Import- und Exportdateigroesse
- genaue Aufteilung des festen Historienbudgets
- konkrete Datensatzgroessen und Verdichtungsstufen
- physischer Boot-Ablauf fuer einen Werksreset bei vergessener PIN
- Verhalten bei physisch beschaedigtem Flash
