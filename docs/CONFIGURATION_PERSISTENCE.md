# Technischer Vertrag fuer Konfigurationspersistenz

## Status und Geltungsbereich

Dieses Dokument ist der kanonische technische Vertrag fuer den Themenbereich
von Issue #16. Es konkretisiert:

- [`SETTINGS_AND_STORAGE.md`](SETTINGS_AND_STORAGE.md)
- [`BACKUP_SECURITY_RETENTION.md`](BACKUP_SECURITY_RETENTION.md)
- [`PR38_REVIEW_CORRECTIONS.md`](PR38_REVIEW_CORRECTIONS.md)

Es nimmt weder Laufpersistenz aus Issue #17, das portable Backupformat aus
Issue #19, die Fehler- und Verriegelungspolitik aus Issue #24 noch konkrete
WLAN- und Authentifizierungsdaten aus Issue #27 vorweg.

Der Release-1-Vertrag folgt ADR-018 (Variante B). Der gesamte Vertrag wird nicht
als ein einziger Implementierungs-PR umgesetzt. Issue #16 bleibt ein
Tracking-Issue; #54 und #55 bilden das bereits umgesetzte Fundament, #56 und
#57 sind die verbleibenden kleinen Umsetzungspakete. Jedes Paket benoetigt vor
seiner Implementierung einen eigenen freigegebenen Plan.

Reale Flash-Atomizitaet, reale Heapreserve und Belastungsmessungen bleiben
spaetere Hardware- beziehungsweise Release-Gates.

## Architektur und Konfigurationsgeneration

`FactoryConfiguration` ist unveraenderlicher Bestandteil der Firmware.

Benutzer-, Service- und Programmkonfiguration werden als getrennte,
vollstaendig typisierte und jeweils schema-versionierte Dokumente gespeichert:

- `UserConfiguration`
- `ServiceConfiguration`
- `ProgramCatalog`

Die Dokumente besitzen eigene Inhaltsrevisionen, werden aber nicht unabhaengig
aktiviert. Ein `ActiveConfigurationManifest` verweist auf genau eine
vollstaendig validierte Kombination konkreter Revisionen von
`UserConfiguration`, `ServiceConfiguration` und `ProgramCatalog` derselben
`StorageEpoch`. Diese Kombination bildet die aktive Konfigurationsgeneration.

Eine Aenderung wird logisch in folgender Reihenfolge ausgefuehrt:

1. nur tatsaechlich geaenderte Dokumente als neue Revision vorbereiten
2. zusammen mit unveraenderten referenzierten Dokumenten einen vollstaendigen
   Kandidaten bilden
3. den gesamten Kandidaten technisch und fachlich validieren
4. neue Dokumentrevisionen schreiben und ruecklesen
5. ein Manifest mit exakten Referenzen schreiben und pruefen
6. einen neuen gueltigen RootRecord committen
7. den vorbereiteten Runtime-Snapshot nicht fehlschlagend atomar
   veroeffentlichen
8. die vorherige aktive Generation als genau eine Rueckfallgeneration behalten

Unveraenderte Dokumentrevisionen duerfen von mehreren Manifesten gemeinsam
referenziert werden. Inhaltsgleichheit wird nie allein anhand eines CRC
angenommen.

Die `FactoryConfiguration` selbst und firmwarefeste Grenzen werden weder als
ueberschreibbares Dokument gespeichert noch in jede Revision kopiert. Davon zu
unterscheiden sind die initialen Benutzerwerte und die gespeicherten
Arbeitskopien der vier Factory-Programme.

Der aktive Lauf bleibt ausserhalb der Geraetekonfiguration und verwendet seinen
unveraenderlichen Laufschnappschuss.

## Dokumente der ersten Schemageneration

### UserConfiguration Schema 1

Schema 1 enthaelt ausschliesslich:

- Sprache des lokalen Touchdisplays
- kanonische IANA-Zeitzone
- sichtbarer Geraetename

#### DisplayLanguageId

Die gespeicherte `DisplayLanguageId`:

- umfasst 2 bis 16 ASCII-Bytes
- erlaubt `a-z`, `0-9` und `-`
- beginnt und endet alphanumerisch
- enthaelt keine aufeinanderfolgenden Bindestriche
- wird nicht normalisiert
- wird exakt gegen den versionierten Firmware-Uebersetzungskatalog geprueft

Release 1 unterstuetzt mindestens `de`, `es` und `en`; Factory-Standard ist
`de`. Die IDs sind interne Firmwarekatalog-IDs und kein vollstaendiges
BCP-47-Datenmodell. Die Websprache bleibt browser- beziehungsweise
sitzungslokal.

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
zusaetzlich:

1. exakte Uebereinstimmung im versionierten Firmware-Zeitzonenkatalog
2. erfolgreiche Vorbereitung durch `ITimeZoneResolver`

Der Katalog garantiert mindestens `Europe/Zurich` und ist unabhaengig vom
Konfigurationsschema versioniert. Nicht gespeichert werden lokalisierte Namen,
feste UTC-Offsets, aktueller Sommerzeitstatus, freie POSIX-TZ-Strings oder
kopierte Zeitzonenregeln.

Eine Zeitzonenaenderung ist nicht neustartpflichtig. Sie veraendert nur lokale
Darstellung und zukuenftige Formatierung. UTC-Werte, Generationen, monotone
Zeiten, Laufwerte, Laufrevisionen und Laufschnappschuss bleiben unveraendert.
Lokale-Zeit-nach-UTC-Terminplanung ist nicht Scope von Issue #16.

#### Sichtbarer Geraetename

Factory-Standard ist `Fermentationsschrank`. Der Name:

- ist gueltiges UTF-8
- enthaelt 1 bis 48 Unicode-Skalarwerte
- belegt hoechstens 96 UTF-8-Bytes
- enthaelt mindestens einen Nicht-Leerraumwert
- besitzt keinen fuehrenden oder abschliessenden Unicode-Leerraum
- enthaelt keine NUL-, C0-, C1-, Zeilen- oder Absatztrennzeichen

Der Wert wird weder gekuerzt noch getrimmt oder automatisch Unicode-
normalisiert. Hostname, mDNS-Name, WLAN-SSID und andere Netzwerkableitungen
folgen mit Issue #27.

Schema 1 enthaelt noch keine Displayhelligkeit, Abdunkelzeit, Lautstaerke,
Tonparameter, Netzwerkparameter, Aufbewahrungsgrenzen oder Hardware-, Sensor-,
Regel- und Sicherheitswerte.

### ServiceConfiguration Schema 1

Schema 1 ist ein gueltiges typisiertes Dokument ohne fachliche Parameterfelder
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

Die unveraenderlichen Factory-Vorlagen verbleiben in der Firmware.
Ersteinrichtung und Werksreset erzeugen daraus Arbeitskopien. Ein
Firmwareupdate ueberschreibt vorhandene Arbeitskopien nicht. Ein bewusstes
Zuruecksetzen eines Standardprogramms ersetzt nur dessen Arbeitskopie durch die
aktuelle, vollstaendig validierte Firmwarevorlage.

Die reservierten Factory-IDs muessen genau einmal vorhanden sein. Factory-
Arbeitskopien stehen zuerst und in der Reihenfolge des Firmwarekatalogs.
Benutzerprogramme folgen in gespeicherter Reihenfolge. Die Position ist die
Anzeigereihenfolge; es gibt kein zusaetzliches Sortierfeld.

Programme mit `TBD_COMMISSIONING`-Werten duerfen als strukturell gueltige
Katalogvorlage gespeichert werden. Vor jedem Start bleibt die vollstaendige
`Runnable`-Validierung erforderlich. Der Katalog enthaelt keinen aktiven Lauf,
Laufschnappschuss oder Laufrevisionen.

#### Programm-ID

Eine Programm-ID:

- umfasst 1 bis 48 ASCII-Bytes
- erlaubt nur `a-z`, `0-9` und `-`
- beginnt und endet alphanumerisch
- enthaelt keine aufeinanderfolgenden Bindestriche
- ist im gesamten Katalog eindeutig
- verwendet keine reservierte Factory-ID fuer ein Benutzerprogramm
- ist nach Erstellung unveraenderlich

Es gibt keine stille Normalisierung.

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
abgelehnt; Programme werden weder geloescht noch abgeschnitten.

## Schema, Revision und Migration

Folgende fachliche Schemas entwickeln sich unabhaengig:

- `UserConfigurationSchema`
- `ServiceConfigurationSchema`
- `ProgramCatalogSchema`
- bestehendes `ProgramDocumentSchema`
- `ConfigurationManifestSchema`

Es wird keine zweite Programmschema-Definition eingefuehrt.

- Schema-Version beschreibt die fachliche Datenstruktur.
- Dokumentrevision identifiziert einen konkreten Dokumentinhalt.
- Manifestgeneration identifiziert eine validierte Kombination.
- Envelope-Version beschreibt ausschliesslich den generischen Speicherrahmen.

Revisionen und Generationen verwenden `uint64_t`, beginnen bei 1, reservieren 0
und laufen niemals still ueber.

Release 1 benoetigt keine separate persistente `MutationSequence`. Der fuer
#56 erbrachte Gleichwertigkeitsnachweis verwendet je `StorageEpoch` die
High-Water-Marks aller technisch gueltigen Dokumentrevisionen,
Manifestgenerationen und Rootsequenzen. Auch verwaiste Vor-Root-Writes
verbrauchen ihre Identitaet fuer andere Inhalte; neue Werte entstehen checked
als `max + 1`. `NoChange` schreibt nichts. Konflikterkennung, eindeutige
Commitaufloesung und Wiederholung besitzen damit keine zusaetzliche Information,
die ein weiterer persistenter Zaehler liefern wuerde. Eine spaetere Einfuehrung
waere eine materielle Planaenderung und benoetigt Ownerfreigabe.
Unterschiedliche starke Typen bleiben auch bei gleichen numerischen Werten
getrennt; unveraenderte Dokumente behalten ihre Revision. Gleiche fachliche
Identitaeten mit unterschiedlichen kanonischen Bytes werden bereits beim Scan
als Persistenzintegritaetsfehler abgelehnt. Ein Revisionsueberlauf blockiert nur
die tatsaechlich geaenderten Dokumenttypen; Manifest- und Rootueberlauf
blockieren jede schreibende Aktivierung.

Migrationen sind ausschliesslich Copy-Migrationen:

1. alten Envelope und vollstaendige Revision lesen
2. Magic, Laenge und Integritaet pruefen
3. altes Schema mit seinem Decoder dekodieren
4. jeden Migrationsschritt einzeln auf einer Kopie ausfuehren
5. aktuelles fachliches Modell erzeugen
6. Dokument und Gesamtkonfiguration vollstaendig validieren
7. nur erforderliche neue Dokumentrevisionen schreiben
8. neues Manifest schreiben und pruefen
9. durch einen neuen kanonischen Root atomar aktivieren
10. vorherige Manifestgeneration bis zum nachweislichen Erfolg behalten

Es gibt keine In-place-Migration und keine erfundenen Defaults fuer noch
ungeklaerte Sicherheits-, Sensor- oder Prozesswerte. Unbekannte neuere
Versionen werden abgelehnt. Eine Migration wird wie jede andere Aenderung erst
nach vollstaendiger Validierung und Runtimevorbereitung durch den Root-Commit
aktiviert.

Fuer erstmals eingefuehrte Schema-1-Dokumente wird keine Schema-0-Migration
erfunden. Der generische Copy-Ablauf wird mit Testschemas und die bestehende
ProgramDocument-Migration im ProgramCatalog getestet.

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
ChangeOrigin- und ChangeOperation-IDs bilden diese ausdrueckliche Ausnahme;
sie liegen gemaess ADR-016 in der Payload, nicht im Envelope.

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
Null wird als nicht kanonisch abgelehnt.

### Envelope-Version 1

Die kanonische Bytefolge lautet:

1. Magic `DPRF`: `44 50 52 46`
2. Envelope-Version: `uint16`, Big Endian
3. Record-Type-ID: `uint16`, Big Endian
4. fachliche Schema-Version: `uint32`, Big Endian
5. StorageEpoch: `uint64`, Big Endian
6. VersionValue: `uint64`, Big Endian
7. Payloadlaenge: `uint32`, Big Endian
8. UTC-Optionaltag: ein Byte
9. bei `0x01`: UTC-Unix-Sekunden als `int64`, Big Endian
10. CRC-32/ISO-HDLC: `uint32`, Big Endian
11. Payload: exakt die angegebene Byteanzahl

`ChangeOrigin` und `ChangeOperation` sind gemaess ADR-016 nicht mehr Teil des
Envelopes; sie wandern als Anwendungssemantik in die Payload.

`VersionValue` wird je Record-Type als Revision, Generation oder RecordSequence
interpretiert; die Produktionscode-Typen bleiben getrennt.

Ungueltig sind Envelope-Version, Record-Type-ID, Schema-Version, StorageEpoch
oder VersionValue 0 sowie UTC-Tags ausser `0x00` und `0x01`. Unbekannte
Envelope-Versionen lehnt `device_platform` ab. Unbekannte Anwendungs-Record-
Types lehnt `fermentation_app` vor der Payloaddekodierung ab. Das Erhalten
unbekannter roher ChangeOrigin-/ChangeOperation-IDs ist damit Aufgabe der
Payloaddekodierung in `fermentation_app`, nicht des Envelopes.

Der Envelope besitzt keine Padding-, Reserve-, Flags- oder ABI-abhaengigen
Bytes. Seine Groesse einschliesslich CRC betraegt 37 Bytes ohne und 45 Bytes mit
UTC. Bei maximal 32.768 Payloadbytes ist ein Konfigurationsrecord hoechstens
32.813 Bytes gross.

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
Headerbytes vor dem CRC und anschliessend die vollstaendige Payload. Das
CRC-Feld selbst ist ausgeschlossen. Der numerische CRC wird Big Endian
gespeichert und durch Neuberechnung verglichen. Die Residue ist keine
Pruefregel fuer den vollstaendigen Big-Endian-Datensatz. CRC ist weder
Manipulationsschutz noch Authentifizierung oder Verschluesselung.

### Laengen- und Allokationsschutz

Payloadgrenzen ohne Envelope:

- globales Konfigurationsmaximum: 32.768 Bytes
- ProgramCatalog Schema 1: hoechstens 32.768 Bytes
- UserConfiguration Schema 1: hoechstens 256 Bytes
- ServiceConfiguration Schema 1: exakt 0 Bytes

Jeder feste Record- oder Manifesttyp besitzt je Schema und Variante eine exakt
definierte kanonische Payloadlaenge. Fehlende und zusaetzliche Bytes werden
abgelehnt. Vor jeder fachlichen Dekodierung werden vorhandene Bytes, Record-
Type, dokumenttypspezifisches Maximum, globales Maximum, ueberlaufsichere
Gesamtgroesse und behauptete Laenge geprueft. Der Speicheradapter setzt ein
caller- oder schluesselspezifisches maximales Leselimit durch.

Technische Envelope- und Wiregroessen liegen in `device_platform`; das globale
Konfigurationsbudget und die Dokumentgrenzen in `fermentation_app`.

### Fachliche Payloads von Schema 1

Alle folgenden Tabellen beschreiben ausschliesslich die fachliche Payload, die
der Envelope als Bytefolge umschliesst; Envelope-Header und -CRC gehoeren nicht
dazu. Mehrbyteige Werte sind Big Endian. Strings bestehen aus einer
`uint16`-Byteanzahl und exakt so vielen Bytes. Boolwerte und Optionaltags
verwenden ausschliesslich `0x00` und `0x01`; auf einen gesetzten Optionaltag
folgt unmittelbar der angegebene Wert. Listen verwenden eine vor der
Ergebnisallokation validierte `uint8`-Anzahl. Es gibt keine Padding-, Reserve-
oder ABI-abhaengigen Bytes.

#### UserConfiguration Schema 1

| Reihenfolge | Feld | Wirebreite | Teilgrenze |
|---:|---|---|---:|
| 1 | DisplayLanguageId | `uint16` Laenge + ASCII | 2..16 Byte |
| 2 | IANA-Zeitzone | `uint16` Laenge + ASCII | 1..64 Byte |
| 3 | sichtbarer Geraetename | `uint16` Laenge + UTF-8 | 1..48 Skalare, hoechstens 96 Byte |

Die Gesamtpayload ist hoechstens 256 Byte. Der Decoder prueft diese Grenze vor
der ersten Feldallokation und lehnt fehlende oder zusaetzliche Bytes ab.

#### ServiceConfiguration Schema 1

Die Payload enthaelt exakt null Bytes. Jedes Byte ist zusaetzlich und damit
ungueltig.

#### ProgramCatalog Schema 1

| Reihenfolge | Feld | Wirebreite | Teilgrenze |
|---:|---|---|---:|
| 1 | Programmanzahl | `uint8` | 4..16 |
| 2 | ProgramDocument 0..n-1 | variable Struktur unten | zuerst exakt 4 Factorykopien, danach hoechstens 12 Benutzerprogramme |

Die Katalogpayload ist hoechstens 32.768 Byte. Programmanzahl,
ProgramDocument-Teilgrenzen und Gesamtpayload werden unabhaengig geprueft.

| Reihenfolge je ProgramDocument | Feld | Wirebreite |
|---:|---|---|
| 1 | ProgramDocument-Schema | `uint32` |
| 2 | vorhandene ProgramField-Bits | `uint64` |
| 3 | ID | `uint16` Laenge + ASCII |
| 4 | Name | `uint16` Laenge + UTF-8 |
| 5 | Notizen | `uint16` Laenge + UTF-8 |
| 6..12 | `builtIn`, `factoryCatalogEntry`, `resettable`, `userDeletable`, `installed`, `enabled`, `preheat` | je ein Boolbyte |
| 13 | SensorPreference | `uint8` Wire-ID |
| 14 | ProductSensorFailurePolicy | `uint8` Wire-ID |
| 15 | optionale Fallbackverzoegerung | Optionaltag + optional `uint32` |
| 16 | Fermentationsphasenanzahl | `uint8`, Schema 4/5 exakt 1 |
| 17 je Phase | Zieltemperatur | Optionaltag + optional binary64 |
| 18 je Phase | Dauer | Optionaltag + optional `uint32` |
| 19 | Zielqualifikationsband | Optionaltag + optional binary64 |
| 20 | Zielqualifikationsdauer | Optionaltag + optional `uint32` |
| 21 | maximale Zielerreichungszeit | Optionaltag + optional `uint32` |
| 22 | maximale Produktwartezeit | nur Schema 5: Optionaltag + optional `uint32` |
| 23 | CompletionMode | `uint8` Wire-ID |
| 24 | Kuehlziel | Optionaltag + optional binary64 |
| 25 | Haltedauer | Optionaltag + optional `uint32` |

`ProgramDefinition::notes` besitzt im bestehenden ProgramDocument-Schema
bewusst kein eigenes `ProgramFieldMask`-Bit. Der ProgramCatalog kodiert die
Notizen immer an Position 5, unabhaengig von der Feldmaske. Es wird weder ein
neues Feldbit noch ein neues ProgramDocument-Schema eingefuehrt. Schema 4
enthaelt Position 22 nicht; nach vollstaendig erfolgreichem Dekodieren wird die
bestehende Copy-Migration 4 nach 5 angewendet. Schema 5 enthaelt Position 22.
Unbekannte ProgramDocument-Schemas, unbekannte Feldmaskenbits und fehlende
Pflichtbits werden vor einer Veroeffentlichung abgelehnt.

Stabile Enum-Wire-IDs:

| Enum | Wire-ID | Wert |
|---|---:|---|
| SensorPreference | 1 | ProductIfAvailableElseAir |
| | 2 | AirProductOptional |
| | 3 | ProductRequired |
| | 4 | AirOnly |
| ProductSensorFailurePolicy | 1 | FallbackToAirAfterTimeout |
| | 2 | WaitForUser |
| | 3 | StopToSafeState |
| CompletionMode | 1 | FinishWithoutCooling |
| | 2 | CoolThenFinish |
| | 3 | CoolAndHoldForDuration |
| | 4 | CoolAndHoldUntilManualStop |

Alle anderen Enum-Wire-IDs sind ungueltig. `ChangeOrigin` und
`ChangeOperation` sind weder Envelope- noch Schema-1-Dokumentfelder.

Die stabilen Anwendungs-Record-Type-IDs sind 1 fuer UserConfiguration, 2 fuer
ServiceConfiguration und 3 fuer ProgramCatalog. Die vier kurzen
ADR-016-Schluessel lauten je Dokumenttyp `uc0`..`uc3`, `sc0`..`sc3` und
`pc0`..`pc3`. Dies ist nur die Namenskonvention. Storezugriff, Slotwahl,
Rotation, Referenzschutz, Manifeste, Roots und Commitlogik folgen nicht in
Issue #55.

Der ProgramCatalog-Encoder berechnet die exakte kanonische Payloadgroesse vor
der Writer-Konstruktion mit ueberlaufsicheren `checkedAddSize`-Schritten fuer
dieselben Felder und Schema-4-/Schema-5-Zweige wie der Encoder. Der
`ByteWriter` reserviert nur diese berechnete Groesse; nach dem Schreiben muss
seine tatsaechliche Groesse exakt damit uebereinstimmen. Eine Abweichung oder
eine Groesse ueber 32.768 Byte wird vor der Veroeffentlichung abgelehnt und
laesst `out` byteidentisch unveraendert.

Dies ist keine absolute Ein-Puffer-Garantie: Das vollstaendige
`ProgramCatalog`-Modell bleibt waehrend der Kodierung vorhanden. Der Encoder
legt hoechstens einen zusaetzlichen, exakt dimensionierten neuen
Payloadpuffer an. Enthaelt `out` bereits einen alten Puffer, besteht auch
dieser bis zum abschliessenden erfolgreichen `swap()` parallel. Beim
Dekodieren bestehen Eingabepayload und der schrittweise aufgebaute
ProgramCatalog-Kandidat nebeneinander; eine Copy-Migration haelt entsprechend
Quellmodell und Migrationskandidat. Die nativen Allokationstests erkennen
Regressionen dieser relativen Vertraege, belegen aber keine reale
ESP32-Heapreserve.

## Revisionsplaetze, kanonische Roots und Referenzschutz

### Physische Slots

Jeder Konfigurationsdokumenttyp besitzt genau vier physische Slots:

- `UserConfiguration`: 4
- `ServiceConfiguration`: 4
- `ProgramCatalog`: 4

Eine Referenz enthaelt mindestens Dokumenttyp, Slot-ID, Revision,
Schema-Version, erwartete Payloadlaenge und erwarteten CRC. Alle Werte muessen
mit dem gelesenen Envelope uebereinstimmen.

Es existieren ausserdem:

- 3 `ConfigurationManifest`-Slots
- 2 `ConfigurationRootRecord`-Slots

### Technisch gueltige Kandidaten und kanonischer Root

`device_platform` prueft RootRecords nur technisch und liefert Kandidaten nach
absteigender Sequenz. `fermentation_app` validiert fuer jeden Kandidaten den
vollstaendigen fachlichen Graphen.

Beim Boot gilt:

1. Rootkandidaten nach Sequenz absteigend untersuchen
2. innerhalb eines Roots zuerst dessen Active-Zweig validieren
3. bei ungueltigem Active dessen Fallback-Zweig validieren
4. den ersten vollstaendig nutzbaren Zweig als kanonischen aktiven Graphen
   waehlen
5. einen verwendeten Fallback stabil diagnostizieren

Ein vorhandenes Manifest oder ein hoher Sequenzwert allein aktiviert niemals
einen Record.

### Schutzwurzeln

Physisch vorhandene, aeltere Rootkopien sind keine zusaetzlichen dauerhaft
schuetzenden fachlichen Wurzeln. Nach erfolgreichem Commit und vollstaendiger
Validierung eines neueren kanonischen Roots bestimmt nur die folgende Menge den
Referenzschutz:

- Active und Fallback des aktuell kanonisch ausgewaehlten ConfigurationRoot
- die gerade ausgefuehrte serialisierte Mutation bis zu ihrem Abschluss

Eine aeltere redundante Rootkopie bleibt ein moeglicher technischer
Bootkandidat, schuetzt aber keine Generation, die ausschliesslich noch von ihr
referenziert wird. Wird ein durch die aktuelle Schutzmenge nicht mehr benoetigter
Slot wiederverwendet, darf diese aeltere Rootkopie dadurch fachlich ungueltig
werden.

Damit bleibt genau eine nachweislich gueltige Rueckfallgeneration geschuetzt,
ohne dass alte redundante Roots nach wenigen Commits alle Manifest- und
Dokumentslots blockieren.

Nur ein Slot ausserhalb dieser kanonischen Schutzmenge darf ueberschrieben
werden. Bereinigung verwendet Referenzanalyse statt Referenzzaehlern.
Unreferenzierte oder nur von nicht mehr kanonisch schuetzenden Altroots
referenzierte Revisionen sind logisch wiederverwendbar und muessen nicht
physisch geloescht werden.

Fehlt ein sicherer Slot, wird mit `NoUnreferencedSlotAvailable` und betroffenem
Dokumenttyp abgelehnt.

### Wiederholte Active-Commits

Ausgehend von einem kanonischen Root mit Active A und Fallback F:

1. einen Manifestplatz waehlen, der nicht durch die kanonische Schutzmenge
   belegt ist
2. neues Manifest N schreiben und vollstaendig validieren
3. den nicht kanonischen Rootslot mit Active N und Fallback A schreiben
4. den neuen Root und seinen gesamten Graphen ruecklesen und validieren
5. erst danach wird dieser Root kanonisch
6. nur noch durch den alten Root benoetigte Generationen fallen aus der
   Schutzmenge und duerfen spaeter wiederverwendet werden

Verbindliche Tests fuehren mindestens fuenf aufeinanderfolgende Active-Commits
durch und beweisen, dass die Slotrotation nicht vorzeitig blockiert.

### Nicht aufloesbares Root-Commit-Ergebnis

Liefert der Root-Write `CommitOutcomeUnknown`, muss die laufende Mutation beide
Rootslots und alle fuer den erwarteten Zielgraphen benoetigten Records
vollstaendig ruecklesen sowie technisch und fachlich validieren. Ergibt dieser
Scan eindeutig den alten oder den neuen kanonischen Graphen, wird genau dieser
Ausgang verwendet.

Die Aufloesung bestimmt zuerst den exakten Ausgang des Zielrootwrites. Liegen
die erwarteten neuen kanonischen Rootbytes persistent vor, darf der Vorgang nie
als alter Ausgang aufgeloest werden. Ist der neue Zielgraph danach noch nicht
vollstaendig lesbar oder bestaetigbar, bleibt der Zustand unbestimmt
beziehungsweise wechselt bei einem eindeutigen nachgelagerten Vertragsfehler in
`ConfigurationRuntimeFailure`. `ResolutionRecoveredOld` ist nur erlaubt, wenn
der Zielroot nachweislich nicht wirksam wurde und der alte kanonische Graph
erneut exakt bestaetigt ist. Ein fremder dritter Root ist weder alt noch neu.

Kann der Scan wegen `ReadError`, `CapacityError`, Integritaetsfehler,
semantischem Graphfehler oder eines vergleichbaren Storefehlers nicht
vollstaendig und eindeutig abgeschlossen werden, entsteht ein stabil
typisierter unbestimmter Commitzustand
`ConfigurationCommitIndeterminate`.

In diesem Zustand gilt:

- weder alter noch neuer persistenter Graph wird als sicher kanonisch
  behauptet;
- der vorbereitete neue `RuntimeConfigurationSnapshot` wird nicht publiziert;
- keine normale Konfigurationsruntime wird weiter freigegeben;
- weitere Konfigurationsmutationen und jede Slotwiederverwendung sind gesperrt;
- der Zustand wird an das `CONFIGURATION_SAFETY_INTEGRATION_GATE` gemeldet;
- es gibt weder automatischen Rollback noch Factory-Fallback;
- ein unveraenderlicher aktiver Laufschnappschuss wird nicht still
  umgeschrieben; seine systemweite Safetywirkung bleibt #24.

Aufloesung ist nur durch einen spaeteren vollstaendig erfolgreichen Scan beider
Rootslots und aller benoetigten Graphrecords oder durch einen Neustart erlaubt,
bei dem der normale Boot-/Recoverypfad denselben Scan erfolgreich abschliesst.
Bleibt das Ergebnis unklar, bleibt der Dienst fail closed und ohne normale
Runtimefreigabe.

### Keine persistente Voraktivierung in Release 1

Release 1 besitzt keinen persistenten Pending-Zweig, keinen `PendingRoot`, kein
Aktivierungsintent und keinen vorbereiteten parallelen Active-Zweig. Ein
Kandidat bleibt bis zum Commit ausschliesslich fluechtig. Neustartpflichtige
Aenderungen werden fuer Release 1 nicht als persistenter Zwischenzustand
gespeichert; ein spaeterer Bedarf benoetigt einen neuen Ownerentscheid und eine
additive, rueckwaertskompatible Erweiterung des Vertrags.

Display, Web und Import koennen Aktivierungswirkung nicht frei setzen. Sie
reichen typisierte Eingaben an denselben Konfigurationsdienst; dieser erzeugt
Vorschau, Validierung, vorbereiteten Runtime-Snapshot und den atomaren Commit.

## Preview und Konflikterkennung

### Zentrales Preview

Ein `ConfigurationPreview` wird ausschliesslich durch den fachlichen
Konfigurationsdienst erzeugt. Es enthaelt:

- einen unveraenderlichen, vollstaendig typisierten Kandidaten
- die erwartete aktive Basisrevision und Manifestgeneration
- eine Integritaetskennung des unveraenderlichen Kandidaten
- berechnete Aktivierungswirkung
- typisierte Aenderungszusammenfassung

Das Preview ist rein fluechtig und wird weder in einen Record noch in einen
Root geschrieben. Oberflaechen erhalten eine redigierte Darstellung und eine
fluechtige Referenz auf genau diesen Kandidaten. Der Commit nimmt keinen vom
Client neu aufgebauten Kandidaten entgegen. Nach Neustart existiert keine
Vorschau mehr.

Unmittelbar vor dem Commit werden erwartete Basis, Kandidatenintegritaet,
vollstaendige technische und fachliche Validierung sowie die
Aktivierungswirkung erneut geprueft. Eine Abweichung fuehrt zu einem typisierten
Konflikt und zu einer neuen Vorschau, nie zu automatischem Merge oder stiller
Umdeutung.

### Kapazitaet und Lebenszyklus

Release 1 erlaubt genau eine sichtbare Vorschau, genau eine volle
Previewerstellung, hoechstens zwei unterschiedliche vollstaendige
Konfigurationsmodellgenerationen und hoechstens acht registrierte
`RuntimeConfigurationReadLease`-Objekte. Eine belegte Modellposition fuehrt
vor tiefer Kopie und vor jedem Write zu `ConfigurationModelBudgetBusy`.
Diese Softwaregrenzen sind keine ungepruefte reale Heapgarantie. Eine begonnene
Commitentscheidung wird ueber die gemeinsame `ConfigurationMutationLease`
gegen parallele Konfigurationsmutationen serialisiert.

Previewaufbau, erfasste Commitvorschau, aktive und ausgemusterte
Runtimegenerationen besitzen dazu eindeutige fluechtige Reservierungen. Das
Entfernen einer sichtbaren Vorschau gibt eine bereits vom Commit erfasste
Modellposition nicht frei. Eine ausgemusterte Runtimegeneration belegt ihre
Position bis zur letzten tatsaechlich zerstoerten Read-Lease und bis alle
lokalen Publish-/Recoverybesitzer freigegeben sind. Veraltete Build-Leases
koennen keine neuere Reservierung entfernen. Der Dienst nimmt erst danach neue
Vollmodelle an.

Secret-Werte erscheinen nie in Preview-Antworten, oeffentlichen Fingerprints,
Zusammenfassungen, Logs, Diagnosen oder Exporten.

### Globale Mutation und Konflikte

In der Konfigurationspersistenz laeuft hoechstens eine persistente Mutation
gleichzeitig. Dies umfasst normale Konfiguration, Migration, Bootstrap und
Werksreset. Lesen und Preview-Berechnung duerfen parallel erfolgen.

Nach Belegung werden erneut geprueft:

- fluechtige Vorschauidentitaet und unveraenderlicher Kandidat
- StorageEpoch und Active-Generation
- Kandidatenintegritaet
- alle fachlichen und technischen Voraussetzungen

Abweichungen werden typisiert als Konflikt abgelehnt; es gibt kein
automatisches Merge.

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
werden als `Unknown` mit roher Wire-ID erhalten.

Ein Commit liefert genau eine typisierte Kategorie:

- `ConfigurationCommitSuccess`
- `ConfigurationValidationFailure`
- `PersistenceFailure`
- `ConfigurationConflictFailure`
- `ActivationFailure`
- `MigrationFailure`

Erfolg unterscheidet mindestens `NoChange` und `Activated`.

## Laufzeitaktivierung und Grenze zu Issue #24

Vor dem persistenten Root-Commit werden der vollstaendige Kandidat erneut
validiert, Plattformwerte wie die Zeitzone aufgeloest und alle falliblen
Ressourcen vorbereitet. Das Ergebnis ist ein unveraenderlicher
`RuntimeConfigurationSnapshot` ohne sichtbare Wirkung.

Unmittelbar vor dem Rootwrite liest der Dienst das vorbereitete Manifest aus
dem Store zurueck und prueft den gesamten Zielgraphen nochmals. Dies umfasst
alle geaenderten und unveraenderten Referenzen, Envelope, Schema, Epoche,
Laenge, CRC, fachliche Dekodierung sowie die exakte Bindung an den vorbereiteten
typisierten Runtime-Snapshot. Jede Abweichung oder jeder Read-/Capacity-Fehler
bricht ohne Rootwrite ab. Die Aenderungsmaske wird aus exakten kanonischen
Inhaltsvergleichen abgeleitet und nie vom Aufrufer als Persistenzwahrheit
uebernommen.

Der erfolgreiche dauerhafte Write eines neuen gueltigen
`ConfigurationRootRecord` mit hoeherer rootSequence ist der einzige persistente
Linearisierungspunkt. Danach wird innerhalb derselben exklusiven Mutation der
vorbereitete Snapshot nicht allokierend, nicht serialisierend und vertraglich
nicht fehlschlagend atomar sichtbar. Leser sehen nur die vollstaendig alte oder
neue Generation.

Ein eindeutig festgestellter Fehler vor Root-Commit laesst alte Konfiguration
und Snapshot unveraendert. Ein nicht aufloesbares
`CommitOutcomeUnknown` folgt dagegen dem stabilen fail-closed Zustand
`ConfigurationCommitIndeterminate`; es darf nicht als Fehler vor dem Commit
umgedeutet werden.
Eine unerwartete Publish-Vertragsverletzung nach Root-Commit verursacht keinen
automatischen Rollback. Der #16-Vertrag sieht dafuer nur einen typisierten
`ConfigurationRuntimeFailure` vor; die Umsetzung in #56 verhindert weitere
normale Konfigurationsfreigabe.

Die vollstaendige Fehlerklassifizierung, persistente Verriegelung,
`SAFE_BOOT`-Politik und reale Aktorsperre gehoeren zu Issue #24. Issue #16 darf
diese Logik weder vorwegnehmen noch eine konkrete Aktorsteuerung implementieren.
Bis zur Integration mit #24 bleibt die Konfigurationslaufzeit in diesem
Vertragsverletzungsfall fachlich nicht betriebsbereit.

Stromausfall vor Root-Commit laedt den alten Graphen; nach dem dauerhaft
erfolgreichen Root-Commit den neuen. Es existiert kein nachgelagerter
persistenter Aktivierungsintent.

## Nachgelagertes CONFIGURATION_SAFETY_INTEGRATION_GATE

Zwischen #24 und #56/#57 entsteht keine zyklische oder pauschale
Implementierungsabhaengigkeit. Stattdessen ist vor Abschluss des integrierten
Safetykerns das verbindliche `CONFIGURATION_SAFETY_INTEGRATION_GATE` zu
erfuellen:

- #56 produziert `ConfigurationRuntimeFailure`, den fail-closed Zustand des
  Konfigurationsdienstes und den unbestimmten Commitzustand;
- #57 produziert `ConfigurationUnavailable` und
  `ConfigurationIntegrityFailure` und gibt in diesen Faellen keine Runtime
  frei;
- #24 konsumiert diese realen Eingaben und bildet sie auf persistente
  Verriegelung, sichere Bootprioritaet beziehungsweise `SAFE_BOOT`, keine
  normale Aktorfreigabe und reproduzierbare Fehlerinjektion ab.

#56 und #57 duerfen ihre typisierten Producer-Vertraege unabhaengig umsetzen
und abschliessen; #24 darf seinen Fehlerkern parallel vorbereiten. #24 darf
aber nicht als vollstaendig abgeschlossen gelten, bevor die realen Producer
integriert und getestet sind. Wird der #24-Core zuerst gemergt, bleibt #24 bis
zur Gate-Abnahme offen. Reale Aktoradapter und produktive Aktorfreigabe duerfen
das Gate nicht umgehen.

Die Gate-Tests umfassen mindestens `ConfigurationRuntimeFailure`,
`ConfigurationUnavailable`, `ConfigurationIntegrityFailure`, den unbestimmten
Commitzustand, Neustart mit erhaltener notwendiger Verriegelung, keine
Aktorfreigabe bei unbekanntem oder unaufgeloestem Konfigurationszustand sowie
Recovery ausschliesslich nach dem #24-Fehlerresetvertrag.

## Spaetere persistente Connectivity- und Authentication-Domaenen

Konfigurationsdokumente enthalten weder Geheimnisse noch Passwort- oder PIN-
Pruefnachweise. #16, #56 und #57 erzeugen keine leeren Connectivity- oder
Authentication-Manifeste, -Roots, -Slots oder Reservepayloads.

Eine reale persistente Connectivity- beziehungsweise Authentication-Domaene
entsteht erst mit ihrem ersten produktiven Konsumenten. Sie muss dann eigene
stark typisierte, versionierte und an die aktuelle `StorageEpoch` gebundene
Records sowie ihre fachlich notwendige Commit-, Widerrufs- und
Recoverysemantik erhalten. Der spaetere Detailplan darf diese Semantik nicht
stillschweigend in `UserConfiguration`, `ProgramCatalog` oder freie Key/Value-
Strukturen verschieben.

Normale Backups enthalten weder wiederverwendbare Secrets noch
Pruefnachweise. Das konkrete portable Format folgt mit Issue #19.

## Bootstrap, StorageEpoch und Werksreset

### BootstrapRecord

Es existieren zwei redundante `ConfigurationBootstrapRecord`-Slots. Ein Record
enthaelt mindestens BootstrapSequence, Speicherformatversion, StorageEpoch und
einen Zustand:

- `Initializing`
- `Initialized`
- `Resetting`

`NotFound` ist kein gespeicherter Zustand. Lesefehler werden nie wie NotFound
behandelt.

Die Umsetzung in #57 verwendet Record-Type-ID 6, die beiden Keys `cb0` und
`cb1`, Schema 1 und eine exakt fuenf Byte grosse Payload. Die Payload enthaelt
die 32-Bit-Speicherformatversion und den Zustands-Wirewert; `StorageEpoch` und
`BootstrapSequence` liegen in den vorhandenen Envelopefeldern. Fuer Schema 1
gilt die geschlossene Historienrelation:

- `Initializing`: ausschliesslich Epoche 1, Sequence 1;
- `Initialized`: `Sequence = 2 * StorageEpoch`;
- `Resetting`: ab Epoche 2, `Sequence = 2 * StorageEpoch - 1`.

Gleiche Identitaet mit verschiedenen kanonischen Bytes, unzulaessige Spruenge
und unbekannte neuere Schemas bleiben fail closed. Ein byteidentisches
Duplikat ist diagnostizierbar, aber keine neue Identitaet.

Automatische Initialisierung ist nur erlaubt, wenn alle erforderlichen Reads
erfolgreich waren, weder gueltige noch beschaedigt vorhandene Roots erkannt
wurden und kein BootstrapRecord existiert. Vor der ersten Dokumentrevision wird
`Initializing` persistiert.

Das Factory-Neuheitsorakel liest unter derselben
`ConfigurationMutationLease` jeden der 19 bekannten Bootstrap-, Root-,
Manifest- und Dokumentkeys genau einmal. Die beiden bereits gebundenen
Bootstrapbeobachtungen werden nicht erneut gelesen. Erst der exakt
bestaetigte `Initializing`- beziehungsweise `Resetting`-Record erzeugt die
nicht kopierbare, an Epoche, Plan und Lease gebundene Graphwrite-Capability.
Ein vorbereiteter Runtime-Snapshot ist vor dem bestaetigten und vollstaendig
validierten Root nicht publish-faehig; normale Runtimeakquisition bleibt bis
zum bestaetigten `Initialized` gesperrt.

Danach entstehen unter StorageEpoch 1:

- UserConfiguration Revision 1: `de`, `Europe/Zurich`,
  `Fermentationsschrank`
- leere ServiceConfiguration Revision 1
- ProgramCatalog Revision 1 mit vier Factory-Arbeitskopien
- ConfigurationManifest Generation 1
- ConfigurationRootRecord rootSequence 1 ohne Fallback

Erst nach Schreiben, Ruecklesen und Validieren des gesamten Graphen wird
Bootstrap auf `Initialized` fortgeschrieben.

### Beschaedigte Daten

Vorhandene ungueltige, beschaedigte oder unlesbare Daten sind nie fabrikneuer
Speicher. Gibt es weder Active noch Fallback, wird keine Factory-Konfiguration
erzeugt und nichts still geloescht.

Issue #16 liefert in diesem Fall einen typisierten
`ConfigurationUnavailable`- beziehungsweise `ConfigurationIntegrityFailure`-
Zustand und gibt keine RuntimeConfiguration frei. Die Einordnung in die
systemweite Fehlerklasse und `SAFE_BOOT` erfolgt spaeter durch Issue #24.

Firmwareupdates ueberschreiben keine Dokumente oder Standard-Arbeitskopien.
Unbekannte oder nicht migrierbare Schemas loesen keine Factory-Neuanlage aus.

### StorageEpoch

Die gesamte Konfigurationspersistenz ist an die aktuelle `StorageEpoch`
gebunden. Eine Referenz ist nur gueltig, wenn alle beteiligten Objekte dieselbe
aktuelle Epoche besitzen. Spaetere reale Connectivity- und
Authentication-Domaenen muessen sich ebenfalls an diese Epoche binden, werden
aber nicht von #57 vorbereitet.

### Werksreset

Ein bestaetigter Vollreset ist mit `BootstrapState::Resetting`
wiederaufnehmbar:

1. StorageEpoch ohne stillen Ueberlauf erhoehen
2. Resetting unter neuer Epoche persistieren
3. Referenzen alter Epochen logisch ungueltig machen
4. neue Initialdokumente, Manifest und Root unter der neuen Epoche schreiben,
   ruecklesen und vollstaendig validieren
5. den neuen Konfigurationsgraphen aktivieren
6. Bootstrap als `Initialized` abschliessen

Der Reset invalidiert Active und Fallback der alten Epoche. Spaetere reale
Connectivity- und Authentication-Domaenen muessen ihre eigenen
epochengebundenen Resetregeln ergaenzen; #57 erfindet sie nicht vorab. Eine
sichere physische Loeschung alter Flashbytes wird ohne Plattformnachweis nicht
zugesichert. Die geraetespezifische Touchkalibrierung bleibt erhalten.
Bedienablauf, physische Geste sowie Lauf-, Journal- und Historiendaten bleiben
bei ihren zustaendigen Issues. Korruption loest niemals automatisch einen
Werksreset aus.

Ein ausdruecklich fachlich autorisierter Werksreset darf auch ohne geladene
Runtime beginnen, wenn ein kanonischer, unterstuetzter `Initialized`-
Bootstrap die aktuelle Epoche und den vollstaendigen Bootstrap-High-Water
eindeutig bindet. Ein fehlender oder unbrauchbarer Konfigurationsgraph ist in
diesem Fall kein Resetverbot. Bootstrapkorruption, ein unbekanntes neueres
Bootstrap- oder Speicherformat sowie Read-/Capacity-Fehler der Bootstrapslots
blockieren dagegen jeden Resetwrite. `Initializing` und `Resetting` werden
wiederaufgenommen und starten keinen zweiten Reset.

Vor dem bestaetigten `Resetting` wird kein Zielepochenrecord geschrieben.
Danach werden exakte Records der Zielepoche idempotent wiederverwendet;
`NotFound` und eindeutig andere Epochen duerfen als Zielslot dienen. Korrupte,
unlesbare, kollidierende oder einer unbekannten Epoche zugehoerige Slots sind
weder exakte Zielrecords noch sicher frei. Reicht die sichere Slotmenge nicht,
bleibt der Reset fail closed. Alte Bytes duerfen physisch bestehen bleiben,
werden aber weder referenziert noch als erfolgreiche Loeschung ausgegeben.

Unbestimmte Writeausgaenge bleiben fachlich getrennt:

- Ein unbestimmter Bootstraprecord betrifft nur `Initializing`, `Resetting`
  oder `Initialized` und wird durch exakten Readback dieses Bootstraprecords
  aufgeloest.
- Ein unbestimmter Dokument- oder Manifestwrite liegt vor dem Root-
  Linearisierungspunkt. Der Readback akzeptiert nur den vollstaendig gegen das
  gebundene Modell validierten neuen Record. Ein nach Portvertrag sicher nicht
  wirksamer Write beendet den Versuch ohne weiteren Write; fremde oder nicht
  abschliessend pruefbare Werte bleiben fail closed.
- Ein unbestimmter Rootwrite behaelt unveraendert den aus #56 stammenden
  `ConfigurationCommitIndeterminate`- und `resolveCommitDetailed()`-Vertrag.
  In Initialisierungs- und Resetkontexten fuehrt ein bestaetigter neuer Root
  erst zur internen Aktivierung und zu `BootstrapFinalizationPending`, nicht
  direkt zu `Operational`. Normale Runtime bleibt bis zum bestaetigten
  `Initialized` gesperrt.

Ein erfolgreicher lokaler Reset loescht weder einen bereits erzeugten
Konfigurationsfehler noch eine spaetere Verriegelung aus #24. Deren Aufhebung
bleibt Teil des `CONFIGURATION_SAFETY_INTEGRATION_GATE` und des dortigen
Fehlerresetvertrags.

## Speicherport und Modulgrenzen

Das produktive Backend ist gemaess ADR-016 die ESP32-Speicherschicht NVS;
Werte werden als Blob gespeichert und bleiben binaersicher. Der
Schluessel/Wert-Port verwendet feste deterministische Schluessel: 1 bis 15
Bytes aus `[A-Za-z0-9_.-]`, portseitig erzwungen (kleinster gemeinsamer Nenner
plausibler Backends, auch fuer dateibasierte Backends gueltig und in Logs
lesbar). Listing, Verzeichnisse, Rename und Mehrschluesseltransaktionen sind
nicht erforderlich.

Der Port garantiert je Schluessel binaersicheres, dauerhaftes und
stromausfallsicheres Replace: Nach Unterbrechung existiert der vollstaendige
alte oder neue Wert; ein abgeschnittener oder gemischter Wert ist kein
erfolgreicher Write; ein erfolgreich zurueckgekehrter Write ist dauerhaft.

`device_platform` enthaelt ausschliesslich anwendungsneutrale Bausteine:

- `IStateStore`, `ISecureRandomSource`, `ITimeSource`
- begrenzte Byte-Reader und -Writer, Big-Endian-Primitiven und CRC
- generischen Envelope, feste Revisionsslots und redundante Recordslots
- technische Kandidatenpruefung und -sortierung
- starke Typen fuer StorageEpoch, Revision, Generation, RecordSequence, SlotId
  und RecordTypeId
- technische Speicher-, Envelope-, Integritaets- und Sequenzfehler

Die Plattform erhaelt Schluessel, Slotzahl, Record-Type und Schutzmengen von der
Anwendung. Sie kennt keine konkreten Dokumente, Manifestbedeutungen,
Factorywerte, Programme, Vorschau, Lauf oder Aktivierungswirkung.

Ein einzelner technischer Kandidatenscan verarbeitet hoechstens acht Slots
derselben Recordgruppe. Die aktuell spezifizierten Gruppen benoetigen
hoechstens vier Slots; die verdoppelte technische Obergrenze laesst eine kleine
Reserve fuer dieselbe Plattformabstraktion, ohne eine unbeschraenkte oder
spekulative Slotplattform zu schaffen.

`fermentation_app` besitzt konkrete Dokumente, Schemas, IDs, Schluessel,
Manifest-, Root- und Bootstrapbedeutung, Graphvalidierung, ProgramCatalog,
fluechtige Vorschau, Migration, Boot/Recovery,
`RuntimeConfigurationSnapshot` und den typisierten
`ConfigurationRuntimeFailure` fuer die spaetere Integration in #24. #56 und
#57 fuehren keinen Port zu #17 und keine vorbereitete Secret-Domaene ein.

`device_platform_test_support` enthaelt nur anwendungsneutrale Testadapter wie
`SimulatedPersistentStateStore`, kontrollierbare Zufallsquelle, Cut-Point- und
Korruptionsinjektion. Fachliche Orakel bleiben in den Anwendungstests. Kein
Produktionsmodul oder ESP32-Profil darf Test-Support referenzieren.

`src/main.cpp` bleibt reine Composition Root.

## Ressourcenvertrag

Die Umsetzung erzwingt zentrale Softwareobergrenzen fuer fluechtige Vorschau,
Recordpuffer, typisierten ProgramCatalog und alle Payloads: eine sichtbare
Vorschau, eine volle Previewerstellung, zwei Modellgenerationen, acht
Read-Leases und eine persistente Konfigurationsmutation. Daraus folgt weder
eine ungepruefte reale Hardwaregarantie noch eine PSRAM-Verfuegbarkeit.

Ein Preview wird erst nach erfolgreicher Ressourcenbereitstellung sichtbar.
Vor Root-Commit bricht jeder Ressourcenfehler typisiert ohne Teilaktivierung ab.
Nach Root-Commit allokiert, serialisiert und reserviert Publish nichts.
Waehrend eines vollstaendigen Commits existiert global hoechstens ein
vollstaendiger kodierter Recordpuffer. Dies ist ein Vertrag auf Ebene des
gesamten Commit-Workflows (#56/#57): er verlangt zusaetzlich, dass der
Commit-Workflow selbst keinen alten Ausgabepuffer parallel zum neu kodierten
haelt. Die Envelope-Codec-Fundamentschicht (#54, `encodeEnvelope()`) liefert
dafuer die notwendige, aber fuer sich allein schwaechere Voraussetzung:
hoechstens ein zusaetzlicher, neu aufgebauter vollstaendiger Recordpuffer pro
Encode-Aufruf, veroeffentlicht per `swap()` ohne Vollkopie - siehe die
Praezisierung unten im Abschnitt zu Issue #54.

Der technische Slot-Scan waechst nicht mit `Slotzahl * Payloadgroesse`, weil er
keine Payload im Ergebnis haelt und waehrend der Iteration nur einen
Recordpuffer liest. Seine Kandidaten- und Diagnosemetadaten wachsen weiterhin
mit der tatsaechlich gescannten Slotzahl, sind aber durch die verbindliche
Obergrenze von acht Slots endlich begrenzt. Das ist keine absolute
Unabhaengigkeit von der Slotzahl.

Der kanonische Loader haelt nur den aktiven Graphen als vollstaendiges
typisiertes Laufzeitmodell. Einen gueltigen Fallback prueft er recordweise und
behaelt danach nur Manifest- und Referenzmetadaten. Muss der Fallback einen
ungueltigen Active-Zweig ersetzen, wird er direkt zum aktiven Modell; ein
zweites dauerhaft gehaltenes Fallback-`ProgramCatalog`-Modell entsteht nicht.
`ReadError` oder `CapacityError` eines benoetigten Root- oder Graphrecords
brechen die Bestimmung fail closed ab und erlauben nicht das Ausweichen auf
einen aelteren scheinbar gueltigen Graphen.

Auch die schreibende `ValidationOnly`-Pruefung wiederholt die globale
Rootordnung und Active-/Fallback-Auswahl, bindet jeden zweiten Rootread an die
zuvor gescannten Metadaten und validiert beide benoetigten Zweige technisch,
referenziell und fachlich. Ein neuerer fremder Root, ein veraenderter
Fallbackschutz oder ein nicht lesbarer benoetigter Record blockiert vor jedem
Write. Diese Pruefung verwendet begrenzte Recordpuffer und baut kein drittes
Vollmodell auf.

Beide ESP32-Produktionsprofile muessen je Teilissue bauen. Base-SHA und PR-Head
werden mit identischer Toolchain fuer statisches RAM, Flash, `firmware.bin` und
`firmware.elf` verglichen. Werte bleiben informativ, bis reale Budgets
bestaetigt sind.

Ohne reale Messung werden kein freier Heap, groesster Block, Parallelreserve,
reale Flash-Replace-Atomizitaet oder Flashlebensdauer zugesichert.

## Verbindliche Tests

### Simulierter Persistenzspeicher

`device_platform_test_support` stellt einen binaersicheren
`SimulatedPersistentStateStore` bereit. Er trennt committed Daten, aktuelle
Schreiboperation und fluechtigen Zustand. Pro Write sind Fehler vor Beginn,
Stromausfall vor Commit, Stromausfall nach Commit vor Rueckkehr, Erfolg sowie
gezielte Read-, NotFound- und Korruptionsinjektionen simulierbar.

Nach Neustart bleiben nur committed Daten. Abgeschnittene oder veraenderte
Werte sind nachtraegliche Korruptionsinjektion, nicht erlaubtes Ergebnis eines
erfolgreichen atomaren Writes.

### Cut-Point-Matrix

Jeder mehrstufige Workflow wird erfolgreich zur Aufzeichnung aller Writes
ausgefuehrt und dann von identischem Ausgangszustand mit Stromausfall unmittelbar
vor und nach jeder Commit-Grenze wiederholt. Dienste und Runtime werden jeweils
neu aufgebaut; Zeit, Zufall und externe Quellen sind kontrollierbar.

Die vollstaendige Matrix umfasst nach Abschluss aller Teilissues mindestens:

- Bootstrap
- mindestens fuenf aufeinanderfolgende Active-Commits
- fluechtige Vorschau erstellen, verwerfen, bestaetigen und bei geaenderter
  Basis als Konflikt ablehnen
- Manifest- und Root-Slotrotation bis ueber die erste Wiederverwendung hinaus
- technische und fachliche Graphkorruption an Active und Fallback
- Fehler bei Dokument-, Manifest- und Root-Write sowie Readback
- `CommitOutcomeUnknown` mit eindeutig altem beziehungsweise neuem Readback
- `CommitOutcomeUnknown` mit `ReadError`, `CapacityError`, CRC- oder
  Semantikfehler beim Aufloesungsscan
- exakter neuer Zielroot mit voruebergehend nicht lesbarem oder ungueltigem
  Zielgraphen darf nie als alter Ausgang gelten
- wiederholter erfolgloser Aufloesungsscan und Neustart mit eindeutigem
  beziehungsweise weiterhin unklarem Ergebnis
- im unbestimmten Commitzustand kein Publish, keine weitere Mutation und keine
  Slotrotation
- neuerer kanonischer Root, veraenderte Rootmetadaten, Fallbackschutz und
  fachlich ungueltige CRC-korrekte Payload vor dem ersten Write
- vollstaendige Zielgraphpruefung nach Manifestwrite und vor Rootwrite fuer
  jedes geaenderte und unveraenderte Dokument
- falsche positive und negative Aenderungsbits mit identischem Reload-Orakel
- erfasste, ersetzte und parallele Vorschauen sowie ein bis acht gehaltene alte
  Runtime-Leases ohne Ueberschreitung der Zwei-Modell-Grenze
- Runtimevorbereitung vor dem Root-Commit
- Neustart unmittelbar vor und nach dem Root-Commit
- unerwartete Verletzung des nicht fehlschlagenden Publish-Vertrags
- Schema-Copy-Migration ueber denselben Active-/Fallback-Commitpfad
- Werksreset

Jeder Cut besitzt ein konkretes Recovery-Orakel; ein allgemeiner Fehlerzustand
ist kein pauschal erlaubtes Ergebnis.

Immer gelten:

- keine gemischten Dokumentgenerationen
- keine Aktivierung allein wegen hoher Generation
- keine persistente oder nach Neustart wiederauflebende Vorschau
- kein Pending, Aktivierungsintent oder vorbereiteter paralleler Active-Zweig
- keine Behandlung beschaedigter Daten als leer
- keine Konfigurationsfreigabe bei unklarem Zustand
- Uebergabe aller typisierten Konfigurationsfehler an das
  `CONFIGURATION_SAFETY_INTEGRATION_GATE`
- Touchkalibrierung bleibt beim normalen Werksreset erhalten

Korruptionstests unterscheiden rohe Byteaenderung ohne CRC-Anpassung und
semantisch ungueltige, CRC-korrekt neu kodierte Datensaetze.

Golden-Vektoren pruefen mindestens CRC-Checkwert, Envelope mit und ohne UTC,
Big-Endian-Integer, Revisionen, Bool, Optional, signierte Integer, binary64,
`-0.0`, UTF-8-Grenzen und bekannte beziehungsweise unbekannte
Aenderungsmetadaten.

## Verbindliche Aufteilung der Umsetzung

Vor Beginn der Implementierung werden fuer die folgenden Pakete eigene,
abhaengige GitHub-Issues, Agent-Auftraege, Branches und kleine PRs angelegt:

### Paket A: Plattformpersistenz und Wireformat

- begrenztes binaersicheres `IStateStore`
- starke technische Typen
- Big-Endian-Codecs, CRC und Envelope
- feste generische Slot-/Recordmechanik
- sicherer Zufallsport (Zeitzonenport folgt spaeter mit seinem Aufrufer)
- SimulatedPersistentStateStore und Golden Tests

Umgesetzt mit Issue #54 (`lib/device_platform/src/storage_types.hpp`,
`byte_buffer.hpp`, `big_endian_codec.hpp`, `binary64_codec.hpp`,
`checked_size.hpp`, `crc32.hpp`/`.cpp`, `storage_envelope.hpp`/`.cpp`,
`storage_slot_candidates.hpp`/`.cpp`, `storage_slot_limits.hpp`,
`secure_random_source.hpp`,
`state_store_key.hpp`, das erweiterte `state_store.hpp`; Testadapter in
`lib/device_platform_test_support/src/simulated_persistent_state_store.hpp`/
`.cpp`, `mock_secure_random_source.hpp`/`.cpp`). Bewusst noch ohne fachliche
Slotzahlen, Root-/Manifestbedeutung oder Schutzmengen - das bleibt Paket C
(#56). Die anwendungsneutrale Fundamentschicht begrenzt lediglich jeden
einzelnen technischen Scan auf hoechstens acht Slots.

`SimulatedPersistentStateStore` modelliert die drei geforderten Zustands-
bereiche explizit als getrennte private Datenhaltung: dauerhaft `committed_`,
eine gestagte, aber noch nicht committete Schreiboperation
(`std::optional<PendingWrite> pendingWrite_` mit Schluessel und vollstaendigem
neuem Wert) sowie sonstiger fluechtiger Testzustand (Fault-Schalter, Read-/
NotFound-Injektion). `write()` bildet damit die reale Reihenfolge "vollstaendig
staging, dann atomar committen" nach: bei `FailBeforeBegin` und
`CapacityExceeded` entsteht kein Staging und `committed_` bleibt unberuehrt;
bei `PowerCutBeforeCommit` wird der vollstaendige neue Wert gestagt, aber nie
in `committed_` uebernommen - erst ein simulierter `restart()` verwirft das
Staging, danach ist ausschliesslich der alte committed Wert sichtbar; bei
Erfolg und bei `PowerCutAfterCommitBeforeReturn` wird der gestagte Wert atomar
komplett in `committed_` uebernommen (nie ein Teil- oder Mischwert) und das
Staging sofort geleert. `restart()` loescht `pendingWrite_` sowie alle
fluechtigen Testschalter, laesst `committed_` aber unveraendert. Ein rein
testinterner Zugriff `hasPendingWriteForTesting()` macht das gestagte-aber-
nicht-committete Staging fuer native Tests beobachtbar, ohne die produktive
`IStateStore`-Schnittstelle zu vergroessern. Der ebenfalls rein testinterne
`pendingWriteMatchesForTesting()`-Beleg prueft, dass nicht nur irgendein
Staging existiert, sondern Schluessel und vollstaendiger binaerer Wert exakt
der laufenden Schreiboperation entsprechen.

`IStateStore::write` liefert vier eindeutig unterscheidbare Ergebnisse statt
einer pauschalen "unveraendert bei Fehler"-Garantie: `Success` (neuer Wert
dauerhaft gespeichert), `WriteError` und `CapacityError` (sicher
unveraendert) sowie `CommitOutcomeUnknown` (Commit-Ausgang unbekannt, z. B.
nach einem Stromausfall zwischen Commit und Rueckkehr - der neue Wert kann
bereits dauerhaft gespeichert sein; der Aufrufer muss zuruecklesen). `read()`
und `write()` verwenden bewusst getrennte Statustypen -
`StateStoreReadStatus` (`Success`/`NotFound`/`ReadError`/`CapacityError`) und
`StateStoreWriteStatus` (`Success`/`WriteError`/`CapacityError`/
`CommitOutcomeUnknown`) -, nicht nur eine dokumentierte Teilmenge eines
gemeinsamen Enums: ein Adapter kann `WriteError` oder `CommitOutcomeUnknown`
schon aufgrund des Rueckgabetyps nicht als Leseergebnis liefern, und
umgekehrt. Die technische Slotkandidaten-Ermittlung
(`scanTechnicalSlotCandidates`) verwirft uebersprungene Slots nicht
stillschweigend, sondern liefert zusaetzlich zu den sortierten Kandidaten
eine typisierte `SlotIssue`-Liste (`NotFound`, Lese-/Kapazitaetsfehler, jede
Envelope-Integritaetsverletzung, technisch gueltige aber nicht passende
Kandidaten, sowie `UnexpectedStatus` fuer den an der Aufrufstelle nicht
erreichbaren Success-Fallback der internen Statusmapper) - `ReadError` wird
dabei nie wie `NotFound` behandelt, damit spaeterer Bootstrap-/Recovery-Code
fabrikleeren von beschaedigtem Speicher unterscheiden kann. Der Scan
dekodiert nur die Metadaten (`decodeEnvelopeMetadata`, CRC direkt ueber den
Eingabepuffer ohne die Payload zu materialisieren) und traegt keine Payloads
im Ergebnis: der Speicher waechst nicht mit `Slotzahl * Payloadgroesse`, und
zu keinem Zeitpunkt liegt mehr als ein Recordpuffer im Speicher. Kandidaten-
und Diagnosemetadaten wachsen mit der gescannten Slotzahl, sind durch die
Obergrenze von acht aber endlich begrenzt. Die Payload eines gewaehlten
Kandidaten wird ueber
`loadSlotPayload` geladen, das CRC, Record-Identitaet und `versionValue`
gegen den beim Scan gesehenen Wert vollstaendig neu validiert.
`nextSlotRoundRobin` liefert ein typisiertes `NextSlotResult`
(`NextSlotStatus::Success`/`InvalidSlotCount`/`InvalidLastSlot` mit
`std::optional<SlotId>`): eine ungueltige Slotanzahl (`0` oder technisch
nicht darstellbar `> UINT32_MAX`) sowie ein `lastWrittenSlot >= slotCount`
werden typisiert und unterscheidbar abgelehnt, statt einen scheinbar
gueltigen `SlotId(0)` zu liefern oder einen ausserhalb liegenden Wert still
per Modulo zu normalisieren. Bei gueltigen Eingaben
(`lastWrittenSlot < slotCount`) ueberlaeuft `lastWrittenSlot.value() + 1`
unabhaengig von der `size_t`-Breite der Zielplattform nie.

`state_store_key.hpp` stellt einen gueltig-by-construction begrenzten
`StateStoreKey`-Werttyp bereit: kein oeffentlicher Default-Konstruktor,
`create()` ist der einzige Erzeugungsweg und erzwingt gemaess ADR-016
portseitig 1 bis 15 Bytes aus `[A-Za-z0-9_.-]`; ein leerer (`Empty`), ein zu
langer (`TooLong`) und ein zeichensatzverletzender (`InvalidCharacter`)
Schluessel werden typisiert und unterscheidbar abgelehnt, statt einen
scheinbar gueltigen Schluessel zu liefern. Die Nutzlast bleibt davon
unberuehrt binaersicher; konkrete Schluesselwerte bleiben Aufgabe der
aufrufenden Anwendung (Namenskonvention in #55).

`storage_types.hpp` ergaenzt ausserdem einen generischen
`checkedIncrement`-Baustein fuer die starken `uint64_t`-Zaehlertypen: lehnt
sowohl den reservierten Ausgangswert 0 (`InvalidCurrentValue`) als auch einen
Ueberlauf von `UINT64_MAX` auf 0 (`Overflow`) stabil ab, statt eine der
beiden Situationen still in einen scheinbar gueltigen Wert zu verwandeln; der
erste gueltige Wert 1 wird explizit vom Bootstrap-/Anwendungscode erzeugt,
nicht durch Inkrementieren von 0. Die konkrete Revisions-/Sequenzvergabe
bleibt Paket C (#56).

CRC-32/ISO-HDLC ist als inkrementeller Akkumulator (`Crc32IsoHdlc`) verfuegbar
und wird von `encodeEnvelope()`/`decodeEnvelope()` genutzt, um den CRC direkt
ueber Header und Payload zu berechnen, ohne dafuer einen zusaetzlichen
`header + payload`-Hilfspuffer anzulegen (der kleine, auf die feste
Headergroesse begrenzte `header`-Puffer zaehlt nicht als recordgross).
`encodeEnvelope()` veroeffentlicht den fertigen Record erst nach
vollstaendigem Erfolg per `swap()` (`ByteWriter::takeBytes()` gefolgt von
`outBytes.swap(encoded)`) statt per Kopie: es entsteht hoechstens ein
zusaetzlicher, neu aufgebauter vollstaendiger Recordpuffer.

Das ist bewusst keine absolute Aussage fuer die gesamte Aufrufdauer: haelt
die aufrufende Anwendung in `outBytes` bereits einen alten vollstaendigen
Record, bleibt dieser bis zur erfolgreichen `swap()`-Zeile unveraendert
bestehen - waehrend dieses kurzen Zeitraums existieren alter und neuer
Puffer gleichzeitig. `outBytes` bleibt bei jedem Fehler (`InvalidField`,
`CapacityExceeded`) vollstaendig unveraendert. Die staerkere, absolute
Aussage aus dem Ressourcenvertrag oben ("waehrend eines vollstaendigen
Commits existiert global hoechstens ein vollstaendiger Recordpuffer") ist
damit auf dieser Fundamentschicht vorbereitet, aber nicht bereits
vollstaendig erzwungen - das erfordert zusaetzlich, dass der aufrufende
Commit-Workflow (#56/#57) `encodeEnvelope()` nicht mit einem noch benoetigten
alten Record in `outBytes` aufruft.

`InvalidField` und `CapacityExceeded` sind trennscharf: `InvalidField` gilt
ausschliesslich fuer die vier reservierten Nullwertfelder (Envelope-Version,
Record-Type-ID, Schema-Version, StorageEpoch/VersionValue) und ungueltige
Optionaltags. Jede Groessen- oder Kapazitaetsfrage - einschliesslich einer
Payloadgroesse, die nicht in `uint32_t` darstellbar ist - liefert konsequent
`CapacityExceeded`; `outBytes` bleibt dabei unveraendert. Die reine
Groessenentscheidung ist in der freien, zustandslosen Funktion
`checkEnvelopeEncodedSize(payloadSize, hasUtc, maxTotalBytes)` gekapselt, die
`encodeEnvelope()` intern aufruft und die denselben Status samt
Gesamtgroesse als reine Zahlen liefert - ohne einen Puffer aufzubauen. Damit
lassen sich Grenzwerte bis `UINT32_MAX` nativ testen, ohne eine reale
4-GiB-Payload zu allokieren.

`decodeEnvelope()` validiert die beanspruchte Laenge vollstaendig, bevor die
Payload allokiert wird, und berechnet den CRC ebenfalls inkrementell ueber
den bereits vorhandenen Eingabepuffer (Headerabschnitt und Payloadabschnitt),
ohne dafuer einen zusaetzlichen `forCrc`-Verkettungspuffer anzulegen; der vom
Storage-Port gelesene Puffer zaehlt dabei als "der gelesene Record" und wird
nicht doppelt gezaehlt. Die Payload wird beim Aufbau des Ergebnisses hoechstens
einmal in dieses Ergebnis verschoben bzw. kopiert.

Der Verzicht auf einen `forCrc`-Verkettungspuffer ist an dieser Stelle
strukturell erzwungen, nicht durch einen zur Laufzeit zaehlbaren Test
belegbar: `std::string`-Payloads lassen keine Kopieranzahl instrumentieren.
Der Beleg besteht aus drei Teilen: (1) im gesamten Produktionscode existiert
kein `forCrc`-Verkettungspuffer mehr (geprueft per
`grep -rn "forCrc" lib/ src/`), (2) `encodeEnvelope()` veroeffentlicht
ausschliesslich per `swap()` (keine Vollkopie, siehe oben fuer die praezise,
nicht absolute Formulierung dieser Garantie) und (3) die Golden-Vector- sowie
Rundlauftests in `test_storage_wireformat.cpp` beweisen, dass der
inkrementelle CRC ueber Header und Payload byteidentische Ergebnisse zum
vormaligen Verkettungspuffer liefert. `scanTechnicalSlotCandidates`
materialisiert dagegen ueberhaupt keine Payload mehr (nur Metadaten ueber
`decodeEnvelopeMetadata`); ein Allokationszaehler-Test belegt, dass der
Spitzenspeicherbedarf nicht mit `Slotzahl * Payloadgroesse` skaliert. Separate
Grenztests belegen einen erfolgreichen Scan mit exakt acht Slots sowie die
typisierte Ablehnung von neun Slots vor jedem Store-Read und jeder dynamischen
Ergebnisallokation. `loadSlotPayload` laedt die Payload eines gewaehlten Slots
erst spaeter mit vollstaendiger Neuvalidierung (CRC, Record-Identitaet,
`versionValue`).

Ein Test mit einem absichtlich vertragsverletzenden Test-Store (der `read()`
mit `WriteError`/`CommitOutcomeUnknown` oder `write()` mit
`NotFound`/`ReadError` zurueckgeben liesse) ist seit der Statustyp-Trennung
nicht mehr in gueltigem C++ konstruierbar: `IStateStore::read()` gibt
`StateStoreReadResult` mit `StateStoreReadStatus` zurueck, ein Typ ohne
`WriteError`-/`CommitOutcomeUnknown`-Werte. Das ist ein Compilefehler statt
eines Laufzeitfehlers und damit ein staerkerer Beleg als ein Test es waere.

`ByteWriter::writeBytes`, `ByteReader::readBytes`, `Crc32IsoHdlc::update` und
`ISecureRandomSource::fill` (samt `MockSecureRandomSource::fill`) setzen den
Nullzeigervertrag technisch durch, statt ihn wie `std::memcpy` nur zu
dokumentieren: bei `length == 0` ist der Zeigerparameter erlaubt `nullptr` zu
sein, wird nie dereferenziert und der Aufruf ist ein erfolgreicher No-Op ohne
jede Zustandsaenderung (bei `ISecureRandomSource::fill` insbesondere: ein
vorbereiteter Override wird nicht konsumiert, der Generatorzustand nicht
weiterbewegt). Bei `length > 0` und `nullptr` lehnen alle vier Stellen
beobachtbar ab (`bool`/`false`), ohne Speicherzugriff und ohne UB.
`Crc32IsoHdlc::update` ist dafuer `[[nodiscard]] bool`; alle
Produktionsaufrufer behandeln den Rueckgabewert mit einem expliziten
Fehlerzweig, sodass nie ein CRC aus nur einem Teil der vorgesehenen Chunks
veroeffentlicht oder akzeptiert wird. Die reine Rohzeiger-
One-Shot-Ueberladung `computeCrc32IsoHdlc(const void*, size_t)` wurde entfernt,
da kein Aufrufer sie noch braucht und ein sinnvoller Sentinel-Fehlerwert fuer
`uint32_t`-CRCs nicht existiert; `computeCrc32IsoHdlc(const std::string&)`
bleibt bestehen, da `std::string::data()` nie `nullptr` ist.
`scanTechnicalSlotCandidates` erzwingt die zentrale anwendungsneutrale Grenze
`kMaximumTechnicalSlotsPerScan == 8` zu Beginn des Aufrufs. Neun oder mehr
Schluessel liefern `SlotScanStatus::SlotLimitExceeded`, bevor der Store gelesen
oder eine Ergebnisliste allokiert wird; Kandidaten und Slot-Issues bleiben leer.
Ein erfolgreicher Scan ohne Kandidaten bleibt durch `SlotScanStatus::Success`
eindeutig unterscheidbar. `SlotIssueKind::CapacityError` bezeichnet weiterhin
ausschliesslich den Kapazitaetsfehler eines tatsaechlich gelesenen Slots.

### Paket B: Typisierte Konfigurationsdokumente

- UserConfiguration Schema 1
- ServiceConfiguration Schema 1
- ProgramCatalog Schema 1
- Text-, ID- und Payloadgrenzen
- fachliche Codecs, Validierung und Migration

### Paket C: Manifeste, kanonische Roots, Preview und Runtimeaktivierung

- Active/Fallback und korrigierte kanonische Schutzwurzeln
- vollstaendige technische und fachliche Graphvalidierung
- sichere Manifest- und Root-Slotrotation
- fluechtige vollstaendig validierte Vorschau mit Revisions-, Basis- und
  Konfliktschutz
- vorbereiteter `RuntimeConfigurationSnapshot`, atomarer Root-Commit und
  vertraglich nicht fehlschlagendes Publish
- typisierter `ConfigurationRuntimeFailure` fuer die spaetere Integration in
  #24
- kein Pending, Aktivierungsintent oder vorbereiteter paralleler Active-Zweig

### Paket D: Bootstrap, StorageEpoch, Werksreset und End-to-End-Recovery

- Bootstrap und StorageEpoch
- Korruptionssperre statt stiller Factory-Neuanlage
- wiederaufnehmbarer Werksreset
- Erhalt der geraetespezifischen Touchkalibrierung
- keine leeren Connectivity-/Authentication-Manifeste, -Roots oder -Slots
- vollstaendige Cut-Point- und Ressourcenmatrix

Jedes Paket erhaelt einen eigenen kleinen PR. Issue #16 bleibt als
Tracking-Issue offen und wird erst abgeschlossen, wenn alle Teilissues gemergt
und die End-to-End-Akzeptanzkriterien erfuellt sind.

#54 und #55 sind abgeschlossen; #56 wurde mit PR #65 gemergt. #57 wird im
Draft-PR #68 als eigenes Implementierungsvorhaben umgesetzt. Reale ESP32-NVS-,
Heap-, Jitter-, Watchdog-, Flashatomizitaets- und Lebensdauermessungen bleiben
sichtbare nachgelagerte Gates; fuer den Gesamtvertrag wird kein einzelner
Implementierungsbranch erstellt.

## Ausdruecklicher Nicht-Scope

Der Themenbereich implementiert noch keine:

- Laufpersistenz, Kontrollpunkte oder reale Recoverable-Run-Erkennung aus #17
- portables Backupformat, Journale und Aufbewahrung aus #19
- systemweite Fehlerklassen, persistente Verriegelungen, vollstaendige
  `SAFE_BOOT`-Politik oder reale Aktorsperren aus #24
- reale Connectivity-Domaene, WLAN-Credentials, Passwort-/PIN-Pruefnachweise,
  Anmeldung, Sitzungen, Tokens, CSRF oder Sperrzeiten aus den spaeteren
  Connectivity-/Authentication-Arbeiten
- noch nicht fachlich definierte Display-, Ton-, Sensor-, Regel-, Sicherheits-
  oder Hardwarefelder
- lokale Terminplanung oder lokale-Zeit-nach-UTC-Regeln
- physische Recovery-Geste oder behauptete reale Flashgarantie
- zweites ProgramDocument-Schema oder freie Secret-Blob-/Reservefelder
