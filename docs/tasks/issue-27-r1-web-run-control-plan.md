# Issue #27 – R1-Webbedienung: Run-Mutationsroute produktiv komponieren (Plan)

Status: `PLAN_PROPOSED` – keine Implementierung vor Ownerfreigabe der exakten
Plan-SHA. Basis: `main` bei `b390207`. Scope: genau Issue #27.

## 1. Ziel und Nicht-Ziele

Ziel: Die in PR #170 gemergte, aber nicht registrierte Mutationsroute
`POST /internal/ui/run` wird produktiv in den `WebRouteDispatcher` komponiert
und hardwarefrei nachgewiesen. Web nutzt dieselben Application-Einstiege wie
Touch (`prepareEnvelope` -> `confirmPrepared` -> `applyConfirmedPrepared`),
dieselben erwarteten Revisionen und dieselbe Persistenz. Keine zweite
Fachlogik, kein Web-State-Owner. Die kleine Shell erhaelt die minimal noetige
Bedienung.

Nicht-Ziele (gehoeren nicht in diesen PR):

- Funktionen und Abhaengigkeiten aus #19 und #28, Journal, Historie,
  Backup/Import, Laufexport;
- Hardware-Commissioning, Flash, Hardwaretests, Aktorverdrahtung/-freigabe
  (`ACTUATOR_RELEASE=NO` bleibt), R_IS/L_IS bleibt `FUTURE_RELEASE`;
- Servicebereich im Web (PIN-Lease), Konfigurationsaenderungen, Moduswechsel
  Passwortschutz, Werksreset (bleibt bewusst nicht ueber HTTP erreichbar);
- neue Auth-/Session-/Replay-Mechanik, persistente Anmeldung, Frameworks;
- neue Wireformate, mit genau einer ownerabhaengigen Ausnahme: der
  Read-only-Response `GET /api/v1/programs` (C4, nur bei D2 = A);
  Request-Format (`v`,`r`,`i`) und bestehende Responses bleiben unveraendert;
- erfundene PI-/Safety-/GPIO-Werte; OTA.

## 2. Verifizierte Ausgangslage (Repository `b390207`)

| Befund | Beleg |
|---|---|
| Handler `WebRunMutationHandler::handle` vollstaendig implementiert: Methode, Metadaten, Content-Type, Same-Origin, Body, Codec (`v`,`r`,`i`), Replay-Digest, Cookie/Session, CSRF, Mutation-Sequenz, `touch`, `reserveMutation`, Prepare/Confirm/Apply, `completeMutation` | `web_application_routes.cpp:489-` |
| Route ist **nicht** registriert: `WebRouteDispatcher::handle` endet mit `return false`; Dispatcher hat kein Handler-Mitglied | `web_application_routes.cpp:931-`, `web_application_routes.hpp` |
| Ein Test erzwingt heute die Nicht-Erreichbarkeit | `test_provision_route_does_not_open_the_run_mutation_route` |
| Handler prueft `webAuthenticationState()` **nicht**; nur `handleReadOnly` sperrt `Unprovisioned`/`RecoveryRequired`/`Indeterminate` mit 503 | `handleReadOnly` |
| Der POST bestaetigt intern (Prepare+Confirm+Apply in einem Request); es gibt keinen serverseitigen Zwei-Phasen-Zustand | Handler |
| Handler-Tests existieren (Session/CSRF/Same-Origin/JSON, Replay, stale, Manuallaeufe 503 ohne Limits-Owner, ack/mute) – aber nur gegen den Handler, nicht ueber den Dispatcher | `test/test_web_application_routes` |
| `ApplicationCallSerializer` (rekursiv) serialisiert jede oeffentliche Application-Methode; zwischen Prepare/Confirm/Apply kann Touch einschieben -> Ausdruck ueber Revisionen/Revalidierung | `fermentation_application.hpp/.cpp` |
| Read-only `/api/v1/status` liefert `revisions` (`stateSequence`, `run`, `messages`, `fault`, `recoveryEpisode`, `userConfiguration`, `programCatalog`), `/alerts` liefert `id` und `revision` je Meldung | `web_json_codec.cpp:875-1017` |
| **Luecke:** Es gibt keine Programmliste; `start-program` braucht eine `programId`, die ein Client nicht kennt | Codec / API-Routen |
| **Luecke:** Der Browser kennt die naechste Mutationssequenz nicht. `WebSessionManager::mutationSequence()` liefert `Available(nextMutationSeq)`, `InFlight(inFlightMutationSeq)` oder `Exhausted`, wird aber weder von Shell, `/status`, Login noch POST-Resultaten ausgegeben. `reserveMutation` akzeptiert nur exakt `highWater+1` (Gap/Reused/Expired/Conflict -> 409 `sequence-conflict`); eine Sequenz wird erst nach `reserveMutation` verbraucht (auch bei 409-stale/422/503-Outcome), nicht bei Fehlern davor (400/401/403/415) | `web_session.cpp`, `reservationError` |
| Die Shell wird serverseitig pro Request mit Session gerendert und bettet bereits das CSRF-Token ein (`handleShell`/`webShell`); derselbe Mechanismus kann die Sequenz einbetten, ohne neuen Endpunkt | `web_application_routes.cpp` |
| Persistenzergebnisse werden unterschiedlich projiziert: `WriteFailed` -> 500 `write-failed` (nur mit `OwningOutcome`+`Rejected`); `PersistenceIndeterminate`, `PersistenceCommittedApplyFailed`, `RecoveryPending`, `NotInitialized`, `Blocked` -> 503 `unavailable`; `CapacityExceeded` -> 413. `PersistenceCommittedApplyFailed` bedeutet: Durability `MayHaveChanged`, Koordinator-Zustand gesperrt (Recovery/Service), es kann bereits dauerhaft geschrieben sein | `projectCommandResult`, `fermentation_application.cpp:2395-2410`, `run_persistence_coordinator.cpp` |
| Heute positiv ueber den echten Application-Pfad nachgewiesen im Web-Testfixture: `ack-message`/`mute-message` (RAM-owned). Programmstart/Stop/Abschluss mit Persistenz haben dort **keinen** positiven Web-Test; Manuallaeufe, `abort-and-cool` und Abschluss mit Kuehlstart sind durch `kManualRunLimitsOwnerAvailable=false` gesperrt (503) | `test_web_application_routes`, `fermentation_application.cpp:159-385` |
| Shell (max. 4096 Byte) hat nur Login/Logout/Polling, keine Mutationssteuerung | `webShell` |
| Vier-Session-/RAM-Gate bestanden | `docs/ROADMAP.md`, `docs/audits/PR170_HW_GATE_20261005/` |

Historische Plaene (`docs/tasks/issue-27-*`) wurden nur als Herkunft gelesen;
massgebend sind Code, `docs/WEB_UI.md`, ROADMAP und Live-Issue.

## 3. Betroffene Module, Vertraege, Gates

- `fermentation_app` (nur gegen abstrakte Ports): `web_application_routes`,
  ggf. `web_json_codec` (nur falls D2 = A). Keine Aenderung an
  `device_platform*`.
- Vertraege: `docs/WEB_UI.md` (Gleichbehandlung, Revisionsschutz, Quelle
  `web`), Replay-/Session-Vertrag PR #170, Touch-Bestaetigungssemantik.
- Gates: ADR-013-Architekturcheck (`check_architecture_boundaries.py`),
  clang-tidy-Liste im Pre-Ready-Skript, ESP-IDF-Build, Shell <= 4096 Byte,
  `WebSessionManager` <= 4096 Byte (unveraendert).

## 4. Vorgeschlagene Schnitte (je Commit anhalten, Ownerfreigabe)

**C1 – Dispatcher-Komposition der Route (Kern).**
`WebRunMutationHandler` als Dispatcher-Mitglied; `handle` leitet
`/internal/ui/run` an ihn weiter. Vorgeschaltetes Auth-State-Gate wie bei
read-only: `Unprovisioned`/`RecoveryRequired`/`Indeterminate` -> 503
`authentication-unavailable`, ohne Session-Lookup, Digest oder Application-
Aufruf. Setup-Flow behaelt Vorrang (Route dort nicht erreichbar). Kommentare
in Header/Dispatcher entsprechend anpassen. Der Negativtest zur Nicht-
Erreichbarkeit wird zum Positivtest.

**C2 – Nativer Integrations-/Negativnachweis ueber den Dispatcher.**
Tests im bestehenden `test_web_application_routes` mit vorhandenen Fakes
(kein neues Test-Framework). C2 testet nur ueber direkte HTTP-Requests
(vorhandene Request-Helfer); die Shell-Bedienung aus C3/C4 wird nicht
vorausgesetzt.

Zuerst ein kurzer Erreichbarkeits-Check (Teil von C2, Ergebnis im
Commit-Text): Welche Run-Aktionen sind unter der heutigen Limits-/Safety-
Evidence ueber den produktiven Application-Pfad mit simulierter owning
Evidence (Sensoren, Safety-Gate, Katalog) positiv erreichbar?

- **Garantiert positiv:** `ack-message`, `mute-message` (existierender
  RAM-owned Pfad) ueber den Dispatcher inkl. Replay desselben Requests ohne
  zweite Wirkung.
- **Bedingt positiv:** `start-program`, `stop-run` (ohne Kuehlung) und
  `complete-run` (ohne Kuehlstart) nur, wenn der Check zeigt, dass Katalog und
  owning Evidence im nativen Fixture ohne Commissioning-Werte und ohne
  Gate-Umgehung bereitstellbar sind. Dann: Start, Stop, Abschluss als
  Positivfaelle; Konkurrenz «zwei Sessions starten dasselbe Programm -> genau
  ein Start erfolgreich, der andere stale».
- **Sonst (Fallback, ohne Anspruch auf Programmstart-Erfolg):** die Stale-/
  Doppelwirkungsinvariante am zulaessigen Pfad: zwei Sessions quittieren
  dieselbe Meldung mit gleicher Revision -> genau eine Wirkung (zweite stale
  oder `NoChange` laut Application), Touch-seitige Aenderung zwischen
  Statuslesen und POST -> 409, kein Zustandswechsel. `start-program` wird
  separat nur fail-closed geprueft (unbekanntes Programm 422, stale
  Katalogrevision 409, fehlende Sensor-/Safety-Evidence -> typisierte
  Ablehnung, Zustand und Persistenz unveraendert).
- **Dauerhaft gesperrt (503, unveraendert):** Manuallaeufe, `abort-and-cool`,
  Abschluss mit Kuehlstart; bestehender Test bleibt, wird um den Dispatcher-
  Pfad ergaenzt.
- **Auth/CSRF/Replay:** fehlende/abgelaufene Session 401; falsches CSRF 403;
  fremder Origin 403; falscher Content-Type 415; Auth-State-Gate (503 bei
  `Unprovisioned`/`RecoveryRequired`/`Indeterminate`, kein Digest, kein
  Application-Aufruf); `password-disabled` mit Session ok, anonym 401;
  Setup-Flow-Vorrang; Replay mit identischem Body liefert identischen
  Outcome, geaenderter Body bei gleicher Sequenz -> 409.
- **Mutationssequenz (Ergaenzung, siehe C3):** `Available` -> naechste
  Sequenz exakt `highWater+1`; `InFlight` (gleiche Sequenz, gleicher Digest ->
  409 `sequence-conflict`; andere Sequenz -> 409); Gap, Reused, ReplayExpired
  (nach 8 Outcomes), `Exhausted` -> 409 ohne Application-Aufruf und ohne
  Verbrauch; Sequenz wird nach Outcomes 200/409-stale/422/503 verbraucht,
  nach 400/401/403/415 nicht; zwei Sessions haben unabhaengige Sequenzen.
- **Persistenz-Negativmatrix (je Fehlerklasse, getrennt):**
  | Klasse | Antwort | Erwarteter unveraenderter Vertrag / Nachbehandlung |
  |---|---|---|
  | `WriteFailed` (`OwningOutcome`+`Rejected`) | 500 `write-failed` | Run-Zustand und Application-Revision unveraendert; Replay liefert dieselbe 500 |
  | `CapacityExceeded` | 413 | unveraendert |
  | `PersistenceIndeterminate` | 503 `unavailable` | kein Erfolg behauptet, kein Nichtcommit behauptet; Koordinator bleibt in Recovery/Indeterminate; weitere Mutationen werden 503/typisiert abgelehnt; Nachbehandlung nur ueber Status/Recovery, nicht durch Wiederholung mit neuer Sequenz |
  | `PersistenceCommittedApplyFailed` | 503 `unavailable` | Durability `MayHaveChanged`: es kann bereits dauerhaft geschrieben sein; Test prueft nur Antwort, gesperrten Koordinatorzustand (weitere Run-Mutationen abgelehnt) und dass das Replay-Outcome 503 bleibt; keine Behauptung eines Nicht-Commits |
  | `RecoveryPending`/`NotInitialized`/`Blocked` | 503 | unveraendert, fail-closed |
  Die Fehlerklassen werden mit den existierenden Fault-Injection-Hilfen des
  Fixtures (StateStore-Fehler) erzeugt, soweit der Pfad per Application
  erreichbar ist; sonst bleibt der Projektionstest (bestehende Matrix) der
  Nachweis und die Luecke wird im Commit-Text benannt.
- **Quelle:** der Kontext traegt `WebInterface` (Protokollquelle `web`).
- **Ressourcen:** bestehende Vier-Session-Konstanten werden referenziert, nicht
  wiederholt; `static_assert`/Groessenreport bleibt gruen.

**C3 – Minimale Shell-Bedienung (nur wenn D1/D4 entschieden).**
Im 4096-Byte-Budget: Anzeige aktueller Lauf-/Meldungsstatus aus den
bestehenden APIs; Buttons Stop, Abschliessen, Quittieren, Stumm; jede Mutation
erst nach zweitem expliziten Klick (clientseitige Bestaetigung, kein
`window.confirm`), sendet `X-CSRF-Token`, `X-Mutation-Seq`, Revisionen aus dem
letzten Status. Sprachen de/en/es wie bisher. Falls das Budget nicht reicht:
Messung vorlegen, Ownerentscheid (nicht still Limit anheben).

*Sequenz-Bootstrap und Resynchronisation (nutzt nur vorhandene Vertraege):*

1. `handleShell` bettet bei gueltiger Session neben dem CSRF-Token das
   Ergebnis von `WebSessionManager::mutationSequence()` ein (`Available` ->
   `nextMutationSeq`; `InFlight`/`Exhausted` -> Bedienung deaktiviert,
   Hinweis «Neu laden»). Kein neuer Endpunkt, kein neues JSON, keine Aenderung
   an Login/Status/POST-Responses. Die Abfrage erneuert die Idle-Frist nicht.
2. Der Client haelt `nextSeq` nur im Seitenspeicher; kein `localStorage`, kein
   persistentes Login. Pro Zeitpunkt ist hoechstens ein Request in Flight.
3. Nach `200`, `409 stale`, `409 busy`, `409 confirmation-required`, `413`,
   `422` und `500 write-failed` (Antworten mit Outcome aus
   `completeMutation`) wird `nextSeq` um 1 erhoeht und der Status neu
   geladen. Nach `400/401/403/415` (vor Reservierung) bleibt `nextSeq`.
4. Nach `409 sequence-conflict` (Gap, Reused, Expired, InFlight, mehrere
   Tabs/Reload mit veraltetem Wert), nach jedem `503` (nicht unterscheidbar,
   ob vor oder nach der Reservierung) und nach `Exhausted`: kein Folge-Request
   mit neuer Sequenz; die Seite wird neu geladen (`GET /`), was den Wert aus
   Punkt 1 neu liest. Der Nutzer entscheidet danach anhand des frischen
   Status erneut; die Revisionspruefung verhindert Doppelwirkung.
5. Unsicherer Ausgang (Timeout/Netzfehler ohne Antwort): hoechstens ein
   Wiederholungsversuch mit **byte-identischem Body und derselben Sequenz**
   (Replay liefert den gespeicherten Outcome bzw. 409 `InFlight`); danach
   Neuladen wie in 4. Keine blinde Wiederholung mit neuer Sequenz.
6. Tests: native Dispatcher-Tests fuer Shell-Einbettung (Available, InFlight,
   Exhausted, mehrere Sessions, kein `touch`), Replay byte-identisch, Konflikt
   bei zweitem Tab; Verhalten der Client-Skriptlogik wird mangels Browser im
   Gate als Textvertrag der Shell (erwartete Marker, Groessenlimit) geprueft;
   echter Browsertest bleibt `NOT_RUN`.

**C4 – Programm-Start (nur bei D2 = A).** Die Ausnahme von «keine neuen
Wireformate» ist eng begrenzt: ein neuer *Read-only-Response*
`GET /api/v1/programs`, Request-Vertrag und uebrige Responses unveraendert.
Vertrag: Quelle ist ausschliesslich der kanonische Programmkatalog-Owner der
Application (aus dem `uiSnapshot`; kein zweiter Katalog, keine Kopie); je
Eintrag nur `id` und Anzeigename; Feld `catalogRevision` (gleiche
Dezimalstring-Konvention wie `revisions.programCatalog`); begrenzt auf
`kMaximumWebApiResponseBodyBytes` (3072) und eine feste Maximalzahl, bei
Ueberschreitung 503 statt Kuerzung; GET-only, Session-/Auth-State-Gate wie die
uebrigen Read-only-Routen; ohne Katalog (Fehler/leer) leere Liste bzw. 503,
**keine** Platzhalter oder Geraetedaten. Ohne D2 = A wird C4 aus diesem Slice
genommen und `start-program` bleibt am Touch; C1–C3 bleiben unberuehrt.
`start-program`-Steuerung in der Shell nur, wenn das 4096-Byte-Budget haelt,
sonst Messung und Ownerentscheid.

**C5 – Dokumentation.** `docs/ROADMAP.md` (zu Beginn des PR und nach Merge),
`docs/WEB_UI.md` (Mutationsroute produktiv, Gate-Hinweis zum Vier-Session-
Nachweis als erfuellt referenziert), `docs/ACCEPTANCE_TESTS.md` nur fuer neue
SIM-Trace-Zeilen. Keine Anforderungskopien.

Gezielte lokale Tests im Draft: `test_web_application_routes`,
`test_web_session`, `test_web_json_codec` (falls vorhanden),
direkt betroffene Konsumenten (`test_http_server_lifecycle`), ESP-IDF-Build
der betroffenen Targets, Architekturcheck. Voller Pre-Ready-Lauf nur nach
Independent Review, `OPEN_BLOCKERS=0` und Ownerfreigabe. Nicht ausgefuehrte
Tests gelten als nicht bestanden; Hardware: `NOT_RUN`.

## 5. Priorisierung weiterer R1-Webfunktionen aus #27 (nicht in diesem PR)

| Prio | Funktion | Behandlung |
|---|---|---|
| R1-Pflicht, diese Planung | Laufbedienung Stop/Abschluss/Quittieren/Stumm | C1–C3 |
| R1-Pflicht, Owner-abhaengig | Programmstart | C4 bei D2 = A |
| Folge-PR #27 | Fehler zuruecksetzen, Sensorauswahl, Recovery-Zeitkorrektur, Anpassung laufender Laeufe (Codec kennt Intents; Shell-Bedienung folgt) | eigener Schnitt, kleine PRs |
| Folge-PR #27 | Konfiguration (Programme, Netzwerk), Servicebereich mit PIN-Lease | separat, braucht Konfigurationsvertraege |
| Ausserhalb | Journal/Historie/Backup/Import (#19), #28-Funktionen | nicht Teil von #27-Run-Control |
| Spaeter | Manuelle Laeufe | Application liefert heute 503 ohne Limits-Owner; kein Web-Vorgriff |

## 6. Offene Ownerentscheidungen

Alle D1–D4 sind Empfehlungen; es wird keine Entscheidung und keine
Freigabe unterstellt. Bis zur Ownerentscheidung wird nur C1/C2 vorbereitet
(Freigabe der Plan-SHA vorausgesetzt); in C1/C2 benoetigte Tests laufen
ausschliesslich ueber direkte HTTP-Requests und setzen keine Shell-
Bedienung (C3) und keine Programmliste (C4) voraus.

- **D1 – Bestaetigungsmodell.** Empfehlung: Der POST ist die Bestaetigung (wie
  heute implementiert, kein serverseitiger Pending-State, kein Zusatz-RAM);
  Sicherheitsabfrage ausschliesslich clientseitig durch zweiten Klick.
  Alternative: serverseitige Zwei-Phasen-Bestaetigung (neuer Zustand, Replay-
  und RAM-Wirkung, eigener Vertrag).
- **D2 – Programmliste fuer `start-program`.** Empfehlung A: bounded
  `GET /api/v1/programs` in diesem PR (C4). B: Programmstart bleibt Touch-
  only, Web in R1 ohne Start (Stop/Quittieren/Stumm zuerst). Ohne Liste ist
  Web-Start nicht sinnvoll bedienbar.
- **D3 – Routenpfad.** Empfehlung: `POST /internal/ui/run` unveraendert
  beibehalten (Codec, Tests, Replay-Digest und PR-#170-Nachweis beziehen sich
  darauf); Umbenennung nach `/api/v1/...` waere ein Vertragswechsel.
- **D4 – Umfang der ersten Shell-Bedienung.** Empfehlung: Stop, Abschliessen,
  Quittieren, Stumm; Fehler-Reset, Sensorauswahl, Recovery-Zeitkorrektur
  folgen separat.

Keine dieser Entscheidungen blockiert C1/C2; C3 wartet auf D1/D4, C4 auf D2.
`ACTUATOR_RELEASE=NO`; kein Flash, keine Hardwaretests (`NOT_RUN`).

## 7. Risiken

- Shell-Budget 4096 Byte (Messung in C3, kein stilles Anheben).
- Mutationssequenz-Resynchronisation (mehrere Tabs, Reload, verlorene
  Antwort): durch Einbettung in die Shell und Reload-Regel geloest, aber nur
  ueber Textvertrag und Dispatcher-Tests nachweisbar; Browserverhalten bleibt
  `NOT_RUN`.
- Persistenz-Teilfehler (`PersistenceCommittedApplyFailed`): Web meldet nur
  503; Nachbehandlung bleibt Recovery/Service, keine Web-Wiederholung.
- Interleaving Touch/Web zwischen den drei Application-Aufrufen: abgesichert
  durch Revisionen/Revalidierung; Nachweis in C2, kein neuer Lock um den
  Gesamtablauf (waere Blockade der Hauptschleife).
- Fehlende Sensor-/Safety-Voraussetzungen duerfen nie zu Aktorfreigabe
  fuehren: es werden nur bestehende Application-Entscheidungen genutzt;
  Hardwarewirkung bleibt `NOT_RUN`.
- Replay-Cache hat nur 8 Outcomes je Session: Verhalten bei Ueberlauf ist
  bereits PR-#170-Vertrag und wird nicht veraendert.
