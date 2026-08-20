# Firmwareupdate, OTA und Rollback

## Status

Dieses Dokument beschreibt die in Phase 9B akzeptierten Update- und
Rollbackregeln.

Die zentrale Umfangsentscheidung lautet:

- **Release 1 benoetigt nur Update und Wiederherstellung ueber FT232RL/UART.**
- Aktuell werden keine Geraete ausgeliefert; Referenz ist das einzelne reale
  Owner-Geraet mit dauerhaftem physischem UART-Zugriff fuer Update und Recovery.
- Ein Webupload beziehungsweise OTA ist keine Implementierungspflicht fuer das
  erste Release und kann als spaetere Moeglichkeit vollstaendig entfallen.
- Eine spaetere OTA-Loesung wird bei konkretem Ownerbedarf in eigenem Scope,
  Plan und Espressif-first-Nachweis bewertet; Release 1 muss dafuer keine
  Kompatibilitaetsreserve vorhalten.
- Die nachfolgend beschriebenen OTA-, Signatur-, Rollback- und
  Migrationsanforderungen gelten ausschliesslich fuer eine spaetere optionale
  Erweiterung.

Diese Abgrenzung hat Vorrang vor Formulierungen, die Web-OTA als zwingenden
Bestandteil des ersten Releases darstellen.

## Begruendung fuer UART im ersten Release

Die Zielhardware besitzt 4 MB Flash ohne vorausgesetzte PSRAM. Gleichzeitig
werden Firmware, Weboberflaeche, drei Sprachen, Konfiguration, Laufpersistenz,
Fehlerjournal und Messhistorie benoetigt.

In der aktuellen Entwicklungs- und Einzelnutzerphase ist UART der praktischste
Updateweg:

- Der Entwickler benoetigt den FT232RL ohnehin fuer das erste Flashen.
- Auch ein spaeterer neuer Benutzer muss die Firmware mindestens einmal physisch
  aufspielen.
- UART benoetigt keine zweite Firmwarepartition.
- Es wird kein vorzeitiger Flashverbrauch fuer eine moeglicherweise nie benoetigte
  Komfortfunktion reserviert.
- Eine fehlerhafte Firmware kann unabhaengig von WLAN, Webserver und
  Dateisystem erneut aufgespielt werden.
- Die Kernfunktion des Fermentationsschranks kann zuerst stabilisiert und
  vermessen werden.

Das erste Release darf deshalb einen fuer die Anwendung geeigneten
Single-App-Partitionsplan verwenden. Es muss nicht vorsorglich zwei OTA-Slots
reservieren.

Eine spaetere Umstellung auf einen dualen OTA-Partitionsplan darf einen erneuten
physischen UART-Flash inklusive Partitionstabelle verlangen. Das ist akzeptiert.

## Verbindlicher Updateweg fuer Release 1

### FT232RL/UART

Release 1 unterstuetzt mindestens:

- initiales Flashen ueber den herausgefuehrten UART
- Firmwareupdate ueber PlatformIO beziehungsweise das dokumentierte
  Entwicklerwerkzeug
- Wiederherstellung nach unvollstaendigem oder fehlerhaftem Firmwarestand
- Flashen einer bekannten stabilen Firmwareversion
- bei Bedarf vollstaendiges Neuaufspielen der Partitionstabelle
- Zugriff auf den ROM-Bootloader ueber die bestaetigte IO0-/Reset-Prozedur

Die genaue Verdrahtung, Pegel und Bootloader-Prozedur werden erst nach
Hardwareverifikation verbindlich dokumentiert.

### Sicherheitsregeln beim UART-Update

Vor einem geplanten Update:

- aktiven Prozess sicher beenden
- Peltier und H-Bruecke sicher deaktivieren
- notwendige Luefternachlaeufe beenden
- Konfigurations- und Laufexport erstellen, sofern eine Migration oder ein
  vollstaendiger Flash-Loeschvorgang vorgesehen ist
- bestaetigen, dass die Binärdatei zum ESP32-32E mit 4 MB Flash und zur
  vorgesehenen Partitionstabelle passt

Nach dem Flashen gilt derselbe sichere Boot- und Validierungsablauf wie nach
jedem relevanten Neustart:

```text
Boot
  -> alle Peltier- und H-Brueckenausgaenge AUS
  -> Firmware- und Partitionsdaten pruefen
  -> Konfiguration und Laufdaten validieren
  -> Sensoren validieren
  -> verriegelte Fehler pruefen
  -> erst danach Standby oder erlaubten Wiederanlauf freigeben
```

Direkte GPIO-Zustaende werden nie aus dem Zustand vor dem Flashen
wiederhergestellt.

## Rollback im ersten Release

Release 1 besitzt keinen automatischen Firmware-Slot-Rollback, wenn nur eine
Anwendungspartition verwendet wird.

Der Rueckfall erfolgt physisch und bewusst:

1. vorherige bekannte stabile Firmware ueber UART aufspielen
2. kompatible Partitionstabelle verwenden
3. Konfigurationsschema und gespeicherte Revisionen validieren
4. falls erforderlich eine kompatible Sicherung oder alte Konfigurationsrevision
   wiederherstellen
5. vollstaendigen passiven Boot-Selbsttest durchfuehren

Ein Firmware-Downgrade darf keine neueren Konfigurations- oder Laufdaten still
mit einem inkompatiblen Schema verwenden. Nicht verstandene Daten werden nicht
teilweise geraten.

## Optionale spaetere OTA-Evaluierung

Release 1 benoetigt keine OTA-spezifischen Updatequellen, Manifest-, Rollback-,
Migrations- oder sonstigen Interfaces, Platzhalter oder Vorbereitungen. Das
aktuelle Single-App-/UART-Modell bleibt vollstaendig zulaessig und gewollt;
#90 reserviert keine OTA-Partitionen, Bibliotheken, Kompatibilitaetsreserven
oder sonstige Ressourcen.

Falls der Owner spaeter tatsaechlich OTA wuenscht, wird der Nutzen, das
Flashbudget, die Partitionierung, der ESP-IDF-Bootloader-/OTA-Mechanismus,
Rollback, Update-/Recoverystrategie und der genaue API-/Artefaktvertrag in
einem eigenen Scope und Plan Espressif-first neu bewertet. Diese Entscheidung
kann auch vollstaendig entfallen. Dieses Dokument erzeugt dafuer heute keine
Implementierungserlaubnis.

## Spaeterer Updateweg

Ein spaeterer OTA-Wunsch ist kein bestehendes Release- oder Roadmap-Ziel. Nur
nach eigenem Scope, Plan und Ownerentscheid darf ein spaeterer Updateweg
bewertet oder implementiert werden. Die Ausgestaltung folgt dann dem
Espressif-first-Nachweis; der physische UART-Weg bleibt der kontrollierbare
Recoveryweg. Die nachfolgenden OTA-Anforderungen sind deshalb ausschliesslich
optionale spaetere Bewertungsgrundlage und keine Release-1-Pflicht.

## Berechtigung fuer spaeteres Web-OTA

Ein Webupdate verlangt:

- gueltige normale Webanmeldung, sofern der Webpasswortschutz aktiviert ist
- erneute Eingabe der Service-PIN
- Anzeige einer Updatezusammenfassung
- ausdrueckliche Bestaetigung

Beispiel:

```text
Installierte Version:      1.0.2
Neue Version:              1.1.0
Hardwareziel:              ESP32-32E-4MB
Partitionslayout:          ota-v1
Konfigurationsschema:      4 -> 5
Signatur:                  gueltig
Rollbackfaehigkeit:        vorhanden

[Abbrechen] [Update installieren]
```

Eine zusaetzliche zwingende Touchbestaetigung ist nicht erforderlich. Dadurch
bleibt ein spaeteres Update ueber das bestehende WireGuard-VPN moeglich. Die
Service-PIN und die Zusammenfassung schuetzen die kritische Aktion.

## Zulaessige Systemzustaende fuer spaeteres Web-OTA

Ein Update ist nur erlaubt in:

- `STANDBY`
- `COMPLETED`, nachdem Laufabschluss und Persistenz vollstaendig gesichert sind
- `SAFE_BOOT`, soweit das System fuer Update und Schreibvorgaenge stabil genug ist

Gesperrt ist ein Update mindestens waehrend:

- Vorheizen
- Warten auf Produkt
- Zielerreichung
- Zielqualifikation
- Fermentation
- Kuehlen
- Kuehlhalten
- manuellem Temperaturhalten
- laufender Servicepruefung
- aktivem Sicherheitsrueckfuehrungsversuch

Ein Update bricht keinen laufenden Prozess automatisch ab. Der Benutzer muss den
Lauf zuerst bewusst sicher beenden.

## 4-MB-Flash und OTA-Partitionsentscheidung

Web-OTA wird nur freigegeben, wenn ein realer Releasebuild nachweist, dass ein
sicheres duales Layout in 4 MB passt.

Das Budget muss mindestens enthalten:

- Bootloader und Partitionstabelle
- zwei ausreichend grosse Firmware-Slots
- OTA-Auswahldaten
- persistente Konfiguration mit Rueckfallrevision
- kritische Laufpersistenz
- Fehler- und Resetjournal
- notwendige Webressourcen
- Mindesthistorie oder klar dokumentierte Reduktion
- ausreichenden freien Sicherheitsabstand

Entscheidungsregel:

```text
Dual-Slot-Layout passt mit nachgewiesenem Sicherheitsabstand
  -> Web-OTA darf implementiert und getestet werden

Dual-Slot-Layout passt nicht sicher
  -> Web-OTA bleibt deaktiviert
  -> Update weiterhin ueber UART
```

Nicht erlaubt ist:

- direktes riskantes Ueberschreiben der einzigen laufenden Firmwarepartition
- Verkleinern kritischer Lauf- oder Fehlerpersistenz unter die Sicherheitsanforderung
- Aktivieren von OTA allein aufgrund theoretischer Partitionsgroessen ohne realen
  Releasebuild und Ressourcentest

Ob lokale Langzeithistorie fuer ein spaeteres OTA-Layout reduziert werden darf,
wird erst anhand des tatsaechlichen Nutzens und Speicherbudgets entschieden.
Kritische Laufwiederherstellung hat Vorrang vor komfortabler Historie.

## Spaeteres signiertes Firmwarepaket

Ein Webupdate verwendet ein Paket mit mindestens:

- Firmwaredatei
- Firmwareversion
- Buildkennung
- Hardwareziel
- erwarteter Flashgroesse
- erforderlichem Partitionslayout
- minimal und maximal unterstuetztem Konfigurationsschema
- Paket- und Firmwaregroesse
- kryptografischem Hash
- digitaler Signatur
- optionalen Migrationskennungen und Releasehinweisen

Der ESP32 speichert nur den oeffentlichen Pruefschluessel. Der private
Signierschluessel bleibt ausserhalb des Geraets auf dem Entwicklungs- oder
Buildsystem und wird nie in Repository, Firmware, Diagnoseexport oder
Updatepaket abgelegt.

Vor dem Schreiben werden mindestens geprueft:

1. Paketformat und Manifestversion
2. Dateilaenge
3. Hardwareziel und Flashgroesse
4. Partitionskompatibilitaet
5. Schemavertraeglichkeit
6. kryptografischer Hash
7. digitale Signatur
8. verfuegbarer Speicher und zulaessiger Systemzustand

Ein SHA-256-Hash allein erkennt Beschaedigung, beweist aber ohne vertrauenswuerdig
verteilten Sollwert nicht die Autorisierung. Deshalb ist fuer spaeteres Web-OTA
die digitale Signatur vorgesehen.

Direktes UART-Flashing durch den Entwickler benoetigt keine im ESP32 erzwungene
Paketsignatur. Dort liegt die Vertrauenskette beim kontrollierten Quellcode,
Buildprozess und physischen Zugriff.

## Erststart einer neuen OTA-Firmware

Eine neue OTA-Firmware startet im Zustand:

```text
PENDING_VALIDATION
```

Vor dauerhafter Bestaetigung muss sie passiv mindestens pruefen:

1. Boot ohne erneuten Watchdog oder Brownout
2. Firmware- und Partitionsintegritaet
3. Zugriff auf OTA-Status und Fehlerjournal
4. Konfigurations- und Laufdatenschema
5. feste Sensorbusse und Sensoridentitaeten
6. sichere Ausgangszustaende
7. ausreichende Ressourcen
8. minimale stabile Laufzeit ohne kritischen Systemfehler

Die Validierung startet keine Peltier-, Luefter- oder Summerpruefung.

Bei Erfolg:

```text
PENDING_VALIDATION -> VALID
```

Bei fehlgeschlagener Validierung oder wiederholtem abnormalem Neustart:

```text
PENDING_VALIDATION -> Rollback auf letzten gueltigen Slot
```

Ist auch der Rueckfallslot nicht startfaehig, bleibt UART der physische
Wiederherstellungsweg.

## Konfigurationsmigration

Migrationen verwenden getrennte Revisionen:

```text
alte Firmware
  -> alte gueltige Konfigurationsrevision bleibt unveraendert
  -> neue Firmware erzeugt eine separate migrierte Revision
  -> neue Revision vollstaendig validieren
  -> neue Firmware und Systemzustand validieren
  -> erst danach neue Revision aktivieren
```

Verbindliche Regeln:

- Die letzte mit der alten Firmware kompatible Revision wird nicht ueberschrieben.
- Migrationen sind versioniert und wiederholbar beziehungsweise eindeutig als
  bereits ausgefuehrt erkennbar.
- Ein teilweise fehlgeschlagener Migrationsversuch wird nicht aktiviert.
- Ein Firmware-Rollback darf die alte kompatible Revision weiterverwenden.
- Neuere Daten werden nicht still durch eine aeltere Firmware veraendert.
- Bei inkompatiblen Laufdaten wird kein alter Lauf geraten oder automatisch
  aktiviert.

## Automatisches Rollback und manuelles Downgrade

### Automatisches Rollback

Ein automatisches Rollback auf den letzten gueltigen Firmware-Slot ist bei
fehlgeschlagener `PENDING_VALIDATION` immer erlaubt. Es benoetigt keine normale
Benutzerbestaetigung, wird aber protokolliert und sichtbar gemeldet.

### Manuelles Downgrade

Ein bewusstes Downgrade auf eine aeltere Firmware ist spaeter nur erlaubt mit:

- Service-PIN
- deutlicher Warnung
- bestaetigtem Hardwareziel
- bestaetigter Partitionskompatibilitaet
- bestaetigter Konfigurations- und Datenschemakompatibilitaet
- Auswahl einer geeigneten alten Revision oder Sicherung, falls notwendig
- ausdruecklicher Bestaetigung

Ein Downgrade wird abgelehnt, wenn es die einzigen vorhandenen neueren Daten
still beschaedigen, teilweise interpretieren oder unkontrolliert ueberschreiben
wuerde.

## Updatejournal

Release 1 protokolliert mindestens physisch erkennbare relevante Firmware- und
Resetinformationen, soweit verfuegbar:

- Firmwareversion
- Buildkennung
- Partitionstabellenkennung
- Resetursache nach dem Flashen
- Schemamigration oder Rueckfall
- fehlgeschlagene Bootvalidierung

Ein spaeteres Web-OTA-Journal enthaelt zusaetzlich:

- Updatequelle
- Paketkennung und Zielversion
- Signaturpruefergebnis
- Start, Fortschritt und Ende des Schreibvorgangs
- Zielslot
- `PENDING_VALIDATION`-Ergebnis
- Rollbackgrund
- Bedienquelle
- Konfigurationsmigrationsrevision

Geheimnisse, Passwoerter, PINs, Tokens und private Schluessel werden nicht
protokolliert.

## Akzeptierte Entscheidungen aus Phase 9B

### Release 1

- [x] FT232RL/UART ist der ausreichende und verbindliche Update- und
      Wiederherstellungsweg
- [x] Web-OTA ist keine Implementierungspflicht des ersten Releases
- [x] Release 1 darf einen Single-App-Partitionsplan ohne reservierte OTA-Slots
      verwenden
- [x] spaetere Umstellung auf OTA darf erneutes UART-Flashing der Partitionstabelle
      verlangen
- [x] manuelles Firmware-Rollback erfolgt ueber UART und eine bekannte stabile
      Version

### Spaetere optionale Web-OTA-Erweiterung

- [x] lokaler Webupload plus UART-Wiederherstellung; kein automatischer
      GitHub-Download
- [x] Webanmeldung, erneute Service-PIN und ausdrueckliche Zusammenfassung
- [x] Update nur in Standby, gesichertem Completed oder geeignetem SAFE_BOOT
- [x] duale Firmware-Slots nur nach realem 4-MB-Budgetnachweis
- [x] kein riskantes Single-Slot-Webupdate
- [x] signiertes Paket mit Manifest, Hash, Hardwareziel und Schemakompatibilitaet
- [x] neue Firmware startet als `PENDING_VALIDATION`
- [x] passiver Selbsttest vor dauerhafter Bestaetigung
- [x] automatisches Rollback bei fehlgeschlagener Validierung
- [x] Migration erzeugt eine neue Revision und erhaelt die alte kompatible Revision
- [x] automatisches Rollback immer erlaubt
- [x] manuelles Downgrade nur mit Service-PIN, Warnung und nachgewiesener
      Schemakompatibilitaet

## Noch offen fuer Phase 9C und Implementierungsplanung

- konkreter Release-1-Partitionsplan fuer 4 MB
- maximal zulaessige Firmwaregroesse im Single-App-Layout
- reservierter Platz fuer Konfiguration, Laufpersistenz, Journal und Historie
- dokumentierte PlatformIO- und FT232RL-Flashprozedur
- Sicherungs- und Wiederherstellungsablauf vor einem vollstaendigen Flash-Loeschen
- optionale spaetere OTA-Bewertung von Partitionierung und Sicherheitsabstand, nur bei eigenem Scope, Plan und Ownerentscheid
- konkretes Signaturformat und Schluesselverwaltungsverfahren
- genaue `PENDING_VALIDATION`-Dauer und Erfolgskriterien
- spaetere Migrations- und Rollbacktests
