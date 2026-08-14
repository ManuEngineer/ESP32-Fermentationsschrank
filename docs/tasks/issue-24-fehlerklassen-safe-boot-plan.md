# Issue #24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion

## Planrevision 2

| Feld | Verbindlicher Stand |
|---|---|
| Issue | #24, [E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion |
| Planrevision | 2 |
| Planstatus | Zur Ownerfreigabe des exakten Plan-Commits; Revision 1 ist nicht freigegeben |
| Branch | agent/issue-24-fehlerklassen-safe-boot-plan |
| Draft-PR | #107 |
| Ausgangs-HEAD vor dieser Planrevision | 08d19eee307219669533cf245fa2e640252de4da |
| aktueller main-Basisstand | b8eae5f4da5f2666b5a9bda333d115254c4db5b2 |
| freigegebener Plan-Commit | nach Commit einzutragen; bis dahin NONE |
| Implementation | NOT_STARTED |
| Tests und Builds dieser Planungsphase | NOT_RUN |

Revision 1 war nicht ownerfreigegeben. Diese Revision 2 ersetzt sie
vollstaendig und ist eigenstaendig. Sie beschreibt den vollstaendigen #24-Scope,
die Wiederverwendung bestehender Vertraege, die Integrationsgrenzen und die
spaeteren Umsetzungsschnitte. Sie implementiert keine Produktionslogik,
keine Firmwaretests, keine Hardwareintegration und keine Testinjektion.

## 1. Ownerentscheidung und Scope-Gate

Die Ownerentscheidung vom 2026-08-14 ist Bestandteil dieses Plans und fuer
Release 1 verbindlich:

- Drei abnormale Neustarts innerhalb einer offenen Restart-Episode fuehren
  beim folgenden Boot zu SAFE_BOOT.
- Die Stabilitaetsdauer betraegt 30 Minuten.
- Die Grenze ist firmwarefest und nicht servicekonfigurierbar.
- Die Episode beginnt mit dem ersten sicher als abnormal klassifizierten
  Neustart. Der Zaehler bleibt persistent.
- Ein normal gestarteter Boot darf die Episode erst nach 30 Minuten
  abnormal-restartfreiem, stabilem Betrieb schliessen.
- Ein normaler Neustart oder langer stromloser Zustand schliesst die Episode
  nicht.
- Vor einem firmwarekontrollierten abnormalen Neustart muss die aktualisierte
  Restart-Evidenz geschrieben und durch Readback oder einen gleichwertig
  eindeutigen Commitnachweis bestaetigt sein.
- Bei einem Reset ohne vorherige Schreibmoeglichkeit wertet der Bootpfad den
  realen Plattform-Resetgrund aus und schreibt die Evidenz vor normaler
  Freigabe nach.
- SAFE_BOOT wird nicht allein durch Zeitablauf oder normalen Neustart
  verlassen. Es gilt der bestehende #24-Fehlerresetvertrag.

Damit ist die bisher offene fachliche Entscheidung aus
docs/SYSTEM_SAFETY_AND_RECOVERY.md aufgeloest. Die 30 Minuten sind kein
Wall-Clock-Fenster ueber Neustarts. Die Implementierung verwendet eine
monotone Zeitquelle nur fuer den stabilen Boot seit dem letzten erfolgreichen
Boot. NTP oder eine externe RTC sind weder Voraussetzung noch Teil dieses
Plans.

Die Kompatibilitaetspruefung gegen Issue #24, die gemergten #23-Vertraege und
die realen Producer aus #56/#57 ergibt keinen fachlichen Widerspruch:

- #23 liefert die vorhandene Aktor-Safety-Gate-Eingabe und
  ActuatorWatchdogFaultEvidence; #24 bestimmt deren systemweite Wirkung und
  den autorisierten Reset.
- #56 liefert ConfigurationRuntimeFailure, den fail-closed
  Konfigurationszustand und den unbestimmten CommitOutcomeUnknown-Pfad.
- #57 liefert ConfigurationUnavailable und
  ConfigurationIntegrityFailure ohne Runtimefreigabe.
- Die vier Fehlerklassen, persistente Verriegelung, Bootprioritaet,
  SAFE_BOOT, Fehlerreset und Fehlerinjektion bleiben Eigentum von #24.

## 2. Ziel und Ergebnis

Issue #24 liefert einen einzigen anwendungsweiten Fehler- und
Safety-Kern, der:

1. jede sicherheitsrelevante Ursache in eine stabile Klasse und einen stabilen
   Fehlercode einordnet;
2. bei mehreren Ursachen alle aktiven Ursachen behaelt und einen
   deterministischen dominanten Ausgang berechnet;
3. die unmittelbare sichere Reaktion von der Meldungs- und Bediensemantik
   trennt;
4. persistente Safety- und Systemverriegelungen mit Write-before-Apply- und
   Recovery-Nachweis fuehrt;
5. Boot, kontrollierten Neustart, Resetursache und SAFE_BOOT ueber die
   bestehende Prozesszustandsmaschine integriert;
6. #23-Watchdog-Evidence und das
   CONFIGURATION_SAFETY_INTEGRATION_GATE ueber die realen Vertraege
   verarbeitet;
7. bei unbekannter oder unaufgeloester Information fail-closed bleibt;
8. die geforderten Softwarefehler reproduzierbar und automatisiert pruefbar
   macht.

Der Kern endet weiterhin an abstrakten Ports. Kein Teil dieses Plans
entscheidet GPIOs, Pegel, H-Brueckenverdrahtung, reale Luefterrueckmeldung,
Temperaturgrenzen, Commissioningwerte oder konkrete Hardwaretests.

## 3. Verbindliche Quellen und Vertragsrouting

Die Umsetzung muss vor jedem Code-Schnitt die folgenden Quellen gegen den
freigegebenen Plan-Commit erneut pruefen. Die Liste ist bewusst vollstaendig
fuer #24, ersetzt aber keine Live-Pruefung von Issue, PR, Branch, HEAD und
Roadmap.

| Quelle | Fuer #24 verbindlich |
|---|---|
| Issue #24 | Scope, Akzeptanzkriterien und CONFIGURATION_SAFETY_INTEGRATION_GATE |
| docs/SAFETY_AND_FAULTS.md | vier Klassen, Dominanz, Lebenszyklus, Quittierung, Reset und Latch |
| docs/SAFETY_COMPONENT_FAULTS.md | Sensor-, Temperatur-, Luefter- und Aktorwirkung |
| docs/SYSTEM_SAFETY_AND_RECOVERY.md | Bootreihenfolge, sichere Abschaltung, Restart-Episode und SAFE_BOOT |
| docs/ACCEPTANCE_TESTS.md | Pflichtszenarien und Nachweisstatus |
| ADR-018 und relevante Eintraege in docs/DECISIONS.md | Variante B, Commit-Unbestimmtheit und fail-closed Runtime |
| docs/CONFIGURATION_PERSISTENCE.md | #56/#57-Producer, CommitOutcomeUnknown, Write-before-Apply und Recovery |
| docs/IMPLEMENTATION_ISSUES.md | E3-Reihenfolge und Nichtvorwegnahme spaeterer Issues |
| docs/AGENT_WORKFLOW.md | Owner- und Draft-Gates |
| docs/ENGINEERING_PRINCIPLES.md | Repository-first, SOLID, DRY, KISS und Modulgrenzen |
| docs/CI_AND_QUALITY_GATES.md | spaetere gezielte Tests und Statusklassifikation |
| docs/RUN_COMMANDS.md | bestehende Commands, Nachrichten und Fehlerresetentscheidung |
| docs/RUNTIME_BEHAVIOR.md und docs/STATE_MACHINE.md | Prozesszustand, Boot, Fault, Recovery und SAFE_BOOT |
| docs/DIAGNOSTICS_AND_MAINTENANCE.md | aktorfreie Diagnose, Servicegrenzen und Resetnachweis |
| docs/RUN_PERSISTENCE.md und docs/RECOVERY_AND_INTERRUPTION.md | Recovery-, Reboot- und Write-before-Apply-Vertrag |
| docs/tasks/issue-17-implementation-plan.md | #17-Persistenzgrenze und Ausschluss einer parallelen Fault-Domaene |
| docs/tasks/issue-18-restart-weighted-progress-plan.md | #18-Reboot-/Recoverygrenze und kein autonomer Faultreset |
| docs/tasks/issue-23-actuator-planner-plan.md | Watchdog-Evidence und Aktor-Safety-Gate |
| docs/tasks/issue-56-implementation-plan.md | ConfigurationRuntimeFailure und Commit-Unbestimmtheit |
| docs/tasks/issue-57-implementation-plan.md | ConfigurationUnavailable und ConfigurationIntegrityFailure |

## 4. Repository-first-Bestand und Wiederverwendung

Vor #24 existieren bereits mehrere tragende Bausteine. Sie werden erweitert
oder konsumiert, nicht parallel nachgebaut:

| Bestehender Baustein | Verwendung in #24 |
|---|---|
| MessageClass, MessageCode, RuntimeMessage, faultRevision und RunCommandState | stabile Meldungsprojektion und Versionskonflikte; keine zweite Bediennachrichtenhierarchie |
| AcknowledgeMessage, MuteMessage, ResetFault | bestehende Command-Semantik; #24 liefert die fachliche Reset-Evaluation |
| FaultResetEvaluation und criticalSafetyEventPending | zentrale Reset- und Sperrprojektion; das Bool wird nicht zu einem zweiten Latch ausgebaut |
| highestPriorityActiveMessage und vorhandene Revisionspruefungen | zentrale Dominanz-/Stale-Pruefung, erweitert um den kanonischen Faultkern |
| ProcessState::Boot, SafeBoot, Fault, RecoveryEvaluation, ServiceMode und bestehende ProcessEvent | eine bestehende Zustandsmaschine; keine zweite Boot- oder Recovery-State-Machine |
| ActuatorSafetyGateInput und ActuatorSafetyGateStatus aus #23 | einziger vorhandener Aktor-Gate-Eingang; Unresolved bleibt nicht freigabefaehig |
| ActuatorWatchdogFaultEvidence und applyExternalWatchdogFaultReset() aus #23 | vorhandene Watchdog-Evidenz und einziger externer Resetpfad |
| device_platform::IStateStore, StateStoreKey, Read-/Write-Status und Storage-Envelope | persistente Safety-/Restart-Daten; kein neuer allgemeiner Speicherport |
| SimulatedPersistentStateStore | native Cut-Point-, Read-, Korruptions-, Restart- und CommitOutcomeUnknown-Injektion |
| ITimeSource und VirtualTimeSource | monotone 30-Minuten-Stabilitaetspruefung, keine Wall-Clock-Abhaengigkeit |
| Temperatur-, Sensor-, Aktor- und Event-Journal-Mocks | fachliche Eingaben und Orakel in Tests; keine Test-Support-Abhaengigkeit in Produktion |
| #17-/#18-Persistenz- und Recovery-Koordinatoren | bestehende Write-before-Apply- und Wiederanlaufpfade; keine parallele Run-Persistenz |
| #56/#57-Konfigurationsstatus und Producer | reale Safety-Gate-Eingaben; keine erneute Konfigurationsfehlerklassifikation |

Die Repository-Pruefung findet derzeit keinen kanonischen Port fuer
Plattform-Resetursache und kontrollierten Neustart. Dafuer fuehrt #24 genau
einen schmalen, anwendungsneutralen Port im device_platform ein. Sein
verbindlicher Vertrag lautet:

- `ResetCause` ist ein geschlossenes Enum mit
  `PowerOn`, `AuthorizedRestart`, `ControlledSafetyRestart`,
  `WatchdogOrPanic`, `Brownout` und `Unknown`;
- `ResetCauseSnapshot` enthaelt den Enumwert, einen
  `Valid`-/`Unknown`-Beobachtungsstatus und eine bootlokal stabile
  ObservationId. `observeBootReset()` liefert innerhalb eines Boots immer
  denselben Snapshot und verursacht keine Persistenzmutation;
- `ControlledRestartRequest` enthaelt nur den zentralen Zweck
  `ControlledSafetyRestart` oder `AuthorizedFaultReset`. Die fachliche
  RestartEvidenceId und Write-before-Restart-Persistenz bleiben im #24-Core;
- `ControlledRestartResult` hat genau `Accepted`, `Rejected` und
  `OutcomeUnknown`. `Accepted` bestaetigt nur, dass die Plattformanforderung
  angenommen wurde, nicht dass bereits ein neuer Boot erfolgt ist.
  `Rejected` und `OutcomeUnknown` erlauben keinen automatischen zweiten
  Versuch; die Runtime bleibt sicher aus;
- der Plattformport besitzt keinen fachlichen Fault- oder Episodezaehler.
  Der #24-Core konsumiert den bootlokal stabilen Snapshot genau einmal gegen
  die persistierte EvidenceId. Wiederholtes Lesen oder ein zurueckkehrender
  Restart-Aufruf darf weder einen zweiten Zaehlerinkrement noch einen
  zweiten Resetversuch erzeugen. Ein Snapshot ohne sichere Klassifikation
  ist `Unknown` und fail-closed.

Der konkrete ESP-IDF-Adapter, die Abbildung realer ESP-IDF-Resetursachen und
der echte `esp_restart()`-Pfad gehoeren zum E5-Bring-up-Gate #29. R2
implementiert dort nichts und markiert keine Hardwareeigenschaft als
bewiesen. Testseitig gehoert die kontrollierbare Quelle in
device_platform_test_support. Falls bei der Umsetzung ein inzwischen
vorhandener gleichwertiger Port entdeckt wird, wird dieser verwendet und
nicht verdoppelt. Ein Resetport ist kein zweiter Bootautomat und kein neuer
Safety-Gate-Vertrag.

## 5. Stabiler Fehlervertrag

### 5.1 Fehlerklassen

Die vier Klassen erhalten stabile interne IDs, die nicht von deutschen
Meldungstexten oder UI-Kategorien abhaengen:

| ID | Name | Wirkung |
|---:|---|---|
| 1 | PROCESS_WARNING | Prozess darf weiterlaufen, Meldung und Ereignis |
| 2 | OPERATING_FAULT | normale Regelung ist eingeschraenkt; Peltier bleibt aus, ausser ein expliziter sicherer Ersatzbetrieb ist fuer den Code erlaubt |
| 3 | LATCHED_SAFETY_FAULT | sofortige sichere Abschaltung, persistente Sicherheitsverriegelung, bewusster Reset |
| 4 | LATCHED_SYSTEM_FAULT | sicherer Stillstand, persistente Systemverriegelung, SAFE_BOOT oder technischer Servicepfad |

Ein unbekannter Klassenwert, ein unbekannter Fehlercode, ein unvollstaendiger
Faultdatensatz und eine unaufgeloeste Gate-Eingabe werden als Klasse 4
behandelt. Sie duerfen niemals auf Klasse 1 oder eine normale Freigabe
zurueckfallen.

### 5.2 Stabile Fehlercodes

Die folgenden Codes sind Teil von Planrevision 2. Ihre IDs werden vor der
Implementierung eingefroren. Zusaetzliche Detaildaten werden strukturiert
transportiert; ein sichtbarer Text darf den Code nicht ersetzen.

Die Reihenfolge innerhalb jeder Klasse ist zugleich die feste Prioritaet fuer
die prominente Darstellung, wenn mehrere gleichklassige Fehler aktiv sind.
Alle weiteren aktiven Fehler bleiben erhalten.

| Prioritaet | Code | Klasse | Bedeutung und Quelle | Latch / Wiederfreigabe |
|---:|---|---:|---|---|
| 1 | P1-001 | 1 | Prozess erreicht ein Ziel langsamer als erwartet | kein Latch; Prozesswarnung |
| 2 | P1-002 | 1 | Zeit- oder Fortschrittskonfidenz ist vermindert | kein Latch; erneute Bewertung |
| 3 | P1-003 | 1 | nichtkritische Historien-/Bereinigungsmeldung | kein Latch; erneute Bewertung |
| 1 | O2-001 | 2 | veraltete oder nicht mehr zulaessige Regelanforderung | kein persistenter Latch; nur explizite Codepolitik darf auto-rearmen |
| 2 | O2-002 | 2 | Produktfuehler STALE oder FAILED; #21 waehlt die programmabhaengige Ersatzstrategie | kein persistenter Latch; Peltier aus, danach nur qualifizierte Ersatz-/Rueckkehrentscheidung |
| 3 | O2-003 | 2 | thermische Reaktion ist noch nicht bestaetigt | kein persistenter Latch; bounded reevaluation |
| 4 | O2-004 | 2 | notwendiger Sicherheitsfuehler voruebergehend stale, solange eine sichere Neubewertung noch moeglich ist | Peltier aus; kein Auto-Rearm ohne Codepolitik |
| 1 | S3-001 | 3 | Luftfuehler fuer Sicherheitsfreigabe fehlgeschlagen oder unaufloesbar | persistent; bewusster Reset |
| 2 | S3-002 | 3 | Kuehlkoerper-/Peltier-Schutzfuehler fehlgeschlagen oder unaufloesbar | persistent; bewusster Reset |
| 3 | S3-003 | 3 | Produktfuehlerlage eskaliert nur bei zusaetzlicher Sicherheitsunsicherheit, unklarer Pflichtsensorlage oder widerspruechlichem Safety-Nachweis | persistent; bewusster Reset |
| 4 | S3-004 | 3 | qualifizierte Sicherheits-Eingriffsgrenze; initiale Abschaltung und optional typisierte SAFETY_RECOVERY | persistent; Recovery loescht den Latch nicht |
| 5 | S3-005 | 3 | qualifizierte harte thermische Notgrenze | persistent; kein Auto-Rearm |
| 6 | S3-006 | 3 | Innen- oder Aussenluefterfehler mit Sicherheitswirkung | persistent; bewusster Reset |
| 7 | S3-007 | 3 | unzulaessiger Aktor-/H-Bruecken-Ausgang oder sicherheitswidrige Rueckmeldung | persistent; bewusster Reset |
| 8 | S3-008 | 3 | #23 ActuatorWatchdogFaultEvidence / latched watchdog fault | persistent; Reset nur ueber #24 und #23-Resetpfad |
| 9 | S3-009 | 3 | qualifizierter Sicherheitszustand ohne ausreichende thermische Reaktion nach der kanonischen Eskalationsentscheidung | persistent; bewusster Reset |
| 1 | Y4-001 | 4 | #56 ConfigurationRuntimeFailure mit erhaltener Producer-Ursache | persistent; Service-/Resetvertrag |
| 2 | Y4-002 | 4 | #56 ConfigurationCommitIndeterminate beziehungsweise nicht aufgeloestes CommitOutcomeUnknown | persistent; kein Runtime- oder Aktorfreigabepfad |
| 3 | Y4-003 | 4 | #57 ConfigurationUnavailable | persistent; SAFE_BOOT/Service |
| 4 | Y4-004 | 4 | #57 ConfigurationIntegrityFailure | persistent; SAFE_BOOT/Service |
| 5 | Y4-005 | 4 | kritischer Safety-/System-Persistenzschreibfehler | persistent in RAM und, soweit moeglich, minimal persistent |
| 6 | Y4-006 | 4 | unbekannte oder nicht sicher klassifizierbare Resetursache | persistent; SAFE_BOOT vor normaler Freigabe |
| 7 | Y4-007 | 4 | dritte abnormale Ursache in der offenen Restart-Episode | persistent; folgender Boot muss SAFE_BOOT waehlen |
| 8 | Y4-008 | 4 | Sicherheits-, Boot- oder Fehleraufgabe liefert keinen verlaesslichen Zustand | persistent; einmaliger kontrollierter Neustart nur gemaess Episodevertrag |
| 9 | Y4-009 | 4 | Lauf- oder Recoveryzustand nicht sicher rekonstruierbar | persistent; kein automatischer Laufstart |
| 10 | Y4-011 | 4 | interner Zustand, Schema oder Faultdatensatz ist unbestimmt | persistent; SAFE_BOOT |

Die Codes bilden keine Temperatur-, GPIO-, Zeit- oder
Commissioninggrenzwerte ab. Qualifizierte Sensor-, Limit- und
Aktorentscheidungen kommen aus den bestehenden Fachvertraegen. Y4-010 ist
kein R2-Faultcode: ein bekannter Brownout ist Restart-Evidenz und wird als
RestartCauseEvent protokolliert, aber nicht allein als persistenter Klasse-4-
Latch behandelt. Ein unbekannter oder nicht sicher validierter Quellstatus
wird stattdessen zu Y4-006, Y4-008 oder Y4-011 nach der jeweils bekannten
Ursache; bei unbekannter Ursache gilt Y4-011.

RestartCauseEvent besitzt eigene stabile Werte und ist kein Fault:

| Ereigniswert | Bedeutung | Episodewirkung |
|---|---|---|
| RESTART_CONTROLLED_SAFETY | firmwarekontrollierter abnormaler Restart wegen schwerem System-/Safetyfehler | zaehlt genau einmal bei committed Pre-Restart-Evidenz |
| RESTART_WATCHDOG_OR_PANIC | Watchdog-/Panic-artiger Plattformreset | zaehlt genau einmal im Bootpfad |
| RESTART_BROWNOUT | eindeutig klassifizierter Brownout-/Versorgungsreset | zaehlt genau einmal im Bootpfad; kein Latch allein durch Ereignis 1 oder 2 |
| RESTART_POWER_ON | normaler Power-on ohne abnormale Evidenz | zaehlt nicht |
| RESTART_AUTHORIZED | Service-/Update-/Recoveryreset mit eigenem normalem Vertrag | zaehlt nicht und schliesst keine offene Episode |
| RESTART_UNKNOWN | unbekannt oder nicht sicher klassifizierbar | kein harmloser Restart; fail-closed SAFE_BOOT |

### 5.3 Faultdatensatz und Lebenszyklus

Jeder Faultdatensatz besitzt mindestens:

- stabile FaultInstanceId aus einer persistenten, vor Mutation auf Ueberlauf
  geprueften Sequenz;
- stabilen Code und stabile Klasse;
- primaere Komponente beziehungsweise Quelle;
- feste Source-/CorrelationKey-Daten statt unbeschraenkter Texte;
- monotone Entstehungssequenz und, wenn innerhalb des Boots vorhanden,
  monotone Zeit;
- aktiv, quittiert, beseitigt und verriegelt;
- unmittelbare Safety-Disposition;
- erlaubte Wiederfreigabe- und Resetpolitik;
- benoetigte Berechtigung;
- optionale, auf eine bestehende FaultInstanceId zeigende primaryFaultId-
  Referenz;
- pro FaultInstanceId ein einmaliges controlledRestartUsed-Merkmal;
- faultRevision fuer konkurrierende oder veraltete Commands.

Der Status folgt der bestehenden Fachsemantik:

1. ACTIVE_UNACKNOWLEDGED;
2. ACTIVE_ACKNOWLEDGED;
3. CAUSE_CLEARED_LOCKED, wenn die Ursache verschwunden, der Latch aber
   noch nicht resetberechtigt ist;
4. CLEARED erst nach erfolgreicher, persistierter und verifizierter
   Resetwirkung.

Quittiert bedeutet nur, dass die Meldung gesehen wurde. Stummgeschaltet
beeinflusst nur den akustischen Kanal. Beseitigt bedeutet nicht automatisch
resetberechtigt. Ein Neustart veraendert keinen dieser Status zugunsten einer
Freigabe.

FaultInstanceId ist die einzige Zielidentitaet fuer einen Reset. Gleicher
Code und gleiche Quelle werden als dieselbe aktive Instanz fortgefuehrt, wenn
der vorhandene CorrelationKey dieselbe Ursache bezeichnet. Unabhaengige
gleichcodige Ursachen erhalten unterschiedliche FaultInstanceIds. Eine
Instanz wird nach CLEARED nicht still wiederverwendet.

### 5.4 Traceability zur kanonischen Komponentenmatrix

Die fachliche Einordnung aus docs/SAFETY_COMPONENT_FAULTS.md wird ohne
Parallelklassifikation abgebildet:

| kanonische Ursache | R2-Abbildung | Grenze |
|---|---|---|
| Luftfuehler FAILED | S3-001 | Klasse-3-Latch, kein Luft- oder Produktfuehlerersatz |
| Kuehlkoerper-/Peltier-Schutzfuehler FAILED | S3-002 | Klasse-3-Latch, keine normale Aktorfreigabe |
| Produktfuehler STALE/FAILED | O2-002 | #21 entscheidet Ersatzstrategie; Peltier zunaechst aus |
| ProductRequired ohne erlaubten Ersatz | O2-002 mit sicherem Prozessstopp | kein automatischer S3-Latch nur wegen des fehlenden Produktfallbacks |
| Produktlage plus unklare Pflichtsensor-/Safety-Lage | S3-003 | zusaetzliche Sicherheitsunsicherheit ist erforderlich |
| Sicherheits-Eingriffsgrenze | S3-004 | initiale Abschaltung, danach nur SAFETY_RECOVERY bei vollstaendiger Qualifikation |
| harte Notgrenze oder unbeherrschbare Ursache | S3-005 | beide Richtungen gesperrt, niemals SAFETY_RECOVERY |
| Innen-/Aussenluefterfehler | S3-006 | Peltier aus, sichere Luefterstrategie |
| unzulaessiger H-Bruecken-/Aktorzustand | S3-007 | vorhandenes #23-Gate, keine Adapterumgehung |
| fehlende thermische Reaktion | O2-003 zuerst, S3-009 nur nach kanonischer Eskalation | keine voreilige Produktfehlerklassifikation |
| #23-Watchdog-Evidence | S3-008 | kein zweiter Watchdogvertrag |
| Brownout/Watchdog/normaler Reset | RestartCauseEvent | Brownout #1/#2 allein erzeugt keinen Fault-Latch |
| unbekannter Resetgrund | Y4-006 oder Y4-011 | fail-closed SAFE_BOOT |

Konkrete Temperatur-, Leistungs-, Puls-, Trend-, STALE- und
Commissioningwerte bleiben in den bestehenden Vertraegen beziehungsweise
bei #35. Ein fehlender Producerwert deaktiviert die produktive
SAFETY_RECOVERY und gibt keine ersatzweise erfundene Grenze frei.

## 6. Dominanz, Primaer- und Folgefehler

### 6.1 Dominanzregel

Die sichere Ausgangsentscheidung berechnet sich deterministisch:

1. Klasse 4 dominiert Klasse 3, Klasse 3 dominiert Klasse 2, Klasse 2
   dominiert Klasse 1.
2. Bei gleicher Klasse entscheidet die stabile Codeprioritaet aus Abschnitt
   5.2.
3. Bei gleichem Code entscheidet die fruehere Entstehungssequenz; ein
   spaeterer Fehler darf den primaeren Nachweis nicht verdecken.
4. Alle aktiven Fehler werden retained und separat quittier-/resetbar
   bewertet.
5. Wenn die Reihenfolge oder ein Vergleich nicht sicher bestimmbar ist,
   dominiert ein Klasse-4-Unknown-Fault.

Die Dominanz bestimmt Safety-Ausgang, Prozesszustand und prominente Meldung.
Sie loescht keine untergeordneten Ursachen.

### 6.2 Primaer-/Folgefehler

Ein neuer Fault darf auf einen bestehenden Primaerfehler verweisen, wenn die
Kausalitaet durch den Ablauf oder einen bestehenden Vertrag ausreichend
belegt ist. Eine nur vermutete Kausalitaet wird als suspected markiert und
nicht als bewiesen dargestellt.

- Der Primaerfehler bleibt aktiv, bis seine eigene Ursache und sein eigener
  Resetvertrag erfuellt sind.
- Ein Folgefehler hat eine eigene Klasse, Reaktion, Revision und
  Resetberechtigung.
- Das Loeschen des Primaerfehlers loescht keinen aktiven Folgefehler.
- Ein Folgefehler darf den Ausgang weiter dominieren.
- Persistente Safety-/System-Folgefehler werden zusammen mit dem
  notwendigen Latchnachweis erhalten.

## 7. Zentrale Safety-Reaktion und Aktor-Gate

Der #24-Kern berechnet eine einzige systemweite Safety-Disposition und
projiziert sie auf den bestehenden ActuatorSafetyGateInput aus #23. Es wird
kein zweiter Safety-Gate-Abstraktionsvertrag eingefuehrt.

| aktive Lage | zentrale Reaktion |
|---|---|
| nur Klasse 1 und alle anderen Gates Allowed | Prozess darf weiterlaufen; Meldung und Ereignis |
| Klasse 2 | neue Peltieranforderungen sperren; aktive Peltieranforderung sicher beenden; Peltier nur in einem fuer genau diesen Code explizit qualifizierten Ersatzbetrieb zulassen; Luefter nach bestehender sicherer Restwaerme-Strategie |
| Klasse 3, ausser S3-004 in qualifizierter Recoveryphase | Peltier sofort aus, beide H-Brueckenrichtungen deaktivieren, laufende Impulsakkumulatoren und Integratorwirkung sicher verwerfen oder blockieren, erforderliche Luefterreaktion, persistenter Safety-Latch |
| S3-004 Sicherheits-Eingriffsgrenze | ausloesende Richtung sofort aus; Mindest-Auszeit und Polaritaetstotzeit; erneute Sensor-/Luefter-/Aktorqualifikation; danach nur SAFETY_RECOVERY als typisierte Sicherheitsanforderung, niemals normale PI-Freigabe |
| S3-005 harte Notgrenze oder unbeherrschbare Ursache | beide Richtungen sofort und dauerhaft sperren; keine SAFETY_RECOVERY |
| Klasse 4 | sicherer Stillstand, Peltier und H-Bruecke aus, keine automatische Prozessfortsetzung, persistenter System-Latch, SAFE_BOOT oder technischer Diagnosepfad |
| Unresolved, unknown, fehlende Evidenz oder ungueltiger Gatezustand | ImmediateStop beziehungsweise strukturell nicht freigegeben; niemals Allowed |

Die Abschaltung ueberstimmt komfort- oder regelungsseitige Mindestzeiten,
Totzeiten und Richtungswechsel. Die Luefterreaktion bleibt auf die bestehende
abstrakte Strategie beschraenkt. Kein #24-Code schreibt GPIOs oder umgeht
den realen Aktoradapter.

Die bestehende criticalSafetyEventPending-Information wird als kompatible
Projektion des kanonischen Fault-/Safety-Zustands behandelt. Sie darf keine
zweite Quelle fuer Latch, Dominanz oder Reset werden.

### 7.1 SAFETY_RECOVERY innerhalb des bestehenden #23-Gates

Die bestehende ActuatorSafetyGateStatus-Aufzaehlung aus #23 wird um genau
eine fachlich typisierte Disposition erweitert: SAFETY_RECOVERY. Das ist
keine zweite Gate-Abstraktion und kein normaler Allowed-Zustand. Die
Disposition ist nur gueltig, wenn eine interne SafetyRecoveryRequest
beiliegt:

- targetFault: genau die FaultInstanceId von S3-004;
- triggeringDirection und recoveryDirection: aus der qualifizierten
  Ursache abgeleitet, recoveryDirection ist die zugelassene Gegenrichtung;
- faultRevision und safetyEvidenceRevision: stale-pruefbare Revisionen;
- attemptIndex und maxAttempts: fester, begrenzter Versuchsnachweis;
- qualifiedSensorEvidence, qualifiedFanEvidence und
  qualifiedActuatorEvidence;
- hardLimitNotReached, noSensorConflict, triggeringDirectionOff,
  safeCurrentWhenAvailable, minimumOffTimeElapsed und
  polarityDeadTimeElapsed;
- eine validierte SafetyRecoveryParametersRevision mit begrenzter Leistung,
  Pulsdauer und Trendpruefung.

Der typisierte Vertrag ist eine intern erzeugte, nicht von Commands,
UI/Web-Aufrufern oder Tests einsetzbare Capability:

| Feld | Typ/Semantik |
|---|---|
| targetFault | `FaultInstanceId`; muss auf die aktive S3-004-Instanz zeigen |
| triggeringDirection / recoveryDirection | bestehender abstrakter Richtungswert; die zweite Richtung wird nur aus der qualifizierten Ursache abgeleitet |
| faultRevision / safetyEvidenceRevision | gepruefte monotone Revisionen, beide muessen beim Planneraufruf aktuell sein |
| attemptIndex / maxAttempts | feste Ganzzahlfelder mit `1 <= attemptIndex <= maxAttempts <= 2`; keine Laufzeitkonfiguration des Limits |
| qualifiedSensorEvidence, qualifiedFanEvidence, qualifiedActuatorEvidence | unveraenderliche, intern erzeugte Evidenz-Snapshots mit eigener Revision; keine caller-supplied Booleans |
| hardLimitNotReached, noSensorConflict, triggeringDirectionOff, safeCurrentWhenAvailable, minimumOffTimeElapsed, polarityDeadTimeElapsed | abgeleitete, gepruefte Safety-Preconditions; ein fehlendes oder unbekanntes Ergebnis ist false/fail-closed |
| SafetyRecoveryParametersRevision | gepruefte Referenz auf #35-/Commissioningwerte; Inhalt und Gueltigkeit werden nicht von #24 erfunden |

Die RecoveryCapability wird nur vom #24-Core aus dem aktuellen
SafetyStateRecord und den qualifizierten Producer-Snapshots konstruiert. Der
Planner akzeptiert keine strukturell gleich aussehende, aber extern erzeugte
Anforderung. Damit ist die SafetyRecoveryRequest vollstaendig vom normalen
ControlRequest und von `ActuatorSafetyGateStatus::Allowed` getrennt.

Die SafetyRecoveryParametersRevision ist ein qualifizierter Verweis auf
freigegebene Werte aus dem bestehenden Commissioning-/#35-Vertrag. #24
erfindet dafuer keine Zahlen, Defaults oder Produktionsgrenzen. Fehlt die
Revision oder ist sie nicht validiert, bleibt SAFETY_RECOVERY in der
Produktion deaktiviert und das Gate bleibt ImmediateStop.

Der bestehende ActuatorPlanner konsumiert SAFETY_RECOVERY als einzigen
begrenzten Sonderpfad:

1. #24 setzt zuerst den ausloesenden normalen Ausgang aus und verwirft den
   normalen PI-/Impulszustand.
2. #24 berechnet die Request intern aus aktuellem Fault, Sensoren, Lueftern,
   Aktorstatus und SafetyRecoveryParametersRevision.
3. Der Planner akzeptiert keine normale ControlRequest zusammen mit
   SAFETY_RECOVERY und prueft die Revisionen sowie alle Pflichtnachweise.
4. Der Planner fuehrt nur die validierte, begrenzte Recovery-Anforderung
   ueber seine bestehende Aktorplanung und den bestehenden Sinkpfad aus.
5. Nach jedem Versuch geht der Ausgang wieder auf ImmediateStop. Die
   ausloesende Richtung bleibt aus.
6. Ein Trend-/Aktor-/Sensorabbruch, eine harte Notgrenze oder das Erreichen
   der kanonischen maximal zwei Versuche beendet Recovery sofort und
   behaelt den Latch.
7. Auch nach erfolgreicher Rueckfuehrung bleibt S3-004 persistent verriegelt;
   der normale Fermentationslauf und Allowed werden nicht automatisch
   fortgesetzt.

Die SAFETY_RECOVERY-Anforderung ist damit weder ein #22-ControlRequest noch
eine normale Aktorfreigabe. Sie loescht keinen Fault, aendert keine
Faultdominanz und darf nicht durch einen caller-supplied Allowed-Status
ersetzt werden. Eine zweite Aktor-State-Machine entsteht nicht; der
bestehende #23-Planner erhaelt nur die neue typisierte Disposition und
validiert ihren sicheren Sonderpfad.

### 7.2 SAFETY_RECOVERY-Ereignisse

Der Faultkern erzeugt fuer den Beginn, jeden Versuch, Abbruch und Erfolg
eine typisierte Eventprojektion. Der Versuchserfolg ist nur ein
RecoveryResult, niemals ein Reset oder eine normale Freigabe. Der bewusste
Fehlerreset bleibt danach erforderlich.

## 8. Quittieren, Stummschalten und Fehlerreset

### 8.1 Quittieren

AcknowledgeMessage verwendet die vorhandene Message-Revision und markiert
die zugeordnete Meldung beziehungsweise den Fault als gesehen. Es:

- loescht die Ursache nicht;
- aendert keine Klasse;
- loest keinen persistenten Latch;
- aendert keinen Aktor-Gate-Ausgang;
- setzt keinen Watchdog und keine Restart-Episode zurueck.

### 8.2 Stummschalten

MuteMessage aendert ausschliesslich die akustische Ausgabe. Es:

- loescht keine Meldung;
- quittiert keinen Fault implizit;
- loest keinen Reset und keine Freigabe aus;
- darf auch bei aktiver Safety-/Systemverriegelung nur den akustischen Kanal
  beeinflussen.

### 8.3 Fehlerreset

ResetFault bleibt der einzige bestehende Command, aber der #15-Vertrag wird
strukturell geschlossen. Der externe FaultResetRequest enthaelt ab R2 nur:

- CommandEnvelope;
- genau eine targetFault FaultInstanceId;
- expectedFaultRevision als stale-pruefbare Zielrevision.

Er enthaelt keine caller-supplied FaultResetEvaluation und keine positiven
allowed-, Safetycheck- oder Autorisierungsflags. Eine Reset-All-Semantik
existiert in R2 nicht; mehrere Faults werden einzeln mit eigener
FaultInstanceId bewertet.

#24 berechnet intern aus kanonischem SafetyStateRecord, aktuellem Faultkern,
aktuellen Sensor-/Konfigurations-/Recoverychecks, Berechtigung und
expectedFaultRevision eine unveraenderliche interne
FaultResetAuthorization. Nur diese von #24 erzeugte Autorisierung darf den
bestehenden RunCommand-Pfad zur Commitwirkung aufrufen. Die bisherige
FaultResetEvaluation bleibt eine read-only Ergebnis-/Diagnoseprojektion und
ist keine Eingabeautoritaet. UI, Web, Service und Tests koennen sie weder
einspeisen noch ueberschreiben.

FaultResetAuthorization bindet mindestens targetFault, erwartete und
aktuelle Safety-/Faultrevision, eine unveraenderliche Ursachenfreiheits- und
Safetycheck-Evidenz, den Berechtigungsnachweis, die codebezogene Resetpolicy
und die verbleibenden gleich-/hoeherklassigen FaultInstanceIds. Ihre
Erzeugung ist nur im #24-Core moeglich; ein oeffentlicher Konstruktor,
Konvertierungsweg aus FaultResetEvaluation oder caller-supplied positive
Flags ist unzulaessig.

Eine Freigabe ist nur moeglich, wenn:

- die targetFault-Instanz aktiv und resetberechtigt ist;
- die Ursache beseitigt oder durch den Code ausreichend geklaert ist;
- alle erforderlichen Safety- und Integritaetspruefungen bestanden sind;
- kein anderer gleich- oder hoeherklassiger aktiver Latch besteht;
- die betroffene Run-/Recoveryinformation rekonstruierbar ist;
- die erforderliche Berechtigung, insbesondere Service-PIN bei technischen
  oder sicherheitskritischen Codes, vorliegt;
- expectedFaultRevision und die aktuelle Safetyrevision uebereinstimmen;
- die neue Latch-/Faultrevision und der autorisierte
  FaultResetBootIntent Write-before-Apply persistiert und verifiziert
  wurden.

Ein fehlgeschlagener, unbestimmter oder verfruehter Reset bleibt abgelehnt
und erzeugt keine teilweise freigegebene Runtime. ResetEligibleNoRuntime
aus #57 ist nur ein Konfigurationsstatus und hebt keinen #24-Latch.

Der Resetcommit betrifft nur targetFault. Andere aktive FaultInstanceIds,
Primary-/Follow-up-Latches und criticalSafetyEventPending bleiben bestehen
und werden aus der Safetyautoritaet neu projiziert. Bei targetFault aus
S3-008 wird ActuatorPlanner::applyExternalWatchdogFaultReset() erst nach
erfolgreichem #24-Resetcommit und nur fuer diese Zielinstanz ausgefuehrt.

Nach erfolgreichem Resetcommit wird kein direkter Fault-/SafeBoot-Exit
ausgefuehrt. #24 schreibt zuerst einen autorisierten FaultResetBootIntent
und fordert danach genau den zentralen autorisierten Reset-Bootpfad an.
Dieser Reset ist als RESTART_AUTHORIZED klassifiziert, zaehlt nicht als
abnormaler Restart und schliesst keine offene Restart-Episode. Der folgende
Boot konsumiert den passenden Intent genau einmal, validiert Safety,
Konfiguration, Run-Recovery und die verbleibenden Latches und verwendet
danach den bestehenden Bootpfad nach Standby oder Recovery. Ohne passenden
Intent bleibt ein normaler Neustart in Fault beziehungsweise SAFE_BOOT.

Die konkrete #15-Migration ist:

1. FaultResetRequest wird auf Envelope, targetFault und
   expectedFaultRevision reduziert.
2. decideFaultReset erhaelt keine oeffentliche Evaluation mehr, sondern wird
   vom #24-Anwendungsservice mit einem nur intern erzeugbaren
   FaultResetAuthorization aufgerufen.
3. FaultResetAuthorization besitzt einen privaten/gekapselten Erzeugungsweg
   aus dem #24-Faultkern und bindet targetFault, aktuelle Revision,
   Resetpolicy, Berechtigung und Safetychecks.
4. RunCommandState.faultRevision und RuntimeMessage.faultRevision bleiben
   Projektionen beziehungsweise stale-Schutz; sie werden nicht zur zweiten
   Resetautoritaet.
5. Ein veralteter oder falscher targetFault wird vor jeder Persistenzwirkung
   abgelehnt.

Damit gibt es weiterhin genau einen ResetFault-Command, aber keinen
caller-supplied Safety-Backdoor.

### 8.4 Automatische Wiederfreigabe

| Klasse / Codeart | automatische Wiederfreigabe |
|---|---|
| Klasse 1 | keine Verriegelung; laufende Neubewertung |
| Klasse 2 mit explizit markiertem Betriebsfehler | nur nach qualifizierter Ursachefreiheit, bestehender Codepolitik, stabiler Neubewertung und begrenzten Versuchen |
| Klasse 2 mit unklarer Ursache, Sicherheitsfuehler oder unaufgeloestem Gate | verboten; Eskalation zu Klasse 3 oder 4 |
| Klasse 3 | keine automatische Wiederfreigabe nach Latch; eine im Komponentenvertrag vorgesehene bounded Sicherheitsneubewertung ist kein stiller Latchreset |
| Klasse 4 | keine automatische Wiederfreigabe; Service-/Fehlerresetvertrag |
| #23-Watchdog-Latch | kein Auto-Rearm; nur #24 darf nach vollstaendiger Evaluation den bestehenden #23-Reset aufrufen |
| Restart-Episode und SAFE_BOOT | kein Auto-Reset durch Neustart, Stromlosigkeit, NTP oder abgelaufene Wall-Clock |

Konkrete Sensor-, Temperatur-, Luefter- und Sicherheitsgrenzen bleiben in
den bestehenden Fach- und Commissioningvertraegen. #24 erfindet keine Werte.

## 9. Persistente Safety-/Systemverriegelung

### 9.1 Ein kanonischer Record ausserhalb der Run-Journalstruktur

Die notwendige Safety- und Restart-Evidenz wird in einem einzigen
anwendungsseitig definierten, kleinen SafetyStateRecord gefuehrt. Der
Record verwendet den bestehenden IStateStore, StateStoreKey, atomaren
Storage-Envelope und Read-/Write-Status. Er ist kein zweites allgemeines
Journal und keine Erweiterung des von #17 ausgeschlossenen
RunPersistenceSnapshot.

Der Record enthaelt mindestens:

- Record-Schema und gespeicherte Revision;
- eine feste, kleine Menge persistenter aktiver Klasse-3-/Klasse-4-Latches
  mit FaultInstanceId, Code, Quelle und Primar-/Folgebeziehung;
- Faultrevision und sichere Reset-/Recovery-Markierung;
- safeBootRequired und den dominanten Grund;
- faultInstanceSequence sowie pro Instanz den
  controlledRestartUsed-Status;
- offene Restart-Episode mit Episodenkennung, Zaehler und letzter
  RestartEvidenceId;
- ausstehende, firmwarekontrollierte Restart-Evidenz mit eigener
  RestartEvidenceId, Grund und Pending-/Committed-/Consumed-Status;
- ausstehende FaultResetBootIntent mit Ziel-FaultInstanceId und
  Intentrevision;
- letzten sicher klassifizierten Resetgrund und dessen
  Klassifikationsmetadaten.

Die persistente Darstellung ist bounded und besitzt in Release 1 genau
`MAX_PERSISTED_LATCHES = 8` aktive Klasse-3-/Klasse-4-Instanzen. Alle
Datensaetze verwenden feste Breiten und feste Schluessel-/Quellfelder; eine
unbegrenzte vector-, String- oder Ereignishistorie ist im SafetyStateRecord
nicht zulaessig. Primary-/Follow-up-Beziehungen zeigen ausschliesslich auf
FaultInstanceIds im selben bounded Record. FaultInstanceSequence,
Faultrevision, Recordrevision, EpisodeId und RestartEvidenceId werden vor
jeder Mutation auf gueltigen Wertebereich und Ueberlauf geprueft. Ein
Ueberlauf wird vor der Mutation abgelehnt.

Wird die Kapazitaet erreicht, wird kein Fault still verdrangt oder
zusammengelegt, wenn dadurch ein gleich- oder hoeherklassiger Nachweis
verloren ginge. Ein nicht darstellbarer neuer Fault, eine ungueltige
Primary-/Follow-up-Referenz oder ein fehlender Pflichtdatensatz erzeugt
Y4-005, sichere Ausgaenge und einen gesperrten Gatezustand. Die Implementierung
darf die Grenze nicht durch eine zweite unbeschraenkte Nebenstruktur umgehen.

Der SafetyStateRecord und der #24-Faultkern sind die einzige Autoritaet fuer
aktive Safety-Latches, Faultrevision, Dominanz, Restart-Episode und
safeBootRequired. RunCommandState.faultRevision,
criticalSafetyEventPending und RuntimeMessages sind nur daraus abgeleitete
Projektionen fuer bestehende Konsumenten, Meldungen und Stale-Schutz. Sie
werden nach Boot, Recovery und jedem bestaetigten Commit neu aufgebaut. Eine
Abweichung zwischen Projektion und Safetyautoritaet ist ein Unknown-Zustand
und bleibt fail-closed; keine Projektion darf einen Latch selbst loeschen.

Nicht sicher speicherbare Zusatzdetails duerfen den minimalen Latchnachweis
nicht ersetzen. Wenn die begrenzte Storekapazitaet den vollstaendigen
notwendigen Record nicht sicher aufnehmen kann, wird Klasse 4 gesetzt und
die Aktorfreigabe bleibt gesperrt; Ursachen werden nicht still verworfen.

### 9.2 Setzen eines Latches

Bei einem Safety-/Systemfehler gilt die Reihenfolge:

1. neue Aktorfreigabe blockieren und sichere unmittelbare Reaktion ausfuehren;
2. RAM-Latch und Faultrevision setzen;
3. vollstaendigen neuen SafetyStateRecord schreiben;
4. den Writeausgang gemaess IStateStore auswerten und bei unbestimmtem
   Ausgang gezielt zuruecklesen;
5. erst nach eindeutigem Nachweis die persistente Recordrevision als
   bestaetigt publizieren.

Ein Fehler vor oder waehrend dieser Persistenz loescht den RAM-Latch nicht.
Er erzeugt Y4-005, bleibt im sicheren Ausgang und darf nicht automatisch
weiterbooten oder den Latch durch einen zweiten unkontrollierten Schreibversuch
maskieren.

### 9.3 Latch beim Boot und beim Recovery

Vor jeder normalen Runtime- oder Aktorfreigabe werden:

1. sichere Ausgangszustaende hergestellt;
2. der Record geladen und Schema, Laenge, CRC, Revision und semantische
   Vollstaendigkeit geprueft;
3. persistente Latches und offene Restart-Episode ausgewertet;
4. Konfigurations-, Run-, Sensor- und Gate-Status qualifiziert;
5. erst danach der bestehende Prozesszustandsautomat fortgesetzt.

NotFound darf nur im einmalig nachgewiesenen fabrikneuen Bootstrapfall als
leerer Safety-State behandelt werden. Dieser Nachweis verwendet die
bestehende #57-StorageEpoch-/Bootstrapsemantik unveraendert: alle
verpflichtenden Bootstrap- und Konfigurationsreads muessen technisch
erfolgreich sein, #57 muss den expliziten fabrikneuen beziehungsweise
initialisierten Zustand melden, und alle #24-Recordreads muessen NotFound
sein. #24 dupliziert oder deutet StorageEpoch nicht in place um. Erst dann
wird der leere initiale SafetyStateRecord unter sicheren Ausgaengen
geschrieben, rueckgelesen und als committed bestaetigt.

Ausserhalb dieses exakt nachgewiesenen Falls ist NotFound kein clean state.
ReadError, CapacityError, WriteError mit fehlender sicherer Vorrevision,
ungueltige Bytes, unbekannte Revision, semantischer Widerspruch und
unaufgeloester Commitzustand fuehren fail-closed zu Y4-005 oder Y4-011 und
zu SAFE_BOOT.

Die Storeausgaenge werden getrennt behandelt: Bei WriteError oder
CapacityError bleibt nur dann sicher bekannt, dass der alte Record unveraendert
ist, wenn der bestehende Storevertrag dies bestaetigt; die RAM-Safetywirkung
bleibt trotzdem aktiv und es gibt keine Freigabe. Bei CommitOutcomeUnknown
wird der erwartete neue Record mit seiner Revision rueckgelesen. Nur ein
exakter Readback loest den Commit eindeutig auf. Alter Record, fehlender
Record, ReadError, Korruption oder jede Abweichung bleiben Y4-005/Y4-011,
SAFE_BOOT und ohne Aktorfreigabe. Ein fehlgeschlagener Safetywrite darf in
derselben Laufzeit nie zu einer Freigabe fuehren.

Das Loeschen eines Latches ist eine fachliche State-Transition. Die neue
Resetabsicht wird vor der Anwendung geschrieben, eindeutig rueckgelesen und
erst dann in RAM, Prozesszustand und Aktor-Gate sichtbar. Eine einzelne
erfolgreiche Detailpruefung, Quittierung oder ein Neustart darf den Latch nicht
loeschen.

## 10. Restart-Episode und SAFE_BOOT

### 10.1 Restartursachenmatrix

Die Implementierung muss die konkrete Matrix gegen die vom realen
Plattformadapter gelieferten Resetgruende abgleichen. Sie darf keinen
Resetgrundnamen oder ESP-IDF-Wert raten. Die fachliche Einordnung lautet:

| Plattformnachweis | Episodezaehler | Fault-/Bootwirkung |
|---|---:|---|
| normaler Power-on ohne persistierte abnormale Evidenz | nein | normaler Boot nur nach uebrigen Qualifikationen |
| firmwarekontrollierter Neustart wegen schwerem System-/Safetyfehler | ja | Write-before-Restart; folgende Bootphase wertet die persistierte Evidenz aus |
| Watchdog-, Panic- oder vergleichbarer abnormaler Reset | ja | Evidenz vor normaler Freigabe nachfuehren; bei drittem Ereignis SAFE_BOOT |
| eindeutig klassifizierter Brownout-/Versorgungsreset | ja | RESTART_BROWNOUT als Restart-Evidenz vor normaler Freigabe nachfuehren; Ereignis 1 oder 2 allein erzeugt keinen Klasse-4-Latch |
| autorisierter Service-, Firmwareupdate- oder Recovery-Neustart, dessen eigener Vertrag ihn als normal klassifiziert | nein | keine neue Episode; offene Episode bleibt trotzdem offen |
| unbekannter, widerspruechlicher oder nicht sicher klassifizierbarer Grund | nicht als harmlos behandeln | Y4-006/Y4-011, persistenter fail-closed Zustand und SAFE_BOOT vor normaler Freigabe |

Ein normaler, autorisierter Reset darf eine offene Episode nicht abschliessen.
Weitere Resetursachen duerfen nur nach expliziter #24-Klassifikation in die
Matrix aufgenommen werden. Ein nicht aufloesbarer Plattformwert ist kein
normaler Reset.

### 10.2 Episodealgorithmus und Exactly-once-Evidenz

Die fachliche Wahrheit ist eine persistente Episode, nicht ein
Wall-Clock-Fenster. Jede abnormal zaehlende Evidenz besitzt eine stabile
RestartEvidenceId aus der persistenten Episode- und Ereignissequenz. Die
Mutation wird vor Ausfuehrung auf Ueberlauf geprueft.

1. Ein sicher als abnormal klassifizierter Neustart erzeugt oder erhoeht die
   offene Episode genau einmal. Bei einem kontrollierten Restart wird die
   EvidenceId vor dem Reset im SafetyStateRecord als Pending und nach
   eindeutigem Commit als Committed gespeichert.
2. Der folgende Boot liest den einmalig gecachten Resetgrund und die
   persistierte EvidenceId. Ein passender
   RESTART_CONTROLLED_SAFETY-Grund bestaetigt und konsumiert die committed
   Evidenz als Consumed; er erhoeht den Zaehler nicht nochmals.
3. Passt der Plattformgrund nicht zur Pending-/Committed-Evidenz, fehlt die
   erwartete Evidenz oder ist der Status nicht eindeutig, wird die
   Abweichung als Unknown behandelt. Es gibt keine zweite Zaehler-Mutation
   und keinen automatischen Wiederholungsreset; der Boot bleibt
   fail-closed in SAFE_BOOT mit Y4-006 oder Y4-011.
4. Bei Watchdog oder Brownout ohne Vorab-Write wird im Bootpfad eine neue
   EvidenceId erzeugt, der Zaehler einmal erhoeht und der neue Record vor
   jeder normalen Freigabe geschrieben und bestaetigt. Ein sicherer erster
   oder zweiter Brownout-/Watchdognachweis ist Diagnose-/Episode-Evidenz,
   aber kein Klasse-4-Latch allein.
5. Ein Zaehler von drei setzt safeBootRequired fuer den folgenden Boot.
   Ist die Episode offen und der Zaehler kleiner als drei, darf ein normaler
   Boot nur nach vollstaendiger Safety-/Konfigurationsqualifikation in
   STANDBY beziehungsweise den bestehenden normalen Prozesspfad gehen.
6. Dieser qualifizierte Boot startet eine fluechtige monotone
   Stabilitaetsmessung von 30 Minuten. SAFE_BOOT, Fault, unaufgeloeste
   Konfiguration, aktive Klasse-3-/Klasse-4-Ursache, fehlerhafte
   Safety-Aufgabe oder fehlende Persistenzqualifikation sind kein stabiler
   Betrieb.
7. Vor Ablauf der 30 Minuten bleibt die Episode offen, auch nach normalem
   Neustart oder langer Stromunterbrechung.
8. Nach 30 Minuten stabiler, abnormal-restartfreier Laufzeit wird die
   Episodenschliessung als neue persistente Revision geschrieben,
   rueckgelesen und erst bei eindeutigem Commit in RAM angewendet.
9. Schlaegt die Schliessung fehl oder bleibt ihr Commit unbestimmt, bleibt
   die Episode offen; ein kritischer Persistenzfehler darf keine Freigabe
   durch Zeitablauf vortaeuschen.
10. Beim dritten abnormalen Neustart der noch offenen Episode waehlt der
    folgende Boot SAFE_BOOT, bevor normaler Lauf oder normale Aktorfreigabe
    moeglich wird.

Ein firmwarekontrollierter Restart wird fuer dieselbe interne
FaultInstanceId hoechstens einmal automatisch angefordert. Das
controlledRestartUsed-Merkmal wird zusammen mit der Fault-/Episoderevision
persistiert und verhindert eine Neustartschleife derselben Ursache. Ein
Schreibfehler oder unklarer Commit erlaubt keinen Retry; die sichere
Reaktion ist Y4-005 beziehungsweise Y4-011 und SAFE_BOOT. Die Drei-
Ereignisse-Episode ist kein Freibrief fuer wiederholte kontrollierte
Softwareneustarts.

Die 30 Minuten sind in Release 1 eine private firmwarefeste Konstante des
Safetykerns, nicht Teil der Servicekonfiguration oder des
Konfigurationsschemas. Die Implementierung darf dafuer keinen NTP-Sync und
keine externe RTC voraussetzen.

### 10.3 Write-before-Restart

Alle kontrollierten Neustarts laufen ueber einen zentralen Resetpfad mit
explizitem Zweck:

1. Fuer einen automatischen Safety-/Systemrestart wird geprueft, dass die
   konkrete FaultInstanceId noch aktiv ist, ihr
   controlledRestartUsed-Merkmal nicht gesetzt ist und kein dritter
   Episodenzaehler bereits SAFE_BOOT erzwingt.
2. Safety-Disposition auf sicheren Ausgang setzen und die
   RestartEvidenceId, den erwarteten
   RESTART_CONTROLLED_SAFETY-Grund, FaultInstanceId und neue Revision in den
   SafetyStateRecord aufnehmen.
3. Den Record schreiben und den Ausgang gemaess IStateStore auswerten.
   CommitOutcomeUnknown wird durch Readback der exakt erwarteten Revision
   aufgeloest; nur ein exakter Readback darf fortsetzen.
4. Erst nach eindeutigem Commit genau einen kontrollierten Reset anfordern.
   Der Port meldet nur Accepted, Rejected oder OutcomeUnknown; die
   Plattformmeldung ersetzt nicht die persistierte EvidenceId.
5. Bei WriteError, CapacityError, ReadError oder unaufloesbarem Commit wird
   kein automatischer Rebootversuch wiederholt. Die Runtime bleibt sicher
   aus und setzt Y4-005/Y4-011 fail-closed.

Ein echter Watchdog oder Brownout kann diese Reihenfolge nicht ausfuehren.
Der naechste Boot muss deshalb Resetgrund und persistente Vorrevision
kombinieren, die Evidenz einmalig nachtragen und vor normaler Freigabe
eindeutig committen. Ein fehlender oder widerspruechlicher Nachweis fuehrt
zu SAFE_BOOT, nicht zu einer geratenen normalen Freigabe.

Ein autorisierter Fehlerreset verwendet dagegen die in 8.3 beschriebene
FaultResetBootIntent und den Resetgrund RESTART_AUTHORIZED. Dieser Reset ist
kein abnormales Episodenereignis, wird einmalig konsumiert und loescht die
Episode nicht. Die beiden Zwecke teilen den zentralen Portpfad, aber nicht
ihre Safety-Semantik.

### 10.4 Bootprioritaet und SAFE_BOOT

Der bestehende Prozesszustandsautomat wird in dieser Reihenfolge erweitert:

1. Ausgaenge sicher deaktivieren; kein alter GPIO- oder Direktzustand wird
   wiederhergestellt.
2. Resetursache einmalig erfassen und kontrollierte Restart- oder
   FaultResetBootIntent aus dem Safety-Record lesen.
3. Safety-Record, Restart-Episode und persistente Latches pruefen; passende
   controlled-restart-Evidenz exakt einmal konsumieren beziehungsweise
   fehlende oder widerspruechliche Evidenz fail-closed behandeln.
4. Konfiguration, Run-Recovery, Firmware-/Schema-Integritaet, benoetigte
   Sensor-/Ressourcenqualifikation und das
   CONFIGURATION_SAFETY_INTEGRATION_GATE pruefen.
5. Restart-Evidenz beziehungsweise Episodenschliessung gemaess
   Write-before-Apply verifizieren und Projektionen aus der
   Safetyautoritaet neu aufbauen.
6. Bei einem dominanten Latch, drittem abnormalem Restart,
   unaufgeloester Konfiguration oder Unknown-Zustand in SafeBoot uebergehen.
   Ein sicher klassifizierter erster oder zweiter Brownout-/Watchdogreset
   fuehrt ohne eigenstaendige aktive Ursache nach vollstaendiger
   Qualifikation nicht allein zu SafeBoot.
7. Nur bei vollstaendigem Gate und, falls vorhanden, einmalig gueltigem
   FaultResetBootIntent in die bestehende normale
   Boot-/RecoveryEvaluation-/STANDBY-Kette uebergehen. Es gibt keine direkte
   ProcessTransition von Fault oder SafeBoot nach Standby; der autorisierte
   Reset-Boot ist der einzige Exitpfad.

Der ProcessState-Wert ist dabei nur die Laufzeitprojektion des zentralen
Safety-/Bootentscheids. Es gibt keinen separaten ProcessState-Commit, der
einen SafetyStateRecord-Reset ersetzt oder mit ihm konkurriert; der
FaultResetBootIntent wird im Boot genau einmal konsumiert und danach der
bestehende Boot-Transitionpfad verwendet.

In SAFE_BOOT:

- bleiben Peltier und H-Bruecke aus;
- wird kein Lauf automatisch fortgesetzt;
- bleiben direkte Aktor- und Leistungstests sowie SERVICE_MODE gesperrt;
- sind passive Diagnose und geschuetzte Recovery nur soweit der
  Systemzustand stabil genug ist erlaubt;
- wird der dominante Grund lokal und strukturiert sichtbar;
- verlaesst ein normaler Neustart SAFE_BOOT nicht;
- wird die Freigabe nur ueber den bestehenden #24-Fehlerresetvertrag, den
  verifizierten FaultResetBootIntent und eine neue Safetyrevision moeglich.

Ein physischer Recovery-Trigger, konkrete PIN-/Touchwerte oder Hardwarepegel
werden in #24 nicht erfunden.

## 11. Integration der bestehenden Producer

### 11.1 #23-Watchdog-Evidence

Der #24-Kern konsumiert die reale ActuatorWatchdogFaultEvidence:

- detectedAtMonotonicMillis und
  lastObservedSequenceHighWatermarkBeforeFault bleiben unveraendert und
  werden als Diagnosemetadaten erhalten;
- latchedWatchdogFault aus #23 wird als S3-008 mit der vorhandenen
  Evidenz auf den zentralen Fault abgebildet;
- #24 startet keinen zweiten Aktorwatchdog und misst keine parallele
  Watchdogepisode;
- Quittierung, Rebase, neuer Lauf, Recovery oder Neustart loeschen das
  #23-Latch nicht;
- erst ein erfolgreiches #24-ResetFault darf nach allen Checks
  ActuatorPlanner::applyExternalWatchdogFaultReset() aufrufen;
- der Reset wird zusammen mit der Safetyrevision und dem Gatezustand
  persistiert beziehungsweise verifiziert;
- der bestehende #23-Aktorplaner bleibt bei Unresolved oder
  ImmediateStop nicht freigabefaehig.

### 11.2 CONFIGURATION_SAFETY_INTEGRATION_GATE

Das Gate wird ueber die echten Producer und nicht ueber simulierte Ersatztypen
abgenommen:

| realer Producer | #24-Code | Wirkung |
|---|---|---|
| ConfigurationRuntimeFailure | Y4-001 | Runtime bleibt fail-closed; persistenter System-Latch; keine normale Aktorfreigabe |
| ConfigurationCommitIndeterminate / CommitOutcomeUnknown | Y4-002 | weder alter noch neuer Graph wird geraten; keine Runtime, Mutation, Slotwiederverwendung oder Aktorfreigabe |
| ConfigurationUnavailable | Y4-003 | keine Runtimefreigabe; System-Latch und SAFE_BOOT/Recoveryprioritaet |
| ConfigurationIntegrityFailure | Y4-004 | keine Runtimefreigabe; System-Latch und SAFE_BOOT/Service |

Die originalen Producer-Ursachen, insbesondere
ConfigurationRuntimeFailureCause und
ConfigurationCommitIndeterminateCause, bleiben Detailmetadaten und werden
nicht in eine zweite Konfigurationsfehlerhierarchie umbenannt.
ResetEligibleNoRuntime loescht keinen #24-Latch.

Das Gate ist erst bestanden, wenn fuer jeden Producer nachgewiesen ist:

- reale Eingabe kommt am #24-Kern an;
- sofortige Aktorsperre und sichere Bootprioritaet folgen;
- Neustart behaelt die notwendige Verriegelung;
- unbekannter oder unaufgeloester Zustand erzeugt keine Allowed-Eingabe;
- Recovery erfolgt nur ueber die #24-Reset-Evaluation;
- reale Aktoradapter koennen das Gate nicht umgehen;
- die zugehoerige Fehlerinjektion ist reproduzierbar automatisiert.

### 11.3 Typisierte #24-Ereignisprojektion auf IEventJournal

Issue #24 fuehrt keine zweite Journalplattform und keinen strukturierten
Faultspeicher im Journal ein. Der vorhandene Port
`IEventJournal::record(monotonicMillis, message)` bleibt unveraendert und
besitzt weder Lesesemantik noch Aufbewahrungs- oder Retentionsgarantie.

Der #24-Kern erzeugt davor eine feste `FaultEventProjection` mit mindestens
folgenden Ereignistypen:

- FaultCreated oder FaultEscalated;
- FaultCauseCleared;
- FaultAcknowledged;
- FaultResetCommitted oder FaultResetRejected;
- RestartEpisodeAdvanced oder RestartEpisodeClosed;
- SafeBootEntered;
- SafeBootExitDecided beziehungsweise SafeBootExitRejected;
- SafetyRecoveryAttempted, SafetyRecoveryAborted oder
  SafetyRecoverySucceeded.

Jede Projektion traegt, soweit fachlich vorhanden, stabilen Faultcode,
FaultInstanceId, Faultrevision, Primary-FaultInstanceId,
RestartEvidenceId/EpisodeId, dominanten Grund und die Resetentscheidung.
Die Serialisierung in die bestehende Message-Schnittstelle ist
deterministisch: fester Ereignistyp, feste Feldreihenfolge, feste
Schluesselwerte und keine unbounded Freitexthistorie. Die Projektion ist
Diagnoseausgabe; der SafetyStateRecord bleibt die Autoritaet.

Ein Fehler des Journalports verhindert weder die unmittelbare
Safetyabschaltung noch den Safety-/Restart-Commit. Der Fehler wird selbst
als nichtfreigaberelevante Diagnose behandelt, soweit der bestehende Port
das zulaesst. Aufbewahrung, Bereinigung, Backup, Exporthistorie und
allgemeine Journalspeicherpolitik bleiben Issue #19. Native Tests pruefen
die benoetigten #24-Projektionen, deterministische Texte und den sicheren
Weiterlauf bei Journalfehlern, ohne eine neue Journalplattform zu bauen.

### 11.4 Zyklusfreie Gateabschluss-Semantik

Der #24-Nachweis gegen Aktor-Bypass endet bewusst an der bereits vorhandenen
Application-Grenze:

`#24 Safety-Core -> ActuatorSafetyGateInput ->
temperature_control_orchestrator -> ActuatorPlanner ->
ActuatorPlanSinkDriver`.

Der Safety-Core ist die einzige Quelle fuer die pro Zyklus verwendete
Safety-Disposition. Der bestehende ActuatorPlanner bleibt die einzige
Quelle fuer einen angewendeten ActuatorPlanTickResult; der Sink erhaelt
keinen rohen #22-ControlRequest und keine vom Aufrufer gesetzte
Safetyfreigabe. Falls die bestehende Ergebnisstruktur fuer die Ausfuehrung
einen Herkunftsnachweis benoetigt, wird sie um einen schmalen, typisierten
`PlannerQualifiedDisposition`-Nachweis erweitert, der nur im Planner
entsteht und vom Sink auf `Allowed`, `ImmediateStop` oder
`SAFETY_RECOVERY` geprueft wird. Das ist keine zweite Gate-Abstraktion und
keine parallel laufende Aktor-State-Machine.

Native Architektur-, Compile- und Integrationstests sichern an dieser
Grenze:

- Unresolved, ImmediateStop und unbekannte Dispositionen erzeugen keinen
  aktiven Plan am Sink;
- normale ControlRequests koennen den Safety-Eingang nicht ueberschreiben;
- SAFETY_RECOVERY erreicht den Sink nur als intern erzeugte, vollstaendig
  qualifizierte SafetyRecoveryRequest ueber den bestehenden Planner;
- ein direkter Test-/Adapteraufruf ohne Planner-Qualifikation ist
  strukturell abgewiesen oder nicht Teil der Produktionskomposition.

Damit wird in #24 die reale Application-/Planner-/Sink-Grenze geprueft,
ohne einen noch nicht vorhandenen Hardwareadapter zu behaupten. #106 bleibt
das spaetere Gate fuer produktive Plannerparameter und Per-Run-Bindung.
#32/#33 muessen denselben Nichtumgehungsvertrag bei ihren spaeteren
ESP-IDF-/Hardwareadaptern erneut nachweisen; dieser Nachweis ist kein
Zyklus und kein Bestandteil der R2-Implementierung.

## 12. Fail-closed-Regeln fuer unbekannte Zustaende

Folgende Faelle werden nicht heuristisch geheilt:

- unbekannter Faultcode, Klassenwert, Resetgrund, Producerstatus oder
  Record-Schema;
- fehlende Pflichtfelder, ungueltige Revision, CRC-/Laengenfehler oder
  widerspruechliche Primaer-/Folgebeziehung;
- CommitOutcomeUnknown, nicht ruecklesbarer Write oder unvollstaendiger
  Readback;
- ActuatorSafetyGateStatus::Unresolved;
- fehlende aktuelle Sensor-/Aktor-/Safety-Evidenz;
- unbekannter Prozesszustand oder nicht beweisbare Recovery;
- fehlender Nachweis, dass eine kontrollierte Restart-Evidenz committed ist.

Die Reaktion ist sichere Abschaltung, Klasse 4, RAM-Latch und, soweit
moeglich, persistenter Latch. Ein fehlerhafter Persistenzversuch rechtfertigt
keinen zweiten unkontrollierten Reboot und keine Rueckkehr zu Allowed.

## 13. Reproduzierbare Software-Fehlerinjektion

Die Injektionen bleiben in device_platform_test_support und Testfixtures.
Produktionsmodule referenzieren keinen Test-Support. Die Tests verwenden
virtuelle Zeit, deterministische Resetereignisse und den bestehenden
SimulatedPersistentStateStore.

| Injektionsgruppe | reproduzierbarer Stimulus | erwartetes Orakel |
|---|---|---|
| Sensor | Luft-, Produkt- und Kuehlfuehler: CRC-, Bus-, Stale-, Jump-, Conflict-, Failed- und fehlender Ersatzpfad | exakt erwarteter Code, Klasse, Peltier-/Luefterreaktion, Latch und Resetstatus |
| Temperatur / Safety-Limit | qualifiziertes Intervention-, Hard-Limit- und fehlendes Reaktionsereignis ohne erfundene Grenzwerte | Klasse 3, sichere Richtungssperre, keine automatische Latchfreigabe |
| Aktor | stale Request, simultane Richtung, Enable-/Outputfehler, Feedbackfehler, fehlende Reaktion, Servicepulsabbruch | #23-Gate ImmediateStop, S3-007 oder S3-008, kein Adapter-Bypass |
| #23 Watchdog | echte ActuatorWatchdogFaultEvidence mit Sequenz-High-Watermark | ein zentraler Fault, persistente Wirkung, Reset nur ueber #24 |
| Persistenz | ReadError, CapacityError, WriteError, Cut vor Commit, Cut nach Commit, CommitOutcomeUnknown, CRC-/Bytekorruption, NotFound | Write-before-Apply, RAM-Latch, kein Rebootloop, SAFE_BOOT bei unklarem Boot |
| Konfiguration | reale #56/#57-Producer und ConfigurationCommitIndeterminateCause | Y4-001 bis Y4-004, keine Runtime und keine normale Aktorfreigabe |
| Brownout / Restart | virtueller Boot mit eindeutigem Brownout, Watchdog, Panic, kontrolliertem Reset, Power-on und unbekanntem Resetgrund | korrekte Episodeklassifikation, persistenter Zaehler, Bootprioritaet |
| Restart-Episode | erster, zweiter und dritter abnormaler Reset; normaler Reset bei offener Episode; 29:59/30:00/30:01 monotone Stabilitaet | Zaehler bleibt korrekt, dritte Ursache fuehrt vor Freigabe zu SAFE_BOOT, stabile 30 Minuten schliessen nur mit Readback |
| SAFE_BOOT | persistierter Latch, dritter Restart, unaufgeloeste Konfiguration, unklarer Commit, normaler Neustart in SAFE_BOOT | kein Lauf, kein Service-/Aktorleistungstest, keine automatische Entsperrung |
| Commands | Acknowledge, Mute, Reset ohne Berechtigung, Reset bei aktiver Ursache, stale Revision, gueltiger Reset | Quittieren, Stummschalten und Reset bleiben getrennt |
| Primaer/Folge | zwei zeitlich gekoppelte und zwei unabhaengige Ursachen | beide Records erhalten; Dominanz und Reset je Fault korrekt |

Die Restarttests duerfen die 30 Minuten durch VirtualTimeSource vorspulen.
Sie testen keine reale Hardware und benoetigen weder NTP noch RTC.

## 14. Teststrategie und wenige aussagekraeftige Matrizen

Die Umsetzung verwendet fuenf vertikale Matrizen statt einer Vervielfachung
identischer Einzeltests:

### Matrix A – reiner Faultkern

- alle stabilen Codes und Klassen;
- Gleichzeitigkeit und Dominanz;
- Primaer/Folge;
- Statusfolge aktiv, quittiert, Ursache beseitigt, resetberechtigt,
  geloescht;
- unbekannter Code und ungueltige Klasse.

### Matrix B – Commands und Safety-Ausgang

- Acknowledge ohne Reset;
- Mute ohne Status- oder Gateaenderung;
- abgelehnter und gueltiger ResetFault;
- Klasse-1-, Klasse-2-, Klasse-3-, Klasse-4-Ausgang;
- Allowed, ImmediateStop und Unresolved am bestehenden #23-Gate;
- keine Aktorfreigabe aus SAFE_BOOT.

### Matrix C – Persistenz und Recovery

- erfolgreicher Latchwrite und Boot mit Latch;
- WriteError, CapacityError, ReadError, Korruption;
- CommitOutcomeUnknown vor und nach Readback;
- Latch bleibt nach Ack, Mute, Resetversuch und Neustart;
- Resetfreigabe erst nach verifizierter neuer Revision;
- Episodezaehler und kontrollierter Restart Write-before-Restart;
- fehlender Vorab-Write bei Brownout/Watchdog und Boot-Nachtrag.

### Matrix D – Restart-Episode

- Power-on ohne Evidenz;
- abnormaler Restart 1 und 2;
- dritter abnormaler Restart -> SAFE_BOOT;
- normaler Neustart vor Ablauf von 30 Minuten;
- stabile 30 Minuten und persistierte Episodenschliessung;
- Schreibfehler beim Episodenschluss;
- alte offene Episode nach langer Stromlosigkeit;
- normaler Neustart in SAFE_BOOT.

### Matrix E – reale Integrations- und Quellenmatrix

- alle Pflichtsensor- und Aktorinjektionen;
- #23-Watchdog-Evidence und externer Reset;
- ConfigurationRuntimeFailure;
- ConfigurationUnavailable;
- ConfigurationIntegrityFailure;
- ConfigurationCommitIndeterminate;
- reale Gatewirkung gegen den Aktorplaner;
- Unknown-/Unresolved-Eingaben.

Die fuenf Matrizen erhalten dabei diese schaerfenden Orakel, ohne neue
Testframeworks oder parallele Faultmodelle einzufuehren:

| Matrix | zusaetzliche verbindliche Orakel |
|---|---|
| B – Commands und Safety-Ausgang | S3-004 beginnt mit ImmediateStop; nur vollstaendig qualifizierte, typisierte SAFETY_RECOVERY darf den bestehenden Planner erreichen; S3-005, unsichere Pflichtsensoren sowie Luefter-/Aktorfehler verhindern Recovery; Latch und normale Allowed-Freigabe bleiben getrennt |
| C – Persistenz und Recovery | erster fabrikneuer Initialrecord; NotFound ausserhalb des nachgewiesenen #57-Bootstrapfalls fail-closed; bounded Capacity/Overflow; WriteError, ReadError, Korruption und CommitOutcomeUnknown; Safetyprojektionen werden aus der persistenten Autoritaet rekonstruiert |
| C/D – Restart-Episode | Brownout/Watchdog 1, 2 und 3; bei 3 SAFE_BOOT; Pre-Write plus passender Boot zaehlt genau einmal; Hardwarereset ohne Pre-Write zaehlt einmal nach; Mismatch bleibt fail-closed; dieselbe interne Ursache erzeugt keinen automatischen Restartloop |
| B/C – Reset-Trust-Boundary | caller-supplied positive FaultResetEvaluation wird ignoriert beziehungsweise ist nicht mehr eingabefaehig; stale/falsches Faultziel wird abgelehnt; ein Reset loescht keinen anderen Latch; #23-Watchdogreset erfolgt erst nach dem bestaetigten #24-Commit |
| D/E – ProcessState und SAFE_BOOT | der einzige Exit ist FaultResetBootIntent -> zentraler autorisierter Neustart -> Bootqualifikation -> bestehender Standby-/Recoverypfad; normaler Reboot hebt keinen Latch; SAFE_BOOT erlaubt weder SERVICE_MODE noch Aktortest |

Die Journalpruefung in Matrix E umfasst die typisierten #24-Ereignisse und
einen fehlerhaften IEventJournal-Port. Ein Journalfehler darf den sicheren
Ausgang und den SafetyStateRecord-Commit nicht verhindern.

Bestehende #15-, #17-, #18-, #20-, #21- und #23-Tests werden nur um direkte
Konsumentenasserts erweitert. Sie erhalten keine parallele Faultlogik.
Testergebnisse werden nach docs/CI_AND_QUALITY_GATES.md als PASS, FAILED,
NOT_RUN oder BLOCKED ausgewiesen. Hardwaretests bleiben separate, explizit
beauftragte Nachweise.

## 15. Kleine, reviewbare Umsetzungsschnitte

Nach Ownerfreigabe genau dieses Plan-Commits erfolgt die Umsetzung in
folgenden reviewbaren Schnitten. Jeder Slice bleibt auf die benoetigten
Module und gezielten Konsumententests begrenzt:

1. **Faulttypen und Dominanz:** stabile Klassen, Codes, Datensaetze,
   Dominanz, Unknown-Fallback und reine native Orakel.
2. **Command-/Messageintegration:** bestehende Messages, Revisionen,
   Acknowledge, Mute, der auf FaultInstanceId und erwartete Revision
   reduzierte ResetFault sowie FaultResetEvaluation und
   criticalSafetyEventPending nur als Projektionen.
3. **Persistenter SafetyStateRecord:** bounded Recordcodec mit stabilen
   FaultInstanceIds, Primary-/Follow-up-Referenzen, Overflowschutz,
   IStateStore, Latchsetzen, Readback, Write-before-Apply,
   FaultResetBootIntent und Recoveryreset.
4. **Resetport und Restart-Episode:** genau ein schmaler
   device_platform-Resetport mit dem definierten Snapshot-/Resultatvertrag,
   native Testunterstuetzung, Resetmatrix, persistenter Zaehler,
   Exactly-once-Evidenz, per-Fault-Hoechstens-einmal-Neustart,
   30-Minuten-Stabilitaetsfenster und Write-before-Restart. Die reale
   ESP-IDF-Abbildung und `esp_restart()` bleiben #29/E5.
5. **Boot- und SAFE_BOOT-Integration:** bestehende ProcessState-/Event-
   Maschine, Bootprioritaet, der einzige autorisierte Reset-Boot-Exitpfad,
   kein normaler Neustartausstieg, aktorfreie Recovery.
6. **#23-Safety-Gate:** ActuatorWatchdogFaultEvidence, vorhandener
   Watchdog-Latchreset, zentrale Safety-Disposition, typisierte
   SAFETY_RECOVERY und Nachweis gegen Allowed/Unresolved.
7. **#56/#57-Gate:** reale Producerstatus, Ursachenmetadaten,
   ConfigurationCommitIndeterminate und End-to-End-Recovery.
8. **Injektions- und Akzeptanzmatrix:** deterministische Fixtures,
   Restart-/Persistenz-Cut-Points, Reset-Trust-Boundary, Safety-Recovery-,
   Journal-, Konsumenten- und Integrationsorakel an den fuenf Matrizen.
9. **Dokumentationsabschluss:** Safety-, Recovery-, Diagnose-,
   Akzeptanz- und Roadmapstatus auf den finalen Implementierungsstand
   synchronisieren.

Jeder Commit muss seinen Scope, gezielte Tests und den Status offen ausweisen.
Ein Fehler in einem frueheren Slice wird nicht durch spaetere Tests
ueberschrieben. Ein vollstaendiger lokaler Lauf bleibt bis zum separaten
Ownerauftrag nach Review ausgeschlossen.

## 16. Modul- und Architekturgrenzen

### device_platform

- der exakt in Abschnitt 4 definierte anwendungsneutrale
  Resetursachen-/kontrollierte-Neustart-Port, sofern bei der Umsetzung kein
  inzwischen vorhandener gleichwertiger Port gefunden wird;
- bestehende Zeit- und Persistenzports unveraendert wiederverwenden;
- keine Fermentationsbegriffe, Faultcodes oder SAFE_BOOT-Policy.

### device_platform_esp_idf

- in R2 keine Aenderung. Die sichere Auswertung real verfuegbarer
  ESP-IDF-Resetursachen, die Abbildung auf den abstrakten Snapshot und der
  kontrollierte `esp_restart()`-Adapter werden im E5-Bring-up-Gate #29
  verankert;
- keine fachliche Klassifikation, Latch- oder Bootentscheidung;
- keine GPIO-/Aktorfreigabe aus #24.

### fermentation_app

- einziger Faultkern, Fehlercodevertrag, Dominanz, Latch,
  Restart-Episode, Boot-/Recoveryentscheidung und Safety-Disposition;
- konsumiert Sensor-, Konfigurations-, Run- und #23-Vertraege;
- projiziert auf vorhandene Commands, Prozesszustand und #23-Gate;
- keine direkten ESP-IDF- oder GPIO-Abhaengigkeiten.

### device_platform_test_support

- Resetursachen, Restart, virtuelle Zeit, Persistenz-Cut-Points und
  Hardware-/Sensor-/Aktor-Mocks;
- keine Produktionsabhaengigkeit und keine versteckte Fachklassifikation.

## 17. Grenzen zu anderen Issues

### #106

#24 definiert die Safety-Wirkung von Konfigurations-, Run-, Aktor- und
Recoveryfehlern sowie deren Latch-, Gate- und SAFE_BOOT-Folgen. #24
implementiert keinen Aktorplaner-Per-Run-Parametersnapshot, keine
Snapshotbindung und keine Recovery-Bindung aus #106. #106 bleibt ein
separates spaeteres Integrationsgate. #24 darf weder dessen Snapshotmodell
vorwegnehmen noch eine neue Faultklassifikation dafuer erfinden.

### #35

#35 bleibt TBD_COMMISSIONING und offen. #24 setzt keine produktiven
Temperaturgrenzen, Sensorqualitaetszeiten, Aktorzeiten, Luefternachlaeufe,
Servicewerte, PIN-/Touchwerte oder sonstige Commissioningwerte. Der Faultkern
konsumiert dafuer qualifizierte Ereignisse aus den bestehenden Vertraegen.

### Hardware und GPIO

Reale Resetpegel, Bootloaderpegel, GPIOs, H-Brueckenverhalten,
Luefterrueckmeldungen, 12-V-Messung und thermische Nachweise bleiben
Hardware-/Bring-up-Gates. SAFE_BOOT blockiert direkte Aktortests in #24.
Ein erfolgreicher nativer Test ersetzt keinen Hardware-Nachweis.

### #19 und spaetere Diagnosearbeit

#24 erzeugt strukturierte Fault-/Safetyereignisse ueber den bestehenden
Event-/Journalvertrag, implementiert aber nicht die vollstaendige
Journalaufbewahrung, Bereinigung, Backup- oder Importlogik aus #19.

## 18. Dokumentations- und Roadmapwirkung

In dieser Planrevision werden keine normativen Fachquellen umgeschrieben.
Der bereits erfolgte Roadmap-Stand wird in dem Plan-Commit auf den nun
realen Zustand synchronisiert:

- PR #105 und Issue #23 bleiben als abgeschlossen sichtbar;
- #24 ist aktuelle fachliche Arbeit mit Planrevision 2 und
  IMPLEMENTATION: NOT_STARTED;
- die Reihenfolge #23 -> #24 -> #19 bleibt unveraendert;
- #106 bleibt als separates spaeteres Integrationsgate sichtbar;
- #35 bleibt als TBD_COMMISSIONING offen;
- Draft-PR #107 bleibt Draft und erzeugt keine Firmware-CI.

Nach der Implementierung werden mindestens folgende Dokumente nur um
belegte Ergebnisse, Codes, Nachweise und Status aktualisiert:

- docs/SAFETY_AND_FAULTS.md;
- docs/SYSTEM_SAFETY_AND_RECOVERY.md;
- docs/SAFETY_COMPONENT_FAULTS.md;
- docs/ACCEPTANCE_TESTS.md;
- docs/DIAGNOSTICS_AND_MAINTENANCE.md;
- gegebenenfalls docs/DECISIONS.md, falls die neue Plattformport- oder
  Recordarchitektur ein ADR erfordert;
- docs/ROADMAP.md nur als Status-/Gateuebersicht, ohne Anforderungstexte
  zu duplizieren.

## 19. Offene Punkte und Freigabegates

Die materielle Ownerentscheidung zur Restart-Episode ist mit Abschnitt 1
geschlossen. Vor der Implementierung bleiben keine weiteren fachlichen
Ownerentscheidungen in diesem Plan verborgen.

Folgende Punkte sind technische Nachweise beziehungsweise harte
Implementierungsgates, keine frei zu erfindenden Werte:

- die tatsaechlich vom unterstuetzten ESP-IDF-Profil gelieferten
  Resetursachen muessen im Adapter nachgewiesen werden;
- nicht sicher unterscheidbare Plattformwerte muessen Unknown und damit
  fail-closed sein;
- der SafetyStateRecord muss in das bestehende Store-/Envelope- und
  Ressourcenbudget passen; bei unzureichendem Budget bleibt die Freigabe
  gesperrt;
- Sensor-, Temperatur-, Aktor- und Serviceparameter bleiben in ihren
  jeweiligen Owner-/Commissioning-Gates;
- ein unerwarteter Vertragswiderspruch waehrend der Umsetzung stoppt den
  betroffenen Slice und erfordert eine neue Planrevision statt einer
  stillen Annahme.

## 20. Planabschluss und naechstes Gate

Dieser Auftrag endet nach:

1. Commit der eigenstaendigen Planrevision 2 und der notwendigen
   Roadmap-Synchronisierung;
2. git diff --check ohne Befund;
3. Push des Dokumentationscommits auf den bestehenden Branch;
4. Eintrag von Planpfad und exakter Plan-Commit-SHA in Draft-PR #107;
5. Aktualisierung des PR-Texts mit IMPLEMENTATION: NOT_STARTED und
   Tests/Builds: NOT_RUN;
6. genau einem aktuellen SESSION HANDOVER mit HEAD, Plan-SHA, erledigten
   Bereichen, Tests, offenen Befunden und naechstem Schritt.

Danach bleibt PR #107 Draft. Der Agent setzt ihn nicht auf Ready for review,
merged nicht, aktiviert kein Auto-Merge, schliesst Issue #24 nicht, bearbeitet
#106 nicht und loescht den Branch nicht. Die naechste Aktion ist ausschliesslich
die Ownerfreigabe des exakten Plan-Commits.
