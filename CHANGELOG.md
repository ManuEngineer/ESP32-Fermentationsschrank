# Changelog

Alle wesentlichen Aenderungen dieses Projekts werden hier dokumentiert.

## Unreleased

### Changed

- Laufkommandos aus Issue #15 nach Nachreview des gemergten PR #51 korrigiert
  (Refs #15): phasengerechte statt pauschale Laufrevision bei
  Zieltemperaturaenderung ueber einen typisierten Phasenkontext/Wirkungswert;
  unbekannte Phasenkontexte werden sicher abgelehnt; Kapazitaetspruefung nach
  den nicht mutierenden Fachpruefungen und vor jeder Erhoehung der Lauf-,
  Meldungs- und Fehlerrevision sowie der einzelnen Meldungsrevision;
  `commandSequence` wird nur fuer tatsaechlich vorgeschlagene Entscheidungen
  erhoeht; mehrstufige Laufanpassungs-, Abbruch-/Kuehl- und
  Abschluss-/Kuehlentscheidungen werden vor der Uebernahme vollstaendig auf
  einem lokalen Kandidatenzustand validiert; sichere Ablehnung eines
  unbekannten `StopOption`-Werts statt stillschweigender Behandlung als
  Abbruch-und-Ausschalten, wobei Idempotenz vor der Stopwertauswertung geprueft
  wird;
  Startzusammenfassung nun auch fuer eine gueltige, aber unbestaetigte Anfrage
  verfuegbar, ohne dass eine fehlende Bestaetigung eine ungueltige, veraltete
  oder sicherheitsseitig abgelehnte Anfrage maskiert;
  Katalog-/Programmaufloesungsgrenze zu #16 sowie die begrenzte
  In-Memory-Idempotenz zu #17/#27 dokumentiert
- Konfigurationspersistenz fuer Issue #16 mit typisierten Dokumentgenerationen,
  kanonischen Active-/Fallback-/Pending-Roots, getrennten Secret-
  Rueckfallregeln, Wireformat sowie deterministischen Recovery- und
  Ressourcengrenzen spezifiziert; Schutzwurzeldefinition, Abgrenzung zu #17/#24
  und verpflichtende Aufteilung in kleine Teilissues nach Review korrigiert
- Programmmodellgrenzen einschliesslich minimaler und maximaler Anzahl der
  Fermentationsphasen vollstaendig in `program_limits.hpp` zentralisiert; das
  fachliche Verhalten und das Programmschema bleiben unveraendert

### Added

- Anwendungsneutrale Plattformpersistenz und Wireformat fuer Issue #54
  (Paket A von #16, Closes #54): begrenztes binaersicheres `IStateStore` mit
  caller-/schluesselspezifischem Leselimit und vier eindeutig unterscheidbaren
  Schreibergebnissen - `Success`, `WriteError`/`CapacityError` (sicher
  unveraendert) und `CommitOutcomeUnknown` (Commit-Ausgang unbekannt, z. B.
  nach Stromausfall zwischen Commit und Rueckkehr; der neue Wert kann bereits
  dauerhaft gespeichert sein, der Aufrufer muss zuruecklesen); begrenzter,
  binaersicherer `StateStoreKey`-Werttyp mit anwendungsneutraler
  Softwaregrenze (keine reale NVS-Garantie); starke technische Typen fuer
  StorageEpoch, Revision, Generation, RecordSequence, SlotId und RecordTypeId
  sowie ein generischer `checkedIncrement`-Baustein, der deren Ueberlauf von
  `UINT64_MAX` auf 0 stabil ablehnt; begrenzte Big-Endian-Byte-Reader/-Writer
  (Nullzeiger bei Laenge 0 sicher behandelt); portable
  Zweierkomplement-Dekodierung signierter Ganzzahlen ohne
  implementation-defined unsigned-zu-signed-Konvertierung; IEEE-754-binary64-
  Codec mit `-0.0`-Normalisierung und NaN-/Inf-Ablehnung; CRC-32/ISO-HDLC;
  generischer Envelope Version 1 (41/49 Bytes) mit ueberlaufsicherer,
  gestufter Groessenpruefung (eigener `checkedAddSize`-Baustein) vor jeder
  Allokation; rein technische Slotkandidaten-Ermittlung
  (`scanTechnicalSlotCandidates`) mit deterministischer Sortierung, die
  uebersprungene Slots nicht stillschweigend verwirft, sondern als typisierte
  `SlotIssue`-Liste erhaelt (`NotFound` nie gleichbedeutend mit `ReadError`)
  - weiterhin ohne konkrete Slotzahlen oder Root-/Manifestbedeutung;
  `ISecureRandomSource`- und `ITimeZoneResolver`-Ports;
  `SimulatedPersistentStateStore` mit injizierbaren Schreib-Cut-Points
  (Fehler vor Beginn, Stromausfall vor/nach Commit, Kapazitaetsfehler) sowie
  Read-/NotFound-/Korruptionsinjektion fuer native Tests
- Initiale Projektstruktur
- Template auf ESP32-Fermentationsschrank angepasst
- Hardwarekomponenten und Sicherheitsregeln ohne GPIO-Festlegung dokumentiert
- PlatformIO-Profile `native`, `esp32_bringup` und `esp32_release`
- getestete sichere Buildrichtlinien fuer 4 MB Flash, Betrieb ohne PSRAM,
  deaktiviertes Web-OTA und gesperrte reale Aktoren
- virtuelle Zeitquelle `ITimeSource`/`VirtualTimeSource` (monoton und optionale
  UTC-Zeit) mit nativen Tests fuer Zeitfortschaltung, Neustart und
  Zeitvorwaertssprung
- CI-Qualitaetspruefungen: Formatpruefung (clang-format 18.1.8), Static
  Analysis (clang-tidy 18.1.8), Geheimnis-/Lokalkonfigurationspruefung,
  ADR-013-Architekturgrenzen und Firmware-/Ressourcen-Groessenbericht als
  Buildartefakt
- Selbsttest, der beweist, dass Format-, Static-Analysis-, Geheimnis- und
  Architekturpruefung absichtlich fehlerhafte Faelle erkennen
- `-Werror` fuer native und ESP32-Profile; `library.json` in
  `lib/device_platform/`, `lib/device_platform_test_support/` und
  `lib/fermentation_app/`, damit `-Wall -Wextra -Werror` auch dort greifen
  (PlatformIOs `build_src_flags` galt bisher nur fuer `src/`)
- Der gemeinsame `IActuatorSink` wurde durch die kleinen anwendungsneutralen
  Ports `IBidirectionalActuatorSink` und `IBinaryOutputSink` ersetzt; zusammen
  mit `ITemperatureSource`, `IStateStore`, `IEventJournal`, `INetworkStatus` und
  `IUserNotificationSink` stehen deterministisch steuerbare native Mockadapter
  bereit
- einfaches, ausdruecklich unkalibriertes thermisches Simulationsmodell
  (`ThermalSimulationModel`) fuer deterministische Heiz-/Kuehlverlaeufe in
  nativen Tests
- native Tests fuer Sensor-/Aktorfehlerinjektion, Stromausfall-/Neustart-
  Verhalten, Persistenz-Fehlerinjektion und begrenzte Journal-/
  Benachrichtigungspuffer
- explizite Persistenzergebnisse, die fehlende Werte von Lesefehlern
  unterscheiden und fehlgeschlagene Journalschreibvorgaenge melden
- neue interne Bibliothek `lib/device_platform_test_support/` fuer
  Mockadapter und Simulation; `device_platform` enthaelt jetzt ausschliesslich
  anwendungsneutrale Produktionsschnittstellen und -dienste (ADR-013)
- `IEventJournal` von der Mock-Speicherstruktur entkoppelt: der Port kennt nur
  noch `record(...)`, `entries()` ist eine Testhilfe von `MockEventJournal`
- ADR-013 um die verbindliche Trennung von Produktionsplattform und
  `device_platform_test_support` sowie um kleine, rollenunabhaengige Ports
  praezisiert
- `scripts/check_architecture_boundaries.py` erzwingt die erlaubte
  Abhaengigkeitsrichtung und Modulplatzierung in CI
- unbenutzten Include `<cmath>` in `thermal_simulation_model.cpp` entfernt
- versioniertes Release-1-Programmmodell mit deterministischer Struktur- und
  Startvalidierung, explizitem Migrationsschritt von Schema 3 auf 4 sowie
  unveraenderlichem Factory-Katalog fuer Joghurt mild, Joghurt stichfest,
  Milch- und Wasserkefir
- getrennte aktive Programmauswahl und Benutzerkopien; offene
  `TBD_COMMISSIONING`-Werte bleiben als Katalogvorlage ladbar, werden aber als
  produktive Laufzeitwerte sicher abgelehnt
- unveraenderliche Laufschnappschuesse mit getrennten wirksamen Ziel-/Zeitwerten,
  atomaren und begrenzten Laufrevisionen sowie streng validierter
  Wiederherstellung durch deterministisches Wiederabspielen der Historie;
  monotone Revisionsepochen erlauben weitere Anpassungen nach einem Neustart
- freigegebene Spezifikation fuer den deterministischen, hardware- und
  persistenzfreien Zustandsautomaten sowie fuer bestaetigungsbeduerftige
  Uebergangsentscheidungen
- programmspezifische Produktwartezeit fuer Schema 5 mit sicherer
  Schema-4-Migration, zentralen Programmmodell-Validierungsgrenzen und
  `READY`-Freigabe fuer Issue #14
- zentrale `program_limits.hpp` als einzige Quelle fuer entwicklerseitige
  Wertebereiche des Programmmodells
- deterministischer, hardware- und persistenzfreier Zustandsautomat mit
  expliziter Topologie aller kanonischen Zustaende, monotonen Phasenzeiten und
  reversiblen Uebergangsentscheidungen
- native Ablauf- und Negativtests fuer alle vier Standardablaeufe, Vorheizen,
  Produktwartefrist, Zielqualifikation, Abschlussmodi und manuelles Halten
- hardware- und persistenzfreie Laufkommandos fuer bestaetigten Programm- und
  manuellen Start, atomare Stopp-/Abschlussaktionen, zweistufige
  Laufanpassungen, gleichberechtigte Display-/Web-Konflikte sowie getrennte
  Meldungsquittierung, Stummschaltung und qualifizierte Fehlerresetabsicht
