# Einstellungen und Persistenz

## Status

Dieses Dokument definiert Konfigurationsebenen, Aenderungsrechte, Vorschau,
Neustartbedarf, Zeitdarstellung und atomare Speicherung. Laufpersistenz,
Sicherungen und Recovery werden in `RUN_PERSISTENCE.md`,
`BACKUP_SECURITY_RETENTION.md` und `PR38_REVIEW_CORRECTIONS.md` ergaenzt.
Der verbindliche technische Speicher-, Wire-, Root-, Preview- und
Recoveryvertrag fuer Issue #16 steht in
[`CONFIGURATION_PERSISTENCE.md`](CONFIGURATION_PERSISTENCE.md).

## Grundsaetze

- Werkseinstellungen, typisierte Konfigurationsdokumente, Geheimnisse und aktive
  Laufdaten sind getrennt.
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
unveraenderliche FactoryConfiguration in der Firmware
        ↓
UserConfiguration + ServiceConfiguration + ProgramCatalog
        ↓
ActiveConfigurationManifest als gemeinsam aktivierte Generation

getrennt davon: Secret-Domaene und unveraenderlicher Laufschnappschuss
```

### Werkseinstellungen

Enthalten mindestens:

- allgemeine Factory-Werte
- vier Standardprogramme
- Schema- und Migrationsgrundlagen
- firmwarefeste Sicherheitsgrenzen
- verbotene Aktorkombinationen
- nicht unterschreitbare Schutzzeiten

Sie werden im normalen Betrieb nicht ueberschrieben und nicht als
ueberschreibbare Kopie in jedes Dokument eingefuegt. Ein Werksreset erzeugt aus
ihnen eine neue gueltige Konfigurationsgeneration.

### Benutzereinstellungen

Die erste Schemageneration enthaelt nur bereits fachlich bestimmte Werte:

- lokale Displaysprache
- kanonische IANA-Zeitzone
- sichtbarer Geraetename
- ein typisiertes, noch parameterloses ServiceConfiguration-Dokument
- den gespeicherten ProgramCatalog

Spaetere Schemagenerationen koennen nach Festlegung durch ihre zustaendigen
Issues beispielsweise enthalten:

- Sprache, Geraetename, Display und Ton
- WLAN und Webzugang
- Programme und Benutzervoreinstellungen
- freigegebene Serviceparameter
- Touchkalibrierung
- IANA-Zeitzone

Jeder Dokumenttyp besitzt eine eigene Schemaentwicklung und Inhaltsrevision.
Dokumente werden nicht einzeln aktiviert: Ein Manifest referenziert genau eine
vollstaendig validierte Kombination und bildet die gemeinsame
Konfigurationsgeneration. Optionale UTC-Zeit ist nur Metadatum; Reihenfolge und
Konflikte beruhen ausschliesslich auf monotonen Generationen.

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

Alle Kanaele verwenden denselben fachlichen `ConfigurationPreview`-Dienst und
dieselbe Validierung. Release 1 besitzt genau einen globalen, hoechstens 15
Minuten lebenden Previewplatz. Das Preview haelt einen unveraenderlichen
typisierten Kandidaten, Basisgenerationen, Aktivierungswirkung, redigierte
Aenderungszusammenfassung, Ownerbindung und einen nicht vorhersagbaren
Bestaetigungs-Token. Beim Commit wird kein Kandidat von der Oberflaeche erneut
uebernommen. Basis, Validierung und Aktivierungswirkung werden unmittelbar vor
dem Commit erneut geprueft; veraltete oder anders wirkende Kandidaten benoetigen
eine neue Vorschau und Bestaetigung.

Fluechtige UI-Vorschauen veraendern weder Active noch Pending. Ein persistiertes
Pending wird nur durch eine ausdrueckliche Aktion ersetzt oder verworfen.

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

- Neben Active existiert hoechstens ein vollstaendig validiertes Pending-
  Manifest.
- Speichern einer neustartpflichtigen oder gemischten Bearbeitung erzeugt einen
  vollstaendigen Pending-Kandidaten und loest keinen Neustart aus.
- Sobald Pending existiert, bauen auch sofort wirksame gespeicherte Aenderungen
  darauf auf; es entsteht kein paralleler Active-Zweig.
- Weitere Pending-Revisionen ersetzen den bisherigen Kandidaten erst nach
  vollstaendiger Validierung.
- Ausstehende Aenderungen bleiben sichtbar und koennen bewusst ersetzt oder
  verworfen werden.
- `Anwenden und neu starten` ist nur in sicherem Zustand ohne aktiven oder
  wiederherzustellenden Lauf erlaubt.
- Die Aktion persistiert eine an Pending-Integritaet, Pending-Generation und
  erwartete Active-Generation gebundene Aktivierungsabsicht und sperrt danach
  neue Laeufe und Mutationen bis zum unmittelbaren kontrollierten Neustart.
- Ein unerwarteter Neustart ohne passende Absicht aktiviert Pending niemals.
- Nach erfolgreichem Root-Commit wird ein unterbrochener Abschluss idempotent
  fortgesetzt; vor der Commit-Grenze bleibt die alte Generation aktiv.

## Zeitbasis und Zeitzone

- absolute Zeit intern in UTC, sofern verlaesslich
- Lauf- und Schutzzeiten mit monotoner Zeit
- Revisionsreihenfolge und Konflikterkennung nur durch monotone Generationen
- optionale UTC-Revisionsmetadaten ohne ordnende Wirkung
- ausschliesslich kanonische, kataloggepruefte IANA-Zeitzone
- Werkseinstellung `Europe/Zurich`
- lokale Darstellung auf Touch und Web ohne Aenderung gespeicherter UTC-Werte
- Zeitzonenaenderung ist nicht neustartpflichtig und veraendert keinen Lauf

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

- getrennte typisierte Dokumentrevisionen, aber genau eine gemeinsam aktivierte
  Manifestgeneration
- nur geaenderte Dokumente neu schreiben und unveraenderte Revisionen sicher
  weiterreferenzieren
- niemals eine Mischung aus Dokumenten verschiedener Manifestgenerationen laden
- Integritaet auch beim Laden pruefen
- unbekannte Schema-Version nicht blind interpretieren
- Migration auf Kopie ausfuehren und erst danach aktivieren
- Active und Pending getrennt migrieren; Pending dadurch nie aktivieren
- Rueckfall sichtbar protokollieren
- ohne gueltige aktuelle oder Rueckfallrevision sicherer Konfigurationsfehlerzustand
- vorhandene beschaedigte Daten niemals als fabrikneuen Speicher behandeln
- neue Active-Generation ausschliesslich durch atomaren Root-Commit aktivieren
- vor Root-Commit alle falliblen Plattform- und Ressourcenarbeiten abschliessen
- nach Root-Commit nur nicht allokierenden, nicht fehlschlagenden Runtime-
  Snapshot veroeffentlichen
- Geheimnisse und Authentifizierungsnachweise in getrennten, typisierten
  Revisionsdomaenen halten
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
- [x] hybride Dokumentarchitektur mit gemeinsamem Active-Manifest
- [x] genau ein Pending-Zweig ohne parallele Active-Aenderungen
- [x] generationengebundene zentrale Vorschau und Konflikterkennung
- [x] kanonisches binaeres Envelope- und CRC-Wireformat
- [x] getrennte Connectivity- und vorwaertsgerichtete Authentication-Rueckfallpolitik
- [x] wiederaufnehmbarer Bootstrap und Werksreset mit StorageEpoch
- [x] feste Slot-, Payload-, Preview- und Recordgrenzen
- [x] normale Vollresetfunktion PIN-geschuetzt
- [x] PIN-unabhaengiger physischer Vollreset ausschliesslich als Recovery bei
      vergessener Service-PIN
