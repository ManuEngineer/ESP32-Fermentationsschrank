# CI und Qualitaets-Gates

## Zweck

Dieses Dokument ist die kanonische Quelle fuer Ausfuehrungszeitpunkt,
Buildprofile, Werkzeugvertraege, CI-Ausloesung und die Ergebnisbegriffe
`PASS`, `FAILED` und `BLOCKED`. Die vollstaendigen ausfuehrbaren Befehle fuer
die gemeinsamen portablen Runner-Gates und die clang-tidy-Dateiliste stehen
ausschliesslich im versionierten Runner
`scripts/run_pre_ready_gates.sh`; GitHub-CI-only Artefakt-/Privacy-Gates stehen
im Workflow.

Der Host-Gate-Pfad verwendet PlatformIO `6.1.19` und PyYAML `6.0.3` als direkte
Python-Abhaengigkeiten; GitHub-CI provisioniert beide Pins mit Python 3.13.
PyYAML wird fuer den Board-Profile-SSOT-Check benoetigt. Der reine
Firmwarebuild liest nur den eingecheckten generierten Header und benoetigt
PyYAML nicht. Die ESP32-Produktionsprofile verwenden ESP-IDF `v6.1` am Commit
`fff9895c82d744c7237be8847347bdd1b07c6643`.

Der gemeinsame versionierte Gate-Owner ist
`scripts/run_pre_ready_gates.sh`. Er verifiziert vor den Gates PlatformIO
`6.1.19`, clang-format und clang-tidy aus der Major-Linie 21 sowie fuer die
ESP-Phase die bestehende exakte ESP-IDF-/esp-clang-Provenienz. Installation und
Provisionierung der Werkzeuge bleiben Umgebungsaufgabe; eine Abweichung wird
vom Runner als `BLOCKED` oder `FAILED` behandelt und kann keinen lokalen
Pre-Ready-PASS erzeugen.

Der lokale Lauf wird ueber den kleinen Wrapper
`scripts/run_pre_ready_and_publish.sh` ausgefuehrt, der den Runner unveraendert
aufruft und das Ergebnis als Commit Status publiziert (Abschnitt
„Pre-Ready-Status und Merge-Gate“). Der Wrapper enthaelt keine Testliste.

Der Runner enthaelt ausschliesslich portable Engineering-Gates, die lokal und
in GitHub-CI denselben Checkout unabhaengig vom GitHub-Artefakttransport pruefen.
Der portable Repository-Secret-Check ohne `--scan-path` gehoert dazu.
GitHub-, Upload-, generierte Artefakt- und zugehoerige Privacy-Gates bleiben
ausschliesslich im Workflow. Der lokale Pre-Ready-Lauf emuliert weder GitHub
noch den Artefakt-Upload.

## Ausfuehrungszeitpunkt

### Planung

Keine Builds und keine vollstaendigen Testlaeufe. Zulaessig sind nur
Repository-, Diff-, Link- und Dokumentationspruefungen, die den Plan
unterstuetzen.

### Draft-Umsetzung

Nur gezielte lokale Tests und Pruefungen fuer den tatsaechlich geaenderten
Bereich. Bei geaenderten gemeinsamen Vertraegen gehoeren die direkt betroffenen
Konsumententests zum gezielten Umfang. Nicht betroffene Profile und der
vollstaendige Gesamtlauf werden nicht ritualistisch wiederholt. Nach einer
tatsaechlichen Implementation und vor der Uebergabe an den abschliessenden
Independent Full Review fuehrt der Builder den Builder-Static-Analysis-Self-Check
des bestehenden Runners aus. Im Plan-only-Stand ist dieser
Implementation-Self-Check noch nicht erforderlich. Der Runner entscheidet fuer
den konkreten PR selbst, ob clang-format und/oder clang-tidy jeweils
`REQUIRED` oder `NOT_REQUIRED` sind;
diese Entscheidung wird nicht manuell durch den Builder vorselektiert.

### Builder-Static-Analysis-Self-Check

Vor der Uebergabe an den abschliessenden Independent Full Review fuehrt der
Builder auf dem Implementierungs-`HEAD` den gezielten Self-Check aus:

```bash
export PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"
bash scripts/run_pre_ready_gates.sh self-check
```

Der Runner verwendet ausschliesslich die vorhandene lokale Referenz
`refs/remotes/origin/main` und leitet daraus mit `git merge-base HEAD
refs/remotes/origin/main` die Basis ab. Die Referenz muss vorhanden und als
Commit verifizierbar sein; ein frei gesetzter `STATIC_ANALYSIS_BASE_SHA` wird
fail-closed abgelehnt. Dadurch kann ein spaeterer PR-Commit die frueheren
PR-Aenderungen nicht aus dem Self-Check-Scope entfernen.

clang-format-21 prueft frueh nur die geaenderten C/C++-Dateien. Sobald eine
relevante C/C++-Aenderung im hardwareunabhaengigen nativen Produktionskern oder
seinen Headern erkannt wird, erzeugt der Runner die native
Kompilierungsdatenbank und fuehrt die vollstaendige bestehende kanonische
clang-tidy-Dateiliste aus. Include-Abhaengigkeiten und
Translation-Unit-Zuordnungen werden nicht dupliziert. Der Self-Check fuehrt
keine vollstaendige Native-Suite, ESP-IDF-Profile oder esp-clang-Gesamtausfuehrung
aus und ist kein `PRE_READY_LOCAL_GATES=PASS`.

Der eigene Ergebnisstatus lautet `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS`,
`FAILED` oder `BLOCKED`. Ein nicht ausgefuehrter vollstaendiger Pre-Ready-Teil
bleibt `NOT_RUN`.

### Vollstaendiger lokaler Lauf

Nur wenn:

- der vollstaendige Independent Full Review abgeschlossen ist
  (`FULL_REVIEW_COMPLETE`);
- `OPEN_BLOCKERS=0` gilt; klassifizierte `FOLLOW-UP` und `NO-ACTION` blockieren
  den Lauf nicht;
- der zu pruefende `HEAD` final ist;
- der Owner den Lauf ausdruecklich anordnet.

Der **normale Owner-Pre-Ready-/Merge-Gate-Pfad ist ausschliesslich der
Wrapper**, auf demselben finalen, gepushten `HEAD`:

```bash
bash scripts/run_pre_ready_and_publish.sh
```

Er ruft `host` und danach `esp` des Runners selbst auf und publiziert danach
`pre-ready/local` (Abschnitt „Pre-Ready-Status und Merge-Gate“). Ein normaler
Full-PR fuehrt host+esp damit **einmal** aus – nicht zuerst manuell und danach
nochmals im Wrapper.

Die lokale Werkzeugprovisionierung ist Umgebungsvoraussetzung **vor** dem
Wrapper; der Wrapper installiert nichts (`D1`):

- PlatformIO `6.1.19` sowie clang-format und clang-tidy der Major-Linie 21 sind
  fuer die `host`-Phase in der aufrufenden Umgebung verfuegbar (wie in
  GitHub-CI ueber das `bin`-Verzeichnis des gepinnten `esp-clang`);
- `IDF_PATH` zeigt auf den ESP-IDF-6.1-Checkout, `IDF_TOOLS_PATH` auf ein
  vorhandenes Tools-Verzeichnis (Default-Installation `$HOME/.espressif`), und
  `esp-clang` ist installiert, z. B. einmalig
  `python3 "$IDF_PATH/tools/idf_tools.py" install esp-clang`;
- der Wrapper aktiviert `"$IDF_PATH/export.sh"` nur fuer seine `esp`-Phase in
  einer Subshell; `export.sh` wird vorher nicht fuer den Wrapper aktiviert.

Fehlen `IDF_PATH`/`IDF_TOOLS_PATH`, endet der Wrapper mit `BLOCKED`. Der Runner
prueft Werkzeug- und ESP-IDF-Provenienz weiterhin selbst; die detaillierte
esp-clang-Pfad-, Versions-, `tools.json`- und `pyclang`-Pruefung bleibt beim
bestehenden Static-Analysis-Owner. Der Runner ist die einzige Quelle fuer die
gemeinsamen portablen Gatebefehle und die clang-tidy-Dateiliste; sie werden hier
nicht wiederholt.

Die direkten Aufrufe `bash scripts/run_pre_ready_gates.sh host` und
`bash scripts/run_pre_ready_gates.sh esp` (jeweils mit
`PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"`) sind Low-Level-/Diagnose-
bzw. Runner-Referenz, wie sie auch GitHub-CI im Workflow verwendet. Sie sind
**kein vollstaendiges Merge-Gate**, weil sie keinen Commit-Status publizieren.

`host` umfasst den vollständigen clang-format-21-Check, nativen Build und
Ressourcenbericht, komplette native Tests, Compile-Datenbank und den exakten
clang-tidy-21-Lauf sowie Architekturguard und Quality-Gate-Selbsttests. `esp`
umfasst Bring-up-/Release-Build, Ressourcenbericht und esp-clang-Static-
Analysis. Nur wenn beide Phasen mit dem gleichen `PRE_READY_EXPECTED_HEAD`
erfolgreich sind, darf
`PRE_READY_LOCAL_GATES=PASS` dokumentiert werden. Ein nicht ausgeführter
Teil bleibt `NOT_RUN`.

### Pre-Ready-Status und Merge-Gate

`scripts/run_pre_ready_and_publish.sh` publiziert genau einen Commit-Status-
Context `pre-ready/local` auf den getesteten PR-HEAD (`pending`, dann `success`
oder `failure`; GitHub kennt kein `blocked`, die Beschreibung nennt
`FAILED`/`BLOCKED`). Es gibt keine separaten `host`-/`esp`-Contexts.

Semantik: `success` bedeutet „lokales Pre-Ready-/Merge-Gate erfuellt“, nicht
zwingend „host+esp ausgefuehrt“. Die Klasse wird automatisch aus dem Diff gegen
das aktuelle `origin/main` ermittelt (kein Flag):

- Diff mit mindestens einer Nicht-Markdown-Datei (auch Mischdiff, Umbenennung,
  Loeschung oder leerer Diff): voller Lauf `host` und `esp`; `success`
  bedeutet `host+esp PASS`, lokal `PRE_READY_LOCAL_GATES=PASS`;
- rein Markdown-only Diff (jeder Pfad `*.md`): der Runner wird nicht
  ausgefuehrt; Beschreibung `MARKDOWN_ONLY_NOT_REQUIRED`, lokal
  `PRE_READY_LOCAL_GATES=NOT_REQUIRED_MARKDOWN_ONLY` – nie `PASS`.

Der Wrapper publiziert nur, wenn Arbeitsbaum sauber, normaler Branch mit
Upstream `origin/<branch>`, `HEAD == origin/<branch>`, `origin/main` Vorfahre
von `HEAD` und das `gh`-Repository gleich `origin` ist, und prueft dies vor
`success` erneut (inklusive unveraendertem `origin/main` und unveraenderter
Klassifikation). Wird `main` waehrend des Laufs weitergeschrieben, entsteht
kein `success`. Ein Status gilt nur fuer genau seinen SHA; jeder neue Push
benoetigt einen neuen Lauf. Der Status ist keine kryptografische Attestierung,
sondern Ablaufdisziplin im Owner-gefuehrten Einzelrepo.

Branch-Protection-Sollzustand fuer `main`: Required Status Check
`pre-ready/local` und „Require branches to be up to date before merging“
(strict); die schwere GitHub-CI ist **kein** Required Check. Der Sollzustand
gilt als aktiv erst nach der Owner-Umstellung und dem Nachweis
`REQUIRED_STATUS_HEAD_MERGE_COMMIT_INTEROP=PASS` (Stand siehe
`docs/ROADMAP.md`), da bei Heavy-CI-PRs nicht vorausgesetzt werden darf, dass
der Head-Status gegenueber Checks auf dem Test-Merge-Commit massgebend ist.

### GitHub-CI

Die schwere Clean-Room-/Artefakt-CI ist nicht mehr das normale zweite
Engineering-Gate. `.github/workflows/build.yml` reagiert auf:

- `workflow_dispatch` (manueller Full-CI-Lauf);
- `pull_request.opened`;
- `pull_request.ready_for_review`;
- `pull_request.synchronize`;
- `pull_request.reopened`

jeweils bei `pull_request` nur, wenn der Diff eine Datei der positiven Pfadliste
im Workflow trifft (Gate-/CI-/Toolchain-/Buildsystem-Vertrag und direkte
Produktions-Buildinputs). Die Liste steht ausschliesslich im Workflow; der
Selbsttest `scripts/selftest_quality_gates.py` leitet die Pflichtpfade aus dem
Runner, den Profilen und `sdkconfig.defaults` ab und prueft sie. Normale
Feature-PRs loesen sie nicht aus. Wurde sie fuer einen PR ausgeloest oder ordnet
der Owner sie an, ist ihr PASS Pflicht. Ein im Draft erzeugter, uebersprungener
Workfloweintrag ist weder `GITHUB_CI=PASS` noch ein Grund fuer einen zweiten Lauf
vor `Ready for review`.

Der Firmwarejob laeuft nur, wenn der Pull Request kein Draft ist. Draft-Pushes
koennen einen sofort uebersprungenen Workfloweintrag erzeugen, fuehren aber
keine Builds oder Tests aus.

Trifft der PR die Pfadliste, startet der Ownerwechsel auf `Ready for review` die
vollstaendige CI fuer den reviewten Head; jeder spaetere Push auf einen
Nicht-Draft-PR, der die Pfadliste trifft, startet sie erneut und verwirft den
vorherigen Firmware-Pruefnachweis. Der `pre-ready/local`-Status eines frueheren
SHA ersetzt dies nicht und gibt einen spaeteren Push nie frei.

Markdown-only- und Kommentaraenderungen treffen die positive Pfadliste nicht und
loesen die Firmware-CI daher nicht aus. Fuer den Reviewnachweis gilt bei semantischen
Aenderungen die Materialitaetsregel: Eine lokal begrenzte Korrektur wird durch
Fix Verification und den erforderlichen Regression Check verifiziert; eine
materielle Aenderung oder ein breiter neuer Diff erfordert einen neuen Full
Review. Rein redaktionelle Korrekturen ohne Bedeutungs-, Scope-, Vertrags- oder
Akzeptanzwirkung duerfen den bisherigen Reviewnachweis behalten.

Nach einem CI-Fehlschlag dokumentiert der Agent Fehler, Auswirkung und gezielte
Korrektur. Nur der Owner entscheidet ueber eine Rueckstufung auf Draft. Nach
einer CI-Korrektur gilt dieselbe Fix-Verification-/Materialitaetsregel: Der
Korrekturdiff, zuvor offene Blocker sowie direkt betroffene Vertraege und
Regressionen werden geprueft; nur bei materieller Aenderung oder breitem neuem
Diff ist ein neuer Full Review erforderlich. Den neuen Wechsel auf `Ready for
review` fuehrt nur der Owner aus.

Es gibt keinen `push`-Trigger und damit keinen automatischen identischen
Wiederholungslauf nach dem Merge.

### ESP-IDF-Checkout- und Tools-Caching

Der ESP-IDF-Checkout (`$RUNNER_TEMP/esp-idf-<ESP_IDF_TAG>`) und
`IDF_TOOLS_PATH` (`$RUNNER_TEMP/espressif`, enthaelt die von `install.sh
esp32` installierte Toolchain sowie `esp-clang`) werden ueber
`actions/cache` wiederverwendet. Beide Cache-Schluessel sind exakt an den
gepinnten `ESP_IDF_COMMIT` gebunden, der Tools-Schluessel zusaetzlich an die
konkrete `setup-python`-Version sowie `runner.os`/`runner.arch`; es werden
keine `restore-keys` verwendet, sodass ausschliesslich ein exakter
Schluesseltreffer als Cache-Hit zaehlt.

Bei einem gueltigen Checkout-Cache-Treffer entfaellt Clone/Fetch/Checkout/
Submodule vollstaendig; bei einem gueltigen Tools-Cache-Treffer entfallen
`install.sh esp32` und die erneute `esp-clang`-Installation vollstaendig.
Die bestehende Tag-/Commit-/Sauberkeitspruefung des ESP-IDF-Checkouts sowie
alle nachgelagerten Provenienz- und Versionspruefungen
(`verify_expected_esp_environment` in `run_pre_ready_gates.sh`, die
esp-clang-Pfad-/Versions-/`tools.json`-/`pyclang`-Pruefung in
`run_esp_idf_static_analysis.py`) laufen davon unabhaengig bei jedem Lauf
unveraendert; ein Cache-Treffer ersetzt diese Pruefungen nicht, sondern
liefert nur den Baum, gegen den sie laufen. Ein Cache-Wechsel des gepinnten
`ESP_IDF_COMMIT` (z. B. bei einem kuenftigen ESP-IDF-Upgrade) erzeugt
automatisch neue Cache-Schluessel und damit einen sauberen Vollinstall-Pfad.

Die reale Reichweite dieses Caching ist durch den fehlenden `push`-Trigger
begrenzt: Ein Merge nach `main` fuehrt diesen Workflow nicht aus und erzeugt
damit keinen Cache-Eintrag auf `main`. GitHub-Actions-Caches sind zudem
nicht global, sondern nur fuer den erzeugenden Branch sowie fuer Pull
Requests mit Zugriff auf dessen Basis-/Default-Branch-Cache sichtbar. Ohne
main-seitigen Lauf existiert kein Default-Branch-Cache, von dem neue
PR-Branches profitieren koennten; der Nutzen ist auf Folgelaeufe innerhalb
desselben PR-Branches (Reruns, spaetere `synchronize`-Pushes bei
unveraendertem `ESP_IDF_COMMIT`) beschraenkt. Eine zusaetzliche
Cache-Warming- oder Trigger-Infrastruktur, um dies zu aendern, ist nicht
Teil dieses Caching-Mechanismus.

## Buildprofile

| Profil | Werkzeug | Zweck |
|---|---|---|
| `native` | PlatformIO/Host-Compiler | Fachlogik, Simulation und native Tests |
| `esp32_bringup` | ESP-IDF 6.1 | Produktionsbuild mit gesperrten Aktoren und unbestaetigter Hardware |
| `esp32_release` | ESP-IDF 6.1 | Releaseprofil; keine automatische Hardwarefreigabe |

`src/main.cpp` ist der native Composition Root. `main/app_main.cpp` ist der
ESP-IDF Composition Root.

## Gezielte lokale Pruefungen

Der konkrete Umfang wird aus Diff, Plan und betroffenen Tests abgeleitet.
Beispiele:

```bash
pio test -e native --filter <test-verzeichnis-oder-muster>
clang-format --dry-run --Werror <geaenderte-cpp-hpp-h-dateien>
python3 scripts/check_architecture_boundaries.py
git diff --check
```

Die generierte Artefakt-Scanabdeckung und der `check_secrets.py`-Aufruf mit
`--scan-path` sind keine lokalen Pre-Ready-Gates. Diese Aufrufe stehen
ausschliesslich im GitHub-Workflow; der Repository-Secret-Check ohne
`--scan-path` bleibt Bestandteil der gemeinsamen `host`-Phase.

Ein gezielter Lauf muss im PR mit Befehl, Umfang und Ergebnis dokumentiert
werden.

## Vollstaendiger nativer Lauf

Der vollständige native Lauf ist die `host`-Phase des gemeinsamen Runners. Er
wird nicht durch eine zweite manuelle Befehlsliste dokumentiert:

```bash
bash scripts/run_pre_ready_gates.sh host
```

## ESP-IDF-Profile

Nach erfolgreicher `host`-Phase und Aktivierung der festgelegten Toolchain:

```bash
bash scripts/run_pre_ready_gates.sh esp
```

Der Runner verwendet die bestehenden Owner für Profilbuild,
Ressourcenbericht und esp-clang-Analyse. Die lokale Ausführung muss die
ESP-IDF-/esp-clang-Umgebung vor dem Aufruf bereitstellen; bei fehlender oder
abweichender Provenienz bleibt der ESP-Teil `NOT_RUN` beziehungsweise
`BLOCKED`.

Der Upgrade-, Herkunfts- und Hardware-Smoke-Vertrag steht in
`ESP_IDF_UPGRADE_CONTRACT.md`.

## Formatierung und Static Analysis

| Werkzeug | Version | Umfang |
|---|---:|---|
| clang-format | 21 | C/C++ unter `src/`, `include/`, `lib/`, `test/`, `main/` |
| clang-tidy | 21 | hardwareunabhaengiger Produktionskern ueber die native Kompilierungsdatenbank |
| esp-clang | zur ESP-IDF-6.1-Toolchain passend (`esp-21.1.3_20260408`) | beide ESP-IDF-Profile |

Die vollständige Formatprüfung, die native Kompilierungsdatenbank und die
kanonische clang-tidy-Dateiliste stehen ausschließlich im versionierten
Runner `scripts/run_pre_ready_gates.sh`. CI und lokaler Pre-Ready-Lauf rufen
denselben Runner auf. Eine punktuelle
Unterdrueckung verwendet ausschliesslich:

```cpp
// NOLINT(check-name): konkrete Begruendung
```

## Architektur- und Gate-Selbsttests

Architekturguard und Quality-Gate-Selbsttests sind Bestandteile der
gemeinsamen `host`-Phase. Ihre Gatebefehle werden nicht nochmals hier oder im
Workflow dupliziert; fuer den lokalen Pre-Ready-Lauf genügt der Runner-Aufruf.

Der Architekturguard erzwingt die in ADR-013 festgelegte Richtung und ersetzt
kein vollstaendiges Architekturreview.

Der Gate-Selbsttest beweist anhand temporaerer fehlerhafter Fixtures, dass die
Qualitaetspruefungen echte Verstoesse erkennen.

### GitHub-CI-only: generierte Artefakte und Privacy

Der gemeinsame `host`-Runner kontrolliert mit `check_secrets.py` ohne
`--scan-path` portable, getrackte Repositorydateien und lokale
Konfigurations-/Secret-Verstoesse. Im GitHub-Workflow kontrolliert derselbe
Owner nach den ESP-Builds zusaetzlich die hochzuladenden Textartefakte mit
`check_secrets.py --scan-path`.
`check_ci_artifact_scan_coverage.py` prueft dort, dass jeder erfolgreiche
Textartefakt-Upload durch die im Workflow angegebene Scanmenge abgedeckt ist.
Diese Gates und ihre Artefaktpfade werden nicht vom gemeinsamen Runner
ausgefuehrt; die Artefaktpfade sind kein lokaler Pfad-Whitelist-Vertrag.

## Determinismus und Zeit

Native Tests verwenden keine reale Uhrzeit und keine Netzwerkabhaengigkeit.
`ITimeSource` trennt monotone Laufzeit von optionaler UTC-Zeit.
`VirtualTimeSource` schreitet nur durch explizite Testaktionen voran; ein
Neustart wird durch eine neue Instanz simuliert.

Der ESP-IDF-Adapter `EspTimerTimeSource` liefert monotone Laufzeit ueber
`esp_timer`. Eine fachliche Nutzung wird nur behauptet, wenn ein realer
Produktionskonsument existiert.

## Ressourcenbericht und Artefakte

Der native und der ESP-IDF-Ressourcenbericht werden durch die `host`- und
`esp`-Phase des gemeinsamen Runners erzeugt. Die einzelnen Owner-Skripte und
ihre Datenformate bleiben unverändert.

Reale Byte-, Heap-, Partitions- und Puffergrenzen bleiben
`TBD_IMPLEMENTATION_BUDGET`, bis reale Builds und Belastungsmessungen
vorliegen.

Bei erfolgreicher GitHub-CI werden getrennt gesichert:

- finaler Ressourcenbericht;
- Bring-up-Binaer-, ELF-, Map-, Bootloader-, Partition-, Konfigurations-,
  Compile-Database-, Groessen-, Log- und Manifestartefakte;
- entsprechende Releaseartefakte.

Fehlgeschlagene Builds sichern den verfuegbaren Buildlog.

## CI-Pipeline

Der Firmwarejob (schwerer Clean-Room-Pfad; Trigger siehe „GitHub-CI“) fuehrt in
dieser Reihenfolge aus:

1. Checkout und Python;
2. PlatformIO installieren;
3. ESP-IDF `v6.1` am exakten Commit installieren und verifizieren; Checkout
   und `IDF_TOOLS_PATH` werden dabei ueber `actions/cache` wiederverwendet
   (siehe „ESP-IDF-Checkout- und Tools-Caching" oben), die Provenienz-
   verifikation laeuft unveraendert bei jedem Lauf;
4. esp-clang installieren (uebersprungen bei gueltigem Tools-Cache-Treffer),
   dessen `bin`-Verzeichnis fuer Host und ESP voranstellen und die Version
   verifizieren;
5. den gemeinsamen Runner in der `host`-Phase ausfuehren; dieser bricht bei
   Format, Build, Tests oder clang-tidy fail-fast ab;
6. die ESP-IDF-Umgebung aktivieren und den gemeinsamen Runner in der
   `esp`-Phase ausfuehren;
7. die GitHub-CI-only Artefakt-Scanabdeckung und Artefakt-/Privacy-Pruefung
   ausfuehren;
8. Berichte und Buildartefakte sichern.

Damit verwenden Host- und ESP-Phase denselben gepinnten `esp-clang`-Werkzeug-
satz; der lokale Ownerlauf fuehrt beide portablen Runner-Phasen auf demselben
finalen HEAD vollstaendig aus. Die GitHub-CI-only Gates laufen nur im
Workflow.

`concurrency` bricht einen veralteten Lauf desselben Pull Requests ab, sobald
ein neuerer Lauf startet.

## Ergebnisstatus

- **PASS:** Pruefung wurde ausgefuehrt und war erfolgreich.
- **FAILED:** Pruefung wurde ausgefuehrt, ist fehlgeschlagen und blockiert den
  Abschluss.
- **BLOCKED:** Pruefung konnte wegen einer konkret benannten fehlenden
  Voraussetzung nicht ausgefuehrt werden.

Die Statusfelder werden getrennt gefuehrt:

- `INDEPENDENT_REVIEW=PASS` und `OPEN_BLOCKERS=0` bezeichnen den abgeschlossenen
  fachlichen Review;
- `PRE_READY_LOCAL_GATES=PASS` bezeichnet den autorisierten vollständigen
  lokalen Lauf (host+esp) auf dem finalen HEAD;
  `NOT_REQUIRED_MARKDOWN_ONLY` bezeichnet die automatisch klassifizierte rein
  Markdown-only Klasse und ist nie `PASS`;
- `pre-ready/local=success` auf exakt dem finalen HEAD ist der publizierte
  Nachweis beider Klassen und der Required Status Check;
- `GITHUB_CI=PASS` bezeichnet den Ergebnisstatus der schweren CI; er ist nur
  Pflicht, wenn der Workflow fuer den PR ausgeloest wurde oder der Owner ihn
  anordnet.

`Ready for review` darf erst nach erfuelltem lokalem Pre-Ready-Gate und
`pre-ready/local=success` auf dem finalen HEAD durch den Owner gesetzt werden. `INDEPENDENT_REVIEW=PASS` oder `OPEN_BLOCKERS=0` allein sind
dafür nicht ausreichend.

`SKIPPED`, `NOT_RUN` oder eine fehlende Angabe duerfen nicht als `PASS`
bezeichnet werden.

## Ausnahmen

Jede Ausnahme benoetigt eine konkrete Begruendung:

- projektweit in Werkzeugkonfiguration oder diesem Dokument;
- punktuell direkt an der Unterdrueckung;
- auftragsbezogen im freigegebenen Plan und PR-Nachweis.

Unbegruendete Unterdrueckungen von Safety-, Security-, Architektur- oder
Kernfunktionstests sind unzulaessig.
