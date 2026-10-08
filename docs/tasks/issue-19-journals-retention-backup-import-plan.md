# Plan Issue #19 – Journale, Aufbewahrung, Bereinigung, Backup und Import

```text
PLAN_REVISION=1 (neue vollstaendige Planrevision auf main nach PR #189)
PLAN_STATUS=DRAFT_AWAITING_INDEPENDENT_REVIEW_AND_OWNER_APPROVAL (exakte Plan-SHA steht im Draft-PR)
ISSUE=19 (E2.4), Epic #4
BASE_MAIN=9beb68f1935f80c6d2a59b5a612d542e5d9109a7 (PR #189 gemergt am 2026-10-08)
TOOLCHAIN=ESP-IDF v6.1 (fff9895c82d744c7237be8847347bdd1b07c6643)
EARLIER_DRAFT=REVIEW_DRAFT - PRESERVE, NOT APPROVED, NOT CANONICAL (im Repository und in allen PRs nicht auffindbar; dieser Plan stuetzt sich nicht darauf)
SCOPE_OF_THIS_COMMIT=NUR_PLAN_UND_ROADMAP_BEREINIGUNG (keine Produktionslogik, keine produktiven Tests, keine Schemas)
IMPLEMENTATION=NOT_STARTED
HARDWARE=NOT_RUN
ACTUATOR_RELEASE=NO
```

## 0. Owner-Entscheidungen und Gates

Eine Entscheidung gilt nur als getroffen, wenn sie hier ausdruecklich so steht.
Die Freigabe der exakten Plan-SHA ist davon getrennt. Die Empfehlungen sind
begruendet, aber keine stillen Defaults; ohne Ownerentscheid wird der jeweilige
Schnitt nicht begonnen.

| ID | Entscheidung | Alternativen | Empfehlung | Stand |
|---|---|---|---|---|
| O1 | Speicherstruktur fuer Journal und Verlauf | **A** feste Ringslots im vorhandenen `state_store`-NVS ueber `IStateStore` (kein Portumbau, kein Erase); **B** eigene LittleFS-/Datenpartition (Partitionstabelle, neue Komponente, ADR-016-Variante B); **C** Hybrid (Journal NVS, Verlauf LittleFS) | **A** | **OFFEN** (gate fuer C1; bei A mit neuer ADR, siehe 7) |
| O2 | Transportgrenze | **A** #19 liefert anwendungsinterne Erzeuger/Verbraucher (Export-Writer, Import-Service, Reset-Ablauf) plus Nativtests; HTTP-/Touch-Anbindung folgt in #27/#28; **B** #19 registriert zusaetzlich Web-Routen/Touch-Screens | **A** | **OFFEN** |
| O3 | Geraetegebundene Daten im normalen Backup (`sensorCommissioning` mit ROM-Bindung, Planerparameter) | **A** ausschliessen (nur nutzerbezogene Daten und ausdruecklich erlaubte Serviceparameter); **B** einschliessen mit Warnung; **C** einschliessen nur mit zusaetzlicher Bestaetigung | **A** (ROM-Bindung eines Geraets auf einem anderen Geraet waere eine falsche feste Bindung) | **OFFEN** |
| O4 | Verbleib des kritischen Journals nach lokalem Werksreset | **A** wie Verlauf per `StorageEpoch` logisch unerreichbar (entspricht `BACKUP_SECURITY_RETENTION.md`: Reset loescht Laufhistorie und Fehler-/Komforthistorie "soweit vorgesehen"); **B** kritisches Reset-/Sicherheitsjournal bleibt erhalten | **A** | **OFFEN** (Dokumente sind hier mehrdeutig) |
| O5 | Importtransport/-groesse | **A** begrenzter Gesamtbody; **B** chunkweises Streaming mit eigenem Vorab-Validator; **C** reduziertes Maximum (weniger Benutzerprogramme/Notizen im Backup) | **keine Vorabwahl**: Wahl nach C0-Messung des maximal gueltigen Kandidaten (`ADOPT_OR_BUILD.md`, "JSON an externen Grenzen") | **OFFEN, an C0 gebunden** |
| O6 | Aufbewahrung in Release 1 | **A** feste Konstanten 5 Detail / 50 Zusammenfassungen; **B** innerhalb Budget konfigurierbar (wie `BACKUP_SECURITY_RETENTION.md` beschreibt) | **A** (KISS; Konfigurierbarkeit ist spaetere Erweiterung ohne Schemabruch, weil die Grenzen nicht ins Wireformat eingehen) | **OFFEN** (Abweichung vom Dokumentwortlaut) |
| O7 | Aktivierung vor gemessenem Budget | **A** Budgetsperre wie #30: Journal/Verlauf bleiben aus (`NOT_ENABLED`), bis die Konstanten freigegeben sind, und das "Journal verlaesslich"-Startkriterium wird dann nicht ausgewertet; **B** harte Sperre neuer Laeufe ohne Journal | **A**; B wuerde jeden Lauf des heutigen Produkts sperren | **OFFEN** (beruehrt die Startbedingungen, siehe 4.7) |
| O8 | Werksreset-Bedienung | **A** nur der anwendungsinterne mehrstufige Ablauf samt Praesentationsmodell; **B** zusaetzlich LVGL-Bildschirme | **A** | **OFFEN** (haengt an O2) |
| O9 | Hardware-Folgeissue | **A** Folgeissue fuer reale NVS-Wear-/Powercut-/Heap-/Timing-Verifikation (HW-19-xx) anlegen, wie #190 zu #30; **B** Hardwarenachweise bleiben in #19 offen | **A** | **OFFEN**; der Agent legt kein Issue ohne Ownerauftrag an |

Gates: **G0** Freigabe dieser Plan-SHA; **G1** nach C0 (Budgetzahlen, O1, O5);
danach Stopp nach jedem Commit-Schnitt bis zur Ownerfreigabe des naechsten.

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
- Keine Web-Routen und keine Touch-Bildschirme (O2/O8), keine #27-Mutationspfade.
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
| Laufexport JSON/CSV | `web_json_codec` (cJSON, privat gekapselt) fuer `/api/v1/status|temperatures|alerts`; kein Export. | Export-Writer (JSON, CSV), Zeit-/Qualitaetscodierung, Groessengrenze, Redaction. |
| Normales Backup | Konfigurationsdokumente sind typisiert (`UserConfiguration`, `ServiceConfiguration` Schema 3, `ProgramCatalog`); `ChangeOperation::BackupImport` existiert als Wire-ID, hat **keinen Produzenten**. Doku: kein portables Format ("wird mit Issue #19 implementiert"). | Portables, versioniertes, secret-freies Format (Whitelist-Projektion). |
| Validierter Import | Mutationspfad mit fluechtiger Vorschau und atomarem Commit existiert: `ConfigurationService::beginPreview` -> `installPreview(origin, operation)` -> `validatePreviewForConfirmation` -> `confirmPreview` (Active/Fallback, ein persistenter Linearisierungspunkt, ADR-018). Aenderungen waehrend eines aktiven Laufs werden bereits abgelehnt. | Parser/Validator/Migration fuer externe Kandidaten, Konflikt-/Vorschauprojektion, dreifache Lauf-Gate-Pruefung (Annahme, Vorschau/Bestaetigung, unmittelbar vor Commit), Groessenpfad (O5). |
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

### 4.3 Journal (kritisch vs. Information)

- Zwei getrennte Klassen mit **festen, disjunkten Schluesselbereichen**:
  **Kritisch** (Reset-/Brownout-/Watchdog-/Panic-Boot, Fault-/Verriegelungs-
  ereignisse, Lauf Start/Ende/Abbruch/Unterbrechung/Recovery-Ergebnis, Werksreset,
  Importaktivierung, Persistenzfehler) und **Information** (Diagnose-/Hinweis-
  ereignisse, Bereinigungsprotokoll).
- Ein Klassenring verdraengt nur eigene aelteste Eintraege. Komfortdaten koennen
  kritische Daten daher **nicht** verdraengen – die Prioritaet ist strukturell,
  nicht ablaufabhaengig.
- Kritische Eintraege sind **typisiert ohne Freitext** (Ereigniscode, Zeitbasis,
  Laufidentitaet falls vorhanden, kleine Zaehler/Codes). Dadurch kann ein
  Geheimnis nicht ueber eine Meldung in den Speicher oder in einen Export
  gelangen. Der bestehende `IEventJournal`-Port bleibt unveraendert; die
  Produktimplementierung implementiert ihn fuer die Informationsklasse
  (Nachricht gekuerzt/normalisiert) und bietet fuer die kritische Klasse eigene
  typisierte Methoden.
- Ringeintraege sind je ein Envelope V1 (neue Record-Typ-ID(n) ab 13,
  `versionValue` = monotone Sequenz, `StorageEpoch` im Envelope). Schluessel:
  `<Praefix><Index>` (ASCII, <= 15 Zeichen, ADR-016).
- **Kopf ohne eigenen Zeiger:** Beim Boot werden alle N Slots einer Klasse
  gelesen; gueltige Slots mit aktueller Epoche liefern die hoechste Sequenz, der
  naechste Schreibslot ist `Sequenz mod N`. Ungueltige/fremde Slots werden
  gezaehlt und uebersprungen. Es gibt keinen zweiten atomaren Zeiger.
- Powercut-Garantie pro Schreibvorgang: Slot ist alt, neu oder ungueltig; es
  geht hoechstens der gerade geschriebene und der ueberschriebene aelteste
  Eintrag verloren, nie ein anderer. Ein `CommitOutcomeUnknown` wird durch den
  naechsten Scan aufgeloest.
- Schreibkontext: keine neue Schleife und kein neuer Task. Ereignisse werden in
  einer kleinen, statisch begrenzten RAM-Warteschlange gesammelt und im
  vorhandenen niederprioren Hauptschleifen-Schritt geschrieben; Regelung und
  Safety warten nie auf einen Flashzugriff. Ueberlauf der Warteschlange zaehlt
  und wird beim naechsten Schreiben als kritisches `EventsDropped` protokolliert.

### 4.4 Verlauf, Zusammenfassungen, Aufbewahrung

- **Aktiver Lauf** (Kontrollpunkte) bleibt vollstaendig im #17-Owner; #19 schreibt
  dort nichts.
- **Detaildaten:** Ring von 5 Laufplaetzen. Ein Platz besteht aus einer
  begrenzten Zahl von Chunks (Envelopes) mit Fensteraggregaten
  (Min/Mittel/Max je Kanal Schrankluft, Produkt, Kuehlkoerper, Sensor-/
  Gueltigkeitsstatus, Phase). **Die Punktezahl je Lauf ist durch Konstruktion
  begrenzt:** die Fensterlaenge wird beim Laufstart aus der programmierten
  Laufdauer abgeleitet (`windowMinutes = ceil(Dauer / MaxPunkte)`, mindestens
  1 Minute); die Laufdauer ist durch `kMaximumFermentationDurationMinutes`
  (20160 min) und die Programmgrenzen beschraenkt. Ein RAM-Puffer fuer den
  ganzen Lauf entsteht nicht; nur das aktuelle Fenster liegt im RAM und wird
  chunkweise bei Fensterwechsel/Checkpoint geschrieben. Verlust bei Powercut:
  hoechstens das ungeflushte Fenster (Komfortdaten).
- **Zusammenfassungen:** Ring von 50 Plaetzen (Lauf-ID, Programmbezug, Start/Ende
  mit Zeitqualitaet, Endgrund, Zaehler fuer Warnungen/Fehler/Unterbrechungen,
  Min/Mittel/Max). Die Zusammenfassung wird beim Laufende **vor** dem Verwerfen
  des aeltesten Detailplatzes geschrieben.
- **Bereinigung** (proaktiv, idempotent, wiederaufnehmbar), Reihenfolge wie
  `BACKUP_SECURITY_RETENTION.md`: verwaiste temporaere Exportdaten (entfallen
  hier, da der Export streamt, siehe 4.5) -> aelteste Diagrammdetails ->
  Detailplatz zu Zusammenfassung -> aelteste nichtkritische Zusammenfassung.
  Technisch ist das Ueberschreiben des jeweils aeltesten Slots der **einzige**
  Loeschvorgang: kein Erase, kein Portumbau, konstanter NVS-Bedarf. Zwischen den
  Schritten gilt "Zusammenfassung zuerst, dann Detail ueberschreiben"; ein Powercut
  dazwischen laesst Detail und Zusammenfassung beide stehen und der naechste
  Lauf der Bereinigung setzt fort.
- Aufbewahrungswerte sind in Release 1 feste Konstanten (O6). Alle Mengen
  (Slotzahlen, Chunkgroesse, `MaxPunkte`, Journalgroessen) sind
  `TBD_IMPLEMENTATION_BUDGET`, werden in C0 aus den Programm-/Dokumentgrenzen
  berechnet und per Ownerfreigabe festgesetzt und nie als produktiver Laufzeitwert
  geraten. Eine Compile-Zeit-Pruefung stellt sicher, dass die Summe aller
  Klassen unter dem freigegebenen Anteil der `state_store`-Partition bleibt.
- **Kapazitaetsfehler:** Liefert `write` `CapacityError`/`WriteError`, wird das
  als Persistenzfehler protokolliert (kritisch, soweit schreibbar). Komfortdaten
  werden danach ausgesetzt; ein laufender Prozess wird wegen Historienknappheit
  **nicht** gestoppt (`RESOURCE_BUDGET...`).

### 4.5 Laufexport (JSON/CSV)

- Erzeuger schreiben **streamend** in die vorhandene Senke
  `device_platform::IBinaryOutputSink` mit fester Chunkgroesse; kein vollstaendiges
  Dokument im RAM. Maximalgroesse ist aus den Konstanten berechenbar und wird
  vorab abgelehnt, wenn sie die freigegebene Exportgrenze uebersteigt.
- JSON: Format-Kennung, Schemaversion, Firmware-/Schemaangaben, Programm-
  schnappschuss, Laufrevisionen, Phasen und Ereignisse, Zeitreihen, Sensor-
  qualitaet, Abschlussgrund. CSV: Zeitbezug und Zeitqualitaet, Soll-/Istwerte
  der drei Kanaele, Sensorstatus, Phase, relevante Aktorereignisse.
- **Fehlende Werte werden als fehlend codiert** (JSON `null`, CSV leer), nie als
  `0`. Ein Export liest ausschliesslich aus den #19-Records und der Laufprojektion
  und kann strukturell keine Geheimnisse sehen (Whitelist).

### 4.6 Backup und Import

- **Bundle:** ein JSON-Dokument aus Format-Kennung, Schema-/Revisionsangaben,
  Benutzereinstellungen, Benutzerprogrammen, Auswahl der Standardprogramme,
  UI-/Sprach-/Zeitzoneneinstellungen und den ausdruecklich erlaubten
  Serviceparametern (O3), plus Integritaetspruefsumme (CRC-32, kein Manipulations-
  schutz). Nicht enthalten: WLAN-/Web-/PIN-Geheimnisse und Pruefnachweise,
  Sitzungen/Tokens/CSRF, Schluessel, Touchkalibrierung, rohe Flashdaten, rohe
  interne Envelopes. Der Export ist deterministisch (feste Feldreihenfolge).
- **Importablauf** (verbindlich, `BACKUP_SECURITY_RETENTION.md`): Groesse/Format
  pruefen -> Schema identifizieren -> Integritaet -> getestete Migration ->
  **vollstaendige typisierte Validierung** (Bereichsgrenzen, unbekannte kritische
  Felder = Ablehnung, nicht unterstuetzte IANA-Zeitzone = Ablehnung, kein stiller
  Rueckfall) -> Vorschau/Konflikte -> ausdrueckliche Bestaetigung -> Commit.
  Der Kandidat wird als `ConfigurationCommitCandidate` in den **vorhandenen**
  Pfad `beginPreview`/`installPreview(origin, ChangeOperation::BackupImport)`/
  `confirmPreview` gegeben. Es gibt keinen zweiten Aktivierungspfad, kein Pending
  und keinen parallelen Active-Zweig; bis zum Commit bleibt alles fluechtig.
- **Lauf-Gate:** Import nur bei sicher festgestelltem `NoActiveOrRecoverableRun`;
  geprueft bei Annahme, bei Vorschau/Bestaetigung und unmittelbar vor dem Commit
  (aktiv, pausiert, unterbrochen, wiederherstellbar, unbekannt blockieren).
- **Geheimnisse:** Fehlende Geheimnisse ueberschreiben nichts; Factory-Katalog
  und Quellprogramme werden nicht still ueberschrieben; Touchkalibrierung bleibt.
- **Groesse (O5):** C0 erzeugt aus dem Gesamtschema den maximal gueltigen
  Kandidaten und misst Textgroesse und Parser-Spitzenbedarf gegen den RAM-Rahmen
  aus Abschnitt 2. Erst danach faellt die Wahl zwischen Gesamtbody, Chunking mit
  Vorab-Validator oder reduziertem Maximum. cJSON baut den ganzen Baum auf; falls
  das nicht passt, ist das Chunking-/Reduktionsmodell der Plananker, nicht ein
  zweiter Codec.

### 4.7 Startkriterium "Journal verlaesslich"

`RESOURCE_BUDGET_AND_MAINTENANCE.md` verlangt: kein neuer Lauf, wenn das
Fehlerjournal fuer verriegelte Ereignisse nicht verlaesslich ist. Umsetzung:
ein einfaches Praedikat (`criticalJournalReliable()`) im Journal-Dienst, das der
bestehenden Startvorbedingungsliste der Anwendung als zusaetzlicher Grund
angehaengt wird. **Es fuehrt keinen neuen `FaultCode` und keine neue Interlock-
Logik ein** (#24 bleibt unveraendert); die Auswertung ist an O7 gebunden.

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

Nach jedem Schnitt: gezielte lokale Tests, Self-Check, **Stopp bis zur Freigabe
des naechsten Schnitts**. Alle Dateinamen sind Vorschlaege innerhalb der
beschriebenen Module.

| Schnitt | Inhalt | Erwartete Dateien | Nachweis |
|---|---|---|---|
| **C0** (nur Doku/Messung, Gate G1) | Berechnung der Ring-/Chunkgroessen aus den Programm-/Dokumentgrenzen; Wear-Abschaetzung (Schreibzahl je Lauf, Flush-Intervalle) als Rechnung, nicht als Messung; maximal gueltiger Importkandidat (Native-Messung); Entscheid-Vorlagen O1/O5/O6/O7. | `docs/audits/ISSUE19_STORAGE_BUDGET.md` (+ kleines Hostwerkzeug nur fuer die Messung, falls noetig, ohne Produktcode) | Zahlen reproduzierbar; Owner setzt Budgetkonstanten |
| **C1** Ring + Journal | `BoundedRecordRing`, Ereigniskatalog, kritisch/Information, RAM-Warteschlange, `IEventJournal`-Produktimplementierung, Boot-Ereignis aus Reset-Ursache; kapazitaetsbegrenzter Teststore; Epochenbindung. | `device_platform`: ring; `fermentation_app`: journal; `device_platform_test_support`: bounded store; Tests `test_event_journal*` | SIM-19-01..03, 05, 13 |
| **C2** Verlauf + Retention | Fensteraggregate, Detail-/Zusammenfassungsringe, Lauf-Ende-Uebergabe, idempotente Bereinigung, Kapazitaetsverhalten. | `fermentation_app`: run history; Tests | SIM-19-03, 04, 12 |
| **C3** Laufexport | Streamender JSON-/CSV-Writer, Groessengrenze, Redaction-Whitelist, Zeitqualitaet, Fehlend-Codierung. | `fermentation_app`: run export; Tests mit Golden-Dateien | SIM-19-06 |
| **C4** Backup-Export | Whitelist-Projektion, Bundle-Writer, deterministische Ausgabe, CRC. | `fermentation_app`: backup; Tests | SIM-19-07 |
| **C5** Import | Parser/Validator/Migrationstabelle, Kandidatenbau, Vorschau-/Konfliktprojektion, dreifaches Lauf-Gate, Aufruf des vorhandenen Preview-/Commit-Pfads. | `fermentation_app`: import; Tests inkl. Powercut-Matrix ueber alle Write-Cutpoints | SIM-19-08..10 |
| **C6** Werksreset-Ablauf | Mehrstufiger lokaler Ablauf, Aktor-AUS-Vorbedingung, Ablehnung entfernter Ausloeser, Praesentationsmodell (O8-A). | `fermentation_app`: reset flow; Tests | SIM-19-11 |
| **C7** Doku/Abnahme | Acceptance-Eintraege, Dokumentsynchronisierung, ROADMAP, Ressourcenbericht (statisch). | siehe Abschnitt 7 | Review-fertig |

Nach C7: Builder-Self-Check, Stopp fuer den unabhaengigen Review. Pre-Ready
erst auf ausdrueckliche Owneranweisung (Repository-Prozess, hier nicht
dupliziert).

## 6. Tests und Nachweise (hardwarefrei)

| ID | Pruefung |
|---|---|
| SIM-19-01 | Ring: Reihenfolge, Wrap, Kopfermittlung per Scan, ungueltige/fremde Slots werden uebersprungen und gezaehlt |
| SIM-19-02 | Schreib-Powercut-Matrix (vor Commit, nach Commit/`CommitOutcomeUnknown`, Slot-Korruption): nie mehr als der geschriebene und der aelteste Eintrag betroffen |
| SIM-19-03 | Prioritaet: Komfortklassen bis ueber Kapazitaet fuellen -> kritische Ringe unveraendert; kritischen Ring ueberfuellen -> nur aelteste kritische weichen, in Reihenfolge |
| SIM-19-04 | 5 Detail / 50 Zusammenfassungen; Bereinigung bei jedem Cutpoint wiederaufnehmbar und idempotent; Zusammenfassung vor Detailverwurf |
| SIM-19-05 | `StorageEpoch`-Wechsel: alte Records unerreichbar, Ringe neu (und O4-Variante) |
| SIM-19-06 | Laufexport JSON/CSV Golden; fehlend != 0; Zeitqualitaet; Groessengrenze; **Sentinel-Geheimnisse** nicht in der Ausgabe |
| SIM-19-07 | Backup Golden/deterministisch; Whitelist; Sentinel fuer WLAN-/Web-Passwort, PIN-Nachweis, Tokens, SoftAP-Passwort, Touchkalibrierung |
| SIM-19-08 | Importmatrix: unbekanntes Schema, unbekanntes kritisches Feld, falscher Typ/Bereich, nicht unterstuetzte Zeitzone, ueberlang, CRC-Fehler, abgeschnitten, alte Schemas ueber Migration; **jeder Fehler laesst Store-Bytes und `stateRevision` unveraendert** |
| SIM-19-09 | Import atomar: Powercut an jedem `IStateStore`-Write-Cutpoint des Commits -> nach Neustart exakt alter oder neuer Graph, nie gemischt; Lauf-Gate an allen drei Pruefpunkten fuer aktiv/pausiert/unterbrochen/wiederherstellbar/unbekannt |
| SIM-19-10 | Import ueberschreibt weder Geheimnisse noch Touchkalibrierung noch Factory-Katalog |
| SIM-19-11 | Werksreset-Ablauf: alle Schritte noetig, Aktoren-AUS-Vorbedingung, Remote abgelehnt, Touchkalibrierung bleibt, Verlauf/Journal gemaess O4 |
| SIM-19-12 | Regelpfad unbeeinflusst: Journal-/Verlaufs-/Exportfehler und -Kapazitaet stoppen keinen Lauf; kritischer Persistenzausfall blockiert nur den **Start** (O7); kein Flashzugriff im Regelpfad |
| SIM-19-13 | Struktur: kritische Records enthalten keine Freitextfelder |

Zusaetzlich je Schnitt: Format-/Tidy-Self-Check, Architekturcheck (+ Selftest),
und fuer alles, was Library-Quellen beruehrt, Build beider ESP-IDF-Profile. Ein
Build ersetzt keinen Hardwarenachweis.

**Nicht hardwarefrei beweisbar (HW-19-xx, siehe O9):** reale NVS-Wear und
Schreiblatenz der Ringe, Powercut auf echtem Flash, Heap-/Stack-Spitze beim
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
| RAM-Rahmen (8148 B Minimum unter Weblast) | Export/Import koennen nicht ganze Dokumente halten | Streaming, feste Puffer, O5 nach C0-Messung |
| NVS-Wear und Schreiblatenz | Journal-/Verlaufsschreiben nahe am Regelpfad | Schreiben nur im niederprioren Schritt, Fensterflush statt Dauerschreiben, Wear nur als Rechnung (C0) und HW-19 |
| Dauer-/Punkteexplosion (bis 14 Tage) | unbegrenzter Detailbedarf | Fensterlaenge aus Laufdauer, `MaxPunkte` als Budgetkonstante |
| Startkriterium beruehrt Safety-nahe Vorbedingungen | unbeabsichtigte Sperre aller Laeufe | O7, kein neuer FaultCode, Budgetsperre |
| Schluesselraum | NVS-Schluessel <= 15 Zeichen, Kollisionsrisiko | feste Praefixe und Indizes, Review gegen vorhandene Schluessel (`cb0/cb1`, `cc0`, `tc0/tc1`, Run-/Konfigurationsslots) |
| Kein Hardwarenachweis | Annahmen zu Wear/Latenz unbelegt | O9, ausdruecklich `NOT_RUN` |
| Fruehere Entwurfsinhalte nicht auffindbar | moegliche verlorene Anforderungen | Plan stuetzt sich auf Issue-Scope und kanonische Dokumente; Reviewer vergleicht, falls der Owner den Entwurf liefert |

## 9. Grenzen

- **#28** bleibt Diagnose-/Service-/Exportgate und nutzt #19-Daten; #19 liefert
  weder Diagramme noch Diagnoseexport noch Serviceablauf.
- **#27**: Web-Anbindung des Laufexports, Backupdownloads und Imports folgt dort
  bzw. ueber O2-B; #19 vervollstaendigt #27 nicht.
- **Hardware**: keine Hardwaretests, keine Aktorfreigabe, kein Flash-Geraet;
  `ACTUATOR_RELEASE=NO`.
- Release-1-Abgrenzung (`SPECIFICATION_REVIEW.md`): kein OTA, kein
  Roh-Flash-Backup ueber das Web.

## 10. Checkliste Planfreigabe

- [ ] unabhaengiger Plan-Review ohne offene Blocker
- [ ] Ownerentscheide O1–O9 (mindestens O1, O2, O3, O4, O6, O7 vor C1)
- [ ] exakte Plan-SHA vom Owner freigegeben
- [ ] G1 nach C0 (Budgetzahlen, O5)
