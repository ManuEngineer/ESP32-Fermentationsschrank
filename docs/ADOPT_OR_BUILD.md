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
