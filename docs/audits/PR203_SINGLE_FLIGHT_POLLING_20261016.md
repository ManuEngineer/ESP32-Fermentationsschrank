# PR #203 – Single-Flight-Polling: Software-Ergebnis, 2026-10-16

**Builder-Verifikation: PASS. Independent Software Review: PENDING.**
Ownerfreigegebener Zusatzplan:
[`8094b7f1a149269b0ad3e1c49cf4366bdfc40e7c`](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/8094b7f1a149269b0ad3e1c49cf4366bdfc40e7c/docs/tasks/issue-27-pr203-single-flight-polling-plan.md),
einschliesslich P1. Die anschliessende Ownernachricht „plan ist freigegeben bitte
umsetzen“ autorisiert die Softwareumsetzung, keine Hardwarefortsetzung.

**Quell-/Testcommit:** `a9a1156a1d44d4679957526bbc34a30be6422e4e`, Branch
`agent/issue-27-r1-web-dashboard-plan`, bestehendes Issue #27 / Draft-PR #203.
Dieser Bericht und sein Nachweispaket sind ein nachfolgendes Dokumentationsdelta;
Builds/Self-Check beziehen sich auf den sauberen Implementierungscommit, nicht
auf einen spaeteren Dokumentations-HEAD. Kein Pre-Ready oder Hardware-PASS.

## Verhalten und Scope

`web_application_routes.cpp` serialisiert die bestehende Dreiergruppe aus
`status`, `temperatures`, `alerts` bis zum Abschluss aller JSON-Bodies. `F` haelt
das aktive Gruppen-Promise, `C` dessen gemeinsamen AbortController, `W` genau
einen vorgemerkten frischen Refresh nach Mutation. Der unmittelbare Erstabruf
und `setInterval(rf,5000)` bleiben; ausgelassene Ticks erzeugen keine Auftraege
und aktualisieren weiter Frische, gedimmte Anzeige und Aktionssperren.

Die Browserdeadline betraegt wie freigegeben **30 000 ms**. Deadline, HTTP-,
Transport- und JSON-Fehler teilen einen Fehlerpfad: Geschwister abbrechen,
Aktionsgrundlage sperren, Anzeige aktualisieren. Die Gruppensperre faellt erst
nach `Promise.allSettled` und Timerbereinigung. Auch eine verzoegerte oder
ausbleibende Abbruchbestaetigung erlaubt keine zweite Gruppe. Kein Sofort-Retry;
ein spaeterer regulaerer Tick darf nach vollstaendigem Cleanup wieder starten.

Der echte `send()`-Pfad setzt bei jeder verwertbaren Mutationsantwort `P=null`
**vor** `B=false`, aktualisiert UI/Anzeige und wartet `rf(1)` ab. Der Refresh
invalidiert/abortiert eine etwaige Vorgaengergruppe, wartet auf deren Cleanup
und startet erst danach einen neuen Snapshot. `LP` bleibt gedimmt erhalten.
Fehler, Abort oder zu alter Erfolg koennen alte Aktionen nicht reaktivieren.
Dies gilt auch fuer `stale`, `busy`, `confirmation-required`, `rejected`,
`too-large` und `write-failed`. Auth-, Sequenz-, Replay-, POST-Retry- und
Bestaetigungsregeln bleiben erhalten.

`G` schuetzt Ergebnisse gegen invalidierte Generationen; der alte zusaetzliche
`H`-Vergleich ist unter Serialisierung entbehrlich. Ein gueltiges Ergebnis
uebernimmt `LP`/`LT` auch dann, wenn zwei kausal aufeinanderfolgende Starts in
dieselbe Host-Uhrmillisekunde fallen. Das Alter bleibt vom Gruppenstart
gerechnet; ein 24-s-Erfolg ist erlaubt, aber fuer Aktionen bereits stale.

Fuer das bestehende Shellbudget werden ausschliesslich lokale JS-Schreibweisen
verkuerzt: wiederholte DOM-/Katalogeigenschaften und Authzustandsliterale ueber
konstante Schluessel, `Date.now`/Cacheliteral, lokale Namen und truthy/falsy-
Schreibweise des Katalog-Ladeflags. Keine Texte/Sprachen/Bedienung entfernt.
Nach Ruecknahme dieser dokumentierten Schreibweisen sind die dekodierten
Suffixbytes ausserhalb Refresh, Gruppenstatus und explizitem Mutationsdelta
gegen Planbasis `8094b7f` identisch (zusaetzlicher Offline-Diffcheck PASS).

**Unveraendert:** F1-Suffix-/Chunk-Vertrag, Fehlerantworten ohne Shellsuffix,
HTTP-Adapter, API-Pfade, F4-Gate/Schwelle und TCP-/WLAN-/HTTPD-/sdkconfig.
Programmkatalogabruf und Mutationen bleiben eigene Requestpfade. „Eine Gruppe“
ist keine globale Drei-Request- oder Socketgarantie.

## Ausgefuehrte Verifikation

| Nachweis | Ergebnis |
|---|---|
| [Ausfuehrender lokaler JS-Test](PR203_SINGLE_FLIGHT_POLLING_20261016/JS_POLLING.txt), Node v20.19.2 | **PASS, 40 Szenarien** gegen das aus C++ extrahierte Produkt-JavaScript; Fake-Fetch/DOM/Uhr, kein Netzwerk |
| Normale/langsame/haengende Gruppen | Maximal eine ausstehende logische Gruppe / drei Pollerrequests einschliesslich Bodies und Abort-Cleanup; keine Tick-Warteschlange |
| Deadline/Teilfehler/Erholung | Header- und Bodydeadline; Transport-, HTTP-, JSON-Fehler; Geschwisterabbruch, verzoegertes Settlement und ausbleibende Bestaetigung; Erholung ohne Anfragestau |
| P1 / echter `send()`-Pfad | Alle sieben verwertbaren Outcomes jeweils mit frischem, abgebrochenem, fehlgeschlagenem und zu altem Refresh; alle Runbuttons bis zum frischen Snapshot gesperrt, danach zustandsabhaengig freigegeben |
| Mutationsrennen | Alte Bodies nach signalisiertem Abort verworfen; Abschlussrennen, ausstehende Header, kein aktiver Vorgaenger, gleiche Uhrmillisekunde und zusammengefasste explizite Refreshanforderungen |
| [Native Web-/Auth-/HTTP-Suiten](PR203_SINGLE_FLIGHT_POLLING_20261016/NATIVE_TESTS.txt) | **PASS, 144/144**: Routes 100, Authrecords 15, Browserpolicy 4, Sessions 20, HTTP-Lifecycle 5 |
| Native Shell-/Golden-Tests | Alle bisherigen Zustaende, Sprach-/Codec-Texte, Frische-/Generationsschutz, Sequenzreserve, vollstaendige 503-Antwort und Suffixfreiheit PASS; Golden nur fuer beabsichtigtes JS-Delta angepasst |
| [Reale IDF-HTTP-Adapter-Hostregression](PR203_SINGLE_FLIGHT_POLLING_20261016/HTTP_ADAPTER.txt) | **PASS**, 25 PASS-Marker einschliesslich parametrierter Faelle, Chunkabschluss und Fehlerpropagation |
| `python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree` | **PASS**, beide Profile seriell auf sauberem `a9a1156`; IDF `fff9895c82d744c7237be8847347bdd1b07c6643` / v6.1 |
| [Kanonischer Builder-Self-Check](PR203_SINGLE_FLIGHT_POLLING_20261016/SELF_CHECK.txt) | **PASS** auf `a9a1156`, clang-format/clang-tidy 21.1.3 und Boardprofilguard PASS; kein voller Pre-Ready |

Die native Routes-Suite wurde nach Hinzufuegen der reinen Byte-Metrikausgabe
erneut 100/100 erfolgreich ausgefuehrt; die vier anderen Suiten und der
Produktcode blieben dabei unveraendert. Der erste HTTP-Hostaufruf scheiterte an
bereits belegtem Port 8080 (`bind`, errno 98). Der belegende Prozess blieb
unangetastet. Derselbe Test lief anschliessend in einem eigenen User-/Netzwerk-
Namespace mit ausschliesslich aktiviertem Loopback erfolgreich; keine
Adapteraenderung, kein Zugriff auf das Heimnetz/Devboard. Relevante Adapter- und
Hosttestquellen entsprechen unveraendert dem Implementierungscommit.

Native Messung des tatsaechlich zusammengesetzten Antwortbodys: angemeldet
**8056 B / 136 B Reserve**, Passwortschutz aus **8055 B / 137 B Reserve**.
Die bestehende mindestens 128-B-Reserve deckt auch die 19 B fuer die breiteste
Sequenz ab. Mit zwanzigstelliger Sequenz ergibt sich angemeldet 8075 B;
8192-B-Grenze unveraendert. Shellzuwachs bei gleicher Sequenz: **8 B** gegen
8048 B der Baseline, ohne neue Route oder vergroesserten dynamischen Head.

## Artefaktbindung und Review-Einstieg

Vollstaendige Datei-/App-/ELF-Hashes, Profilgroessen und Quellenbindung:
[VERIFICATION.json](PR203_SINGLE_FLIGHT_POLLING_20261016/VERIFICATION.json).
Die App-Metadaten `a9a1156`, die volle `APP_SOURCE_GIT_SHA` in beiden
Kompilierungsdatenbanken und die eingebetteten ELF-Digests wurden offline
gegen die Dateien geprueft. Vorhandene alte `build/*/artifact-manifest.json`
wurden nicht als Nachweis dieses Builds verwendet oder umgeschrieben.

| Profil | App SHA-256 | ELF SHA-256 |
|---|---|---|
| esp32_bringup | `794a33b92452656aaf8b3b646b757d9aa7ad6845a2c0ef3e987b3d5b2a8e3cff` | `3e415854134d05c7e55f0c307930b30cbadb6731bdf20284ecf2f557ab57f85c` |
| esp32_release | `2d069d2db793359177379f492559c69fa949cb470351d57f60dd095889619305` | `dca3cc664f3c143797f1caf29ca51bd3b9afe88eb081f914bec595f065523601` |

Die gefilterten Textnachweise und JSON sind ueber
[SHA256SUMS](PR203_SINGLE_FLIGHT_POLLING_20261016/SHA256SUMS) gebunden.
Sie enthalten Testnamen/Ergebnisse und Groessen, keine HTTP-Headers/-Bodies,
Zugangsdaten oder privaten Readbacks. Buildstdout, externe Compilerdiagnosen
und lokale Toolpfade sind ausgelassen. Lokale Rohlogs bleiben im ignorierten
`build/pr203_polling_validation_a9a1156/` erhalten. Es wurden keine
Hardware-Rohartefakte veraendert.

## Grenzen und naechstes Gate

Die Tests beweisen lokale JS-Orchestrierung und Software-/Adaptervertraege.
Echte Browser-/WLAN-/ESP32-Abbruchwirkung, serverseitige Socketfreigabe und
B4-Wiederverbindung sind damit **NOT_PROVEN**. Browser-Suspendierung kann die
Zustellung eines 30-s-Timers verzoegern; diese Deadline ist keine harte
Echtzeit- oder neue R1-Latenz-Abnahmeschranke. Der Fix begrenzt einen belegten
Polling-Schwachpunkt, beweist aber keine B4-Ursache oder deren Behebung.

Die beiden Code-Scanning-Altbefunde und die Empfehlung einer separaten Triage
bleiben wie im freigegebenen Zusatzplan; kein Security-Zusatzfix. Es gab keinen
Geraetezugriff, UART-Start, Flash, Reset, Browser-/B0–B4-/Lastlauf oder G2.
Kein voller Pre-Ready, Ready-Wechsel oder Merge. Naechster Schritt ist der
**unabhaengige Software-Ergebnisreview**, keine automatische Hardwareaktion.

`B4_RECONNECT=FAIL_OBSERVED` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` ·
`G1_QUALIFICATION=NOT_PROVEN` · `ACTUATOR_RELEASE=NO`.
