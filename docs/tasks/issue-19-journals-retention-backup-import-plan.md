# Plan Issue #19 – Journale, Aufbewahrung, Bereinigung, Backup und Import

```text
PLAN_REVISION=3 (konsolidiert; ersetzt Revision 2 `407f2f5ce2b1fa0029023c26085afb5bc9b55560` und Revision 1, beide nie freigegeben)
PLAN_STATUS=DRAFT_AWAITING_PLAN_FIX_VERIFICATION_AND_OWNER_APPROVAL (exakte Plan-SHA steht im Draft-PR)
ISSUE=19 (E2.4), Epic #4 - Issue bleibt offen
BASE_MAIN=9beb68f1935f80c6d2a59b5a612d542e5d9109a7 (PR #189 gemergt am 2026-10-08)
TOOLCHAIN=ESP-IDF v6.1 (fff9895c82d744c7237be8847347bdd1b07c6643)
OWNER_PRIORISIERUNG=Werksreset R1-PFLICHT; Backup und Import R1-ERWUENSCHT (nur bei nachgewiesener RAM-/Speichereignung); Journal, Laufhistorie, Bereinigung, Laufexport NUR PLANEN
JOURNAL_HISTORY_CLEANUP_EXPORT=DEFERRED_BY_OWNER_PENDING_R1_CONTRACT_RECONCILIATION
EARLIER_DRAFT=REVIEW_DRAFT - PRESERVE, NOT APPROVED, NOT CANONICAL (im Repository und in allen PRs nicht auffindbar; dieser Plan stuetzt sich nicht darauf)
SCOPE_OF_THIS_COMMIT=NUR_PLAN (keine Produktionslogik, keine produktiven Tests, keine Schemas, keine Messung); ROADMAP-Bereinigung und Archiv bleiben, nur #19-Zeilen angepasst
IMPLEMENTATION=NOT_STARTED (dieser Plan autorisiert keinen Produktcode)
HARDWARE=NOT_RUN
ACTUATOR_RELEASE=NO
```

## 0. Ownerpriorisierung und R1-Scope

Verbindliche Ownerpriorisierung. **Sie ist keine Freigabe fuer Produktcode.** Die
Umsetzung der vorgesehenen R1-Funktionen beginnt erst nach (a) Plan-Fix-
Verification, (b) ausdruecklicher Ownerfreigabe genau dieser Plan-SHA und – wo
unten genannt – (c) dem jeweiligen Vorab-Nachweis und Ownerentscheid.

| # | #19-Funktionsbereich | Prioritaet | Planstatus in Revision 3 | Umsetzung |
|---|---|---|---|---|
| 7 | Vollstaendiger **lokaler Werksreset** inkl. PIN-unabhaengigem Recoveryweg bei vergessener Service-PIN und Erhalt der Touchkalibrierung | **R1-PFLICHT** | konkret geplant (Abschnitt 4) | nach Freigabe dieser Plan-SHA und Entscheid O-R1 |
| 5 | Normales, geheimnisfreies **Backup** | R1-ERWUENSCHT, nur bei nachgewiesener RAM-/Speichereignung | bedingt geplant (Abschnitt 5) | erst nach Ressourcennachweis B0 **und** Ownerentscheid |
| 6 | Vollstaendig validierter, atomarer **Import** mit Vorschau | R1-ERWUENSCHT, nur bei nachgewiesener RAM-/Speichereignung | bedingt geplant (Abschnitt 5) | erst nach Ressourcennachweis B0 **und** Ownerentscheid |
| 1 | Priorisiertes **Journal** | NUR PLANEN – `DEFERRED_BY_OWNER_PENDING_R1_CONTRACT_RECONCILIATION` | nur Referenz (Anhang A) | **nicht autorisiert** |
| 2 | Begrenzte **Mess-/Laufhistorie** | NUR PLANEN – `DEFERRED_BY_OWNER_PENDING_R1_CONTRACT_RECONCILIATION` | nur Referenz (Anhang A) | **nicht autorisiert** |
| 3 | **Speicherbereinigung**/Aufbewahrung | NUR PLANEN – `DEFERRED_BY_OWNER_PENDING_R1_CONTRACT_RECONCILIATION` | nur Referenz (Anhang A) | **nicht autorisiert** |
| 4 | **Laufexport** (JSON/CSV) | NUR PLANEN – `DEFERRED_BY_OWNER_PENDING_R1_CONTRACT_RECONCILIATION` | nur Referenz (Anhang A) | **nicht autorisiert** |

Folgen der Zuordnung: Issue #19 bleibt offen und ist mit dem R1-Zuschnitt **nicht**
abschliessbar (seine Akzeptanzkriterien zu Journal, Bereinigung und Export sind
nicht Teil der R1-Umsetzung); die Anpassung von Issue-Scope oder -Abhaengigkeiten
ist ausschliesslich Ownersache (Abschnitt 7). Fuer die zurueckgestellten Bereiche
wird **keine Architektur festgeschrieben**; die Revision-2-Entwuerfe bleiben nur
als nicht verbindliche fachliche Referenz in Anhang A und gelten bei einer
spaeteren Wiederaufnahme nicht als freigegeben (neue vollstaendige Planrevision
auf dann aktuellem `main`).

## 1. Ziel und Nicht-Ziele

**Ziel (R1).** (1) Der vollstaendige lokale Werksreset ist auf dem Geraet
bedienbar und bildet die beiden verbindlichen Abstufungen ab – normal
PIN-geschuetzt und PIN-unabhaengig bei vergessener Service-PIN –, ohne
Remote-Ausloeser, mit Aktoren AUS, wiederaufnehmbarem Epochenwechsel, Widerruf
der Secrets/Sessions und Erhalt der geraetespezifischen Touchkalibrierung.
(2) Backup und Import werden nur dann umgesetzt, wenn ein belastbarer
Ressourcennachweis vorliegt und der Owner den konkreten Umfang entscheidet.

**Nicht-Ziele.**

- Keine Implementierung von Journal, Laufhistorie, Bereinigung, Laufexport; keine
  C0-Messung dazu; keine Budgetkonstanten dafuer.
- #28 (Diagnose, Service, Berichtexport) und #27 (Web-Transport, Mutationspfade)
  werden nicht vorgezogen oder vervollstaendigt.
- Keine Aenderung an Regelung, Safety, Interlock (#24), Aktorplanung oder der
  Laufpersistenz-Wahrheit (#17); keine Aktorfreigabe; keine Hardwaretests.
- Keine neue Datenbank, kein zweites Aktivierungsmodell, kein Pending/Intent, keine
  neue Prozessschleife, kein neuer Task, kein neuer ESP-IDF-Adapter.
- Keine Authentication-/Connectivity-Domaenen-Erweiterung; Geheimnisse bleiben in
  ihren bestehenden epochengebundenen Domaenen.
- Keine OTA-/PSRAM-Reserve, kein Roh-Flash-Backup (nicht portabel, nicht ueber das
  Web).
- Keine stille Kuerzung gueltiger Programm-/Konfigurationslimits und keine
  eigenmaechtige Umdefinition kanonischer Safety-/Release-Vertraege.

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
| Fehler-/Reset-/Ereignisjournal *(zurueckgestellt, Anhang A)* | Nur der Port `IEventJournal::record(ms, string)` (`lib/device_platform/src/event_journal.hpp`) und ein Testmock; **kein Produktaufrufer, keine Produktimplementierung**. Reset-Ursache wird als Evidenz geliefert (`IResetCauseSource`, "not persisted"). `RESOURCE_BUDGET...` legt fest: der Interlock besitzt keine Fault-Historie (`INTERLOCK_OWNS_FAULT_HISTORY=NO`). | Persistentes, typisiertes, priorisiertes Journal; Bootereignis aus Reset-Ursache; Lebenszyklusereignisse. Der Port ist Freitext; kritische Ereignisse brauchen typisierte Records ohne Freitext (Anhang A). |
| Begrenzte Mess-/Laufhistorie *(zurueckgestellt, Anhang A)* | `RunPersistenceSnapshot` ist ausdruecklich nur Run-Domaene; "journal history are outside Issue #17" (`run_persistence_contract.hpp`). | Alles: Fensteraggregate, Laufzusammenfassungen, Aufbewahrung. |
| Aufbewahrung/Bereinigung *(zurueckgestellt, Anhang A)* | Nur Dokumentvertrag. `IStateStore` kennt ausschliesslich `read`/`write` (kein Erase, keine Aufzaehlung, keine Belegungsabfrage); Test-Store `SimulatedPersistentStateStore` kann Powercuts/Kapazitaetsfehler pro Write injizieren, hat aber keine begrenzte Gesamtkapazitaet. | Strukturell begrenzte Ablage mit Prioritaetsklassen, idempotente Bereinigung, kapazitaetsbegrenzter Testspeicher. |
| Laufexport JSON/CSV *(zurueckgestellt, Anhang A)* | `web_json_codec` (cJSON, privat gekapselt) fuer `/api/v1/status|temperatures|alerts`; kein Export. #17 haelt nur den **aktiven** Lauf (`RunPersistenceSnapshot`: Programmsnapshot, Revisionen, Prozesszustand); nach Abschluss/Neustart (`NoActiveRun`) sind Programm, Revisionen sowie Phasen-/Ereignisverlauf nicht mehr vorhanden, und es gibt keinen Mehrlauf-Verlauf. | Begrenztes persistiertes Laufarchiv aus den kanonischen Run-Uebergaengen (Anhang A), Export-Writer (JSON, CSV), Zeit-/Qualitaetscodierung, Groessengrenze, Redaction. |
| Normales Backup *(bedingt, Abschnitt 5)* | Konfigurationsdokumente sind typisiert (`UserConfiguration`, `ServiceConfiguration` Schema 3, `ProgramCatalog`); `ChangeOperation::BackupImport` existiert als Wire-ID, hat **keinen Produzenten**. Doku: kein portables Format ("wird mit Issue #19 implementiert"). | Portables, versioniertes, secret-freies Format (Whitelist-Projektion). |
| Validierter Import *(bedingt, Abschnitt 5)* | Mutationspfad mit fluechtiger Vorschau und atomarem Commit existiert: `ConfigurationService::beginPreview` -> `installPreview(origin, operation)` -> `validatePreviewForConfirmation` -> `confirmPreview` (Active/Fallback, ein persistenter Linearisierungspunkt, ADR-018). Programmaenderungen pruefen bereits Run-Evidenz (`applyProgramEdit`, `makeFermentationUiProgramUsageEvidence`); alle Anwendungseinstiege laufen ueber `ApplicationCallSerializer`. | Parser/Validator/Migration fuer externe Kandidaten, Konflikt-/Vorschauprojektion, atomare Lauf-/Import-Entscheidung im vorhandenen `ApplicationCallSerializer`-Guard (nur bei Umsetzung, Abschnitt 5), Groessenpfad (O5). |
| Werksreset *(R1-PFLICHT, Abschnitt 4)* | Kern vorhanden: `ConfigurationRecoveryService::beginAuthorizedFactoryReset` (wiederaufnehmbar ueber `BootstrapState::Resetting`; prueft **nicht** Laufzustand, PIN oder Ursprung), `FermentationApplication::beginAuthorizedFactoryReset` (Auth-Gate drainieren, Websessions widerrufen, Auth-Zustand zuruecksetzen, Run-Epochenuebergabe). **Touchkalibrierung bleibt erhalten** und ist getestet. Regressionen zu Session-Widerruf, Login-Drain und Run-Handoff vorhanden (4.6). | Produktiver Aufrufer fehlt (nur Testharness `issue_90_slice7`); `PersistentFactoryReset` ist in `safeBootUnavailableCapabilities()` als nicht verfuegbar gefuehrt; kein Ablauf mit Mehrfachbestaetigung/Vorbedingungen, keine Bedienung, Ausloeser fuer den PIN-unabhaengigen Vollreset ungeklaert. |
| Geheimnisse | Epochengebundene Connectivity-/Authentication-Domaenen mit eigenen Records (`cc0`, Typ 9; Typ 11/12), nie in Konfigurationsdokumenten. SoftAP-Passwort ist fluechtig (pro Start neu). | Nur Nachweis: Whitelist-Projektionen und Sentinel-Tests, dass keine Ausgabe Geheimnisse enthaelt. |

Bereits belegte **Wiederverwendung** (nichts davon wird neu erfunden):
`IStateStore` + Envelope V1 + `StorageEpoch` + Slotmechanik, ADR-016-Schluesselraum
(ASCII, <= 15 Zeichen), `ConfigurationService`-Vorschau/Commit,
`ConfigurationRecoveryService`-Reset, Run-Identity (#144), UTC-/Zeitqualitaet
(#126), cJSON-Codecgrenze, Crc32-Hilfen, `FakeDs18b20Bus`-artige Testhilfen im
`device_platform_test_support`. Belegte Record-Typ-IDs: 1–12 (naechste freie: 13).

## 4. R1-PFLICHT: Vollstaendiger lokaler Werksreset

### 4.1 Zwei verbindliche Abstufungen (SSOT: `LOCAL_UI_SETTINGS_SERVICE.md`, ADR-010)

Beide enden im selben Resetkern (4.2), unterscheiden sich aber in Ausloeser,
Berechtigung und Voraussetzungen. Sie sind getrennte, eindeutig bestaetigte
Ablaeufe und werden nicht vermischt – auch nicht mit der PIN-unabhaengigen
Raw-Touch-Kalibrierungs-Recovery (#31).

| Merkmal | **A – Normaler Vollreset** | **B – PIN-unabhaengiger lokaler Vollreset (vergessene PIN)** |
|---|---|---|
| Ausloeser | Menueeintrag im PIN-geschuetzten Servicebereich | bewusster **physischer lokaler Recoveryweg** (Geraet einschalten oder `SAFE_BOOT` aktiv); konkreter Ausloeser: **offen, Entscheid O-R1** (rohe Touchgeste ist `TBD_HARDWARE`) |
| Berechtigung | lokal verifizierte Service-PIN | keine PIN (die PIN darf fuer ihre eigene Wiederherstellung nicht verlangt werden); **kein isolierter PIN-Reset, kein Servicezugang** |
| Laufzustand | nur ohne Lauf (kein aktiver, pausierter, unterbrochener, wiederherstellbarer oder unbekannter Lauf) | Recovery-/Bootfenster ohne laufenden Prozess; ein laufender Prozess blockiert den Ablauf (vorher sicher beenden bzw. Neustart) |
| Bestaetigung | mindestens zweistufig; zeigt geloeschte und wiederhergestellte Daten | mehrstufige Warnung ueber vollstaendigen Datenverlust, danach **lange bewusste lokale Bestaetigung** (Dauer: Ownerwert, nicht geraten) |
| Aktoren | der Ablauf schaltet nichts ein; Aktorpfad bleibt gesperrt | alle Aktoren und beide BTS7960-Richtungen bleiben AUS; es werden keine Aktor-/Servicefunktionen freigeschaltet |
| Fernausloesung | nie | nie (nicht ueber Web oder Netzwerk) |
| Ergebnis | Factory-Programme, Factory-Grenzen, Ersteinrichtungszustand; Touchkalibrierung bleibt | wie A |

Letzter physischer Recoveryweg bleibt UART-Loeschen beziehungsweise Neu-Flashen
(`LOCAL_UI_SETTINGS_SERVICE.md`); er ersetzt den lokalen Ablauf in R1 nicht.

### 4.2 Wiederverwendete Vertraege (nichts davon wird neu erfunden)

- `ConfigurationRecoveryService::beginAuthorizedFactoryReset` (Resetkern): wiederaufnehmbarer
  Epochenwechsel ueber `BootstrapState::Resetting` mit Active/Fallback-Invalidierung und neuer
  Initialkonfiguration, Run-Epochen-Handoff-Beweis (`takeAuthorizedRunEpochHandoffProof`),
  Zaehler-/Handoff-Sperren. **Der Kern prueft weder Laufzustand noch PIN noch Ursprung** –
  "authorized" heisst: der Aufrufer ist dafuer verantwortlich. Diese Pruefungen gehoeren
  deshalb in den neuen Ablauf (4.3).
- `FermentationApplication::beginAuthorizedFactoryReset`: drainiert den Auth-Operation-Gate,
  widerruft Websessions an der Vertrauensgrenze, setzt den Authentication-Zustand zurueck,
  uebergibt die neue Epoche an die Laufpersistenz; laeuft im `ApplicationCallSerializer`.
- Touchkalibrierung bleibt ueber den Epochenwechsel erhalten (Schluessel `tc0`/`tc1`).
- Geheimnisse (Connectivity, Authentication) sind epochengebunden und durch den
  Epochenwechsel logisch unerreichbar; das SoftAP-Passwort ist fluechtig.
- SAFE_BOOT-Modell der Touch-Workspace-Schicht (`FermentationUiSafeBootTarget`/
  `...Capability::PersistentFactoryReset`), Zuordnung nach `ACCEPTANCE_TESTS.md` SIM-26-07
  (Werksreset = #57-Owner); PIN-Eingabemodell `device_ui_pin`; PIN-Pruefung ueber den
  vorhandenen Authentication-Record-Pfad (`verifyServicePin`).

### 4.3 Nachweisbare Luecken und Modulzustaendigkeit

| Luecke | Zustaendigkeit (ADR-013) |
|---|---|
| **Kein produktiver Aufrufer** des Resetkerns (nur der Testharness `issue_90_slice7`). `safeBootUnavailableCapabilities()` fuehrt `PersistentFactoryReset` heute als *nicht verfuegbar*. | neuer Anwendungs-Einstieg in `fermentation_app` |
| Kein Ablauf mit Mehrfachbestaetigung, Vorbedingungspruefung (Lauf, PIN, Ursprung) und Ergebnisprojektion | `fermentation_app`: kleiner Zustandsautomat `FactoryResetFlow` (Arbeitsname); keine neue Schleife, kein Task |
| Kein Bedienpfad (Warn-/Bestaetigungsseiten, PIN-Eingabe, Ergebnis) | Praesentationsmodell in `fermentation_app` (bestehende Touch-Workspace-/Device-UI-Vertraege); Bildschirme im vorhandenen Renderer unter `main/` – eigener Schnitt |
| Ausloeser fuer Ablauf B (physischer Recoveryweg) ist weder implementiert noch im Konzept festgelegt | Ownerentscheid O-R1 |
| Unklar, ob der Anwendungs-Einstieg auch ohne geladene Runtime (SAFE_BOOT, beschaedigte Konfiguration) lauffaehig ist (`storageEpoch_`/`stateStore_` Voraussetzungen) | R0-Vorpruefung |

### 4.4 Invarianten des Ablaufs (pruefbar)

1. **Nur lokal.** Der Ablauf nimmt Eingaben ausschliesslich ueber die lokale
   Praesentationsschicht an; es existiert keine HTTP-/Web-Route und kein
   Netzwerkkommando, das ihn oder den Resetkern erreicht (Routentabelle wird getestet).
2. **Keine Aktorwirkung.** Vor, waehrend und nach dem Reset kein Aktor-Enable; der
   Ablauf liest/aendert keine Interlock-Freigabe (#24 unveraendert).
3. **Vorbedingungen unter dem Guard.** Die bindende Pruefung (kein Lauf; bei A
   verifizierte PIN) liegt im selben `ApplicationCallSerializer`-Guard wie der Aufruf
   des Resetkerns; fruehere Pruefungen in der Bedienung sind nur Fruehabbrueche.
4. **Keine automatische Ausloesung.** Weder Datenfehler noch SAFE_BOOT noch Timeout
   loesen den Reset aus; Abbruch/Timeout an jeder Stelle verlaesst den Zustand
   unveraendert (kein Store-Write vor dem bestaetigten Aufruf).
5. **Wiederaufnehmbar.** Ein Stromausfall mitten im Reset wird durch den vorhandenen
   `Resetting`-Mechanismus aufgeloest; der Ablauf zeigt danach den Ausgang und stellt
   keinen eigenen Zwischenzustand her.
6. **Widerruf.** Nach Erfolg sind alte Websessions und Auth-Zustand ungueltig, alte
   Credentials logisch unerreichbar; Touchkalibrierung bleibt.
7. **Ergebnis ehrlich.** Fehler des Kerns (`...Failure`/`...Rejected`/`CounterOverflow`
   u. a.) werden als solche projiziert; kein erfundenes Erfolgsbild.

### 4.5 Umsetzungsschnitte (nach Planfreigabe; kein Produktcode vorher)

| Schnitt | Inhalt | Gate |
|---|---|---|
| **R0** Vorpruefung (nur Lesen/Dokumentieren) | Belegen: (a) wo die lokale Service-PIN-Pruefung und der PIN-geschuetzte Servicebereich im Code liegen (oder fehlen), (b) ob `FermentationApplication::beginAuthorizedFactoryReset` ohne geladene Runtime nutzbar ist, (c) dass der Runstart-Pfad den `ApplicationCallSerializer` betritt, (d) was "Ersteinrichtung" im Produkt heute ist. Befunde als kurzer Nachtrag im PR; ein Widerspruch zum Plan ist ein Stoppbefund, keine stille Umplanung. | keiner (nur Lesen) |
| **R1** Ablauf-Zustandsautomat | `FactoryResetFlow` fuer A und B inkl. Vorbedingungen im Guard, Stufen, Abbruch, Ergebnisprojektion; Aufruf des vorhandenen Resetkerns; gezielte Tests. | Planfreigabe |
| **R2** Anbindung | Praesentationsmodell, SAFE_BOOT-Capability `PersistentFactoryReset` verfuegbar machen (nur wenn der Ablauf lauffaehig ist), Aktor-AUS-Beleg ueber Mock-Senken. | Planfreigabe |
| **R3** Bildschirme/Ausloeser | Minimale Warn-/Bestaetigungs-/PIN-/Ergebnisseiten im vorhandenen Renderer; Ausloeser fuer B gemaess O-R1. Hardware-Anzeige `NOT_RUN`. | **O-R1** |
| **R4** Doku/Abnahme | Acceptance-Eintraege, Dokumentsynchronisierung, ROADMAP; physische Tests als `NOT_RUN`. | – |

Zwischen R0–R4 gibt es keine Ownerfreigabe je Schnitt; gezielte Tests und
Builder-Self-Check je Schnitt. Danach: Stopp fuer den unabhaengigen Review
(kanonisch), Pre-Ready nur auf ausdrueckliche Owneranweisung.

### 4.6 Tests (hardwarefrei)

Bereits vorhanden und **wiederzuverwenden** (Regression, nicht neu):
`test_factory_reset_advances_epoch_and_preserves_touch_key` und
`test_factory_reset_preserves_real_touch_calibration_record`
(`test_configuration_recovery_service`);
`test_composed_dispatcher_factory_reset_revokes_old_sessions`,
`test_factory_reset_drains_running_login_before_touching_the_store`,
`test_failed_factory_reset_reopens_the_gate_and_keeps_the_domain`,
`test_protected_login_session_created_before_reset_is_revoked`
(`test_web_application_routes`);
`test_application_reset_hands_off_existing_run_store_to_new_epoch` und
`test_application_reconstructs_reset_handoff_after_run_write_cut`
(`test_issue144_run_identity`).

Neu (gezielt, nur fuer den Ablauf):

| ID | Pruefung |
|---|---|
| SIM-R-01 | Ablauf A: Stufen nicht uebersprungen; Abbruch/Timeout an jeder Stufe -> Store-Bytes und `stateRevision` unveraendert, Kern nicht aufgerufen |
| SIM-R-02 | A lehnt bei aktivem/pausiertem/unterbrochenem/wiederherstellbarem/unbekanntem Lauf ab; Pruefung liegt im Guard (Konkurrenzfall Runstart/Reset deterministisch) |
| SIM-R-03 | A verlangt verifizierte lokale PIN; falsche/gesperrte PIN -> kein Reset, kein Store-Write |
| SIM-R-04 | B ohne PIN: nur mit allen Warnstufen und langer Bestaetigung; Abbruch -> unveraendert; B gibt keinen Service-/Aktorzugang frei und setzt die PIN nicht isoliert zurueck |
| SIM-R-05 | Nicht lokale Ursprungsangabe wird abgelehnt; die Web-Routentabelle enthaelt keinen Pfad zu Ablauf oder Kern |
| SIM-R-06 | Mock-Aktorsenken zaehlen waehrend des gesamten Ablaufs (A und B) null Enable-Aufrufe; Interlock-Permission unveraendert |
| SIM-R-07 | Stromausfall-Cutpoints des Kerns (vorhandene Matrix) plus Ablauf-Ebene: nach Neustart wird der Ausgang korrekt projiziert; kein erneuter Reset ohne Bestaetigung |
| SIM-R-08 | Nach Erfolg: alte Session ungueltig, Auth-Zustand zurueckgesetzt, alte Credentials unerreichbar, SoftAP-Passwort neu (Ablauf-Ebene, ergaenzt die obigen Regressionen) |
| SIM-R-09 | Touchkalibrierung bleibt ueber den vollstaendigen Ablauf A und B erhalten |
| SIM-R-10 | SAFE_BOOT-Capability `PersistentFactoryReset` nur verfuegbar, wenn der Ablauf lauffaehig ist |
| SIM-R-11 | Projektion von A nennt geloeschte und wiederhergestellte Daten; B nennt vollstaendigen Datenverlust |

Zusaetzlich je Schnitt: Format-/Tidy-Self-Check, Architekturcheck (+ Selftest) und
Build beider ESP-IDF-Profile, soweit Library-/`main/`-Quellen betroffen sind. Ein Build
ersetzt keinen Hardwarenachweis. **Nicht hardwarefrei beweisbar (`NOT_RUN`):**
physischer Ausloeser von B, reale Anzeige/Touch-Bedienung, Aktor-AUS am realen Geraet,
Powercut auf echtem Flash, Dauer der langen Bestaetigung am Geraet.

## 5. R1-ERWUENSCHT (bedingt): Backup und Import

**Bedingung.** Umsetzung nur, wenn (1) ein belastbarer Ressourcennachweis (B0) die
Eignung zeigt, (2) der Owner den konkreten Funktionsumfang danach entscheidet und
(3) die minimal erforderlichen Transport-/Bedienpfade aus #27 verfuegbar sind
(5.4). Ohne diese Bedingungen bleibt Abschnitt 5 Planung.

### 5.1 Fachlicher Vertrag (unveraendert aus den kanonischen Dokumenten)

Normales Backup: vollstaendiges, versioniertes, **geheimnisfreies** Bundle (keine
WLAN-/Web-/PIN-Geheimnisse oder Pruefnachweise, keine Sitzungen/Tokens, keine
Schluessel, keine Touchkalibrierung, keine rohen Envelopes/Flashkopie). Import:
Groesse/Format -> Schema -> Integritaet -> getestete Migration -> vollstaendige
typisierte Validierung (unbekannte kritische Felder und nicht unterstuetzte
IANA-Zeitzone = Ablehnung) -> Vorschau/Konflikte -> ausdrueckliche Bestaetigung ->
atomare Aktivierung; keine Teilaktivierung; fehlende Geheimnisse ueberschreiben
nichts; Import nur bei sicher festgestelltem `NoActiveOrRecoverableRun`.

### 5.2 Wiederverwendung

- Aktivierung ausschliesslich ueber den **vorhandenen** Pfad `beginPreview` ->
  `installPreview(origin, ChangeOperation::BackupImport)` -> `validatePreviewForConfirmation`
  -> `confirmPreview` (ADR-018, ein persistenter Linearisierungspunkt). Kein zweiter
  Aktivierungspfad, kein Pending, kein paralleler Active-Zweig.
- Lauf-/Import-Entscheidung im selben `ApplicationCallSerializer`-Guard wie der Commit
  (Anwendungs-Einstieg `confirmBackupImport`); kein zweiter Lock-Owner. Die Pruefung,
  dass der Runstart den Guard betritt, ist Teil von R0 (4.5).
- JSON ausschliesslich ueber die vorhandene, gepinnte cJSON-Codecgrenze
  (`ADOPT_OR_BUILD.md`); kein Zweitcodec. Ausgabe streamend ueber einen kleinen
  Chunk-Writer-Vertrag in `fermentation_app` (`write(data, length) -> bool`); der
  Aktorport `IBinaryOutputSink` ist ausdruecklich **nicht** verwendbar.

### 5.3 Ressourcennachweis B0 (vor jeder Implementierung)

B0 bewertet – ohne Produktcode – das effektive Budget einschliesslich Web-/LVGL-Last
und liefert dem Owner die Entscheidungsgrundlage:

- **Messgroessen:** freier Heap, **groesster zusammenhaengender Block**, minimaler Heap
  unter Web- und LVGL-Last, Main-/HTTP-Task-Stack-HWM, statischer RAM. Bekannte
  Ausgangswerte (Hardware-Evidence PR #170/#174): minimaler freier Heap unter Weblast
  8148 B, Main-Task-Stack-HWM 6056 B, LVGL-Pool 48 KiB. Der groesste zusammenhaengende
  Block unter Last liegt nicht als belastbarer Wert vor.
- **Zu bewertender Maximalfall:** der maximal gueltige externe Kandidat, aus dem
  **gesamten Schema und den unveraenderten Limits** erzeugt (u. a. bis zu
  `kMaximumUserProgramCount` = 12 Benutzerprogramme, Programmkatalog-Payload bis
  `kMaximumProgramCatalogPayloadBytes` = 32768 B, Notizen bis 1024 B). Gueltige Limits
  werden nicht still gekuerzt.
- **Keine unbewiesenen Puffergarantien:** cJSON baut einen vollstaendigen Baum auf; ein
  pauschales Einlesen ganzer JSON-Dateien in RAM wird nicht angenommen. Eine
  Modellrechnung/Hostmessung ersetzt die Geraetemessung nicht; eine Geraetemessung ist
  Hardwareaktion und braucht eine eigene Ownerfreigabe.
- **Ergebnis fuer den Owner:** konkrete Strategie (begrenzter Gesamtbody / Chunking mit
  Vorab-Validator / Export-only) samt Spitzenbedarf, oder die Feststellung, dass der
  Vollumfang nicht passt. Eine Reduktion von Limits oder Funktionsumfang ist eine
  **Ownerentscheidung**, nie ein Default.

### 5.4 Minimal erforderliche Bedien-/Transportpfade und #27-Zustaendigkeit

Ein Export allein ist keine benutzbare Funktion; Backup und Import sind ohne
Dateitransport nicht bedienbar, und das Geraet hat dafuer ausser dem Web keinen Weg
(keine SD-Karte/USB-Anwendung). Daher gilt: **Backup/Import sind in Release 1 nur
ueber die Weboberflaeche nutzbar** und haengen an #27. Minimal noetig (Zustaendigkeit
#27, nicht Teil von #19 und hier nicht vorweggenommen):

1. authentifizierter, lesender Download des Backups (gestreamt);
2. authentifizierter Import in den Schritten Upload mit begrenzter/gestreamter
   Annahme und Validierung -> Vorschau mit Konflikten -> ausdrueckliche Bestaetigung;
3. Sperre/Ablehnung bei nicht sicherem Laufzustand, CSRF/Replay-/Berechtigungs-
   Vertrag, Body-Limits (der heutige Provisionierungspfad begrenzt Bodies auf 1024 B;
   ein Import braucht einen eigenen, bewiesenen Annahmepfad).

#19 liefert nur die anwendungsinternen Erzeuger/Verbraucher samt Nativtests. Ob und
wann #27 die Pfade bereitstellt, ist Voraussetzung fuer die R1-Nutzbarkeit; fehlt
sie, ist Backup/Import in R1 nicht lieferbar (kein falsches `DONE`).

### 5.5 Schnitte (alle bedingt)

| Schnitt | Inhalt | Gate |
|---|---|---|
| **B0** | Ressourcennachweis (5.3), Entscheidungsvorlage | Planfreigabe; danach **Ownerentscheid O-BI** |
| **B1** | Backup-Erzeuger (Whitelist-Projektion, deterministisch, CRC-32 als Integritaetspruefsumme, kein Manipulationsschutz), Sentinel-Tests gegen Geheimnisse | B0 + O-BI |
| **B2** | Import-Service (Validator, Migration, Vorschau/Konflikte, `confirmBackupImport` im Guard, vorhandener Preview-/Commit-Pfad), Powercut-/Konkurrenztests | B0 + O-BI + O3 |

Tests (wenn freigegeben): Backup Golden/deterministisch und Whitelist mit Sentinel-
Geheimnissen (WLAN-/Web-Passwort, PIN-Nachweis, Tokens, SoftAP-Passwort,
Touchkalibrierung); Importmatrix (unbekanntes Schema/kritisches Feld, Typ/Bereich,
nicht unterstuetzte Zeitzone, ueberlang, CRC, abgeschnitten, Migration) mit
unveraendertem Store/`stateRevision` bei jedem Fehler; Import atomar ueber alle
Write-Cutpoints (nach Neustart exakt alter oder neuer Graph); Konkurrenz Start/Import im
Guard; Import ueberschreibt weder Geheimnisse noch Touchkalibrierung noch
Factory-Katalog.

## 6. Zurueckgestellt: Journal, Laufhistorie, Bereinigung, Laufexport (#1–#4)

Status: `DEFERRED_BY_OWNER_PENDING_R1_CONTRACT_RECONCILIATION`. **Keine
Implementierung, keine C0-Messung, keine Budgetkonstanten, keine festgeschriebene
Architektur.** Die bisherigen Entwuerfe (Ring-/Journalvertrag, Laufarchiv,
Aufbewahrung/Bereinigung, Laufexport, Startkriterium) stehen unveraendert in
Anhang A als nicht verbindliche fachliche Referenz fuer eine spaetere
Wiederaufnahme. Vor einer Wiederaufnahme ist eine neue vollstaendige Planrevision
auf dann aktuellem `main` erforderlich; die Konfliktuebersicht (Abschnitt 7) muss
vorher entschieden sein.

## 7. R1-Vertragskonflikte (Delta-/Konfliktuebersicht)

Die Zurueckstellung von #1–#4 steht im Widerspruch zu bestehenden
Release-1-Vertraegen. Diese Uebersicht stellt die Konflikte **transparent** dar. Sie
entschaerft keine Safety-/Startbedingung still und behauptet kein R1-`PASS`/`DONE`.
Entscheidungen sind Ownersache (O-R3); eigenmaechtig wird nichts umdefiniert.

| # | Betroffene kanonische Anforderung (Quelle) | Wirkung der Zurueckstellung | Risiko | Spaetere Ownerentscheidung |
|---|---|---|---|---|
| K1 | **Startbedingung:** Ein neuer Lauf darf nicht starten, wenn "das Fehlerjournal fuer verriegelte Ereignisse nicht verlaesslich ist" (`RESOURCE_BUDGET_AND_MAINTENANCE.md`, "Kritische Stufe vor einem neuen Lauf") | Es gibt kein Journal; das Kriterium ist weder erfuellbar noch heute im Produkt umgesetzt. Ein nicht vorhandenes Journal ist **nicht** `RELIABLE`. | Safety-nahe Startbedingung bleibt unerfuellt; Gefahr, dass sie unbemerkt als erfuellt gilt | (a) Journal (mindestens kritische Klasse) fuer R1 doch umsetzen; (b) Kriterium fuer R1 per dokumentierter, befristeter SSOT-Aenderung/ADR anpassen; (c) R1 ohne dieses Kriterium abnehmen – nur als ausdrueckliche Abweichung |
| K2 | **Abnahme #37 / Gate 4–5** (`ACCEPTANCE_TESTS.md`): Speicherbereinigung innerhalb der Budgets, Exporte parallel stabil, Fehler- und Resetjournal innerhalb des Budgets, Exporte und Diagnose geprueft; 7-Tage-Profil mit Exporten, Bereinigung, Flash-/Historienbelegung, Bereinigungen und Schreibfehlern | Diese Kriterien sind ohne #1–#4 nicht pruefbar; ein R1-`PASS` waere falsch | R1-Abnahme nicht moeglich oder nur mit Abweichungsliste | Abnahmekriterien anpassen oder #1–#4 wiederaufnehmen |
| K3 | **Fehlerinjektion** "Historienspeicher bis zur Bereinigung fuellen" (`ACCEPTANCE_TESTS.md`) und #19-Akzeptanzkriterien (kritische Daten nie vor Komfortdaten loeschen; Speicher bis zur Bereinigung fuellbar; Exporte ohne Geheimnisse) | nicht erfuellbar ohne #1–#4 | #19 nicht abschliessbar | Issue #19 aufteilen/neu zuschneiden (Ownerhandlung auf GitHub) |
| K4 | **#28 haengt formell von #19 ab** (Issue-Abhaengigkeit); #28 umfasst Fehler-/Resetjournal-Anzeige, Lauf-/Diagnose-/Servicebericht-Exporte | #28 bleibt formal blockiert, obwohl nur die zurueckgestellten Teile von #19 relevant sind; Diagnose/Journal-Anzeige (`DIAGNOSTICS_AND_MAINTENANCE.md`, SAFE_BOOT-Oberflaeche "Fehler- und Resetjournal, Exporte") ist ohne Daten leer | falsche Abhaengigkeitslage, falsche Erwartung an #28 | Owner legt neue #28-Abhaengigkeit/Scope fest; der Agent aendert keine Issues |
| K5 | **Ressourcenvertrag** (`REQUIREMENTS.md`, "Ressourcen"): Journal und Historie erhalten feste Budgets; alte nichtkritische Protokolle werden proaktiv bereinigt; kritische Daten haben Vorrang | ohne Journal/Historie gibt es nichts zu budgetieren/bereinigen; die Anforderung bleibt offen, nicht erfuellt | Release-Doku sagt mehr zu, als das Produkt liefert | Dokumente an den R1-Zuschnitt anpassen oder Funktionen liefern |
| K6 | **Aufbewahrungsmodell und Werksreset** (`BACKUP_SECURITY_RETENTION.md`): 5 Detail/50 Zusammenfassungen; Reset loescht Laufhistorie/Fehler- und Komforthistorie | nicht vorhanden; Reset hat dort nichts zu loeschen. Spaeter einfuehrbare Daten muessen die `StorageEpoch` tragen, damit der Reset sie erfasst | Reset-Vertrag ohne Wirkung auf Historie | bei Wiederaufnahme (Anhang A, O4) |
| K7 | **Backup/Import** als dokumentierte R1-Funktion | nur bedingt (B0/O-BI) und nur ueber #27-Pfade nutzbar | R1 liefert Backup/Import eventuell nicht | Entscheid nach B0 (O-BI) |

Weder dieser Plan noch sein PR setzen ein R1-Abnahme-`PASS`. Die Kennung
`DEFERRED_BY_OWNER_PENDING_R1_CONTRACT_RECONCILIATION` bleibt bestehen, bis der Owner die
Konflikte K1–K7 entschieden hat; das ist die "notwendige spaetere R1-Scopeentscheidung".

## 8. Verbleibende echte Owner-Gates

| ID | Entscheidung | Alternativen | Empfehlung | Zeitpunkt |
|---|---|---|---|---|
| **G0** | Freigabe der exakten Plan-SHA | – | – | vor R0 |
| **O-R1** | Ausloeser des PIN-unabhaengigen Vollresets B (physischer Recoveryweg) | **A** eigene Boot-Touchgeste, getrennt von der 10-Sekunden-Raw-Touch-Kalibrierung (Geste/Schwellen `TBD_HARDWARE`, Scope #31, Verwechslungsschutz noetig); **B** bewusst tief liegende lokale Funktion auf dem PIN-Eingabebildschirm ("PIN vergessen") mit denselben Warnstufen und langer Bestaetigung (keine neue Hardwareannahme; Ausloesung nur durch physischen Touch am Geraet); **C** in R1 nur `SAFE_BOOT`-Eintritt plus UART-Neuflashen (erfuellt R1-PFLICHT fuer ein gesundes Geraet mit vergessener PIN nicht) | **B** (nur die Ausloesung ist eine Bedienentscheidung; Kern und Schutzstufen bleiben identisch; A kann spaeter ergaenzt werden) | vor R3 |
| **O-R2** | Bedingt: PIN-Quelle fuer Ablauf A, falls R0 keinen lokalen PIN-geschuetzten Servicebereich im Code belegt | **A** vorhandene lokale PIN-Pruefung wiederverwenden; **B** Ablauf A zunaechst nur ueber die lokal verifizierte PIN-Eingabe (`device_ui_pin` + Authentication-Records) ohne Servicebereich | nach R0-Befund | nach R0 |
| **O-BI** | Funktionsumfang und Strategie von Backup/Import nach B0 | Gesamtbody / Chunking mit Vorab-Validator / Export-only / Nichtlieferung in R1; jede Reduktion von Limits ist eigene Entscheidung | keine Vorabwahl | nach B0 |
| **O3** | Bedingt (nur bei B1/B2): geraetegebundene Daten im Backup (`sensorCommissioning` mit ROM-Bindung, Planerparameter) | **A** ausschliessen; **B** mit Warnung; **C** mit Zusatzbestaetigung | **A** | vor B1 |
| **O-R3** | R1-Vertragsabgleich K1–K7 (Abschnitt 7) und Zuschnitt von #19/#28 | Journal fuer R1 doch umsetzen / R1-SSOT per ADR anpassen / Abweichung ausdruecklich akzeptieren | – (nicht vom Plan entschieden) | vor R1-Abnahme, vor Wiederaufnahme von #1–#4 und vor jeder Aenderung von #19/#28 |
| **O-HW** | Hardware-Folgeissue fuer die physische Verifikation von Reset (und spaeter Backup/Import) | **A** Folgeissue wie #190 zu #30; **B** Hardwarenachweise bleiben in #19 offen | **A** | spaetestens vor R4; der Agent legt ohne Ownerauftrag kein Issue an |

Entfallen/verlagert in Anhang A (nur bei Wiederaufnahme von #1–#4): Speicherstruktur
(NVS-Ringe/LittleFS), Journal nach Werksreset (O4), feste vs. konfigurierbare
Aufbewahrung (O6, inkl. Abweichung von der dokumentierten Konfigurierbarkeit),
Journal-Aktivierung (O7), Exportumfang (O10).

## 9. Dokumentationswirkung

Nur wenn die jeweiligen Schnitte umgesetzt werden: `LOCAL_UI_SETTINGS_SERVICE.md`
(Ausloeserkonzept B, O-R1), `ACCEPTANCE_TESTS.md` (SIM-R-01..11, physische Tests
`NOT_RUN`; SIM-26-07-Zuordnung), `OPEN_POINTS.md` (Zurueckstellung #1–#4,
`TBD_HARDWARE` fuer Geste/Dauer), `BACKUP_SECURITY_RETENTION.md` nur bei B-Schnitten,
`docs/ROADMAP.md` (Status/Reihenfolge). **Keine ADR** in diesem Auftrag; eine ADR
entsteht nur auf ausdruecklichen Ownerauftrag (z. B. bei O-R3 Variante "SSOT per ADR
anpassen"). Die Konfliktuebersicht K1–K7 gehoert in dieses Planungsdokument und
wird nicht in kanonische Dokumente kopiert, bevor der Owner entschieden hat.

## 10. Risiken

| Risiko | Wirkung | Gegenmassnahme |
|---|---|---|
| Resetkern prueft Lauf/PIN/Ursprung nicht | Reset koennte fehlerhaft ausloesbar sein | Vorbedingungen unter dem Guard im neuen Ablauf, SIM-R-02..05, R0-Pruefung |
| Ablauf B ohne PIN | zu leichte Ausloesbarkeit | Mehrfachwarnung + lange Bestaetigung, nur lokal, Aktoren AUS, O-R1; keine PIN-Rueckstellung ohne Reset |
| Ausloeser B / Dauer / Geste sind `TBD_HARDWARE` | Verifikation nicht moeglich | O-R1, Hardware `NOT_RUN`, O-HW |
| Zurueckgestellte Journal-Startbedingung (K1) | Safety-nahe Anforderung bleibt unerfuellt | transparent in Abschnitt 7; kein falsches `PASS`; O-R3 |
| RAM-Rahmen fuer Backup/Import (8148 B Minimum unter Weblast, grosse Kataloge) | Funktion evtl. nicht lieferbar | B0-Nachweis vor Umsetzung, O-BI, keine stillen Limit-Kuerzungen |
| #27-Abhaengigkeit von Backup/Import | R1-Nutzbarkeit ungeklaert | 5.4, Voraussetzung explizit, kein falsches `DONE` |
| Fruehere Entwurfsinhalte nicht auffindbar | moegliche verlorene Anforderungen | Plan stuetzt sich auf Issue-Scope und kanonische Dokumente; Reviewer vergleicht, falls der Owner den Entwurf liefert |

## 11. Grenzen

- **#28** bleibt Diagnose-/Service-/Exportgate; formale #19-Abhaengigkeit siehe K4.
- **#27**: Web-Transport, Routen und Body-/Streaming-Annahme fuer Backup/Import liegen
  dort; #19 liefert nur anwendungsinterne Bausteine.
- **Hardware**: keine Hardwaretests, keine Aktorfreigabe, kein Flash-Geraet;
  `ACTUATOR_RELEASE=NO`; keine Messung in diesem Auftrag.
- Release-1-Abgrenzung (`SPECIFICATION_REVIEW.md`): kein OTA, kein
  Roh-Flash-Backup ueber das Web.

## 12. Checkliste Planfreigabe

- [ ] Plan-Fix-Verification ohne offene Blocker
- [ ] exakte Plan-SHA vom Owner freigegeben (G0)
- [ ] O-R1 vor R3; O-R2 nach R0; O-BI nach B0; O3 vor B1; O-HW spaetestens vor R4
- [ ] O-R3 (R1-Vertragsabgleich) bis zur R1-Abnahme entschieden
- [ ] Issue #19 bleibt offen; keine Aenderung an Issues durch den Agenten

## Anhang A – Zurueckgestellte fachliche Referenz (Revision 2, nicht verbindlich)

> **Status: `DEFERRED_BY_OWNER_PENDING_R1_CONTRACT_RECONCILIATION`.** Dieser Anhang bewahrt die
> Entwuerfe aus Planrevision 2 fuer Journal, Laufhistorie, Bereinigung, Laufexport und das
> Startkriterium. Er ist **weder freigegeben noch Umsetzungsvorgabe**; keine Konstante, kein
> Format und kein Schnitt daraus ist beschlossen. Bei einer Wiederaufnahme ist eine neue
> vollstaendige Planrevision auf aktuellem `main` noetig. Abschnittsverweise in diesem Anhang
> (z. B. "4.4", "O7", "C0") beziehen sich auf Revision 2 und sind hier nur historische Marken;
> Abschnitt 4–5 dieses Plans (Reset, Backup/Import) ersetzen die Revision-2-Schnitte C5/C6.
> Bei Wiederaufnahme zu entscheiden waeren u. a.: Speicherstruktur (NVS-Ringe, LittleFS, Hybrid), Verbleib
> des kritischen Journals nach Werksreset, feste vs. konfigurierbare Aufbewahrung (Abweichung von
> der dokumentierten Konfigurierbarkeit), Aktivierung/Startkriterium, Exportumfang.

### A.4.3 Journal und Ringvertrag (kritisch vs. Information)

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

### A.4.4 Verlauf, Laufarchiv, Zusammenfassungen, Aufbewahrung

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

### A.4.5 Laufexport (JSON/CSV)

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

### A.4.7 Startkriterium "Journal verlaesslich" und Aktivierung

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

### A.6 Tests (Revision 2, Referenz)

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
