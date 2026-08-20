# Einstellungen und Persistenz

## Status

Dieses Dokument definiert Konfigurationsebenen, Aenderungsrechte, Vorschau,
Neustartbedarf, Zeitdarstellung und atomare Speicherung. Laufpersistenz,
Sicherungen und Recovery werden in `RUN_PERSISTENCE.md`,
`BACKUP_SECURITY_RETENTION.md` und `SYSTEM_SAFETY_AND_RECOVERY.md` ergaenzt.
Der verbindliche technische Speicher-, Wire-, Root-, Preview- und
Recoveryvertrag fuer Issue #16 steht in
[`CONFIGURATION_PERSISTENCE.md`](CONFIGURATION_PERSISTENCE.md).

## Kanonische Grenze zwischen NVS und LittleFS

Für Release 1 ist ESP-IDF NVS das produktive Backend für kleine, strukturierte
und betriebsrelevante Persistenzdaten:

```text
NVS
├── Gerätekonfiguration
├── Kalibrierung
├── UI-Einstellungen
├── Regelparameter
└── kleine persistente Zustände

LittleFS – zunächst nicht erforderlich
└── später optional für Dateien, Webassets oder Logs
```

LittleFS ist damit keine aktuelle Alternative zu `IStateStore`, Schema 1 oder
dem produktiven NVS-Adapter aus #90. Solange NVS zusammen mit der vorhandenen
Konfigurations-/Run-Recovery den R5.7-Produktvertrag erfuellt, bleiben
LittleFS, Embedded-KV-Stores und weitere Backends geschlossen und werden nicht
vorsorglich evaluiert. Ein konkreter Produkt-FAIL wuerde einen neuen
Ownerentscheid und einen vollstaendigen Adopt-or-build-Nachweis mit
Power-Loss-, Lizenz-, Wartungs-, Ressourcen- und Migrationsvertrag erfordern.
Kleine kritische Zustände werden nicht vorsorglich in ein Dateisystem
verschoben. Die reale NVS-/Partitions-/Flashverifikation bleibt ein Gate aus
#29/#90 und wird durch diese Einordnung nicht vorweggenommen.

## Grundsaetze

- Werkseinstellungen, typisierte Konfigurationsdokumente, Geheimnisse und aktive
  Laufdaten sind getrennt.
- Ein Lauf verwendet einen unveraenderlichen Programmschnappschuss.
- Neue Werte werden vor der Aktivierung vollstaendig validiert.
- Keine teilweise geschriebene Konfiguration gilt als gueltig.
- Die letzte nachweislich gueltige Revision bleibt als Rueckfall erhalten.
- Ein waehrend Power-Cut bearbeiteter Record kann alt, neu, fehlen oder nicht
  verwendbar sein; das ist kein bestaetigter Record-Erfolg. Die
  Produkt-Recovery liefert nur vollstaendig validierten neuen Zustand,
  vollstaendig validierten alten/Fallbackzustand oder Recovery-required.
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

getrennt davon: spaetere reale Secret-Domaenen und unveraenderlicher
Laufschnappschuss
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

Die Device-UI ergaenzt diese Ebene nur ueber ihre eigenen versionierten
Vertraege: Build-Konfiguration bestimmt enthaltenes ManuEngineer-Branding,
Sprachpakete, gezielt erzeugte Fonts und Themes. Die aktive lokale Sprache und
das aktive, enthaltene Theme sind persistente Laufzeitwahlen. Ein Branding ist
in Release 1 nicht zur Laufzeit wechselbar; ein unvollstaendiges Theme faellt
sicher auf das Standardtheme zurueck.

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

### Servicefreigabe auf dem lokalen Touchdisplay

Die lokale Servicefreigabe ist eine fluechtige, getrennte Touchsession. Sie
endet nach 10 Minuten Inaktivitaet, bei Geraeteneustart, ausdruecklichem
Abmelden sowie den durch die Safetylogik definierten sicherheitsrelevanten
Zustandswechseln. Die 10 Minuten sind zentraler Build-/Produktparameter, nicht
als normale oder Serviceeinstellung speicher- oder aenderbar. Release 1 besitzt
keine absolute Maximaldauer fuer diese lokale Freigabe.

Die Web-Servicefreigabe bleibt davon getrennt und folgt weiterhin den in
`WEB_UI.md` definierten 5 Minuten Inaktivitaet und 15 Minuten absolut. Beide
Freigaben ersetzen keine fachliche oder Safetypruefung.

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
dieselbe Validierung. Das Preview bleibt fluechtig und haelt einen
unveraenderlichen typisierten Kandidaten, die erwartete aktive Basis, eine
Kandidatenintegritaet, die Aktivierungswirkung und eine redigierte
Aenderungszusammenfassung. Beim Commit wird kein Kandidat von der Oberflaeche
erneut uebernommen. Basis, Integritaet, Validierung und Aktivierungswirkung
werden unmittelbar vor dem Commit erneut geprueft; veraltete oder anders
wirkende Kandidaten benoetigen eine neue Vorschau und Bestaetigung. Neustart
verwirft jede Vorschau. Anzahl, Lebensdauer und Ressourcenobergrenzen werden im
Detailplan und Ressourcennachweis von #56 festgelegt.

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

Release 1 besitzt keinen persistenten Pending-Zweig und kein
Aktivierungsintent. Ein Kandidat bleibt bis zum atomaren Commit fluechtig.
Neustartpflichtige Aenderungen werden nicht als voraktivierter persistenter
Zweig gespeichert. Falls ein spaeterer Produktbedarf dafuer nachgewiesen wird,
benoetigt er eine additive Vertrags- und Ownerentscheidung; Variante B wird
nicht vorsorglich um leere Zukunftsstrukturen erweitert.

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
- Migration ueber denselben vollstaendig validierten Active-/Fallback-
  Commitpfad ausfuehren
- Rueckfall sichtbar protokollieren
- ohne gueltige aktuelle oder Rueckfallrevision sicherer Konfigurationsfehlerzustand
- vorhandene beschaedigte Daten niemals als fabrikneuen Speicher behandeln
- neue Active-Generation ausschliesslich durch atomaren Root-Commit aktivieren
- vor Root-Commit alle falliblen Plattform- und Ressourcenarbeiten abschliessen
- nach Root-Commit nur nicht allokierenden, nicht fehlschlagenden Runtime-
  Snapshot veroeffentlichen
- `CommitOutcomeUnknown` nur nach vollstaendigem Root-/Graph-Readback als
  `NEW_VALID_CONFIGURATION`, `OLD_VALID_CONFIGURATION`/
  `FALLBACK_VALID_CONFIGURATION` oder `CONFIGURATION_RECOVERY_REQUIRED`
  aufloesen
- bei nicht abschliessbarem Readback stabil typisiert fail closed bleiben:
  kein Snapshot-Publish, keine normale Runtime, keine weitere Mutation oder
  Slotwiederverwendung, kein heuristischer Rollback und keine Factory-New-
  Annahme; eine explizit validierte Fallbackgeneration bleibt zulaessig
- unbestimmten Commitzustand sowie Konfigurations-, Integritaets- und
  Verfuegbarkeitsfehler verbindlich ueber das
  `CONFIGURATION_SAFETY_INTEGRATION_GATE` in #24 integrieren
- reale Geheimnisse und Authentifizierungsnachweise erst mit ihrem ersten
  produktiven Konsumenten in getrennten, typisierten und epochengebundenen
  Revisionsdomaenen einfuehren
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
- [x] kein persistenter Pending-Zweig und kein Aktivierungsintent in R1
- [x] fluechtige basis- und integritaetsgebundene Vorschau mit
      Konflikterkennung
- [x] unbestimmter Root-Commit-Ausgang bleibt bis zum erfolgreichen Vollscan
      fail closed
- [x] nachgelagertes `CONFIGURATION_SAFETY_INTEGRATION_GATE` vor Abschluss von
      #24
- [x] kanonisches binaeres Envelope- und CRC-Wireformat
- [x] reale Connectivity-/Authentication-Domaenen erst mit ihrem ersten
      produktiven Konsumenten
- [x] wiederaufnehmbarer Bootstrap und Werksreset mit StorageEpoch
- [x] feste Slot-, Payload- und Recordgrenzen; Previewbudget im #56-Nachweis
- [x] normale Vollresetfunktion PIN-geschuetzt
- [x] PIN-unabhaengiger physischer Vollreset ausschliesslich als Recovery bei
      vergessener Service-PIN
