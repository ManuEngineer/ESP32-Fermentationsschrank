# Issue #29 – ESP32-Bring-up, Partition, Ressourcen und sichere Ausgangszustände

## 1. Status, Basis und Owner-Gate

Issue #29 ist live `OPEN`/`READY`. `READY` autorisiert ausschließlich den
Plan-first-Draft-PR. Es gibt keine Firmwareimplementation, kein Flashen und
keine Hardwaremessung ohne ausdrückliche Ownerfreigabe der exakten
Plan-Commit-SHA.

Verifizierte Planungsbasis:

- Repository: `ManuEngineer/ESP32-Fermentationsschrank`
- Basis: `main @ 87dd593fcdc8d26831873a4163b174340b4347c0`
- ESP-IDF: `v6.0.2`
- ESP-IDF-Commit: `7101770dc6db2667b3c477cc31365dd1acd6db4e`
- Branch dieses Plan-PRs: `agent/issue-29-esp32-bringup-plan`
- Kanonischer Plan: `docs/tasks/issue-29-implementation-plan.md`
- Planfreigabe: Owner muss die exakte Plan-Commit-SHA freigeben

Die strategische Reihenfolge bleibt:

```text
#29 -> #90 -> #25 -> #26 -> #31 -> #30 -> #32 -> #33
```

Der native Codex Plan Mode ist nur das Arbeitsmittel für die Planung. Er ist
weder Freigabeartefakt noch Implementationserlaubnis. Dieser versionierte
Markdown-Plan ist die einzige kanonische Planwahrheit.

### Unbedingtes Plan-first-Stop-Gate

Nach dem Plan Mode sind zunächst ausschließlich diese Schritte erlaubt:

1. frischen Checkout des live verifizierten `main` herstellen;
2. den oben genannten Branch erstellen;
3. diesen vollständigen Plan anlegen;
4. `docs/ROADMAP.md` nur minimal synchronisieren;
5. ausschließlich Plan und Roadmap committen;
6. einen Draft-PR für Issue #29 erstellen;
7. Planpfad, exakte Plan-SHA, Base-SHA und Branch im PR ausweisen;
8. genau einen aktuellen `SESSION HANDOVER` erstellen;
9. anhalten.

Vor Ownerfreigabe der exakten Plan-SHA erfolgen ausdrücklich keine Änderung
an `main/app_main.cpp`, keine Firmwareimplementation, keine Buildprofiländerung,
kein Flashen und keine Hardwaremessung. Kein `Ready for review`, Merge,
Auto-Merge oder Issue-Schluss.

## 2. Ziel und Scope

Issue #29 liefert die reale, sichere und aktorfreie ESP32-32E-Baseline für die
folgenden Issues:

- Boardrevision und reale Flashgröße erfassen;
- Betrieb ohne PSRAM nachweisen;
- aktuelle generierte Partitionstabelle erfassen, ohne eine finale
  Produktionspartitionierung festzulegen;
- reproduzierbares Flashen, Booten, Resetten und UART-/ROM-Bootloader-Recovery
  dokumentieren;
- Resetursachen diagnostisch erfassen;
- Heap, Minimum-Free-Heap, größten zusammenhängenden freien Block und
  Task-Stack-High-Water-Mark erfassen;
- GPIO-/Businventar und sicher zugängliche unbelastete Pegel dokumentieren;
- das offene reale `CommandDecision`-Ressourcen- und Allokationsgate aus PR #53
  auf einem ESP32 ohne PSRAM nachweisen.

Die Baseline bleibt aktorfrei. `HARDWARE_UNVERIFIED` wird durch #29 nicht
global aufgehoben. `TBD_HARDWARE`, `TBD_COMMISSIONING` und
`TBD_IMPLEMENTATION_BUDGET` werden nicht als produktive Laufzeitwerte verwendet.

### Nicht-Scope

Nicht vorweggenommen werden:

- der produktive ESP-IDF-NVS-Adapter und alle übrigen Inhalte von #90;
- Sensor-, Display-, Touch-, WLAN-, UI- oder Fachintegrationen;
- Peltier-, BTS7960-, Lüfter-, Summer-, MOSFET- oder reale Aktorintegration;
- belastete MOSFET- oder Verbrauchertests aus #32/#33;
- finale GPIO-, Kanal-, Controller- oder Verbraucherzuordnung;
- finale Produktionspartitionierung;
- OTA-Bibliotheken, OTA-Slots oder Firmwaredownload;
- ein neues `IResourceMonitor`;
- eine neue Persistenz-, Safety-, Wire- oder Parallelarchitektur;
- eine reale Safetyfreigabe oder eine Aufhebung von `HARDWARE_UNVERIFIED`.

Alle Verbraucher bleiben physisch getrennt oder nachweislich sicher inaktiv.

## 3. Verbindliche Quellen und Architekturgrenzen

Der Plan verwendet die bestehende Wahrheit aus:

- `docs/AGENT_WORKFLOW.md` und `docs/ENGINEERING_PRINCIPLES.md`;
- `docs/SPECIFICATION_REVIEW.md` und `docs/DECISIONS.md`, insbesondere
  ADR-002, ADR-008, ADR-011, ADR-012 und ADR-013;
- `docs/HARDWARE.md`, `docs/HARDWARE_REVISIONS.md` und `docs/OPEN_POINTS.md`;
- `docs/ESP_IDF_UPGRADE_CONTRACT.md`;
- `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`;
- `docs/SYSTEM_SAFETY_AND_RECOVERY.md`;
- `docs/FIRMWARE_UPDATE_AND_ROLLBACK.md` und `docs/ACCEPTANCE_TESTS.md`;
- dem Live-Issue #29 und der aktuellen `docs/ROADMAP.md`.

ADR-013 bleibt unverändert:

- `device_platform`: portable, anwendungsneutrale Ports und Dienste;
- `device_platform_esp_idf`: konkrete ESP-IDF-Adapter;
- `fermentation_app`: Fachlogik nur gegen abstrakte Plattformports;
- `device_platform_test_support`: Testhilfen, nie Produktionsabhängigkeit.

Bestehende Modelle, `CommandDecision`-/Apply-Verträge, ResetCause-Quelle,
Profile, Buildskripte, Persistenzverträge und Tests werden wiederverwendet.
Parallelverträge und stille Ersatzmodelle sind unzulässig.

## 4. Profile, Flash, Partition und Boot

### Profile und Toolchain

`esp32_bringup` und `esp32_release` bleiben getrennte ESP-IDF-Profile.

Beide Profile bleiben:

- ESP32-32E ohne PSRAM-Abhängigkeit;
- auf 4 MB als bestehende Buildgrenze ausgerichtet, aber mit realer
  Flashmessung;
- ohne OTA und ohne automatischen Firmwaredownload;
- ohne reale Aktoren;
- mit fail-closed Boot- und Fehlerverhalten.

Die `esp32_bringup`-only Probe- und Fault-Instrumentierung wird vollständig aus
`esp32_release` ausgeschlossen. Sie darf keine produktive Laufzeitarchitektur
oder neue Plattformabhängigkeit erzeugen.

### Reale Flash- und Partitionserfassung

Die Flashgröße wird über den bestehenden ESP-IDF-/UART-Werkzeugweg am realen
Board gemessen und nicht nur aus `sdkconfig` oder Compile-Definitionen
abgeleitet.

Die aktuell generierte Partitionstabelle wird aus den realen Buildartefakten
erfasst, einschließlich:

- Partitionen, Offsets und Größen;
- Bootloader-, Partition- und App-Binärdateien;
- belegtem Flash und verbleibendem Platz;
- Profil, ESP-IDF-Version und ESP-IDF-Commit;
- Source-/Implementierungs-HEAD;
- relevanter Konfigurations- und Artefaktprüfsummen.

Die Tabelle ist eine Bring-up-Baseline. Es wird keine finale
Produktionspartitionierung, kein OTA-Slot und keine OTA-Speicherreserve
festgelegt.

### UART, ROM-Bootloader und Reset

Der reproduzierbare Weg verwendet UART/FT232RL und den ROM-Bootloader:

- dokumentierter UART-Port;
- manuelles Boot-/Reset-Verfahren;
- reproduzierbarer Flash- und Monitoraufruf;
- reproduzierbare Recovery bei fehlgeschlagenem Start;
- keine OTA-Abhängigkeit.

Die bestehende Reset-Cause-Quelle wird diagnostisch verwendet. Resetursachen
werden geloggt, aber nicht gezählt, persistiert oder als automatische
Resume-Entscheidung verwendet.

Alle normalen Boot-/Resetpfade starten aktorfrei und führen die erforderliche
Revalidierung erneut aus.

## 5. Sichere Hardware- und Pegelgrenzen

In #29 werden nur sicher zugängliche folgende Punkte geprüft:

- MCU-Pegel;
- Steuer- und Gate-Pegel;
- Boot-Pegel;
- unbelastete Messpunkte.

Es gibt keine externe 12-V-Verbraucherversorgung, keine angeschlossenen
Verbraucher und keine belastete MOSFET-Prüfung. Aus #29 wird keine Aussage über
vollständige MOSFET-Polarität, aktive Kanalzuordnung oder reale
Verbraucherwirkung abgeleitet.

Aktive MOSFET-/Verbraucherzuordnung und belastete Prüfungen bleiben #32
vorbehalten. Unbekannte, nicht sicher zugängliche oder nicht bestätigte Punkte
bleiben `TBD_HARDWARE`.

Während des 35-Sekunden-Smokes werden Brownout, Panic, Watchdog und
unerwartete Resets als Fehlerkriterium beobachtet. Eine absichtliche
Unterspannungs- oder Brownout-Injektion ist ohne ausdrücklich sicheren
Prüfaufbau nicht Bestandteil von #29 und wird andernfalls als `NOT_RUN` oder
späteres Hardwaregate dokumentiert.

## 6. Ressourcenprobe: realer ESP32 ohne PSRAM

Eine schmale bring-up-only Probe verwendet den bestehenden
`CommandDecision`-, Entscheidungs- und Apply-Vertrag mit dem maximalen
48/96/1024-Kandidaten.

Auf dem realen ESP32 ohne PSRAM werden vier Probezeitpunkte erfasst:

1. vor der Decision;
2. während die vollständige `CommandDecision` gehalten wird;
3. während der lokalen RAM-Anwendung;
4. nach Abschluss und Freigabe des lokalen Probeabschnitts.

Jeder Probezeitpunkt erfasst mindestens:

- freien Heap;
- Minimum-Free-Heap;
- größten zusammenhängenden 8-Bit-Heap-Block;
- Task-Stack-High-Water-Mark.

Die Stack-Einheit wird entsprechend der verwendeten ESP-IDF-/FreeRTOS-API
korrekt ausgewiesen. Eine Umbenennung in Bytes erfolgt nur bei verifizierter
Umrechnung.

Die Ressourcenprobe:

- erzeugt eine reale maximale `CommandDecision` aus den bestehenden
  Fachmodellen;
- hält die vollständige Decision tatsächlich im Zielprozess;
- führt nur lokale RAM-Entscheidung und lokale Apply-Logik aus;
- ruft keine Persistenz, kein NVS und keine Sinks auf;
- aktiviert keine Aktoren;
- fügt kein UI, keine Parallelarchitektur und keinen neuen Ressourcenport ein;
- führt kein `IResourceMonitor` ein.

Die bestehenden Smoke-Verträge behalten exakt zwei reguläre
Ressourcenmessungen. Die vier internen Probezeitpunkte sind separat markierte
Probe-Artefakte und zählen nicht als zusätzliche reguläre Smoke-Messungen.

## 7. Fehlervertragsprobe und On-Target-Fault-Seam

Die Allokationsfehlerreaktion wird nicht nur nativ geprüft. Der verbindliche
On-Target-Nachweis läuft im `esp32_bringup`-Kontext gegen den bestehenden
produktiven Vertragsweg:

```text
CommandDecision
  -> bestehender Command-/Apply-Pfad
  -> RunPersistenceCoordinator / Application-Orchestrator
  -> bestehender IStateStore-/Persistenzvertrag
```

### Ownerentscheidung: lokaler `esp32_bringup`-only Seam

Es wird ein eng begrenzter, compile-time aktivierter Fault-Seam an der
bestehenden Kandidatenkopie vor lokaler Apply-/Persistenzmutation verwendet.

Der Seam:

- ist ausschließlich `esp32_bringup`-only;
- ist kein öffentlicher Produktionsport;
- ist keine produktive Allocator-API;
- überschreibt nicht global `operator new`;
- baut keine neue Speicherarchitektur;
- wird vollständig aus `esp32_release` ausgeschlossen;
- ändert weder Persistenzschema noch Wireformat;
- erzeugt keine zweite Safety-Wahrheit.

Der Fault-Seam simuliert deterministisch das Scheitern der relevanten
Kandidaten-/Allokationsgrenze und führt den bestehenden fail-closed
Nicht-Erfolgsweg aus. Eine eigenständige Wegwerf-Testanwendung wird nicht
gebaut.

### Fehlervertragsprobe

Der `esp32_bringup`-Probeabschnitt:

1. erzeugt die reale maximale `CommandDecision`;
2. hält den vollständigen fachlichen Zustand vor dem Apply fest;
3. aktiviert einmalig den lokalen Allokationsfehler;
4. ruft den bestehenden Command-/Apply-/Persistenzpfad auf;
5. verwendet einen kleinen lokalen `IStateStore`-Testdouble nur für diesen
   bring-up-only Nachweis;
6. verwendet sichere All-off-Sinks zur Beobachtung;
7. prüft Zustand, Ergebnis, Persistenzaufrufe, committed Bytes und
   Sink-Aufrufe.

Der On-Target-Nachweis muss beweisen:

- fachlicher Zustand bleibt unverändert;
- keine Teilmutation und kein teilweises Publish;
- kein Persistenz-Commit;
- keine Persistenzfreigabe wird fälschlich als erfolgreich behandelt;
- keine neue Aktorfreigabe entsteht;
- Safety bleibt fail-closed.

Zusätzlich werden auf demselben produktionsnahen Pfad deterministische
Store-Fehler, insbesondere Kapazitätsfehler und unbekannter Commitausgang,
beobachtet. Kein NVS-Adapter aus #90 und keine reale Aktorintegration aus
#32/#33 werden hierfür vorgezogen.

Der native `SimulatedPersistentStateStore` und vorhandene native
Allokationsharnesses bleiben ergänzende Regressionstests. Sie ersetzen den
realen On-Target-Nachweis nicht.

Falls sich bei der Umsetzung zeigt, dass dieser lokale Seam nur durch einen
öffentlichen Produktions-Allocator, eine globale Speicherarchitektur oder eine
neue Parallelstruktur möglich ist, wird die Umsetzung angehalten. Dann werden
Befund, Auswirkungen und minimale Owneralternativen vorgelegt; es erfolgt
keine stille Scopeerweiterung.

## 8. Hardware-Smoke und Abnahme

`esp32_bringup` und `esp32_release` laufen mindestens 35 Sekunden auf demselben
unbelasteten Board.

Der Smoke muss zeigen:

- erwartete Boot- und Profilzeilen;
- aktorfreien sicheren Hardwarezustand;
- monotone Uptime;
- erwartete Heartbeats;
- exakt zwei reguläre Ressourcenmessungen;
- keine unerwartete Aktoraktivität;
- keinen Panic;
- keinen Watchdog;
- keinen Brownout;
- keinen unerwarteten Reset.

Das Nachweisprotokoll enthält:

- Board und Revision;
- Flash-ID und reale Flashgröße;
- PSRAM-Status;
- generierte Partitionstabelle;
- UART-Port und ROM-Recovery-Weg;
- Profil;
- Firmware-, Build- und Source-SHA;
- Smoke-Dauer;
- Ressourcenwerte;
- Resetursache;
- sichere Pegelmesspunkte;
- Ergebnisstatus.

Ein Build ist kein Hardware- oder Pegelnachweis. Nicht ausgeführte Nachweise
werden nicht als bestanden dargestellt.

## 9. Gezielte Tests und Ergebnisstatus

Nach Ownerfreigabe und ausschließlich auf dem finalen
Implementierungs-HEAD sind folgende gezielte Nachweise vorgesehen:

- direkt betroffene Command-/Apply- und `CommandDecision`-Native-Tests;
- direkt betroffene Persistenz-/Orchestrator-Tests;
- Tests der Fehlerreaktion ohne Teilmutation und ohne Aktorfreigabe;
- gezielte Architekturgrenzen;
- Secret-Scan;
- Formatprüfung;
- `git diff --check`;
- die dokumentierten ESP-IDF-Build-, Static-Analysis- und Report-Schritte;
- beide Hardware-Smokes;
- reale Ressourcen- und Fehlervertragsprobe.

In diesem Plan-only-PR werden keine Firmware-, Native- oder Hardwaretests
ausgeführt. Ihr Status ist `NOT_RUN`.

Alle späteren Nachweise verwenden ausschließlich:

- `PASS` für nachweislich erfüllte Kriterien;
- `FAILED` für einen konkreten negativen Nachweis;
- `BLOCKED` für eine reproduzierbare Umgebungs- oder Hardwareblockade;
- `NOT_RUN` für nicht ausgeführte Prüfungen.

`NOT_RUN` ist kein bestandenes Ergebnis.

## 10. Dokumentation und Roadmap

Der Plan dokumentiert die betroffenen Module und Vertragsgrenzen, ohne
Produktionscode zu ändern. Nach Ownerfreigabe sind insbesondere
`main/app_main.cpp`, die ESP-IDF-Profilkomposition und die
bring-up-only Probe betroffen; produktive Fachmodelle, Persistenz- und
Plattformverträge werden nur im genehmigten Umfang berührt.

`docs/ROADMAP.md` wird minimal synchronisiert:

- #29 bleibt Priorität 1;
- die Reihenfolge `#29 -> #90 -> #25 -> #26 -> #31 -> #30 -> #32 -> #33`
  bleibt unverändert;
- der neue Plan-only Draft-PR, Planpfad und die ausstehende Ownerfreigabe
  werden als nächstes Gate vermerkt;
- fachliche Anforderungen werden nicht in die Roadmap kopiert.

## 11. Plan-only Commit, Draft-PR und Handover

Der Commit dieses Plan-PRs enthält ausschließlich:

- `docs/tasks/issue-29-implementation-plan.md`;
- die minimal notwendige Änderung an `docs/ROADMAP.md`.

Der Draft-PR enthält mindestens:

- Issue #29;
- Base-SHA `87dd593fcdc8d26831873a4163b174340b4347c0`;
- Branch `agent/issue-29-esp32-bringup-plan`;
- Planpfad `docs/tasks/issue-29-implementation-plan.md`;
- exakte Plan-Commit-SHA;
- `IMPLEMENTATION: NOT_STARTED`;
- `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`;
- Tests und Hardwaregates als `NOT_RUN`;
- ausdrücklichen Stop vor jeder Firmware- oder Hardwareänderung.

Nach dem Push werden Branch, Base, HEAD, Draft-Status, PR-Body, Issue-Link und
Plan-SHA remote erneut gelesen. Danach wird genau ein aktueller
`SESSION HANDOVER` erstellt. Er nennt Branch, HEAD, exakte Plan-SHA, geänderte
Dokumente, `NOT_RUN`-Nachweise, offene Hardware-/Owner-Gates und als nächsten
Schritt ausschließlich die Ownerfreigabe der exakten Plan-SHA.

Danach STOP. Keine Firmwareänderung, kein Flashen, keine Hardwaremessung,
kein Ready for review, kein Merge, kein Auto-Merge und keine Issue-Schließung.
