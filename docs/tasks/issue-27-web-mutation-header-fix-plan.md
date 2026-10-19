# Issue #27 – kanonischer Web-Mutationsheader

**Stand 2026-10-19: PLAN_ONLY / PLAN_REVIEW_PENDING.**
Ownerauftrag: ausschliesslich Planung dieses Header-Fixes; Implementierung erst
nach unabhaengiger Planpruefung und ausdruecklicher Freigabe der exakten
Plan-Commit-SHA. [Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27)
bleibt offen; [Draft-PR #205](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/205),
Branch `agent/issue-27-web-mutation-header-fix-plan`.

## Basis und Vertrag

Live geprueft: `main=288a6f515e13e0334c46cbf6cdc3dc5b4753925d`, Merge von
PR #203 aus `4f532073ebecf4e680d8f009c441447b76fb50ef`. Issue #27 OPEN;
kein anderer offener Folge-PR beim Start. Der neue Branch basiert auf genau
diesem `main`; seine erste Aenderung fuehrt den Post-Merge-Status in
[ROADMAP](../ROADMAP.md) nach. Der letzte kanonische #203-Handover bestaetigt
Full Review GO / OPEN_BLOCKERS=0 und `pre-ready/local=success` auf `4f53207`;
diese historischen Ergebnisse sind keine Verifikation dieses Folge-Fixes.

Im echten `send(i)` sendet die Shell `X-Mutation-Seq: String(n)` mit `n=Q`.
Der bestehende ESP-IDF-Adapter liest ausschliesslich `X-UI-Mutation-Seq` in
`request.metadata.mutationSeq`; ohne diesen Wert antwortet der Run-Handler
`400 sequence-required`. Zu korrigieren ist allein der Clientname. Wert,
POST-Body, CSRF, Session, Sequenzfortschritt, Replay, vorhandenes Retryverhalten,
Bestaetigung und Single-Flight/Refresh-Aktionssperre bleiben unveraendert.

Verbindliche Quellen: [Workflow](../AGENT_WORKFLOW.md),
[Engineering](../ENGINEERING_PRINCIPLES.md),
[CI/Gates](../CI_AND_QUALITY_GATES.md),
[Dokumentationsprioritaet/Release-Scope](../SPECIFICATION_REVIEW.md),
[Web-Vertrag](../WEB_UI.md), Root-AGENTS und lokale AGENTS von
`fermentation_app`/`device_platform_esp_idf`.
Der Adapter ist Vertragsquelle und unveraenderter Konsument, kein Aenderungsort.

## Umsetzung nach Freigabe – ein kleiner Implementierungs-/Testschnitt

1. In `lib/fermentation_app/src/web_application_routes.cpp` ausschliesslich den
   Headernamen im vorhandenen Fetch auf `X-UI-Mutation-Seq` aendern. Kein
   Server-Alias, Fallback, neuer Headervertrag oder Abstraktionslayer.
2. `scripts/test_web_shell_polling.mjs` wiederverwenden: Das vorhandene
   Fetch-Fake speichert fuer `/internal/ui/run` URL und Optionen; die Fixture
   setzt `window.__WEB_CSRF__` auf einen festen lokalen Testwert. Den echten
   extrahierten `send()`-Pfad mit gueltigem `P` ausfuehren und die tatsaechlichen
   Optionen pruefen: URL `/internal/ui/run`, Methode `POST`,
   `X-UI-Mutation-Seq === '1'` bei `Q=1`, `X-CSRF-Token` gleich dem Fixturewert,
   `Content-Type: application/json`, alter Schluessel `X-Mutation-Seq` fehlt.
   Nach verwertbarer Antwort und frischem Snapshot erneut senden und Wert `'2'`
   pruefen; Sequenzfortschritt nicht nachbauen. Die neue Assertion muss gegen
   den alten Produktheader fehlschlagen und nach dem Fix bestehen. Alle
   vorhandenen Polling-, Abort-, Replay-nahen POST- und Aktionssperrtests behalten;
   kein neues Framework, kein Netzwerk im JS-Test.
3. In `test/test_web_application_routes/test_main.cpp` nur die betroffene
   Header-Assertion auf den kanonischen Namen und Abwesenheit des alten Namens
   anpassen. Im eingefrorenen Generator
   `test/test_web_application_routes/shell_golden.hpp` denselben einzelnen
   Headernamen nachvollziehbar aendern; keine blinde Golden-Neugenerierung.
   Bestehende Budget-/Zustands-/Suffix-/503-Tests unveraendert weiterverwenden.
4. Gezielte Verifikation unten, Builder-Self-Check und kurzer Ergebnisnachweis
   auf dem Implementierungscommit; danach Independent Review. Plan und
   Umsetzung bleiben in diesem Branch/PR. Materielles Zusatzdelta zuerst dem
   Owner vorlegen, keine stillschweigende Scopeerweiterung.

## Verifikation nach Implementierungsfreigabe

Im eingerichteten Repository-Toolchain-Kontext, seriell im selben Worktree:

```bash
node scripts/test_web_shell_polling.mjs
pio test -e native -f test_web_application_routes -f test_web_browser_policy -f test_web_session -f test_http_server_lifecycle
idf.py -C test/esp_idf_http_server_adapter_host -B /tmp/build_issue27_mutation_header_http_host build
/tmp/build_issue27_mutation_header_http_host/issue27_http_server_adapter_host.elf
export PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"
bash scripts/run_pre_ready_gates.sh self-check
git diff --check
```

Die nativen Suiten pruefen den geaenderten Shell-/Run-Konsumenten sowie dessen
bestehende Browser-, Session-/Replay- und HTTP-Lifecycle-Grenzen. Der
[Adapter-Hosttest](../../test/esp_idf_http_server_adapter_host/README.md)
prueft mit echtem IDF-HTTPD ueber Host-Loopback bereits die Extraktion des
kanonischen Headers in `metadata.mutationSeq` (Wert `7`), CSRF und
Chunk-/Fehlervertraege; er beweist allein keine komplette Web-Mutation.
Nur Loopback, kein Board. Bei belegtem Port 8080 keine fremden Prozesse
beenden; den Test getrennt isolieren oder als BLOCKED berichten.
Keine weiteren Authspeicher-/KDF- oder Gesamtsuiten fuer das unveraenderte
Subsystem. Keine zusaetzlichen Firmwareprofile allein fuer das Stringdelta;
der erforderliche Adapter-Hostbuild und der kanonische Self-Check decken die
hier betroffenen Buildpfade ab. Ein spaeterer finaler Pre-Ready folgt erst
nach Independent Review / OPEN_BLOCKERS=0 und separater Owneranweisung dem
CI-Vertrag, nicht als Teil der jetzigen Planung.

**Budget:** Den vollstaendig zusammengesetzten Kopf plus statischen Suffix in
allen vorhandenen Shellzustaenden messen. Unveraendert maximal **8192 B**;
bestehende mindestens **128 B Reserve** bei der normalen einstelligen Sequenz
enthaelt den **19-B-Aufschlag** fuer die breiteste, zwanzigstellige uint64-Sequenz.
Auch mit diesem Aufschlag muss die Shell unter 8192 B bleiben. Der Name waechst
um genau **3 B**. Aus historisch 8056 B angemeldet / 8055 B Passwortschutz aus
folgen rechnerisch 8059 / 8058 B und mit maximaler Sequenz 8078 / 8077 B;
das sind **Erwartungen, keine neuen Messwerte**. Tatsachen aus dem nativen
Budgettest dokumentieren; keine Grenzerhoehung, Minifizierung oder F1-Umbauten
bei Abweichung, sondern HOLD mit konkretem Fehlbetrag.

## Abgabe und Grenzen

Jetzt werden ausschliesslich diese Planfassung und die Roadmap-Nachpflege
versioniert; Diff-/Link-/Scopepruefung, keine Produkt-/Testaenderung und keine
Test-/Build-/Self-Check-/Pre-Ready-Ausfuehrung. Die Roadmap verlinkt diesen Plan;
historische Auditdateien, Diagnosebranches und Browser-Rohdaten bleiben erhalten.
Es gibt keine neue fachliche Entscheidung oder offene Designalternative fuer
den Headerfix; das naechste Gate ist die exakte Planfreigabe.

Weitere Webfelder, Adapter-/Auth-/Command-Aenderungen, F4, Sensorik/Aktoren,
G1'/G2 und Hardware-/Browser-/Touch-/Netzwerktests, Flash/Reset/UART sind ausser
Scope. Der Fix belegt keine WLAN-/TCP-Ursache oder deren Behebung und keine
vollstaendige R1-Inbetriebnahme. G1' bleibt storniert NOT_RUN/NOT_PROVEN,
F4 PROVISIONAL 13312/16640 B mit akzeptiertem begrenztem D3-Restrisiko;
B0–B4 nur funktionaler Teilnachweis, formale G2-/R1-U-/D9-Abnahme separat offen.
Historische FAIL/HOLD-Evidence wird nicht umgewertet. `ACTUATOR_RELEASE=NO`.
