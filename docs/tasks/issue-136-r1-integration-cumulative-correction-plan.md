# Issue #136 – kumulativen Integrationsbefund von PR #135 KISS-korrigierbar planen

## 0. Planstatus, Live-Basis und harte Grenzen

```text
PLAN_REVISION=FULL_STANDALONE_REPLACEMENT
SUPERSEDES_PLAN_SHA=ef67b827391abdcbf581d7d3cd38f6ba5292965f
PLAN_ONLY=YES
THIS_ROUND=PLAN_CORRECTION_ONLY
CORRECTION_IMPLEMENTATION_ALLOWED=NO
OWNER_MUST_APPROVE_EXACT_FINAL_PLAN_SHA=YES

ISSUE=136
PR=137
PR_STATE=OPEN_DRAFT
BASE_BRANCH=integration/r1-development
BASE_SHA=8e4c52a07a488a41b59d98f6fb11742b0678f52a
PLAN_PATH=docs/tasks/issue-136-r1-integration-cumulative-correction-plan.md
PLAN_SHA=ASSIGNED_BY_THIS_PLAN_REVISION_COMMIT

INTEGRATION_ISSUE=134
INTEGRATION_PR=135
PR135_STATE=OPEN_DRAFT
PR135_SOURCE_HEAD=8e4c52a07a488a41b59d98f6fb11742b0678f52a
PR135_TARGET_BRANCH=main
PR135_TARGET_HEAD=87dd593fcdc8d26831873a4163b174340b4347c0
PR135_READY=NO
PR135_MERGE=NO
ISSUE25_STARTED=NO
ACTUATOR_RELEASE=NO
```

Dies ist ein kleiner Integrationscleanup, kein Folgeprojekt fuer
Hochverfuegbarkeit oder Hardening:

```text
KISS=REQUIRED
ONLY_PROVEN_GAPS_ARE_IMPLEMENTED=YES
PRECAUTIONARY_HARDENING=NO
NEW_PARALLEL_ARCHITECTURE=NO

CORE_PRODUCT_ARCHITECTURE_REDESIGN=NO
RECOVERY_REDESIGN=NO
INTERLOCK_REDESIGN=NO
PERSISTENCE_SCHEMA_CHANGE=NO
BOARD_PROFILE_REDESIGN=NO

PRIMARY_SCOPE=
  CURRENT_DOCUMENTATION_AND_STATUS_ALIGNMENT
  + THREE_NARROW_EXPLICIT_ALLOCATION_BOUNDARIES
  + OWNER_POLICY_F5
EXPECTED_PRODUCT_CODE_FILES=3
```

Die aktuelle Produktarchitektur, der gemergte #124-Recoveryvertrag und der
stateless `ActuationInterlock` werden nicht neu entworfen. In dieser
Planrevision werden ausschließlich dieser Plan, `docs/ROADMAP.md`, der
PR-#137-Body und Issue-#136-Statusmetadaten aktualisiert. Kein Produktions-,
Test-, Build-, Dependency-, Boardprofil-, GPIO- oder Hardwarechange.

## 1. Revalidierte Quellen und Zielvertrag

| Quelle | revalidierter Befund | Planrolle |
|---|---|---|
| PR #135 / Issue #134 | PR #135 ist Draft auf der oben genannten Integrationsbasis und vom Owner kumulativ `CHANGES_REQUIRED`; reine Promotion nach `main` | kein Entwicklungsort |
| #124 / PR #125 | freigegebener Plan `6f4e1a54d521ba60de185f350d571cbefaa23d71`, gemergt | Current-`FERMENTING`-Recoveryvertrag |
| #126 / PR #127 | freigegebener Plan `52bd69f37e7baac782ebd2fb927f3fa57003f1c7`, gemergt | app-neutrale trusted UTC, RTC optional, NTP-only faehig |
| #132 / PR #133 | freigegebener Plan `ae893f20acd68aa96142ce36a9ce8a0ff117b23f`, gemergt | Messwaiver- und Hardwarestatusvertrag |
| aktueller Code | `ActuationInterlock::evaluate(const ActuationEvidence&)` ist stateless; Planner besitzt den Watchdog-Latch | keine Interlock-/Recovery-Refactorarbeit |
| `SPECIFICATION_REVIEW.md`, `DECISIONS.md` / ADR-013 | aktuelle Quellenrollen und app-neutraler Plattformzuschnitt | keine neue Modulgrenze |

Der nur zu synchronisierende Zielvertrag lautet:

```text
R1_INTERLOCK=ActuationInterlock
INTERLOCK_STATELESS=YES
INTERLOCK_OWNS_ACTIVE_FAULT_STATE=NO
INTERLOCK_OWNS_ACKNOWLEDGED_FAULT_STATE=NO
INTERLOCK_OWNS_FAULT_HISTORY=NO
ACTUATION_PERMISSION_INPUT=FRESH_ACTUATION_EVIDENCE
ACTUATION_GATE_DEFAULT=UNRESOLVED
ACKNOWLEDGEMENT_HAS_SAFETY_EFFECT=NO
WATCHDOG_LATCH_OWNER=ActuatorPlanner
FAULT_CODES=FINITE_TYPED_SET

TRUSTED_CURRENT_FERMENTING_LOGICAL_RECOVERY=AUTOMATIC
POWER_LOSS_ALONE_REQUIRES_USER_CONFIRMATION=NO
ACTUATOR_RELEASE_FROM_RECOVERY_ALONE=NO
OLDER_VALID_CHECKPOINT_AUTO_PROMOTION=NO
OLDER_VALID_CHECKPOINT_AUTO_ACTIVATION=NO

ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED
NOT_MEASURED!=PASS
WAIVED_MEASUREMENT!=FUNCTIONAL_PASS
```

## 2. Findings: bestätigter Befund und schmaler Scope

| Finding | Status und Kategorie | konkrete Evidenz | minimaler geplanter Change |
|---|---|---|---|
| F1 | `CONFIRMED`; `CURRENT_DOCUMENTATION_DRIFT` | stateless Code in `actuation_interlock.hpp/.cpp`; aktive SafetyCore-Sprache u. a. in `ARCHITECTURE.md`, `SAFETY_AND_FAULTS.md`, `RUNTIME_BEHAVIOR.md`, `ACTUATOR_TIMING.md`, `RUN_PERSISTENCE.md`, `IMPLEMENTATION_ISSUES.md`, `DIAGNOSTICS_AND_MAINTENANCE.md`, `IMPLEMENTATION_PLAN.md`, `REQUIREMENTS.md`, `ACCEPTANCE_TESTS.md`, `ROADMAP.md` | nur jeweils aktuelle normative/statusbezogene Aussage auf Interlock/Fresh-Evidence/Planner-Latch richten |
| F2 | `CONFIRMED`; `CROSS_PR_CONTRACT_DRIFT` | #124-Plan, `FermentationApplication::evaluateCurrentRecovery()`/`reevaluateWaitingForTrustedTime()`; pauschale Gegenaussagen in `REQUIREMENTS.md` und `ACCEPTANCE_TESTS.md` | nur alte blanket Resume-/NoActiveRun- und SafetyCore-Testketten auf die #124-Matrix korrigieren |
| F3 | `CONFIRMED`; `CURRENT_DOCUMENTATION_DRIFT` | `STATE_MACHINE.md` Bootsequenz vor Run-Klassifikation; Produktcode trennt Classification und `ActuationInterlock::evaluate()` | Boot-Load-Klassifikation von späterer Aktorpermission textlich trennen, kein Boot-SafetyCore |
| F4 | `CONFIRMED`; `CURRENT_DOCUMENTATION_DRIFT` | aktive Messpflicht in `SYSTEM_SAFETY_AND_RECOVERY.md`; aktuelle Waiverquellen in ADR-002, `HARDWARE.md`, `ACCEPTANCE_TESTS.md` und #132 | nur generelle Pegelmesspflicht entfernen; funktionale fail-closed-Nachweise bleiben |
| F5 | `CONFIRMED`; `OWNER_DECISION_OPEN` | offline Normalbetrieb in `REQUIREMENTS.md`/`PRODUCT_VISION.md`; RTC-less/NTP-only plus trusted-UTC Startgate in `ARCHITECTURE.md`/`HARDWARE.md` | ausschließlich Owner-gewählte Produktpolicy dokumentieren |
| F6 | `CONFIRMED`; `EXPLICIT_ALLOCATION_ERROR_CONTRACT_MISMATCH` | die drei Grenzen aus Abschnitt 4 | drei existierende Status-/nullptr-Fehlerwege nutzen; kein Architekturredesign |
| F7 | `CONFIRMED`; `STATUS_METADATA_DRIFT` | Roadmap vor dieser Revision hatte #134/#135/#136 nicht vollständig und verwies bei #90 auf offene Ownership | aktuelle Statuszeilen und `OPEN_POINTS.md`-Ownership präzisieren, keinen Gate-PASS erfinden |
| F8 | `CONFIRMED`; `CURRENT_DOCUMENTATION_DRIFT` einer historischen Lesart | `RELEASE_1_FUNCTION_MATRIX.md` ist ein Audit von 2026-07-27 und enthält pre-#121/#124/#126-Status | nur klaren `NON_CURRENT_STATUS`-Snapshotmarker setzen |

Die drei verbindlichen Befundfelder bleiben für die spätere Umsetzung:

```text
FINDING_F1_STATUS=CONFIRMED
FINDING_F1_EVIDENCE=current normative SafetyCore references versus ActuationInterlock code
FINDING_F1_REQUIRED_CHANGE=current documentation only
FINDING_F2_STATUS=CONFIRMED
FINDING_F2_EVIDENCE=accepted #124 plan, recovery code, stale Requirements/Acceptance statements
FINDING_F2_REQUIRED_CHANGE=exact #124 wording only
FINDING_F3_STATUS=CONFIRMED
FINDING_F3_EVIDENCE=STATE_MACHINE boot order versus current separated code paths
FINDING_F3_REQUIRED_CHANGE=document order only
FINDING_F4_STATUS=CONFIRMED
FINDING_F4_EVIDENCE=SYSTEM_SAFETY_AND_RECOVERY generic measurement mandate
FINDING_F4_REQUIRED_CHANGE=remove mandate only
FINDING_F5_STATUS=CONFIRMED
FINDING_F5_EVIDENCE=offline-product requirement versus optional RTC/trusted-UTC gate
FINDING_F5_REQUIRED_CHANGE=OWNER_DECISION_BEFORE_IMPLEMENTATION
FINDING_F6_STATUS=CONFIRMED
FINDING_F6_EVIDENCE=the three named explicit factory allocations only
FINDING_F6_REQUIRED_CHANGE=three narrow status/nullptr mappings only
FINDING_F7_STATUS=CONFIRMED
FINDING_F7_EVIDENCE=Roadmap/Open Points current ownership and checkpoint drift
FINDING_F7_REQUIRED_CHANGE=current status alignment only
FINDING_F8_STATUS=CONFIRMED
FINDING_F8_EVIDENCE=historical Function Matrix presented without an explicit non-current marker
FINDING_F8_REQUIRED_CHANGE=snapshot marker only
```

`SafetyCore` in completed plans, reviews and historic audit provenance remains
`HISTORICAL_PROVENANCE_OK`; C2/#18 legacy recovery remains
`LEGACY_COMPATIBILITY_KEEP`. Neither is globally rewritten.

## 3. Dokument- und Statusslice (F1–F4, F7–F8)

```text
SLICE_1=CURRENT_DOC_STATUS_ALIGNMENT_F1_F2_F3_F4_F7_F8
CHANGE_ONLY_IF_CURRENT_STATEMENT_IS_ACTUALLY_STALE=YES
UNCHANGED_CORRECT_SECTIONS=KEEP
HISTORICAL_SECTIONS=KEEP
MECHANICAL_GLOBAL_REPLACE=NO
```

The implementation re-runs the required searches for `SafetyCore`,
`SafetyDisposition`, recovery terms, electrical-level terms and the current
Issue markers. Every hit is classified as `CURRENT_NORMATIVE_MUST_FIX`,
`CURRENT_STATUS_MUST_FIX`, `HISTORICAL_SUPERSEDED_KEEP`,
`LEGACY_COMPATIBILITY_KEEP`, `NON_NORMATIVE_NEEDS_EXPLICIT_HISTORICAL_MARKER`
or `UNRELATED`. A correct current statement stays unchanged.

| current file group | allowed change |
|---|---|
| F1 terminology: `ARCHITECTURE.md`, `SAFETY_AND_FAULTS.md`, `RUNTIME_BEHAVIOR.md`, `ACTUATOR_TIMING.md`, `RUN_PERSISTENCE.md`, `DIAGNOSTICS_AND_MAINTENANCE.md`, `IMPLEMENTATION_ISSUES.md`, `IMPLEMENTATION_PLAN.md`, `REQUIREMENTS.md`, `ACCEPTANCE_TESTS.md`, `ROADMAP.md` | only stale current `SafetyCore`/`SafetyDisposition` wording; retain correct text and all historic sections |
| F2/F3: `REQUIREMENTS.md`, `ACCEPTANCE_TESTS.md`, `STATE_MACHINE.md` | exact #124 current-`FERMENTING`, `WaitingForTrustedTime`, `ResumeOffer`, `NoActiveRun` and boot/permission wording; no C2/#18 rewrite |
| F4: `SYSTEM_SAFETY_AND_RECOVERY.md` and only further active mandated-search hits | remove generic electrical measurement requirement, not functional Boot/Reset safety |
| F7: `ROADMAP.md`, `OPEN_POINTS.md` | #134/#135/#136/#137 current state, #25 not-started, real open owners; #29/#90 as closed evidence only |
| F8: `docs/audits/RELEASE_1_FUNCTION_MATRIX.md` | dated/base-SHA historical snapshot marker and links to current SSOTs; do not alter stale rows selectively |

No hardware status becomes `PASS`; fixed GPIO design assignment and remaining
functional/commissioning verification remain separate.

## 4. F5 owner policy gate

```text
SLICE_2=F5_OWNER_SELECTED_POLICY_DOCUMENT_ALIGNMENT
F5_OWNER_DECISION_REQUIRED=YES
OWNER_DECISION_ID=FERMENTER_R1_OFFLINE_NEW_RUN_TIME_SOURCE_POLICY
IMPLEMENTATION_NOT_ALLOWED_UNTIL_OWNER_DECISION=YES
RECOMMENDED_OPTION=A
```

The Owner chooses A or B; this plan records no chosen policy as already
approved.

```text
IF_OPTION_A:
  GENERIC_TIME_PLATFORM_RTC_OPTIONAL=YES
  GENERIC_DEVICE_PLATFORM_NTP_ONLY_SUPPORTED=YES
  FERMENTER_R1_CONCRETE_PRODUCT_PROFILE_RTC_REQUIRED=YES
  FERMENTER_R1_NEW_RUN_OFFLINE_SUPPORTED=YES
  NEW_RUN_WITHOUT_TRUSTED_UTC=NO
  RTC_PHYSICAL_VERIFICATION_STILL_REQUIRED=YES
  RTC_HARDWARE_OWNER=<existing suitable open issue OR UNASSIGNED>
  AUTO_CREATE_RTC_ISSUE=NO
  AUTO_CREATE_RTC_BRANCH=NO
  AUTO_CREATE_RTC_PR=NO

IF_OPTION_B:
  GENERIC_TIME_PLATFORM_RTC_OPTIONAL=YES
  GENERIC_DEVICE_PLATFORM_NTP_ONLY_SUPPORTED=YES
  FERMENTER_R1_RTC_OPTIONAL=YES
  RTC_LESS_PROFILE_NEW_RUN_WITHOUT_NETWORK=NOT_SUPPORTED
  NEW_RUN_WITHOUT_TRUSTED_UTC=NO

IF_NO_EXISTING_OWNER:
  RTC_HARDWARE_OWNER=UNASSIGNED
  RTC_HARDWARE_SCOPE_REQUIRED=YES
  OWNER_LATER_DECIDES_TRACKING_SCOPE=YES
```

The #136 implementation only documents the selected product policy and the
still-open real hardware proof. It neither starts a second RTC workflow nor
adds multi-RTC support. Issue #126 remains closed historical/digital evidence.

## 5. F6: exactly three explicit allocation boundaries

```text
SLICE_3=F6_THREE_NARROW_ALLOCATION_BOUNDARIES
F6=EXPLICIT_ALLOCATION_ERROR_CONTRACT_MISMATCH
F6_CORE_FUNCTIONAL_LOGIC_DEFECT=NO
F6_ARCHITECTURE_REDESIGN_REQUIRED=NO
F6_CONFIRMED_PRODUCT_CODE_SCOPE=
  lib/device_platform_esp_idf/src/ds3231_sn_rtc_adapter.cpp
  lib/device_platform_esp_idf/src/nvs_state_store.cpp
  main/app_main.cpp
EXPECTED_PRODUCT_CODE_FILES=3
ADDITIONAL_F6_FILE_ALLOWED=NO_BY_DEFAULT

ENABLE_CXX_EXCEPTIONS_AS_FIX=NO
GLOBAL_NO_HEAP_REWRITE=NO
CUSTOM_ALLOCATOR_OR_PMR_REWRITE=NO
NEW_ALLOCATOR_FRAMEWORK_FOR_TEST=NO
GLOBAL_OPERATOR_NEW_OVERRIDE=NO
EXCEPTION_ENABLE=NO
```

| boundary | existing failure contract | minimal change and proof |
|---|---|---|
| `Ds3231SnRtcAdapter::initialize()` / `std::make_unique<RtcDevice>()` | `esp_err_t`, already claimed I2C port, `noexcept` | map allocation failure to existing error return; release only the just-acquired port; no descriptor access, no false `ESP_OK` |
| `NvsStateStore::open()` / `new NvsStateStore(...)` | `{esp_err_t, nullptr}` after a successful handle open | map allocation failure to error plus null; close the just-opened NVS handle; no false success |
| `NvsOwningContext::create()` / `new NvsOwningContext(...)` | `unique_ptr`/`nullptr`; `app_main()` stops if null | map allocation failure to null; clean up opened store and initialized partition; no application/recovery start |

A simple direct comparable boot/factory search is permitted before editing to
ensure no identical pattern was missed. It does **not** authorize a general
`new`/`make_unique`, ConfigurationService/graph-snapshot, nested-STL or
callgraph-hardening audit. If it proves a fourth directly comparable boundary:

```text
STOP
NEW_F6_FINDING=YES
OWNER_PLAN_REVIEW_REQUIRED=YES
```

Tests use the smallest existing seam/helper and existing ownership/resource
tests. No production abstraction is added merely to force OOM. If a
deterministic OOM test would require a generic allocator framework, do not add
it; retain the minimal code/cleanup proof and document that test limitation
accurately.

## 6. Four execution slices, targeted checks and Definition of Done

```text
SLICE_1=CURRENT_DOC_STATUS_ALIGNMENT_F1_F2_F3_F4_F7_F8
SLICE_2=F5_OWNER_SELECTED_POLICY_DOCUMENT_ALIGNMENT
SLICE_3=F6_THREE_NARROW_ALLOCATION_BOUNDARIES
SLICE_4=TARGETED_CHECKS_AND_HANDOVER
```

1. Revalidate the approved plan SHA, live #134/#135/#136/#137, affected current
   statements and historical classifications. Apply Slice 1 only where a
   statement is actually stale.
2. Stop at F5 until the Owner policy decision. After a decision, replace this
   file in place with a complete policy-selected revision and obtain the new
   exact owner approval before implementing Slice 2.
3. Implement only the three F6 files, their direct cleanup, and the smallest
   corresponding existing tests/helpers. Stop for any fourth comparable finding.
4. Run targeted document searches, relevant existing narrow tests where
   available, `git diff --check`, Markdown/link/plan-anchor checks and complete
   diff review. Full native suite, ESP-IDF profiles, static analysis and
   hardware tests are not part of this plan round.

Done after implementation means: current norms/statuses are aligned without
historic rewrite; F5 is owner-selected but real RTC proof stays open with an
existing owner or `UNASSIGNED`; exactly three F6 boundaries follow their
existing error/null contract and do not claim false success; and no GPIO,
pull, net, design, schema, actor-release or hardware state changes.

## 7. Handover contract for this plan revision

```text
ISSUE136=OPEN
PR137=OPEN_DRAFT
PLAN_SHA=<new exact plan commit>

PLAN_SCOPE_KISS=PASS
CORE_ARCHITECTURE_REDESIGN=NO
RECOVERY_REDESIGN=NO
INTERLOCK_REDESIGN=NO
F1_F4_F7_F8=DOCUMENT_STATUS_ALIGNMENT
F5=OWNER_DECISION_PENDING
F6=THREE_NARROW_ALLOCATION_BOUNDARIES

IMPLEMENTATION=NOT_STARTED
CORRECTION_IMPLEMENTATION_ALLOWED=NO
PRODUCTION_CODE_CHANGE=NO
TEST_CODE_CHANGE=NO
BUILD_CHANGE=NO
DEPENDENCY_CHANGE=NO
BOARD_PROFILE_CHANGE=NO
GPIO_CHANGE=NO
HARDWARE_TEST=NOT_RUN
NEW_HARDWARE_MEASUREMENT=NOT_RUN

PR135=OPEN_DRAFT
PR135_READY=NO
PR137_READY=NO
MERGE=NO
ISSUE25_STARTED=NO
OWNER_PLAN_REVIEW_REQUIRED=YES
```

After publishing exactly one current `SESSION HANDOVER` on PR #137, stop for
Owner plan review. Neither PR is made Ready and nothing is merged.
