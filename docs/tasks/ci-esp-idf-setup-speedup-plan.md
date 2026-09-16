# Plan: GitHub-CI ESP-IDF-Setup beschleunigen

## Context

`main` ist seit PR #160 auf ESP-IDF `v6.1 @ fff9895c82d744c7237be8847347bdd1b07c6643`
kanonisch. Der Owner beauftragt, die pro GitHub-CI-Lauf wiederholte
Toolchain-Einrichtung deutlich zu beschleunigen, ohne bestehende Gates,
Pinning-Garantien oder Nachweisqualität zu schwächen. Kein passendes
GitHub-Issue existiert; der Owner hat ausdrücklich entschieden, ohne
Issue-Referenz fortzufahren (Branch `agent/ci-esp-idf-setup-speedup-plan`,
Draft-PR #161).

Dies ist die zweite, vollstaendige und eigenstaendig ausfuehrbare Revision
dieses Plans. Sie korrigiert einen Full-Review-Befund zur ersten Revision
(Plan-SHA `66ab4f3678a5265ca176998dfad1d428d1806982`, vom Owner freigegeben
und bereits umgesetzt bis Implementierungs-HEAD `033a6aa`): Das dort geplante
`cache: pip` fuer `actions/setup-python` ist wirkungslos, da das Repository
weder `requirements.txt` noch `pyproject.toml` besitzt und `setup-python@v6`
seinen Standard-Pip-Cache nur darüber initialisiert. Diese Revision entfernt
den Punkt ersatzlos aus dem Scope, korrigiert eine unzutreffende Aussage zur
Pre-Ready-/Ready-Reihenfolge und ergaenzt `docs/ROADMAP.md`- sowie
PR-Statuspflege als notwendige Dokumentationswirkung. Die genehmigte
Caching-statt-Docker-Entscheidung sowie alle uebrigen Commit-2/3-Inhalte der
ersten Revision bleiben unveraendert gueltig und sind bereits umgesetzt.

Verifizierter Live-Ausgangsbefund (heutiger Baseline-CI-Lauf, PR #160,
`gh run view 35014280372` auf `main`-HEAD `dc017f4`, Ergebnis `success`,
Gesamtdauer 10:26 min):

| Step | Dauer | Anteil |
|---|---:|---|
| Format-/Analysewerkzeuge installieren (clang 18) | 12 s | apt-get update+install, obwohl bereits vorhanden |
| Gemeinsame Gateausfuehrung (Host) | 2:24 min | echte Prüfung, nicht Setup |
| **ESP-IDF installieren (v6.1, exakter Commit)** | **3:35 min** | Repo-Clone + Submodule + `install.sh esp32` |
| Gemeinsame Gateausfuehrung (ESP-IDF) | 3:51 min | enthält `esp-clang`-Install + echte Builds/Analyse |

Der groesste wiederholt eingerichtete Block ist der ESP-IDF-Clone/Submodule/
Toolchain-Schritt (3:35 min von 10:26 min Gesamtlaufzeit).

## Reuse-before-Build-Bewertung

1. **Offizielles Espressif-Docker-Image** (`espressif/idf`): geprüft und
   verifiziert reproduzierbar nutzbar — der unveraenderliche Docker-Hub-Tag
   `v6.1` wird aus dem Git-Tag `v6.1` gebaut, und `git ls-remote
   "https://github.com/espressif/esp-idf.git" "v6.1^{}"` dereferenziert exakt
   auf `fff9895c82d744c7237be8847347bdd1b07c6643` — deckungsgleich mit dem
   Pinning-Vertrag. Das Image enthaelt aber weder `esp-clang` noch
   PlatformIO, clang-format/clang-tidy muessten weiterhin separat bereitgestellt
   werden, und der bestehende Single-Job-Workflow (Host-Phase auf dem nackten
   Runner, ESP-Phase im selben Job) muesste fuer die ESP-Phase auf
   Container-Steps (`docker run` mit Volume-Mount des Checkouts,
   UID/Artefaktpfad-Handling) umgebaut werden. Der strukturelle Mehraufwand
   steht nicht im Verhaeltnis zum Zusatznutzen gegenueber Option 3.
2. **Offizielles Image mit duenner Ergaenzung**: gleiche strukturelle
   Mehrarbeit wie 1, zusaetzlich eigene Ergaenzungsebene (esp-clang, ggf.
   PlatformIO) zu pflegen. Verworfen aus demselben Grund.
3. **GitHub-Actions-Caching**: verwendet ausschliesslich die vom Runner
   bereits mitgebrachten Werkzeuge und offizielle `actions/cache`-Infrastruktur,
   aendert die Jobstruktur nicht, und cached genau die beiden groessten
   wiederholten Kostenbloecke (ESP-IDF-Checkout, `IDF_TOOLS_PATH` inkl.
   `esp-clang`) exakt schluesselgebunden an den gepinnten Commit. Einfacher
   und fuer den bestehenden Single-Job-Aufbau insgesamt besser geeignet als 1/2.
4. **Eigenes CI-Image**: nicht erforderlich; Option 3 loest das Kernproblem
   ohne eigene Image-/Registry-Pflege.

**Gewaehlt: Option 3 (GitHub-Actions-Caching) plus eine unabhaengige
Sofortkorrektur** (clang-format-18/clang-tidy-18 sind auf dem
`ubuntu-24.04`-Runner-Image bereits vorinstalliert — verifiziert direkt gegen
das offizielle Manifest `actions/runner-images:images/ubuntu/Ubuntu2404-Readme.md`
(`Clang-format: 16.0.6, 17.0.6, 18.1.3`; `Clang-tidy: 16.0.6, 17.0.6, 18.1.3`),
nicht nur gegen eine Suchergebniszusammenfassung; der `apt-get`-Schritt ist
redundant und wird durch reines Verlinken ersetzt). `verify_clang_major` in
`scripts/run_pre_ready_gates.sh` verlangt weiterhin explizit Major-Linie 18;
diese Anforderung wird durch die Umstellung nicht veraendert, nur die Quelle
der Binaries.

Diese Wahl wird im Draft-PR dokumentiert und kann vom Owner bei der
Plan-Freigabe explizit verworfen werden, falls ein Container-Image dennoch
gewuenscht ist.

## Zu erhaltende Vertraege (unveraendert)

- `EXPECTED_TAG`/`EXPECTED_COMMIT`-Verifikation (Tag-Match, Commit-Match,
  sauberer Arbeitsbaum) laeuft in jedem Lauf unveraendert, unabhaengig von
  Cache-Hit oder -Miss (fail-closed bei Cache-Korruption oder abweichendem
  Commit).
- Beide ESP-IDF-Profile, `esp-clang`-Analyse, Host-Gates, Artefakterzeugung,
  `SOURCE_GIT_SHA`/Build-Commit-Semantik, Secret-/Datenschutzpruefung bleiben
  strukturell unveraendert (`scripts/run_pre_ready_gates.sh` wird nicht
  angefasst).
- Keine Firmware-/Fachlogik-/Safety-/Hardwareverhaltensaenderung.
- Cache-Schluessel sind exakt an `EXPECTED_COMMIT` gebunden, ohne
  `restore-keys`-Fallback: Ein Wechsel des gepinnten Commits (z. B. ein
  kuenftiges ESP-IDF-Upgrade) erzeugt automatisch einen neuen Cache-Schluessel
  und damit einen sauberen Vollinstall-Pfad — keine manuelle Cache-Pflege,
  keine Vermischung unterschiedlicher Toolchain-Versionen.

## Umsetzungsschnitte

**Commit 1 — `.github/workflows/build.yml`: redundante Clang-Installation entfernen**
- `apt-get update && apt-get install -y clang-format-18 clang-tidy-18`
  ersetzen durch direktes Verlinken der bereits auf `ubuntu-24.04`
  vorhandenen Binaries (`/usr/bin/clang-format-18`, `/usr/bin/clang-tidy-18`)
  plus Versionsausgabe zur Absicherung; bei fehlendem Pfad schlaegt der
  Schritt sichtbar fehl (fail-closed, kein stiller Fallback auf `apt-get`).
- **Kein** `cache: pip` fuer `actions/setup-python`: Das Repository besitzt
  weder `requirements.txt` noch `pyproject.toml`, wodurch `setup-python@v6`
  seinen vorgesehenen Standard-Pip-Cache nicht initialisieren kann (die
  Aktion cached darueber nur bei erkanntem Dependency-Manifest). Es wird
  weder eine Dummy-Dependency-Datei noch ein eigener zusaetzlicher
  PlatformIO-Pip-Cache eingefuehrt; dieser Optimierungsschritt entfaellt
  ersatzlos aus dem Scope. `id: setup-python` bleibt bestehen, da
  `steps.setup-python.outputs.python-version` weiterhin Bestandteil des
  ESP-IDF-Tools-Cache-Schluessels in Commit 2 ist.

**Commit 2 — `.github/workflows/build.yml`: ESP-IDF-Checkout und `IDF_TOOLS_PATH` cachen und bei verifiziertem Hit tatsaechlich wiederverwenden**
- `ESP_IDF_TAG`/`ESP_IDF_COMMIT` als Job-`env` einmal definieren (bisher
  inline dupliziert) und vom Cache-Schluessel sowie vom bestehenden
  Verifikationscode referenzieren lassen (DRY, keine Vertragsaenderung).
- Zwei `actions/cache@<gepinnte-SHA>`-Schritte mit eigener `id:` vor dem
  bestehenden "ESP-IDF installieren"-Schritt einfuegen, jeweils **ohne**
  `restore-keys` (nur exakter Schluesseltreffer zaehlt als Hit):
  - `id: cache-esp-idf-checkout`, `path: ${{ runner.temp }}/esp-idf-${{ env.ESP_IDF_TAG }}`
    (Pfad aus `ESP_IDF_TAG` abgeleitet statt hartkodiert),
    `key: esp-idf-checkout-${{ env.ESP_IDF_COMMIT }}`
  - `id: cache-esp-idf-tools`, `path: ${{ runner.temp }}/espressif`,
    `key: esp-idf-tools-${{ env.ESP_IDF_COMMIT }}-py${{
    steps.setup-python.outputs.python-version }}-${{ runner.os }}-${{
    runner.arch }}`. Der `actions/setup-python`-Schritt erhaelt dafuer die
    `id: setup-python`. Grund: `install.sh esp32` legt unter
    `IDF_TOOLS_PATH/python_env` ein Virtualenv an, das fest auf den
    konkreten `setup-python`-Interpreterpfad
    (`/opt/hostedtoolcache/Python/3.13.x/...`) verweist; bumpt der
    Runner-Image die 3.13-Patchversion, waere ein rein commit-schluesseliger
    Cache-Hit ein valider Treffer auf ein totes Interpreterziel. Die
    Python-Version (und OS/Arch) im Schluessel macht einen solchen Bump zu
    einem regulaeren Cache-Miss statt zu einem stillen Laufzeitfehler.
- "ESP-IDF installieren"-Schritt so umbauen, dass die drei bisherigen Phasen
  (Checkout, Provenienzverifikation, Toolinstallation) unabhaengig
  voneinander bedingt ausgefuehrt werden:
  1. **Checkout**: Nur wenn `steps.cache-esp-idf-checkout.outputs.cache-hit
     != 'true'` **oder** `$IDF_PATH/.git` nach dem Restore tatsaechlich
     fehlt, laeuft der bestehende
     Clone/Fetch/Checkout/Submodule-Block unveraendert. Bei echtem Hit
     entfaellt dieser Block vollstaendig (kein Netzwerkzugriff auf das
     ESP-IDF-Repository).
  2. **Provenienzverifikation (immer, unbedingt)**: `ACTUAL_TAG`-, `ACTUAL_COMMIT`-
     und Sauberkeits-Pruefung laufen in jedem Fall gegen das tatsaechlich
     vorliegende `$IDF_PATH` — unveraendert gegenueber heute, unabhaengig von
     Cache-Hit oder -Miss. Ein Cache-Hit ersetzt diese Pruefung nicht,
     sondern liefert nur den Baum, gegen den sie laeuft. Schlaegt die
     Pruefung trotz gemeldetem Cache-Hit fehl, bricht der Lauf fail-closed
     ab (Cache-Korruption wird nicht stillschweigend akzeptiert).
  3. **`install.sh esp32`**: wird nur ausgefuehrt, wenn
     `steps.cache-esp-idf-tools.outputs.cache-hit != 'true'`. Bei einem
     echten Tools-Cache-Hit wird der Aufruf vollstaendig uebersprungen statt
     ihn gegen einen bereits warmen Cache erneut laufen zu lassen.
- Im Schritt "Gemeinsame Gateausfuehrung (ESP-IDF)" den `idf_tools.py
  install esp-clang`-Aufruf an dieselbe Bedingung knuepfen: nur ausfuehren,
  wenn `steps.cache-esp-idf-tools.outputs.cache-hit != 'true'`. Bei Hit wird
  direkt mit `. "$IDF_PATH/export.sh"` fortgefahren.
- Die beiden Cache-Treffer sind unabhaengig voneinander wirksam: Ein
  Checkout-Hit ohne Tools-Hit ueberspringt nur den Clone, ein Tools-Hit ohne
  Checkout-Hit ueberspringt nur `install.sh`/`esp-clang`-Install; da beide
  Schluessel an denselben unveraenderlichen `ESP_IDF_COMMIT` gebunden sind,
  bleibt jede Kombination korrekt (Tools, die zu genau diesem Commit
  installiert wurden, passen unabhaengig davon, ob der Checkout aus dem
  Cache oder frisch geklont kam).
- Bestehende, nachgelagerte Provenienz-/Versionspruefungen entfallen nicht:
  `verify_expected_esp_environment` in `scripts/run_pre_ready_gates.sh`
  (IDF_PATH/IDF_TOOLS_PATH/`idf.py` vorhanden) und die esp-clang-Pfad-/
  Versions-/`tools.json`-/`pyclang`-Pruefung in
  `scripts/run_esp_idf_static_analysis.py` laufen unveraendert bei jedem
  Lauf, unabhaengig davon, ob die Installationsbefehle uebersprungen wurden.

**Commit 3 — Dokumentation**
- `docs/CI_AND_QUALITY_GATES.md`, Abschnitt „CI-Pipeline": ergaenzen, dass
  ESP-IDF-Checkout und `IDF_TOOLS_PATH` ueber `actions/cache`, exakt an den
  gepinnten Commit gebunden, wiederverwendet werden; bei verifiziertem Hit
  entfallen Checkout, `install.sh esp32` und `esp-clang`-Install tatsaechlich,
  ohne die Verifikations- oder Gate-Reihenfolge zu aendern.
- Live-Pruefung auf aktuellem `main` (`dc017f4`) ergab: die fruehere
  `v6.0.2`-Dokumentationsinkonsistenz in `CI_AND_QUALITY_GATES.md` ist bereits
  behoben (Datei nennt durchgehend `v6.1`); kein Korrekturbedarf mehr. Sollte
  eine gezielte Pruefung waehrend der Umsetzung doch eine reale verbliebene
  Abweichung finden, wird nur diese redaktionell korrigiert.
- Korrektur aus dieser Revision: Die CI-Pipeline-Schrittbeschreibung
  ("Checkout und Python (mit `pip`-Cache)") wird auf den tatsaechlich
  verbleibenden Zustand ohne Pip-Cache korrigiert; es wird keine neue
  Pip-Cache-Loesung dokumentiert. Die bestehende ESP-IDF-Checkout-/
  Tools-Cache-Dokumentation bleibt inhaltlich unveraendert.

**Commit 4 — `docs/ROADMAP.md` und PR-Beschreibung synchronisieren**
- `docs/ROADMAP.md`: den verifizierten Live-Stand von PR #160 auf `MERGED`
  mit Merge-Commit `dc017f4ee7c4f33be240fac23c1686606f340225`
  synchronisieren; Issue #159 nur als weiterhin `OPEN` fuehren, solange der
  Owner es nicht schliesst; PR #161 als aktuelle parallele CI-/
  Governance-Arbeit mit der revidierten freigegebenen Plan-SHA und dem
  aktuellen Implementierungs-/Reviewstatus aufnehmen. Keine fachlichen
  Anforderungen in die Roadmap kopieren — nur Statuszeilen im bestehenden
  Roadmap-Format.
- PR-Beschreibung von #161: die veraltete Beschreibung ("Planungsphase",
  noch offene Caching-vs-Docker-Entscheidung) auf den tatsaechlichen Status
  und die dann freigegebene revidierte Plan-SHA aktualisieren.
- Dies ist eine notwendige Status-/Dokumentationswirkung gemaess
  `AGENT_WORKFLOW.md` §5 (Plan, PR-Beschreibung und `docs/ROADMAP.md` werden
  bei tatsaechlicher Status- oder Scopewirkung aktualisiert), keine
  Scopeerweiterung.

## Nachweis (Vorher/Nachher)

- **Vorher**: siehe Tabelle oben (Baseline-Run `35014280372`, `main`-Stand
  nach PR #160).
- **Nachher**: nach Owner-Freigabe und Wechsel auf `Ready for review` wird der
  erste reale GitHub-CI-Lauf dieses PR ausgewertet (`gh api
  repos/.../actions/runs/<id>/jobs`); dieser erste Lauf ist erwartungsgemaess
  ein **Cache-Miss** (Caches existieren noch nicht) und wird als solcher
  ausgewiesen, nicht mit dem Cache-Hit-Fall verwechselt. Fuer den
  Cache-Hit-Nachweis wird anschliessend `gh run rerun <run-id>` auf demselben
  Lauf ausgefuehrt (guenstiger und realistischer als ein erzwungener zweiter
  semantischer Push) und dessen Jobzeiten ausgewertet. Verglichen werden:
  - **Headline-Metrik**: Summe der Setup-Steps bis "Gemeinsame
    Gateausfuehrung (ESP-IDF)" (Cache-Restore-Steps eingerechnet) gegen die
    Baseline-Summe aus derselben Steps; ein mehrere GB grosser Cache-Restore
    braucht selbst einige zehn Sekunden, das "Nahe 0 s" gilt fuer die
    uebersprungenen Installationsbefehle, nicht fuer den Restore-Vorgang
    selbst.
  - Dauer "Format-/Analysewerkzeuge installieren" (erwartet: nahe 0 s statt
    12 s);
  - Dauer "ESP-IDF installieren" bei echtem Checkout-/Tools-Cache-Hit
    (erwartet: nur noch Provenienzverifikation, kein Clone/`install.sh`);
  - Gesamtlaufzeit;
  - tatsaechliche Cache-Eintragsgroessen (`gh cache list`) gegen das
    10-GB-Repository-Cache-Budget;
  - Bestaetigung, dass Tag-/Commit-/Sauberkeitspruefung, beide Profile,
    `esp-clang`-Analyse, Artefakt-Uploads und Secret-Scan im Nachher-Lauf
    weiterhin unveraendert `PASS` sind.
  - Reale Reichweitenbegrenzung (dokumentiert, nicht durch neue Infrastruktur
    geloest): `.github/workflows/build.yml` hat keinen `push`-Trigger — laut
    `CI_AND_QUALITY_GATES.md` gibt es "keinen automatischen identischen
    Wiederholungslauf nach dem Merge". Ein Merge nach `main` erzeugt damit
    selbst **keinen** Cache-Eintrag auf `main`; GitHub-Actions-Caches sind
    zudem nicht global, sondern nur fuer den erzeugenden Branch sowie fuer
    PRs mit direktem Zugriff auf dessen Basis-/Default-Branch-Cache
    sichtbar. Ohne einen main-seitigen Lauf existiert kein
    Default-Branch-Cache, von dem neue PR-Branches profitieren koennten.
    Der real erreichbare Nutzen dieses PR ist damit auf Folgelaeufe
    **innerhalb desselben PR-Branches** beschraenkt (Reruns und spaetere
    `synchronize`-Pushes bei unveraendertem `ESP_IDF_COMMIT`); ein neuer,
    unabhaengiger PR-Branch startet weiterhin mit einem Cache-Miss. Eine
    zusaetzliche Trigger- oder Cache-Warming-Infrastruktur (z. B. ein
    main-seitiger Aufwaerm-Workflow), um dies zu aendern, ist nicht
    Bestandteil dieses PR (KISS/YAGNI) und wird nicht eingefuehrt; der
    Nachweis misst ausschliesslich den tatsaechlich erreichten
    Rerun-/Same-Branch-Effekt und behauptet keinen repoweiten Cross-PR-Nutzen.
- Sollte ein erwarteter Effekt (z. B. wegen Runner- oder Cache-Backend-
  Verhalten) nicht zuverlaessig messbar sein, wird dies im PR-Nachweis
  transparent dokumentiert statt zusaetzliche Komplexitaet nachzuschieben.

## Betroffene Dateien

- `.github/workflows/build.yml`
- `docs/CI_AND_QUALITY_GATES.md`
- `docs/ROADMAP.md`
- PR-Beschreibung #161 (kein Repository-Dateiartefakt, aber Teil der
  Statuspflege aus Commit 4)

## Tests / gezielte Pruefungen

- `git diff --check` auf den geaenderten Dateien.
- Vor der Uebergabe an den Independent Review laeuft gemaess
  `AGENT_WORKFLOW.md` §7 der Builder-Static-Analysis-Self-Check auf dem
  Implementierungs-`HEAD`:
  ```
  export PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"
  bash scripts/run_pre_ready_gates.sh self-check
  ```
  Erwartetes Ergebnis, da keine C/C++-Datei geaendert wird:
  `CLANG_FORMAT=NOT_REQUIRED`, `CLANG_TIDY=NOT_REQUIRED`,
  `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS`.
- Fuer die verbindliche Pre-Ready-/Ready-/CI-Reihenfolge gilt ausschliesslich
  der kanonische `docs/AGENT_WORKFLOW.md` (Independent Review abgeschlossen
  -> `OPEN_BLOCKERS=0` -> Owner autorisiert lokalen Pre-Ready-Lauf ->
  `PRE_READY_LOCAL_GATES=PASS` -> Owner setzt `Ready for review` ->
  GitHub-CI `PASS` -> Merge-Gate); dieser Plan erfindet keine abweichende
  Reihenfolge. Der separate Cache-Miss-/Cache-Hit-Performance-Nachweis (siehe
  Abschnitt „Nachweis (Vorher/Nachher)") bleibt ein GitHub-CI-Nachweis nach
  dem Owner-Ready-Gate, da GitHub-gehostetes Cache-Backend-Verhalten lokal
  nicht reproduzierbar ist.

## Offene Entscheidungen

Keine. Die einzige offene Entscheidung der ersten Revision (Caching statt
offizielles Docker-Image) hat der Owner bei der Freigabe der Plan-SHA
`66ab4f3678a5265ca176998dfad1d428d1806982` ausdruecklich bestaetigt. Diese
Revision fuegt keine neue offene Entscheidung hinzu.
