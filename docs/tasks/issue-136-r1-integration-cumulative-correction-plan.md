# Issue #136 – kumulativen Integrationsbefund von PR #135 korrigierbar konsolidieren

## 0. Status, Ausfuehrungsgate und Nicht-Ziele

```text
PLAN_REVISION=FULL_STANDALONE_INITIAL_VERSION
THIS_ROUND=PLAN_ONLY
CORRECTION_IMPLEMENTATION_ALLOWED=NO
OWNER_MUST_APPROVE_EXACT_FINAL_PLAN_SHA=YES

ISSUE=136
BASE_BRANCH=integration/r1-development
BASE_SHA=8e4c52a07a488a41b59d98f6fb11742b0678f52a
PLAN_BRANCH=agent/issue-136-integration-correction-plan
PLAN_SHA=ASSIGNED_BY_THIS_PLAN_COMMIT

INTEGRATION_ISSUE=134
INTEGRATION_PR=135
PR135_STATE=OPEN_DRAFT
PR135_SOURCE_BRANCH=integration/r1-development
PR135_SOURCE_HEAD=8e4c52a07a488a41b59d98f6fb11742b0678f52a
PR135_TARGET_BRANCH=main
PR135_TARGET_HEAD=87dd593fcdc8d26831873a4163b174340b4347c0
PR135_AHEAD_BY=263
PR135_BEHIND_BY=0
CUMULATIVE_OWNER_REVIEW=CHANGES_REQUIRED
OWNER_READY_GATE=NO
PR135_READY=NO
MERGE=NO

PRODUCTION_CODE_CHANGE=NO_THIS_ROUND
TEST_CODE_CHANGE=NO_THIS_ROUND
BUILD_CONFIGURATION_CHANGE=NO_THIS_ROUND
DEPENDENCY_CHANGE=NO_THIS_ROUND
BOARD_GPIO_ASSIGNMENT_CHANGE=NO_THIS_ROUND
PULL_CONFIGURATION_CHANGE=NO_THIS_ROUND
NET_CONFIGURATION_CHANGE=NO_THIS_ROUND
DESIGN_RESISTOR_VALUE_CHANGE=NO_THIS_ROUND
ACTUATOR_RELEASE=NO
HARDWARE_TEST=NOT_RUN
NEW_HARDWARE_MEASUREMENT=NOT_RUN
ISSUE25_STARTED=NO
```

Dieser Plan ist die einzige vollstaendige, nach Freigabe ausfuehrbare Fassung
des Korrekturscopes. Nach einer Ownerentscheidung zu F5 wird **diese Datei
in-place** zu einer einzigen vollstaendigen Fassung konsolidiert und neu zur
exakten SHA-Freigabe vorgelegt. Es entsteht keine Amendment- oder Delta-Kette.

Der Branch entwickelt nicht direkt in PR #135. PR #135 bleibt ein reiner
Integrations-/Promotions-PR gegen `main`; weder Ready-Status noch Merge,
Auto-Merge oder Issue-Schliessung gehoeren zu diesem Scope.

Die einzigen Repositoryaenderungen dieser Planrunde sind diese Planfassung und
die erforderliche aktuelle Einordnung von #134/#135/#136 in `docs/ROADMAP.md`.

## 1. Revalidierte Live- und Provenienzbaseline

| Gegenstand | Revalidierter Befund | Rolle im Plan |
|---|---|---|
| PR #135 | Open Draft, `integration/r1-development @ 8e4c52a...` nach `main @ 87dd593...`, `263` ahead, `0` behind | Integrationsbaseline, kein Entwicklungsziel |
| Issue #134 | Open; beschreibt ausschließlich den Integrationscheckpoint | Statusquelle fuer #135 |
| Issue #124 / PR #125 | geschlossen/gemergt; freigegebener Plan `docs/tasks/issue-124-r1-power-loss-recovery-plan.md @ 6f4e1a54d521ba60de185f350d571cbefaa23d71` | verbindlicher R1-Current-`FERMENTING`-Recoveryvertrag |
| Issue #126 / PR #127 | geschlossen/gemergt; freigegebener Plan `docs/tasks/issue-126-absolute-time-rtc-ntp-plan.md @ 52bd69f37e7baac782ebd2fb927f3fa57003f1c7` | vorhandener app-neutraler RTC/NTP-/trusted-UTC-Vertrag |
| Issue #132 / PR #133 | geschlossen/gemergt in die Basis-SHA; Ownerfreigabe des Plans `ae893f20acd68aa96142ce36a9ce8a0ff117b23f` | Waiver- und Hardwarestatusvertrag; kein offener #29/#90-Owner |
| Issues #29, #90, #124, #126 | geschlossen | historische/digitale Evidenz, nicht als aktueller offener Owner ausgeben |
| Issues #30, #31, #32, #33 | offen | reale Hardware-/Adapter-Owner gemaess ihrem spezifischen Scope |
| Issue #25 | offen, nicht begonnen | bleibt erst nach erfolgreicher Korrektur und Promotion naechste fachliche Arbeit |

`docs/SPECIFICATION_REVIEW.md` bestimmt die Quellenprioritaet:
akzeptierte spaetere ADRs, das Specification Review, thematische Fachvertraege,
danach Requirements/Architecture/Hardware; klar historische Audit- und
Plandokumente sind keine gegenwartsbezogene Status-SSOT. ADR-013 bleibt
unveraendert: `device_platform` app-neutral, ESP-IDF-Adapter in
`device_platform_esp_idf`, Fach-/Recoverylogik in `fermentation_app`.

## 2. Aktueller Zielvertrag, den die Umsetzung nur konsistent macht

### 2.1 Interlock, Boot und Aktorfreigabe

```text
R1_INTERLOCK=ActuationInterlock
INTERLOCK_STATELESS=YES
INTERLOCK_OWNS_ACTIVE_FAULT_STATE=NO
INTERLOCK_OWNS_ACKNOWLEDGED_FAULT_STATE=NO
INTERLOCK_OWNS_FAULT_HISTORY=NO
FAULT_CODES=FINITE_TYPED_SET
ACKNOWLEDGEMENT_HAS_SAFETY_EFFECT=NO
WATCHDOG_LATCH_OWNER=ActuatorPlanner
ACTUATION_GATE_DEFAULT=UNRESOLVED
ACTUATION_PERMISSION_INPUT=FRESH_ACTUATION_EVIDENCE

BOOT_LOAD_CLASSIFICATION=
  configuration trust -> persistence load/integrity -> run classification
  -> optional Current-FERMENTING time evaluation

ACTUATOR_PERMISSION=
  fresh configuration evidence -> fresh persistence evidence
  -> fresh sensor evidence -> planner evidence
  -> explicit activation path where applicable
  -> current ActuationInterlock evaluation
  -> owning hardware/adapter gates where applicable
```

Codeevidenz: `actuation_interlock.hpp/.cpp` definiert eine statische
`evaluate(const ActuationEvidence&)`, einen endlichen `FaultCode`-Satz und
liest den Watchdog-Latch aus `ActuatorPlanner`; der Interlock hat keinen
speichernden Fault-/Ack-/Historienmember. `ActuationInterlock::evaluate()`
liefert ohne ausreichende aktuelle Evidenz weiterhin `Unresolved`.

### 2.2 Recovery

```text
TRUSTED_CURRENT_FERMENTING_LOGICAL_RECOVERY=AUTOMATIC
POWER_LOSS_ALONE_REQUIRES_USER_CONFIRMATION=NO
ACTUATOR_RELEASE_FROM_RECOVERY_ALONE=NO

CURRENT_FERMENTING_WITH_EXACT_VALID_RECORD_AND_TRUSTED_UTC=
  automatic logical recovery
CURRENT_FERMENTING_WITHOUT_TRUSTED_CURRENT_UTC=
  RecoveryEvaluation / WaitingForTrustedTime / no waiting write / all-off
OLDER_VALID_CHECKPOINT_FALLBACK=
  no automatic promotion / no automatic activation
PREHEATING_COOLING_MANUAL_HOLDING=
  existing explicit ResumeOffer semantics
NON_RESUMABLE_TRUSTED_PHASES=
  existing NoActiveRun/discard semantics
```

Die Ausfallwandzeit wird nur fuer den exakt validierten Current-
`FERMENTING`-Pfad mit `priorBootPhaseElapsed` plus trusted UTC verarbeitet.
`runProgress.observedRunSeconds` bleibt beobachtete Laufzeit. Missing UTC
mutiert keine Persistenz. Dies ist keine Aktorfreigabe und reaktiviert weder
Charge-Recovery noch automatische Fallback-Promotion.

### 2.3 Hardware-Nachweisarten

```text
ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED
MULTIMETER_REQUIRED_FOR_R1_ACCEPTANCE=NO
BOOT_LEVEL_MEASUREMENT_REQUIRED=NO
GPIO_VOLTAGE_MEASUREMENT_REQUIRED=NO

SSOT_CONFORMANCE=<scope-specific>
FUNCTIONAL_HARDWARE_VERIFICATION=<scope-specific>
ADAPTER_SAFETY_VERIFICATION=<only where applicable, especially Issue #33>
THERMAL_COMMISSIONING=<where applicable>

NOT_MEASURED!=PASS
WAIVED_MEASUREMENT!=FUNCTIONAL_PASS
```

Die Waiver-Entscheidung entfernt keine funktionale fail-closed
Boot-/Resetforderung. Sie entfernt nur die generelle R1-Pflicht einer
elektrischen Pegel-/Spannungs-/Multimetermessung.

## 3. Source-of-truth- und Current-vs-Historical-Matrix

| Klasse | Quellen / Trefferrolle | Geplante Behandlung |
|---|---|---|
| `CURRENT_NORMATIVE_MUST_FIX` | `ARCHITECTURE.md`, `SAFETY_AND_FAULTS.md`, `RUNTIME_BEHAVIOR.md`, `IMPLEMENTATION_ISSUES.md`, `ACTUATOR_TIMING.md`, `RUN_PERSISTENCE.md`, `DIAGNOSTICS_AND_MAINTENANCE.md`, `IMPLEMENTATION_PLAN.md`, `REQUIREMENTS.md`, `ACCEPTANCE_TESTS.md`, `STATE_MACHINE.md`, `SYSTEM_SAFETY_AND_RECOVERY.md`, `HARDWARE.md` | Auf den Zielvertrag aus Abschnitt 2 und die nach F5 entschiedene Produktpolicy synchronisieren; keine neue Architektur erfinden. |
| `CURRENT_STATUS_MUST_FIX` | `ROADMAP.md`, `OPEN_POINTS.md` und offene Live-Issue-Bodies, soweit sie #29/#90/#124/#126 weiter als offene Owner oder GPIO-Design als unentschieden ausgeben | Nur aktuelle Status-/Ownershipfelder korrigieren; offene echte Hardware-/Commissioning-Gates bleiben `PENDING`/`NOT_RUN`. |
| `HISTORICAL_SUPERSEDED_KEEP` | abgeschlossene Planhistorie #24/#90/#121/#124/#126/#132, historische PR-/Build-/Messberichte #29/#90 | Nicht umschreiben. Jede damals zutreffende `SafetyCore`-, Pegelmess- oder offene-NVS-Aussage bleibt historische Evidenz. |
| `LEGACY_COMPATIBILITY_KEEP` | explizit als C2/#18-Legacy markierte Teile von `RUN_PERSISTENCE.md`, `RECOVERY_AND_INTERRUPTION.md`, `STATE_MACHINE.md` | Lesbarkeit/Codec-Kompatibilitaet erhalten; aktive R1-Abschnitte klar abgrenzen. |
| `NON_NORMATIVE_NEEDS_EXPLICIT_HISTORICAL_MARKER` | `docs/audits/RELEASE_1_FUNCTION_MATRIX.md` | Strategie A: Audit als Snapshot auf seiner dokumentierten Originalbasis einfrieren, sichtbar `NON_CURRENT_STATUS`, mit Datum/Basis-SHA und Verweis auf aktuelle SSOTs; keine Zeile nachtraeglich auf Interlock umschreiben. |
| `UNRELATED` | Treffer in Beispieltexten, Drittanbieter-/UI-/Netzwerkdokumenten ohne aktuellen Safety-, Recovery-, Mess- oder Statusvertrag | Im Implementation-Review als nicht betroffen protokollieren; nicht massenhaft umformulieren. |

### 3.1 Befundkategorien (nicht vermischen)

| Finding | primaere Kategorie | historische Gegenklasse / Bedeutung |
|---|---|---|
| F1 | `CURRENT_DOCUMENTATION_DRIFT` | `HISTORICAL_PROVENANCE_OK` fuer #24/#121-Plaene und klar historische Audits. |
| F2 | `CROSS_PR_CONTRACT_DRIFT` zwischen den gemergten #124-/#126-Vertraegen und noch aktiven Requirements/Acceptance-Texten | #18/C2-Legacy bleibt `HISTORICAL_PROVENANCE_OK`, wenn eindeutig markiert. |
| F3 | `CURRENT_DOCUMENTATION_DRIFT` | kein Architekturdefekt im aktuellen Produktcode. |
| F4 | `CURRENT_DOCUMENTATION_DRIFT` | historische #29-Messprovenienz bleibt `HISTORICAL_PROVENANCE_OK`. |
| F5 | `OWNER_DECISION_OPEN` | kein automatischer Dokument- oder Produktprofilentscheid. |
| F6 | `RUNTIME_IMPLEMENTATION_DEFECT` an bestätigten expliziten Factory-/Bootgrenzen | bereits `nothrow`-gesicherte Grenzen sind `HISTORICAL_PROVENANCE_OK` beziehungsweise aktuell ausgerichtet, nicht umzubauen. |
| F7 | `STATUS_METADATA_DRIFT` | geschlossene #29/#90/#124/#126-Evidenz bleibt historisch korrekt. |
| F8 | `CURRENT_DOCUMENTATION_DRIFT` in der Lesbarkeit eines ansonsten `HISTORICAL_PROVENANCE_OK`-Snapshots | Marker statt zeitlich gemischter Modernisierung. |

## 4. Vollstaendige Such- und Klassifikationsmatrix

Die Umsetzung wiederholt jede Suche auf dem dann freigegebenen
Implementierungs-HEAD, schreibt die konkrete Trefferliste in den
Implementierungs-Handover und klassifiziert jeden Treffer nach Abschnitt 3.
Ein `0` ist nur zulaessig fuer aktive normative Treffer, niemals als
Repository-pauschalaussage.

| Suchfamilie | Bereits revalidierte relevante Treffer | Klassifikation und Ziel |
|---|---|---|
| `SafetyCore`, `SafetyDisposition`, `CUSTOM_SAFETY_CORE`, `Release-1-Safety-Core` | aktuelle Architektur-, Safety-, Runtime-, Timing-, Persistenz-, Requirements-, Acceptance-, Roadmap- und Issue-Dokumente; historische #24/#121-Plaene und Function Matrix | aktuelle Normativ-/Statussprache auf `ActuationInterlock`; historische Plaene behalten; Function Matrix markieren/einfrieren. |
| `automatic resume`, `kein automatischer Resume`, `NoActiveRun`, `ResumeOffer`, `CurrentRunRecoverable`, `WaitingForTrustedTime`, `FERMENTING`, `OLDER_VALID_CHECKPOINT_RESUME` | aktuelle `REQUIREMENTS.md`/`ACCEPTANCE_TESTS.md` widersprechen #124; `RUN_PERSISTENCE.md`, `STATE_MACHINE.md`, `SYSTEM_SAFETY_AND_RECOVERY.md`, `RECOVERY_AND_INTERRUPTION.md` enthalten den aktuellen Vertrag und Legacygrenzen | Requirements/Acceptance korrigieren; aktuelle korrekte Fachvertraege behalten; Legacy als solche lassen. |
| `multimeter`, `Pegelmess`, `Bootpegel`, `GPIO voltage`, `electrical verification`, `electrical level` | aktive falsche Messpflicht in `SYSTEM_SAFETY_AND_RECOVERY.md`; aktuelle Waiverquellen in `HARDWARE.md`, `ACCEPTANCE_TESTS.md`, `DECISIONS.md`, #132-Plan; historische #29-Berichte | aktive Pflicht entfernen; Waiver nicht als PASS ausgeben; historische Messprovenienz nicht umschreiben. |
| `RTC_HARDWARE_OPTIONAL`, `NTP_ONLY_MODE_SUPPORTED`, `NEW_PRODUCTIVE_RUN_START_REQUIRES_TRUSTED_UTC`, `offline`, `without WLAN`, `ohne WLAN`, `ohne Internet` | `REQUIREMENTS.md`, `SPECIFICATION_REVIEW.md` und `PRODUCT_VISION.md` verlangen netzunabhaengigen Normal-/Bedienbetrieb; `ARCHITECTURE.md`/`HARDWARE.md` erlauben RTC-less NTP-only, verlangen aber trusted UTC vor neuem Lauf | Produktkonflikt F5, kein stiller Fix. Option A/B nur nach Ownerentscheidung in aktuelle Quellen einarbeiten. |
| `new (`, `make_unique`, `nothrow` | F6-Grenzen in RTC/NVS/`app_main`; bestehend korrekte `nothrow`-Fabriken in `fermentation_app`; weitere `ConfigurationService`-/Graph-Snapshot-Allocationen | nur Factory-/Bootgrenzen im Integrationsdiff nach Callgraph und Fehlervertrag klassifizieren; keine globale Heap-Policy. |
| `#29`, `#90`, `#124`, `#126`, `#134`, `#135`, `ISSUE25_STARTED` | Roadmap, Open Points, historische Reports/Plans, Live-Issues/PRs | Closed evidence nicht als offene Ownership; #134/#135 und #136 als aktuelle Reihenfolge; #25 `NOT_STARTED`. |

## 5. Revalidierte Findings und minimaler Korrekturschnitt

### F1 – SafetyCore-Dokumentdrift

```text
FINDING_F1_STATUS=CONFIRMED
FINDING_F1_EVIDENCE=
  code: lib/fermentation_app/src/actuation_interlock.hpp/.cpp
  stale-current: docs/ARCHITECTURE.md:27-39,
    SAFETY_AND_FAULTS.md:14-23, RUNTIME_BEHAVIOR.md:13-19,
    IMPLEMENTATION_ISSUES.md:87-103, ACTUATOR_TIMING.md:17-28,
    RUN_PERSISTENCE.md:67-93, ACCEPTANCE_TESTS.md:66-75,126-136,319-342,
    REQUIREMENTS.md:86-136, ROADMAP.md:37-42,167-182
  historical: docs/tasks/issue-24-safety-core-replan.md,
    docs/tasks/issue-121-lifecycle-safety-simplification-plan.md,
    docs/audits/RELEASE_1_FUNCTION_MATRIX.md
FINDING_F1_REQUIRED_CHANGE=
  current normative and current status documentation only; preserve historical
  plan/audit evidence according to the classification matrix
```

`SafetyDisposition` ist keine aktuelle Produkt-API. Interlock-eigene aktive
oder acknowledged Fault-Masken, Fault-Historie und ein acknowledgement-induced
Safety-Effekt werden aus aktuellen Texten entfernt. `FaultCode` bleibt ein
endlicher typisierter Satz; die Watchdog-Verriegelung bleibt beim Planner.

### F2 – #124-Recoverydrift in Requirements und Acceptance

```text
FINDING_F2_STATUS=CONFIRMED
FINDING_F2_EVIDENCE=
  authoritative-plan: issue-124 plan @ 6f4e1a54d521ba60de185f350d571cbefaa23d71
  code: FermentationApplication::evaluateCurrentRecovery(),
    reevaluateWaitingForTrustedTime(),
    RunPersistenceCoordinator::evaluateCurrentFermentingRecovery()
  stale-current: REQUIREMENTS.md:112-134;
    ACCEPTANCE_TESTS.md:27-41,74,126-131,168-186,334-336
  aligned-current: STATE_MACHINE.md:26-34,76-102;
    RUN_PERSISTENCE.md:25-47,75-99;
    SYSTEM_SAFETY_AND_RECOVERY.md:424-455
FINDING_F2_REQUIRED_CHANGE=
  replace blanket no-resume/no-active-run and SafetyCore test chains with the
  exact #124 matrix; retain explicit ResumeOffer and non-promoting fallback
```

### F3 – Bootklassifikation versus Aktorfreigabe

```text
FINDING_F3_STATUS=CONFIRMED
FINDING_F3_EVIDENCE=
  stale-current: STATE_MACHINE.md:144-156 places sensor/hardware checks before
  stored-run classification
  code: FermentationApplication::beginPersistent() performs config recovery,
  persistence load/classification and Current-FERMENTING evaluation separately;
  ActuationInterlock::evaluate() consumes fresh permission evidence later
FINDING_F3_REQUIRED_CHANGE=
  rewrite the current boot sequence and its dependent acceptance wording to
  separate BOOT_LOAD_CLASSIFICATION from later ACTUATOR_PERMISSION; no central
  SafetyCore and no product-code boot redesign
```

### F4 – verbliebene aktive Pegelmesspflicht

```text
FINDING_F4_STATUS=CONFIRMED
FINDING_F4_EVIDENCE=
  stale-current: SYSTEM_SAFETY_AND_RECOVERY.md:220-238
  aligned-current: DECISIONS.md:17-58,190-209;
    HARDWARE.md:23-51; ACCEPTANCE_TESTS.md:221-259;
    IMPLEMENTATION_PLAN.md:108-155; OPEN_POINTS.md:39-54
  historical-only: Issue #29 reports and superseded #29 plans that record the
  former measurement context
FINDING_F4_REQUIRED_CHANGE=
  remove the active generic level-measurement mandate repository-wide after
  search classification; retain functional fail-closed boot/reset verification
  and historical evidence
```

### F5 – Offline-R1 versus optional RTC versus trusted-UTC start gate

```text
FINDING_F5_STATUS=CONFIRMED
FINDING_F5_EVIDENCE=
  REQUIREMENTS.md:12-17 and PRODUCT_VISION.md:69-76 require network-independent
  normal/local operation;
  ARCHITECTURE.md:382-395 and HARDWARE.md:316-319 allow RTC-less NTP-only but
  require trusted UTC for a new productive run;
  accepted issue-126 plan @ 52bd69f37e7baac782ebd2fb927f3fa57003f1c7 preserves
  generic RTC optionality and NTP-only support
FINDING_F5_REQUIRED_CHANGE=OWNER_DECISION_BEFORE_IMPLEMENTATION

OWNER_DECISION_REQUIRED=YES
OWNER_DECISION_ID=FERMENTER_R1_OFFLINE_NEW_RUN_TIME_SOURCE_POLICY
IMPLEMENTATION_NOT_ALLOWED_UNTIL_OWNER_DECISION=YES
```

The owner must select exactly one policy:

| Option | Generic platform | concrete Fermenter R1 | resulting documentation/ownership work |
|---|---|---|---|
| A – preferred technical solution | `GENERIC_DEVICE_PLATFORM_NTP_ONLY_SUPPORTED=YES`; `GENERIC_TIME_PLATFORM_RTC_OPTIONAL=YES` | `FERMENTER_R1_CONCRETE_PRODUCT_PROFILE_RTC_REQUIRED=YES`; `FERMENTER_R1_NEW_RUN_OFFLINE_SUPPORTED=YES`; `NEW_RUN_WITHOUT_TRUSTED_UTC=NO` | Require physical RTC variant confirmation and functional RTC verification in a dedicated open hardware scope; update product profile and all current sources consistently. |
| B | generic platform remains NTP-only capable | `FERMENTER_R1_RTC_OPTIONAL=YES`; `RTC_LESS_PROFILE_NEW_RUN_WITHOUT_NETWORK=NOT_SUPPORTED`; `NEW_RUN_WITHOUT_TRUSTED_UTC=NO` | Narrow the top-level offline requirement: regulation, safety and local operation remain offline; a new productive run on RTC-less NTP-only waits for network time. |

Issue #126 is closed and cannot be reintroduced as a fake open hardware owner.
The implementation must live-check open Issue #30/#31/#32/#33 and their
scope/labels. If none explicitly owns RTC hardware, create **one new dedicated
hardware Issue/branch/Draft-PR**, stop at its exact plan SHA, and record
`RTC_HARDWARE_OWNING_ISSUE=NEW_DEDICATED_HARDWARE_SCOPE_REQUIRED`. It must not
be hidden in #30, #31 or #33.

### F6 – explicit boot-factory allocation boundaries

```text
FINDING_F6_STATUS=CONFIRMED
FINDING_F6_EVIDENCE=
  Ds3231SnRtcAdapter::initialize() uses std::make_unique<RtcDevice>() in a
  noexcept esp_err_t boundary (ds3231_sn_rtc_adapter.cpp:33-68);
  NvsStateStore::open() uses new NvsStateStore in an esp_err_t+nullptr boundary
  (nvs_state_store.cpp:66-76);
  NvsOwningContext::create() uses new NvsOwningContext in a nullptr boundary
  (main/app_main.cpp:59-88)
FINDING_F6_REQUIRED_CHANGE=
  make the named factory allocations non-throwing and map null to their existing
  fail-closed result, with rollback/no-leak proof
```

The scope is deliberately not a global no-heap or STL guarantee:

```text
ENABLE_CXX_EXCEPTIONS_AS_FIX=NO
GLOBAL_NO_HEAP_REWRITE=NO
CUSTOM_ALLOCATOR_OR_PMR_REWRITE=NO
NESTED_STL_ALLOCATION_GLOBAL_GUARANTEE=NO
```

The implementation audit must classify all explicit factory/boot allocations
added in `main..8e4c52a` before editing:

| Candidate | initial classification | planned disposition |
|---|---|---|
| RTC descriptor factory | `RUNTIME_IMPLEMENTATION_DEFECT` | `new (std::nothrow)` or equivalent narrow ownership construction; if null, release the previously claimed I2C port and return `ESP_ERR_NO_MEM`; no descriptor access, no false success. |
| NVS store factory | `RUNTIME_IMPLEMENTATION_DEFECT` | nonthrowing allocation; close the newly opened NVS handle on null and return `{ESP_ERR_NO_MEM, nullptr}`. |
| NVS owning-context factory | `RUNTIME_IMPLEMENTATION_DEFECT` | nonthrowing allocation; destroy moved store/deinitialize the partition on null and return `nullptr`, so `app_main()` stops before app/recovery boot. |
| `ConfigurationRecoveryService::create()` | `CURRENT_IMPLEMENTATION_ALREADY_ALIGNED` | already `nothrow` with `nullptr`; retain and test only if a shared factory seam is introduced without architectural expansion. |
| `FermentationApplication::beginPersistent()` and its `RunCommandState` helpers | `CURRENT_IMPLEMENTATION_ALREADY_ALIGNED` | existing `nothrow` plus `requireService(..., applicationAllocationFailure=true)`; preserve as the reference failure contract. |
| `ConfigurationService` runtime/graph snapshot `make_unique`/`make_shared` and direct `new` paths | `COMPARABLE_BOOT_OR_RECOVERY_CANDIDATE_REQUIRES_CALLGRAPH_CLASSIFICATION` | trace every path reached from configuration boot/recovery. If an allocation can bypass an existing status/fail-closed boundary, include it in this same implementation slice with a narrow result/null mapping and targeted test. If it is not a factory/boot boundary, document why it is outside F6 rather than silently ignoring it. |
| ordinary containers, snapshots and allocations outside factory/boot callgraphs | `OUT_OF_SCOPE_UNLESS_CALLGRAPH_PROVES_BOUNDARY_ESCAPE` | no blind replacement. |

### F7 – Roadmap/Open Points status SSOT

```text
FINDING_F7_STATUS=CONFIRMED
FINDING_F7_EVIDENCE=
  ROADMAP.md claims sole current status at lines 3-7 but lists #25 as priority
  one and has no #134/#135 checkpoint; OPEN_POINTS.md still attributes some
  closed #29/#90-era gates too broadly
FINDING_F7_REQUIRED_CHANGE=
  this plan commit synchronizes #134/#135 and #136; later implementation
  corrects remaining status ownership and stale Safety-Core language
```

The current plan commit already establishes:

```text
CHECKPOINT_ISSUE=134
INTEGRATION_PR=135
CUMULATIVE_OWNER_REVIEW=CHANGES_REQUIRED
CORRECTION_ISSUE=136
ISSUE25_STARTED=NO
PR135_READY=NO
MERGE=NO
```

The later implementation must make `OPEN_POINTS.md` distinguish fixed GPIO
design assignment from `PENDING` functional verification, preserve all
functional/commissioning gates as open, and attach each real remaining gate to
a meaningful open owner. Closed #29/#90 must be cited only as closed evidence.

### F8 – Audit currentness

```text
FINDING_F8_STATUS=CONFIRMED
FINDING_F8_EVIDENCE=
  RELEASE_1_FUNCTION_MATRIX.md says Original-Audit 2026-07-27 / base
  7713a66... while rows still describe CUSTOM_SAFETY_CORE, missing NVS/#90 and
  pre-#121/#124/#126 assumptions
FINDING_F8_REQUIRED_CHANGE=
  Strategy A historical snapshot marker; do not selectively modernize rows
```

The marker must state the audit date and basis, `NON_CURRENT_STATUS=YES`, that
no current task/implementation status may be read from it, and direct readers
to `ROADMAP.md`, the specialized current contracts and live Issues/PRs.

## 6. Exact planned file and metadata changes after plan approval

| Area | Planned files | exact minimal change |
|---|---|---|
| current Safety/architecture wording | `ARCHITECTURE.md`, `SAFETY_AND_FAULTS.md`, `RUNTIME_BEHAVIOR.md`, `ACTUATOR_TIMING.md`, `RUN_PERSISTENCE.md`, `DIAGNOSTICS_AND_MAINTENANCE.md`, `IMPLEMENTATION_ISSUES.md`, `IMPLEMENTATION_PLAN.md`, `REQUIREMENTS.md`, `ACCEPTANCE_TESTS.md` | Replace active `SafetyCore`/`SafetyDisposition` gate chains with stateless `ActuationInterlock`, fresh evidence, planner watchdog ownership and finite `FaultCode`; retain historical labels only where explicitly historical. |
| recovery/boot wording | `REQUIREMENTS.md`, `ACCEPTANCE_TESTS.md`, `STATE_MACHINE.md`, cross-links in the current contracts above | Install the exact #124 matrix and separate load classification from later permission evaluation. |
| measurement status | `SYSTEM_SAFETY_AND_RECOVERY.md`; any additional active result of the mandated repository search | Remove only generic level-measurement requirement; retain scope-specific functional safety, SSOT, adapter safety and commissioning gates. |
| F5 after owner decision | current product/architecture/hardware/requirements sources identified by the option-specific search, plus one owning live Issue or a new dedicated hardware scope | Apply only the owner-selected A or B policy consistently; no generic platform regression. |
| F6 code | `ds3231_sn_rtc_adapter.cpp`, `nvs_state_store.cpp`, `main/app_main.cpp`; only further files proven by the explicit callgraph classification | Convert relevant explicit boot/factory allocations to existing null/status failure boundaries, including cleanup and no partial ownership leak. |
| F6 tests | existing RTC adapter, ESP-IDF NVS host adapter, and composition-root/consumer seam tests; add only a narrow seam where fault injection is otherwise impossible | Prove each named OOM boundary, no false success, no start after NVS owning-context failure, and no resource leak/invalid descriptor access. |
| current status | `ROADMAP.md`, `OPEN_POINTS.md`; live Issue #136/PR body and current owning issue bodies only after approved implementation | keep #134/#135/#136 actual; preserve #25 not-started; repair open-owner metadata without closing Issues. |
| historical audit | `docs/audits/RELEASE_1_FUNCTION_MATRIX.md` | add only snapshot/currentness header and links, not mixed row rewrite. |

No board profile, GPIO assignment, pull configuration, net, resistor/design
value, adapter activation, dependency, build configuration or hardware
measurement belongs to this correction scope.

## 7. Implementation order and owner gates

1. Recheck branch/HEAD, PR #135, Issue #134, Issue #136, all current source
   files and current open hardware Issue ownership against this plan SHA.
2. Apply the documentation corrections for F1-F4 and F7-F8 in one coherent
   current-versus-historical pass. Do not alter historical claims other than the
   F8 snapshot marker.
3. Stop at F5 unless the Owner has chosen Option A or B. Record the selected
   policy in this plan **in place**, commit its full replacement revision and
   obtain a new exact owner approval before its implementation.
4. Perform the bounded F6 allocation callgraph audit; change only proven
   factory/boot boundaries and their direct tests, preserving current
   status/null/fail-closed contracts.
5. Update current Roadmap/Open Points and live metadata only after re-reading
   the changed source and owning Issues. Do not mark a hardware/functional gate
   `PASS` without real scope-specific evidence.
6. Review the complete diff against this plan; then run only the targeted
   checks authorized by `CI_AND_QUALITY_GATES.md`. Full native suite, ESP-IDF
   profiles, static analysis and other full gates remain later Owner/workflow
   gates on the final implementation HEAD.

## 8. Later targeted test and proof plan

| Proof | required result |
|---|---|
| current documentation audit | `CURRENT_NORMATIVE_SAFETYCORE_REFERENCES=0_OR_JUSTIFIED`; `CURRENT_NORMATIVE_RECOVERY_CONTRACT=#124_ALIGNED`; `STALE_GENERAL_ELECTRICAL_LEVEL_GATE_REFERENCES=0`; every residual has an explicit role classification. |
| status/historical audit | `ROADMAP_CURRENT_CHECKPOINT=PASS`; `OPEN_POINT_OWNERSHIP=PASS`; `HISTORICAL_PROVENANCE_NOT_REWRITTEN=PASS`. |
| RTC OOM | `RTC_DESCRIPTOR_ALLOCATION_FAILURE_FAILS_CLOSED=PASS`; port claim cleanup/no descriptor use/no false `ESP_OK`. |
| NVS OOM | `NVS_OPEN_ALLOCATION_FAILURE_FAILS_CLOSED=PASS`; opened handle is closed and result is error plus null. |
| composition OOM | `NVS_OWNING_CONTEXT_ALLOCATION_FAILURE_FAILS_CLOSED=PASS`; store/partition cleanup and `app_main()` starts no application/recovery path. |
| expanded F6 candidates | `EXPLICIT_BOOT_FACTORY_ALLOCATION_FAILURE_PATHS=PASS`; every audited candidate has a callgraph classification and test or a documented non-comparability reason. |
| invariants | `GPIO_ASSIGNMENT_CHANGED=NO`; `PULL_CONFIGURATION_CHANGED=NO`; `NET_CONFIGURATION_CHANGED=NO`; `DESIGN_VALUES_CHANGED=NO`; `R_IS_L_IS_R1=DISABLED`; `ACTUATOR_RELEASE=NO`. |

Tests are targeted fault seams, document searches, Markdown/link checks,
`git diff --check`, plan-anchor checks and diff review. This plan round runs no
firmware build, native suite, ESP-IDF profile, static analysis, hardware test
or new measurement.

## 9. Definition of Done and handover

Implementation is done only when:

- each F1-F8 status is rechecked and the corresponding minimal scope is
  completed or explicitly `FALSE_POSITIVE`/`HISTORICAL_ONLY` with evidence;
- F5 has a recorded Owner decision, and any RTC hardware scope has a real open
  owner or the required new dedicated scope;
- F6 has no escaping allocation failure at each applicable explicit
  boot/factory boundary, without broader allocator/exception redesign;
- all current normative and status documents are mutually consistent, historic
  evidence remains intact, and the Function Matrix cannot be misread as
  current status;
- targeted proof results are recorded accurately; unrun hardware/full-suite
  gates remain `NOT_RUN` and no actor release is claimed;
- the complete final diff passes review against the exact approved plan SHA.

The final implementation handover must contain at least:

```text
CORRECTION_ISSUE=136
CORRECTION_PR=<number>
CORRECTION_PR_STATE=OPEN_DRAFT
BASE_BRANCH=integration/r1-development
BASE_SHA=8e4c52a07a488a41b59d98f6fb11742b0678f52a
PLAN_PATH=docs/tasks/issue-136-r1-integration-cumulative-correction-plan.md
PLAN_SHA=<exact approved commit>

CUMULATIVE_FINDINGS_COVERED=F1_F2_F3_F4_F5_F6_F7_F8
OWNER_DECISION_REQUIRED=YES
OWNER_DECISION_ID=FERMENTER_R1_OFFLINE_NEW_RUN_TIME_SOURCE_POLICY
IMPLEMENTATION=<NOT_STARTED|status after a separately approved run>
ISSUE25_STARTED=NO
ACTUATOR_RELEASE=NO

PR135_STATE=OPEN_DRAFT
PR135_READY=NO
MERGE=NO
OWNER_PLAN_REVIEW_REQUIRED=YES
```

For this plan round, `IMPLEMENTATION=NOT_STARTED`; after publishing the single
current Session Handover on the new Draft PR, stop for Owner plan review.
