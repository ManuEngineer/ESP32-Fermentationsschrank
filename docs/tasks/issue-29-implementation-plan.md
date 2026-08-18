# Issue #29 – ESP32-Bring-up, Partition, Ressourcen und sichere Ausgangszustände

Diese vollständige Planrevision korrigiert die Planreview-Befunde F1–F7 des
vorherigen Plan-Commits `b4cc9e367145cb761ba72db731416ec969f798b7`. Sie ersetzt
die vorherige Planfassung vollständig; ihre neue Commit-SHA ist das erneute
Owner-Gate.

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

### Aussagegrenze des #29-Ressourcennachweises

Dieser Nachweis schließt das offene PR-#53-Gate erstmals auf realer
ESP32-Hardware ohne PSRAM für den tatsächlich in #29 ausgeführten
Bring-up-nahen Pfad. Er beweist nicht die spätere Ressourcenwirkung von noch
nicht existierenden UI-, NVS-, Web-, Display- oder Parallelkonsumenten.

Sobald solche Konsumenten entstehen, müssen ihre zusätzlichen Stack-, Heap-,
Fragmentierungs- und Parallelwirkungen erneut gemessen werden. #29 erfüllt
sein eigenes Ressourcen-Akzeptanzkriterium mit dem realen Bring-up-Nachweis;
die abschließende Messung unter Parallel-/Releasebelastung bleibt als späteres
Lastgate, insbesondere #37, sichtbar offen. Ein #29-Nachweis darf dieses
spätere Gate nicht stillschweigend als bestanden markieren.

### Sichere Ausführungsgrenze für den realen Probeweg

Der reale `CommandDecision`-/`RunCommandState`-Pfad läuft nicht implizit auf
dem normalen `app_main`-Stack. PR #53 dokumentiert für den Xtensa-/ESP32-ABI
`sizeof(CommandDecision) = 8.608 B` und weist ausdrücklich auf das Stack- und
Integrationsrisiko hin. Da `sdkconfig.defaults` keinen eigenen Wert für
`CONFIG_ESP_MAIN_TASK_STACK_SIZE` setzt, wird der Main-Task-Stack weder
pauschal erhöht noch als ausreichende Probegrenze angenommen.

Die Ressourcen- und Fehlervertragsprobe verwendet deshalb einen separaten,
transienten Diagnose-Task, der nur im Profil `esp32_bringup` registriert wird:

- `main/app_main.cpp` bleibt Composition Root und startet, überwacht und
  beendet den Probeablauf über eine kleine private Bring-up-Schnittstelle;
- die große Decision-/Apply-/Persistenzausführung liegt in der privaten
  `main/issue_29_bringup_probe.hpp`/`.cpp`-Taskfunktion und läuft nicht auf
  dem `app_main`-Stack;
- der Task wird vor der eigentlichen Probe zunächst blockiert, damit
  `app_main` den Heap-/Stack-Baselinepunkt vor und nach der Task-Erzeugung
  getrennt erfassen kann. Erst danach wird genau ein begrenztes Startsignal
  gegeben;
- der Task besitzt nur die für #29 erforderliche Diagnosekoordination, keine
  allgemeine Task-, Ressourcen- oder Monitoringplattform, und wird nach dem
  Ergebnis sauber beendet und freigegeben;
- die Ressourcenprobe und die Fehlervertragsprobe bleiben fachlich getrennt,
  nutzen aber dieselbe sichere Task-Ausführungsgrenze. Nur die
  Fehlervertragsprobe verwendet den lokalen Storedouble und All-off-Sinks;
- `esp32_release` und native Builds registrieren weder diese Taskquelle noch
  ihren privaten Probeport.

Die initiale Task-Stackgröße wird nicht als Produktivwert erfunden. Vor der
ersten Hardwareausführung wird sie für den tatsächlich kompilierten
`esp32_bringup`-Pfad begründet und im Messprotokoll festgehalten:

1. Zielcompiler und Xtensa-Build prüfen `sizeof(CommandDecision)` — erwartet
   sind die aus PR #53 bekannten 8.608 B — sowie `sizeof(RunCommandState)` und
   die Größen aller absichtlich auf dem Task-Stack gehaltenen Probeobjekte;
2. der tatsächlich kompilierte Aufrufpfad wird einschließlich der
   Compiler-Stack-Usage-/Map-Ausgaben, soweit der ESP-IDF-Toolchainlauf sie
   liefert, und einer manuellen Prüfung der verbleibenden lokalen Frames
   erfasst;
3. daraus werden die maximale statische Objekt-/Framebelegung, die
   API-/Aufrufreserve und ein konservativer, begründeter Messpuffer zu einer
   ausgerichteten Bring-up-Taskgröße abgeleitet. Dieser Wert gilt nur für den
   Diagnose-Task und wird nicht in `sdkconfig.defaults`,
   `CONFIG_ESP_MAIN_TASK_STACK_SIZE` oder einen Produktionsvertrag
   übernommen;
4. fehlen eine belastbare Stack-Usage-Ausgabe, ein vollständiger Call-Path
   oder eine begründbare Reserve, bleibt die Hardwareausführung `BLOCKED`.

Die Messung trennt explizit Task-Stackreserve und dynamische Heapkosten:

- `B0` vor `xTaskCreate`: freier Heap, Minimum-Free-Heap, größter Block und
  Main-Task-High-Water-Mark ohne Diagnose-Task;
- `B1` nach Erzeugung des zunächst blockierten Diagnose-Tasks: derselbe
  Heap-/Block-/Main-Task-Satz, damit TCB- und Task-Stack-Allokation sichtbar
  bleiben, aber noch keine `CommandDecision`-Kopie oder dynamische Strings
  bewertet werden;
- innerhalb des Diagnose-Tasks: Task-Stack-High-Water-Mark und die vier
  fachlichen Probezeitpunkte vor Decision, bei vollständig gehaltener
  Decision, während lokaler Apply und nach Abschluss/Freigabe;
- innerhalb jedes dieser Punkte: freier Heap, Minimum-Free-Heap und größter
  zusammenhängender 8-Bit-Block. Dynamische Strings/Kopien werden als
  Heapwirkung dokumentiert und nicht als Stackreserve ausgegeben;
- `B2` nach Taskabschluss und Freigabe: Heap-/Block-/Main-Task-Werte sowie die
  eindeutige Task-Lifecycle-Information. Die Zusatzallokation des
  Diagnose-Tasks darf in keinem Ergebnis als Teil der allgemeinen
  Anwendungslast verborgen werden.

Der konfigurierte Stackwert wird in der tatsächlichen Einheit der verwendeten
ESP-IDF-/FreeRTOS-Task-API und zusätzlich, nur mit verifizierter Umrechnung,
in Bytes dokumentiert. Der Task veröffentlicht sein Ergebnis über die kleine
private Abschlusskoordination, signalisiert `app_main` den Abschluss und wird
danach über den bestehenden FreeRTOS-Lifecyclepfad selbst beendet; `app_main`
erfasst `B2` erst nach nachweisbarer Freigabe. Ein nicht eindeutig besitzbarer
oder nicht freigebbarer Task ist `FAILED` beziehungsweise `BLOCKED`.

Der Task wartet nur innerhalb eines begrenzten, überwachten Ablaufs, gibt an
definierten Stellen Schedulerzeit ab und meldet einen eindeutigen Abschluss-
oder Fehlerstatus. Stackoverflow, Watchdog, Panic, fehlgeschlagene
Task-Erzeugung oder ein nicht ausreichend begründbarer Startstack sind
`FAILED` beziehungsweise `BLOCKED`, niemals `PASS`. Die konfigurierte
Taskgröße, alle Herleitungsdaten, die gemessene High-Water-Mark und die
Heapkosten werden in `docs/ISSUE_29_MEASUREMENTS.md` dokumentiert.

Der Diagnose-Task verändert den fachlichen Vertrag nicht und verschiebt die
große `CommandDecision` nicht künstlich auf den Heap. Spätere reale
UI-/NVS-/Web-/Display- und Parallelkonsumenten müssen ihre eigenen Task-,
Stack- und Heapwirkungen erneut messen; #29 bleibt der erste Bring-up-
Nachweis und schließt das spätere #37-Lastgate nicht.

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

Auf demselben produktionsnahen Pfad wird keine allgemeine Store-Fehlermatrix
und kein unabhängiger
`CommitOutcomeUnknown`-/Power-Cut-Test vorgezogen. Der lokale Storedouble
liefert nur die für den Probeaufbau notwendige initiale Read-Sicht und zeichnet
jeden Write-Versuch auf. Beim injizierten Allokationsfehler muss die Writezahl
null bleiben; jeder Write-Versuch ist ein Probe-Fehler und kann keinen Commit
als Erfolg melden. Damit wird unmittelbar die fehlende Persistenzfreigabe des
#29-Fehlervertrags beobachtbar, ohne #90 zu duplizieren.

Allgemeine NVS-, Kapazitäts-, Power-Cut-, Readback- und
`CommitOutcomeUnknown`-Validierung bleibt dem bestehenden Store-Vertrag und
#90 vorbehalten. Keine reale Aktorintegration aus #32/#33 wird hierfür
vorgezogen.

Der native `SimulatedPersistentStateStore` und vorhandene native
Allokationsharnesses bleiben ergänzende Regressionstests. Sie ersetzen den
realen On-Target-Nachweis nicht.

Falls sich bei der Umsetzung zeigt, dass dieser lokale Seam nur durch einen
öffentlichen Produktions-Allocator, eine globale Speicherarchitektur oder eine
neue Parallelstruktur möglich ist, wird die Umsetzung angehalten. Dann werden
Befund, Auswirkungen und minimale Owneralternativen vorgelegt; es erfolgt
keine stille Scopeerweiterung.

### Konkrete Compile-time-Isolation

Der Seam sitzt an der bestehenden Kandidaten-/Apply-Grenze in
`lib/fermentation_app`, nicht nur in `main/app_main.cpp`. Die Isolation wird
mit der vorhandenen Kconfig-Profilwahl und privaten Component-Definitionen
umgesetzt:

- `lib/fermentation_app/CMakeLists.txt` setzt nur bei
  `CONFIG_APP_PROFILE_ESP32_BRINGUP` die private Definition
  `APP_ISSUE_29_BRINGUP_PROBE=1` auf den `fermentation_app`-Component;
- `CONFIG_APP_PROFILE_ESP32_RELEASE` setzt diese Definition nicht;
- `main/CMakeLists.txt` setzt dieselbe Definition und registriert nur für
  `esp32_bringup` die private Quelldatei
  `main/issue_29_bringup_probe.cpp` sowie den privaten Include-Pfad
  `../lib/fermentation_app/private`;
- die konkrete interne Grenze besteht aus
  `lib/fermentation_app/private/issue_29_bringup_fault_seam.hpp` und einer
  ausschließlich unter `APP_ISSUE_29_BRINGUP_PROBE` kompilierten, lokal
  begrenzten Seam-Sektion in
  `lib/fermentation_app/src/run_persistence_coordinator.cpp`, also genau an
  der bestehenden Kandidaten-/Apply-Grenze;
- `main/app_main.cpp` bleibt Composition Root und startet/überwacht den
  transienten Task über dessen private Bring-up-Schnittstelle; die
  ESP-IDF-Ressourcenmessung und der große Probeaufruf liegen in
  `main/issue_29_bringup_probe.cpp`. Der private Header wird nur über den
  vorgenannten Bring-up-Include-Pfad eingebunden;
- der bestehende öffentliche Fachvertrag wird nicht erweitert und die
  private Headergruppe wird nicht als Component-Include-Verzeichnis
  installiert;
- `fermentation_app` verwendet dafür keine ESP-IDF-Header oder ESP-IDF-API.

Der Seam wird in der gemeinsamen Kandidaten-/Apply-Implementierung mit
`#if defined(APP_ISSUE_29_BRINGUP_PROBE)` vollständig aus dem Releasepfad
ausgeschlossen. Native PlatformIO-Builds erhalten weder die Kconfig-Variable
noch die Definition und kompilieren den Probe-/Fault-Code nicht als aktiven
Pfad.

Die Isolation wird nachgewiesen durch:

- Prüfung der `esp32_bringup`- und `esp32_release`-Compile-Commands: die
  Definition und die Diagnose-Taskquelle dürfen nur im Bring-up-Component
  vorkommen;
- Prüfung, dass native Builds die Definition nicht verwenden;
- Prüfung, dass der Release-Compile-/Symbolbestand keinen nutzbaren
  `issue_29`-Probe-/Fault-Seam enthält;
- `python scripts/check_architecture_boundaries.py` ohne neue
  `fermentation_app`-zu-ESP-IDF-Abhängigkeit;
- gezielte Kconfig-/CMake-Gegenprüfung, dass widersprüchliche Profilwahl
  weiterhin abbricht.

Eine globale `operator new`-Überschreibung, ein öffentlicher
Produktions-Allocator und eine allgemeine Fault-Injection-Plattform bleiben
ausgeschlossen.

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

## 10. Umsetzungs- und Commit-Schnitte nach Ownerfreigabe

Die Umsetzung erfolgt in wenigen klaren Schnitten. Jeder Schnitt erhält einen
eigenen Commit oder eine klar dokumentierte, nicht weiter teilbare
Nachweisgrenze. Es werden keine künstlichen Mini-Commits erzeugt. Jeder
Schnitt startet erst nach Freigabe der exakten Plan-SHA.

### Schnitt 1 – Sichere Diagnostik und Profil-/Buildintegration

Betroffene Dateigruppen:

- `main/app_main.cpp` und der bestehende ESP-IDF-Composition-Root;
- `main/CMakeLists.txt` sowie die vorhandenen Profil-/Kconfig-Dateien;
- ausschließlich bereits vorhandene ResetCause-, Zeit- und
  Diagnosegrenzen.

Zweck:

- sichere Boot-/Resetdiagnostik;
- Profil-, Flash-, PSRAM- und Partition-Artefakt-Erfassung vorbereiten;
- genau zwei reguläre Smoke-Ressourcenpunkte mit den vereinbarten Werten
  vorbereiten;
- keine Aktor-, UI-, NVS- oder neue Plattformintegration.

Direkte Nachweise:

- `git diff --check`;
- gezielte CMake-/Kconfig-Prüfungen für genau ein Profil;
- native und ESP-IDF-Compile-Prüfung des unveränderten aktorfreien Pfades;
- Architektur- und Secret-Gate für die geänderten Dateien.

Der Schnitt ist abgeschlossen, wenn beide Profile getrennt konfigurierbar
sind, die Diagnose ausschließlich fail-closed und aktorfrei bleibt und noch
keine `CommandDecision`-Fault-Instrumentierung enthalten ist.

### Schnitt 2 – `CommandDecision`-Ressourcenprobe und Fault-Seam

Betroffene Dateigruppen:

- `lib/fermentation_app/CMakeLists.txt`;
- `lib/fermentation_app/private/issue_29_bringup_fault_seam.hpp` und die
  guarded Seam-Sektion in
  `lib/fermentation_app/src/run_persistence_coordinator.cpp`;
- `main/app_main.cpp` für den Composition-Root-Start, die Überwachung und den
  privaten Start-/Monitor-Vertrag;
- `main/issue_29_bringup_probe.hpp`/`.cpp` für den transienten Diagnose-Task
  und die reale ESP-IDF-Ressourcenprobe;
- die private Start-/Monitor-Schnittstelle des Bring-up-Tasks;
- die bestehende Kandidaten-/Apply-Grenze in
  `run_persistence_coordinator.cpp` beziehungsweise dem tatsächlich
  zuständigen bestehenden Modul;
- der bring-up-only Aufruf im Composition Root;
- direkt betroffene native Command-/Apply-/Persistenztests.

Zweck:

- maximalen 48/96/1024-Kandidaten real halten und vermessen;
- Ressourcenprobe und Fehlervertragsprobe getrennt halten;
- den einmaligen `esp32_bringup`-only Fault-Seam compile-time isolieren;
- den bestehenden Command-/Apply-/Persistenzvertrag mit lokalem Storedouble
  und All-off-Sinks beobachten;
- keine allgemeine Allocator- oder Store-Fault-Plattform erzeugen.

Direkte Nachweise:

- native Regressionen für Decision, lokale Apply-Atomizität und unveränderten
  Zustand;
- Compile-Command-/Symbolprüfung für Bring-up-only versus Release/native;
- Herleitungs- und Lifecycle-Nachweis für Task-Stackgröße, Task-Stack-HWM,
  Heapkosten sowie Task-Erzeugung und -Freigabe;
- Architekturgrenzen;
- gezielte ESP-IDF-Buildprüfung beider Profile.

Der Schnitt ist abgeschlossen, wenn der On-Target-Probeaufruf den bestehenden
Pfad trifft, der injizierte Fehler keine RAM-/Persistenz-/Aktorfreigabe erzeugt
und Release/native keinen nutzbaren Seam enthalten.

### Schnitt 3 – Gezielte lokale und ESP-IDF-Nachweise

Betroffene Dateigruppen:

- direkt betroffene `test/`-Dateien;
- Build-/Analysekonfiguration nur, soweit sie durch Schnitt 1/2 erforderlich
  ist;
- keine neuen Fachverträge.

Zweck:

- gezielte native Command-/Apply-/Persistenzregressionen;
- Architektur-, Secret-, Format- und Diff-Prüfungen;
- `esp32_bringup`- und `esp32_release`-Builds sowie Static Analysis;
- Build-/Ressourcenreport für den exakten Implementierungs-HEAD vorbereiten.

Direkte Nachweise:

- dokumentierte gezielte Native-Tests;
- `python scripts/check_architecture_boundaries.py`;
- `python scripts/check_secrets.py`;
- `clang-format --dry-run --Werror` für geänderte C/C++-Dateien;
- `git diff --check`;
- `python scripts/build_esp_idf_profiles.py all`;
- `python scripts/run_esp_idf_static_analysis.py all`;
- `python scripts/build_report.py --append --esp-idf-profiles bringup release`
  gemäß CI-Dokument.

Der Schnitt ist abgeschlossen, wenn alle direkt betroffenen lokalen und
profilbezogenen Nachweise mit `PASS`, `FAILED` oder reproduzierbarem
`BLOCKED` dokumentiert sind. Nicht ausgeführte Hardwaregates bleiben
`NOT_RUN`.

### Schnitt 4 – Reale Board-, Flash-, UART-, Recovery- und Smoke-Nachweise

Betroffene Dateigruppen/Artefakte:

- kein weiterer Firmwarecode;
- finaler Implementierungs-HEAD aus Schnitt 3;
- ESP-IDF-Buildartefakte, UART-Logs und Messprotokoll.

Zweck:

- reale Boardrevision, Flashgröße, PSRAM, Partition und Boot-/Resetursachen
  erfassen;
- ROM-Bootloader-/FT232RL-Recovery ausführen;
- beide Profile mindestens 35 Sekunden auf demselben unbelasteten Board
  prüfen;
- Ressourcenprobe und Fehlervertragsprobe auf dem realen ESP32 ausführen;
- sichere, unbelastete Messpunkte prüfen.

Nicht-Ziele sind weiterhin externe 12-V-Versorgung, Verbraucher, belastete
MOSFET-Tests, reale Aktorfreigabe und Brownout-Injektion.

Direkte Nachweise:

- Messprotokoll mit Werkzeugversionen, Befehlen, Port, Board, SHA und Logs;
- Build-/Ressourcenreport;
- beide Smoke-Logs;
- Probe-/Fault-Seam-Log;
- Status je Kriterium als `PASS`, `FAILED`, `BLOCKED` oder `NOT_RUN`.

Der Schnitt ist abgeschlossen, wenn die Nachweise auf dem exakten finalen
Implementierungs-HEAD reproduzierbar erfasst oder mit konkretem Blocker
dokumentiert sind. Hardwaretests gelten nicht als durch Buildgrün ersetzt.

### Schnitt 5 – Versionierte Ergebnisdokumentation und Rückführung

Betroffene Dokumente/Artefakte:

- `docs/ISSUE_29_MEASUREMENTS.md` als versioniertes #29-Messprotokoll;
- `docs/HARDWARE.md` für tatsächlich bestätigte Hardwarefakten;
- `docs/OPEN_POINTS.md` nur für tatsächlich nachgewiesene Punkte;
- referenzierte Build-/Ressourcenreports und UART-/Probe-Artefakte.

Das Messprotokoll enthält mindestens Board/Revision, Flash/PSRAM,
Partitionstabelle, Build-/Source-SHA, Toolchain, UART-/Recoveryweg,
Resetursachen, beide Smokezeiten, Ressourcenwerte, Probe-/Fault-Ergebnis,
sichere Messpunkte, offene Punkte und die direkten Artefaktverweise.

Rückführung:

- `docs/HARDWARE.md` erhält nur mit `confirmed_test` belegte Board-, Flash-,
  PSRAM-, Boot- und unbelastete Pegelfakten und verweist auf das Protokoll;
- `docs/OPEN_POINTS.md` wird nur bei vorhandenem konkretem Link auf
  Messprotokoll, PR, Hardwarebericht oder Buildartefakt abgehakt;
- nicht gemessene GPIO-, Verbraucher-, belastete MOSFET- und
  Commissioning-Punkte bleiben unverändert `TBD_HARDWARE` bzw.
  `TBD_COMMISSIONING`;
- `TBD_IMPLEMENTATION_BUDGET` bleibt für nicht durch #29 belegte
  Produktions-, Parallel- und Lastbudgets bestehen;
- `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md` wird nur geändert, wenn ein
  gemessener Wert eine dortige kanonische Regel oder Aussage tatsächlich
  ändert. Rohmesswerte werden dort nicht doppelt gepflegt; das Protokoll ist
  die Quelle der #29-Messwerte.

Der Schnitt ist abgeschlossen, wenn alle behaupteten Fakten auf konkrete
Artefakte verweisen und offene spätere #37-/Konsumenten-/Lastgates sichtbar
bleiben.

## 11. Kanonische Ergebnis- und Messdokumentation

Der versionierte #29-Nachweis wird nach realer Durchführung unter
`docs/ISSUE_29_MEASUREMENTS.md` geführt. Der Plan-only-PR erzeugt dieses
Messprotokoll noch nicht, weil keine Messung stattfinden darf.

Der Bericht referenziert die zugehörigen Build-/Ressourcenreports, zum
Beispiel den versionierten oder als PR-/CI-Artefakt abgelegten Report mit
Source-/Implementierungs-HEAD, Profil und Toolchain. UART-Logs und
Probe-/Fault-Logs werden ebenfalls mit eindeutiger SHA-/HEAD-Zuordnung
referenziert.

Die Dokumentationsquelle ist dadurch eindeutig:

```text
reale Messung/Buildartefakt
  -> docs/ISSUE_29_MEASUREMENTS.md
  -> bestätigte Fakten in docs/HARDWARE.md
  -> nur belegte Abhakpunkte in docs/OPEN_POINTS.md
```

`docs/RESOURCE_BUDGET_AND_MAINTENANCE.md` bleibt unverändert, solange keine
kanonische Regel durch die Messung sachlich geändert werden muss. #29 liefert
den ersten realen Bring-up-Nachweis; spätere UI-/NVS-/Web-/Display- und
Parallelbelastung sowie das abschließende #37-Lastgate bleiben offen.

## 12. Dokumentation und Roadmap

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

Diese Planrevision ändert Reihenfolge, Status und nächstes Roadmap-Gate nicht;
die bereits vorhandene minimale #29-Zeile ist deshalb ausreichend und wird in
dieser Revision nicht erneut geändert.

## 13. Plan-only Commit, Draft-PR und Handover

Der kumulative PR-Scope umfasst ausschließlich:

- `docs/tasks/issue-29-implementation-plan.md`;
- `docs/ROADMAP.md`.

Der bisherige Zwei-Commit-Stand bestand aus dem ursprünglichen Plan mit der
minimalen Roadmap-Synchronisierung und der folgenden vollständigen
Planrevision, die ausschließlich die Plan-Datei änderte. Der nun folgende
F6/F7-Korrekturcommit ändert wiederum ausschließlich
`docs/tasks/issue-29-implementation-plan.md`; eine weitere Roadmapänderung ist
nicht erforderlich. Damit bleibt der kumulative PR-Diff dokumentarisch und
enthält keine Firmware-, Buildprofil-, Flash- oder Hardwareänderung.

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
