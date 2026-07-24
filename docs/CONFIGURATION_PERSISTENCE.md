# Technischer Vertrag fuer Konfigurationspersistenz

## Status und Geltungsbereich

Dieses Dokument ist die technische Spezifikation fuer Issue #16. Es konkretisiert
[`SETTINGS_AND_STORAGE.md`](SETTINGS_AND_STORAGE.md) und
[`BACKUP_SECURITY_RETENTION.md`](BACKUP_SECURITY_RETENTION.md), ohne
Laufpersistenz aus Issue #17, das portable Backupformat aus Issue #19 oder die
konkrete WLAN- und Authentifizierungssemantik aus Issue #27 vorwegzunehmen.

Issue #16 implementiert die hardwareunabhaengige, nativ testbare
Konfigurationspersistenz. Reale Flash-Atomizitaet, reale Heapreserve und
Belastungsmessungen bleiben spaetere Hardware- beziehungsweise Release-Gates.

## Architektur und Konfigurationsgeneration

`FactoryConfiguration` ist unveraenderlicher Bestandteil der Firmware.

Benutzer-, Service- und Programmkonfiguration werden als getrennte,
vollstaendig typisierte und jeweils schema-versionierte Dokumente gespeichert:

- `UserConfiguration`
- `ServiceConfiguration`
- `ProgramCatalog`

Die Dokumente besitzen eigene Inhaltsrevisionen, bilden aber keine unabhaengig
aktivierten Konfigurationsstroeme. Ein `ActiveConfigurationManifest` verweist
auf genau eine validierte Kombination konkreter Dokumentrevisionen und auf eine
konkrete Connectivity-Secret-Generation. Es bildet die gemeinsame
Konfigurationsgeneration.

Eine Aenderung wird in folgender Reihenfolge ausgefuehrt:

1. nur tatsaechlich geaenderte Dokumente als neue Revision schreiben
2. zusammen mit den unveraenderten referenzierten Dokumenten einen vollstaendigen
   Kandidaten bilden
3. den gesamten Kandidaten technisch und fachlich validieren
4. ein Manifest mit den exakten Dokumentrevisionen schreiben
5. Integritaet und Referenzen pruefen
6. das Manifest atomar aktivieren
7. die vorherige aktive Manifestgeneration als Rueckfall behalten

Unveraenderte Dokumentrevisionen duerfen von mehreren Manifesten gemeinsam
referenziert werden. Inhaltsgleichheit wird nicht allein anhand eines CRC
angenommen.

Die `FactoryConfiguration` selbst und firmwarefeste Grenzen werden weder als
gespeichertes Dokument kopiert noch ueberschreibbar gemacht. Davon zu
unterscheiden sind die ausdruecklich spezifizierten initialen Benutzerwerte und
die gespeicherten Arbeitskopien der vier Factory-Programme. Der aktive Lauf
bleibt vollstaendig ausserhalb der Geraetekonfiguration und verwendet seinen
unveraenderlichen Laufschnappschuss.

## Dokumente der ersten Schemageneration

### UserConfiguration Schema 1

Schema 1 enthaelt ausschliesslich:

- Sprache des lokalen Touchdisplays
- kanonische IANA-Zeitzone
- sichtbarer Geraetename

#### Displaysprach-ID

Die `DisplayLanguageId`:

- umfasst 2 bis 16 ASCII-Bytes
- erlaubt `a-z`, `0-9` und `-`
- beginnt und endet alphanumerisch
- enthaelt keine aufeinanderfolgenden Bindestriche
- wird nicht normalisiert
- wird exakt gegen den versionierten Firmware-Uebersetzungskatalog geprueft

Release 1 unterstuetzt mindestens `de`, `es` und `en`; Factory-Standard ist
`de`. Die IDs sind interne Firmwarekatalog-IDs und kein vollstaendiges
BCP-47-Datenmodell. Die Websprache bleibt browser- beziehungsweise
sitzungslokal. Neue Katalogeintraege erfordern keine Schemaaenderung.

#### Gespeicherte Zeitzone

Gespeichert wird ausschliesslich ein kanonischer IANA-Bezeichner. Factory-
Standard ist `Europe/Zurich`.

Die gespeicherte ID:

- umfasst 1 bis 64 ASCII-Bytes
- besteht aus durch `/` getrennten, nicht leeren Komponenten
- erlaubt innerhalb einer Komponente `A-Z`, `a-z`, `0-9`, `.`, `_`, `-` und `+`
- erlaubt weder `.` noch `..` als vollstaendige Komponente
- beginnt und endet nicht mit `/`
- enthaelt kein `//`
- ist nicht `Etc/Unknown`
- wird nicht normalisiert

Die strukturelle Pruefung allein macht die ID nicht gueltig. Erforderlich sind
zusaetzlich die exakte Uebereinstimmung im versionierten Firmware-
Zeitzonenkatalog und eine erfolgreiche Vorbereitung durch
`ITimeZoneResolver`. Der Katalog garantiert mindestens `Europe/Zurich` und ist
unabhaengig vom Konfigurationsschema versioniert. Seine endgueltige Release-1-
Groesse richtet sich nach dem gemessenen Flashbudget.

Nicht gespeichert werden lokalisierte Namen, feste UTC-Offsets, aktueller
Sommerzeitstatus, freie POSIX-TZ-Strings oder kopierte Zeitzonenregeln. Aliase
werden nur bei ausdruecklicher deterministischer Normalisierung akzeptiert;
persistiert wird immer der kanonische Katalogeintrag. Die normale UI verwendet
eine Katalogauswahl und keine freie Texteingabe.

Eine Zeitzonenaenderung ist nicht neustartpflichtig. Sie wirkt nur auf lokale
Darstellung und zukuenftige Formatierung. UTC-Werte, Generationen, monotone
Zeiten, Laufwerte, Laufrevisionen und Laufschnappschuss bleiben unveraendert.
Freie lokale Uhrzeiten nach UTC umzurechnen, insbesondere an DST-Uebergaengen,
ist nicht Scope von Issue #16.

#### Sichtbarer Geraetename

Factory-Standard ist `Fermentationsschrank`. Der Name:

- ist gueltiges UTF-8
- enthaelt 1 bis 48 Unicode-Skalarwerte
- belegt hoechstens 96 UTF-8-Bytes
- enthaelt mindestens einen Nicht-Leerraumwert
- besitzt keinen fuehrenden oder abschliessenden Unicode-Leerraum
- enthaelt keine NUL-, C0-, C1-, Zeilen- oder Absatztrennzeichen

Der Wert wird weder gekuerzt noch getrimmt oder automatisch Unicode-
normalisiert. Er wirkt sofort fuer fachliche Anzeigen und
Systeminformationen. Hostname, mDNS-Name, WLAN-SSID und andere
Netzwerkableitungen folgen mit Issue #27.

Schema 1 enthaelt noch keine Displayhelligkeit, Abdunkelzeit, Lautstaerke,
Tonparameter, Netzwerkparameter, Aufbewahrungsgrenzen oder Hardware-, Sensor-,
Regel- und Sicherheitswerte.

### ServiceConfiguration Schema 1

Schema 1 ist ein gueltiges, typisiertes Dokument ohne fachliche Parameterfelder
und mit exakt 0 Payloadbytes. Jedes gueltige Konfigurationsmanifest referenziert
eine konkrete ServiceConfiguration-Revision. Es gibt weder ein paralleles
`ServiceConfiguration fehlt`-Modell noch Reservefelder oder freie
Schluessel/Wert-Eintraege.

### ProgramCatalog Schema 1

Der Katalog enthaelt:

- genau vier gespeicherte, bearbeitbare Arbeitskopien der Firmware-
  Standardprogramme
- hoechstens zwoelf Benutzerprogramme
- bestehende `ProgramDocument`s mit ihrem bestehenden Programmschema
- stabile, katalogweit eindeutige Programm-IDs
- die gespeicherte Anzeigereihenfolge
- bestehende Factory-, Ruecksetz-, Installations- und Aktivierungsmerkmale

Die unveraenderlichen Factory-Vorlagen verbleiben ausschliesslich in der
Firmware. Ersteinrichtung und Werksreset erzeugen daraus Arbeitskopien. Ein
Firmwareupdate ueberschreibt vorhandene Arbeitskopien nicht. Ein bewusstes
Zuruecksetzen eines Standardprogramms ersetzt nur dessen Arbeitskopie durch die
aktuelle, vollstaendig validierte Firmwarevorlage.

Die reservierten Factory-IDs muessen genau einmal vorhanden sein. Factory-
Arbeitskopien stehen zuerst und in der Reihenfolge des Firmwarekatalogs. Sie
werden nicht physisch entfernt; eine Deaktivierung verwendet gegebenenfalls
das bestehende Merkmal `installed`. Benutzerprogramme folgen in gespeicherter
Reihenfolge. Die Position ist die Anzeigereihenfolge; es gibt kein zusaetzliches
Sortierfeld.

Programme mit `TBD_COMMISSIONING`-Werten duerfen als strukturell gueltige
Katalogvorlage gespeichert werden. Vor jedem Start ist weiterhin die
vollstaendige `Runnable`-Validierung erforderlich. Der Katalog enthaelt keinen
aktiven Lauf, Laufschnappschuss oder Laufrevisionen.

#### Programm-ID

Eine Programm-ID:

- umfasst 1 bis 48 ASCII-Bytes
- erlaubt nur `a-z`, `0-9` und `-`
- beginnt und endet alphanumerisch
- enthaelt keine aufeinanderfolgenden Bindestriche
- ist im gesamten Katalog eindeutig
- verwendet keine reservierte Factory-ID fuer ein Benutzerprogramm
- ist nach Erstellung unveraenderlich

Es gibt keine stille Normalisierung. Eine UI darf vor dem Speichern einen
deterministischen gueltigen Vorschlag erzeugen.

#### Sichtbarer Programmname und Notizen

Der sichtbare Programmname folgt denselben Unicode-, Leerraum- und
Steuerzeichengrenzen wie der Geraetename: 1 bis 48 Unicode-Skalarwerte und
hoechstens 96 UTF-8-Bytes.

Notizen:

- sind gueltiges UTF-8 und duerfen leer sein
- enthalten hoechstens 512 Unicode-Skalarwerte
- belegen hoechstens 1.024 UTF-8-Bytes
- enthalten weder NUL noch C0- oder C1-Steuerzeichen, ausser `LF` (`U+000A`)
- enthalten kein `CR`, `U+2028` oder `U+2029`

CRLF oder CR duerfen vor Vorschau und Bestaetigung deterministisch auf LF
normalisiert werden. Danach erfolgt keine weitere stille Veraenderung.

#### Katalog- und Payloadgrenzen

- Factory-Programme: genau 4
- Benutzerprogramme: hoechstens 12
- Gesamtzahl: hoechstens 16
- Payload ohne Envelope: hoechstens 32.768 Bytes

Programmanzahl, Feldgrenzen und Payloadgroesse werden unabhaengig geprueft. Bei
Ueberschreitung wird die gesamte Aktion mit einem stabilen Kapazitaetsfehler
abgelehnt; Programme werden weder geloescht noch abgeschnitten. Katalogweite
Grenzen stehen in `program_catalog_limits.hpp`; einzelne ProgramDocument-
Grenzen bleiben im Programmmodellbereich zentralisiert.

## Schema, Revision und Migration

Folgende fachliche Schemas entwickeln sich unabhaengig:

- `UserConfigurationSchema`
- `ServiceConfigurationSchema`
- `ProgramCatalogSchema`
- das bestehende `ProgramDocumentSchema`
- `ConfigurationManifestSchema`
- `ConnectivityManifestSchema`
- `AuthenticationManifestSchema`

Es wird keine zweite Programmschema-Definition eingefuehrt.

- Schema-Version beschreibt die fachliche Datenstruktur.
- Dokumentrevision identifiziert einen konkreten Dokumentinhalt.
- Manifestgeneration identifiziert eine validierte Kombination.
- Envelope-Version beschreibt ausschliesslich den generischen Speicherrahmen.

Revisionen und Generationen verwenden `uint64_t`, beginnen bei 1, reservieren 0
und laufen niemals still ueber. Eine persistente monotone `MutationSequence` je
`StorageEpoch` wird vor den eigentlichen Schreibvorgaengen dauerhaft ueber
redundante `StorageMutationSequenceRecord`s reserviert. Reservierte Werte werden
nicht wiederverwendet; Luecken sind zulaessig. Unterschiedliche starke Typen
bleiben auch bei gleichen numerischen Werten getrennt. Unveraenderte Dokumente
behalten ihre Revision. `CredentialEpoch` bleibt eine eigene fachliche,
vorwaertsgerichtete Epoche und wird nicht automatisch mit der globalen
MutationSequence gleichgesetzt.

Migrationen sind ausschliesslich Copy-Migrationen:

1. alten Envelope und vollstaendige Revision lesen
2. Magic, Laenge und Integritaet pruefen
3. altes Schema mit seinem Decoder dekodieren
4. jeden Migrationsschritt einzeln auf einer Kopie ausfuehren
5. aktuelles fachliches Modell erzeugen
6. Dokument und Gesamtkonfiguration vollstaendig validieren
7. nur erforderliche neue Dokumentrevisionen schreiben
8. neues Manifest schreiben und pruefen
9. atomar aktivieren oder weiterhin als Pending referenzieren
10. vorherige Manifestgeneration bis zum nachweislichen Erfolg behalten

Es gibt keine In-place-Migration und keine scheinbar gueltigen Defaults fuer
noch ungeklaerte Sicherheits-, Sensor- oder Prozesswerte. Jede Firmware kennt
je Dokumenttyp das aktuelle und das aelteste unterstuetzte Schema. Unbekannte
neuere Versionen werden abgelehnt. Active und Pending werden getrennt migriert;
eine Pending-Migration aktiviert den Kandidaten nicht.

Fuer erstmals eingefuehrte Schema-1-Dokumente wird keine Schema-0-Migration
erfunden. Issue #16 testet aktuelle Schemas, die Ablehnung neuerer Schemas, den
generischen Copy-Ablauf mit Testschemas und reale bestehende ProgramDocument-
Migrationen im ProgramCatalog.

## Kanonisches Wireformat

Die Payload ist dokument- und schemaabhaengig, deterministisch und unabhaengig
von C++-Layout, Padding, ABI und nativer Enumdarstellung. Sie verwendet:

- Big Endian fuer alle mehrbyteigen Werte
- explizite Integerbreiten und Zweierkomplement fuer signierte Integer
- stabile, explizite Wire-IDs fuer Enums und Statuswerte
- `0x00`/`0x01` als einzige gueltige Boolwerte
- `0x00`/`0x01` als einzige gueltige Optionaltags
- feste Feldreihenfolge
- validiertes, begrenztes UTF-8
- begrenzte Listen und Payloads

Unbekannte fachliche Enumwerte werden abgelehnt, sofern ihre Spezifikation
nicht ausdruecklich Vorwaertskompatibilitaet vorsieht. Unbekannte
ChangeOrigin- und ChangeOperation-IDs bilden diese ausdrueckliche Ausnahme.

### Gleitkommawerte

Gleitkommawerte werden als IEEE-754 binary64, Big Endian gespeichert. Folgende
Voraussetzungen werden zur Compilezeit geprueft:

- `sizeof(double) == 8`
- `std::numeric_limits<double>::is_iec559`
- `radix == 2`
- `digits == 53`
- `max_exponent == 1024`

NaN und positive oder negative Unendlichkeit sind beim Kodieren und Dekodieren
ungueltig. Der Encoder normalisiert `-0.0` zu `+0.0`; eine eingelesene negative
Null wird als nicht kanonisch abgelehnt. Nach dem Dekodieren folgt immer die
vollstaendige fachliche Validierung.

### Envelope-Version 1

Die kanonische Bytefolge lautet:

1. Magic `DPRF`: `44 50 52 46`
2. Envelope-Version: `uint16`, Big Endian
3. Record-Type-ID: `uint16`, Big Endian
4. fachliche Schema-Version: `uint32`, Big Endian
5. StorageEpoch: `uint64`, Big Endian
6. VersionValue: `uint64`, Big Endian
7. Payloadlaenge: `uint32`, Big Endian
8. ChangeOrigin-Wire-ID: `uint16`, Big Endian
9. ChangeOperation-Wire-ID: `uint16`, Big Endian
10. UTC-Optionaltag: ein Byte
11. bei `0x01`: UTC-Unix-Sekunden als `int64`, Big Endian
12. CRC-32/ISO-HDLC: `uint32`, Big Endian
13. Payload: exakt die angegebene Byteanzahl

`VersionValue` wird je Record-Type als Revision, Generation oder RecordSequence
interpretiert; die Produktionscode-Typen bleiben getrennt.

Ungueltig sind Envelope-Version, Record-Type-ID, Schema-Version, StorageEpoch
oder VersionValue 0 sowie UTC-Tags ausser `0x00` und `0x01`. Unbekannte
Envelope-Versionen lehnt `device_platform` ab. Unbekannte Anwendungs-Record-
Types lehnt `fermentation_app` vor der Payloaddekodierung ab. Unbekannte
ChangeOrigin- und ChangeOperation-IDs werden als rohe IDs erhalten.

Der Envelope besitzt keine Padding-, Reserve-, Flags- oder ABI-abhaengigen
Bytes. Seine Groesse einschliesslich CRC betraegt 41 Bytes ohne und 49 Bytes mit
UTC. Bei maximal 32.768 Payloadbytes ist ein Konfigurationsrecord hoechstens
32.817 Bytes gross.

### CRC-32/ISO-HDLC

Es gilt ausschliesslich:

- Polynom `0x04C11DB7`
- reflektiertes Polynom `0xEDB88320`
- Initialwert `0xFFFFFFFF`
- Eingabe und Ausgabe reflektiert
- finales XOR `0xFFFFFFFF`
- Pruefwert fuer ASCII `123456789`: `0xCBF43926`
- katalogisierte Residue `0xDEBB20E3`

Das CRC-Feld steht unmittelbar vor der Payload. Berechnet wird ueber alle
Headerbytes vor dem CRC und anschliessend die vollstaendige Payload. Das CRC-
Feld selbst ist ausgeschlossen. Der numerische CRC wird Big Endian gespeichert
und durch Neuberechnung verglichen. Die katalogisierte Residue ist keine
Pruefregel fuer den vollstaendigen Big-Endian-Datensatz. Eine Verfahrensaenderung
erfordert eine neue Envelope-Version. CRC erkennt zufaellige Beschaedigung und
ist weder Manipulationsschutz noch Authentifizierung oder Verschluesselung.

### Laengen- und Allokationsschutz

Payloadgrenzen ohne Envelope:

- globales Konfigurationsmaximum: 32.768 Bytes
- ProgramCatalog Schema 1: hoechstens 32.768 Bytes
- UserConfiguration Schema 1: hoechstens 256 Bytes
- ServiceConfiguration Schema 1: exakt 0 Bytes

Jeder feste Record oder Manifesttyp besitzt je Schema und Variante eine exakt
definierte kanonische Payloadlaenge. Fehlende und zusaetzliche Bytes werden
abgelehnt. Vor jeder fachlichen Dekodierung werden vorhandene Bytes, Record-
Type, dokumenttypspezifisches Maximum, globales Maximum, ueberlaufsichere
Gesamtgroesse und behauptete Laenge geprueft. Der Speicheradapter setzt ein
caller- oder schluesselspezifisches maximales Leselimit durch.

Technische Envelope- und Wiregroessen liegen in `device_platform`; das globale
Konfigurationsbudget und die Dokumentgrenzen in `fermentation_app`.

## Revisionsplaetze und Referenzschutz

### Konfigurationsdokumente

Jeder Dokumenttyp besitzt genau vier physische Slots:

- `UserConfiguration`: 4
- `ServiceConfiguration`: 4
- `ProgramCatalog`: 4

Sie decken hoechstens Active, Fallback, Pending und Staging ab. Eine Referenz
enthaelt Dokumenttyp, Slot-ID, `uint64_t`-Revision, Schema-Version, erwartete
Payloadlaenge und erwarteten CRC. Alle Werte muessen mit dem gelesenen Envelope
uebereinstimmen.

Nur ein Slot, der von keinem vollstaendig gueltigen Root-Teilgraphen geschuetzt
ist, darf ueberschrieben werden. Jeder von einem technisch gueltigen Root
referenzierte Active-, Fallback- oder Pending-Zweig wird getrennt bewertet und
schuetzt seine Slots erst nach erfolgreicher Manifest-, Referenz-, Envelope-,
CRC- und Gesamtvalidierung. Ein beschaedigter Active-Zweig entwertet einen
vollstaendig gueltigen Fallback-Zweig desselben RootRecords nicht. Technisch
beschaedigte oder unvollstaendige Roots schuetzen keine Slots. Zusaetzliche
Schutzwurzeln sind eine passende Aktivierungsabsicht mit gueltigem Pending-Graph
und die aktuell ausgefuehrte serialisierte Schreiboperation.

Bereinigung verwendet Referenzanalyse statt Referenzzaehlern. Nicht
referenzierte Slots sind logisch wiederverwendbar und muessen nicht vorher
physisch geloescht werden. Teilweise oder vollstaendig geschriebene, aber
unreferenzierte Revisionen bleiben wirkungslos. Fehlt ein sicherer Slot, wird
mit `NoUnreferencedSlotAvailable` und betroffenem Dokumenttyp abgelehnt.

### Active und Fallback

Es existieren:

- 3 `ConfigurationManifest`-Slots
- 2 CRC-geschuetzte `ConfigurationRootRecord`-Slots

Ein Root referenziert gleichzeitig Active und Fallback. Eine neue aktive
Manifestgeneration wird im freien dritten Manifestplatz geschrieben und
vollstaendig validiert. Erst danach wird der inaktive Root mit der neuen
Active-/Fallback-Zuordnung geschrieben.

Beim Boot werden technisch gueltige Roots absteigend nach Sequenz untersucht.
Je Root wird zuerst sein Active-Zweig und bei dessen Ungueltigkeit der
zugehoerige Fallback-Zweig vollstaendig validiert. Der erste vollstaendig
nutzbare Zweig ist massgebend; ein verwendeter Fallback wird stabil
diagnostiziert. Ein lediglich vorhandenes Manifest wird niemals wegen seiner
hohen Generation aktiviert.

### Pending

Es existieren:

- 2 `PendingConfigurationManifest`-Slots
- 2 CRC-geschuetzte `PendingRootRecord`-Slots

Das bisherige Pending bleibt gueltig, bis neues Manifest und neuer Root
vollstaendig geschrieben und geprueft wurden.

### Aktivierungsabsicht

Die Aktivierungsabsicht verwendet zwei CRC-geschuetzte Revisionsplaetze und ist
mindestens gebunden an:

- erwartete aktive Manifestgeneration
- exakte Pending-Manifestgeneration
- Integritaetskennung des Pending-Manifests
- monotone Intent-Sequenz
- Intent-Status

Waehrend eine gueltige Aktivierungsabsicht besteht, duerfen Pending und seine
Dokumentrevisionen nicht ersetzt werden.

## Neustartpflichtige Konfiguration

Neben Active existiert hoechstens ein vollstaendig validiertes
`PendingConfigurationManifest`. Neustartpflichtige Aenderungen werden
gespeichert, dadurch aber nicht aktiv. Pending bleibt sichtbar und kann bewusst
ersetzt oder verworfen werden.

Enthaelt eine Bearbeitung sofort und erst nach Neustart wirksame Aenderungen,
wird der gesamte Kandidat Pending; ein Speichervorgang wird nicht aufgeteilt.
Sobald Pending existiert, bauen alle weiteren dauerhaft gespeicherten
Aenderungen darauf auf. Es entsteht kein paralleler neuer Active-Zweig. Die
aktive Konfiguration bleibt bis Anwenden oder Verwerfen vollstaendig
unveraendert. Reversible UI-Vorschauen bleiben fluechtig.

Aktivierung erfordert `Anwenden und neu starten` aus einem sicheren Zustand ohne
aktiven oder wiederherzustellenden Lauf. Die atomare Absicht bindet exakte
Pending-Generation und -Integritaet sowie die erwartete Active-Generation.
Danach werden neuer Lauf, weitere Konfigurationsaenderung und Pending-Ersetzung
gesperrt und der kontrollierte Neustart unmittelbar ausgefuehrt.

Beim Boot wird Pending nur aktiviert, wenn Absicht, Pending und alle Dokumente
gueltig und passend sind, Active weiterhin der Erwartung entspricht, kein
aktiver oder wiederherzustellender Lauf existiert und keine Verriegelung oder
Integritaetsstoerung entgegensteht. Ein Stromausfall oder unerwarteter Neustart
ohne passende Absicht aktiviert Pending niemals. Lauf-Recovery verwendet die
vorher aktive Konfigurationsgeneration und den persistierten Laufschnappschuss.

Bei Erfolg wird der bisherige Active zum Fallback, Pending zum Active, die
Absicht abgeschlossen und Pending entfernt. Ist das Pending-Ziel beim Boot
bereits Active, erfolgt keine zweite Aktivierung, sondern nur der idempotente
Abschluss. Bei Fehler bleibt der alte Active wirksam, es gibt keine
Teilaktivierung, ein stabiler Diagnosegrund wird gespeichert und die Absicht
wird nicht unbegrenzt bei jedem Boot erneut ausgefuehrt.

Welche Felder einen Neustart erfordern, bestimmt ausschliesslich das typisierte
Modell. Display, Web und Import koennen dies nicht frei setzen.

## Preview und Konflikterkennung

### Zentrales Preview

Ein `ConfigurationPreview` wird ausschliesslich durch den fachlichen
Konfigurationsdienst erzeugt. Es enthaelt:

- einen unveraenderlichen, vollstaendig typisierten Kandidaten
- alle Basisgenerationen
- berechnete Aktivierungswirkung
- typisierte Aenderungszusammenfassung
- Owner-Kontext
- monotone Erstellungs- und Ablaufzeit
- eindeutigen PreviewHandle
- opaken Bestaetigungs-Token

Oberflaechen erhalten nur ihre redigierte Darstellung, Handle und Token. Beim
Commit uebermitteln sie keinen Kandidaten erneut. Der Dienst verwendet nur den
zentral gespeicherten unveraenderlichen Kandidaten.

Der PreviewHandle enthaelt mindestens eine bootlokale RuntimeInstanceId, eine
monotone PreviewSequence und eine PreviewNonce. RuntimeInstanceId, Nonce und
Token umfassen je 16 zufaellige Bytes aus `ISecureRandomSource`. Sie werden
nicht allein aus Zeit, MAC, Kennung oder Zaehlern abgeleitet und nie geloggt.
Der Token wird konstantzeitlich verglichen. Ohne sichere Zufallswerte wird kein
bestaetigbares Preview veroeffentlicht.

Die PreviewSequence beginnt je Runtime bei 1. Bei `UINT64_MAX` entstehen keine
weiteren Previews. Neustart erzeugt eine neue RuntimeInstanceId und invalidiert
alle alten Previews. Handle ist keine Autorisierung; Commit-Kontext und Owner
muessen uebereinstimmen.

Die Bestaetigung bindet mindestens Handle, Token, Owner, StorageEpoch, Active-
Generation, erwartete Pending-Generation oder Nichtvorhandensein, vollstaendige
Aktivierungswirkung und typisierte Zusammenfassung. Lokalisierte Zeichenketten
sind kein Persistenzvertrag.

Ein interner Kandidatenfingerprint darf ergaenzend verwendet werden, ersetzt
aber weder Handle-, Token- und Ownerpruefung noch unveraenderlichen Kandidaten,
Basispruefung, Validierung oder erneute Wirkungsbestimmung. Fingerprints mit
Secret-Material werden nicht ausgegeben. Eine Kollision darf nie einen anderen
Kandidaten aktivieren.

Vor Commit werden Basiszustand, Validierung und Wirkung neu berechnet. Eine
abweichende Wirkung fuehrt zu Ablehnung und neuem Preview statt stiller
Umdeutung zwischen sofort, `FutureRunsOnly`, Pending und unzulaessig.

### Kapazitaet und Lebenszyklus

Release 1 besitzt genau einen globalen Previewplatz. Lebend sind `Ready` und
`Committing`. Ein abgelaufenes Preview wird vor Neuerstellung logisch verworfen;
ein nicht abgelaufenes wird nie verdraengt. Ein zweiter Versuch erhaelt
`PreviewCapacityReached`.

Ein Preview lebt hoechstens 15 Minuten monotone Zeit; Lesen verlaengert nicht.
Nur sein Owner darf es in `Ready` abbrechen. Andere Owner sehen keinen Inhalt.
Owner-, Handle-, Token- und Ablaufpruefung sowie Belegung des globalen
Mutationsslots erfolgen gemeinsam. Ist der Slot belegt, bleibt Preview `Ready`.
Danach wechselt es atomar zu `Committing`, kann weder abgebrochen noch doppelt
committed werden und wird nach Erfolg oder Fehler verbraucht.

Preview-eigene variable Daten sind auf 49.152 Bytes begrenzt. Der getrennte
Record-Arbeitsbereich haelt hoechstens einen Record von 32.817 Bytes. Die Summe
beider kontrollierter Bereiche ist 81.969 Bytes, aber keine Aussage ueber die
gesamte Heapspitze.

Die Aenderungszusammenfassung enthaelt hoechstens 256 Eintraege. Eine groessere
detaillierte Zusammenfassung wird deterministisch auf Programm- oder
Dokumentebene aggregiert und nie still gekuerzt. Nach Veroeffentlichung ist das
Preview unveraenderlich und erzeugt keine weiteren eigenen Allokationen. Kann
ein fachlich maximaler Kandidat nicht dargestellt werden, wird die Grenze nicht
still gelockert, sondern Bedarf und Ursache werden vor Anpassung berichtet.

Secret-Werte bleiben nur im fluechtigen zentralen Kandidaten. Sie erscheinen
nie in Preview-Antworten, oeffentlichen Fingerprints, Zusammenfassungen, Logs,
Diagnosen oder Exporten. Oberflaechen erhalten nur typisierte Hinweise auf die
Art der Secret-Aenderung. Abbruch, Ablauf, Neustart und verbrauchender Commit
machen sie unerreichbar. Sichere physische RAM-Loeschung wird nur bei
nachweisbarer Plattformgarantie zugesichert.

### Globale Mutation und Konflikte

In der gesamten Konfigurations- und Secret-Persistenzdomäne laeuft hoechstens
eine persistente Mutation gleichzeitig. Dies umfasst Konfiguration,
Connectivity, Authentication, Pending, Migration, Bootstrap und Werksreset.
Lesen und Preview-Berechnung duerfen parallel erfolgen.

Nach Belegung werden erneut geprueft:

- Preview-ID und Kandidatenfingerprint
- StorageEpoch und Active-Generation
- erwartetes Pending oder dessen Nichtvorhandensein
- erwartetes Nichtvorhandensein einer Aktivierungsabsicht
- bei Authentication committed Generation und CredentialEpoch

Lauf- und Aktivierungsbedingungen werden unmittelbar vor Commit erneut
bewertet. Abweichungen werden typisiert als Konflikt abgelehnt; es gibt kein
automatisches Merge.

Eine Sequenz wird erst nach globaler Belegung, allen Basispruefungen,
vollstaendiger Validierung, Aktivierungsvorpruefung und No-op-Pruefung dauerhaft
reserviert. Luecken sind zulaessig; reservierte Werte werden nie wiederverwendet
und laufen nicht still ueber.

### Aenderungsmetadaten und Ergebnisse

`ChangeOrigin` besitzt stabile Wire-IDs fuer:

- `InternalSystem`
- `LocalDisplay`
- `WebInterface`

`ChangeOperation` besitzt stabile Wire-IDs fuer:

- `NormalEdit`
- `FactoryInitialization`
- `BackupImport`
- `SchemaMigration`
- `FactoryReset`
- `StandardProgramReset`

Autorisierungsrollen sind weder Origin noch Operation. Unbekannte Metadaten-IDs
werden als `Unknown` mit roher Wire-ID erhalten und invalidieren den fachlichen
Inhalt nicht.

Ein Commit liefert genau eine typisierte Kategorie:

- `ConfigurationCommitSuccess`
- `ConfigurationValidationFailure`
- `PersistenceFailure`
- `ConfigurationConflictFailure`
- `ActivationFailure`
- `MigrationFailure`

Erfolg unterscheidet mindestens `NoChange`, `Activated`, `StoredAsPending` und
`ReplacedPending`. Fehler enthalten stabile Codes und nur passende strukturierte
Details. Freie Texte sind kein API-Vertrag. Secret-Fehler enthalten keine
geheimen oder daraus abgeleiteten Payloadbestandteile.

## Laufzeitaktivierung

Vor dem persistenten Root-Commit werden der vollstaendige Kandidat erneut
validiert, Plattformwerte wie die Zeitzone aufgeloest und alle falliblen
Ressourcen vorbereitet. Das Ergebnis ist ein unveraenderlicher
`RuntimeConfigurationSnapshot` ohne sichtbare Wirkung.

Der erfolgreiche dauerhafte Schreibvorgang eines neuen gueltigen
`ConfigurationRootRecord` mit hoeherer `rootSequence` ist der einzige
persistente Linearisierungspunkt. Vorher darf Vorbereitung ohne fachliche
Wirkung verworfen werden. Danach wird innerhalb derselben exklusiven Mutation
der vorbereitete Snapshot nicht allokierend, nicht serialisierend und nicht
fehlschlagend atomar sichtbar. Leser sehen nur die vollstaendig alte oder neue
Generation.

Ein Fehler vor Root-Commit laesst alte Konfiguration und Snapshot unveraendert.
Eine unerwartete Publish-Vertragsverletzung nach Root-Commit fuehrt nicht zu
automatischem Rollback, sondern zu sicherer Konfigurationsstoerung und
gesperrten Aktoren. Verlorene Aenderungsmeldungen aendern den abrufbaren
aktuellen Snapshot nicht.

Stromausfall vor Root-Commit laedt den alten Graphen; nach Root-Commit den neuen.
Stromausfall nach Publish, aber vor Intent-Abschluss behaelt den neuen Graphen
und setzt den Abschluss idempotent fort.

## Secret-Domaene

Konfigurationsdokumente enthalten weder Geheimnisse noch Passwort- oder PIN-
Pruefnachweise. Die getrennte Secret-Domaene unterscheidet Connectivity,
Webpasswort und Service-PIN. Integritaetspruefung wird nicht als
Verschluesselung dargestellt.

### Connectivity

Es existieren vier WLAN-Secret-Revisionsslots und vier
`ConnectivitySecretSetManifest`-Slots, aber keine eigenen Connectivity-Roots.
Ein Manifest wird nur durch einen vollstaendig gueltigen Active-, Fallback- oder
Pending-Konfigurationsgraphen wirksam. Eine passende Aktivierungsabsicht und die
laufende serialisierte Schreiboperation schuetzen ebenfalls ihre Referenzen.

Bei unveraenderten WLAN-Secrets wird dieselbe Generation weiterverwendet. Neue
Secrets werden zuerst geschrieben und geprueft; erst ein neues
Konfigurationsmanifest aktiviert die Kombination. Active, Pending und Fallback
duerfen jeweils passende Generationen referenzieren. Solange eine Referenz
existiert, wird die Revision nicht bereinigt. WLAN-Secrets duerfen mit der
Konfiguration zurueckfallen.

Schema 1 des Connectivity-Manifests enthaelt ausschliesslich StorageEpoch,
Manifestgeneration und den kanonischen Zustand `NotProvisioned`; es enthaelt
keine Secret-Referenzen.

### Authentication

Es existieren:

- 3 Slots fuer Webpasswort-Pruefnachweise
- 3 Slots fuer Service-PIN-Pruefnachweise
- 3 `AuthenticationManifest`-Slots
- 2 redundante CRC-geschuetzte `AuthenticationRootRecord`-Slots

Roots sind vorwaertsgerichtet und besitzen `Prepared` oder `Committed`. Nur ein
vollstaendig gueltiger `Committed`-Root erlaubt Authentifizierung. Credential-
und Manifestslots werden vorbereitet, danach ein Root als Prepared und der
zweite als Committed geschrieben. Erst nach erfolgreicher Pruefung des neuen
Committed-Roots ist die Aenderung aktiv; anschliessend wird der vorbereitete
Root auf dieselbe committed Generation nachgefuehrt.

Nach Aktivierung darf kein committed Root der aelteren CredentialEpoch
verbleiben. Alte Nachweise sind widerrufen und erlauben keine Anmeldung. Vor der
Commit-Grenze bleibt die alte committed Authentifizierung aktiv. Ein Prepared-
Root allein erlaubt nie Anmeldung. Fehlt ein gueltiger committed Root, wird
keine alte Epoche reaktiviert; Authentifizierung ist sicher nicht verfuegbar.

Authentication Schema 1 enthaelt ausschliesslich StorageEpoch,
Manifestgeneration, CredentialEpoch sowie `NotProvisioned` fuer Webpasswort und
Service-PIN, ohne Pruefnachweisreferenzen. StorageEpoch und CredentialEpoch
beginnen bei 1; 0 ist reserviert.

Issue #27 fuehrt mit neuer Schemageneration reale WLAN-Dokumente und -Grenzen,
Pruefnachweise, KDF- und Algorithmus-IDs, Salt, Work-Factor, gegebenenfalls
Pepper, Provisioned/Disabled, Anmeldung, Sitzungen, Tokens, CSRF, Sperrzeiten
und konkrete Befehle ein. Issue #16 legt keine freie oder opake produktive
Secret-Payload an. Seine generischen Mechanismen werden mit ausschliesslich im
Test-Support vorhandenen typisierten Beispieldokumenten ohne Produktions-Wire-
IDs geprueft.

Normale Backups besitzen keine Secret-Bindung und enthalten weder Secret-
Inhalte noch Pruefnachweise. Fehlende Secrets eines Imports ueberschreiben keine
bestehenden Secrets und werden als `Neueinrichtung erforderlich` behandelt.

## Bootstrap, StorageEpoch und Werksreset

### BootstrapRecord

Es existieren zwei redundante CRC-geschuetzte
`ConfigurationBootstrapRecord`-Plaetze. Ein Record enthaelt mindestens monotone
BootstrapSequence, Speicherformatversion, StorageEpoch und einen Zustand:

- `Initializing`
- `Initialized`
- `Resetting`

`NotFound` ist kein gespeicherter Zustand. Lesefehler werden nie wie NotFound
behandelt.

Automatische Initialisierung ist nur erlaubt, wenn alle erforderlichen Reads
erfolgreich waren, weder gueltige noch beschaedigt vorhandene Roots erkannt
wurden und kein BootstrapRecord existiert. Vor der ersten Dokumentrevision wird
`Initializing` persistiert.

Danach entstehen deterministisch unter StorageEpoch 1:

- UserConfiguration Revision 1: `de`, `Europe/Zurich`,
  `Fermentationsschrank`
- leere ServiceConfiguration Revision 1
- ProgramCatalog Revision 1 mit vier Factory-Arbeitskopien ohne Benutzerprogramm
- ConfigurationManifest Generation 1
- ConfigurationRootRecord rootSequence 1 mit Active Generation 1 ohne Fallback
- Connectivity- und Authentication-Manifeste Generation 1 als `NotProvisioned`

FactoryConfiguration selbst wird nicht gespeichert. Erst nach Schreiben,
Ruecklesen und Validieren des gesamten Root-Graphen wird Bootstrap atomar auf
`Initialized` fortgeschrieben.

Stromausfall in `Initializing` nimmt deterministisch wieder auf. Existiert
bereits ein gueltiger Graph, wird er verwendet und nur Bootstrap vervollstaendigt.

### Beschaedigte Daten

Vorhandene, ungueltige, beschaedigte oder unlesbare Daten sind nie fabrikneuer
Speicher. RootRecords werden nach Sequenz geprueft: Active-Graph, zugehoeriger
Fallback und danach der neueste vollstaendig nutzbare Graph. Verwendeter
Fallback wird stabil diagnostiziert.

Gibt es weder Active noch Fallback, wird keine Factory-Konfiguration erzeugt,
nichts still geloescht und nichts teilweise aktiviert. Das Geraet wechselt in
einen sicheren Konfigurationsfehlerzustand beziehungsweise `SAFE_BOOT`.

Firmwareupdates ueberschreiben keine Dokumente oder Standard-Arbeitskopien.
Neue Factory-Vorlagen wirken nur bei Ersteinrichtung, bewusstem Vollreset oder
bewusstem Standardprogrammreset. Unbekannte oder nicht migrierbare Schemas
loesen keine Factory-Neuanlage aus.

### StorageEpoch

Die gesamte Konfigurations- und Secret-Persistenz ist an die aktuelle
StorageEpoch gebunden, mindestens Bootstrap-, Konfigurations-, Pending- und
Authentication-Roots, Intent, Manifeste, Dokumente, Connectivity-Secrets sowie
Passwort- und PIN-Nachweise. Eine Referenz ist nur gueltig, wenn alle Objekte
dieselbe aktuelle Epoche besitzen.

### Werksreset

Ein bestaetigter Vollreset ist mit `BootstrapState::Resetting` wiederaufnehmbar:

1. StorageEpoch ohne stillen Ueberlauf erhoehen
2. Resetting unter neuer Epoche persistieren
3. Referenzen und Objekte alter Epochen logisch ungueltig machen
4. Connectivity und Authentication neu als `NotProvisioned` erzeugen
5. neue Initialkonfiguration unter der neuen Epoche aktivieren
6. Bootstrap als `Initialized` abschliessen

Der Reset invalidiert Active, Pending und Fallback, macht alte Secrets logisch
unerreichbar und reaktiviert nie eine fruehere CredentialEpoch. Eine sichere
physische Loeschung alter Flashbytes wird ohne Plattformnachweis nicht
zugesichert. Touchkalibrierung bleibt erhalten. Bedienablauf, physische Geste
sowie Lauf-, Journal- und Historiendaten bleiben bei ihren zustaendigen Issues.
Korruption loest niemals automatisch einen Werksreset aus.

## Speicherport und Modulgrenzen

Der bestehende Schluessel/Wert-Port reicht aus. Slots und Roots besitzen feste,
deterministische Schluessel. Listing, Verzeichnisse, Rename und
Mehrschluesseltransaktionen sind nicht erforderlich.

Der Port garantiert je Schluessel binaersicheres, dauerhaftes und
stromausfallsicheres Replace: Nach Unterbrechung existiert der vollstaendige
alte oder neue Wert; ein abgeschnittener oder gemischter Wert ist kein
erfolgreicher Write; ein erfolgreich zurueckgekehrter Write ist dauerhaft;
NUL und beliebige Bytes bleiben erhalten. Ein Adapter muss diese Garantie
herstellen oder ist inkompatibel.

`device_platform` enthaelt ausschliesslich anwendungsneutrale Bausteine:

- `IStateStore`, `ISecureRandomSource`, `ITimeSource`, `ITimeZoneResolver`
- begrenzte Byte-Reader und -Writer, Big-Endian-Primitiven und CRC
- generischen Envelope, feste Revisionsslots und redundante Recordslots
- technische Kandidatenpruefung und -sortierung
- starke Typen fuer StorageEpoch, Revision, Generation, RecordSequence, SlotId
  und RecordTypeId
- technische Speicher-, Envelope-, Integritaets- und Sequenzfehler

Die Plattform erhaelt Schluessel, Slotzahl, Record-Type und Schutzmengen von der
Anwendung. Sie aktiviert nie allein aufgrund hoher Sequenz und kennt keine
konkreten Dokumente, Manifestbedeutungen, Factorywerte, Programme, Pending-
Regeln, Authentifizierungsbedeutung, Preview, Lauf oder Aktivierungswirkung.

`fermentation_app` besitzt konkrete Dokumente, Schemas, IDs, Schluessel,
Manifest-, Root-, Bootstrap- und Intentbedeutung, Graphvalidierung,
ProgramCatalog, Preview, Pending, Secret-Manifeste, Migration, Boot/Recovery,
RuntimeConfigurationSnapshot und Laufschutz.

`device_platform_test_support` enthaelt nur anwendungsneutrale Testadapter wie
`SimulatedPersistentStateStore`, kontrollierbare Zufallsquelle, Cut-Point- und
Korruptionsinjektion. Fachliche Orakel bleiben in den Anwendungstests. Kein
Produktionsmodul oder ESP32-Profil darf Test-Support referenzieren.

`src/main.cpp` instanziiert und verbindet Adapter, Dienst und Anwendung, enthaelt
aber keine Persistenz-, Migrations-, Recovery-, Validierungs- oder
Aktivierungslogik. Es entsteht kein universelles Datenbank-, ORM-, Plugin- oder
dynamisches Schemasystem.

## Ressourcenvertrag

Issue #16 erzwingt zentrale Softwareobergrenzen fuer Preview, Recordpuffer,
typisierten ProgramCatalog und alle Payloads, aber behauptet damit kein reales
Hardwarebudget und keine PSRAM-Verfuegbarkeit.

Ein Preview wird erst nach erfolgreicher Ressourcenbereitstellung sichtbar.
Vor Root-Commit bricht jeder Ressourcenfehler typisiert ohne Teilaktivierung ab.
Nach Root-Commit allokiert, serialisiert und reserviert Publish nichts und kann
nicht fehlschlagen. Waehrend Commit existiert hoechstens ein vollstaendiger
kodierter Dokument- oder Recordpuffer.

Native Tests decken maximalen Katalog, Preview, Groessenberechnung,
Serialisierung, Grenzwerte, jede fallible Vorbereitung, Freigabe nach Erfolg
und Fehler sowie fehlende Ressourcen nach Root-Commit ab. Sie verwenden keine
C++-Exceptions als Voraussetzung.

Beide ESP32-Produktionsprofile muessen bauen. Base-SHA und PR-Head werden mit
identischer Toolchain und sauberem Build fuer statisches RAM, Flash,
`firmware.bin` und `firmware.elf` verglichen. Werte bleiben informativ, bis
reale Budgets bestaetigt sind. Test-Support darf nicht im Produktionsartefakt
liegen.

Ohne reale Messung werden kein freier Heap, groesster Block, Parallelreserve,
reale Flash-Replace-Atomizitaet oder Flashlebensdauer zugesichert. Diese Punkte
bleiben verknuepfte Hardware-/Adapterpruefungen und spaetere Release-Gates.

## Verbindliche native Tests

### Simulierter Persistenzspeicher

`device_platform_test_support` stellt einen binaersicheren
`SimulatedPersistentStateStore` bereit. Er trennt committed Daten, aktuelle
Schreiboperation und fluechtigen Zustand. Pro Write sind Fehler vor Beginn,
Stromausfall vor Commit, Stromausfall nach Commit vor Rueckkehr, Erfolg sowie
gezielte Read-, NotFound- und Korruptionsinjektionen simulierbar. Stromausfall
beendet den Kontrollfluss ohne normale Rueckkehr. Nach Neustart bleiben nur
committed Daten.

Abgeschnittene oder veraenderte Werte sind nachtraegliche Korruptionsinjektion,
nicht erlaubtes Ergebnis eines erfolgreichen atomaren Writes.

### Cut-Point-Matrix

Jeder mehrstufige Workflow wird erfolgreich zur Aufzeichnung aller Writes
ausgefuehrt und dann von identischem Ausgangszustand mit Stromausfall unmittelbar
vor und nach jeder Commit-Grenze wiederholt. Dienste und Runtime werden jeweils
neu aufgebaut; Zeit, Zufall und externe Quellen sind kontrollierbar.

Dies umfasst mindestens Bootstrap, sofortige Aktivierung, Pending erzeugen,
ersetzen und verwerfen, `Anwenden und neu starten`, kombinierte Connectivity-
und Konfigurationstransaktion, Authentication, Active-/Pending-Migration und
Werksreset.

Jeder Cut besitzt ein konkretes Recovery-Orakel; ein allgemeines SAFE_BOOT ist
kein pauschal erlaubtes Ergebnis. Immer gelten:

- keine gemischten Dokumentgenerationen
- keine Aktivierung allein wegen hoher Generation
- keine Wirkung unreferenzierter Secrets
- keine Anmeldung ueber Prepared-Authentication
- keine Reaktivierung widerrufener Credentials
- keine Pending-Aktivierung ohne passende Absicht
- keine Aenderung eines aktiven oder wiederherzustellenden Laufs
- keine Behandlung beschaedigter Daten als leer
- keine Aktorfreigabe bei unklarem Konfigurationszustand

Korruptionstests unterscheiden rohe Byteaenderung ohne CRC-Anpassung und
semantisch ungueltige, CRC-korrekt neu kodierte Datensaetze.

### Golden- und Aktivierungstests

Golden-Vektoren pruefen mindestens CRC-Checkwert, Envelope mit und ohne UTC,
Big-Endian-Integer, Revisionen, Bool, Optional, signierte Integer, binary64,
`-0.0`, UTF-8-Grenzen und bekannte/unbekannte Aenderungsmetadaten.

Ausserdem sind mindestens verbindlich:

- Vorbereitung scheitert: keine Revision, kein Root, alter Snapshot aktiv
- Root-Write scheitert: alter Root und Snapshot aktiv
- Stromausfall direkt vor Root: alter Graph
- Stromausfall direkt nach Root: neuer Graph
- Stromausfall nach Publish vor Intent-Abschluss: neuer Graph, Abschluss weiter
- Pending-Ziel bereits Active: keine zweite Aktivierung, idempotenter Abschluss
- Allocation-Failure nach `prepare()`: Publish allokiert nicht und gelingt
- Zeitzonenaufloesung scheitert: kein Root-Commit
- Leser waehrend Aktivierung: immer vollstaendig alte oder neue Generation
- verlorene Aenderungsmeldung: aktueller Snapshot bleibt abrufbar
- aktiver Lauf: Laufschnappschuss bleibt byte- beziehungsweise wertgleich
- simulierte Publish-Vertragsverletzung: kein Rollback, Aktoren gesperrt,
  sichere Konfigurationsstoerung

Die vollstaendige Matrix laeuft nativ. `esp32_bringup` und `esp32_release`
kompilieren ohne Testadapter und liefern Ressourcenberichte. Ein konkreter
ESP32-Speicheradapter benoetigt spaeter eigene Reset- und Atomizitaetstests; bis
dahin ist seine reale Replace-Garantie nicht bestaetigt.

## Ausdruecklicher Nicht-Scope

Issue #16 implementiert noch keine:

- reale WLAN-Credential-Dokumente oder Netzwerkvalidierung
- Passwort-/PIN-Pruefnachweise, KDF, Salt, Work-Factor oder Pepper
- Anmeldung, Sitzungen, Tokens, CSRF oder Sperrzeiten
- konkrete Display-, Ton-, Sensor-, Regel-, Sicherheits- oder Hardwarefelder
- lokale Terminplanung oder lokale-Zeit-nach-UTC-Regeln
- portable Backupserialisierung oder rohe Envelope-Exporte
- Laufpersistenz, Laufjournal oder Historienbereinigung
- physische Recovery-Geste oder reale Flashgarantie

Neue Bereiche und Felder erhalten erst in ihrem zustaendigen Issue eine eigene
Schemageneration und getestete Migration. Es werden keine Reserve- oder
Platzhalterfelder angelegt.
