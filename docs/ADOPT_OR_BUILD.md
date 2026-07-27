Status: DRAFT – Ownerfreigabe ausstehend

# Adopt-or-build-Grundsatz

## Zweck

Dieser Entwurf beschreibt einen dauerhaften Entscheidungsweg fuer neue
technische Komponenten. Er aendert keine akzeptierte ADR und waehlt keine
Bibliothek aus. Die Auditgrundlage und die konkreten Kandidaten stehen im
[`Release-1-Adopt-or-build-Audit`](audits/RELEASE_1_ADOPT_OR_BUILD_AUDIT.md)
und in den
[`Komponentenevaluationen`](audits/COMPONENT_EVALUATIONS.md).

## Grundsatz

Vor jeder Eigenentwicklung wird geprueft, ob eine gepflegte vorhandene Loesung
die technische Aufgabe bereits erfuellt. Geprueft werden in dieser Reihenfolge:

1. mit der fixierten ESP32-/Arduino-Toolchain gelieferte Frameworkfunktion;
2. offizielles Herstellerpaket oder offizieller Referenzcode;
3. gepflegte Bibliothek mit nachvollziehbarer Herkunft und Lizenz;
4. kleine eigene Implementierung, wenn die vorherigen Varianten den Vertrag
   nicht sicher, nicht ressourcengerecht oder nicht wartbar erfuellen.

Eine externe Loesung wird nicht direkt zur Facharchitektur. Sie bleibt hinter
einem schmalen Port oder Adapter. Der Adapter uebersetzt Fehler, Grenzen und
Lebenszyklus in den bestehenden Projektvertrag. Die Anwendung kennt keine
Bibliothekstypen.

## Was selbst entwickelt wird

Der sicherheits- und fachkritische Kern bleibt unter Projektkontrolle:

- Sensorqualitaet, Plausibilitaet und Rollenbindung;
- Regelsensorauswahl und Ersatzbetrieb;
- zeitproportionale PI-Regelung und Luftbegrenzung;
- Aktorplanung, Mindestzeiten, Totzeit und Luefternachlauf;
- Fehlerklassen, Verriegelungen, `SAFE_BOOT` und Freigabeentscheidungen;
- Prozess-, Lauf-, Recovery- und Konfigurationsfachlogik;
- Berechtigungs-, Konflikt- und Bediensemantik.

Treiberbibliotheken duerfen Bytes lesen, Pixel zeichnen oder Protokolle
abwickeln. Sie entscheiden nie, ob Heizen oder Kuehlen sicher freigegeben ist.

## Hardwareabhaengige Entscheidungen

Eine Hardwarebibliothek wird erst nach einem reproduzierbaren Spike gegen die
tatsaechliche Boardrevision bewertet. Alle Kandidaten verwenden denselben
Aufbau, dieselben Tests und dieselbe fixierte Toolchain. Gemessen werden
mindestens Funktion, Fehlerverhalten, Flash, statisches RAM, freier Heap,
groesster freier Heapblock und Stabilitaet. Ein README-Versprechen ist kein
Hardwarebeweis.

## Herkunft und Lizenzen

Vor Einbindung werden Quelle, Version oder Commit, Abrufdatum, Lizenzdatei,
enthaltene Drittbestandteile und Publikationspflichten dokumentiert. Direkte
Codeuebernahme, unveraenderte Bibliotheksnutzung und eigener neu geschriebener
Adapter werden getrennt erfasst. Unklare Herkunft blockiert nicht zwingend eine
interne technische Untersuchung, wohl aber eine ungepruefte oeffentliche
Weiterverteilung direkt uebernommener Dateien.

Das vorgeschlagene Register steht in
[`THIRD_PARTY_COMPONENTS.md`](THIRD_PARTY_COMPONENTS.md); der ausfuehrliche
Herkunftsnachweis steht im
[`Third-Party-Review`](audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md).

## Ressourcen und Zukunftsfunktionen

Es werden keine ungenutzten Bibliotheken, grossen Puffer oder vorsorglichen
Ports fuer spaetere Funktionen eingebunden. Eine Empfehlung gilt erst nach
Base-/Kandidatenmessung mit identischer Toolchain. Release 1 erhaelt weder
Web-OTA, Bluetooth, Cloud/Push, PID-Autotuning noch eine PSRAM-Abhaengigkeit.

## Persistenz und additive Erweiterbarkeit

Auch eigene Speichervertraege folgen dem Adopt-or-build-Grundsatz: Der kleinste
aktuell benoetigte sichere Vertrag wird umgesetzt, ohne einen spaeteren Ausbau
durch inkompatible Einwegvereinfachungen zu verbauen. Dokumente, Manifeste,
Roots, Envelopes, Record-Type-IDs, Revisionen und Schluessel bleiben versioniert
und stark typisiert. Unbekannte neuere Schemas werden ohne Teilwirkung
abgelehnt; bestehende Daten werden nicht still umgedeutet.

Spaetere Funktionen werden additiv ueber neue Recordtypen, neue Schema-
versionen und explizite Copy-Migrationen ergaenzt. Gemeinsame Transaktionsschritte
wie Kandidatenerzeugung, Validierung, Runtimevorbereitung, persistenter Commit
und Publish bleiben getrennt und wiederverwendbar. Erweiterbarkeit rechtfertigt
keine leeren Manifeste, Dummyrecords, ungenutzten Slots, Ports oder
Zukunftsservices. Solche Produktionsbausteine entstehen erst mit einem echten
fachlichen Konsumenten.

## WLAN-Onboarding

Standardisierte Portaltechnik soll zuerst adoptiert werden, waehrend
Connectivity-Ablauf, Credential-Kandidaten und -Commit, Secrets, Recovery,
Redaction und Safetyisolation projektspezifisch bleiben. Fuer Release 1 wird
WiFiManager zuerst begrenzt geprueft. Ein eigener Adapter aus den
Arduino-ESP32-Frameworkbausteinen wird nur bei einem nachgewiesenen Problem als
identischer Gegenprototyp nachgezogen. Weder dieser Rueckfall noch ein
spaeterer Wechsel rechtfertigt eine vorsorgliche Provisioning-, Provider-,
Plugin- oder Mehradapterarchitektur; die konkrete Integration bleibt an der
Composition Root austauschbar.

## JSON an externen Grenzen

Standardkonformes JSON-Parsing und -Serialisieren wird adoptiert statt als
allgemeiner Parser neu entwickelt. ArduinoJson `7.4.3` ist dafuer der
bevorzugte Release-1-Kandidat, wird aber erst nach einem begrenzten Build-,
Grenzwert-, Fuzz- und Ressourcennachweis endgueltig uebernommen. Eine
Alternative wird nur bei einem konkret belegten Release-1-Problem untersucht.

Die Bibliothek bleibt hinter einer kleinen konkreten DTO-/Codecgrenze.
Fachschema, Werte, Berechtigungen, Konflikte, Redaction, Importvorschau und
Aktivierung kontrolliert das Projekt selbst. Bibliothekstypen gelangen nicht
in Fach-, Safety-, Persistenz- oder gemeinsame View-Modelle; interne atomare
Persistenz bleibt beim typisierten binaeren Wireformat. Ein spaeterer Wechsel
ersetzt die konkrete Codecgrenze und rechtfertigt weder einen
`IJsonProvider`, ein Pluginregister noch einen vorsorglichen Zweitcodec.

## Journal, Historie und Import

Das Speicherbackend und ein Standard-JSON-Codec werden adoptiert; die
fachliche Semantik bleibt im Projekt. Dazu gehoeren typisierte
Ereigniskategorien, kritische Prioritaeten, Retention, begrenzte und
verdichtete Laufhistorie, stromausfallsichere Bereinigung, Redaction,
vollstaendige Importvalidierung und atomare Aktivierung ueber den
Active-/Fallback-Kern. Weder NVS noch eine Codecbibliothek entscheidet, welche
Daten sicherheitskritisch sind oder wann ein Kandidat veroeffentlicht werden
darf.

Interne Journale und Historien verwenden weiterhin typisierte binaere Records.
JSON bleibt an begrenzten externen Laufexport-, secret-freien Backup- und
Importgrenzen. Der nur lesende Export-/Backuppfad wird vor dem
zustandsveraendernden Import umgesetzt. Es entsteht weder eine eigene
allgemeine Datenbank noch eine parallele Aktivierungssemantik; Fehler,
Unterbruch oder Neustart duerfen keine Teilaktivierung hinterlassen.

## Praesentationsmodelle und Sprachen

Die fachliche Praesentationsprojektion bleibt unter Projektkontrolle. Kleine
ansichtsbezogene Modelle bilden kanonische Fach-, Sensorqualitaets-, Safety-
und Berechtigungsentscheidungen ab, ohne sie in der UI neu herzuleiten. Ein
allumfassender globaler UI-Zustand oder eine Mega-View wird ebenso wenig
vorbereitet wie eine Bindung an HTML, Webserver-, Displaytreiber-, Widget- oder
LVGL-Typen.

Gemeinsame Textschluessel, sprachunabhaengige Fehlercodes, Kataloge und
semantische Formatierungsregeln sind eigene Produktvertraege. Eine
Uebersetzungsbibliothek oder ein UI-Framework wird dafuer nicht vorsorglich
ausgewaehlt. Konkrete Treiber und Frameworks bleiben hinter schmalen
Integrationsgrenzen; Touch- und Webnavigation sowie ihre Layouts bleiben
oberflaechenspezifisch.

Die lokale Screen-, Navigations-, Dialog- und Aktionslogik wird als kleine
projektspezifische Verantwortung selbst entwickelt und nativ mit simulierten
Touchereignissen geprueft. Sie ist weder ein zweiter Fach-/Safetyzustandsautomat
noch ein Anlass fuer ein allgemeines Widget-, Layout- oder UI-Pluginframework.
Display-/Touchtreiber und eine allfaellige Widgettechnik werden erst nach dem
Hardwarevergleich hinter der konkreten Integrationsgrenze adoptiert; schlanke
Views und LVGL werden nicht vorsorglich parallel vorbereitet. Der vorhandene
microSD-/SD-Karten-Slot begruendet ohne R1-Konsumenten keine Bibliothek, keinen
Port, Adapter oder Zukunftspfad.

## Entscheidungsnachweis

Jede spaetere Komponentenentscheidung nennt:

- konkreten Release-1-Vertrag;
- gepruefte Alternativen;
- Version oder Commit und Lizenz;
- Spike- oder Buildnachweis;
- Ressourcenvergleich;
- benoetigten Adapter und verbleibende Eigenlogik;
- Risiken und Rueckfallstrategie;
- Ownerfreigabe und das umsetzende Issue.

Eigenentwicklung wird mit einem konkreten Sicherheits-, Vertrags-, Ressourcen-
oder Wartungsgrund begruendet. "Selbst schreiben" und "Bibliothek verwenden"
sind keine ausreichenden Begruendungen.
