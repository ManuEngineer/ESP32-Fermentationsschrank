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
- neue Wireformate, neue Auth-/Session-/Replay-Mechanik, Frameworks;
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
(kein neues Test-Framework):

- Positiv: Login -> Status lesen -> `ack-message`/`mute-message` und
  `stop-run`/`complete-run` (soweit ohne Limits-Owner zulaessig) mit
  Revisionen aus `/api/v1/status`; Replay desselben Requests liefert
  identisches Ergebnis ohne zweite Wirkung.
- Auth/CSRF/Replay: fehlende/abgelaufene Session 401; falsches CSRF 403;
  fremder Origin 403; falscher Content-Type 415; Sequenz-Luecke/-Wiederholung/
  In-Flight; geaenderter Body bei gleicher Sequenz; `password-disabled`
  funktioniert mit Session, anonym nicht.
- Stale: Revision aus `/status`, dann Touch-seitige Aenderung ueber dieselbe
  Application, dann Web-POST -> 409 stale, keine Mutation; umgekehrt
  Web zuerst, Touch mit alter Revision wird abgelehnt (Doppelausfuehrung
  verhindert).
- Konkurrenz: zwei Web-Sessions starten dasselbe Programm -> genau eine
  Ausfuehrung, die andere 409.
- Fail-closed: kein/ungueltiger Sensor, offenes Safety-Gate, Recovery/
  Indeterminate -> typisierte Ablehnung (409/422/503), Zustand und Persistenz
  unveraendert; Persistenzfehler -> 500 `write-failed`, kein Teilzustand.
- Quelle: Journal-/Audit-Kontext traegt `WebInterface`.
- Ressourcen: bestehende Vier-Session-Konstanten werden referenziert, nicht
  wiederholt; `static_assert`/Groessenreport bleibt gruen.

**C3 – Minimale Shell-Bedienung (nur wenn D1/D2 entschieden).**
Im 4096-Byte-Budget: Anzeige aktueller Lauf-/Meldungsstatus aus den
bestehenden APIs; Buttons Stop, Abschliessen, Quittieren, Stumm; jede
Mutation erst nach zweitem expliziten Klick (clientseitige Bestaetigung, kein
`window.confirm`), sendet `X-CSRF-Token`, `X-Mutation-Seq` (monoton, aus
Antwort/Zustand der Seite), Revisionen aus dem letzten Status; bei 409 Neu-
laden des Snapshots statt blindem Wiederholen. Sprachen de/en/es wie bisher.
Falls das Budget nicht reicht: Messung vorlegen, Ownerentscheid (nicht
still Limit anheben oder Shell aufblasen).

**C4 – Programm-Start (nur bei D2 = A).** Bounded read-only
`GET /api/v1/programs` (id, Anzeigename, Katalogrevision) im bestehenden
Codec-Stil, Body <= `kMaximumWebApiResponseBodyBytes`; `start-program`-
Steuerung in der Shell nur, wenn das 4096-Byte-Budget haelt, sonst Messung
und Ownerentscheid.

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

Keine dieser Entscheidungen blockiert C1/C2; C3/C4 warten auf D1/D2/D4.

## 7. Risiken

- Shell-Budget 4096 Byte (Messung in C3, kein stilles Anheben).
- Interleaving Touch/Web zwischen den drei Application-Aufrufen: abgesichert
  durch Revisionen/Revalidierung; Nachweis in C2, kein neuer Lock um den
  Gesamtablauf (waere Blockade der Hauptschleife).
- Fehlende Sensor-/Safety-Voraussetzungen duerfen nie zu Aktorfreigabe
  fuehren: es werden nur bestehende Application-Entscheidungen genutzt;
  Hardwarewirkung bleibt `NOT_RUN`.
- Replay-Cache hat nur 8 Outcomes je Session: Verhalten bei Ueberlauf ist
  bereits PR-#170-Vertrag und wird nicht veraendert.
