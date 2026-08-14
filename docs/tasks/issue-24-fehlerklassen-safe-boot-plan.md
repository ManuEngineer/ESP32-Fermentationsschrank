# Issue #24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion

## Planrevision 1

| Feld | Verbindlicher Stand |
|---|---|
| Issue | #24, [E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion |
| Planrevision | 1 |
| Planstatus | Zur Ownerfreigabe des exakten Plan-Commits |
| Branch | agent/issue-24-fehlerklassen-safe-boot-plan |
| Draft-PR | #107 |
| Ausgangs-HEAD vor dieser Planrevision | 447e6788d2ed5de27e615d77ee9bc86aaa12a93e |
| aktueller main-Basisstand | b8eae5f4da5f2666b5a9bda333d115254c4db5b2 |
| freigegebener Plan-Commit | nach Commit einzutragen; bis dahin NONE |
| Implementation | NOT_STARTED |
| Tests und Builds dieser Planungsphase | NOT_RUN |

Diese Revision ist eigenstaendig. Sie beschreibt den vollstaendigen #24-Scope,
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
Plattform-Resetursache und kontrollierten Neustart. Dafuer darf #24 genau
einen schmalen, anwendungsneutralen Port im device_platform einfuehren,
beispielsweise einen IResetService mit:

- einem geschlossenen, generischen Resetursachen-Enum;
- einer lesbaren, einmaligen Boot-Resetursache;
- einer kontrollierten Neustartanforderung mit explizitem Grund;
- einem Ergebnis, das einen nicht ausgefuehrten, ausgefuehrten oder
  unbestimmten Resetversuch unterscheidet.

Der konkrete ESP-IDF-Adapter gehoert ausschliesslich in
device_platform_esp_idf. Testseitig gehoert die kontrollierbare Quelle in
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

Die folgenden Codes sind Teil von Planrevision 1. Ihre IDs werden vor der
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
| 2 | O2-002 | 2 | Produktfuehler voruebergehend unbrauchbar bei explizit erlaubtem Ersatzbetrieb | kein persistenter Latch; qualifizierter Ersatzbetrieb |
| 3 | O2-003 | 2 | thermische Reaktion ist noch nicht bestaetigt | kein persistenter Latch; bounded reevaluation |
| 4 | O2-004 | 2 | notwendiger Sicherheitsfuehler voruebergehend stale, solange eine sichere Neubewertung noch moeglich ist | Peltier aus; kein Auto-Rearm ohne Codepolitik |
| 1 | S3-001 | 3 | Luftfuehler fuer Sicherheitsfreigabe fehlgeschlagen oder unaufloesbar | persistent; bewusster Reset |
| 2 | S3-002 | 3 | Kuehlkoerper-/Peltier-Schutzfuehler fehlgeschlagen oder unaufloesbar | persistent; bewusster Reset |
| 3 | S3-003 | 3 | Produktfuehler ohne zulaessigen sicheren Ersatz oder widerspruechliche Messlage | persistent; bewusster Reset |
| 4 | S3-004 | 3 | qualifizierte thermische Sicherheitsintervention | persistent; kein Auto-Rearm |
| 5 | S3-005 | 3 | qualifizierte harte thermische Notgrenze | persistent; kein Auto-Rearm |
| 6 | S3-006 | 3 | Innen- oder Aussenluefterfehler mit Sicherheitswirkung | persistent; bewusster Reset |
| 7 | S3-007 | 3 | unzulaessiger Aktor-/H-Bruecken-Ausgang oder sicherheitswidrige Rueckmeldung | persistent; bewusster Reset |
| 8 | S3-008 | 3 | #23 ActuatorWatchdogFaultEvidence / latched watchdog fault | persistent; Reset nur ueber #24 und #23-Resetpfad |
| 9 | S3-009 | 3 | qualifizierter Sicherheitszustand ohne ausreichende thermische Reaktion | persistent; bewusster Reset |
| 1 | Y4-001 | 4 | #56 ConfigurationRuntimeFailure mit erhaltener Producer-Ursache | persistent; Service-/Resetvertrag |
| 2 | Y4-002 | 4 | #56 ConfigurationCommitIndeterminate beziehungsweise nicht aufgeloestes CommitOutcomeUnknown | persistent; kein Runtime- oder Aktorfreigabepfad |
| 3 | Y4-003 | 4 | #57 ConfigurationUnavailable | persistent; SAFE_BOOT/Service |
| 4 | Y4-004 | 4 | #57 ConfigurationIntegrityFailure | persistent; SAFE_BOOT/Service |
| 5 | Y4-005 | 4 | kritischer Safety-/System-Persistenzschreibfehler | persistent in RAM und, soweit moeglich, minimal persistent |
| 6 | Y4-006 | 4 | unbekannte oder nicht sicher klassifizierbare Resetursache | persistent; SAFE_BOOT vor normaler Freigabe |
| 7 | Y4-007 | 4 | dritte abnormale Ursache in der offenen Restart-Episode | persistent; folgender Boot muss SAFE_BOOT waehlen |
| 8 | Y4-008 | 4 | Sicherheits-, Boot- oder Fehleraufgabe liefert keinen verlaesslichen Zustand | persistent; einmaliger kontrollierter Neustart nur gemaess Episodevertrag |
| 9 | Y4-009 | 4 | Lauf- oder Recoveryzustand nicht sicher rekonstruierbar | persistent; kein automatischer Laufstart |
| 10 | Y4-010 | 4 | eindeutig klassifizierter Brownout-/Versorgungsreset | Restart-Episode; Bootqualifikation vor jeder Freigabe |
| 11 | Y4-011 | 4 | interner Zustand, Schema oder Faultdatensatz ist unbestimmt | persistent; SAFE_BOOT |

Die Codes bilden keine Temperatur-, GPIO-, Zeit- oder
Commissioninggrenzwerte ab. Qualifizierte Sensor-, Limit- und
Aktorentscheidungen kommen aus den bestehenden Fachvertraegen. Ein
unbekannter oder nicht sicher validierter Quellstatus wird stattdessen zu
Y4-006, Y4-008 oder Y4-011 nach der jeweils bekannten Ursache; bei
unbekannter Ursache gilt Y4-011.

### 5.3 Faultdatensatz und Lebenszyklus

Jeder Faultdatensatz besitzt mindestens:

- stabilen Code und stabile Klasse;
- primaere Komponente beziehungsweise Quelle;
- monotone Entstehungssequenz und, wenn innerhalb des Boots vorhanden,
  monotone Zeit;
- aktiv, quittiert, beseitigt und verriegelt;
- unmittelbare Safety-Disposition;
- erlaubte Wiederfreigabe- und Resetpolitik;
- benoetigte Berechtigung;
- optionale primaryFaultId-Referenz;
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
| Klasse 3 | Peltier sofort aus, beide H-Brueckenrichtungen deaktivieren, laufende Impulsakkumulatoren und Integratorwirkung sicher verwerfen oder blockieren, erforderliche Luefterreaktion, persistenter Safety-Latch |
| Klasse 4 | sicherer Stillstand, Peltier und H-Bruecke aus, keine automatische Prozessfortsetzung, persistenter System-Latch, SAFE_BOOT oder technischer Diagnosepfad |
| Unresolved, unknown, fehlende Evidenz oder ungueltiger Gatezustand | ImmediateStop beziehungsweise strukturell nicht freigegeben; niemals Allowed |

Die Abschaltung ueberstimmt komfort- oder regelungsseitige Mindestzeiten,
Totzeiten und Richtungswechsel. Die Luefterreaktion bleibt auf die bestehende
abstrakte Strategie beschraenkt. Kein #24-Code schreibt GPIOs oder umgeht
den realen Aktoradapter.

Die bestehende criticalSafetyEventPending-Information wird als kompatible
Projektion des kanonischen Fault-/Safety-Zustands behandelt. Sie darf keine
zweite Quelle fuer Latch, Dominanz oder Reset werden.

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

ResetFault bleibt der bestehende Command. #24 erzeugt dafuer die
fachlich aktuelle FaultResetEvaluation und nutzt die vorhandenen
Revisions- und Bestaetigungspruefungen. Eine Freigabe ist nur moeglich, wenn:

- der konkrete Faultcode resetberechtigt ist;
- die Ursache beseitigt oder durch den Code ausreichend geklaert ist;
- alle erforderlichen Safety- und Integritaetspruefungen bestanden sind;
- keine gleich- oder hoeherklassige aktive Ursache besteht;
- die betroffene Run-/Recoveryinformation rekonstruierbar ist;
- die erforderliche Berechtigung, insbesondere Service-PIN bei technischen
  oder sicherheitskritischen Codes, vorliegt;
- die neue Latch-/Faultrevision Write-before-Apply persistiert und verifiziert
  wurde.

Ein fehlgeschlagener, unbestimmter oder verfruehter Reset bleibt abgelehnt
und erzeugt keine teilweise freigegebene Runtime. ResetEligibleNoRuntime
aus #57 ist nur ein Konfigurationsstatus und hebt keinen #24-Latch.

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
- persistente aktive Klasse-3-/Klasse-4-Latches mit Code, Quelle und
  Primar-/Folgebeziehung;
- Faultrevision und sichere Reset-/Recovery-Markierung;
- safeBootRequired und den dominanten Grund;
- offene Restart-Episode, abnormaler Zaehler und Episodenkennung;
- ausstehende, firmwarekontrollierte Restart-Evidenz mit Grund;
- letzten sicher klassifizierten Resetgrund und dessen
  Klassifikationsmetadaten.

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

NotFound darf nur als nachgewiesener fabrikneuer Safety-State behandelt
werden. ReadError, CapacityError, ungueltige Bytes, unbekannte Revision,
semantischer Widerspruch und unaufgeloester Commitzustand sind nicht
fabrikneu; sie fuehren fail-closed zu Y4-005 oder Y4-011 und zu
SAFE_BOOT.

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
| eindeutig klassifizierter Brownout-/Versorgungsreset | ja | Y4-010, Evidenz vor normaler Freigabe nachfuehren |
| autorisierter Service-, Firmwareupdate- oder Recovery-Neustart, dessen eigener Vertrag ihn als normal klassifiziert | nein | keine neue Episode; offene Episode bleibt trotzdem offen |
| unbekannter, widerspruechlicher oder nicht sicher klassifizierbarer Grund | nicht als harmlos behandeln | Y4-006/Y4-011, persistenter fail-closed Zustand und SAFE_BOOT vor normaler Freigabe |

Ein normaler, autorisierter Reset darf eine offene Episode nicht abschliessen.
Weitere Resetursachen duerfen nur nach expliziter #24-Klassifikation in die
Matrix aufgenommen werden. Ein nicht aufloesbarer Plattformwert ist kein
normaler Reset.

### 10.2 Episodealgorithmus

Die fachliche Wahrheit ist eine persistente Episode, nicht ein
Wall-Clock-Fenster:

1. Ein sicher als abnormal klassifizierter Neustart erzeugt oder erhoeht die
   offene Episode.
2. Der Zaehler und die Ursache werden persistent geschrieben. Ein Zaehler von
   drei setzt safeBootRequired fuer den folgenden Boot.
3. Ein firmwarekontrollierter Reset wird nur nach erfolgreichem
   Write-before-Restart ausgeloest.
4. Bei Watchdog oder Brownout ohne Vorab-Write wird die Episode im Bootpfad
   anhand des Plattformgrunds erhoeht und vor normaler Freigabe persistiert.
5. Ist die Episode offen und der Zaehler kleiner als drei, darf ein normaler
   Boot nur nach vollstaendiger Safety-/Konfigurationsqualifikation in
   STANDBY beziehungsweise den bestehenden normalen Prozesspfad gehen.
6. Dieser Boot startet eine fluechtige monotone Stabilitaetsmessung von
   30 Minuten. SAFE_BOOT, Fault, unaufgeloeste Konfiguration, aktive
   Klasse-3-/Klasse-4-Ursache, fehlerhafte Safety-Aufgabe oder fehlende
   Persistenzqualifikation sind kein stabiler Betrieb.
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

Die 30 Minuten sind in Release 1 eine private firmwarefeste Konstante des
Safetykerns, nicht Teil der Servicekonfiguration oder des
Konfigurationsschemas. Die Implementierung darf dafuer keinen NTP-Sync und
keine externe RTC voraussetzen.

### 10.3 Write-before-Restart

Der kontrollierte Neustart besitzt einen einzigen zentralen Ablauf:

1. Safety-Disposition auf sicheren Ausgang setzen;
2. Restart-Episode und Grund in den neuen SafetyStateRecord aufnehmen;
3. den Record schreiben;
4. bei CommitOutcomeUnknown gemaess bestehendem Persistenzvertrag
   ruecklesen und nur bei eindeutiger neuer Revision fortsetzen;
5. bei eindeutigem Nachweis genau einen kontrollierten Reset anfordern;
6. bei Schreibfehler oder unaufloesbarem Commit keinen automatischen
   Rebootversuch wiederholen, sondern im sicheren Zustand mit Y4-005
   verbleiben.

Ein echter Watchdog oder Brownout kann diese Reihenfolge nicht ausfuehren.
Der naechste Boot muss deshalb Resetgrund und persistente Vorrevision
kombinieren, die Evidenz nachtragen und vor normaler Freigabe eindeutig
committen. Ein fehlender oder widerspruechlicher Nachweis fuehrt zu
SAFE_BOOT, nicht zu einer geratenen normalen Freigabe.

### 10.4 Bootprioritaet und SAFE_BOOT

Der bestehende Prozesszustandsautomat wird in dieser Reihenfolge erweitert:

1. Ausgaenge sicher deaktivieren; kein alter GPIO- oder Direktzustand wird
   wiederhergestellt.
2. Resetursache einmalig erfassen und kontrollierte Resetabsicht aus dem
   Safety-Record lesen.
3. Safety-Record, Restart-Episode und persistente Latches pruefen.
4. Konfiguration, Run-Recovery, Firmware-/Schema-Integritaet, Journal und
   benoetigte Sensor-/Ressourcenqualifikation pruefen.
5. Restart-Evidenz beziehungsweise Episodenschliessung gemaess
   Write-before-Apply verifizieren.
6. bei einem dominanten Latch, drittem abnormalem Restart,
   unaufgeloester Konfiguration oder Unknown-Zustand in SafeBoot
   uebergehen.
7. nur bei vollstaendigem Gate in die bestehende normale
   Boot-/RecoveryEvaluation-/STANDBY-Kette uebergehen.

In SAFE_BOOT:

- bleiben Peltier und H-Bruecke aus;
- wird kein Lauf automatisch fortgesetzt;
- bleiben direkte Aktor- und Leistungstests gesperrt;
- sind passive Diagnose, Export und geschuetzte Recovery nur soweit der
  Systemzustand stabil genug ist erlaubt;
- wird der dominante Grund lokal und strukturiert sichtbar;
- verlaesst ein normaler Neustart SAFE_BOOT nicht;
- wird die Freigabe nur ueber den bestehenden #24-Fehlerresetvertrag und
  eine verifizierte, persistierte neue Safetyrevision moeglich.

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
   Acknowledge, Mute, FaultResetEvaluation und
   criticalSafetyEventPending als Projektion.
3. **Persistenter SafetyStateRecord:** Recordcodec, bestehender
   IStateStore, Latchsetzen, Readback, Write-before-Apply und
   Recoveryreset.
4. **Resetport und Restart-Episode:** genau ein schmaler
   device_platform-Resetport, Testsupport, ESP-IDF-Adapter, Resetmatrix,
   persistenter Zaehler, 30-Minuten-Stabilitaetsfenster und
   Write-before-Restart.
5. **Boot- und SAFE_BOOT-Integration:** bestehende ProcessState-/Event-
   Maschine, Bootprioritaet, kein normaler Neustartausstieg, aktorfreie
   Recovery.
6. **#23-Safety-Gate:** ActuatorWatchdogFaultEvidence, vorhandener
   Watchdog-Latchreset, zentrale Safety-Disposition und Nachweis gegen
   Allowed/Unresolved.
7. **#56/#57-Gate:** reale Producerstatus, Ursachenmetadaten,
   ConfigurationCommitIndeterminate und End-to-End-Recovery.
8. **Injektions- und Akzeptanzmatrix:** deterministische Fixtures,
   Restart-/Persistenz-Cut-Points, Konsumenten- und Integrationsorakel.
9. **Dokumentationsabschluss:** Safety-, Recovery-, Diagnose-,
   Akzeptanz- und Roadmapstatus auf den finalen Implementierungsstand
   synchronisieren.

Jeder Commit muss seinen Scope, gezielte Tests und den Status offen ausweisen.
Ein Fehler in einem frueheren Slice wird nicht durch spaetere Tests
ueberschrieben. Ein vollstaendiger lokaler Lauf bleibt bis zum separaten
Ownerauftrag nach Review ausgeschlossen.

## 16. Modul- und Architekturgrenzen

### device_platform

- anwendungsneutraler Resetursachen-/kontrollierter-Neustart-Port, nur wenn
  die Repository-Pruefung vor Umsetzung keinen vorhandenen gleichwertigen
  Port findet;
- bestehende Zeit- und Persistenzports unveraendert wiederverwenden;
- keine Fermentationsbegriffe, Faultcodes oder SAFE_BOOT-Policy.

### device_platform_esp_idf

- sichere Auswertung der real verfuegbaren ESP-IDF-Resetursachen;
- kontrollierter Resetadapter;
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
- #24 ist aktuelle fachliche Arbeit mit Planrevision 1 und
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

1. Commit der eigenstaendigen Planrevision 1 und der notwendigen
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
