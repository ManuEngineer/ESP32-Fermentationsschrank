# Plan Issue #19 – Journale, Aufbewahrung, Bereinigung, Backup und Import

```text
PLAN_REVISION=2 (konsolidiert; ersetzt Revision 1 `d63279646049e9b5e4b4004dda0adaac6c9489b1`, die nie freigegeben war)
PLAN_STATUS=DRAFT_AWAITING_PLAN_FIX_VERIFICATION_AND_OWNER_APPROVAL (exakte Plan-SHA steht im Draft-PR)
ISSUE=19 (E2.4), Epic #4
BASE_MAIN=9beb68f1935f80c6d2a59b5a612d542e5d9109a7 (PR #189 gemergt am 2026-10-08)
TOOLCHAIN=ESP-IDF v6.1 (fff9895c82d744c7237be8847347bdd1b07c6643)
EARLIER_DRAFT=REVIEW_DRAFT - PRESERVE, NOT APPROVED, NOT CANONICAL (im Repository und in allen PRs nicht auffindbar; dieser Plan stuetzt sich nicht darauf)
SCOPE_OF_THIS_COMMIT=NUR_PLAN (keine Produktionslogik, keine produktiven Tests, keine Schemas); ROADMAP-Bereinigung und Archiv sind akzeptiert und unveraendert
IMPLEMENTATION=NOT_STARTED
HARDWARE=NOT_RUN
ACTUATOR_RELEASE=NO
```

## 0. Owner-Entscheidungen und Gates

Eine Entscheidung gilt nur als getroffen, wenn sie hier ausdruecklich so steht.
Die Freigabe der exakten Plan-SHA ist davon getrennt. Empfehlungen sind begruendet,
aber keine stillen Defaults; ohne Ownerentscheid wird der jeweilige Schnitt nicht
begonnen. Nur echte, nicht bereits durch Scope-/Abhaengigkeitsvertraege
entschiedene Fragen stehen in der Tabelle.

**Keine Entscheidung (Scope-Abgrenzung, bereits kanonisch).** #19 liefert
anwendungsinterne Erzeuger/Verbraucher (Journal, Verlauf, Export-Writer,
Backup-Projektion, Import-Service, Reset-Ablauf) samt Nativtests. HTTP-
Transport und Bedienoberflaechen sind Sache von #27 (Web, Mutationspfade) und
#28 (Diagnose/Service/Berichtexport) beziehungsweise der Touch-UI; #28 haengt
hart an #19. Wird beim Umsetzen ein echter Konflikt dieser Abgrenzung
gefunden, wird er dem Owner vorgelegt – bis dahin ist das keine offene
Architekturentscheidung.

| ID | Entscheidung | Alternativen | Empfehlung | Stand |
|---|---|---|---|---|
| O1 | Speicherstruktur fuer Journal und Verlauf | **A** feste Ringslots im vorhandenen `state_store`-NVS ueber `IStateStore` (kein Portumbau, kein Erase); **B** eigene LittleFS-/Datenpartition (Partitionstabelle, neue Komponente, ADR-016-Variante B); **C** Hybrid | **A** | **OFFEN**, Gate G1 (mit neuer ADR, siehe 7) |
| O3 | Geraetegebundene Daten im normalen Backup (`sensorCommissioning` mit ROM-Bindung, Planerparameter) | **A** ausschliessen (nur nutzerbezogene Daten und ausdruecklich erlaubte Serviceparameter); **B** einschliessen mit Warnung; **C** nur mit zusaetzlicher Bestaetigung | **A** (die ROM-Bindung eines Geraets waere auf einem anderen Geraet eine falsche feste Bindung) | **OFFEN** |
| O4 | Verbleib des kritischen Journals nach lokalem Werksreset | **A** wie der Verlauf per `StorageEpoch` logisch unerreichbar; **B** kritisches Reset-/Sicherheitsjournal bleibt erhalten (dokumentierte Epochenuebernahme) | **A** (`BACKUP_SECURITY_RETENTION.md` sagt "soweit vorgesehen" und ist hier mehrdeutig) | **OFFEN** |
| O5 | Importtransport/-groesse | **A** begrenzter Gesamtbody; **B** chunkweises Streaming mit Vorab-Validator; **C** reduziertes Maximum (weniger Benutzerprogramme/kuerzere Notizen) | **keine Vorabwahl**: Wahl erst nach der C0-Messung des maximal gueltigen Kandidaten (`ADOPT_OR_BUILD.md`) | **OFFEN**, an G1 gebunden |
| O6 | Aufbewahrung in Release 1 | **A** feste Konstanten 5 Detail / 50 Zusammenfassungen; **B** innerhalb des Budgets konfigurierbar | **A** (KISS). **Bewusste Abweichung:** `BACKUP_SECURITY_RETENTION.md` und `RUN_PERSISTENCE.md` beschreiben die Werte als innerhalb fester Obergrenzen konfigurierbar; A schiebt die Konfigurierbarkeit auf eine spaetere Erweiterung (die Grenzen gehen nicht ins Wireformat ein) und verlangt einen Dokumentvermerk | **OFFEN** |
| O7 | Aktivierung des Journals und Startkriterium (Details 4.7) | **A** Das Journal wird innerhalb von #19 nach G1 produktiv komponiert; das Startkriterium aus `RESOURCE_BUDGET_AND_MAINTENANCE.md` ist dann aktiv. `NOT_COMPOSED` ist nur ein Zwischenzustand der Commits; **B** Das Journal bleibt in #19 bis zu einem Hardware-Folgenachweis nicht komponiert; das Startkriterium wird bis dahin **nicht** ausgewertet = ausdruecklich vorgelegte, befristete SSOT-Abweichung (Eintrag in `OPEN_POINTS.md`) | **A** (SSOT-konform; Risiko: Aktivierung ohne Hardwarenachweis, durch #36/#37 und HW-19 begrenzt) | **OFFEN** |
| O8 | Werksreset-Bedienung | **A** nur anwendungsinterner mehrstufiger Ablauf samt Praesentationsmodell; **B** zusaetzlich LVGL-Bildschirme | **A** | **OFFEN** |
| O9 | Hardware-Folgeissue | **A** Folgeissue fuer reale NVS-Wear-/Powercut-/Heap-/Timing-Verifikation (HW-19-xx) anlegen, wie #190 zu #30; **B** Hardwarenachweise bleiben in #19 offen | **A** | **OFFEN**; der Agent legt ohne Ownerauftrag kein Issue an |
| O10 | Exportumfang, falls das Budget den vollstaendigen Laufexport nicht traegt (4.4/4.5) | **A** Export enthaelt nur Programmsnapshot, Endgrund und Aggregate (ohne Ereignis-/Revisionsverlauf); **B** weniger aufbewahrte Detailplaetze; **C** groesserer Speicheranteil (beruehrt Layout, nur mit ADR) | keine Vorabwahl | **BEDINGT**: nur falls C0 zeigt, dass der vollstaendige Umfang nicht passt |

**Gates (nur diese).**

- **G0** Plan-Fix-Verification und Ownerfreigabe der exakten Plan-SHA.
- **G1** nach C0: Budgetzahlen und Konstanten, O1, O5 und gegebenenfalls O10.
- Danach laufen C1–C7 ohne weitere Ownerfreigabe je Schnitt durch; je Schnitt gelten
  die gezielten Tests und der Builder-Self-Check. Kanonisch erforderlich bleiben
  danach: Stopp fuer den unabhaengigen Review, Pre-Ready nur auf ausdrueckliche
  Owneranweisung, Ready/Merge durch den Owner. Ein Stopp mitten in C1–C7 erfolgt
  nur bei einer materiellen Planabweichung oder einem Stoppbefund.

## 1. Ziel und Nicht-Ziele

**Ziel.** Die in #19 verbliebenen hardwareunabhaengigen Datenfunktionen werden
auf den vorhandenen Persistenz-, Konfigurations- und Recovery-Ownern umgesetzt:
priorisiertes Journal, begrenzte Mess-/Laufhistorie mit Zusammenfassungen,
proaktive atomare Bereinigung, Laufexport (JSON/CSV), secret-freies Backup,
validierter Import mit Vorschau und atomarer Aktivierung sowie der lokale
Werksreset-Ablauf. Kritische Daten werden strukturell nie vor Komfortdaten
verdraengt.

**Nicht-Ziele.**

- #28 (Diagnoseanzeige, Diagramme, Serviceablauf, Diagnose-/Servicebericht-Export)
  wird nicht vorgezogen; #19 liefert nur Datenhaltung und die in #19 genannten
  Exporte (Lauf, Backup).
- Keine Web-Routen/-Transporte (#27), keine #27-Mutationspfade und keine Touch-Bildschirme ausser gemaess O8-B.
- Keine neue Datenbank, kein zweites Aktivierungsmodell, kein Pending/Intent,
  keine neue Prozessschleife, kein neuer Task.
- Keine Aenderung an Regelung, Safety, Interlock (#24), Aktorplanung oder der
  Laufpersistenz-Wahrheit (#17); keine Aktorfreigabe.
- Keine Authentication-/Connectivity-Domaenen-Erweiterung; Geheimnisse bleiben
  in ihren bestehenden epochengebundenen Domaenen und werden nie gelesen,
  gespiegelt oder exportiert.
- Keine OTA-/PSRAM-Reserve, kein Roh-Flash-Backup (nicht portabel, nicht ueber
  das Web).
- Keine Hardwaretests und keine erfundenen Hardware- oder Budgetwerte.

## 2. Verifizierte Live-Ausgangslage (2026-10-08)

- `main` `9beb68f`; PR #189 gemergt. Branch dieses Plans:
  `agent/issue-19-journals-retention-backup-import-plan`.
- #19 `OPEN`. Abhaengigkeiten: #16 (geschlossen 2026-10-08) und #17 (PR #84)
  erfuellt. #28 `OPEN` und haengt hart an #19. #27 `OPEN` (PR #170 gemergt;
  `POST /internal/ui/run` produktiv nicht registriert, daher gibt es keinen
  produktiven Web-Mutationspfad). #30 `OPEN` (Software gemergt, Kriterientransfer
  an #190 offen), #190 `OPEN / BLOCKED_HARDWARE`.
- Plattform: ESP32-32E, 4 MB Flash, kein PSRAM, kein OTA. Partitionen
  (`partitions/issue_90_state_store.csv`): `nvs` 24 KiB (Systemdaten/PHY),
  `phy_init`, `factory` 2,9 MiB, **`state_store` 1 MiB (NVS, einziger
  Produktspeicher des `IStateStore`-Adapters)**.
- Messbarer RAM-Rahmen (Hardware-Evidence PR #170/#174): minimaler freier Heap
  unter Weblast 8148 B, Main-Task-Stack-HWM 6056 B. Jede #19-Funktion hat
  deshalb strikt begrenzte, vorab bekannte Puffer; ein ungestreamter Export
  oder Import in der Groessenordnung des Programmkatalogs (bis 32768 B Payload,
  `kMaximumProgramCatalogPayloadBytes`) passt nicht in diesen Rahmen.

Quellen: `BACKUP_SECURITY_RETENTION.md`, `RESOURCE_BUDGET_AND_MAINTENANCE.md`,
`DIAGNOSTICS_AND_MAINTENANCE.md`, `RUN_PERSISTENCE.md`,
`CONFIGURATION_PERSISTENCE.md`, `SETTINGS_AND_STORAGE.md`, `ADOPT_OR_BUILD.md`
(Abschnitte "JSON an externen Grenzen", "Journal, Historie und Import"),
`ADR-009`, `ADR-010`, `ADR-013`, `ADR-016`, `ADR-018`.

## 3. Repository-Audit: Abgedecktes und nachweisbare Luecken

| Anforderung | Bereits vorhanden (Beleg) | Nachweisbare Luecke |
|---|---|---|
| Fehler-/Reset-/Ereignisjournal | Nur der Port `IEventJournal::record(ms, string)` (`lib/device_platform/src/event_journal.hpp`) und ein Testmock; **kein Produktaufrufer, keine Produktimplementierung**. Reset-Ursache wird als Evidenz geliefert (`IResetCauseSource`, "not persisted"). `RESOURCE_BUDGET...` legt fest: der Interlock besitzt keine Fault-Historie (`INTERLOCK_OWNS_FAULT_HISTORY=NO`). | Persistentes, typisiertes, priorisiertes Journal; Bootereignis aus Reset-Ursache; Lebenszyklusereignisse. Der Port ist Freitext; kritische Ereignisse brauchen typisierte Records ohne Freitext (siehe 4.3). |
| Begrenzte Mess-/Laufhistorie | `RunPersistenceSnapshot` ist ausdruecklich nur Run-Domaene; "journal history are outside Issue #17" (`run_persistence_contract.hpp`). | Alles: Fensteraggregate, Laufzusammenfassungen, Aufbewahrung. |
| Aufbewahrung/Bereinigung | Nur Dokumentvertrag. `IStateStore` kennt ausschliesslich `read`/`write` (kein Erase, keine Aufzaehlung, keine Belegungsabfrage); Test-Store `SimulatedPersistentStateStore` kann Powercuts/Kapazitaetsfehler pro Write injizieren, hat aber keine begrenzte Gesamtkapazitaet. | Strukturell begrenzte Ablage mit Prioritaetsklassen, idempotente Bereinigung, kapazitaetsbegrenzter Testspeicher. |
| Laufexport JSON/CSV | `web_json_codec` (cJSON, privat gekapselt) fuer `/api/v1/status|temperatures|alerts`; kein Export. #17 haelt nur den **aktiven** Lauf (`RunPersistenceSnapshot`: Programmsnapshot, Revisionen, Prozesszustand); nach Abschluss/Neustart (`NoActiveRun`) sind Programm, Revisionen sowie Phasen-/Ereignisverlauf nicht mehr vorhanden, und es gibt keinen Mehrlauf-Verlauf. | Begrenztes persistiertes Laufarchiv aus den kanonischen Run-Uebergaengen (4.4), Export-Writer (JSON, CSV) hinter einem kleinen Chunk-Writer-Vertrag (4.5), Zeit-/Qualitaetscodierung, Groessengrenze, Redaction. |
| Normales Backup | Konfigurationsdokumente sind typisiert (`UserConfiguration`, `ServiceConfiguration` Schema 3, `ProgramCatalog`); `ChangeOperation::BackupImport` existiert als Wire-ID, hat **keinen Produzenten**. Doku: kein portables Format ("wird mit Issue #19 implementiert"). | Portables, versioniertes, secret-freies Format (Whitelist-Projektion). |
| Validierter Import | Mutationspfad mit fluechtiger Vorschau und atomarem Commit existiert: `ConfigurationService::beginPreview` -> `installPreview(origin, operation)` -> `validatePreviewForConfirmation` -> `confirmPreview` (Active/Fallback, ein persistenter Linearisierungspunkt, ADR-018). Programmaenderungen pruefen bereits Run-Evidenz (`applyProgramEdit`, `makeFermentationUiProgramUsageEvidence`); alle Anwendungseinstiege laufen ueber `ApplicationCallSerializer`. | Parser/Validator/Migration fuer externe Kandidaten, Konflikt-/Vorschauprojektion, atomare Lauf-/Import-Entscheidung im vorhandenen `ApplicationCallSerializer`-Guard, Groessenpfad (O5). |
| Werksreset | Kern vorhanden: `ConfigurationRecoveryService::beginAuthorizedFactoryReset`, `FermentationApplication::beginAuthorizedFactoryReset` (Auth-Gate drainieren, Websessions widerrufen, Auth-Zustand zuruecksetzen, Run-Persistenz-Epochenuebergabe), wiederaufnehmbar ueber `BootstrapState::Resetting`. **Touchkalibrierung bleibt erhalten** und ist getestet (`test_factory_reset_advances_epoch_and_preserves_touch_key`, `..._preserves_real_touch_calibration_record`). | Produktiver Aufrufer fehlt (ausser Testharness `issue_90_slice7`); lokaler mehrstufiger Bestaetigungsablauf, Aktor-AUS-Vorbedingung, Ablehnung entfernter Ausloeser; Behandlung der neuen #19-Domaenen (4.8). Das SAFE_BOOT-Capability-Modell der Touch-Workspace-Schicht kennt `PersistentFactoryReset` nur als Zielbezeichnung. |
| Geheimnisse | Epochengebundene Connectivity-/Authentication-Domaenen mit eigenen Records (`cc0`, Typ 9; Typ 11/12), nie in Konfigurationsdokumenten. SoftAP-Passwort ist fluechtig (pro Start neu). | Nur Nachweis: Whitelist-Projektionen und Sentinel-Tests, dass keine Ausgabe Geheimnisse enthaelt. |

Bereits belegte **Wiederverwendung** (nichts davon wird neu erfunden):
`IStateStore` + Envelope V1 + `StorageEpoch` + Slotmechanik, ADR-016-Schluesselraum
(ASCII, <= 15 Zeichen), `ConfigurationService`-Vorschau/Commit,
`ConfigurationRecoveryService`-Reset, Run-Identity (#144), UTC-/Zeitqualitaet
(#126), cJSON-Codecgrenze, Crc32-Hilfen, `FakeDs18b20Bus`-artige Testhilfen im
`device_platform_test_support`. Belegte Record-Typ-IDs: 1–12 (naechste freie: 13).

## 4. Zielarchitektur und Vertraege

### 4.1 Modulzuordnung (ADR-013)

- `device_platform` (app-neutral): `BoundedRecordRing` – feste Anzahl Slots ueber
  `IStateStore`, kein Fachbegriff.
- `fermentation_app`: Ereigniskatalog (typisiert), Journal-/Verlaufsdienst,
  Export-Writer, Backup-Projektion, Import-Service, Reset-Ablauf.
- `device_platform_test_support`: kapazitaetsbegrenzter Testspeicher.
- **Kein neuer ESP-IDF-Adapter.** Der vorhandene generische `NvsStateStore` wird
  unveraendert genutzt; einzige produktive Beruehrung ausserhalb der Libraries
  ist die budgetgesperrte Komposition in `main/app_main.cpp` (O7).
- Der Architekturcheck (`check_architecture_boundaries.py`) wird nur erweitert,
  wo neue Dateien Rollenneutralitaet oder Abhaengigkeitsgrenzen betreffen.

### 4.2 Espressif-/Repository-first-Pruefung (Papierpruefung)

| Kandidat | Befund | Entscheidung |
|---|---|---|
| NVS (`IStateStore`-Adapter) | vorhanden, Wear-Leveling und Eintrags-CRC, ADR-016 accepted | **verwenden** |
| `espressif/cjson` | gepinnt, privat in `fermentation_app` | **verwenden** an Export-/Import-Grenze; kein Zweitcodec |
| LittleFS / FAT mit Wear-Levelling | benoetigt Partitionstabellenaenderung und Komponente; `RUN_PERSISTENCE.md` verweist die Aufteilung NVS/LittleFS/Ring auf #19 | **nicht vorgewaehlt**; nur ueber O1-B/C |
| ESP-Diagnostics/Insights, Core Dump | cloud-/telemetriegebunden bzw. eigene Partition; widersprechen lokal-first und dem 4-MB-Layout ohne Zusatzpartition | **nicht verwenden**; nicht vertieft |
| ESP-IDF-Log | fluechtig | kein Journal |
| CSV | triviales Zeilenformat | **kein Fremdcode**, eigener schmaler Writer |

### 4.3 Journal und Ringvertrag (kritisch vs. Information)

**Klassen.** Zwei getrennte Journalklassen mit festen, disjunkten
Schluesselbereichen: **Kritisch** (Reset-/Brownout-/Watchdog-/Panic-Boot,
Fault-/Verriegelungsereignisse, Lauf Start/Ende/Abbruch/Unterbrechung/Recovery-
Ergebnis, Werksreset, Importaktivierung, Persistenzfehler) und **Information**
(Diagnose-/Hinweisereignisse, Bereinigungsprotokoll). Ein Klassenring verdraengt
nur eigene aelteste Eintraege; Komfortklassen ueberschreiben nie kritische Slots.
Das ist die logische Prioritaet. **Die physische Schreibreserve in der gemeinsamen
NVS-Partition folgt nicht aus getrennten Schluesselnamen** und wird deshalb
gesondert nachgewiesen (4.4, "Kapazitaetsreserve").

**Typisierung.** Kritische Eintraege sind typisiert ohne Freitext (Ereigniscode,
Zeitbasis, Laufidentitaet falls vorhanden, kleine Zaehler/Codes); damit kann kein
Geheimnis ueber eine Meldung in Speicher oder Export gelangen. Der bestehende
`IEventJournal`-Port bleibt unveraendert; die Produktimplementierung bedient die
Informationsklasse (Nachricht normalisiert und gekuerzt) und bietet fuer die
kritische Klasse eigene typisierte Methoden.

**Record/Schluessel.** Je Slot ein Envelope V1 (neue Record-Typ-ID(n) ab 13,
`versionValue` = monotone Sequenz, `StorageEpoch` im Envelope). Schluessel:
`<Praefix><Index>` (ASCII, <= 15 Zeichen, ADR-016).

**Garantien – nur was Port und Backend belegen.** `IStateStore` garantiert fuer
einen unterbrochenen Write kein OLD/NEW. Ein spaeterer Read desselben Slots kann
einen vollstaendigen Record, `NotFound`, `ReadError`, `CapacityError` oder
beschaedigte Bytes liefern; auch ein NVS-interner Vorgang (z. B. Seitenbereinigung)
ist auf Portebene nicht als Nachbarslot-isoliert belegt. Der Ring behauptet daher
**nur** Folgendes:

1. *Totale, deterministische Klassifikation.* Jeder Slot wird beim Scan genau
   einer Klasse zugeordnet: `Valid(seq)` (Envelope, CRC, Typ, Schema und aktuelle
   `StorageEpoch` stimmen), `Absent` (`NotFound`), oder `Invalid` (`ReadError`,
   `CapacityError`, CRC-/Laengen-/Typfehler, fremde oder unbekannte Epoche/Schema).
2. *Kopfermittlung ohne eigenen Zeiger.* Der Scan liest alle N Slots einer Klasse.
   Mindestens ein `Valid` -> naechste Sequenz = hoechste `Valid`-Sequenz + 1, Zielslot
   = `Sequenz mod N`. Gleiche Sequenz mit abweichenden Bytes in zwei Slots ->
   beide gelten als `Invalid` (keine Raterei). Nur `Absent` -> leerer Ring, Start
   bei Sequenz 1. Mindestens ein `Invalid` und kein `Valid` -> Kopf nicht
   bestimmbar: die Klasse ist `Degraded` (und `criticalJournalReliable()` falsch);
   der naechste Schreibvorgang ist ein kritisches `JournalReinitialized` bei
   Sequenz 1 an Slot 0. Erst eine verifizierte Schreibung hebt `Degraded` auf. Die
   ungueltigen Altbytes sind nicht verwertbar; es gibt keinen Rueckgriff.
3. *Schreiben mit Verifikation.* Nach jedem Write wird der Zielslot zurueckgelesen
   und gegen die beabsichtigte Sequenz geprueft. `Success` + gleiche Sequenz =
   protokolliert. Alles andere (inkl. `CommitOutcomeUnknown` mit Readback
   `NotFound`/`ReadError`/anderer Sequenz) = *nicht protokolliert*; der Eintrag
   bleibt in der RAM-Warteschlange, der naechste Versuch verwendet **dieselbe
   Sequenz am selben Slot** (idempotent). `WriteError`/`CapacityError` =
   sicher nicht wirksam, ebenfalls Wiederholung bzw. Degradierung.
4. *Zuverlaessigkeit.* `criticalJournalReliable()` gilt nur, wenn der letzte Scan
   abgeschlossen ist, die Klasse nicht `Degraded` ist, kein kritischer Eintrag
   unprotokolliert in der Warteschlange wartet, kein kritischer Warteschlangen-
   Ueberlauf unprotokolliert ist und die letzte kritische Schreib-
   Verifikation erfolgreich war. Einzelne beschaedigte Altslots (`Invalid` bei
   gleichzeitig vorhandenem `Valid`) machen den Ring nicht unzuverlaessig, werden
   aber gezaehlt und als kritisches Ereignis gemeldet.
5. *Verlustgrenze.* Verloren gehen koennen der gerade geschriebene Eintrag und –
   falls die Ueberschreibung des aeltesten Slots nicht abgeschlossen wird – dieser
   aelteste Eintrag. Eine Aussage, dass **andere** Slots unberuehrt bleiben, ist eine
   Eigenschaft des Testdoubles, kein Nachweis fuer das echte NVS-Backend; die
   Korrektheit der Klassifikation und des Kopfes haengt nicht davon ab. Das
   reale Verhalten ist `HW-19` (`NOT_RUN`).

**Schreibkontext.** Keine neue Schleife und kein neuer Task. Ereignisse werden in
einer statisch begrenzten RAM-Warteschlange gesammelt und im vorhandenen
niederprioren Hauptschleifenschritt geschrieben; Regelung und Safety warten nie
auf einen Flashzugriff. Ein Warteschlangenueberlauf zaehlt und wird, sobald
schreibbar, als kritisches `EventsDropped` protokolliert (und macht
`criticalJournalReliable()` bis dahin falsch).

### 4.4 Verlauf, Laufarchiv, Zusammenfassungen, Aufbewahrung

**Grundsatz.** Der aktive Lauf (Kontrollpunkte, `RunPersistenceSnapshot`) bleibt
vollstaendig im #17-Owner; #19 schreibt dort nichts und fuehrt **keine parallele
Run-Wahrheit**. #17 haelt aber nur den *aktiven* Lauf; nach Abschluss oder
Neustart ist das Programm-/Revisions-/Phasen-/Ereignismaterial dort nicht mehr
vorhanden (`variant = NoActiveRun`). Fuer einen Export abgeschlossener Laeufe
werden deshalb die kleinsten begrenzten, persistierten Daten vorgesehen, die
**aus den kanonischen Run-Uebergaengen abgeleitet (kopiert), nie rekonstruiert**
werden.

**Laufarchiv je Laufplatz (Detail).** Ein Laufplatz besteht aus:

| Teil | Inhalt (jeweils aus der kanonischen Quelle kopiert) | Begrenzung |
|---|---|---|
| Kopf | Lauf-ID (#144), Sensormodus, `RunProgramSnapshot` bzw. `ManualRunPlan`, Startzeit mit Zeitqualitaet | durch die vorhandenen Dokument-/Programmgrenzen |
| Revisionen | effektive Laufrevisionen (`RunRevision`) | hoechstens `kMaximumRunRevisions` = 32 |
| Ereignisliste | typisierte Phasenwechsel, Unterbrechungen, Recovery-Ergebnisse, Warnungen/Fehler (Code, Zeit), Korrekturen | hoechstens `kMaximumArchivedRunEvents` (`TBD_IMPLEMENTATION_BUDGET`); bei Ueberschreitung zaehlt `eventsTruncated`, es wird nie still verworfen |
| Zeitreihen | Fensteraggregate (Min/Mittel/Max je Kanal Schrankluft, Produkt, Kuehlkoerper, Sensor-/Gueltigkeitsstatus, Phase) in Chunks | `MaxPunkte` (`TBD_IMPLEMENTATION_BUDGET`) durch Konstruktion |
| Ende | Endzeit mit Zeitqualitaet, Endgrund (`Completed`/`Aborted`/…), Zaehler | fest |

**Uebergabezeitpunkte aus dem Run-Owner.** Die Archivdaten entstehen an den
bereits vorhandenen, erfolgreich persistierten Run-Uebergaengen
(`persistCommand`/`persistTransition`/Checkpoint-Erfolg), indem die Anwendung die
bereits validierte Projektion kopiert, nicht neu berechnet:

1. *Start:* nach erfolgreichem Start-Checkpoint wird der Kopf geschrieben.
2. *Revisionsaenderung / Phasenwechsel / Unterbrechung / Recovery-Ergebnis /
   Warnung / Fehler:* nach dem jeweiligen erfolgreichen Run-Uebergang wird ein
   Ereignis (bzw. die Revisionsliste) angehaengt.
3. *Ende:* nach erfolgreichem Terminal-Uebergang wird das Ende geschrieben und
   die Zusammenfassung erzeugt.

Ein Archivschreibfehler beendet oder stoppt den Lauf nie (Komfortdaten); er setzt
`archiveIncomplete` fuer diesen Lauf. Wird ein Lauf unterbrochen/neu gestartet,
ohne dass ein Ende geschrieben wurde, markiert der naechste Scan ihn als
*Ende unbekannt*; der Export gibt das so aus. Es werden keine Daten erfunden.

**Zeitreihenbegrenzung.** Die Fensterlaenge folgt aus der programmierten
Laufdauer: `windowMinutes = ceil(Dauer / MaxPunkte)`, mindestens 1 Minute; die
Dauer ist durch `kMaximumFermentationDurationMinutes` (20160 min) und die
Programmgrenzen beschraenkt. Es gibt keinen RAM-Puffer ueber den ganzen Lauf; nur
das aktuelle Fenster liegt im RAM und wird bei Fensterwechsel bzw. Checkpoint
chunkweise geschrieben. Powercut-Verlust: hoechstens das ungeflushte Fenster.

**Ringe.** Detail: Ring von 5 Laufplaetzen. Zusammenfassungen: Ring von 50 Plaetzen
(Lauf-ID, Programmbezug, Start/Ende mit Zeitqualitaet, Endgrund, Zaehler,
Min/Mittel/Max). Ringregeln wie 4.3 (Klassifikation, Kopf, Verifikation). Die
Zusammenfassung wird beim Laufende **vor** dem Ueberschreiben des aeltesten
Detailplatzes geschrieben.

**Bereinigung (proaktiv, idempotent, wiederaufnehmbar).** Reihenfolge wie
`BACKUP_SECURITY_RETENTION.md`: (temporaere Exportdaten entfallen, der Export
streamt) -> aelteste Diagrammdetails -> Detailplatz zu Zusammenfassung -> aelteste
nichtkritische Zusammenfassung. Ueberschreiben des jeweils aeltesten Slots ist der
einzige Loeschvorgang (kein Erase, kein Portumbau, konstanter Bedarf). Zwischen
den Schritten gilt "Zusammenfassung zuerst, dann Detail ueberschreiben"; bei
Unterbrechung bleiben beide stehen und der naechste Lauf der Bereinigung setzt
fort.

**Konstanten und Budget (C0/G1).** Aufbewahrung ist in Release 1 fest (O6). Alle
Mengen (Slotzahlen, Chunkgroesse, `MaxPunkte`, `kMaximumArchivedRunEvents`,
Journalgroessen) sind `TBD_IMPLEMENTATION_BUDGET`; C0 berechnet sie aus den
Programm-/Dokumentgrenzen, der Owner setzt sie bei G1, und sie sind nie geratene
Laufzeitwerte. Passt der vollstaendige Archivumfang (inkl. Ereignis- und
Revisionsverlauf) fuer 5 Detailplaetze + 50 Zusammenfassungen + Journale nicht in
den freigegebenen Anteil, legt C0 die konkrete Scope-/Budgetentscheidung O10 vor;
es wird nichts stillschweigend gekuerzt.

**Kapazitaetsreserve in der gemeinsamen Partition (C0/G1-Nachweis).** Die
Partition `state_store` (1 MiB NVS) wird von Konfigurationsgraph, Laufpersistenz,
Credentials/Auth, Touchkalibrierung, Bootstrap und kuenftig #19 geteilt. C0 weist
fuer das NVS-Seitenmodell der fixierten ESP-IDF-Version (Seiten-/Eintragsgroesse,
Blob-Verteilung, mindestens eine freie Seite fuer Garbage Collection) – aus Quelle
und Dokumentation zitiert, nicht aus dem Gedaechtnis – eine Bilanz nach:
**Bedarf aller bestehenden Domaenen im Maximum + kritische #19-Ringe + NVS-
Garbage-Collection-Reserve + eine explizite kritische Schreibreserve <= Kapazitaet,
auch bei maximaler Belegung aller Komfortklassen.** Die Reserve wird als
Compile-Zeit-Bilanz (`static_assert` ueber die Budgetkonstanten) erzwungen; Komfort-
schreiber duerfen die Bilanz nicht ueberschreiten. Das ist ein Rechenmodell; die
reale NVS-Belegung bleibt `HW-19`.

**Kapazitaetsfehler zur Laufzeit.** Liefert ein Komfort-Write `CapacityError`/
`WriteError`, werden Komfortklassen bis zum naechsten erfolgreichen Scan
ausgesetzt (zaehlend, protokolliert); der Lauf wird wegen Historienknappheit
**nicht** gestoppt. Liefert ein kritischer Write `CapacityError`/`WriteError`,
wird `criticalJournalReliable()` falsch und der **Start** neuer Laeufe blockiert
(4.7); ein laufender Prozess laeuft weiter (`RESOURCE_BUDGET_AND_MAINTENANCE.md`).

### 4.5 Laufexport (JSON/CSV)

- **Ausgabevertrag.** Es gibt im Repository keinen allgemeinen Byte-Stream-Sink:
  `device_platform::IBinaryOutputSink` ist ein *Aktorausgang* (`setEnabled(bool)`)
  und wird fuer den Export **nicht** verwendet. Fuer die Exportgrenze wird nur der
  kleinste notwendige Schreibvertrag in `fermentation_app` definiert:
  `bool ExportChunkWriter::write(const char* data, std::size_t length)` –
  `false` = Senke voll oder abgebrochen, der Export stoppt sofort und liefert
  einen typisierten Abbruchstatus (kein Teilerfolg wird als vollstaendig
  gemeldet). Keine allgemeine I/O-Abstraktionsschicht, keine Aktor-Beruehrung.
  Wie ein Transport (#27) diesen Writer bedient, ist nicht Teil von #19.
- **Streaming.** Erzeuger schreiben mit fester Chunkgroesse; kein vollstaendiges
  Dokument im RAM. Die Maximalgroesse ist aus den Konstanten berechenbar und wird
  vorab gegen die freigegebene Exportgrenze geprueft.
- **Quellen.** Abgeschlossene Laeufe: Laufarchiv (4.4). Aktiver Lauf: kanonische
  Projektion des Run-Owners plus bereits geschriebene Archiv-Chunks. Ein Export
  nach Abschluss/Neustart braucht damit keine rekonstruierten Daten.
- **JSON:** Format-Kennung, Schemaversion, Firmware-/Schemaangaben,
  Programmsnapshot, effektive Laufrevisionen, Phasen-/Ereignisverlauf,
  Zeitreihen, Sensorqualitaet, Abschlussgrund (bzw. *Ende unbekannt*,
  `archiveIncomplete`, `eventsTruncated`). **CSV:** Zeitbezug und Zeitqualitaet,
  Soll-/Istwerte der drei Kanaele, Sensorstatus, Phase, relevante Aktorereignisse.
- **Fehlende Werte werden als fehlend codiert** (JSON `null`, CSV leer), nie als
  `0`. Der Export liest ausschliesslich aus #19-Records und der Laufprojektion
  und kann strukturell keine Geheimnisse sehen (Whitelist). Ein Teilumfang gemaess
  O10 wird im Exportdokument selbst ausgewiesen.

### 4.6 Backup und Import

- **Bundle:** ein JSON-Dokument aus Format-Kennung, Schema-/Revisionsangaben,
  Benutzereinstellungen, Benutzerprogrammen, Auswahl der Standardprogramme,
  UI-/Sprach-/Zeitzoneneinstellungen und den ausdruecklich erlaubten
  Serviceparametern (O3), plus Integritaetspruefsumme (CRC-32, kein Manipulations-
  schutz). Nicht enthalten: WLAN-/Web-/PIN-Geheimnisse und Pruefnachweise,
  Sitzungen/Tokens/CSRF, Schluessel, Touchkalibrierung, rohe Flashdaten, rohe
  interne Envelopes. Die Ausgabe ist deterministisch (feste Feldreihenfolge) und
  streamt ueber denselben `ExportChunkWriter`.
- **Importablauf** (verbindlich, `BACKUP_SECURITY_RETENTION.md`): Groesse/Format
  pruefen -> Schema identifizieren -> Integritaet -> getestete Migration ->
  **vollstaendige typisierte Validierung** (Bereichsgrenzen, unbekannte kritische
  Felder = Ablehnung, nicht unterstuetzte IANA-Zeitzone = Ablehnung, kein stiller
  Rueckfall) -> Vorschau/Konflikte -> ausdrueckliche Bestaetigung -> Commit. Der
  Kandidat wird als `ConfigurationCommitCandidate` in den **vorhandenen** Pfad
  `beginPreview`/`installPreview(origin, ChangeOperation::BackupImport)`/
  `confirmPreview` gegeben. Kein zweiter Aktivierungspfad, kein Pending, kein
  paralleler Active-Zweig; bis zum Commit bleibt alles fluechtig.
- **Atomare Lauf-/Import-Entscheidung ueber das vorhandene Application-Gate.**
  Drei zeitlich getrennte Pruefungen allein schliessen kein Rennen. Der
  bindende Test liegt deshalb **innerhalb desselben `ApplicationCallSerializer`-
  Guards** (`applicationCallSerializer_.enter()`, rekursiver Mutex), den die
  Anwendungseinstiege – darunter Konfigurationsaenderungen wie `applyProgramEdit`
  und die Run-Kommandos – bereits verwenden: Ein neuer Anwendungseinstieg
  `confirmBackupImport(handle)` betritt den Guard, wertet den Lauf-Zustand
  (`NoActiveOrRecoverableRun`, sonst Ablehnung und `cancelPreview`) aus und ruft
  `ConfigurationService::confirmPreview` **ohne den Guard zu verlassen**. Ein
  konkurrierender Runstart ist ueber denselben Guard entweder vollstaendig vor
  der Pruefung (dann Ablehnung des Imports) oder nach dem Commit (dann Start auf
  der neuen Konfiguration) serialisiert. Die frueheren Pruefungen bei Annahme und
  Vorschau sind reine Fruehabbrueche fuer die Bedienung. Es gibt keinen zweiten
  Lock-/Mutation-Owner. Voraussetzung, die C5 zuerst belegt: der Runstart-Pfad
  betritt den Guard; andernfalls ist das ein Stoppbefund an den Plan.
- **Geheimnisse:** Fehlende Geheimnisse ueberschreiben nichts; Factory-Katalog
  und Quellprogramme werden nicht still ueberschrieben; Touchkalibrierung bleibt.
- **Groesse (O5):** C0 erzeugt aus dem Gesamtschema den maximal gueltigen
  Kandidaten und misst Textgroesse und Parser-Spitzenbedarf gegen den RAM-Rahmen
  aus Abschnitt 2. Erst danach faellt die Wahl zwischen Gesamtbody, Chunking mit
  Vorab-Validator oder reduziertem Maximum. cJSON baut den ganzen Baum auf; passt
  das nicht, ist ein Chunking-/Reduktionsmodell der Plananker, kein zweiter Codec.

### 4.7 Startkriterium "Journal verlaesslich" und Aktivierung

`RESOURCE_BUDGET_AND_MAINTENANCE.md` verlangt: kein neuer Lauf, wenn das
Fehlerjournal fuer verriegelte Ereignisse nicht verlaesslich ist. Diese
Anforderung wird **nicht** aufgeweicht.

- *Mechanik:* `criticalJournalReliable()` (4.3, Punkt 4) wird der bestehenden
  Startvorbedingungsliste der Anwendung als zusaetzlicher Ablehnungsgrund
  angehaengt. Kein neuer `FaultCode`, keine neue Interlock-Logik (#24 bleibt
  unveraendert).
- *Zustaende:* `RELIABLE` und `UNRELIABLE` entscheiden ueber den Start.
  `NOT_COMPOSED` (Software vorhanden, nicht in die Produktkomposition
  eingebunden) ist **kein** `RELIABLE`: in diesem Zustand bleibt es beim heutigen
  Produktverhalten (das Kriterium ist im Produkt heute nicht umgesetzt – eine
  bestehende SSOT-Luecke), und der Zustand ist nur zulaessig, solange die
  Komposition nicht freigegeben ist.
- *Aktivierung (O7):* Variante A komponiert das Journal in #19 nach G1 produktiv;
  danach gilt das Kriterium vollstaendig und es gibt keinen `NOT_COMPOSED`-Rest.
  Variante B laesst die Komposition bis zu einem Hardwarenachweis aus und legt
  dem Owner die daraus folgende **befristete SSOT-Abweichung** ausdruecklich vor
  (Vermerk in `OPEN_POINTS.md`, Ende der Abweichung = Komposition). Eine stille
  Ignorierung ist in keiner Variante vorgesehen.

### 4.8 Werksreset und neue Domaenen

- Der Kern bleibt unveraendert. #19 liefert den **lokalen Ablauf**: mehrere
  Bestaetigungsschritte, Aktoren/Peltier AUS als Vorbedingung, nur lokaler
  Ursprung (kein Web-/Remote-Ausloeser), Datenverlust-Warnung, Aufruf von
  `beginAuthorizedFactoryReset`, danach Ersteinrichtungszustand. Vergessene
  Service-PIN hat keinen Bypass; dies ist der einzige Weg.
- Journal-/Verlaufsrecords tragen die `StorageEpoch`. Nach dem Epochenwechsel sind
  alte Records logisch unerreichbar und die Ringe starten neu; ein physisches
  Loeschen wird nicht zugesichert (`BACKUP_SECURITY_RETENTION.md`). Bei O4-B
  wird das kritische Journal ueber eine dokumentierte Epochenuebernahme
  erhalten.
- Touchkalibrierung bleibt erhalten (bestehende Tests); #19 ergaenzt nur eine
  Regression, falls der neue Ablauf diesen Pfad beruehrt.

## 5. Umsetzungs- und Commit-Schnitte

Gate G1 liegt nach C0. C1–C7 laufen danach ohne weitere Ownerfreigabe je Schnitt;
nach jedem Schnitt gelten die gezielten lokalen Tests und der Builder-Self-Check
(AGENTS.md: nur geaenderter Bereich plus direkte Konsumenten). Dateinamen sind
Vorschlaege innerhalb der beschriebenen Module.

| Schnitt | Inhalt | Erwartete Dateien | Nachweis |
|---|---|---|---|
| **C0** (Doku/Messung, **Gate G1**) | Berechnung der Ring-/Chunk-/Archivgroessen aus den Programm-/Dokumentgrenzen; **NVS-Kapazitaetsbilanz mit kritischer Schreibreserve** (4.4) aus der Quelle der fixierten ESP-IDF-Version; Wear-Abschaetzung als Rechnung; maximal gueltiger Importkandidat (Nativ-Messung); Vorlagen O1/O5/O6/O7/O10. | `docs/audits/ISSUE19_STORAGE_BUDGET.md` (+ kleines Hostwerkzeug nur fuer die Messung, ohne Produktcode) | Zahlen reproduzierbar; Owner setzt Konstanten und entscheidet O1/O5/O10 |
| **C1** Ring + Journal | `BoundedRecordRing` (Klassifikation, Kopf, Verifikation, Wiederholung), Ereigniskatalog, kritisch/Information, RAM-Warteschlange, `IEventJournal`-Produktimplementierung, Boot-Ereignis aus Reset-Ursache, `criticalJournalReliable()`; kapazitaetsbegrenzter Teststore mit gemeinsamem Kapazitaetsmodell; Epochenbindung. | `device_platform`: ring; `fermentation_app`: journal; `device_platform_test_support`: bounded store; `test_event_journal*` | SIM-19-01..03, 05, 13, 14 |
| **C2** Laufarchiv + Verlauf + Retention | Archivkopf/Revisionen/Ereignisse/Ende aus den kanonischen Run-Uebergaengen, Fensteraggregate, Detail-/Zusammenfassungsringe, idempotente Bereinigung, Kapazitaetsverhalten, `archiveIncomplete`/*Ende unbekannt*. | `fermentation_app`: run archive/history; Tests | SIM-19-03, 04, 12, 15 |
| **C3** Laufexport | `ExportChunkWriter`, JSON-/CSV-Writer, Groessengrenze, Whitelist, Zeitqualitaet, Fehlend-Codierung, Export nach Abschluss/Neustart. | `fermentation_app`: run export; Tests mit Golden-Dateien | SIM-19-06, 15 |
| **C4** Backup-Export | Whitelist-Projektion, Bundle-Writer, deterministische Ausgabe, CRC. | `fermentation_app`: backup; Tests | SIM-19-07 |
| **C5** Import | Parser/Validator/Migrationstabelle, Kandidatenbau, Vorschau-/Konfliktprojektion, `confirmBackupImport` mit Lauf-Gate im Application-Guard, Aufruf des vorhandenen Preview-/Commit-Pfads. | `fermentation_app`: import; Tests inkl. Powercut-Matrix ueber alle Write-Cutpoints und Konkurrenzfall Start/Import | SIM-19-08..10, 16 |
| **C6** Werksreset-Ablauf | Mehrstufiger lokaler Ablauf, Aktor-AUS-Vorbedingung, Ablehnung entfernter Ausloeser, Praesentationsmodell (O8-A). | `fermentation_app`: reset flow; Tests | SIM-19-11 |
| **C7** Doku/Abnahme | Acceptance-Eintraege, Dokumentsynchronisierung, ROADMAP, statischer Ressourcenbericht; Komposition gemaess O7. | siehe Abschnitt 7 | Review-fertig |

Nach C7: Builder-Self-Check, **Stopp fuer den unabhaengigen Review** (kanonisches
Gate). Pre-Ready erst auf ausdrueckliche Owneranweisung.

## 6. Tests und Nachweise (hardwarefrei)

Nur tatsaechlich ausgefuehrte Tests werden als bestanden gemeldet. Das
Kapazitaetsmodell des Teststores ist ein Rechenmodell, kein NVS-Beweis.

| ID | Pruefung |
|---|---|
| SIM-19-01 | Ring: Reihenfolge, Wrap, Kopfermittlung per Scan nach den Regeln 4.3 (nur `Absent`, `Valid`+`Invalid`, nur `Invalid`/`Degraded`, doppelte Sequenz), fremde Epoche/Schema |
| SIM-19-02 | Schreib-Cutpoint-Matrix (vor Commit, nach Commit/`CommitOutcomeUnknown`, Readback `NotFound`/`ReadError`/`CapacityError`/anderer Sequenz, Slot-Korruption): **kein OLD/NEW wird vorausgesetzt**; geprueft werden Totalitaet und Determinismus der Klassifikation, idempotente Wiederholung mit derselben Sequenz, korrekte `criticalJournalReliable()`-Folge und dass Fehler nie als protokolliert gelten. Optionaler Nachbarslot-Verlust (Testdouble) darf Scan und Kopf nicht ungueltig machen |
| SIM-19-03 | Prioritaet: Komfortklassen bis ueber Kapazitaet fuellen -> kritische Ringe unveraendert; kritischen Ring ueberfuellen -> nur aelteste kritische weichen, in Reihenfolge |
| SIM-19-04 | 5 Detail / 50 Zusammenfassungen; Bereinigung bei jedem Cutpoint wiederaufnehmbar und idempotent; Zusammenfassung vor Detailverwurf |
| SIM-19-05 | `StorageEpoch`-Wechsel: alte Records unerreichbar, Ringe neu (und O4-Variante) |
| SIM-19-06 | Laufexport JSON/CSV Golden; fehlend != 0; Zeitqualitaet; Groessengrenze; Senke meldet `false` -> typisierter Abbruch, nie als vollstaendig gemeldet; **Sentinel-Geheimnisse** nicht in der Ausgabe |
| SIM-19-07 | Backup Golden/deterministisch; Whitelist; Sentinel fuer WLAN-/Web-Passwort, PIN-Nachweis, Tokens, SoftAP-Passwort, Touchkalibrierung |
| SIM-19-08 | Importmatrix: unbekanntes Schema, unbekanntes kritisches Feld, falscher Typ/Bereich, nicht unterstuetzte Zeitzone, ueberlang, CRC-Fehler, abgeschnitten, alte Schemas ueber Migration; **jeder Fehler laesst Store-Bytes und `stateRevision` unveraendert** |
| SIM-19-09 | Import atomar: Powercut an jedem `IStateStore`-Write-Cutpoint des Commits -> nach Neustart exakt alter oder neuer Graph (gemaess ADR-018-Aufloesung), nie gemischt; Lauf-Zustaende aktiv/pausiert/unterbrochen/wiederherstellbar/unbekannt blockieren |
| SIM-19-10 | Import ueberschreibt weder Geheimnisse noch Touchkalibrierung noch Factory-Katalog |
| SIM-19-11 | Werksreset-Ablauf: alle Schritte noetig, Aktoren-AUS-Vorbedingung, Remote abgelehnt, Touchkalibrierung bleibt, Verlauf/Journal gemaess O4 |
| SIM-19-12 | Regelpfad unbeeinflusst: Journal-/Verlaufs-/Exportfehler und -Kapazitaet stoppen keinen Lauf; kritischer Persistenzausfall blockiert nur den **Start** (4.7); kein Flashzugriff im Regelpfad |
| SIM-19-13 | Struktur: kritische Records enthalten keine Freitextfelder |
| SIM-19-14 | Kapazitaet: gemeinsames Kapazitaetsmodell bei maximaler Komfortbelegung -> kritischer Write gelingt (Reserve); `CapacityError` auf kritischem Write -> `criticalJournalReliable()` falsch, Start blockiert, laufender Lauf unberuehrt; `CapacityError` auf Komfortwrite -> Komfort ausgesetzt, Lauf unberuehrt |
| SIM-19-15 | Export nach Abschluss und nach Neustart ohne aktiven Lauf: Programmsnapshot, Revisionen, Ereignisse, Zeitreihen, Ende aus dem Archiv; *Ende unbekannt* nach abgebrochenem Archiv; `eventsTruncated` ausgewiesen; keine rekonstruierten Werte |
| SIM-19-16 | Konkurrenz Start/Import: Lauf-Gate im selben `ApplicationCallSerializer`-Guard; deterministisch (a) Start vor Pruefung -> Import abgelehnt, Preview geschlossen, Store unveraendert; (b) Import-Commit vor Start -> Start sieht die neue Konfiguration; Thread-/Hook-Test ohne zweiten Lock-Owner |

Zusaetzlich je Schnitt: Format-/Tidy-Self-Check, Architekturcheck (+ Selftest),
und fuer alles, was Library-Quellen beruehrt, Build beider ESP-IDF-Profile. Ein
Build ersetzt keinen Hardwarenachweis.

**Nicht hardwarefrei beweisbar (HW-19-xx, siehe O9):** reale NVS-Wear und
Schreiblatenz der Ringe, reale Kapazitaets-/GC-Reserve der gemeinsamen Partition,
Powercut auf echtem Flash (inkl. Nachbarslot-Verhalten), Heap-/Stack-Spitze beim
Export/Import auf dem Geraet, Timing der Hauptschleife unter Schreiblast. Diese
Punkte bleiben `NOT_RUN`.

## 7. Dokumentationswirkung und ADR

- `BACKUP_SECURITY_RETENTION.md`: konkretes Bundle-/Exportformat, Aufbewahrung
  (O6), Resetdetails (O4).
- `RUN_PERSISTENCE.md`: Aufteilung Journal/Verlauf/Zusammenfassung; die Zeile
  "Aufteilung NVS/LittleFS/Ring wird in #19 festgelegt" wird aufgeloest.
- `RESOURCE_BUDGET_AND_MAINTENANCE.md` / `OPEN_POINTS.md`: freigegebene
  Budgetkonstanten.
- `CONFIGURATION_PERSISTENCE.md`: Nicht-Scope-Zeile zu "Backupformat, Journale und
  Aufbewahrung" auf den Ist-Stand bringen.
- `ACCEPTANCE_TESTS.md`: SIM-19-01..13 mit geprueften Testnamen; HW-19-xx als
  `NOT_RUN`.
- `DECISIONS.md`: **ADR-020** nur bei O1-Entscheid ("Verlaufs-/Journalspeicher");
  ohne Ownerfreigabe wird keine ADR geschrieben.
- `docs/ROADMAP.md` bei Planfreigabe, jedem Schnitt-Merge und Abschluss.

## 8. Risiken

| Risiko | Wirkung | Gegenmassnahme |
|---|---|---|
| RAM-Rahmen (8148 B Minimum unter Weblast) | Export/Import koennen nicht ganze Dokumente halten | Streaming mit `ExportChunkWriter`, feste Puffer, O5 nach C0-Messung |
| Gemeinsame NVS-Partition | Komfortdaten koennten die physische Schreibreserve der kritischen Ringe verbrauchen | C0-Kapazitaetsbilanz mit kritischer Reserve und GC-Reserve, Compile-Zeit-Bilanz, SIM-19-14; reale Belegung `HW-19` |
| Keine OLD/NEW-Garantie des Stores | Ring koennte staerkere Garantien vortaeuschen | nur Klassifikation/Verifikation/idempotente Wiederholung (4.3), SIM-19-02 ohne OLD/NEW-Annahme |
| Archivumfang gegen Budget | vollstaendiger Export evtl. nicht moeglich | C0-Rechnung, O10 mit konkreten Alternativen, keine stille Kuerzung |
| Archiv-Hook an Run-Uebergaengen | Rueckwirkung auf Run-Owner/Regelpfad | nur Kopie der validierten Projektion nach Erfolg, Schreiben im niederprioren Schritt, Fehler stoppt nie den Lauf |
| Import-/Start-Rennen | Aktivierung waehrend eines neuen Laufs | Lauf-Gate im vorhandenen `ApplicationCallSerializer`-Guard, SIM-19-16; Stoppbefund, falls der Runstart den Guard nicht betritt |
| NVS-Wear und Schreiblatenz | Schreiben nahe am Regelpfad | Schreiben nur im niederprioren Schritt, Fensterflush, Wear als Rechnung (C0) und `HW-19` |
| Dauer-/Punkteexplosion (bis 14 Tage) | unbegrenzter Detailbedarf | Fensterlaenge aus Laufdauer, `MaxPunkte` als Budgetkonstante |
| Startkriterium beruehrt Startvorbedingungen | unbeabsichtigte Sperre aller Laeufe bzw. Aufweichen des SSOT | O7: kein neuer FaultCode, `NOT_COMPOSED` != `RELIABLE`, Abweichung nur ausdruecklich vorgelegt |
| Schluesselraum | NVS-Schluessel <= 15 Zeichen, Kollisionsrisiko | feste Praefixe und Indizes, Review gegen vorhandene Schluessel (`cb0/cb1`, `cc0`, `tc0/tc1`, Run-/Konfigurationsslots) |
| Kein Hardwarenachweis | Annahmen zu Wear/Latenz unbelegt | O9, ausdruecklich `NOT_RUN` |
| Fruehere Entwurfsinhalte nicht auffindbar | moegliche verlorene Anforderungen | Plan stuetzt sich auf Issue-Scope und kanonische Dokumente; Reviewer vergleicht, falls der Owner den Entwurf liefert |

## 9. Grenzen

- **#28** bleibt Diagnose-/Service-/Exportgate und nutzt #19-Daten; #19 liefert
  weder Diagramme noch Diagnoseexport noch Serviceablauf.
- **#27**: Web-Transport und -Routen fuer Laufexport, Backupdownload und Import
  folgen dort; #19 vervollstaendigt #27 nicht (Scope-Abgrenzung in Abschnitt 0).
- **Hardware**: keine Hardwaretests, keine Aktorfreigabe, kein Flash-Geraet;
  `ACTUATOR_RELEASE=NO`.
- Release-1-Abgrenzung (`SPECIFICATION_REVIEW.md`): kein OTA, kein
  Roh-Flash-Backup ueber das Web.

## 10. Checkliste Planfreigabe

- [ ] Plan-Fix-Verification ohne offene Blocker
- [ ] Ownerentscheide O1, O3, O4, O6, O7, O8 (vor C1 bzw. C5/C6 wo betroffen), O9; O1 und O5 sowie gegebenenfalls O10 spaetestens bei G1
- [ ] exakte Plan-SHA vom Owner freigegeben (G0)
- [ ] G1 nach C0 (Budgetzahlen, Kapazitaetsbilanz, O1/O5/O10)
