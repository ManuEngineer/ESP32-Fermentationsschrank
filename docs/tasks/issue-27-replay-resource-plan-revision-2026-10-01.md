# Issue #27 / PR #170 – Replay-RAM-Planrevision nach Resource-Gate-Fail

```text
ISSUE=27
PR=170
PLAN_REVISION=REPLAY_RESOURCE_2026-10-01
CONTEXT_BASELINE_BRANCH=agent/issue-27-web-api-auth-main-restart
CONTEXT_BASELINE_SHA=1a54ae2b6d51e9e6cf2022429f3eb831ff49868d
CONTEXT_HEAD_SHA=1a54ae2b6d51e9e6cf2022429f3eb831ff49868d
CONTEXT_PLAN_SHA=46e0ea470b307a34867e24337b63a9d38166d771
CONTEXT_REFRESH_MODE=FULL
BASE_MAIN=7e3948652453ae97eede07956af466ef6dddb602
PR_STATE=OPEN_DRAFT
SLICE4C=PASS
INDEPENDENT_SLICE4C_FIX_VERIFICATION=PASS
FOUR_SESSION_NO_PSRAM_RESOURCE_GATE=FAIL
OPEN_SLICE_BLOCKERS=0
OPEN_RESOURCE_BLOCKERS=1
PRODUCTIVE_RUN_MUTATION=NO
ACTUATOR_RELEASE=NO
PRODUCT_CODE_CHANGE_THIS_STEP=NO
OWNER_DECISION_REQUIRED=YES
NEXT=STOP_FOR_INDEPENDENT_PLAN_REVIEW
```

Diese vollständige Planrevision ist die einzige kanonische Arbeitsgrundlage
für eine spätere Replay-Ressourcenkorrektur. Sie ersetzt keine Ownerfreigabe
und autorisiert noch keinen Produktcode, keinen Test-Harness, keinen Flash und
keinen neuen Hardwarelauf.

## 1. Ziel und Nicht-Ziele

Ziel ist ein revidierter, bounded Replay-/Session-RAM-Vertrag für die reale
ESP32-WROOM-32E-/ESP32-D0WD-V3-Basis ohne vorausgesetztes PSRAM. Der Vertrag
muss die bereits festgelegten Auth-/Session-Semantiken erhalten:

- höchstens vier unabhängige Sessions;
- keine Verdrängung einer aktiven Session durch eine fünfte Anmeldung;
- höchstens eine In-Flight-Mutation pro Session;
- deterministische Behandlung identischer Retrys;
- kein zweites Anwenden eines bereits angewendeten Commands;
- fail-closed bei ungültiger Sequenz, Replay-Konflikt, Kapazitätsfehler und
  Ressourcenfehler;
- `ReplayExpired`/Konflikt bleibt für Retrys außerhalb des garantierten Fensters
  eine zulässige und dokumentierte Antwort.

Nicht Bestandteil dieser Revision sind ein neuer Auth-Vertrag, eine neue
Mutation, eine Änderung der vier Sessionplätze, PSRAM, ein kleinerer
Main-Task-Stack, ein zweiter HTTP-Dispatcher, eine Textpack-Optimierung oder
eine Aktivierung der produktiven Run-Mutationsroute.

## 2. Verifizierte Ausgangslage

Der Live-PR steht auf `1a54ae2b6d51e9e6cf2022429f3eb831ff49868d`, ist offen und
Draft. Die reale #170-Evidence auf diesem exakten Artefakt lautet:

```text
BOARD=ESP32-D0WD-V3 revision v3.1 / ESP32-WROOM-32E board family
FLASH_SIZE=4MB
PSRAM_PRESENT_PHYSICALLY=NO
PSRAM_USED_BY_BUILD=NO
ESP_IDF=v6.1
APP_REQUIRE_PSRAM=0
CONFIG_LV_USE_QRCODE=1
CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576
FLASH=PASS
APPLICATION_READY=PASS
BASELINE_STABLE=NOT_REACHED
FAILURE=abort()
FAULT_PATH=operator new -> makeFermentationUiTextPacks()
FAULT_SITE=main/app_main.cpp:582
RESET_AFTER_ABORT=SW_CPU_RESET
```

`FermentationApplication::initializeNetwork()` reserviert den
`WebSessionManager` aktuell mit `new (std::nothrow)` vollständig beim Aufbau
des Webpfads (`lib/fermentation_app/src/fermentation_application.cpp:714-718`).
Die produktive UI/Textpack-Erzeugung folgt später in `app_main()`
(`main/app_main.cpp:582`). Der beobachtete Textpack-Allocator ist damit der
erste sichtbare Folgefehler nach der großen Eager-Reservation; diese Revision
behauptet nicht, dass `fermentation_ui_text.cpp` selbst defekt ist.

Die aktuelle Replay-Struktur und die Rohpayload-Formel stehen in
`lib/fermentation_app/src/web_session.hpp:16-31,148-199`; der bestehende
Grenz- und Replay-Test steht in `test/test_web_session/test_main.cpp:415-488`.
Die produktive Mutationsroute reserviert vor `prepareEnvelope()`, führt
`prepare -> confirm -> apply` erst nach erfolgreicher Reservation aus und
speichert den Outcome danach (`lib/fermentation_app/src/web_application_routes.cpp`).
Diese Reihenfolge bleibt verbindlich.

### Quellenabgleich der historischen Hardwarebasis

Der Auftrag nennt für eine frühere #171-Strecke ohne #170-Session-Manager:

```text
FREE_HEAP_OBSERVED=9320..11940 B
MINIMUM_FREE_HEAP_WATERMARK=1552..5084 B
LARGEST_FREE_BLOCK=2688..7680 B
```

Die aktuell kanonische, versionierte Issue-164-Evidence dokumentiert dagegen
für ihren eigenen Reviewed HEAD `a245e686...` `10060/5024/7424` am Startup,
`10060/4976/7424` am stabilen SetupAccessPoint und `10228/4632/7424` nach 30 s
(`docs/tasks/issue-164-hardware-verification-evidence-2026-10-01.md:79-91`).
Die zusammengeführte PR-171-Evidence enthält zusätzlich abweichende
historische Messpunkte. Das ist als `SOURCE_OF_TRUTH_CONFLICT` sichtbar zu
halten; keiner dieser Werte wird in dieser Revision zu einem harten Heap-
Schwellwert erhoben. Vor der späteren Hardwarefreigabe muss der Owner den
anzuwendenden Rohmitschnitt/Reviewed HEAD festlegen.

## 3. ESP32-Target-Speicherbilanz

Die Werte wurden nicht mit Host-`sizeof` bestimmt. Verwendet wurde die
ESP-IDF-6.1-Release-Compile-Commandline des Target-Builds für
`lib/fermentation_app/src/web_session.cpp`, mit dem XTensa-ESP32-C++17-
Compiler und den Release-Headern. Ein temporärer stdin-Layout-Probe legte
absichtlich `static_assert(sizeof(T) == 0)` an; die Compilerdiagnostik gab die
Targetwerte aus. Es wurde kein Produktfile und kein Buildprofil geändert.

```text
TARGET_SIZEOF_COMPLETED_MUTATION=848 B
TARGET_SIZEOF_OPTIONAL_COMPLETED_MUTATION=856 B
TARGET_SIZEOF_COMPLETED_ARRAY_8=6784 B
TARGET_SIZEOF_SESSION=7776 B
TARGET_SIZEOF_WEB_SESSION_MANAGER=31144 B
TARGET_SIZEOF_SERVICE_SESSION_LEASE=48 B
TARGET_SIZEOF_SERVICE_SESSION_POLICY=24 B
TARGET_SIZEOF_MUTEX=4 B
```

Die Speicherrechnung wird ausdrücklich in Rohpayload, Replay-Slots und
Gesamtobjekt getrennt:

```text
SESSION_BASE_STATE_BYTES =
    sizeof(Session)
    - sizeof(optional<CompletedMutation>)
    - sizeof(array<CompletedMutation, 8>)
  = 7776 - 856 - 6784
  = 136 B

IN_FLIGHT_REPLAY_BYTES = sizeof(optional<CompletedMutation>) = 856 B
COMPLETED_REPLAY_BYTES_PER_ENTRY = sizeof(CompletedMutation) = 848 B
COMPLETED_REPLAY_BYTES_PER_SESSION = 8 * 848 = 6784 B
TOTAL_4_SESSION_REPLAY_BYTES = 4 * (856 + 6784) = 30560 B

CURRENT_RAW_REPLAY_PAYLOAD = 29952 B
CURRENT_WEB_SESSION_MANAGER =
    4 * (136 + 856 + 6784) + manager_overhead
  = 31144 B
CURRENT_MANAGER_SIZE_BOUND = 32768 B
STATIC_MANAGER_MARGIN = 1624 B
```

`CURRENT_RAW_REPLAY_PAYLOAD=29952 B` ist nur die bisherige Nutzdatenformel
für Fingerprint- und Outcome-Arrays. `TOTAL_4_SESSION_REPLAY_BYTES=30560 B`
ist die relevante Target-Speicherbelegung der In-Flight-/Completed-Slots
einschließlich `std::optional`-Layout und Padding; `31144 B` ist das komplette
Target-Objekt einschließlich Session-Basiszustand, Mutex, Policy, Referenz
und Array. Eine `static_assert` gegen `32768 B` beweist deshalb keine reale
Gesamtsystemreserve.

## 4. Bestehender Vertrag, der erhalten oder ausdrücklich geändert werden muss

Der aktuelle Vertrag verwendet pro Replayeintrag:

- Sequenznummer;
- exakte Requestidentität als bis zu 512 Bytes aus Methode, Pfad,
  Body-Länge und exakten Bodybytes;
- HTTP-Status;
- Content-Type bis 64 Bytes;
- Outcome-Body bis 256 Bytes.

Der Content-Type der produktiven Mutationsergebnisse ist aktuell konstant
`application/json; charset=utf-8`. Der begrenzte Outcome-Satz ist in
`web_application_routes.cpp` geschlossen: `applied`, `unavailable`, `stale`,
`rejected`, `busy`, `confirmation-required`, `write-failed` und
`too-large`, jeweils mit dem bestehenden HTTP-Status und dem bestehenden
kanonischen JSON-Body. Eine spätere Implementierung darf nicht stillschweigend
beliebige neue Replay-Bodies als kompakt rekonstruierbar annehmen.

Bei jeder Repräsentation gilt:

1. `reserve` muss den Replayplatz beziehungsweise die erforderliche Capacity
   erfolgreich sichern, bevor irgendein `prepare`, `confirm` oder `apply`
   beginnt.
2. Ein Capacity-/Allokationsfehler liefert einen typisierten fail-closed
   Fehler; `std::bad_alloc`/`abort()` darf keine Ressourcenreaktion sein.
3. Nach einem erfolgreichen Apply muss der exakte für den Retry erforderliche
   Identitäts-/Outcome-Eintrag sicher speicherbar sein; sonst darf der Command
   nicht angewendet werden.
4. Identische gültige Retrys liefern denselben HTTP-Outcome, unterschiedliche
   Requestbytes dürfen nicht als derselbe Retry gelten.
5. Fensterablauf liefert weiter `ReplayExpired`/Conflict und wendet nichts an.

## 5. Kandidatenvergleich

### Kandidat A – aktuelles eager fixed layout

```text
CANDIDATE_A=FAIL_RESOURCE_BASIS
RAW_PAYLOAD=29952 B
TARGET_REPLAY_SLOTS=30560 B
TARGET_MANAGER=31144 B
BOOT_BEHAVIOR=full replay reservation before UI/textpack path
```

Das reale Abort-Ereignis auf no-PSRAM verwirft A als R1-Ressourcenbasis. Eine
andere Allokationsreihenfolge, ein Verschieben der Textpacks oder ein Ignorieren
von `new` ändert den Worst Case nicht und ist keine Korrektur.

### Kandidat B – lazy exact replay storage

Read-only Sessions würden keinen Replaypayload reservieren. Ein Replayplatz
entsteht erst bei einer Mutation; `reserveMutation()` müsste Capacity vor
`prepare -> confirm -> apply` atomar fail-closed sichern. Das erhält die
exakte Byteidentität ohne Hash-Kollisionsannahme.

```text
BOOT_REPLAY_ALLOCATION=0 B
STEADY_STATE_WORST_CASE_WITH_CURRENT_WINDOW=30560 B REPLAY SLOTS
CONTRACT_CHANGE_REQUIRED=NO_FOR_IDENTITY; YES_IF_CAPACITY_STATUS/API CHANGES
```

B allein ist nicht zulässig: Der erlaubte Vier-Session-/Acht-Outcome-
Worst-Case bleibt praktisch unverändert und würde den Ressourcenfehler nur in
die erste oder spätere Mutation verschieben. B ist nur zusammen mit einem
nachweislich tragfähigen Replayfenster oder einer kompakten Repräsentation
vertretbar.

### Kandidat C – kompakter Identitäts-/Outcome-Vertrag

Ein proportionaler Kandidat ist ein fester Digest der exakten Requestbytes
plus ein kleiner endlicher Outcome-Code. Der vorhandene feste Content-Type und
die acht bestehenden Outcome-Varianten werden rekonstruiert; ein 256-Byte-
Bodybuffer entfällt.

```text
ALGORITHM=SHA-256 via existing ESP-IDF mbedTLS primitive
DIGEST_BYTES=32
INPUT_EXACT_BYTES=method || NUL || path || NUL || body_length || NUL || body
COLLISION_MODEL=computational collision resistance, not mathematical identity
HOST_AND_ESP_IDF_IMPLEMENTATION_SOURCE=existing mbedTLS/ESP-IDF primitive;
  only a small project adapter and host-test path if required
SECURITY_CORRECTNESS_TRADEOFF=bounded RAM and deterministic reconstruction,
  but a digest collision would be a false replay identity; Owner approval is
  required before implementation
```

Für eine konkrete, noch nicht freigegebene K=8-Projektion ergibt sich bei
`uint64 sequence + 32-byte digest + uint8 outcomeCode`, 8-Byte-Alignment und
einem optionalen In-Flight-Slot:

```text
ESTIMATED_COMPACT_ENTRY=48 B
ESTIMATED_OPTIONAL_COMPACT_ENTRY=56 B
ESTIMATED_4_SESSION_REPLAY_SLOTS=4 * (56 + 8 * 48) = 1760 B
ESTIMATED_MANAGER_WITH_CURRENT_BASE_STATE=approximately 2344 B
```

Diese Zahlen sind Layoutprojektion, kein freigegebenes Heapbudget. Die
Implementierung müsste sie auf der echten Release-Target-Commandline erneut
als `sizeof`-Evidence bestimmen. C bewahrt den 4-Session-/8-Outcome-
Grundvertrag, benötigt aber eine materielle Ownerentscheidung zum
Collision-Modell. Ein eigener Crypto-/Replay-Framework oder eine neue
allgemeine Bibliothek ist ausgeschlossen.

### Kandidat D – Replayfenster revidieren

Mit unverändert exakter Requestidentität ergibt sich für ein eager fixed layout
folgende reine Target-Rechnung. `K` ist die Zahl abgeschlossener Outcomes pro
Session; der In-Flight-Slot bleibt erhalten:

| Variante | Replay-Slots für 4 Sessions | geschätzter Manager aus aktueller Basis | Retry-/UX-Wirkung |
|---|---:|---:|---|
| `K=1` | `4*(856+1*848)=6816 B` | ca. `7400 B` | Nur das letzte abgeschlossene Outcome pro Session replaybar; ältere Retrys `ReplayExpired`/Conflict |
| `K=2` | `10208 B` | ca. `10792 B` | Zwei Outcomes; ältere Multi-Tab-Retryfälle laufen aus dem Fenster |
| `K=4` | `16992 B` | ca. `17576 B` | Bessere Retry-UX, aber deutlich größere Fragment-/Reservewirkung |
| `K=8` | `30560 B` | `31144 B` | bisheriger Vertrag; reale Resource-Gate-Basis FAIL |

Eine lazy Variante von D macht den Bootbedarf null, ändert aber nicht die
jeweilige maximale belegte Replaymenge. D erhält die exakte Byteidentität und
vermeidet eine Hash-Kollisionsannahme, verändert aber bei `K<8` den
Retry-Vertrag und die Multi-Tab-Auswirkung. Kein `K` wird allein deshalb
ausgewählt, weil es wahrscheinlich bootet; die finale Variante benötigt
target-realistische Capacity-/Worst-Case-Evidence.

## 6. Empfehlung und Ownerentscheidung

```text
MEASURED_ROOT_CAUSE=eager fixed replay layout reserves 30560 B of actual
  no-PSRAM replay slots / 31144 B WebSessionManager before later UI allocation;
  makeFermentationUiTextPacks() is the first observed failing allocator
TARGET_SIZEOF_WEB_SESSION_MANAGER=31144 B on ESP32 release C++17 target
CURRENT_WORST_CASE_REPLAY_RAM=30560 B actual slots; 29952 B raw payload
CANDIDATE_A=FAIL_RESOURCE_BASIS
CANDIDATE_B=NOT_SUFFICIENT_ALONE
CANDIDATE_C=RECOMMENDED_CONDITIONAL_OWNER_APPROVAL
CANDIDATE_D=VIABLE_EXACT-IDENTITY_FALLBACK_REQUIRES_WINDOW_DECISION
RECOMMENDED_R1_REPLAY_STORAGE=Candidate C: fixed compact digest/outcome
  representation, retain four sessions and eight completed outcomes, no new
  generic replay framework; only after Owner accepts SHA-256 collision model
RECOMMENDED_MAX_RAM=1760 B provisional compact replay-slot projection for C
  plus target-measured session/base/manager overhead; no new heap floor
  threshold is introduced by this plan
CONTRACT_CHANGE_REQUIRED=YES
OWNER_DECISION_REQUIRED=YES
PRODUCT_CODE_CHANGE_THIS_STEP=NO
PRODUCTIVE_RUN_MUTATION=NO
FOUR_SESSION_NO_PSRAM_RESOURCE_GATE=FAIL
NEXT=STOP_FOR_INDEPENDENT_PLAN_REVIEW
```

C ist die Empfehlung, weil er den bestehenden Vier-Session-/Acht-Outcome-
Retryvertrag am ehesten erhält und den beobachteten Bootdruck proportional
reduziert. Die Empfehlung ist keine stille Ownerentscheidung: Akzeptiert der
Owner das Digest-Collision-Modell nicht, muss D mit einem ausdrücklich
freigegebenen `K` gewählt werden; B allein ist ausgeschlossen. Bis dahin
bleibt `FOUR_SESSION_NO_PSRAM_RESOURCE_GATE=FAIL`.

## 7. Umsetzung nach Ownerfreigabe

Die spätere Implementierung erfolgt erst nach Freigabe dieses exakten
Plan-Commits und der gewählten Variante:

1. `web_session.hpp/.cpp`: genau eine freigegebene Replay-Repräsentation und
   eine explizite, fail-closed Capacity-Reservation einführen. Öffentliche
   Semantik von Session-Lifecycle, CSRF, Lease, Sequence-Gap, Replay-Outcome,
   `ReplayExpired` und `SequenceReused` bleibt erhalten, soweit die
   Ownerentscheidung nichts anderes festlegt.
2. Bei C: Digest-Eingabe bytegenau definieren, Digest-Erzeugung über die
   vorhandene ESP-IDF-/mbedTLS-Primitive anbinden, den endlichen Outcome-Code
   zentral auf Status/Content-Type/Body abbilden und unbekannte Outcomes
   ablehnen. Keine zweite Hash-/JSON-/Replay-Abstraktion bauen.
3. Bei B/D: Speicher erst bei erfolgreicher Mutation-Reservation bzw. im
   freigegebenen Fenster belegen; Allokations-/Capacity-Fehler müssen vor
   Application-Prepare und -Apply als fail-closed HTTP-/Domain-Ergebnis
   zurückkehren. Kein `bad_alloc`-Abort.
4. Die Target-`sizeof`-Probe als reproduzierbare Evidence oder als kleine
   test-only Target-Evidence pflegen. Alle neuen Zahlen sind Layoutnachweise,
   keine frei erfundenen Heap-Grenzwerte.
5. Nur die direkt betroffenen Web-Session- und Route-Regressionen ändern;
   keine produktive Run-Mutationsroute registrieren und keinen Read-only-
   Pollinglauf als Replay-Nachweis ausgeben.

## 8. Tests und späteres Resource-Gate

### Native-/Target-Tests

- alle bestehenden Web-Session-/Route-Regressionen;
- vier Sessions aktiv, fünfte Anmeldung fail-closed, keine aktive Session
  verdrängt;
- identischer Request liefert deterministisch denselben gespeicherten
  Outcome; ein Byte Unterschied, eine andere Methode oder ein anderer Pfad
  wird nicht als Retry erkannt;
- In-Flight, Gap, Reuse, Expired, Fenstergrenze und maximale Sequenz bleiben
  fail-closed;
- jeder maximale Replay-/Outcome-Fall wird vor `prepare -> confirm -> apply`
  auf Capacity geprüft;
- bei C: bekannte endliche Outcome-Tabelle, Digest-Domain/Length-Framing und
  Host-/ESP32-Konsistenz;
- bei D: jede freigegebene `K`-Grenze einschließlich `ReplayExpired` nach
  Fensterablauf;
- Release-Target-`sizeof` für Entry, Session, Manager und die verwendete
  Worst-Case-Reservation; kein Hostwert als PASS.

### Hardware-/Gesamtsystem-Gate nach der Implementation

Erst nach Builder-Self-Check, Independent Review/Fix Verification und
Ownerfreigabe erfolgt auf dem exakten finalen Artefakt erneut:

- stabiler Boot, `BASELINE_STABLE`, `BASELINE_AFTER_30S`, Resetgrund,
  Watchdog-/Brownout-/Panic-Überwachung;
- vier unabhängige Login-Sessions und fail-closed fünfte Session;
- je Session `GET /api/v1/status`, `/api/v1/temperatures` und
  `/api/v1/alerts` alle fünf Sekunden, mindestens zehn Minuten;
- `LOAD_START`, `LOAD_1_MIN`, `LOAD_5_MIN`, `LOAD_10_MIN`,
  `POST_LOAD_1_MIN` mit freiem Heap, Minimum, größtem 8-bit-Block,
  Main-Task-HWM, aktiven Sessions, Erfolgen/Fehlern und 5xx;
- lokale Touch-/Display-Reaktionsfähigkeit, kohärentes Netzwerk,
  laufendes `application.update()` und kein semantisches Idle-Renewal durch
  Read-only-Polling;
- die gewählte Replay-Strategie selbst in ihrem maximal garantierten Zustand:
  bei C vier Sessions mit je acht abgeschlossenen Outcomes plus In-Flight-
  Reservation beziehungsweise die exakt freigegebene Variante; bei D exakt
  das neue `K`-Fenster;
- die Replay-Evidence darf keine produktive Run-Mutation aktivieren. Ein
  separater autorisierter Mutationstest bleibt ein eigenes Gate.

PASS ist nur zulässig, wenn nach der Speicherstrategieänderung kein
monotonischer Heap-/Largest-Block-Verlust, kein Abort/Reset/Watchdog/Panic,
keine UI-/Application-Blockade und keine Replay-/Capacity-Verletzung auftritt.
Nicht ausgeführte Teile bleiben `NOT_RUN`; sie werden nicht durch einen
Read-only-Teilpass ersetzt.

## 9. Status- und Handover-Grenze

Die Statusführung für diese Revision lautet sachlich:

```text
SLICE4C=PASS
INDEPENDENT_SLICE4C_FIX_VERIFICATION=PASS
FOUR_SESSION_NO_PSRAM_RESOURCE_GATE=FAIL
OPEN_SLICE_BLOCKERS=0
OPEN_RESOURCE_BLOCKERS=1
PRODUCTIVE_RUN_MUTATION=NO
ACTUATOR_RELEASE=NO
NEXT=STOP_FOR_INDEPENDENT_PLAN_REVIEW
```

Der bestehende kanonische `SESSION HANDOVER` wird nach dem Plan-Commit
aktualisiert; es wird kein neuer Handover-Kommentar angelegt. Danach hält der
Builder für Independent Plan Review und Ownerentscheidung an. Kein Ready,
Merge, Auto-Merge, Flash oder Produktcode-Fix in dieser Phase.
