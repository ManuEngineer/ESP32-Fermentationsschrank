# Sicherung, Geheimnisse und Datenaufbewahrung

Der technische Revisions-, Referenz- und Aktivierungsvertrag der
Konfigurationsdomaene steht in
[`CONFIGURATION_PERSISTENCE.md`](CONFIGURATION_PERSISTENCE.md). Dieses Dokument
definiert die fachliche Backup-, Geheimnis- und Aufbewahrungspolitik.

## Plattformrahmen

- ESP32-32E
- 4 MB Flash als harte Planungsbasis
- keine PSRAM-Abhängigkeit
- Release 1 verwendet UART/FT232RL für Updates und Recovery
- Release 1 benötigt keine OTA-Slots oder OTA-Reserve
- konkrete Partitionen und Datenbudgets bleiben `TBD_IMPLEMENTATION_BUDGET`

## Geheimnisse

Geheimnisse und Authentifizierungsnachweise liegen ausserhalb der
Konfigurationsdokumente. #16, #56 und #57 bereiten dafuer keine leeren
`NotProvisioned`-Manifeste, Roots, Slots oder Reservepayloads vor. Eine reale
Connectivity- beziehungsweise Authentication-Domaene entsteht erst mit ihrem
ersten produktiven Konsumenten und verwendet eigene typisierte, versionierte
und an die aktuelle `StorageEpoch` gebundene Records.

Connectivity muss spaeter den kontrollierten Credentialwechsel und Recovery
definieren. Authentication bleibt vorwaertsgerichtet: Eine erfolgreich
aktivierte neuere Credential-Epoche darf durch Konfigurationsfallback oder
Recovery niemals auf eine aeltere Credential-Epoche zurueckfallen. Diese
fachlichen Vertraege werden nicht durch vorzeitig leere Speicherstrukturen
ersetzt.

### Webpasswort

- wird nicht im Klartext gespeichert
- gesalzener, einseitiger Prüfnachweis
- begrenzte Anmeldeversuche und zeitliche Verzögerung
- niemals in Backup, Diagnose oder Export

### Service-PIN

- getrennt vom Webpasswort
- gesalzener, einseitiger Prüfnachweis
- lokale und Web-Serviceaktionen verwenden dieselbe Berechtigungslogik
- niemals auslesbar oder exportierbar

### WLAN-Passwort

Das WLAN-Passwort muss für eine erneute Verbindung verwendbar bleiben und kann
daher nicht nur als Prüfnachweis gespeichert werden.

Anforderungen:

- eigener, klar getrennter Speicherbereich
- nie in normalen Backups oder Diagnoseexporten
- nie in Logs oder UART-Ausgaben
- Zugriff nur durch Netzwerkadapter
- zukünftige Flashverschlüsselung darf ergänzt werden

Ohne aktivierte Plattform- beziehungsweise Flashverschlüsselung wird keine
falsche kryptografische Sicherheit behauptet.

Fehlende, ungueltige oder leere Secret-Werte gelten nicht als Zugangsdaten.
Integritaetspruefung durch CRC ist weder Verschluesselung noch
Authentifizierung. Alte Flashbytes werden durch StorageEpoch und
Referenzwechsel logisch unerreichbar; eine kryptografisch sichere physische
Loeschung wird ohne konkreten Plattformnachweis nicht zugesichert.

## Normales Backup

Ein normales Backup ist ein vollstaendiges, bewusst versioniertes Bundle aus
einem portablen Manifest und allen von ihm referenzierten dekodierten
Konfigurationsdokumenten. Das portable Manifest ist eine secret-freie Projektion
der Konfigurationsgeneration und enthaelt keine Connectivity- oder
Authentication-Bindung. Es werden keine rohen internen Envelopes exportiert.
Das konkrete portable Format und sein Import werden mit Issue #19 implementiert.

Enthalten:

- Benutzerprogramme
- aktive Auswahl der Standardprogramme
- sichere Benutzereinstellungen
- erlaubte Serviceparameter
- Sprache, Zeitzone und UI-Einstellungen
- Aufbewahrungs- und Exportoptionen
- Schema- und Revisionsinformationen

Nicht enthalten und nicht gebunden:

- WLAN-Passwort
- Webpasswort oder Prüfnachweis
- Service-PIN oder Prüfnachweis
- Sitzungen, Tokens und CSRF-Daten
- private Schlüssel
- aktiver flüchtiger Anmeldezustand
- rohe Flashpartitionen

Nach Import muessen fehlende Zugangsdaten neu eingerichtet werden. Fehlende
Secrets ueberschreiben keine auf dem Ziel vorhandenen Secrets. Ein
Importkandidat mit einer von der laufenden Firmware nicht unterstuetzten IANA-
Zeitzone wird abgelehnt oder muss vor Bestaetigung bewusst korrigiert werden;
es gibt keinen stillen Rueckfall auf `Europe/Zurich`.

## Entwickler- und physischer Backupweg

Eine spätere physische Sicherung über UART oder externen Flashzugang kann für den
Entwickler interessant sein, ist aber keine Release-1-Anwendungsfunktion.

Eine rohe Flashkopie:

- ist nicht portabel zwischen Hardware- oder Partitionsständen
- ist kein normaler Import
- kann Geheimnisse enthalten
- darf nicht über die Weboberfläche angeboten werden

## Laufexport

Jeder Lauf kann einzeln exportiert werden.

### JSON

- vollständiger Programmschnappschuss
- effektive Laufrevisionen
- Phasen und Ereignisse
- Temperaturen und Qualitätsstatus
- Warnungen, Fehler, Unterbrechungen und Korrekturen
- Abschluss- oder Abbruchgrund

### CSV

- geeignete Zeitreihen
- Zeitbezug und Zeitqualität
- Soll- und Istwerte
- Schrankluft, Produkt und Kühlkörper
- Sensorstatus
- Phase und relevante Aktorereignisse

Kein Laufexport enthält Geheimnisse.

## Import

Verbindlicher Ablauf:

```text
Datei einlesen
-> Format und Grösse prüfen
-> Schema identifizieren
-> Integrität prüfen
-> unterstützte Migration anwenden
-> gesamten Inhalt validieren
-> Vorschau und Konflikte anzeigen
-> ausdrückliche Bestätigung
-> neue Revision atomar speichern
-> erst danach aktivieren
```

Regeln:

- keine teilweise Aktivierung
- Import nur bei sicher festgestelltem `NoActiveOrRecoverableRun`; aktiver,
  unterbrochener, wiederherstellbarer oder unbekannter Laufzustand blockiert
- Quellprogramm und Factory-Katalog nicht still überschreiben
- unbekannte kritische Felder führen zur Ablehnung
- ältere Schemas nur über getestete Migration
- fehlende Geheimnisse werden nicht mit leeren Werten überschrieben
- Import verwendet denselben zentralen Preview-, Validierungs-, Konflikt- und
  Bestaetigungspfad wie Display und Web
- Importkandidat und Vorschau bleiben bis zur atomaren Aktivierung fluechtig;
  es entsteht kein Pending oder paralleler Active-Zweig
- das interne binaere Revisionsformat ist kein portables Backupformat

## Aufbewahrungsmodell

Werkseinstellung:

```text
Aktiver Lauf:                 vollständig
Abgeschlossene Detaildaten:   letzte 5 Läufe
Zusammenfassungen:            letzte 50 Läufe
```

Diese Werte dürfen nur innerhalb eines festen, gemessenen Speicherbudgets
konfiguriert werden.

## Verhalten bei knappem Speicher

Normale proaktive Reihenfolge:

1. verwaiste temporäre Exportdateien entfernen
2. älteste nichtkritische Diagrammdetails verdichten
3. ältere Detaildatensätze zu Zusammenfassungen reduzieren
4. älteste nichtkritische Zusammenfassungen entfernen
5. Sicherheits-, Reset- und Fehlerereignisse länger erhalten
6. aktiven Lauf, Kontrollpunkte und gültige Konfigurationen schützen

Die Bereinigung erfolgt sichtbar und protokolliert, aber ohne unnötige
Benutzerunterbrechung.

Wenn nach normaler Bereinigung keine sichere Reserve hergestellt werden kann:

- keinen neuen Lauf starten, falls kritische Persistenz nicht garantiert ist
- laufenden Prozess nur stoppen, wenn Regelung oder kritische Persistenz
  tatsächlich nicht mehr sicher möglich ist
- nie den aktiven Wiederherstellungsdatensatz zugunsten alter Diagramme löschen

## Werksreset

Ein vollständiger lokaler Werksreset löscht:

- Benutzerprogramme
- Benutzereinstellungen
- Serviceparameter
- WLAN-Zugangsdaten
- Webpasswort
- Service-PIN
- Sitzungen und Tokens
- Laufhistorie und Zusammenfassungen
- aktive Laufdaten
- Fehler- und Komforthistorie soweit für den Reset vorgesehen

Er stellt wieder her:

- Factory-Standardprogramme
- Factory-Grenzen
- Ersteinrichtungszustand

Er behält:

- gerätespezifische Touchkalibrierung

Der Reset ist lokal, mehrstufig und bei jedem Schritt aktorsicher.

Technisch ist der Reset ein mit `BootstrapState::Resetting`
wiederaufnehmbarer Epochenwechsel. Er invalidiert Active und Fallback der alten
Epoche und erzeugt eine neue Initialkonfiguration. Spaetere reale Connectivity-
und Authentication-Domaenen muessen ihre eigenen epochengebundenen Reset- und
Widerrufsregeln mitbringen; #57 bereitet sie nicht leer vor. Der Reset wird nie
automatisch aufgrund beschaedigter Konfiguration gestartet.

## Vergessene Service-PIN

Es gibt keinen separaten PIN-Bypass. Wiederherstellung ist nur durch den
vollständigen lokalen Werksreset möglich.

Der Ablauf:

- physischer lokaler Zugriff erforderlich
- keine Remote-Auslösung
- klare Warnung über Datenverlust
- mehrere Bestätigungsschritte
- Peltier und Aktoren AUS
- danach vollständige Ersteinrichtung

## Ressourcenpriorität

1. sichere Firmware und Bootfähigkeit
2. gültige Konfiguration und Rückfallrevision
3. aktiver Lauf und Wiederherstellung
4. Sicherheits- und Resetjournal
5. notwendige Web- und UI-Ressourcen
6. Laufzusammenfassungen
7. alte detaillierte Historie

Web-OTA ist eine mögliche Zukunftsfunktion und erhält in Release 1 weder
Partitionen noch Speicherreserve.
