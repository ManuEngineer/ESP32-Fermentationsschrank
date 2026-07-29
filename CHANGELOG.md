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
- Konfigurationspersistenz fuer Issue #16 auf den Variante-B-Vertrag mit
  typisierten Dokumentgenerationen, kanonischem Active/Fallback, Wireformat
  sowie deterministischen Recovery- und Ressourcengrenzen konsolidiert;
  persistentes Pending, Aktivierungsintent und vorbereitete Secret-Domaenen
  entfernt; Schutzwurzeldefinition, Abgrenzung zu #17/#24 und verpflichtende
  Aufteilung in kleine Teilissues nach Review korrigiert
- Programmmodellgrenzen einschliesslich minimaler und maximaler Anzahl der
  Fermentationsphasen vollstaendig in `program_limits.hpp` zentralisiert; das
  fachliche Verhalten und das Programmschema bleiben unveraendert
- Plattformpersistenz aus Issue #54 gemaess ADR-016 (NVS als Backend) korrigiert:
  - `StateStoreKey` portseitig auf 1..15 Bytes aus `[A-Za-z0-9_.-]` begrenzt,
    eigener Status `InvalidCharacter` (Befund 1)
  - Slot-Scan haelt hoechstens einen Recordpuffer (Metadaten-Scan plus
    `loadSlotPayload` mit voller Neuvalidierung); kein Wachstum mit
    `Slotzahl * Payloadgroesse`, Metadatenwachstum durch maximal acht Slots pro
    Scan begrenzt (Befund 2)
  - `nextSlotRoundRobin` lehnt `lastWrittenSlot >= slotCount` als
    `InvalidLastSlot` ab statt still per Modulo zu normalisieren (Befund 3)
  - `ChangeOrigin`/`ChangeOperation` aus dem Envelope in die Payload verschoben
    (Header 37/45 Bytes, Version bleibt 1); Golden-Vektoren neu berechnet
    (Befund 4)
  - aufruferlosen `ITimeZoneResolver`-Port samt Mock entfernt (Befund 5)
  - Code-Kommentare auf den Vertrag reduziert, ohne Verhaltensaenderung
    (Befund 6)

### Added

- Bootstrap-, `StorageEpoch`- und Recoverykern fuer Issue #57: redundanter
  Schema-1-Bootstrap mit geschlossener Historienrelation, exakt gebundenes
  Factory-Neuheitsorakel, wiederaufnehmbare Initialisierung und autorisierter
  Werksreset mit erhaltener Touchkalibrierung. Epocheninitiale Graphwrites
  verwenden den bestehenden #56-Graphstore und dieselbe Mutationslease;
  Dokument-/Manifest-, Root- und Bootstrap-Unknowns bleiben getrennt. Runtime
  wird erst nach vollstaendig validiertem Root intern publiziert und erst nach
  bestaetigtem `Initialized` normal freigegeben. No-Runtime-Reset,
  Active-/Fallback-Boot, Cut-Points, Konkurrenz, unbekannte Schemas, additive
  Erweiterbarkeit und Zwei-Modell-Grenze sind nativ abgedeckt. Keine
  Connectivity-, Authentication-, Secret-, Lauf- oder Safetyimplementierung
  wurde vorgezogen; reale NVS-/Heap-/Jitter-/Watchdog-/Flashmessungen und das
  `CONFIGURATION_SAFETY_INTEGRATION_GATE` aus #24 bleiben offen.

- Active-/Fallback-Konfigurationskern fuer Issue #56: kanonische Manifest- und
  Rootcodecs, vollstaendige Graph- und Identitaetsvalidierung, High-Water-
  basierte sichere Slotrotation ohne separate `MutationSequence`, gemeinsame
  nicht blockierende Mutationslease, rein fluechtige validierte Vorschau,
  begrenzte Runtime-Read-Leases sowie Root-Commit mit vorbereitetem
  nicht fehlschlagendem Snapshot-Publish. Nicht aufloesbare Rootausgaenge
  wechseln in `ConfigurationCommitIndeterminate`; nach bestaetigtem Root
  nicht abschliessbare Verifikation wechselt fail closed in
  `ConfigurationRuntimeFailure`. Native Tests decken Kollisionen, verwaiste
  Writes, mindestens fuenf Rotationen, Cut-Points, Konkurrenz,
  Leserlebensdauer und die Zwei-Modell-Grenze ab. Das nachgelagerte
  `CONFIGURATION_SAFETY_INTEGRATION_GATE` in #24 sowie reale ESP32-Heap-,
  NVS-, Jitter-, Watchdog- und Lebensdauermessungen bleiben offen.
  Der Loader behaelt den Fallback nur als validierte Metadaten, scannt
  Recordgruppen ohne Payloadsammlung und behandelt nicht lesbare benoetigte
  Root-/Graphrecords vor jeder Runtimefreigabe fail closed. Die vollstaendige
  Reviewkorrektur ergaenzt die erneute kanonische Root-/Fallbackbestimmung vor
  jedem Write, die vollstaendige Zielgraphpruefung vor dem Rootwrite, intern
  abgeleitete Aenderungsmasken, getrennte Persistenzursachen sowie besitzsichere
  Preview-, Reader-, Retirement- und Recoveryreservierungen. Ein exakt
  persistenter neuer Root kann nicht als alter Ausgang aufgeloest werden.

- Typisierte Konfigurationsdokumente aus Issue #55: UserConfiguration,
  exakt leere ServiceConfiguration und ProgramCatalog Schema 1 mit
  vollstaendiger ID-/UTF-8-/Unicode-/Anzahl-/Payloadvalidierung, versionierten
  Sprach- und Zeitzonenkatalogen sowie einem schmalen `ITimeZoneResolver` und
  deterministischem Testadapter. Die fachlichen Big-Endian-Payloadcodecs
  besitzen feste Golden-Bytes, stabile Enum-Wire-IDs, getrennte starke
  Schema-/Revisionstypen und Copy-Migration ohne Teilwirkung. Der
  ProgramCatalog verwendet das vorhandene ProgramDocument-Schema 5 samt
  bestehender Migration 4 nach 5; Notizen werden ohne neues Feldmaskenbit als
  ausdrueckliches Katalogfeld kodiert. Explizite Enum-Switches in beiden
  Richtungen entkoppeln die stabilen Wire-IDs von nativen Enumordinalwerten.
  Der ProgramCatalog-Encoder berechnet seine kanonische Payloadgroesse
  ueberlaufsicher vorab und reserviert den Writer exakt statt pauschal mit
  32.768 Byte. Native Allokationsregressionstests unterscheiden Katalogmodell,
  alten Ausgabepuffer, neuen Payloadpuffer und Migrationskandidaten, ohne eine
  reale ESP32-Heapgarantie zu behaupten.

- Anwendungsneutrale Plattformpersistenz und Wireformat fuer Issue #54
  (Paket A von #16, Closes #54): begrenztes binaersicheres `IStateStore` mit
  caller-/schluesselspezifischem Leselimit und vier eindeutig unterscheidbaren
  Schreibergebnissen - `Success`, `WriteError`/`CapacityError` (sicher
  unveraendert) und `CommitOutcomeUnknown` (Commit-Ausgang unbekannt, z. B.
  nach Stromausfall zwischen Commit und Rueckkehr; der neue Wert kann bereits
  dauerhaft gespeichert sein, der Aufrufer muss zuruecklesen); `read()` und
  `write()` verwenden bewusst getrennte Statustypen `StateStoreReadStatus`/
  `StateStoreWriteStatus` statt eines gemeinsamen Enums - ein Adapter kann
  `WriteError`/`CommitOutcomeUnknown` schon aufgrund des Rueckgabetyps nicht
  als Leseergebnis liefern, und umgekehrt; gueltig-by-construction
  begrenzter `StateStoreKey`-Werttyp (kein oeffentlicher Default-Konstruktor,
  `create()` erzwingt gemaess ADR-016 1..15 Bytes aus `[A-Za-z0-9_.-]` und
  lehnt leeren (`Empty`), zu langen (`TooLong`) und zeichensatzverletzenden
  (`InvalidCharacter`) Schluessel typisiert ab); starke technische Typen fuer
  StorageEpoch, Revision,
  Generation, RecordSequence, SlotId und RecordTypeId sowie ein generischer
  `checkedIncrement`-Baustein, der sowohl den reservierten Ausgangswert 0
  (`InvalidCurrentValue`) als auch einen Ueberlauf von `UINT64_MAX` auf 0
  (`Overflow`) stabil ablehnt; begrenzte Big-Endian-Byte-Reader/-Writer
  (`ByteWriter::writeBytes`/`ByteReader::readBytes` setzen den
  Nullzeigervertrag technisch durch: `length == 0` ist ein sicherer No-Op
  auch mit `nullptr`, `length > 0` mit `nullptr` wird beobachtbar per
  `false` abgelehnt statt undefiniertes Verhalten zu riskieren; dedizierter
  `uint8`-Golden-Test mit festen Werten `0x00`/`0x01`/`0x7F`/`0x80`/`0xFF`
  ergaenzt den bisher erst bei `uint16` beginnenden Big-Endian-Golden-Test,
  inklusive Lesen aus leerem Puffer, das Ausgabeparameter und Leseposition
  unveraendert laesst); portable
  Zweierkomplement-Dekodierung signierter Ganzzahlen ohne
  implementation-defined unsigned-zu-signed-Konvertierung; IEEE-754-binary64-
  Codec mit `-0.0`-Normalisierung und NaN-/Inf-Ablehnung; inkrementeller
  CRC-32/ISO-HDLC-Akkumulator (`Crc32IsoHdlc::update`, jetzt
  `[[nodiscard]] bool` mit demselben technisch durchgesetzten
  Nullzeigervertrag; die reine Rohzeiger-One-Shot-Ueberladung
  `computeCrc32IsoHdlc(const void*, size_t)` wurde entfernt, da kein
  Aufrufer sie noch braucht und ein Sentinel-Fehlerwert fuer `uint32_t`-CRCs
  nicht existiert), von Envelope-Encoding und -Decoding genutzt, um den CRC
  direkt ueber Header und Payload zu berechnen, ohne einen zusaetzlichen
  `header + payload`-Hilfspuffer anzulegen; generischer Envelope Version 1
  (37/45 Bytes) mit ueberlaufsicherer, gestufter Groessenpruefung (eigener
  `checkedAddSize`-Baustein sowie die neue freie, zustandslose
  `checkEnvelopeEncodedSize(payloadSize, hasUtc, maxTotalBytes)`-Funktion,
  die dieselbe Entscheidung als reine Zahlen ohne Pufferaufbau liefert und
  damit Grenzwerte bis `UINT32_MAX` ohne reale 4-GiB-Allokation testbar
  macht) vor jeder Allokation und Veroeffentlichung des fertigen Records
  erst nach vollstaendigem Erfolg per `swap()` statt Kopie - dabei entsteht
  hoechstens ein zusaetzlicher, neu aufgebauter vollstaendiger Recordpuffer;
  ein bereits in `outBytes` gehaltener alter Record bleibt bis zur
  erfolgreichen `swap()`-Zeile unveraendert bestehen und wird bei jedem
  Fehler nicht angetastet - `InvalidField` gilt dabei ausschliesslich fuer
  die vier reservierten Nullwertfelder und ungueltige Optionaltags,
  `CapacityExceeded` einheitlich fuer jede Groessen-/Kapazitaetsfrage
  einschliesslich einer in `uint32_t` nicht darstellbaren Payloadgroesse
  (die staerkere globale Ein-Puffer-Garantie waehrend eines vollstaendigen
  Commits ist Aufgabe des Commit-Workflows in #56/#57); rein technische
  Slotkandidaten-Ermittlung (`scanTechnicalSlotCandidates`) mit
  deterministischer Sortierung, die uebersprungene Slots nicht
  stillschweigend verwirft, sondern als typisierte `SlotIssue`-Liste erhaelt
  (`NotFound` nie gleichbedeutend mit `ReadError`; `UnexpectedStatus` fuer den
  nachweislich unerreichbaren Success-Fallback in den internen
  Statusmappern) und keine Payload im Ergebnis mehr materialisiert
  (Metadaten-Scan; `loadSlotPayload` laedt eine gewaehlte Payload spaeter mit
  voller Neuvalidierung), mit zentraler anwendungsneutraler Obergrenze von acht
  Slots pro Scan und typisiertem `SlotScanStatus::SlotLimitExceeded` vor jedem
  Store-Read oder jeder Ergebnisallokation - weiterhin ohne fachliche
  Slotzahlen oder Root-/Manifestbedeutung;
  typisierte `nextSlotRoundRobin`-Rotation (`NextSlotStatus::Success`/
  `InvalidSlotCount`/`InvalidLastSlot` mit `std::optional<SlotId>`), die eine
  ungueltige Slotanzahl und ein `lastWrittenSlot >= slotCount` typisiert und
  unterscheidbar ablehnt, statt sie mit einer erfolgreichen Rotation zu Slot 0
  zu verwechseln oder still per Modulo zu normalisieren, unabhaengig von der
  `size_t`-Breite der Zielplattform ueberlaufsicher; `ISecureRandomSource`-
  (`fill()` mit demselben technisch durchgesetzten Nullzeigervertrag: bei
  `length == 0` wird weder ein vorbereiteter Override konsumiert noch der
  Generatorzustand weiterbewegt, bei `length > 0` mit `nullptr` lehnt jede
  Implementierung inklusive `MockSecureRandomSource` beobachtbar ab);
  `SimulatedPersistentStateStore` mit injizierbaren Schreib-Cut-Points
  (Fehler vor Beginn, Stromausfall vor/nach Commit, Kapazitaetsfehler) sowie
  Read-/NotFound-/Korruptionsinjektion fuer native Tests; die drei
  geforderten Zustandsbereiche sind als getrennte private Datenhaltung
  modelliert - dauerhaft `committed_`, eine gestagte, aber noch nicht
  committete Schreiboperation (`std::optional<PendingWrite> pendingWrite_`)
  sowie fluechtiger Testzustand; `write()` bildet "vollstaendig staging,
  dann atomar committen" nach, `restart()` verwirft `pendingWrite_` und alle
  fluechtigen Testschalter, laesst `committed_` unveraendert; ein
  testinterner Zugriff macht Existenz sowie exakten Schluessel und
  vollstaendigen binaeren Inhalt des Stagings fuer native Tests beobachtbar,
  ohne die produktive `IStateStore`-Schnittstelle zu vergroessern; alle
  Envelope-CRC-Aufrufer behandeln den booleschen `update()`-Rueckgabewert
  explizit, statt ihn nur als nicht fehlschlagend zu unterstellen
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
