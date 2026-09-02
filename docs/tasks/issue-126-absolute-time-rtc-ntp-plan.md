# [E3.8] Plan: R1-Absolute-Zeitplattform mit DS3231SN und ESP-IDF-SNTP

## Status und Owner-Gate

```text
PLAN_STATUS=PLAN_ONLY
IMPLEMENTATION=NOT_STARTED
MERGE=NO
OWNER_PLAN_REVIEW_REQUIRED=YES
OWNER_PLAN_REVIEW_SCOPE=EXACT_PLAN_COMMIT
```

Dieser Plan ist die vollstaendige, eigenstaendige Planfassung fuer Issue #126.
Er wird nach der bestaetigten Owner-Freigabe des exakten Plan-Commits
umgesetzt. In dieser Planrunde werden keine Produktionsdateien, Testdateien,
Abhaengigkeiten, ADRs oder Hardwareprofile mit produktiven Werten geaendert.

## 1. Live-Baseline und Ausfuehrungsgate

Der Auftrag wurde erst nach der Live-Pruefung von PR #125 ausgefuehrt:

```text
PR125_STATE=MERGED
PR125_MERGE_COMMIT=5b8b86b99347bb0bb104dd1c2968040656119440
R1_DEVELOPMENT_BASE_BRANCH=integration/r1-development
R1_DEVELOPMENT_BASE_SHA=5b8b86b99347bb0bb104dd1c2968040656119440
CURRENT_PLAN_BRANCH=agent/issue-126-absolute-time-rtc-ntp-plan
ISSUE=126
```

Die Merge-Ancestor-Pruefung des PR-125-Merge-Commits gegen
`origin/integration/r1-development` war `PASS`. Der alte #124-Featurebranch
wurde nicht fortgesetzt; die neue Arbeit basiert auf dem aktuellen
Integrationsbranch. Vor Erstellung dieses Draft-PR bestanden keine offenen
PRs.

Issue #124 bleibt fachlich unveraendert und ist nach PR #125 geschlossen. Der
neue Zeitbaustein liefert nur die trusted UTC, die #124 bereits als Eingang
verwendet:

```text
trusted current UTC available   -> #124 exact recovery evaluation
trusted current UTC unavailable -> #124 WaitingForTrustedTime
```

## 2. Ownerentscheidungen und Ziel

Die folgenden Entscheidungen sind fuer diesen Plan vorgegeben:

```text
ABSOLUTE_TIME_ARCHITECTURE=RTC_PLUS_NTP
RTC=DS3231SN
RTC_DEVICE=DS3231SN
RTC_FAMILY=DS3231
R1_RTC_VARIANT=DS3231SN
NTP=ESP_IDF_SNTP
APPLICATION_TIME_PORT=device_platform::ITimeSource
PROVISIONAL_OPERATOR_RESUME_BEFORE_TRUSTED_TIME=NO
RTC_STORAGE_TIMEZONE=UTC
RTC_I2C_ADDRESS=0x68
NTP_REAL_NETWORK_OPERATION_DEPENDS_ON_CONNECTIVITY=YES
RTC_HARDWARE_OPTIONAL=YES
NTP_ONLY_MODE_SUPPORTED=YES
RTC_REQUIRED_FOR_IMMEDIATE_OFFLINE_RECOVERY=YES
NEW_PRODUCTIVE_RUN_START_REQUIRES_TRUSTED_UTC=YES
FRONTEND_ONLY_TIME_GATE=NO
APPLICATION_DOMAIN_START_GATE=YES
NEW_RUN_CAN_CREATE_UTC_LESS_RECOVERY_ANCHOR=NO
R1_CURRENT_FERMENTING_CHECKPOINT_REQUIRES_TRUSTED_UTC=YES
UTC_LESS_CURRENT_FERMENTING_COMMIT=FORBIDDEN
LAST_VALID_UTC_CURRENT_RETAINED_ON_TIME_TRUST_LOSS=YES
PERIODIC_FERMENTING_CHECKPOINT_WITHOUT_TRUSTED_UTC=SKIP_NO_WRITE
SKIPPED_FOR_MISSING_TRUSTED_UTC_DOES_NOT_CONFIRM_CHECKPOINT_SCHEDULE=YES
FERMENTING_MUTATION_RESULTING_IN_FERMENTING_REQUIRES_TRUSTED_UTC=YES
FERMENTING_MUTATION_WITHOUT_TRUSTED_UTC=REJECT_OR_DEFER_BEFORE_PERSISTENCE_AND_RAM_APPLY
FERMENTING_MUTATION_WITHOUT_TRUSTED_UTC_RAM_APPLY=NO
ALL_PERSISTENCE_REQUIRES_UTC=NO
WRITE_INVARIANT_TIGHTENED=YES
READ_COMPATIBILITY_RETAINED=YES
NO_SYNTHETIC_UTC_FROM_MONOTONIC=YES
NO_LAST_UTC_PLUS_MONOTONIC_SECOND_CLOCK=YES
NO_RETROACTIVE_CHECKPOINT_REWRITE=YES
NO_FALLBACK_AUTO_PROMOTION=YES
NO_ISSUE124_CHANGE=YES

NTP_ONLY_START_BEFORE_SYNC=REJECT_NO_MUTATION_OR_CHECKPOINT
NTP_ONLY_START_AFTER_SYNC=ALLOW_FIRST_CHECKPOINT_HAS_TRUSTED_UTC
RTC_START_WITHOUT_NETWORK=ALLOW_AFTER_TRUSTED_RTC_BOOT_SEED
SMALL_RTC_NTP_DELTA_POLICY=USE_ESP_IDF_SMOOTH_SYNC_FOR_NORMAL_CORRECTION
CUSTOM_SMALL_DELTA_THRESHOLD=NO
MONOTONIC_MILLIS_RETROGRADE=NEVER
TRUSTED_ITIME_SOURCE_RETROGRADE_PUBLICATION=NO
CUSTOM_LARGE_DELTA_THRESHOLD=NO
ESP_IDF_SMOOTH_SYNC=YES
ESP_IDF_DOCUMENTED_LARGE_DELTA_IMMEDIATE_STEP=ACCEPTED_PLATFORM_BEHAVIOR
LARGE_DELTA_RECOVERY_POLICY=NO_RETROACTIVE_RECOVERY_ENGINE
LARGE_TIME_DELTA_POLICY=ACCEPT_ESP_IDF_DOCUMENTED_SYNC_BEHAVIOR
RTC_SYNC_WRITE_FREQUENCY=AFTER_EVERY_COMPLETED_NTP_SYNCHRONIZATION
RTC_WRITE_DURING_SMOOTH_IN_PROGRESS=NO
RTC_WRITE_AFTER_SYNC_COMPLETED=YES
WIFI_LOSS_AFTER_SUCCESSFUL_NTP_SYNC_INVALIDATES_CURRENT_BOOT_TIME=NO
NTP_SERVER_UNREACHABLE_AFTER_SUCCESSFUL_SYNC_INVALIDATES_CURRENT_BOOT_TIME=NO
RTC_UNREACHABLE_AFTER_SUCCESSFUL_BOOT_SEED_INVALIDATES_CURRENT_BOOT_TIME=NO
DS3231SN_EOSC_REQUIRED=0
DS3231SN_32KHZ_OUTPUT_R1=DISABLED
SQW_INT_R1=UNUSED
R1_DS3231SN_SUPPORTED_UTC_YEAR_RANGE=2000..2099
DS3231SN_LIBRARY_COMPATIBILITY=TO_BE_VERIFIED_BY_ESP_IDF_6_0_2_BUILD_AND_TARGETED_TESTS
LIBRARY_VARIANT_MISMATCH=NO
FULL_OWN_DS3231_DRIVER=NO
RAW_REGISTER_VALIDATION=REQUIRED
RAW_REGISTER_VALIDATION_SOURCE=ANALOG_DEVICES_DS3231_DATASHEET
RAW_REGISTER_VALIDATION_VARIANT=DS3231_DS3231SN
NARROW_DS3231_HEALTH_SHIM=YES_IF_REQUIRED_FOR_EOSC_AND_RAW_REGISTER_VALIDATION
FULL_REGISTER_DRIVER=NO
DS3231_RS_BITS_R1_FUNCTIONAL_USE=NO
DS3231SN_RS_BITS_PRESERVED_BY_HEALTH_SHIM=REQUIRED
DS3231_SQUAREWAVE_RATE_API_REQUIRED=NO
DS3231_AGING_OFFSET_API_REQUIRED=NO
DS3231_TEMPERATURE_API_REQUIRED=NO
RTC_PHYSICAL_VARIANT_CONFIRMATION=REQUIRED
DS3231M_R1_SUPPORT=NO
MULTI_RTC_VARIANT_SUPPORT_R1=NO
DEPENDENCY_OWNER_COMPONENT=lib/device_platform_esp_idf
MANIFEST_PATH=lib/device_platform_esp_idf/idf_component.yml
DEPENDENCIES_LOCK=project-level generated dependencies.lock
NTP_SERVER_CONFIGURATION_OWNER=ISSUE126_TIME_PLATFORM
NETWORK_CONNECTIVITY_OWNER=ISSUE89
I2C_PORT_LIFETIME_OWNER=I2CDEV
I2CDEV_GLOBAL_INIT_OWNER=DEVICE_PLATFORM_ESP_IDF_COMPOSITION_LIFETIME
SECOND_I2C_MASTER_BUS_OWNER_ON_SAME_PORT=NO
SHARED_I2C_BUS=YES
I2C_PIN_CONFLICT_POLICY=REJECT_NO_SILENT_RECONFIGURATION
I2C_LIBRARY_ADOPTION_ABORT_GATE=STOP_LIBRARY_ADOPTION_GATE
OWNER_DECISIONS_REQUIRED=NONE
```

Der Datenfluss ist:

```text
DS3231SN RTC ------------------\
                                -> ESP-Systemzeit / UTC
ESP-IDF SNTP ------------------/             |
                                             v
                              device_platform::ITimeSource
                                             |
                                             v
                                      fermentation_app
```

`fermentation_app` kennt weder DS3231SN noch SNTP. `device_platform` bleibt
app-neutral und enthaelt keine ESP-IDF- oder Fermentationsbegriffe.

Zielsemantik des bestehenden Ports:

```text
monotonicMillis()
  -> weiterhin monotone, bootlokale Laufzeit
  -> von RTC-/NTP-Korrekturen unberuehrt

unixTimeSeconds()
  -> trusted UTC aus der ESP-Systemzeit
  -> std::nullopt, solange keine Quelle trusted ist
```

## 3. Nicht-Ziele und harte Grenzen

- Kein zweiter Zeit-Port, kein Zeitqualitaets-Enum und keine parallele
  `fermentation`-Zeit-API.
- Keine WLAN-Provisionierung, Credentials, SoftAP-/Captive-Portal-,
  Connectivity- oder Connection-Manager-Logik. Das bleibt Issue #89.
- Kein eigener NTP-Client.
- Kein vollstaendiger eigener DS3231SN-Registertreiber. Ein schmaler,
  dokumentierter Health-/Trust-Shim ist nur fuer EOSC und rohe
  Registervalidierung zulaessig, falls die ausgewaehlte Komponente diese
  Nachweise nicht oeffentlich ermoeglicht.
- Keine Aenderung an `priorBootPhaseElapsed`, `observedRunSeconds`,
  `wall_clock_since_checkpoint_seconds`, `RecoveryDisposition` oder der
  bestehenden FSM-Recoverytopologie.
- Kein produktiver Run darf ohne trusted UTC beginnen. Ein UI darf diesen
  Zustand anzeigen, ist aber nicht die Autoritaet; ein direkter
  Domain-/Application-Aufruf wird am gemeinsamen Startgate ebenfalls
  abgewiesen.
- Keine UTC-lose Run-Start-Checkpoint-Ankerung und keine nachtraegliche
  Reparatur historischer UTC-loser Checkpoints.
- Kein neuer aktiver `ProgramRun`-/`ManualRun`-Current im Zustand
  `ProcessState::Fermenting` darf ohne trusted UTC committed werden. Bei einer
  Trust-Luecke wird der letzte UTC-tragende Current behalten; es werden weder
  synthetische UTC noch eine zweite monotone Ersatzuhr, Checkpoint-Rewrites
  oder eine automatische Fallback-Hochstufung gebaut.
- Keine globale Regel `ALL_PERSISTENCE_REQUIRES_UTC=YES`: Stop/Abort zu
  `NoActiveRun`, terminale/sichere Non-`FERMENTING`-Zustaende und andere
  bestehende gueltige Nicht-Recovery-Pfade behalten ihre vorhandene Semantik,
  sofern kein anderer Vertrag UTC verlangt.
- Kein provisionaler Operator-Resume und keine Aktorfreigabe ohne trusted
  absolute Zeit, wenn #124 sie fuer Current-`FERMENTING` benoetigt.
- Keine geratenen GPIOs, I2C-Ports, Pegel, Batterieannahmen oder
  Hardwarefreigaben.
- Keine SQW-, INT- oder 32-kHz-Nutzung in R1, solange der Audit keinen echten
  Bedarf nachweist.
- Kein Hardwarelauf in diesem PR. `TBD_HARDWARE`, `TBD_COMMISSIONING` und
  `TBD_IMPLEMENTATION_BUDGET` werden niemals produktive Laufzeitwerte.

## 4. Verifizierter Ist-Audit

### 4.1 Dokumente und Entscheidungen

| Quelle | Verifizierter Iststand | Konsequenz fuer Issue #126 |
|---|---|---|
| `docs/AGENT_WORKFLOW.md` | Plan-first; versionierter vollstaendiger Plan, exakte Plan-SHA, Draft-PR und Handover | Nur Dokumentations- und Reihenfolgesynchronisierung in dieser Runde |
| `docs/ENGINEERING_PRINCIPLES.md` | Repository-first, Espressif-first, Adopt-or-build vor Eigenbau, keine Parallelvertraege | Erst ESP-IDF-Bestand und Registry-Kandidat bewerten; Adapter hinter bestehendem Port |
| `docs/DECISIONS.md`, ADR-013 | `device_platform` fuer app-neutrale Ports, `device_platform_esp_idf` fuer konkrete Adapter, `fermentation_app` nur gegen Ports | DS3231SN/SNTP in `device_platform_esp_idf`; `ITimeSource` bleibt einziger App-Port |
| `docs/ARCHITECTURE.md` | `ITimeSource` und optionale UTC sind vorhanden; NTP ist als absolute Zeitquelle vorgesehen; RTC war bisher Erweiterungspunkt | Der Isttext benoetigt nach Ownerfreigabe eine konsistente RTC-Ergaenzung; in dieser Planrunde nicht umschreiben |
| `docs/RECOVERY_AND_INTERRUPTION.md` | #124 verlangt trusted aktuelle UTC fuer den exakten Current-`FERMENTING`-Pfad; fehlende UTC bleibt RAM-only `WaitingForTrustedTime` und all-off | Zeitplattform liefert nur den bestehenden Eingang; keine Recoverylogik duplizieren |
| `docs/SYSTEM_SAFETY_AND_RECOVERY.md` | Fail-closed, keine Aktorfreigabe aus unbekannter Lage, frische Safety-Evidenz bleibt erforderlich | RTC/NTP-Trust darf keine Safety-Grenze umgehen |
| `docs/HARDWARE.md` | ESP32-32E, 4 MB, keine PSRAM-Abhaengigkeit; Pins/Boardrevision bleiben offen; RTC ist dort noch als nicht verpflichtende Zukunftshardware dokumentiert | RTC bleibt optional; `rtc.present=false` und NTP-only sind gueltige Profile, waehrend trusted RTC sofortige Offline-Recovery ermoeglicht |
| `docs/HARDWARE_REVISIONS.md` | NTP primaer, RTC optional, spaetere Zeitkorrektur historisch beschrieben | Historischer Konflikt wird sichtbar gehalten; Umsetzung benoetigt eine neue konsistente normative Fassung |
| `docs/OPEN_POINTS.md` | Boardrevision, GPIOs, Bootpegel, Flash/RAM und reale Hardwaremessungen sind offen | RTC-Software darf digital geplant werden; RTC-Hardware bleibt blockiert |
| `docs/ESP_IDF_UPGRADE_CONTRACT.md` | ESP-IDF `v6.0.2` am Commit `7101770dc6db2667b3c477cc31365dd1acd6db4e`; Komponenten werden erst bei echtem Bedarf mit Lockfile gebunden | `idf_component.yml` und generiertes `dependencies.lock` erst nach Planfreigabe |
| `docs/CI_AND_QUALITY_GATES.md` | Planung fuehrt keine Builds oder vollstaendigen Testlaeufe aus; native und beide ESP-IDF-Profile sind spaetere Gates | Alle Planungsnachweise bleiben `NOT_RUN` |

Die bisherigen Aussagen "RTC nicht verpflichtend" und "RTC Zukunftshardware"
werden nicht in eine RTC-Pflicht fuer jedes Produktprofil umgedeutet:
`rtc.present=false` und NTP-only sind gueltig. Die Ownerentscheidung bestimmt
DS3231SN als optionale Plattformquelle und verlangt sofortige Offline-Recovery
nur bei vorhandener und trusted RTC. Die alten Dokumente benoetigen erst in
einem spaeteren freigegebenen Normativschnitt eine konsistente Einordnung; bis
dahin gilt `SOURCE_OF_TRUTH=ISSUE126_OWNER_DECISION_WITH_RTC_OPTIONALITY`.

### 4.2 Bestehende Zeit- und Recovery-Implementierung

Der relevante Produktionsstand ist:

- `lib/device_platform/src/time_source.hpp` definiert den einzigen
  app-neutralen `ITimeSource`-Port mit `uint64_t monotonicMillis()` und
  `std::optional<int64_t> unixTimeSeconds()`.
- `VirtualTimeSource` trennt monotone Testzeit und optionale simulierte UTC;
  `setUnixTimeSeconds()` beeinflusst die monotone Zeit nicht.
- `lib/device_platform_esp_idf/src/esp_timer_time_source.*` verwendet
  `esp_timer_get_time()` mit einem Instanz-Baselinewert und liefert fuer UTC
  derzeit immer `std::nullopt`.
- `main/app_main.cpp` erstellt `EspTimerTimeSource`, injiziert sie vor dem
  Application-Boot und verwendet danach denselben Port fuer die Laufzeit.
- `FermentationApplication::currentCheckpointTime()` liest ausschliesslich
  `ITimeSource`. `update()` ruft bereits die bestehende
  `reevaluateWaitingForTrustedTime()` auf.
- `RunPersistenceCoordinator` behandelt fehlende aktuelle UTC nach validiertem
  Checkpoint-UTC als `WaitingForTrustedTime`; negative oder unklare UTC-Deltas
  werden nicht geraten, sondern abgelehnt.
- `RunCheckpointTime.utcUnixSeconds` ist optional und
  `FermentationApplication::currentCheckpointTime()` reicht die optionale
  Rueckgabe des `ITimeSource` unveraendert weiter.
- `checkpointPeriodic(...)` prueft Faelligkeit und baut danach einen Snapshot
  mit der optionalen Zeit. Ohne Guard koennte ein faelliger
  `FERMENTING`-Checkpoint mit `utcUnixSeconds=nullopt` Slot und Current
  ersetzen; `RunCheckpointSchedule::confirm()` wird erst nach dem durable
  Commit aufgerufen.
- `persistCommand`, `persistTransition` und `persistSensorSelection` bilden
  ebenfalls zuerst den Ziel-Snapshot und fuehren den Write-before-Apply-Pfad
  aus. Ohne gemeinsame Pruefung koennte auch eine Mutation einen aktiven
  `FERMENTING`-Current ohne UTC durable machen. Ein bestehender
  `AppliedRamOnly`-Sonderpfad ist kein neuer durable Current und wird nicht
  pauschal durch eine UTC-Regel blockiert.
- `makeRunPersistenceSnapshotInto(...)` und der Snapshot-Validator sehen die
  UTC im aeusseren `StorageEnvelope` nicht vollstaendig. Der neue
  Schreibinvariant muss deshalb an einer Application-/Persistence-Domain-
  Grenze geprueft werden, die Ziel-Snapshot und `RunCheckpointTime` gemeinsam
  sieht; eine alleinige Validatorverschaerfung waere fuer den Envelopevertrag
  unzureichend.
- Die bestehenden Programm- und Manual-Startentscheidungen nehmen derzeit
  keine Zeitquelle entgegen. Der bestehende
  `TemperatureControlApplicationOrchestrator::persistFreshStartCommand(...)`
  ist jedoch die gemeinsame Application-Grenze vor Persistenz und Apply und
  prueft aktuell nur die Start-Entscheidungsart. Ohne zusaetzliches Gate kann
  `RunPersistenceCoordinator::persistCommand` dadurch einen neuen aktiven
  Checkpoint mit `utcUnixSeconds=nullopt` serialisieren.
- `ProcessStateMachine` kennt weder Persistenz, RTC, NTP noch Systemzeit und
  bleibt unveraendert.
- Das Repository besitzt derzeit keine ESP-IDF-Komponentendependency, kein
  `idf_component.yml`, kein `dependencies.lock`, keinen produktiven I2C-
  Adapter und keinen SNTP-Lifecycle.

Damit ist keine zweite Zeit-API erforderlich. Die kleinste Erweiterung besteht
aus einer konkreten Systemzeit-/Trust-Implementierung hinter demselben Port,
einem DS3231SN-Adapter, einem kleinen ESP-IDF-SNTP-/Arbitrationskoordinator und
einem einzigen produktiven Startgate sowie einem gemeinsamen
Current-FERMENTING-Schreibguard an der bestehenden Application-/Persistence-
Grenze.

### 4.3 Trusted-Time-Startgate-Audit

Der Startgate-Befund ist eine Domain-/Application-Luecke, keine UI-Luecke:

- `fermentation_app` kennt bereits nur `device_platform::ITimeSource`; RTC,
  NTP und deren Herkunft bleiben ausserhalb der Application.
- Der Gatepunkt muss vor der ersten Start-Persistenz und vor dem Apply einer
  produktiven Startentscheidung liegen. Eine reine UI-Deaktivierung des
  Startbuttons waere durch direkte Command-Aufrufe umgehbar.
- Die spaetere rendererunabhaengige Command-/Result-Schicht muss einen
  eindeutigen Ablehnungsgrund sinngemaess `TrustedAbsoluteTimeRequired`
  projizieren koennen. Der konkrete Enum-/Typname folgt den vorhandenen
  Command-Namenskonventionen; es wird kein neuer Zeit-Port eingefuehrt.

Der geplante minimale Gatevertrag ist:

```text
ITimeSource::unixTimeSeconds().has_value() == false
    -> produktiven neuen Run ablehnen
    -> keine aktive Run-Mutation und kein Start-Checkpoint

trusted UTC vorhanden
    -> bestehenden Startpfad normal fortsetzen
    -> erster persistierter Run-Checkpoint enthaelt UTC
```

Der Gatevertrag ist unabhaengig davon, ob die trusted UTC aus RTC oder NTP
stammt. #124 bleibt unveraendert; insbesondere werden keine bestehenden
Recoveryfelder, Formeln, Zustandsuebergaenge oder UTC-losen Alt-Checkpoints
nachtraeglich umgeschrieben.

### 4.4 Board- und I2C-Audit

Es gibt aktuell kein bestaetigtes Boardprofil mit RTC-Konfiguration:

- `config/hardware.example.yaml` beschreibt RTC bisher als
  `required: false`, `status: FUTURE_RELEASE`.
- `config/pins.example.yaml` enthaelt nur Rollen und `TBD_HARDWARE`; es gibt
  weder RTC-SDA/SCL noch einen bestaetigten I2C-Bus.
- `docs/HARDWARE.md` bestaetigt weder konkrete GPIOs noch eine Boardrevision.

Der Plan darf daher keine Zahlenwerte erfinden. Die Umsetzung muss ein
semantisches Profil mit dieser Form unterstuetzen, sobald das kanonische
Boardprofil und seine Pin-/Busnachweise vorliegen:

```yaml
rtc:
  present: <verified boolean>
  type: DS3231SN
  bus: <verified I2C bus>
  sda: <verified board-profile pin>
  scl: <verified board-profile pin>
  address: 0x68
```

`RTC_DEVICE=DS3231SN`, `RTC_FAMILY=DS3231` und
`RTC_I2C_ADDRESS=0x68` sind die Hardwareentscheidungen. Bus, SDA und SCL
bleiben bis zum Owner-Hardwareprofil offen. Der I2C-Bus wird als geteilter Bus
geplant, nicht als DS3231SN-Sonderbus. Bei Adoption von `ds3231` + `i2cdev` ist
`i2cdev` der
kanonische I2C-Port-/Device-Lebensdauerowner mit seinem per-Port-Bus-Handle,
Mutex-, Device-Registry- und Reference-Count-Vertrag:

```text
I2C_PORT_LIFETIME_OWNER=I2CDEV
I2CDEV_GLOBAL_INIT_OWNER=DEVICE_PLATFORM_ESP_IDF_COMPOSITION_LIFETIME
SECOND_I2C_MASTER_BUS_OWNER_ON_SAME_PORT=NO
SHARED_I2C_BUS=YES
```

Die Device-Platform-/Composition-Root besitzt, wann der I2C-Subsystem-
Lebenszyklus gestartet und beendet wird, erzeugt aber fuer denselben Port
keinen zweiten konkurrierenden `i2c_master_bus_handle_t`. Spaetere I2C-
Geraete benutzen denselben kanonischen Bus-/Handle-Vertrag. Ein fremder
ESP-IDF-Adapter verwendet, falls erforderlich, den von `i2cdev` vorgesehenen
Shared-Bus-/Handle-Weg. Unterschiedliche SDA-/SCL-Anforderungen fuer denselben
Port werden abgewiesen; der Bus wird nicht still umkonfiguriert.

Wenn der feste ESP-IDF-6.0.2-Spike keinen sauberen Shared-Bus-/Lifecycle-
Vertrag erlaubt oder einen zweiten Busowner erfordern wuerde, gilt:

```text
I2C_LIBRARY_ADOPTION_ABORT_GATE=STOP_LIBRARY_ADOPTION_GATE
```

Es wird dann weder ein zweiter Bus erzeugt noch die Architektur still
verbogen; stattdessen erfolgt eine begruendete Alternativpruefung mit neuer
Planrevision. Der DS3231SN-Adapter darf keine bereits belegten Busressourcen
ueberschreiben.

`rtc.present=false` ist ein gueltiges Produktprofil. Es erzeugt keinen
RTC-/I2C-Fehler und waehlt den NTP-only-Betriebsmodus. `rtc.present=true`
aktiviert nur die konfigurierte DS3231SN-Pruefung; fehlende oder untrusted RTC
bleibt ein fail-closed Zeitquellenzustand.

Die fruehere vorlaeufige DS3231M-Bezeichnung ist durch die konkrete
HW-084-Modulwahl DS3231SN verworfen und kein R1-Variantenvertrag. R1 baut keine
Multi-RTC-Variantenschicht; ein spaeteres anderes DS3231-kompatibles
Boardprofil waere ein separates Thema.

### 4.5 Audit des recoverbaren Current-FERMENTING-Schreibpfads

Der bestehende Checkpointvertrag traegt UTC optional, obwohl #124 fuer die
Recovery eines aktiven `FERMENTING`-Current einen UTC-tragenden Current
benoetigt. Ein neuer Run wird durch das Startgate nur beim ersten Start
geschuetzt; eine spaetere Trust-Luecke koennte ohne weiteren Guard einen
gueltigen Current A durch einen UTC-losen Current B ersetzen. #124 darf einen
frueheren Current nicht automatisch aus Fallback oder Historie hochstufen.

Der Zielvertrag fuer neue Writes ist deshalb:

```text
IF Current.variant IN {ProgramRun, ManualRun}
AND Current.processState == Fermenting
THEN Current.utcUnixSeconds MUST be present
```

Die Pruefung muss den resultierenden Snapshot und die zu speichernde
`RunCheckpointTime` gemeinsam sehen. Sie liegt in der
`fermentation_app`-/Persistence-Domain-Grenze unterhalb von UI und einzelnen
Orchestrator-Aufrufern, bleibt aber ausserhalb von `device_platform`,
DS3231SN-Adapter und SNTP-Adapter. Sie erfolgt vor dem durable Write und, fuer
Write-before-Apply-Pfade, vor dem RAM-Apply.

Bei `checkpointPeriodic(...)` wird bei Faelligkeit und fehlender trusted UTC
ohne jede dauerhafte oder RAM-seitige Mutation ein ehrlicher bestehender
Resultstatus verwendet. Der genaue Statusname (`NotDue`, `NotEligible`,
`Blocked` oder ein gleichwertiger vorhandener Status) wird im
Umsetzungs-Audit anhand der vorhandenen Semantik gewaehlt; fuer den
Trust-Fehler wird kein unnoetiger neuer Persistenzstatus erfunden. Der
Schedule wird nicht bestaetigt, weil kein Checkpoint erfolgreich geschrieben
wurde. Der letzte gueltige Current, Slot und Fallback bleiben unangetastet.

Bei einer persistierten Command-/Automatikmutation, deren Ziel weiterhin ein
aktiver `FERMENTING`-Current ist, wird ohne trusted UTC vor Persistenz und
RAM-Apply abgelehnt oder deferiert. Terminale oder sonstige gueltige
Non-`FERMENTING`-Ziele werden nicht allein durch diesen Guard blockiert. Die
Anwendung erfindet keine UTC aus monotonic time, schreibt keinen kleineren
Ersatzwert und korrigiert keine gespeicherte Historie rueckwirkend.

Historische UTC-lose Records bleiben lesbar. #124 behandelt sie bei einem
Recoveryversuch weiterhin mit seinem bestehenden fail-closed-Vertrag; es gibt
keine Fallback-Auto-Promotion und keine Aenderung der #124-Formel oder
Recovery-Disposition.

## 5. Zielvertrag der Zeitplattform

### 5.1 Trust-Gate fuer RTC

Der DS3231SN-Wert darf nur dann als trusted absolute Zeit in die Systemzeit und
`ITimeSource` gelangen, wenn alle folgenden Nachweise in derselben validierten
Leseoperation vorhanden sind:

```text
I2C_DEVICE_REACHABLE=YES
REGISTER_READ_VALID=YES
CALENDAR_VALUE_VALID=YES
OSF_OSCILLATOR_STOP_FLAG=0
EOSC=0
```

`CALENDAR_VALUE_VALID` umfasst mindestens rohe Registerwerte oder eine
gleichwertig beweiskraeftige Validierung: valide BCD-Nibbles, Sekunden 0..59,
Minuten 0..59, gueltiges 12-/24-Stunden-Encoding, valide Monat/Tag-
Kombination, Monatslaenge/Schaltjahr und den R1-Bereich 2000..2099. Es duerfen
keine reservierten oder inkonsistenten Bits akzeptiert werden. Die
UTC-Konvertierung muss timezonefrei und checked erfolgen. Der Adapter darf
die von der Bibliothek gelieferte `struct tm` nicht ungeprueft als
Kalenderbeweis uebernehmen.

`EOSC=0` ist fuer den Battery-Backup-Vertrag erforderlich: Ein Zustand, in dem
EOSC den Oszillator auf VBAT anhalten wuerde, ist nicht fuer
Power-Loss-Recovery korrekt konfiguriert. Der ungenutzte EN32kHz-Ausgang wird
in R1 deaktiviert; SQW/INT bleiben ungenutzt. Es werden keine willkuerlichen
Produktgrenzen wie lokale Sommerzeit oder Benutzerzeitzone in den RTC-Trust
eingebaut.

`OSF=1` bedeutet zwingend `RTC_TIME_TRUSTED=NO`. Das Flag wird nicht nur
geloescht, um einen vorhandenen Wert nachtraeglich vertrauenswuerdig aussehen
zu lassen.

Der R1-Health-/Trust-Vertrag ist fuer DS3231SN nach dem DS3231-Datenblatt zu
pruefen. Die benoetigten Kernregister sind:

```text
DS3231SN Control 0x0E: EOSC = bit 7; RS2 = bit 4; RS1 = bit 3
DS3231SN Status  0x0F: OSF = bit 7; EN32KHZ = bit 3
DS3231SN I2C address: 0x68
RAW_REGISTER_VALIDATION_SOURCE=ANALOG_DEVICES_DS3231_DATASHEET
RAW_REGISTER_VALIDATION_VARIANT=DS3231_DS3231SN
DS3231_RS_BITS_R1_FUNCTIONAL_USE=NO
```

Die DS3231-RS1/RS2-Bits werden als ungenutzte, aber zu bewahrende Control-
Registerbits behandelt. Der Health-Shim darf sie bei Read/Modify/Write nicht
unbeabsichtigt veraendern. Die Library-dekodierte
`struct tm` allein bleibt kein Rohregisterbeweis.

### 5.2 Systemzeit und bestehender Port

Die konkrete ESP32-Zeitquelle behaelt die bisherige monotone Baseline. Sie
fuehrt intern nur einen kleinen Trust-Zustand und ein bootlokales
`lastPublishedTrustedUtc`-High-Water-Gate fuer die absolute Systemzeit:

```text
untrusted boot / keine gueltige Quelle -> unixTimeSeconds() == nullopt
trusted RTC seed                    -> gettimeofday() als UTC publizierbar
trusted NTP sync                    -> System-UTC publizierbar
Systemzeit < lastPublishedTrustedUtc -> nullopt, keine retrograde Publikation
echter inkonsistenter Zustand       -> Trust verlieren, nullopt, fail closed
```

Der Trust-Zustand ist kein neuer App-Port und wird nicht durch die Anwendung
gesetzt. Nur der konkrete Plattformkoordinator darf ihn nach abgeschlossenem
RTC-/NTP-Nachweis aktualisieren. Solange die gelesene Systemzeit mindestens
dem High-Water entspricht, darf sie publiziert und der High-Water aktualisiert
werden. Ein beobachteter Wert darunter wird nicht als kleinerer trusted Wert
ausgegeben; der absolute Publikationspfad liefert `nullopt`. Zeitzone/DST
bleiben im vorhandenen `ITimeZoneResolver` und in der Anzeigeebene; die RTC
speichert und liest UTC. Ein High-Water-Gate ist keine zweite Uhr und kein
fachlicher Recoveryzustand.

### 5.3 Bootsequenz

Nach Ownerfreigabe wird folgende Reihenfolge implementiert:

1. Das Profil lesen. Bei `rtc.present=false` wird kein RTC-/I2C-Fehler erzeugt;
   der Adapter startet im gueltigen NTP-only-Modus. Bei `true` werden
   ESP-IDF-/I2C-Bus und DS3231SN-Adapter gemaess validiertem Boardprofil
   vorbereitet.
2. Bei `rtc.present=true` DS3231SN erreichen, rohe Control-/Statusregister und
   Kalender lesen, EOSC/EN32kHz/OSF sowie den Kalendervertrag validieren.
3. Bei vollstaendig trusted RTC: UTC in die ESP-Systemzeit setzen, den
   bestehenden `ITimeSource`-Trust setzen und den High-Water initialisieren.
4. Bei fehlender, nicht erreichbarer oder untrusted RTC: keine absolute Zeit
   publizieren; `unixTimeSeconds()` bleibt `nullopt`. NTP-only bleibt gueltig.
5. Die Anwendung wird mit demselben `ITimeSource` gestartet. #124 bleibt bei
   fehlender trusted UTC in `WaitingForTrustedTime`; sofortige Offline-Recovery
   ist nur mit trusted RTC moeglich.
6. Ein spaeteres NTP-Sync-Ereignis laeuft ueber die ESP-IDF-SNTP-Policy. Bei
   `IN_PROGRESS` wird nicht in die RTC geschrieben. Erst `COMPLETED` liest die
   konvergierte System-UTC, schreibt die RTC und schliesst deren Trust ab.
7. Das bestehende `FermentationApplication::update()` reevaluert #124
   automatisch; der Zeitadapter startet keinen zweiten Recoverypfad.

Ein untrusted Boot erzeugt weder einen provisionalen Resume noch einen
Operator-Resume vor trusted Zeit. Ohne RTC und ohne NTP bleibt #124
fail-closed in `WaitingForTrustedTime`; ein vorhandenes, aber untrusted RTC
liefert ebenfalls keine absolute Zeit.

### 5.4 NTP-Vertrag und #89-Grenze

Es wird die offizielle ESP-IDF-SNTP-Huelle verwendet, bevorzugt
`esp_netif_sntp_*`. #126 besitzt den app-neutralen SNTP-Client-/Lifecycle- und
Serverkonfigurationsvertrag; #89 besitzt Connectivity. Der Zeitbaustein
besitzt:

- keinen WLAN-Treiber, kein Credentialmanagement und keine Provisionierung;
- keine eigene Connectivity-, Credential- oder Onboarding-Policy; die
  konkrete Server-/DHCP-Policy wird ueber den #126-Konfigurationsvertrag
  eingespeist und bleibt frei von Cloud-/Accountzwang;
- einen klaren Start-/Sync-Hook, den der spaetere Connectivity-Besitzer
  aufrufen kann;
- einen Sync-Callback oder ein Sync-Ereignis, das die Systemzeit-/Trust- und
  RTC-Nachfuehrung ueber den konkreten Plattformkoordinator ausloest.

NTP darf:

```text
- die ESP-Systemzeit setzen oder korrigieren;
- untrusted RTC nach erfolgreicher Synchronisierung initialisieren;
- eine trusted RTC periodisch nachfuehren.
```

NTP darf nicht direkt `fermentation_app` aufrufen, Recoveryzustande
entscheiden oder Aktoren freigeben. Ein echter Netzwerk-/NTP-Lauf bleibt
abhängig von #89 und wird in diesem PR nicht ausgefuehrt.

```text
NTP_SERVER_CONFIGURATION_OWNER=ISSUE126_TIME_PLATFORM
NETWORK_CONNECTIVITY_OWNER=ISSUE89
```

### 5.5 Trusted-Time-Startgate fuer neue produktive Runs

Ein neuer produktiver Run darf erst beginnen, wenn der bestehende
`device_platform::ITimeSource`-Port eine trusted absolute UTC liefert:

```text
NEW_PRODUCTIVE_RUN_START_REQUIRES_TRUSTED_UTC=YES
FRONTEND_ONLY_TIME_GATE=NO
APPLICATION_DOMAIN_START_GATE=YES
NEW_RUN_CAN_CREATE_UTC_LESS_RECOVERY_ANCHOR=NO
```

Die Application prueft dafuer ausschliesslich
`ITimeSource::unixTimeSeconds().has_value()`. Sie kennt weder RTC noch NTP und
wertet auch nicht aus, welche Quelle die UTC etabliert hat. Das Gate liegt am
gemeinsamen Domain-/Application-Startpfad, konkret vor der ersten produktiven
Start-Persistenz und vor dem Apply; der vorhandene
`TemperatureControlApplicationOrchestrator::persistFreshStartCommand(...)`
ist dafuer der bevorzugte Integrationspunkt, sofern der vollstaendige
Umsetzungs-Audit keinen noch frueheren gemeinsamen Startpfad nachweist.

Ohne trusted UTC wird der neue Start abgelehnt, ohne aktive Run-Mutation und
ohne Checkpoint. Der Ablehnungsgrund wird ueber den vorhandenen
Command-/Result-Vertrag sinngemaess als `TrustedAbsoluteTimeRequired`
projiziert; der konkrete Typname folgt bestehenden Konventionen. Das Gate ist
auch bei einem direkten Domain-/Application-Command wirksam. Ein spaeteres UI
darf den Grund und den deaktivierten Start anzeigen, ist aber nicht die
Autoritaet.

Die Betriebsmodi sind damit:

```text
rtc.present=false + NTP nicht trusted
    -> Boot, Konfiguration und Standby moeglich
    -> neuer produktiver Run abgewiesen

rtc.present=false + NTP trusted
    -> NTP-only-Start moeglich
    -> erster committed Run-Checkpoint enthaelt UTC

trusted RTC beim Boot
    -> Start ohne Netzwerk-/NTP-Wartezeit moeglich

RTC vorhanden, aber untrusted, + NTP nicht trusted
    -> unixTimeSeconds() == nullopt
    -> neuer produktiver Run abgewiesen
```

Das Gate verhindert nur neue nicht rekonstruierbare Anker. Es aendert weder
`priorBootPhaseElapsed`, `observedRunSeconds`,
`wall_clock_since_checkpoint_seconds`, `RecoveryDisposition`, die bestehende
`WaitingForTrustedTime`-Reevaluation noch die FSM-Recoverytopologie. Alte
UTC-lose Checkpoints werden nicht nachtraeglich repariert.

### 5.6 Schreibschutz fuer den recoverbaren Current-FERMENTING-Zustand

Das Startgate schuetzt den ersten Checkpoint eines neuen Runs. Zusaetzlich
gilt fuer jeden neuen Write, der einen aktiven Current repraesentiert:

```text
R1_CURRENT_FERMENTING_CHECKPOINT_REQUIRES_TRUSTED_UTC=YES
UTC_LESS_CURRENT_FERMENTING_COMMIT=FORBIDDEN
LAST_VALID_UTC_CURRENT_RETAINED_ON_TIME_TRUST_LOSS=YES
```

Der resultierende Snapshot ist die Entscheidungsgrundlage. Wenn

```text
Current.variant IN {ProgramRun, ManualRun}
AND Current.processState == Fermenting
```

gilt, muss die zu speichernde `RunCheckpointTime.utcUnixSeconds` vorhanden
sein. Die gemeinsame Guard-Pruefung sieht deshalb den Ziel-Snapshot und die
aktuelle Checkpoint-Zeit zusammen und liegt an der tiefsten gemeinsamen
`fermentation_app`-/Persistence-Domain-Grenze, die vor dem durable Write
erreicht wird. Sie darf nicht nur im UI, in einem einzelnen Orchestrator,
  `device_platform`, dem DS3231SN-Adapter oder dem SNTP-Adapter liegen.

Fuer `checkpointPeriodic(...)` gilt bei aktivem `FERMENTING`, faelligem
Schedule und `unixTimeSeconds()==nullopt`:

```text
PERIODIC_FERMENTING_CHECKPOINT_WITHOUT_TRUSTED_UTC=SKIP_NO_WRITE
CURRENT_HEAD_MUTATION=NO
CHECKPOINT_SLOT_MUTATION=NO
FALLBACK_MUTATION=NO
RUN_RAM_MUTATION=NO
LAST_VALID_CURRENT_RETAINED=YES
NO_DURABLE_MUTATION
```

Der vorhandene ehrliche Resultstatus wird nach Ist-Audit gewaehlt
(`NotDue`, `NotEligible`, `Blocked` oder gleichwertig); fuer diese
Planentscheidung wird kein neuer Persistenzstatus erfunden. Der ausgelassene
Checkpoint bestaetigt den Schedule nicht:

```text
SKIPPED_FOR_MISSING_TRUSTED_UTC_DOES_NOT_CONFIRM_CHECKPOINT_SCHEDULE=YES
```

Der Guard muss vor einer moeglichen Kandidaten-/Live-Evidence-Mutation
greifen. Sobald trusted UTC wieder verfuegbar ist, darf der naechste regulär
faellige Checkpoint normal geschrieben und der Schedule bestaetigt werden.

Bei einem Command oder einer automatischen/persistierten Mutation, deren
erfolgreiches Apply einen neuen weiterhin aktiven `FERMENTING`-Current
durable machen wuerde, gilt:

```text
FERMENTING_MUTATION_RESULTING_IN_FERMENTING_REQUIRES_TRUSTED_UTC=YES
NO_TRUSTED_UTC=REJECT_OR_DEFER_BEFORE_PERSISTENCE_AND_RAM_APPLY
FERMENTING_MUTATION_WITHOUT_TRUSTED_UTC_RAM_APPLY=NO
```

Write-before-Apply bleibt der bestehende Ablauf. Der neue Guard wird nach der
Zielprojektion, aber vor Persistenz und RAM-Apply ausgefuehrt. Ein bestehender
RAM-only-Pfad ohne neuen durable Current wird nicht durch eine globale
UTC-Pflicht umdefiniert. Stop/Abort zu `NoActiveRun`, terminale/sichere
Non-`FERMENTING`-Ziele und andere bestehende gueltige Nicht-Recovery-Ziele
bleiben erlaubt, sofern kein anderer bestehender Vertrag UTC verlangt:

```text
ALL_PERSISTENCE_REQUIRES_UTC=NO
TERMINAL_NO_ACTIVE_RUN_PATHS_BLOCKED_BY_NEW_GUARD=NO
```

Es gibt keine synthetische UTC aus monotonic time, keine zweite
`last UTC + monotonic`-Uhr, keinen retroaktiven Checkpoint- oder
Recovery-Rewrite und keine automatische Fallback-Promotion. Historische
Records ohne UTC bleiben lesbar; der bestehende #124-Read-/Recoveryvertrag
entscheidet weiterhin fail-closed.

## 6. Vollstaendige RTC/NTP-Arbitration

Die folgenden Punkte sind der verpflichtende Entscheidungs- und Testumfang.
Die Ownerentscheidungen aus dieser Planrevision sind verbindlich festgelegt;
keine der KISS-Entscheidungen bleibt als offene Delta-Frage bestehen.

### 6.1 Gueltige RTC und spaeteres NTP

```text
CURRENT_STATE=
  Kein RTC-/NTP-Producer im aktuellen Repository. #124 besitzt bereits den
  einzigen fachlichen Re-Evaluationspfad ueber ITimeSource.

MINIMAL_KISS_OPTION=
  Nach einem erfolgreichen NTP-Sync die NTP-UTC als Netzwerkreferenz fuer die
  Systemzeit behandeln und die RTC erst nach dem abgeschlossenen Sync in einem
  serialisierten Plattformpfad nachfuehren. MonotonicMillis bleibt unberuehrt.

RECOMMENDATION=
  NTP wird nach dem explizit freigegebenen Sync-Ereignis Referenz; der bereits
  trusted RTC-Wert wird nicht als zweiter fachlicher Zeitpfad weitergereicht.

WHY=
  Es gibt genau eine publizierte System-UTC und genau einen App-Port. Damit
  werden Quelle und Anwendung entkoppelt und kein Zeit-Consensus-Modell
  eingefuehrt.

OWNER_DECISION_REQUIRED=NO
```

### 6.2 Kleine RTC-NTP-Abweichung

```text
CURRENT_STATE=
  Es gibt keine eigene Delta-Toleranz im Projekt. ESP-IDF-SNTP bietet Smooth-
  Sync fuer normale Korrekturen und kann bei einer dokumentierten sehr grossen
  Abweichung selbst auf Immediate-Step wechseln.

MINIMAL_KISS_OPTION=
  Normale Korrekturen ausschliesslich mit ESP-IDF-Smooth-Sync behandeln.
  Keine fachliche Sekunden-/Minuten-Schwelle und keine eigene Konsensus-
  Toleranz einfuehren.

RECOMMENDATION=
  SMALL_RTC_NTP_DELTA_POLICY=USE_ESP_IDF_SMOOTH_SYNC_FOR_NORMAL_CORRECTION.
  Die technische ESP-IDF-Grenze fuer den Wechsel zu Immediate-Step ist keine
  fachliche Definition von "klein".

WHY=
  Die Plattform nutzt den Herstellermechanismus, ohne eine neue
  Produktkonstante oder Zeit-Consensus-Engine zu erfinden. #124 wird durch
  eine normale Korrektur nicht fachlich erweitert.

OWNER_DECISION_REQUIRED=NO
```

### 6.3 Grosse RTC-NTP-Abweichung

```text
CURRENT_STATE=
  Eine eigene grosse-Delta-Erkennung oder Schwelle existiert nicht und wird
  nicht eingefuehrt. ESP-IDF darf seine dokumentierte Immediate-Step-Semantik
  selbst anwenden.

MINIMAL_KISS_OPTION=
  CUSTOM_LARGE_DELTA_THRESHOLD=NO und ESP_IDF_SMOOTH_SYNC=YES. Wenn ESP-IDF
  wegen einer sehr grossen Abweichung selbst sofort setzt, wird dieses
  Plattformverhalten akzeptiert und nicht nachgebaut.

RECOMMENDATION=
  ESP_IDF_DOCUMENTED_LARGE_DELTA_IMMEDIATE_STEP=ACCEPTED_PLATFORM_BEHAVIOR.
  Eine Vorwaertskorrektur darf als neue trusted Systemzeit publiziert werden,
  sofern das High-Water-Gate nicht verletzt ist. Eine Rueckwaertskorrektur
  unterhalb des letzten trusted Werts wird nicht publiziert.

WHY=
  Es gibt eine System-UTC, einen ITimeSource-Port und keinen zweiten Delta-
  Algorithmus. Diagnose darf den Konflikt sichtbar machen, aber keine neue
  Application-Recovery erfinden.

OWNER_DECISION_REQUIRED=NO
```

### 6.4 Rueckwaertszeit und Publikationsgate

```text
CURRENT_STATE=
  POSIX-/ESP-Systemzeit kann durch settimeofday beziehungsweise SNTP
  korrigiert werden. #124 verwirft negative UTC-Deltas; der Zeitadapter besitzt
  bisher noch kein bootlokales lastPublishedTrustedUtc-Gate.

MINIMAL_KISS_OPTION=
  MONOTONIC_MILLIS_RETROGRADE=NEVER. Nach absoluter Vertrauensetablierung
  speichert der Adapter den letzten publizierten trusted UTC-Wert als
  High-Water. Nur Systemzeit >= High-Water wird publiziert; kleinere Werte
  ergeben std::nullopt.

RECOMMENDATION=
  TRUSTED_ITIME_SOURCE_RETROGRADE_PUBLICATION=NO. Ein real beobachteter
  Ruecksprung unter den High-Water verliert die absolute Publizierbarkeit fuer
  diesen Gatepfad; kein kleinerer Wert wird nach aussen gegeben. Der
  High-Water bleibt bootlokal. Es gibt keine Persistenz-/Checkpointkorrektur,
  keine FSM-Sonderlogik und keine Reconciliation-Engine.

WHY=
  MonotonicMillis bleibt vollstaendig unabhaengig. Das Gate schuetzt den
  bestehenden UTC-Vertrag mit minimalem Zustand und verhindert, dass ein
  retrograder Wert Recovery-/Checkpointsemantik verfalscht.

OWNER_DECISION_REQUIRED=NO
```

### 6.5 Grosse Delta-Recovery

```text
CURRENT_STATE=
  #124 verwendet nur den bestehenden trusted UTC-Eingang und seine
  unveraenderten Checkpoint-/Recoveryregeln. Ein SNTP-Ereignis darf keinen
  direkten FSM-Handoff ausloesen.

MINIMAL_KISS_OPTION=
  LARGE_DELTA_RECOVERY_POLICY=NO_RETROACTIVE_RECOVERY_ENGINE. Bei
  WaitingForTrustedTime fuehrt ein spaeter trusted NTP-Wert die normale
  bestehende Reevaluation ueber application.update() aus.

RECOMMENDATION=
  Nach bereits erfolgtem logischem Recovery aus trusted RTC werden weder
  priorBootPhaseElapsed noch observedRunSeconds, wall_clock_since_checkpoint_seconds,
  Persistenz-Checkpoint-Historie oder aktive monotone FSM-Timer rueckwirkend
  umgeschrieben. Kein zweites Recovery. Bei retrograder UTC greift nur das
  Publikationsgate aus 6.4: nullopt/fail-closed fuer absolute UTC.

WHY=
  Die aktive Prozesszeit bleibt auf der etablierten monotonen Basis. Die
  seltene RTC/NTP-Konfliktlage bleibt damit fail-closed und lokal begrenzt,
  ohne die #124-Fachsemantik zu duplizieren.

OWNER_DECISION_REQUIRED=NO
```

### 6.6 RTC-Schreibfrequenz und Smooth-Completion

```text
CURRENT_STATE=
  Kein RTC-Write und keine NTP-Eventverarbeitung vorhanden. Ein Sync-Event
  kann bei Smooth-Sync eintreffen, waehrend die Systemzeit noch konvergiert;
  deshalb ist ein Event nicht automatisch ein abgeschlossener Sync.

MINIMAL_KISS_OPTION=
  RTC_SYNC_WRITE_FREQUENCY=AFTER_EVERY_COMPLETED_NTP_SYNCHRONIZATION. Ein
  `IN_PROGRESS`-Status schreibt nicht. Nach `COMPLETED` liest der asynchrone
  Plattformkoordinator die konvergierte System-UTC, schreibt die RTC, liest
  sie zurueck, validiert Kalender/Control-/Statusregister und schliesst erst
  danach den OSF-Vertrag.

RECOMMENDATION=
  RTC_WRITE_DURING_SMOOTH_IN_PROGRESS=NO und
  RTC_WRITE_AFTER_SYNC_COMPLETED=YES. Kein blockierendes Warten im
  Application-Boot und keine eigene RTC-Periodik neben esp_netif_sntp_*.

WHY=
  Der RTC-Wert wird nicht mit einer Zwischenzeit beschrieben. Ein Write pro
  abgeschlossenem NTP-Sync ist nachvollziehbar und aktualisiert den
  Batteriezeitanker ohne Flash- oder Consensus-Engine.

OWNER_DECISION_REQUIRED=NO
```

### 6.7 Fehlgeschlagener RTC-Write

```text
CURRENT_STATE=
  Es gibt keine Fehlerabbildung fuer RTC-/I2C-Fehler. #124 unterscheidet
  trusted UTC nur ueber ITimeSource und besitzt keinen RTC-Status.

MINIMAL_KISS_OPTION=
  Die erfolgreich etablierte NTP-Systemzeit bleibt fuer den aktuellen Boot
  trusted. Ein fehlgeschlagener RTC-Write macht nur den RTC-Stand fuer
  zukuenftige Boots untrusted/unsynchronisiert; OSF wird nicht geloescht. Der
  Write wird beim naechsten abgeschlossenen NTP-Sync erneut versucht.

RECOMMENDATION=
  RTC-Fehler sichtbar diagnostizieren, aber nicht in einen Application-
  Recoverystatus umdeuten. Ohne spaeteren NTP bleibt ein naechster Boot ohne
  trusted RTC in WaitingForTrustedTime.

WHY=
  Ein RTC-Schreibfehler ist keine Evidenz gegen die aktuell validierte NTP-
  Systemzeit und darf sie nicht unnoetig entwerten. Der unbestaetigte RTC darf
  zugleich nicht fuer Offline-Recovery freigegeben werden.

OWNER_DECISION_REQUIRED=NO
```

### 6.8 DS3231SN-OSF-, EOSC- und EN32kHz-Vertrag

```text
CURRENT_STATE=
  Die bestehende esp-idf-lib/ds3231-Komponente bietet OSF-Zugriff und die
  Deaktivierung des 32-kHz-Ausgangs, aber keine eindeutige oeffentliche
  EOSC-Read/Write-API im geprueften Header. Eine Set-/Readback-/OSF-
  Transaktion gegen die DS3231SN-Hardware ist noch nicht implementiert.

MINIMAL_KISS_OPTION=
  NTP setzt eine UTC in die DS3231SN, liest Zeit und rohe Control-/Statusregister
  zurueck und validiert sie gegen das DS3231-Datenblatt. Erst danach wird OSF
  geloescht und anschliessend erneut gelesen. Nur OSF=0 und EOSC=0 schliessen
  den RTC-Trust. EN32KHZ wird deaktiviert; SQW/INT bleiben ungenutzt.

RECOMMENDATION=
  DS3231SN_EOSC_REQUIRED=0 und DS3231SN_32KHZ_OUTPUT_R1=DISABLED. Die
  Control-0x0E-Bits RS2/RS1 werden nicht funktional verwendet, aber bei
  Read/Modify/Write bewahrt. Fehler bei Setzen, Readback,
  Kalender-/Controlvalidierung oder OSF-Clear lassen RTC-Trust untrusted; kein
  Clear-first und kein stiller Erfolg.

WHY=
  OSF=1 beweist untrusted RTC. EOSC darf den Battery-Backup-Oszillator nicht
  am Weiterlaufen hindern, und der ungenutzte 32-kHz-Ausgang darf keine
  unbeabsichtigte R1-Abhaengigkeit erzeugen.

OWNER_DECISION_REQUIRED=NO
```

### 6.9 Optionale und untrusted RTC

```text
CURRENT_STATE=
  Es gibt noch kein bestaetigtes Hardwareprofil. `rtc.present=false` ist
  jedoch ein gueltiger Produktmodus und kein Fehler.

MINIMAL_KISS_OPTION=
  RTC_HARDWARE_OPTIONAL=YES und NTP_ONLY_MODE_SUPPORTED=YES. Bei
  `rtc.present=false` wird kein RTC-/I2C-Fehler erzeugt. Bei fehlender,
  unerreichbarer oder untrusted RTC liefert der Port keine UTC und #124 bleibt
  bis NTP in WaitingForTrustedTime/all-off.

RECOMMENDATION=
  RTC_REQUIRED_FOR_IMMEDIATE_OFFLINE_RECOVERY=YES. RTC vorhanden und trusted
  bedeutet UTC beim Boot und keine NTP-Wartezeit fuer #124. RTC absent bedeutet
  NTP-only. RTC vorhanden, aber OSF/EOSC/Kalender/Erreichbarkeit untrusted,
  bedeutet ebenfalls auf NTP warten. Kein provisionaler Operator-Resume.

WHY=
  Optionalitaet betrifft Hardwareausstattung, nicht die Vertrauensanforderung.
  Batterie, Versorgung und Oszillatorzustand sind ohne den vollstaendigen
  Trust-Gate kein Offline-Zeitbeweis.

OWNER_DECISION_REQUIRED=NO
```

### 6.10 Trusted-Time-Startgate

```text
CURRENT_STATE=
  RunCheckpointTime.utcUnixSeconds ist optional. Der aktuelle gemeinsame
  Start-/Persistenzpfad kann deshalb ohne zusaetzliches Gate einen neuen
  Checkpoint ohne UTC anlegen, obwohl #124 diesen spaeter nicht exakt
  rekonstruieren kann.

MINIMAL_KISS_OPTION=
  ITimeSource::unixTimeSeconds().has_value() am gemeinsamen
  Domain-/Application-Startgate pruefen. Bei false keine Startmutation und
  keinen Checkpoint erzeugen; bei true den bestehenden Startpfad unveraendert
  fortsetzen.

RECOMMENDATION=
  NEW_PRODUCTIVE_RUN_START_REQUIRES_TRUSTED_UTC=YES,
  FRONTEND_ONLY_TIME_GATE=NO und APPLICATION_DOMAIN_START_GATE=YES. Der
  Grund wird ueber den bestehenden Command-/Result-Vertrag projiziert,
  sinngemaess TrustedAbsoluteTimeRequired. RTC-/NTP-Herkunft bleibt fuer die
  Application unsichtbar.

WHY=
  NTP-only bleibt ein vollwertiger Betriebsmodus, aber ein neuer Run erzeugt
  keinen UTC-losen Recoveryanker. Das Gate ist fail-closed und vermeidet eine
  spaetere Recovery-Sonderlogik, ohne #124 zu aendern.

OWNER_DECISION_REQUIRED=NO
```

### 6.11 Ehrliches `nullopt` und Source-Verlust

```text
CURRENT_STATE=
  Der Port hat die optionale Rueckgabe bereits; der ESP-IDF-Adapter liefert
  derzeit immer nullopt und besitzt noch kein Trust-/High-Water-Gate.

MINIMAL_KISS_OPTION=
  Trust wird nur nach validiertem RTC-Seed oder abgeschlossenem NTP-Sync
  etabliert. WIFI_LOSS_AFTER_SUCCESSFUL_NTP_SYNC_INVALIDATES_CURRENT_BOOT_TIME=NO,
  NTP_SERVER_UNREACHABLE_AFTER_SUCCESSFUL_SYNC_INVALIDATES_CURRENT_BOOT_TIME=NO
  und RTC_UNREACHABLE_AFTER_SUCCESSFUL_BOOT_SEED_INVALIDATES_CURRENT_BOOT_TIME=NO.

RECOMMENDATION=
  `unixTimeSeconds()` publiziert trusted System-UTC solange der Wert gueltig
  ist und nicht unter `lastPublishedTrustedUtc` liegt. Trust/Publizierbarkeit
  geht nur bei nie etabliertem Trust, fehlgeschlagener RTC-Validierung vor dem
  Seed, echter High-Water-Retrograde, ungueltiger Systemzeitkonvertierung oder
  explizit inkonsistentem Clock-Zustand verloren. Kein kleinerer trusted Wert,
  keine Quelleinformation in der Application und keine zweite API.

WHY=
  Lokale Systemzeit laeuft nach erfolgreicher absoluter Etablierung auch ohne
  Netzwerk oder RTC weiter. Reiner Source-/Connectivity-Verlust ist keine
  Evidenz gegen die aktuelle Bootzeit.

OWNER_DECISION_REQUIRED=NO
```

### 6.12 Shared-I2C-Ownership

```text
CURRENT_STATE=
  Der esp-idf-lib/ds3231-Kandidat verwendet esp-idf-lib/i2cdev. Der bisherige Plan
  beschrieb einen geteilten I2C-Lebensdauerowner, wies aber nicht explizit zu,
  ob i2cdev oder die Plattform den Bus auf einem Port erzeugt.

MINIMAL_KISS_OPTION=
  Bei Adoption besitzt i2cdev den per-Port Bus-/Device-Lifecycle. Die
  device_platform_esp_idf-Composition Root besitzt nur den globalen
  I2C-Subsystem-Lebenszyklus und erzeugt auf demselben Port keinen zweiten
  i2c_master_bus_handle_t. Spaetere Devices teilen den kanonischen Bus.

RECOMMENDATION=
  I2C_PORT_LIFETIME_OWNER=I2CDEV,
  I2CDEV_GLOBAL_INIT_OWNER=DEVICE_PLATFORM_ESP_IDF_COMPOSITION_LIFETIME,
  SECOND_I2C_MASTER_BUS_OWNER_ON_SAME_PORT=NO und SHARED_I2C_BUS=YES.
  Unterschiedliche SDA-/SCL-Werte fuer denselben Port werden ohne stille
  Reconfiguration abgewiesen.

WHY=
  Ein einziger Busowner verhindert konkurrierende Masterhandles, doppelte
  Loeschung und Use-after-free, waehrend mehrere Geraete den Bus teilen
  koennen. Wenn der v6.0.2-Spike diesen Vertrag nicht erfuellt, wird die
  Kandidatenadoption abgebrochen statt die Ownership zu duplizieren.

OWNER_DECISION_REQUIRED=NO
```

### 6.13 Current-FERMENTING-Guard bei Trust-Luecke

```text
CURRENT_STATE=
  RunCheckpointTime.utcUnixSeconds ist optional und wird vom bestehenden
  ITimeSource unveraendert uebernommen. Nach einem trusted Start koennte ein
  periodischer oder persistierter FERMENTING-Write bei spaeterem nullopt den
  recoverbaren Current ersetzen. #124 kann den frueheren Current nicht
  automatisch hochstufen.

MINIMAL_KISS_OPTION=
  Den resultierenden Snapshot an der gemeinsamen
  fermentation_app-/Persistence-Domain-Grenze gegen die aeussere
  RunCheckpointTime pruefen. Aktiver ProgramRun-/ManualRun-Current im Zustand
  Fermenting verlangt UTC. Periodic ohne UTC wird vor Kandidaten-/Live-
  Evidence-Mutation und vor jedem Write uebersprungen; persistierte
  Fermenting-Mutationen werden vor Persistenz und RAM-Apply abgelehnt oder
  deferiert. Nicht-Fermenting-/NoActiveRun-Ziele behalten ihre bestehende
  Persistenzsemantik.

RECOMMENDATION=
  R1_CURRENT_FERMENTING_CHECKPOINT_REQUIRES_TRUSTED_UTC=YES,
  UTC_LESS_CURRENT_FERMENTING_COMMIT=FORBIDDEN und
  LAST_VALID_UTC_CURRENT_RETAINED_ON_TIME_TRUST_LOSS=YES. Fuer Periodic gilt
  SKIP_NO_WRITE ohne Head-, Slot-, Fallback- oder RAM-Mutation; der Schedule
  wird nicht bestaetigt. Fuer Mutationen gilt
  REJECT_OR_DEFER_BEFORE_PERSISTENCE_AND_RAM_APPLY. Der bestehende ehrliche
  Resultstatus wird nach Audit gewaehlt; kein neuer Status, keine synthetische
  UTC, kein retroaktiver Rewrite, keine Fallback-Auto-Promotion.

WHY=
  Ein aktiver FERMENTING-Current ohne UTC ist fuer den bestehenden #124-
  Recoveryvertrag nicht rekonstruierbar. Das Behalten des letzten gueltigen
  Current deckt die gesamte spaetere Wall-Clock-Zeit ab und bewahrt die
  bestehende fail-closed Recoverysemantik. Ein Guard am tiefen gemeinsamen
  Write-before-Apply-Punkt verhindert UI-Bypass und Teilmutationen, ohne
  device_platform mit Fermentationssemantik zu belasten oder Terminalpfade
  global zu blockieren.

OWNER_DECISION_REQUIRED=NO
```

### 6.14 Aufgeloeste Ownerentscheidungen

```text
OWNER_DECISIONS_REQUIRED=NONE
SMALL_RTC_NTP_DELTA_POLICY=USE_ESP_IDF_SMOOTH_SYNC_FOR_NORMAL_CORRECTION
CUSTOM_SMALL_DELTA_THRESHOLD=NO
BACKWARD_TIME_POLICY=NO_TRUSTED_RETROGRADE_PUBLICATION
LARGE_TIME_DELTA_POLICY=ACCEPT_ESP_IDF_DOCUMENTED_SYNC_BEHAVIOR
CUSTOM_LARGE_DELTA_THRESHOLD=NO
LARGE_DELTA_RECOVERY_POLICY=NO_RETROACTIVE_RECOVERY_ENGINE
RTC_SYNC_WRITE_FREQUENCY=AFTER_EVERY_COMPLETED_NTP_SYNCHRONIZATION
NEW_PRODUCTIVE_RUN_START_REQUIRES_TRUSTED_UTC=YES
FRONTEND_ONLY_TIME_GATE=NO
APPLICATION_DOMAIN_START_GATE=YES
NEW_RUN_CAN_CREATE_UTC_LESS_RECOVERY_ANCHOR=NO
R1_CURRENT_FERMENTING_CHECKPOINT_REQUIRES_TRUSTED_UTC=YES
UTC_LESS_CURRENT_FERMENTING_COMMIT=FORBIDDEN
LAST_VALID_UTC_CURRENT_RETAINED_ON_TIME_TRUST_LOSS=YES
PERIODIC_FERMENTING_CHECKPOINT_WITHOUT_TRUSTED_UTC=SKIP_NO_WRITE
SKIPPED_FOR_MISSING_TRUSTED_UTC_DOES_NOT_CONFIRM_CHECKPOINT_SCHEDULE=YES
FERMENTING_MUTATION_RESULTING_IN_FERMENTING_REQUIRES_TRUSTED_UTC=YES
FERMENTING_MUTATION_WITHOUT_TRUSTED_UTC=REJECT_OR_DEFER_BEFORE_PERSISTENCE_AND_RAM_APPLY
FERMENTING_MUTATION_WITHOUT_TRUSTED_UTC_RAM_APPLY=NO
ALL_PERSISTENCE_REQUIRES_UTC=NO
TERMINAL_NO_ACTIVE_RUN_PATHS_BLOCKED_BY_NEW_GUARD=NO
WRITE_INVARIANT_TIGHTENED=YES
READ_COMPATIBILITY_RETAINED=YES
NO_SYNTHETIC_UTC_FROM_MONOTONIC=YES
NO_LAST_UTC_PLUS_MONOTONIC_SECOND_CLOCK=YES
NO_RETROACTIVE_CHECKPOINT_REWRITE=YES
NO_FALLBACK_AUTO_PROMOTION=YES
NO_ISSUE124_CHANGE=YES
I2C_PORT_LIFETIME_OWNER=I2CDEV
I2CDEV_GLOBAL_INIT_OWNER=DEVICE_PLATFORM_ESP_IDF_COMPOSITION_LIFETIME
SECOND_I2C_MASTER_BUS_OWNER_ON_SAME_PORT=NO
SHARED_I2C_BUS=YES
I2C_PIN_CONFLICT_POLICY=REJECT_NO_SILENT_RECONFIGURATION
I2C_LIBRARY_ADOPTION_ABORT_GATE=STOP_LIBRARY_ADOPTION_GATE
```

Die Empfehlungen sind damit verbindlicher Planbestandteil. Eine materielle
Abweichung benoetigt vor Umsetzung eine neue exakte Planrevision.

## 7. Bibliotheks- und Lizenz-Audit

### 7.1 Pflichtkandidat fuer DS3231SN: esp-idf-lib/ds3231

Der am 2026-08-28 live gepruefte Library-Kandidat fuer die konkrete
DS3231SN-Hardware ist:

```text
DS3231_COMPONENT_CANDIDATE=esp-idf-lib/ds3231
DS3231_COMPONENT_VERSION=1.1.7
DS3231_COMPONENT_REGISTRY_COMMIT=cbe14063d3f2bf39489e18d896c725b8111b5cc4
DS3231_COMPONENT_LICENSE=MIT
DS3231_COMPONENT_TARGETS=esp32,...,esp32s3
```

Der Registry-Eintrag beschreibt einen DS1337-/DS3231-Treiber, nennt die
Installation ueber den ESP Component Manager und fuehrt `esp32` sowie `esp32s3`
als Targets. Die gepruefte API besitzt unter anderem:

- `DS3231_ADDR` mit `0x68`;
- `ds3231_init_desc()`/`ds3231_free_desc()`;
- `ds3231_get_time()` und `ds3231_set_time()`;
- `ds3231_get_oscillator_stop_flag()`;
- `ds3231_clear_oscillator_stop_flag()`;
- die Deaktivierung des 32-kHz-Ausgangs.

Der Treiber dokumentiert die `struct tm`-Zeit als timezoneagnostisch und
empfiehlt GMT/UTC. Das passt zum RTC-UTC-Vertrag, ersetzt aber nicht die
projektseitige Kalender- und Trustvalidierung. Eine eindeutige oeffentliche
EOSC-Read/Write-API ist im geprueften v1.1.7-Header nicht vorhanden. Ebenso
ist die von `ds3231_get_time()` bereits dekodierte `struct tm` allein kein
Beweis fuer rohe BCD-Nibbles und reservierte Registerbits.

DS3231SN ist eine DS3231-Part-Number. Deshalb bleibt der explizit fuer
DS3231 dokumentierte Kandidat ein KISS-Reuse-Kandidat. Vor Adoption wird der
tatsaechlich benoetigte R1-API-Subset gegen den DS3231-/DS3231SN-
Datenblattvertrag und den festen ESP-IDF-6.0.2-Build verifiziert. Nicht
benoetigte optionale DS3231-Funktionen sind kein R1-Gate.

```text
DS3231SN_LIBRARY_COMPATIBILITY=TO_BE_VERIFIED_BY_ESP_IDF_6_0_2_BUILD_AND_TARGETED_TESTS
LIBRARY_VARIANT_MISMATCH=NO
FULL_OWN_DS3231_DRIVER=NO

I2C descriptor/lifecycle
get time
set time
oscillator stop flag read and clear
32kHz output disable
raw Control/Status access via the allowed narrow shim
EOSC verification
calendar readback/validation
```

Die DS3231-/DS3231SN-Registersemantik ist verbindlich: Control 0x0E bit 7 ist
EOSC, bits 4 und 3 sind RS2/RS1, Status 0x0F bit 7 ist OSF und Status 0x0F bit
3 ist EN32KHZ. R1 verwendet RS1/RS2 nicht funktional, der Health-Shim muss
sie bei Read/Modify/Write aber erhalten. Die Bibliotheks-`struct tm` allein
beweist keine rohen BCD-Nibbles oder reservierten Bits.

Der Projektvertrag begrenzt deshalb den R1-Kalender explizit:

```text
R1_DS3231SN_SUPPORTED_UTC_YEAR_RANGE=2000..2099
RTC_TIME_TRUSTED=NO_OUTSIDE_R1_YEAR_RANGE
```

Ein groesserer Bereich wird nicht behauptet, solange Century-Bit-Semantik der
verwendeten Library nicht im Projektvertrag bewiesen ist.

### 7.2 Direkte Abhaengigkeiten

Die Version 1.1.7 deklariert:

```text
esp-idf-lib/i2cdev             version="*"  -> beim Einbinden exakt locken
esp-idf-lib/esp_idf_lib_helpers version="*" -> beim Einbinden exakt locken
```

Der am Planungsdatum aufgeloeste Registry-Stand ist:

```text
esp-idf-lib/i2cdev=2.1.2
LICENSE=MIT
CURRENT_IMPLEMENTATION=driver/i2c_master.h plus FreeRTOS mutexes

esp-idf-lib/esp_idf_lib_helpers=1.4.0
LICENSE=ISC

ESP-IDF esp_netif / I2C master components
LICENSE=Apache-2.0 (ESP-IDF component source)
```

`i2cdev` v2.1.2 verwendet den modernen `i2c_master`-Treiber und besitzt
Port-/Device-Mutexe. Der Header enthaelt zusaetzlich Legacy-Kompatibilitaet;
die ESP-IDF-6.0-Linie kennzeichnet den alten `driver/i2c.h`-Treiber als EOL.
Das ist ein konkretes Compile-/Warnungsrisiko und kein bereits bestaetigter
Kompatibilitaetsnachweis.

### 7.3 Adopt-or-build-Entscheidung

```text
REUSE_EXISTING_COMPONENT=YES_PROVISIONAL
DS3231SN_LIBRARY_COMPATIBILITY=TO_BE_VERIFIED_BY_ESP_IDF_6_0_2_BUILD_AND_TARGETED_TESTS
LIBRARY_VARIANT_MISMATCH=NO
FULL_OWN_DS3231_DRIVER=NO
FULL_REGISTER_DRIVER=NO
ALLOWED_NARROW_ADAPTER_SHIM=YES_IF_REQUIRED_FOR_EOSC_AND_RAW_REGISTER_VALIDATION
NARROW_DS3231_HEALTH_SHIM=YES_IF_REQUIRED_FOR_EOSC_AND_RAW_REGISTER_VALIDATION
ESP_IDF_6_0_2_COMPATIBILITY=UNVERIFIED_PLAN_GATE
ESP32_TARGET_COMPATIBILITY=REGISTRY_DECLARED_NOT_BUILT
ESP32S3_TARGET_COMPATIBILITY=REGISTRY_DECLARED_NOT_BUILT
I2C_API_COMPATIBILITY=MODERN_I2C_MASTER_DECLARED_BY_RESOLVED_I2CDEV_NOT_BUILT
OSF_ACCESS_CAPABILITY=API_PRESENT_NOT_RUN
EOSC_ACCESS_CAPABILITY=PUBLIC_API_MISSING_NARROW_SHIM_REQUIRED_IF_ADOPTED
EN32KHZ_ACCESS_CAPABILITY=DISABLE_API_PRESENT_NOT_RUN
RAW_REGISTER_VALIDATION=SHIM_OR_EQUIVALENT_PROOF_REQUIRED
RAW_REGISTER_VALIDATION_SOURCE=ANALOG_DEVICES_DS3231_DATASHEET
RAW_REGISTER_VALIDATION_VARIANT=DS3231_DS3231SN
DS3231_RS_BITS_R1_FUNCTIONAL_USE=NO
DS3231SN_RS_BITS_PRESERVED_BY_HEALTH_SHIM=REQUIRED
DS3231_SQUAREWAVE_RATE_API_REQUIRED=NO
DS3231_AGING_OFFSET_API_REQUIRED=NO
DS3231_TEMPERATURE_API_REQUIRED=NO
THREAD_SAFETY=DEVICE/PORT_MUTEXES_DECLARED_NOT_INTEGRATED
FLASH_RAM_COST=NOT_MEASURED
LICENSE_NOTICE_REQUIREMENT=ALL_RESOLVED_COMPONENT_LICENSES_AND_NOTICES_RECORD
MAINTENANCE_STATUS=REGISTRY_VERSION_LATEST_AT_AUDIT_DATE_MAINTAINER_DECLARED
```

Die bevorzugte Umsetzung verwendet die `esp-idf-lib/ds3231`-Komponente fuer
den DS3231SN-/DS3231-Kernvertrag. Falls EOSC oder rohe Registervalidierung
erforderlich sind, darf nur ein kleiner dokumentierter DS3231-/DS3231SN-
Health-/Trust-Shim im konkreten
`device_platform_esp_idf`-Adapter ueber die ohnehin vorhandene `i2cdev`-
Transportebene ergaenzt werden. Er darf keine zweite allgemeine RTC-Library
und keinen vollstaendigen Registertreiber bilden. Wenn dieser Shim wegen
Component-/I2C-Lebensdauer oder Lizenzgrenzen nicht sauber moeglich ist:

```text
STOP_LIBRARY_ADOPTION_GATE
```

Dann wird ein Alternativkandidat mit neuer Planrevision verglichen; es gibt
keinen stillen Eigenbau. Die Auswahl wird erst nach dem festen
ESP-IDF-6.0.2-Compile-/Static-Test, Lockfilepruefung, Lizenzabdeckung und
gezieltem DS3231SN-Adaptertest endgueltig.
Die Registry-Lizenz- und Abhaengigkeitsangaben sind in diesem Plan referenziert:

- [esp-idf-lib/ds3231 Registry v1.1.7](https://components.espressif.com/components/esp-idf-lib/ds3231/versions/1.1.7/readme?language=en)
- [esp-idf-lib/ds3231 v1.1.7 Manifest und Commit](https://github.com/esp-idf-lib/ds3231/blob/cbe14063d3f2bf39489e18d896c725b8111b5cc4/idf_component.yml)
- [esp-idf-lib/ds3231 API-Dokumentation](https://esp-idf-lib.github.io/ds3231/)
- [esp-idf-lib/ds3231-Abhaengigkeiten](https://components.espressif.com/components/esp-idf-lib/ds3231/versions/1.1.7/dependencies?language=en)
- [Analog Devices DS3231-Datenblatt](https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf)
- [i2cdev Registry](https://components.espressif.com/components/esp-idf-lib/i2cdev/versions/2.1.2/readme)
- [ESP-IDF Systemzeit und SNTP](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/system/system_time.html)
- [ESP-NETIF-SNTP-API](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/network/esp_netif_programming.html)
- [ESP-IDF-I2C-API](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/peripherals/i2c.html)

Die ESP-IDF-Dokumentation empfiehlt fuer SNTP die `esp_netif`-Wrapper wegen
Thread-Safety. Sie dokumentiert unmittelbare und smooth Korrektur sowie die
explizite Sync-Warte-/Event-/Callback-Semantik. Der Umsetzungsschnitt muss
gegen die exakt installierte v6.0.2-API kompilieren; eine Dokumentationsseite
allein ist kein Buildnachweis.

## 8. Geplante Modul- und Implementierungsschnitte

Alle folgenden Schnitte beginnen erst nach Owner-Freigabe dieser Plan-SHA.

### Schnitt A – Komponenten und ESP-IDF-Basis

- `lib/device_platform_esp_idf/idf_component.yml` als eindeutiges Manifest des
  konkreten ESP-IDF-Adapters mit fester `esp-idf-lib/ds3231`-Version anlegen.
- Die Komponente mit dem ESP Component Manager aufloesen; das projektweite
  erzeugte `dependencies.lock` versionieren und nicht manuell editieren.
- `DEPENDENCY_OWNER_COMPONENT=lib/device_platform_esp_idf` bleibt eindeutig;
  `device_platform` und `fermentation_app` erhalten keine
  `esp-idf-lib/ds3231`-/i2cdev-
  Dependency.
- Direkte und transitive Lizenzdateien/Notices fuer die
  `esp-idf-lib/ds3231`-Komponente, `i2cdev`,
  `esp_idf_lib_helpers`, `esp_netif` und I2C-Bestand erfassen.
- `device_platform_esp_idf` um nur die konkret benoetigten ESP-IDF-
  Komponentenanforderungen erweitern.
- Beide Produktionsprofile bleiben ESP-IDF `v6.0.2`, 4 MB, ohne PSRAM und
  mit gesperrten Aktoren.

Abbruchsignal: Die Komponente baut nicht sauber gegen v6.0.2, benoetigt eine
ungepruefte Legacy-I2C-API, verletzt den Lockfilevertrag, kann den benoetigten
DS3231SN-R1-API-Subset nicht gegen den DS3231-/DS3231SN-Datenblattvertrag
nachweisen oder ermoeglicht den untenstehenden Shared-I2C-Lifecycle nicht.
Insbesondere gilt bei einem inkompatiblen benoetigten API-Subset:
Dann Implementation anhalten, Befund dokumentieren und einen neuen Plan fuer
einen begruendeten Alternativkandidaten vorlegen:

```text
DS3231SN_REQUIRED_API_SUBSET_INCOMPATIBLE=YES
I2C_LIBRARY_ADOPTION_ABORT_GATE=STOP_LIBRARY_ADOPTION_GATE
```

### Schnitt B – Schmale konkrete Platform-Adapter

In `lib/device_platform_esp_idf`:

- DS3231SN-Adapter fuer Erreichbarkeit, Register-/Kalenderread,
  UTC-Konvertierung, `set_time`, Readback, OSF-Zugriff, EOSC-Vertrag und
  EN32KHZ-Deaktivierung gegen die DS3231-/DS3231SN-Bitsemantik;
- falls erforderlich der schmale dokumentierte DS3231-/DS3231SN-Health-/Trust-
  Shim fuer rohe Control-/Statusregister, ohne einen vollstaendigen
  Eigen-Treiber;
- `i2cdev` als I2C-Port-/Device-Lebensdauerowner bei Adoption, mit seinem
  kanonischen per-Port Bus-Handle, Mutex-, Device-Registry- und
  Reference-Count-Vertrag;
- Composition Root als Owner des globalen I2C-Subsystem-Lebenszyklus, ohne
  einen zweiten `i2c_master_bus_handle_t` auf demselben Port zu erzeugen;
- Shared-Bus-Verwendung fuer spaetere Devices, inklusive Abweisung von
  SDA-/SCL-Konflikten ohne stille Bus-Reconfiguration;
- Erweiterung oder klare Umbenennung von `EspTimerTimeSource`, ohne den
  `ITimeSource`-Port zu duplizieren, mit Trust-Latch und bootlokalem
  `lastPublishedTrustedUtc`-High-Water;
- ESP-IDF-Systemzeit-/Trust-Koordinator fuer RTC-Seed, NTP-Event,
  Ruecksprungschutz, RTC-Resync und Fehlerstatus;
- `esp_netif_sntp_*`-Start-/Sync-Hook, Serverkonfigurationsvertrag und
  asynchrone `IN_PROGRESS`-/`COMPLETED`-Beobachtung ohne Connectivity- oder
  Credentialbesitz.

Der Koordinator bleibt klein und linear. Keine allgemeine Zeit-Consensus-
Engine, kein globaler Service-Locator und keine Fermentationskenntnis.
Callback-Arbeit, RTC-Schreiben und Systemzeit-Trust werden gegen die konkrete
ESP-IDF-/FreeRTOS-Lebensdauer und Thread-Sicherheit serialisiert. Ein NTP-
  Callback darf die FSM nicht direkt aufrufen. Ein Smooth-Sync-Event in
  `IN_PROGRESS` darf keinen RTC-Write ausloesen; nur `COMPLETED` startet den
  asynchronen Write-/Readback-/Trust-Abschluss. Der Adapter darf keine direkte
  zweite Businitialisierung auf demselben I2C-Port einfuehren.

### Schnitt C – Boardprofil und Composition Root

- Das kanonische Boardprofil um `rtc.present`, `rtc.type`, `rtc.bus`,
  `rtc.sda`, `rtc.scl` und `rtc.address` erweitern.
- `rtc.type=DS3231SN`, `RTC_FAMILY=DS3231` und `rtc.address=0x68`
  werden semantisch festgelegt; Bus und Pins bleiben verifiziertes
  Boardprofilmaterial und werden nicht geraten.
- Keine `TBD_HARDWARE`-Zahl in eine produktive Konfiguration uebernehmen.
- `main/app_main.cpp` bleibt der Composition Root fuer Erstellung, Besitz,
  Lebensdauer und Injektion der konkreten Plattformobjekte.
- `src/main.cpp` bleibt native Composition Root; native Tests verwenden
  `VirtualTimeSource` und Fake-/Host-Transporte.
- Der gemeinsame Application-Startpfad prueft vor Persistenz/Apply den
  bestehenden `ITimeSource`; die spaetere UI projiziert nur den
  `TrustedAbsoluteTimeRequired`-Grund. Kein RTC-/NTP-Typ gelangt in
  `fermentation_app`.
- Die gemeinsame Application-/Persistence-Domain-Grenze prueft jeden
  resultierenden Snapshot, der ein aktiver `ProgramRun`-/`ManualRun`-Current im
  Zustand `Fermenting` waere, zusammen mit der zu speichernden
  `RunCheckpointTime`. Ohne trusted UTC wird ein solcher persistierter Write
  vor durable Write und RAM-Apply abgelehnt/deferiert. Der Periodic-Pfad
  ueberspringt bei fehlender UTC ohne Kandidaten-, RAM-, Slot-, Head- oder
  Fallback-Mutation und bestaetigt den Schedule nicht. Terminale und andere
  Non-`FERMENTING`-Ziele bleiben von dieser Guard allein unberuehrt.
- Der bestehende `ITimeSource`-Aufruf und die automatische #124-Reevaluation
  bleiben unveraendert.

### Schnitt D – Host/Fake-I2C und Arbitration

Die digitale Testschicht muss ohne reale Uhr, Netzwerk oder DS3231SN auskommen:

- Fake-I2C-Registertransporte fuer erreichbares/nicht erreichbares Geraet,
  Read-/Write-Fehler und konkurrierende Zugriffe;
- gueltige/ungueltige rohe BCD-, DS3231-/DS3231SN-Control-,
  DS3231-/DS3231SN-Status- und Kalenderwerte,
  einschliesslich 12-/24-Stunden-Encoding, reservierter Bits und 2000..2099;
- DS3231SN-OSF=0/1, EOSC=0/1 und EN32KHZ; OSF-Clear erst nach erfolgreichem
  Setzen, Readback, Kalender-/Controlvalidierung und finalem OSF-Readback;
- `DS3231SN_CALENDAR_READ_WRITE`: R1-Kalender ueber die DS3231SN-Register
  lesen/schreiben und 2000..2099 validieren;
- `DS3231SN_OSF_READ_CLEAR_SEQUENCE`: OSF lesen, erst nach validiertem Setzen
  und Readback loeschen und final erneut lesen;
- `DS3231SN_EOSC_VALIDATION`: EOSC=0 als Batterie-/Trust-Voraussetzung
  nachweisen;
- `DS3231SN_EN32KHZ_DISABLE`: den ungenutzten Ausgang deaktivieren und
  Readback pruefen;
- `DS3231SN_RAW_CONTROL_STATUS_VALIDATION`: Control-/Statusbits gegen das
  DS3231-Datenblatt und den DS3231SN-Vertrag validieren;
- `DS3231SN_RS_BITS_PRESERVED_BY_HEALTH_SHIM`: Control 0x0E bits 4/3 als
  DS3231-RS2/RS1 erhalten und bei Read/Modify/Write nicht unbeabsichtigt
  veraendern;
- `DS3231SN_LIBRARY_API_COMPATIBILITY`: den benoetigten R1-Subset der Library
  vor Adoption gegen DS3231/DS3231SN pruefen;
- RTC-Bootseed, `rtc.present=false`, fehlender/untrusted RTC und vollwertiger
  NTP-only-Pfad;
- normaler ESP-IDF-Smooth-Sync mit `IN_PROGRESS` ohne RTC-Write und
  `COMPLETED` mit konvergierter UTC, RTC-Write, Readback und Trust-Abschluss;
- dokumentierter grosser Immediate-Step ohne eigene Delta-Schwelle;
- grosse Rueckwaertskorrektur unter `lastPublishedTrustedUtc` und ehrliches
  `nullopt` ohne retrograde trusted Publikation;
- fehlender Source/Netzwerk nach erfolgreicher Zeit-Etablierung ohne Verlust
  des aktuellen Boot-Trusts;
- fehlgeschlagener RTC-Write bei weiter trusted NTP-Systemzeit;
- unveraenderte monotone Zeit trotz Systemzeitkorrektur;
- `PERIODIC_FERMENTING_CHECKPOINT_WITHOUT_TRUSTED_UTC`: Current A traegt UTC,
  danach wird `unixTimeSeconds()` nullopt und ein faelliger Periodic-Write
  versucht. Es gibt keinen Slot-, Head-, Fallback- oder RAM-Write; Current A
  bleibt erhalten und der Schedule wird nicht als bestaetigt markiert.
- `POWER_LOSS_DURING_TIME_TRUST_GAP`: Nach einem oder mehreren ausgelassenen
  Periodic-Checkpoints wird Current A beim spaeteren Boot mit trusted UTC von
  #124 normal bewertet. Die Wall-Clock-Differenz umfasst das gesamte
  Intervall, ohne Doppelzaehlung, Fallback-Auto-Promotion oder History-Rewrite.
- `TIME_TRUST_RETURNS`: Nach wieder verfuegbarer trusted UTC schreibt der
  naechste regulaer faellige Periodic-Checkpoint Current B mit UTC und bestaetigt
  erst dann den Schedule.
- `FERMENTING_MUTATION_WITHOUT_TRUSTED_UTC`: Eine AdjustRun- oder andere
  persistierte Mutation, deren Ziel weiterhin aktiver FERMENTING-Current waere,
  wird vor durable Write und RAM-Apply abgelehnt/deferiert; Current A bleibt.
- `FERMENTING_MUTATION_AFTER_TRUST_RESTORED`: Der semantisch frische oder
  gemaess Commandvertrag neu bewertete Command laeuft nach Trust-Rueckkehr
  durch die normalen Stale-/Idempotency-Pruefungen und commit/apply normal;
  stale Entscheidungen werden nicht blind wiederverwendet.
- `TERMINAL_NO_ACTIVE_RUN`: Ein gueltiger Stop/Abort nach `NoActiveRun` wird
  ohne trusted UTC nicht allein durch den neuen FERMENTING-Guard blockiert.
  Mindestens ein weiterer bestehender sicherer/terminaler Non-FERMENTING-Pfad
  prueft ebenfalls, dass keine globale `ALL_PERSISTENCE_REQUIRES_UTC`-Regel
  eingefuehrt wurde.
- `UI_BYPASS_ACTIVE_FERMENTING`: Ein direkter Application-/Persistence-Pfad
  ohne UI kann keinen aktiven FERMENTING-Current mit `utcUnixSeconds=nullopt`
  durable schreiben; es gibt keinen RAM-Apply ohne passenden durable Commit.
- `NTP_ONLY_START_BEFORE_SYNC`: `rtc.present=false` und `unixTimeSeconds()`
  ohne Wert weist einen neuen produktiven Run ab und erzeugt weder aktive
  Startmutation noch Checkpoint;
- `NTP_ONLY_START_AFTER_SYNC`: nach abgeschlossenem NTP-Sync wird der Start
  zugelassen und der erste committed Checkpoint enthaelt UTC;
- `RTC_START_WITHOUT_NETWORK`: trusted RTC-Bootseed erlaubt den Start ohne
  Netzwerk-/NTP-Wartezeit;
- `UNTRUSTED_RTC_NO_NTP`: OSF/EOSC/Kalenderfehler lassen UTC `nullopt` und
  weisen den Start ab;
- `UI_BYPASS`: ein direkter Domain-/Application-Command ohne trusted UTC wird
  am selben Startgate abgewiesen;
- `NEW_RUN_CAN_CREATE_UTC_LESS_RECOVERY_ANCHOR=NO`: ein abgewiesener Start
  veraendert keine produktive Run-/Checkpointstruktur;
- `ONE_I2C_PORT_ONE_BUS_OWNER`: DS3231SN und ein zweites Fake-Device teilen
  einen Port mit genau einem Bus-Lifecycle und ohne doppelte Master-Erzeugung;
- `PIN_CONFLICT`: ein zweites Device mit abweichendem SDA/SCL fuer denselben
  Port wird abgewiesen und konfiguriert den Bus nicht still um;
- `DEVICE_REMOVAL`: das Entfernen eines Devices laesst den Shared-Bus leben,
  solange weitere Devices registriert sind;
- `LAST_DEVICE_REMOVAL`: die Busfreigabe folgt dem gepinnten i2cdev-Vertrag;
- `COMPOSITION_SHUTDOWN`: geordneter Shutdown fuehrt weder zu doppeltem
  Delete noch zu Use-after-free;
- `FermentationApplication`-/#124-Test: `WaitingForTrustedTime` wird nach
  einem spaeter gelieferten trusted Wert automatisch ueber `update()` erneut
  bewertet, ohne neue Persistenzmutation nur durch das Warten.

Die Fake-I2C-Schicht ist ein technischer Testseam und keine neue oeffentliche
Zeit-API. Produktionscode haengt nicht von `device_platform_test_support` ab.

## 9. Test-, Build- und Nachweisplan

### 9.1 Digitale Nachweise

Nach Umsetzung werden gezielt und reproduzierbar ausgefuehrt. Diese Gates
werden vom Agenten selbst ausgefuehrt und benoetigen keine Owner-Anweisung:

```text
TARGETED_TESTS=REQUIRED
F1_DOMAIN_START_GATE=REQUIRED
F2_SHARED_I2C_OWNERSHIP=REQUIRED
R1_CURRENT_FERMENTING_CHECKPOINT_GUARD=REQUIRED
FERMENTING_MUTATION_TRUST_GUARD=REQUIRED
PERSISTENCE_WRITE_INVARIANT=REQUIRED
DS3231SN_LIBRARY_API_COMPATIBILITY=REQUIRED
DS3231SN_RAW_CONTROL_STATUS_VALIDATION=REQUIRED
DS3231SN_OSF_EOSC_EN32KHZ_CONTRACT=REQUIRED
FULL_NATIVE_SUITE=REQUIRED_BEFORE_OWNER_FINAL_IMPLEMENTATION_REVIEW
ESP_IDF_BRINGUP_BUILD=REQUIRED
ESP_IDF_RELEASE_BUILD=REQUIRED
ESP_CLANG_BRINGUP=REQUIRED
ESP_CLANG_RELEASE=REQUIRED
BOTH_ESP_IDF_PROFILES_REQUIRED=YES
ARCHITECTURE_GATES=REQUIRED
LICENSE_NOTICE_GATES=REQUIRED
```

```text
targeted native:
  pio test -e native --filter <betroffene Zeit-/RTC-/Arbitrationstests>

full native:
  pio run -e native
  pio test -e native

ESP-IDF:
  . /var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.0.2/export.sh
  python3 scripts/build_esp_idf_profiles.py all
  python3 scripts/run_esp_idf_static_analysis.py all

Architektur/Qualitaet:
  python3 scripts/check_architecture_boundaries.py
  python3 scripts/check_secrets.py
  git diff --check
  clang-format --dry-run --Werror <geaenderte C/C++-Dateien>
  pio run -e native -t compiledb
```

Der konkrete Testfilter wird im Umsetzungs-PR nach den tatsaechlich geaenderten
Dateien festgelegt. Der vollstaendige native Lauf ist ein normales
Softwaregate und muss vor dem Owner Final Implementation Review bestehen.
`build_esp_idf_profiles.py all` und `run_esp_idf_static_analysis.py all`
decken beide Profile und die jeweiligen esp-clang-Gates ab. Lizenz-/Notice-
Nachweise werden gegen die aufgeloesten Komponenten und das Repository-
Noticeformat geprueft. Nur reale Hardware-/Power-Cycle-/Netzwerkaktionen
bleiben separat `NOT_RUN` oder `BLOCKED`.

### 9.2 Profil- und Architekturtests

Der Nachweis muss fuer `esp32_bringup` und `esp32_release` pruefen:

- ESP-IDF exakt v6.0.2 und kein Arduino-Produktionspfad;
- ESP32- und, soweit die Komponente dies behauptet, ESP32-S3-Kompatibilitaet;
- moderne I2C-Master-API ohne unkontrollierte Legacy-Driver-Annahme;
- bei Adoption genau ein `i2cdev`-Busowner je konfiguriertem Port und kein
  zweiter `i2c_master_bus_handle_t` fuer denselben Port;
- DS3231SN-API-Subset und Control-/Status-Reserved-Bits gegen das
  DS3231-Datenblatt, nicht nur gegen den DS3231-Header;
- korrekte `REQUIRES`-/Lockfile-/Lizenzabdeckung;
- `ESP_CLANG_BRINGUP=REQUIRED` und `ESP_CLANG_RELEASE=REQUIRED` mit dem
  kanonischen Static-Analysis-Treiber;
- keine `fermentation_app`-Abhaengigkeit aus `device_platform_esp_idf`;
- keine Zeit-/RTC-/NTP-Typen in der Application ausser `ITimeSource`;
- Aktoren bleiben in beiden Profilen gesperrt.

### 9.3 Hardware- und Netzwerkgrenzen

Diese Nachweise sind in diesem Plan ausdruecklich nicht ausfuehrbar:

```text
RTC_SOFTWARE=may PASS digitally after implementation
RTC_HARDWARE=BLOCKED_OWNER_HARDWARE_PENDING
RTC_REAL_DETECTION=NOT_RUN
RTC_REAL_I2C_TRANSACTION=NOT_RUN
RTC_REAL_OSF_BEHAVIOR=NOT_RUN
RTC_BATTERY_BACKUP=NOT_RUN
RTC_POWER_CYCLE_RETENTION=NOT_RUN
RTC_BOOT_SEED_ON_BOARD=NOT_RUN
NTP_TO_RTC_REAL_RESYNC=NOT_RUN
LONG_DURATION_DRIFT=NOT_RUN
NTP_REAL_NETWORK_RUN=NOT_RUN
```

Die spaetere Hardwarekampagne benoetigt Boardrevision, exakte Pins/Bus,
Verdrahtung, Versorgung, Pull-ups, Batterie, Power-Cycle- und UART-/Log-
Provenienz. Sie ersetzt keinen digitalen Test und wird erst auf dem finalen
Implementierungs-Head ausgefuehrt.

## 10. Dokumentations- und Roadmapwirkung

### In dieser Planrunde

- dieser vollstaendige Plan unter
  `docs/tasks/issue-126-absolute-time-rtc-ntp-plan.md`;
- `docs/ROADMAP.md` minimal auf den gemergten PR #125 und die Einordnung
  `#124 -> #126 -> #25` synchronisieren;
- Issue #126 mit Ziel, Grenzen, Ownerentscheidungen, Hardware-/Netzwerkstatus
  und Planverweis synchronisieren;
- Draft-PR mit Basis, Planpfad, exakter Plan-SHA, `IMPLEMENTATION=NOT_STARTED`,
  `MERGE=NO` und `OWNER_DECISIONS_REQUIRED=NONE` synchronisieren;
- den bestehenden einzigen aktuellen `SESSION HANDOVER`-Kommentar
  aktualisieren, keinen zweiten anlegen;
- keine normative Recovery-, Hardware-, Architektur- oder Lizenzregister-
  Umschreibung, keine Produktions-/Testimplementation.

### Nach Ownerfreigabe und Umsetzung

- normative Architecture-/Hardware-/Zeitquellen gegen die neue RTC-Owner-
  entscheidung widerspruchsfrei aktualisieren;
- bei erster Komponenteneinbindung Lockfile, Lizenz-/Notice-Nachweis und
  `docs/THIRD_PARTY_COMPONENTS.md` nach Projektvertrag aktualisieren;
- nur bei einer neuen, ueber die vorgegebenen Entscheidungen hinausgehenden
  Architekturentscheidung ADR-Register ergaenzen;
- #124 fachlich unveraendert lassen und nur den vorhandenen
  `ITimeSource`-Eingang verifizieren;
- Roadmap nach dem live verifizierten Umsetzungsstatus aktualisieren.

## 11. Definition of Done fuer die spaetere Umsetzung

- [ ] `esp-idf-lib/ds3231`-Komponentenkandidat gegen ESP-IDF 6.0.2 gebaut oder
      begruendet verworfen; der R1-API-Subset ist gegen DS3231/DS3231SN
      validiert und es gibt keinen vollstaendigen Eigen-Registertreiber.
- [ ] DS3231SN wird als konkrete DS3231-Part-Number mit dem DS3231-
      Registervertrag behandelt; Control-/Status-Semantik folgt dem DS3231-
      Datenblatt, RS1/RS2 werden nicht funktional verwendet, aber vom
      Health-Shim erhalten.
- [ ] DS3231SN-Library-Compatibility bleibt bis zum festen Spike
      TO_BE_VERIFIED_BY_ESP_IDF_6_0_2_BUILD_AND_TARGETED_TESTS; Adoption bleibt
      PROVISIONAL und `LIBRARY_VARIANT_MISMATCH=NO`.
- [ ] Der benoetigte DS3231SN-R1-API-Subset fuer I2C-Lifecycle, Kalender,
      Set/Get, OSF, EOSC, EN32KHZ, Readback und Raw-Registervalidierung ist
      nachgewiesen; bei Inkompatibilitaet greift STOP_LIBRARY_ADOPTION_GATE.
- [ ] `rtc.present=false` als fehlerfreien NTP-only-Modus und
      `rtc.present=true` als optionalen DS3231SN-Pfad nachgewiesen.
- [ ] Direkte Dependencies, feste Lockfile-Versionen, Lizenzen und Notices
      nachgewiesen.
- [ ] Component-Manager-Manifest liegt ausschliesslich unter
      `lib/device_platform_esp_idf/idf_component.yml`; `dependencies.lock` ist
      generiert und versioniert.
- [ ] `ITimeSource` bleibt einziger Application-Zeitvertrag.
- [ ] Jeder neue produktive Run verlangt vor Persistenz/Apply eine vorhandene
      trusted UTC; ein UI-only-Gate ist ausgeschlossen und ein UTC-loser
      Recoveryanker kann nicht entstehen.
- [ ] Jeder neue `ProgramRun`-/`ManualRun`-Current im Zustand `Fermenting`
      enthaelt UTC. Ein UTC-loser aktiver FERMENTING-Current kann durch keinen
      neuen #126-kompatiblen Write committed werden.
- [ ] Ein faelliger Periodic-Checkpoint bei fehlender trusted UTC wird ohne
      Slot-, Head-, Fallback-, Kandidaten- oder RAM-Mutation uebersprungen;
      der letzte gueltige Current bleibt erhalten und der Schedule wird nicht
      bestaetigt. Der vorhandene ehrliche Resultstatus ist dokumentiert.
- [ ] Nach Trust-Rueckkehr schreibt der naechste regulaer faellige Periodic-
      Checkpoint wieder einen UTC-tragenden Current und bestaetigt den
      Schedule erst nach durablem Erfolg.
- [ ] Persistierte Mutationen mit resultierendem aktivem FERMENTING-Current
      werden ohne trusted UTC vor Persistenz und RAM-Apply abgelehnt/deferiert;
      der direkte Application-/Persistence-Bypass kann den Guard nicht
      umgehen. Nach Trust-Rueckkehr gelten die normalen frischen
      Stale-/Idempotency-Pruefungen.
- [ ] Stop/Abort zu `NoActiveRun` und mindestens ein weiterer sicherer oder
      terminaler Non-`FERMENTING`-Pfad werden nicht allein durch den neuen
      Guard blockiert; `ALL_PERSISTENCE_REQUIRES_UTC=NO` bleibt erhalten.
- [ ] Die Schreibinvariante ist auf neue Writes begrenzt, alte UTC-lose
      Records bleiben lesbar und #124 behandelt sie weiterhin fail-closed.
      Es gibt kein Schemafeld, keine synthetische UTC, keinen retroaktiven
      Rewrite und keine Fallback-Auto-Promotion.
- [ ] NTP-only weist den Start vor Synchronisierung ab, erlaubt ihn nach
      abgeschlossenem NTP-Sync mit UTC im ersten Checkpoint, und trusted
      RTC erlaubt den Start ohne Netzwerkwartezeit.
- [ ] Untrusted RTC ohne NTP und ein direkter Domain-/Application-Bypass
      werden am selben Startgate abgewiesen; der Grund ist ueber den
      Command-/Result-Vertrag projizierbar.
- [ ] monotone Zeit bleibt von RTC-/NTP-Korrekturen unberuehrt.
- [ ] `unixTimeSeconds()` liefert nur bei trusted System-UTC, oberhalb des
      bootlokalen High-Water, und sonst `nullopt`.
- [ ] RTC-Trust prueft Reachability, Rohregister/gleichwertigen Beweis,
      Kalender 2000..2099, OSF=0 und EOSC=0 nach dem DS3231-Datenblatt.
- [ ] OSF wird erst nach bewusstem validiertem Setzen/Readback/Clear und
      finalem OSF-Readback geschlossen; DS3231SN-EN32KHZ ist deaktiviert.
- [ ] DS3231SN-Kalender-Read/Write, OSF-Read/Clear-Sequenz, EOSC-Validierung,
      EN32KHZ-Deaktivierung, Raw-Control-/Statusvalidierung, RS1/RS2-
      Erhaltung und der Library-API-Subset sind gezielt getestet.
- [ ] RTC-Seed, NTP-Systemzeit und NTP-zu-RTC-Sync sind getestet.
- [ ] Smooth-Sync schreibt waehrend `IN_PROGRESS` nicht in die RTC und schreibt
      genau nach `COMPLETED` mit konvergierter UTC.
- [ ] Rueckwaerts- und grosse Delta-Policy verwenden keine eigene Schwelle,
      akzeptieren den dokumentierten ESP-IDF-Immediate-Step und publizieren
      keine retrograde trusted UTC.
- [ ] Grosse Delta-Recovery bleibt ohne Rueckschreibung, zweites Recovery,
      Checkpoint-Historienrewrite oder direkten FSM-Callback.
- [ ] Verlust von WLAN, NTP-Erreichbarkeit oder RTC nach etablierter
      Bootzeit entwertet diese Zeit nicht automatisch.
- [ ] `i2cdev` besitzt bei Adoption den Port-/Device-Lifecycle, die Composition
      Root den globalen Subsystem-Lebenszyklus, und es gibt keinen zweiten
      Master-Busowner auf demselben Port.
- [ ] Shared-I2C-Tests decken Pin-Konflikt ohne stille Reconfiguration,
      Device-Removal, Last-Device-Removal und Composition-Shutdown ohne
      Double-Delete/Use-after-free ab.
- [ ] Shared-I2C-Bus und semantisches Boardprofil verwenden keine geratenen
      Pins oder `TBD_HARDWARE` als Laufzeitwerte.
- [ ] #89-Grenze und #124-Vertrag bleiben erhalten.
- [ ] Native Fake-I2C-/Arbitration-/#124-Reevaluationstests und die vollstaendige
      native Suite bestehen.
- [ ] Beide ESP-IDF-Profile, beide esp-clang-Gates und Architekturgrenzen
      bestehen.
- [ ] RTC-Hardware und NTP-Netzwerkstatus sind separat als PASS/BLOCKED/
      NOT_RUN belegt; kein digitaler Build wird als Hardwarefreigabe dargestellt.

## 12. Planabschluss und Owner-Review

```text
R1_DEVELOPMENT_BASE_SHA=5b8b86b99347bb0bb104dd1c2968040656119440
NEW_TIME_ISSUE=126
PLAN_BRANCH=agent/issue-126-absolute-time-rtc-ntp-plan
PLAN_PATH=docs/tasks/issue-126-absolute-time-rtc-ntp-plan.md
PLAN_SHA=RECORDED_IN_PR127_AND_SESSION_HANDOVER

TIME_ARCHITECTURE=RTC_PLUS_NTP
RTC_DEVICE=DS3231SN
RTC_FAMILY=DS3231
RTC_I2C_ADDRESS=0x68
RTC_STORAGE_TIMEZONE=UTC
RTC_TRUST_OSF=OSF_1_MEANS_RTC_TIME_TRUSTED_NO
NTP_IMPLEMENTATION=ESP_IDF_SNTP
APPLICATION_TIME_PORT=device_platform::ITimeSource
PROVISIONAL_OPERATOR_RESUME_BEFORE_TRUSTED_TIME=NO
ITIME_SOURCE_REUSED=YES
RTC_HARDWARE_OPTIONAL=YES
NTP_ONLY_MODE_SUPPORTED=YES
RTC_REQUIRED_FOR_IMMEDIATE_OFFLINE_RECOVERY=YES

DS3231_COMPONENT_CANDIDATE=esp-idf-lib/ds3231
DS3231_COMPONENT_VERSION=1.1.7
DS3231_COMPONENT_LICENSE=MIT
ESP_IDF_6_0_2_COMPATIBILITY=UNVERIFIED_PLAN_GATE

RTC_TO_SYSTEM_TIME=PLANNED_AFTER_VALIDATED_RTC_TRUST
NTP_TO_SYSTEM_TIME=PLANNED_VIA_ESP_NETIF_SNTP_SYNC
NTP_TO_RTC_SYNC=PLANNED_AFTER_COMPLETED_SYNC_READBACK_OSF_CLEAR

SMALL_RTC_NTP_DELTA_POLICY=USE_ESP_IDF_SMOOTH_SYNC_FOR_NORMAL_CORRECTION
CUSTOM_SMALL_DELTA_THRESHOLD=NO
BACKWARD_TIME_POLICY=NO_TRUSTED_RETROGRADE_PUBLICATION
TRUSTED_ITIME_SOURCE_RETROGRADE_PUBLICATION=NO
UTC_HIGH_WATER_PUBLICATION_GATE=YES_BOOT_LOCAL
LARGE_TIME_DELTA_POLICY=ACCEPT_ESP_IDF_DOCUMENTED_SYNC_BEHAVIOR
CUSTOM_LARGE_DELTA_THRESHOLD=NO
ESP_IDF_SMOOTH_SYNC=YES
ESP_IDF_DOCUMENTED_LARGE_DELTA_IMMEDIATE_STEP=ACCEPTED_PLATFORM_BEHAVIOR
LARGE_DELTA_RECOVERY_POLICY=NO_RETROACTIVE_RECOVERY_ENGINE
RETROACTIVE_ACTIVE_RUN_RECOVERY=NO
RTC_SYNC_WRITE_FREQUENCY=AFTER_EVERY_COMPLETED_NTP_SYNCHRONIZATION
RTC_WRITE_DURING_SMOOTH_IN_PROGRESS=NO
RTC_WRITE_AFTER_SYNC_COMPLETED=YES
SOURCE_REACHABILITY_LOSS_INVALIDATES_ESTABLISHED_TIME=NO
WIFI_LOSS_AFTER_SUCCESSFUL_NTP_SYNC_INVALIDATES_CURRENT_BOOT_TIME=NO
NTP_SERVER_UNREACHABLE_AFTER_SUCCESSFUL_SYNC_INVALIDATES_CURRENT_BOOT_TIME=NO
RTC_UNREACHABLE_AFTER_SUCCESSFUL_BOOT_SEED_INVALIDATES_CURRENT_BOOT_TIME=NO

DS3231SN_EOSC_REQUIRED=0
DS3231SN_32KHZ_OUTPUT_R1=DISABLED
SQW_INT_R1=UNUSED
R1_DS3231SN_SUPPORTED_UTC_YEAR_RANGE=2000..2099
RAW_REGISTER_VALIDATION=REQUIRED
RAW_REGISTER_VALIDATION_SOURCE=ANALOG_DEVICES_DS3231_DATASHEET
RAW_REGISTER_VALIDATION_VARIANT=DS3231_DS3231SN
DS3231_RS_BITS_R1_FUNCTIONAL_USE=NO
DS3231SN_RS_BITS_PRESERVED_BY_HEALTH_SHIM=REQUIRED
NARROW_DS3231_HEALTH_SHIM=YES_IF_REQUIRED_FOR_EOSC_AND_RAW_REGISTER_VALIDATION
FULL_OWN_DS3231_DRIVER=NO
FULL_REGISTER_DRIVER=NO
ALLOWED_NARROW_ADAPTER_SHIM=YES_IF_REQUIRED_FOR_EOSC_AND_RAW_REGISTER_VALIDATION
DS3231SN_LIBRARY_COMPATIBILITY=TO_BE_VERIFIED_BY_ESP_IDF_6_0_2_BUILD_AND_TARGETED_TESTS
LIBRARY_VARIANT_MISMATCH=NO
DS3231_SQUAREWAVE_RATE_API_REQUIRED=NO
DS3231_AGING_OFFSET_API_REQUIRED=NO
DS3231_TEMPERATURE_API_REQUIRED=NO

DS3231SN_CALENDAR_READ_WRITE=REQUIRED
DS3231SN_OSF_READ_CLEAR_SEQUENCE=REQUIRED
DS3231SN_EOSC_VALIDATION=REQUIRED
DS3231SN_EN32KHZ_DISABLE=REQUIRED
DS3231SN_RAW_CONTROL_STATUS_VALIDATION=REQUIRED
DS3231SN_LIBRARY_API_COMPATIBILITY=REQUIRED
RTC_PHYSICAL_VARIANT_CONFIRMATION=REQUIRED
DS3231M_R1_SUPPORT=NO
MULTI_RTC_VARIANT_SUPPORT_R1=NO

R1_CURRENT_FERMENTING_CHECKPOINT_REQUIRES_TRUSTED_UTC=YES
UTC_LESS_CURRENT_FERMENTING_COMMIT=FORBIDDEN
LAST_VALID_UTC_CURRENT_RETAINED_ON_TIME_TRUST_LOSS=YES
PERIODIC_FERMENTING_CHECKPOINT_WITHOUT_TRUSTED_UTC=SKIP_NO_WRITE
SKIPPED_FOR_MISSING_TRUSTED_UTC_DOES_NOT_CONFIRM_CHECKPOINT_SCHEDULE=YES
FERMENTING_MUTATION_RESULTING_IN_FERMENTING_REQUIRES_TRUSTED_UTC=YES
FERMENTING_MUTATION_WITHOUT_TRUSTED_UTC=REJECT_OR_DEFER_BEFORE_PERSISTENCE_AND_RAM_APPLY
FERMENTING_MUTATION_WITHOUT_TRUSTED_UTC_RAM_APPLY=NO
ALL_PERSISTENCE_REQUIRES_UTC=NO
TERMINAL_NO_ACTIVE_RUN_PATHS_BLOCKED_BY_NEW_GUARD=NO
WRITE_INVARIANT_TIGHTENED=YES
READ_COMPATIBILITY_RETAINED=YES
SCHEMA_CHANGE=NO
NEW_PERSISTENCE_FIELD=NO
NO_SYNTHETIC_UTC_FROM_MONOTONIC=YES
NO_LAST_UTC_PLUS_MONOTONIC_SECOND_CLOCK=YES
NO_RETROACTIVE_CHECKPOINT_REWRITE=YES
NO_FALLBACK_AUTO_PROMOTION=YES
NO_ISSUE124_CHANGE=YES

I2C_PORT_LIFETIME_OWNER=I2CDEV
I2CDEV_GLOBAL_INIT_OWNER=DEVICE_PLATFORM_ESP_IDF_COMPOSITION_LIFETIME
SECOND_I2C_MASTER_BUS_OWNER_ON_SAME_PORT=NO
SHARED_I2C_BUS=YES
I2C_PIN_CONFLICT_POLICY=REJECT_NO_SILENT_RECONFIGURATION
I2C_LIBRARY_ADOPTION_ABORT_GATE=STOP_LIBRARY_ADOPTION_GATE

DEPENDENCY_OWNER_COMPONENT=lib/device_platform_esp_idf
MANIFEST_PATH=lib/device_platform_esp_idf/idf_component.yml
DEPENDENCIES_LOCK=project-level generated dependencies.lock
NTP_SERVER_CONFIGURATION_OWNER=ISSUE126_TIME_PLATFORM
NETWORK_CONNECTIVITY_OWNER=ISSUE89

TARGETED_TESTS=REQUIRED
F1_DOMAIN_START_GATE=REQUIRED
F2_SHARED_I2C_OWNERSHIP=REQUIRED
FULL_NATIVE_SUITE=REQUIRED_BEFORE_OWNER_FINAL_IMPLEMENTATION_REVIEW
ESP_IDF_BRINGUP_BUILD=REQUIRED
ESP_IDF_RELEASE_BUILD=REQUIRED
ESP_CLANG_BRINGUP=REQUIRED
ESP_CLANG_RELEASE=REQUIRED
BOTH_ESP_IDF_PROFILES_REQUIRED=YES
ARCHITECTURE_GATES=REQUIRED
LICENSE_NOTICE_GATES=REQUIRED

ISSUE89_BOUNDARY=CONNECTIVITY_AND_WLAN_OWNED_BY_ISSUE89
ISSUE124_CONTRACT_CHANGE=NO
DEVICE_PLATFORM_APP_SPECIFIC_LEAK=NO

RTC_SOFTWARE_IMPLEMENTATION=NOT_STARTED
RTC_HARDWARE=BLOCKED_OWNER_HARDWARE_PENDING
NTP_REAL_NETWORK_RUN=NOT_RUN

OWNER_DECISIONS_REQUIRED=NONE

ROADMAP_PLAN_SHA_PIN=REQUIRED_AFTER_CANONICAL_PLAN_COMMIT
ISSUE126_SYNC=REQUIRED
PR127_BODY_SYNC=REQUIRED
SESSION_HANDOVER_SYNC=REQUIRED_EXISTING_COMMENT_ONLY
SESSION_HANDOVER_COUNT=1

MERGE=NO
OWNER_PLAN_REVIEW_REQUIRED=YES
```

STOP – Owner Review der finalen RTC-Variantenkorrektur.
