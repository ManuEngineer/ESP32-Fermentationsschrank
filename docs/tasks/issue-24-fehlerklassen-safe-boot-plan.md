# Issue #24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion

## Planrevision 3 – PLAN_RESET / OWNER_DECISION_REQUIRED

| Feld | Verbindlicher Stand |
|---|---|
| Issue | #24 – [E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion |
| Draft-PR | #107 |
| Branch | `agent/issue-24-fehlerklassen-safe-boot-plan` |
| Kontext-HEAD dieser Planpruefung | `f1220c34b65b3add8aaba6c33023b0b648fe88c9` |
| Base/main | `b8eae5f4da5f2666b5a9bda333d115254c4db5b2` |
| historischer R2-Plan | `3b2befaf7595066cb8fcc0521b32e93212360ba5` |
| aktueller Planstatus | `OWNER_DECISION_REQUIRED`; R2 ist als Implementierungsgrundlage suspendiert |
| exakte SHA dieser Revision | nach dem Plan-/Dokumentationscommit einzutragen; vor Ownerfreigabe verbindlich im PR-Body |
| Produktionscode und produktive Tests | `FROZEN`; in diesem Auftrag nicht geaendert |

Diese Revision ist eigenstaendig. Sie enthaelt Ziel, Abgrenzung, Quellenrouting,
R2-Reconciliation, wiederzuverwendende Vertraege, Zielarchitektur, Testmatrix,
Umsetzungsschnitte und Owner-Gates vollstaendig. Sie setzt keine fruehere
Planrevision als technische oder normative Quelle voraus. Die historische R2-
Fassung wird nur fuer die geforderte Reconciliation bewertet.

Der Plan wird in dieser Fassung nicht zur Implementierung freigegeben. Die
Quellenpruefung hat materielle Ownerentscheidungen offen gelegt. Daher werden
in diesem Plan-/Dokumentationscommit keine Produktionslogik, produktiven Tests,
Adapter, ADRs, Hardwareannahmen oder erfundene Commissioningwerte eingefuehrt.

## 1. Kontextbaseline und verbindliches Stop-Gate

Die Erstpruefung wurde auf folgender Baseline ausgefuehrt:

```text
CONTEXT_BASELINE_BRANCH: agent/issue-24-fehlerklassen-safe-boot-plan
CONTEXT_BASELINE_SHA: f1220c34b65b3add8aaba6c33023b0b648fe88c9
CONTEXT_HEAD_SHA: f1220c34b65b3add8aaba6c33023b0b648fe88c9
CONTEXT_PLAN_SHA: 3b2befaf7595066cb8fcc0521b32e93212360ba5
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: Live-Issue/PR/Kommentare, main, kompletter PR-Diff,
                Root-/lokale AGENTS.md, Workflow, Quellenhierarchie,
                Safety-/Recovery-/Persistenzvertraege und R2-Reconciliation
SOURCE_OF_TRUTH_CONFLICT: mehrere R2-Regeln wurden im PR-Diff als kanonische
                           Dokumentabschnitte vorgezogen, waehrend main die
                           entsprechenden Entscheidungen offen laesst
```

Live gepruefter Stand:

- Issue #24 ist offen, Status `PLANNED_SPEC_PENDING`, und verlangt den realen
  `CONFIGURATION_SAFETY_INTEGRATION_GATE` gegen #56/#57 vor Abschluss.
- PR #107 ist offen und Draft. Der aktuelle Branch-HEAD ist der oben genannte
  Dokumentationsstand; es gibt keine Ownerfreigabe fuer eine Implementierung
  gegen die R2-Basis.
- `src/main.cpp` und `main/app_main.cpp` sind Composition Roots, binden aktuell
  aber nur `DevicePlatform` und `FermentationApplication`. Eine im Issue-Test
  instanziierte `ConfigurationSafetyIntegrationGate` ist deshalb kein
  produktiver Root-Nachweis.
- Die bestehenden #56/#57-Dienste und ihre typisierten Ergebnisse existieren
  auf `main`; die #56/#57-Plaene schliessen produktive ESP-IDF-NVS- und
  Composition-Root-Verkabelung ausdruecklich aus.
- #29/#32/#33, #35, #106, #19 und E4 werden nicht vorgezogen. #29 ist die
  spaetere konkrete ESP-IDF-/Hardwaregrenze, nicht automatisch eine pauschale
  Implementierungsabhaengigkeit fuer den abstrakten #24-Kern.

Stopregel:

1. R2 ist bis zur Ownerentscheidung suspendiert.
2. Der bestehende Implementierungscode bleibt unveraendert eingefroren; seine
   Tests und Dokumente sind Bestandsaufnahme beziehungsweise historische
   Ausfuehrungsnachweise, nicht Spezifikation.
3. Die in Abschnitt 6 aufgefuehrten Ownerentscheidungen werden nicht durch
   Code, Testfixtures oder Planwiederholung beantwortet.
4. Erst eine vollstaendige, nach diesen Entscheidungen synchronisierte
   Fassung mit exakter SHA darf zur Implementierung freigegeben werden.

## 2. Ziel und Nicht-Ziele

### 2.1 Ziel

Issue #24 liefert einen einzigen, zentralen und fail-closed Safety-/Faultpfad
fuer die bestehende Anwendung. Er muss:

- vier fachliche Fehlerklassen und stabile maschinenlesbare Fehlercodes
  abbilden;
- unmittelbare sichere Reaktionen vor Komfort-, Prozess- und alten
  Aktoranforderungen durchsetzen;
- Quittierung, Stummschaltung, automatische Wiederfreigabe und bewussten
  Fehlerreset getrennt halten;
- persistente Sicherheits- und Systemverriegelungen ueber Neustarts erhalten;
- Watchdog-, Brownout- und kontrollierte Neustartevidenz begrenzt und
  nachvollziehbar verarbeiten;
- wiederholte abnormale Neustarts gemaess Ownerentscheidung in `SAFE_BOOT`
  ueberfuehren;
- Primaer-/Folgefehler ohne Unterdrueckung der eigenen Safety-Reaktion
  nachvollziehbar verknuepfen;
- die vorhandenen #15-, #21-, #22-, #23-, #56- und #57-Vertraege konsumieren;
- reale #56/#57-Producer am bestehenden Application-/Compositionpfad in den
  zentralen Safetykern fuehren;
- den vorhandenen #23-Aktor-Gate-/Planner-/Sinkpfad ohne Bypass speisen;
- reproduzierbare native Fehlerinjektionen fuer Sensoren, Aktoren,
  Persistenz, Brownout und Neustart ermoeglichen;
- strukturierte Ereignisse ueber das bestehende `IEventJournal` projizieren;
- ohne Netzwerk, UI, NTP oder Hardwareannahmen sicher reagieren.

### 2.2 Nicht-Ziele

Nicht Bestandteil dieses Plans sind:

- GPIO-, Pegel-, BTS7960-, ESP-IDF- oder reale Resetursachenimplementierung;
- NVS-, Flash-, Partitionierungs- oder Hardwareabnahme;
- Commissioningwerte, Recovery-Leistung, Recovery-Dauer, Trendgrenzen,
  Temperaturgrenzen oder Service-PIN-Werte;
- ein zweiter Faultkern, ein zweiter Persistenzspeicher, ein zweiter
  Journalpfad oder eine zweite Boot-/Recovery-State-Machine;
- eine allgemeine C++-Capability-, Token- oder Pointer-Sicherheitsarchitektur;
- #19-Aufbewahrung, Bereinigung, Backup, Import oder ein neues Journalformat;
- #35-Commissioning, #106-Per-Run-Bindung, E4-UI/Web oder E5-Hardware;
- ein automatischer Neustart nach jedem Faultreset;
- eine unbounded Fault-Historie oder eine unbegruendete feste Latchkapazitaet.

## 3. Live-Issue-Scope und Akzeptanzkriterien

Der Live-Issue-Scope ist die verbindliche Arbeitsgrenze. Die technische
Kontraktion erfolgt aus den Fachquellen, nicht aus dem aktuellen PR-Code.

Vollstaendig abzudecken sind:

1. vier Fehlerklassen und stabile Fehlercodes;
2. unmittelbare sichere Reaktionen;
3. getrennte Quittierung und Fehlerreset;
4. automatische Wiederfreigabe nur fuer explizit erlaubte Betriebsfehler;
5. persistente Sicherheits- und Systemverriegelungen;
6. Watchdog, begrenzter kontrollierter Neustart und `SAFE_BOOT`;
7. Primaer-/Folgefehlerbeziehung;
8. reproduzierbare Software-Fehlerinjektionen fuer Sensoren, Aktoren,
   Persistenz, Brownout und Neustart;
9. Dominanz der hoechsten aktiven Klasse;
10. Erhalt notwendiger Verriegelung ueber Neustart;
11. wiederholte abnormale Neustarts fuehren zu `SAFE_BOOT`;
12. Unknown/Unresolved erzeugt keine normale Aktorfreigabe;
13. reales `CONFIGURATION_SAFETY_INTEGRATION_GATE` fuer #56/#57;
14. Recovery hebt eine Verriegelung nur nach dem tatsaechlichen #24-
    Fehlerresetvertrag auf;
15. kein Aktorpfad umgeht das Safety-Gate;
16. Journalereignisse und Dokumentation gemaess DoD.

Die offene Definition von Restart-Zeitfenster, Recoveryparametern, konkreten
Codes und Implementierungsbudget wird nicht durch diese Aufzaehlung geschlossen.

## 4. Quellenhierarchie und Reconciliation-Grundlage

Die Prioritaet lautet gemaess `docs/SPECIFICATION_REVIEW.md`:

1. akzeptierte ADRs in `docs/DECISIONS.md`;
2. `docs/SPECIFICATION_REVIEW.md` fuer Release-Scope und TBD-Kategorien;
3. thematisch zustaendige Fachvertraege;
4. `REQUIREMENTS.md`, `ARCHITECTURE.md`, `HARDWARE.md`;
5. Beispiele;
6. historische Plaene, Audits und Revisionsdokumente.

Verbindliche Quellen fuer die spaetere Umsetzung und ihre Rolle:

| Quelle | Rolle |
|---|---|
| Live-Issue #24 | Scope, DoD und reales #56/#57-Integrationsgate |
| `docs/SAFETY_AND_FAULTS.md` | Klassen, unmittelbare Reaktion, Quittierung, Reset, Auto-Rearm, Lebenszyklus, Dominanz, Primaer/Folge |
| `docs/SAFETY_COMPONENT_FAULTS.md` | Sensor-, Aktor-, Luefter- und S3-004-Recoverygrenzen; Commissioning-TBD |
| `docs/SYSTEM_SAFETY_AND_RECOVERY.md` | Boot, Persistenzfehler, SAFE_BOOT und offene Restartparameter |
| `docs/ACCEPTANCE_TESTS.md` | verpflichtende Orakel und Persistenzfehlervertrag |
| `docs/DECISIONS.md` ADR-013/014/018 | Modularchitektur, deterministischer Zustandsautomat, Konfigurationspersistenz und #24-Completion-Gate |
| `docs/CONFIGURATION_PERSISTENCE.md` | #56/#57-Resultate, `IStateStore`, Root-/Runtimegrenzen, Ressourcenbudget |
| `docs/IMPLEMENTATION_ISSUES.md` | E3-Reihenfolge und #56/#57-Gate ohne zyklische Abhaengigkeit |
| `docs/RUN_COMMANDS.md` | bestehender #15-Command-/Messagepfad und Evaluationstypen |
| `docs/RUNTIME_BEHAVIOR.md` | fachliche Laufzeitabgrenzung |
| `docs/STATE_MACHINE.md` | kanonische Boot-, SAFE_BOOT-, FAULT- und Recoverytopologie |
| `docs/DIAGNOSTICS_AND_MAINTENANCE.md` | SAFE_BOOT-Diagnosegrenzen und Service-/Journalgrenze |
| `docs/RUN_PERSISTENCE.md` | Laufpersistenz, Recovery und Write-before-Apply ausserhalb des Faultrecords |
| `docs/RECOVERY_AND_INTERRUPTION.md` | phasenbezogener Wiederanlauf, keine erfundene Zeit, #21-Fallback |
| #15/#17/#18/#20/#21/#23/#56/#57-Plaene | bereits akzeptierte Vertraege und Abgrenzungen |

Die R2-Datei ist fuer die nachstehende Matrix eine historische Eingabe. Sie
hat keine hoehere Prioritaet als `main`-Quellen, ADRs oder das Live-Issue.

## 5. R2-Reconciliation-Matrix

Die Matrix deckt jede materielle R2-Regel ab. `Basiscode` bezeichnet den auf
`main` bestehenden Vertrag; PR-Code wird nur als eingefrorene Bestandsaufnahme
genannt. `CANONICALIZE_OWNER_DECISION` bedeutet, dass vor Implementierung die
zuständige Fachquelle und gegebenenfalls ein akzeptierter ADR synchronisiert
werden muessen.

| R2-Regel | Live-Issue-Bezug | Kanonische Quelle | Basiscode auf `main` | Bewertung | Begruendung | Ownerentscheidung |
|---|---|---|---|---|---|---|
| Vier Klassen mit `P1/O2/S3/Y4`-Namensraum | vier Klassen, stabile Codes | `SAFETY_AND_FAULTS.md`, Phase-8B/8C-Open-Points | vier fachliche Klassen, konkrete Codes noch offen | `KEEP` fuer Klassen; `CANONICALIZE_OWNER_DECISION` fuer IDs/Namensraum | Klassen sind akzeptiert, konkrete stabile IDs wurden in R2 neu eingefuehrt | ja, fuer Codes |
| R2-Codeliste bis `Y4-011` und Unknown-Fallback | stabile Codes, Unknown fail-closed | `SAFETY_AND_FAULTS.md` offen fuer konkrete Codes; Komponentenquelle | keine zentrale #24-Codeliste auf `main` | `CANONICALIZE_OWNER_DECISION` | Test-/Codeenum darf keine normative Quelle ersetzen | ja |
| Exakte Kapazitaet `8` / 2048-Byte-Record | bounded/sichere Fehlerbehandlung | `SPECIFICATION_REVIEW.md` `TBD_IMPLEMENTATION_BUDGET`; `ACCEPTANCE_TESTS.md` bounded | generische bounded Stores, kein #24-Limit | `REMOVE` als Produktfakt; `KEEP` bounded | `8` und 2048 sind nicht aus Issue/Worst Case/Budget belegt | ja, falls konkrete Grenze benoetigt |
| `FaultInstanceId` aus persistenter High-Watermark | mehrere Fehler, Persistenz ueber Reboot | `SAFETY_AND_FAULTS.md` Datensatz/Traceability; `RUN_PERSISTENCE.md` Persistenzvorrang | kein #24-Faultsequenzvertrag auf `main` | `KEEP` als notwendiger Designpunkt, minimal begruenden | ID-Wiederverwendung waere fuer Primary/Follow-up und Diagnose unsicher | ja, nur Owner bestaetigt Persistenzfeld/Scope |
| Fault-Lifecycle inklusive Reaktivierung derselben Ursache | Primaer-/Folgefehler und Ursache | `SAFETY_AND_FAULTS.md` Zustandsmodell | vier Zustände, keine zentrale Korrelation | `SIMPLIFY_REUSE` | bestehende Zustaende behalten; Korrelation nur aus stabiler Produceridentitaet, keine neue Domaene | ja, falls keine Produceridentitaet existiert |
| Dominanz Klasse 4 > 3 > 2 > 1 und Codeprioritaet | hoechste aktive Klasse bestimmt Ausgang | `SAFETY_AND_FAULTS.md` | Klassenprioritaet vorhanden, Detailprioritaet offen | `KEEP` Klassen; `CANONICALIZE_OWNER_DECISION` Detailprioritaet | fachliche Dominanz ist kanonisch, konkrete Rangliste nicht | ja fuer Detailrangliste |
| `FaultResetRequest` mit Ziel/Revision und private `FaultResetAuthorization` | Reset nach Ursache/Checks/Berechtigung | `SAFETY_AND_FAULTS.md`, `RUN_COMMANDS.md`, ADR-014 | #15 besitzt `FaultResetRequest` und `FaultResetEvaluation` als Requestfeld | `SIMPLIFY_REUSE` und `CANONICALIZE_OWNER_DECISION` | Evaluation soll Safety-Core-Ergebnis sein; private Token-/Pointer-Authority ist nicht automatisch noetig | ja fuer API-Migration und Berechtigungsproducer |
| Positive Caller-Evaluation/Booleans werden ignoriert | keine unerlaubte Freigabe | `SAFETY_AND_FAULTS.md` | #15 konsumiert Evaluation im Command-Entscheid | `KEEP` Trustziel, `SIMPLIFY_REUSE` Umsetzung | Trust-Grenze muss zentral sein, aber keine zweite Capability-Domaene | nein fuer Safetyziel; ja fuer konkrete API |
| Intern erzeugte `SAFETY_RECOVERY`-Capability mit #35-Revision | S3-004/Recovery ohne Gate-Bypass | `SAFETY_COMPONENT_FAULTS.md`, #35-TBD | S3-004 begrenzte Gegenrichtung; Parameter offen | `DEFER` produktive Ausfuehrung; `KEEP` Gatecontract | R2 fakte eine Qualifikation mit Testwerten und zog #35 vor | ja: Contract-only oder aktiv erst nach #35 |
| Klasse-2 wird bei Ursachefreiheit pauschal geloescht | Auto-Rearm nur explizit erlaubt | `SAFETY_AND_FAULTS.md`, `RECOVERY_AND_INTERRUPTION.md`, #21 | #21 besitzt konkrete Sensorfallback-/Rueckkehrsemantik | `REMOVE` pauschales Auto-Rearm; `SIMPLIFY_REUSE` #21 | Policy muss pro Producer/Code konsumiert werden | nein fuer Verbot; ja fuer fehlende Codepolicy |
| Persistenter `SafetyStateRecord` mit nur S3/Y4-Latches | persistente Sicherheits-/Systemverriegelung | `SAFETY_AND_FAULTS.md`, `SYSTEM_SAFETY_AND_RECOVERY.md` | generischer `IStateStore`, keine #24-Recordsemantik | `KEEP` fachliche Trennung; `DEFER` genaue Schema-/Kapazitaetswahl | P1/O2 werden beim Boot neu bewertet, S3/Y4 bleiben sicher verriegelt | ja fuer Schema/Budget |
| Commitfehler rollt Faultcore im RAM zurueck | Persistenzfehler fail-closed | `SYSTEM_SAFETY_AND_RECOVERY.md:290-322`, `ACCEPTANCE_TESTS.md` | `IStateStore` unterscheidet WriteError/Unknown | `REMOVE` Rollback; `KEEP` RAM-Latch | kanonisch bleibt RAM-Latch bis Neustart wirksam; isolierter spaeter Write ist keine Entwarnung | nein |
| `FaultResetBootIntent` fuer jeden Faultreset | Reset ist bewusste codebezogene Aktion | `SAFETY_AND_FAULTS.md`, `STATE_MACHINE.md`, `SYSTEM_SAFETY_AND_RECOVERY.md` | kein generischer Faultreset-Reboot auf `main` | `REMOVE` pauschale Kopplung; `DEFER` begruendete Bootintents | nur konkrete schwere Software-/Systemfehler duerfen kontrollierten Neustart verlangen | ja fuer codebezogene Resetpolicy |
| Drei abnormale Neustarts in 30 Minuten stabiler Laufzeit | wiederholte abnormale Neustarts -> SAFE_BOOT | `SYSTEM_SAFETY_AND_RECOVERY.md:210-226` | Ausgangspunkt 3, Zeitfenster offen | `CANONICALIZE_OWNER_DECISION` | 30 Minuten stabile Laufzeit und Episode-Schluss sind neue Semantik | ja |
| Observation-ID exactly once und persistierte Evidence | reproduzierbare Restart-Injektion | `SYSTEM_SAFETY_AND_RECOVERY.md`, `ACCEPTANCE_TESTS.md` | kein Resetport/Observationvertrag auf `main` | `KEEP` Ziel; `DEFER` genaue Persistenz | doppelte Bootauswertung darf keine zweite Evidence erzeugen; Identitaet/Bootgrenze muss ownerkonform sein | ja fuer Persistenz-/Zeitsemantik |
| Kontrollierter Neustart vor Persistenz und #23-Evidence | Watchdog/kontrollierter Neustart | `SAFETY_COMPONENT_FAULTS.md`, #23-Plan | #23 liefert Watchdog-Evidence, kein Resetport | `SIMPLIFY_REUSE` | #24 konsumiert #23-Evidence; ein kontrollierter Restart bleibt auf begruendete interne Diagnose begrenzt | ja fuer Resetport/Producergrenze |
| SAFE_BOOT-Exit nur nach autorisiertem Folgeboot | SAFE_BOOT bleibt sicher, Reset loescht keine Sperre | `STATE_MACHINE.md`, `SYSTEM_SAFETY_AND_RECOVERY.md` | normaler Neustart verlaesst SAFE_BOOT nicht automatisch | `KEEP` Grundsatz; `DEFER` konkrete Exit-Policy | R2 machte daraus einen generellen Reset-Bootpfad, den Quellen nicht fordern | ja fuer Exitarten |
| 64-bit Watchdogdiagnose separat von 32-bit Correlation | reproduzierbare #23-Evidence | #23-Plan, `DIAGNOSTICS_AND_MAINTENANCE.md` | #23-Evidence ist vorhanden; R2 speichert Zusatzfeld | `KEEP` Evidenzintegritaet; `SIMPLIFY_REUSE` Correlation | keine stille Trunkierung; keine neue unbounded Struktur | nein fuer Evidenzziel |
| `ConfigurationSafetyIntegrationGate` als native Bridge | reale #56/#57-Producer | Issue #24, ADR-018, #56/#57-Plaene | #56/#57 realer `ConfigurationRecoveryService`; Root bindet ihn nicht | `KEEP` Mapper als schmale Application-Funktion; `REMOVE` Testfixture als DoD | Bridge allein ist kein systemweiter Gate-Nachweis | ja fuer konkrete Rootbesitz-/Abnahmeregel |
| #29/#90 als zwingende #24-Eingangsgates | reale Producerintegration, keine pauschale Abhaengigkeit | Issue #24, `IMPLEMENTATION_ISSUES.md`, #56/#57-Plaene | E3 bleibt abstrakt bis E5; #90 produktiver Adapter spaeter | `REMOVE` pauschale Abhaengigkeit | Hardwareadapter duerfen #24 nicht umgehen, sind aber nicht automatisch Voraussetzung fuer native Producerintegration | nein fuer Grenze; ja falls Root fehlt |
| vollstaendige strukturierte Eventliste | Journalereignisse/DoD | `SAFETY_AND_FAULTS.md`, `DIAGNOSTICS_AND_MAINTENANCE.md`, `IEventJournal` | `IEventJournal` ist schmal, #19 besitzt Aufbewahrung | `SIMPLIFY_REUSE` | fachliche Ereignisprojektion ja, keine neue Plattform und keine Eventliste als Produktdecision ohne Source | ja fuer finale Events |
| fuenf grosse R2-Testmatrizen | Issue-Tests und `ACCEPTANCE_TESTS.md` | Live-Issue, `ACCEPTANCE_TESTS.md` | bestehende #15/#21/#23/#56/#57-Tests | `SIMPLIFY_REUSE` | Matrix behalten, aber offene Werte parameterisieren und kanonischen Persistenzfehler korrigieren | nein fuer Testgrundsaetze |

## 6. Offene Ownerentscheidungsmatrix

Diese Fragen werden nicht selbst beantwortet. Die Empfehlungen sind keine
Entscheidung und werden erst nach Ownerantwort in der zustaendigen Quelle
festgeschrieben.

| ID | Frage | Quellenlage | Optionen | Technische Folge | Empfehlung |
|---|---|---|---|---|---|
| OD-24-01 | Welche Restart-Schwelle und welches Zeitfenster gelten fachlich? | Drei abnormale Neustarts sind Ausgangspunkt; exakter Wert/Fenster offen in `SYSTEM_SAFETY_AND_RECOVERY.md`. | A: 3 innerhalb gemessener Zeitspanne; B: 3 aufeinanderfolgende Episoden; C: andere explizite Grenze. | Bestimmt persistierten Zaehler, Zeitbasis, Episode-Schluss, Brownout-/Normalbootsemantik und SAFE_BOOT-Tests. | A nur mit sicherer Zeitbasis; keine 30-Minuten-Stabilitaet ohne ausdrueckliche Bestaetigung. |
| OD-24-02 | Darf stromlose Zeit eine Restart-Episode schliessen oder bleibt der Zaehler erhalten? | Quellen sagen Neustart loescht keine Verriegelung, definieren Episode-Schluss aber nicht. | erhalten; gemessene Zeitspanne; nur expliziter Service-Reset. | Persistenzfelder, lange Stromlosigkeit, Bootqualifikation. | Sicherheitsseitig erhalten, bis die Ownersemantik feststeht. |
| OD-24-03 | Welche stabilen Fehlercode-IDs und Detailprioritaeten werden kanonisch? | Klassen sind akzeptiert; konkrete Codes stehen in Phase 8B/8C offen. | vorhandene R2-Namensraeume bestaetigen; neue fachquellenbasierte IDs; schrittweise Producercodes. | `FaultCode`, Wire-/Diagnoseprojektion, Mapper, Tests und Dokumente. | Erst fachlich bestaetigen, dann `SAFETY_AND_FAULTS.md`/Komponentenquelle synchronisieren. |
| OD-24-04 | Welche bounded aktive Latchkapazitaet ist nach Worst Case und Budget zulaessig? | `TBD_IMPLEMENTATION_BUDGET`; R2-8 ist unbelegt. | begruendete Zahl; Record-/Ressourcenbudget-Gate; andere bounded Struktur. | Codec, Recordgroesse, Capacity-Fault, Verlust-/Overflowreaktion, Tests. | Keine Zahl bis Worst Case und Budget nachgewiesen sind. |
| OD-24-05 | Soll #24 S3-004 jetzt aktiv Recovery versuchen oder nur den Gate-/Vertragspfad vorbereiten? | S3-004 erlaubt begrenzte Gegenrichtung; Leistung/Dauer/Trend sind #35-TBD. | Contract-only fail-closed; aktiv erst nach #35-Revision; anderer Ownerpfad. | Planner-Gate, Parameterproducer, Testfixture und SAFE_BOOT-/Latchsemantik. | Contract-only bis vollstaendige #35-Commissioningrevision existiert. |
| OD-24-06 | Wie kommt die echte Resetberechtigung in den #15-Commandpfad? | `SAFETY_AND_FAULTS.md` verlangt je Code Berechtigung; aktueller Code besitzt keinen produktiven Service-PIN-Producer. | bestehende Evaluation als Safety-Core-Ergebnis; neuer schmaler Authorization-Result-Port; Reset nur bei vorhandener Berechtigung. | API-/Application-Grenze und negative Tests; kein `true`-Fallback. | Bestehenden #15-Pfad erhalten und nur positive Eingabe aus dem Caller entfernen, sofern Owner zustimmt. |
| OD-24-07 | Ist eine API-Migration von `FaultResetEvaluation` als Requestfeld erforderlich, und ist sie ADR-pflichtig? | #15 besitzt den Vertrag; R2 fuehrte private Token-/Pointer-Authority ein. | Evaluation als zentral berechnetes Ergebnis; Request nur Ziel/Revision; minimale Kompatibilitaetsmigration. | Betroffene #15-Konsumenten, Wire-/Testvertraege, Review-/ADR-Gate. | Keine Capabilityarchitektur; minimale Migration nur mit Ownerfreigabe. |
| OD-24-08 | Ist ein neuer `device_platform::IResetController`-Port als oeffentliche Architekturentscheidung gewollt? | #24 braucht Resetbeobachtung/kontrollierten Neustart; `device_platform` ist fuer schmale anwendungsneutrale Ports zustaendig; #29 liefert ESP-IDF-Mapping. | schmaler Port; bestehender anderer Port falls vorhanden; Resetbeobachtung ausserhalb #24. | Modul-/ADR-/Testgrenze und spaeterer #29-Adapter. | Schmalen nativen Port nur nach Notwendigkeitsnachweis und ADR-Pruefung vorsehen. |
| OD-24-09 | Wo wird der reale #56/#57-Producerpfad produktiv komponiert, solange die Roots nur Skeletons enthalten? | Issue/ADR-018 verlangen systemweite Integration; `ConfigurationSafetyIntegrationGate` ist derzeit nur in Tests verdrahtet. | #24 erweitert `src/main.cpp`/`app_main` mit abstrakter App-Komposition; benanntes spaeteres Application-Gate; andere Ownerzuordnung. | Entscheidet DoD-Erreichbarkeit, Root-Dateien und Abnahmeschnitt; keine #29/#90-Pauschalabhaengigkeit. | #24 soll die abstrakte Root-Grenze liefern, Hardwareadapter bleiben spaeter; Owner muss die Besitzgrenze bestaetigen. |
| OD-24-10 | Welche SAFE_BOOT-Exitarten gelten pro Faultklasse? | SAFE_BOOT verlangt Integritaet und je Ursache Service-PIN; R2 verlangte generellen FaultResetBootIntent. | nur codebezogener bewusster Reset; zusaetzlicher kontrollierter Neustart fuer ausgewiesene Y4-Faelle; Service/Wartung. | Bootintent, Resetpolicy, Tests und Dokumentation. | Kein genereller Reboot; pro Code/Klasse aus Fachquelle ableiten. |

Bis OD-24-01, OD-24-03, OD-24-04, OD-24-05, OD-24-06, OD-24-08 und
OD-24-09 beantwortet sind, lautet der Planstatus `OWNER_DECISION_REQUIRED`.
Die Antworten muessen in der passenden kanonischen Quelle beziehungsweise in
einem akzeptierten ADR festgehalten werden. Der Plan darf die Antworten nicht
durch weitere Textdetails vorwegnehmen.

## 7. Bestehende Vertraege und minimale Zielarchitektur

### 7.1 Wiederverwenden

- #15 `CommandEnvelope`, `FaultResetRequest`, `FaultResetEvaluation`,
  `RuntimeMessage`, Revisionen, Ack- und Mute-Semantik;
- #21 Sensorqualitaet, Fallback, `fallback_to_air_after_timeout`, Rueckkehr- und
  Evidenzrevisionen;
- #22 `ControlRequest` und Control-Kontext ohne Safetyentscheidung im PI-Kern;
- #23 `ActuatorSafetyGateInput`, `ActuatorSafetyGateStatus`,
  `ActuatorWatchdogFaultEvidence`, Planner und Sink-Driver;
- #56/#57 `ConfigurationRecoveryService`, `ConfigurationRecoveryResult`,
  `ConfigurationRuntimeFailure`, `ConfigurationCommitIndeterminate`,
  `ConfigurationUnavailable` und `ConfigurationIntegrityFailure`;
- `device_platform::IStateStore` mit getrennten Read-/Write-Status und
  `CommitOutcomeUnknown`;
- `device_platform::IEventJournal` als einziger Ereignisport;
- ADR-014s entscheidungs- und persistenzfreie Zustandsautomatenlogik sowie
  `RunPersistenceCoordinator` fuer Laufdaten.

### 7.2 Minimale Schichten

```text
#20/#21/#23/#56/#57 Producer
        -> eine #24 Application-Safety-Grenze
        -> zentraler Fault-/Latch-/Restartkern
        -> bestehende #15-Projektion und #23 ActuatorSafetyGateInput
        -> bestehender Planner/Sink
        -> abstrakte device_platform-Sinks
```

Der Faultkern besitzt keine UI-, Web-, ESP-IDF- oder GPIO-Abhaengigkeit. Die
Application-Grenze besitzt genau eine Safetyinstanz und fuehrt keine zweite
Boot-/Recovery-State-Machine. Ein eventueller Resetport bleibt schmal,
anwendungsneutral und fuer native Tests kontrollierbar; die ESP-IDF-
Implementierung bleibt #29/E5.

`ConfigurationSafetyIntegrationGate` darf als schmale Application-Funktion
oder Kompositionsobjekt bestehen, wenn es die echten #56/#57-Typen verarbeitet.
Eine nur aus Tests erreichbare Instanz erfuellt die DoD nicht.

## 8. Fehlerklassen, Codes und unmittelbare Reaktion

Die Klassen sind die vier kanonischen Klassen aus `SAFETY_AND_FAULTS.md`:

| Klasse | Fachliche Bedeutung | Grundreaktion |
|---|---|---|
| Prozesswarnung | keine unmittelbare Gefahr, sichere Regelung moeglich | melden, journalisieren, keine Safetyfreigabe durch Warnung erzwingen |
| behebbarer Betriebsfehler | Normalbetrieb voruebergehend nicht moeglich oder erlaubter Ersatzbetrieb | Peltier aus, erlaubten #21-Ersatzbetrieb verwenden, nur codebezogen auto-rearm |
| verriegelter Sicherheitsfehler | Sicherheit nicht eindeutig nachgewiesen | Peltier/H-Bruecke aus, Latch, sichere Luefterreaktion, bewusster Reset |
| schwerer Systemfehler | Software-, Speicher-, Zeit- oder Hardwarefunktion nicht verlaesslich | sicherer Stillstand, System-Latch soweit sicher speicherbar, keine Fortsetzung |

Jeder Code muss fachlich Klasse, Ursache, unmittelbare Reaktion, erlaubte
Wiederfreigabe, Berechtigung und gegebenenfalls Primary-/Follow-up-Bezug
tragen. Die konkreten IDs bleiben bis OD-24-03 offen. Unbekannte Codes,
unbekannte Klassen und unvollstaendige Producerdaten werden nicht in eine
harmlose Klasse umgedeutet, sondern erzeugen eine sichere Unknown-/Unresolved-
Wirkung nach der kanonischen Codeentscheidung.

## 9. Fehlerlebenszyklus und Primaer-/Folgefehler

Die kanonischen Zustaende bleiben:

```text
ACTIVE_UNACKNOWLEDGED
ACTIVE_ACKNOWLEDGED
CAUSE_CLEARED_LOCKED
CLEARED
```

Quittieren aendert nur Kenntnisnahme. Ursachenfreiheit aendert nicht
automatisch einen verriegelten Fehler. `CLEARED` entsteht erst nach dem fuer
Code/Klasse zulaessigen Reset beziehungsweise expliziten Auto-Rearm.

Alle gleichzeitig relevanten Ursachen bleiben bounded erfasst. Dominanz waehlt
den sicheren Ausgang und die sichtbare Hauptmeldung, loescht aber keine
Nebenursache. Primary-/Follow-up-Beziehungen sind diagnostische Beziehungen;
ein Follow-up behält seine eigene Safetyreaktion.

Reaktivierung derselben Ursache darf nur dieselbe Faultinstanz wieder aktiv
setzen, wenn der Producer eine stabile gleiche Ursachenidentitaet liefert. Ohne
solche Identitaet wird eine neue Instanz mit Primary-/Follow-up-Beziehung
angelegt. In beiden Faellen steigen die relevanten Revisionen; ein alter
Resetkontext wird stale. Eine neue Korrelationsdomaene wird nicht erfunden.

## 10. Persistente Latches und Persistenzfehlervertrag

### 10.1 Persistenzinhalt

Der Safetyrecord enthaelt nur den fuer Neustart und sichere Bootprioritaet
notwendigen Zustand, insbesondere persistente S3-/Y4-Latches, Revisionen,
dominante Projektion und die vom Owner beschlossene Restart-/Evidence-
Semantik. P1/O2-Hinweise werden beim Boot anhand aktueller Producerdaten neu
bewertet; sie werden nicht als S3/Y4-Latch kodiert.

Die Recordkapazitaet bleibt bounded. Eine konkrete Zahl wird erst nach
Worst-Case-, Datenmodell- und Ressourcenbudgetnachweis sowie OD-24-04 in der
kanonischen Quelle festgelegt. Cleared-Historie gehoert nicht in die aktive
Latchmenge. Journalisierung bleibt `IEventJournal`/#19.

### 10.2 Write-before-Apply und Fehlerwirkung

Bei jeder Mutation, die Aktoren oder Boot-/Recoveryfreigaben beeinflussen kann:

1. sichere Wirkung und neue Autoritaet werden vorbereitet;
2. die neue Safety-/Latchrevision wird ueber den bestehenden `IStateStore`
   atomar geschrieben;
3. Readback beziehungsweise der Storevertrag wird gemaess Ergebnis validiert;
4. erst danach wird die Anwendungsmutation uebernommen.

Bei kritischem `WriteError` oder `CapacityError` gilt der kanonische Vertrag:

- neue Aktoranforderungen bleiben gesperrt;
- Peltier und beide H-Brueckenrichtungen bleiben aus;
- der RAM-seitige Safety-/Persistenzfehler-Latch bleibt gesetzt;
- ein minimaler persistenter Latch wird nur gemaess Fachvertrag versucht;
- scheitert auch dieser, bleibt die Laufzeit bis Neustart fail-closed und der
  naechste unklare Boot geht in `SAFE_BOOT`;
- ein spaeter isoliert erfolgreicher Schreibversuch ist keine Entwarnung.

Bei `CommitOutcomeUnknown`, ReadbackError, ReadbackMismatch, Korruption,
NotFound ausserhalb eines vollstaendigen Factory-New-Proofs oder
CapacityError wird nie vom alten oder neuen Zustand geraten. Der Safetykern
bleibt gesperrt und meldet den unaufgeloesten Zustand. Ein RAM-Rollback, das
den Fehler-Latch entfernt, ist unzulaessig.

## 11. Quittierung, Fehlerreset und Wiederfreigabe

Quittierung und Mute bleiben reine #15-Projektionen. Ein Fehlerreset benoetigt
mindestens:

- genaues Faultziel und erwartete aktuelle Revision;
- nachgewiesene Ursachenfreiheit;
- die fuer den Code erforderlichen Sensor-, Aktor-, Integritaets- und
  Persistenzpruefungen;
- keine weitere gleich- oder hoeherklassige blockierende Ursache;
- die kanonisch vorhandene Berechtigung;
- die codebezogene Resetpolicy.

Die `FaultResetEvaluation` wird, sofern der Owner den bestehenden Vertrag
beibehaelt, ausschliesslich vom zentralen Safetykern berechnet. Kein UI-, Web-,
Test- oder normaler ControlRequest darf positive Felder als Safetyautoritaet
einspeisen. Die konkrete API-Migration bleibt OD-24-06/07 offen; eine private
Token-/Pointer-Capability wird nicht als Selbstzweck geplant.

Ein erfolgreicher Reset entfernt nur das exakt autorisierte Ziel. Er loescht
keine andere Ursache, keinen System-Latch und keine unbestaetigte
Persistenzsperre. Ein Neustart ist kein Fehlerreset.

Automatisches Re-Arm ist ausschliesslich dort zulaessig, wo die kanonische
Producerpolicy es ausdruecklich erlaubt. #21 liefert fuer Produktfuehler-
Fallback und Rueckkehr die bestehende Policy; #24 kopiert keine Zeit- oder
Wiederholungsgrenzen.

## 12. S3-004 und SAFETY_RECOVERY-Abgrenzung zu #35

Die kanonische Komponentenquelle erlaubt an der Sicherheits-Eingriffsgrenze
eine begrenzte Gegenrichtung, bei maximal zwei Versuchen und mit
`TBD_COMMISSIONING`-Leistungs-, Zeit-, Trend- und Temperaturwerten. An der
Notgrenze oder bei unklarer Evidenz bleibt alles gesperrt. Nach erfolgreicher
Rueckfuehrung bleibt der Sicherheitsfehler verriegelt.

In #24 bedeutet das:

- initiale Reaktion und Aktor-Gate sind sicher aus;
- ein Recovery-Gate darf nur vollstaendig qualifizierte, spaeter von #35
  gelieferte Parameter und aktuelle Sensor-/Luefter-/Aktor-Evidenz akzeptieren;
- Recovery darf den bestehenden #23-Planner nicht umgehen und loescht keinen
  Latch;
- ohne vollstaendige #35-Qualifikation bleibt das Gate `Unresolved`/fail-closed;
- Testwerte, positive Booleans oder erfundene Revisionsnummern sind keine
  produktive Qualifikation.

Ob #24 jetzt nur diesen deaktiviert-fail-closed Contract testet oder einen
aktiven Pfad nach #35-Verfuegbarkeit abnimmt, entscheidet OD-24-05.

## 13. Watchdog, kontrollierter Neustart und SAFE_BOOT

### 13.1 Resetport und Evidenz

Der Safetykern benoetigt fuer Resetursachen und einen begrenzten kontrollierten
Neustart einen schmalen, anwendungsneutralen Port. Der Port darf weder Fault-
noch Fermentationsbegriffe enthalten und keine Policy besitzen. Konkrete
ESP-IDF-Abbildung und `esp_restart()` bleiben #29/E5. Die Notwendigkeit und
ADR-Pflicht des Ports ist OD-24-08.

Der #23-Watchdog liefert seine vorhandene Evidence unverkuerzt. Ein eventueller
bounded CorrelationKey ist nur technische Korrelation; die vollstaendige
64-bit-Diagnosefolge bleibt getrennt. Keine unbounded Zusatzhistorie entsteht.

### 13.2 Restartsemantik

Der kanonische Ausgangspunkt sind drei abnormale Neustarts in einem noch nicht
festgelegten Zeitfenster. Der Plan legt weder 30 Minuten noch eine andere
Schwelle fest. Die Zeitbasis muss ohne Netzwerkzeit sicher und native
deterministisch simulierbar sein; bei unbekannter oder widerspruechlicher
Evidenz gilt fail-closed.

Ein kontrollierter Neustart ist nur fuer eine fachlich begruendete Software-/
Treiber- oder Safety-Reaktion zulaessig. Ein plausibler physischer Sensorfehler
wird nicht durch Neustartschleifen behandelt. Der Neustart selbst ist kein
Fehlerreset.

### 13.3 Exactly-once

Innerhalb eines Boots darf dieselbe stabile Resetbeobachtung nur einmal als
Episode, Evidence oder Resetaktion konsumiert werden. Eine wiederholte
`evaluateBoot()`-Auswertung derselben Beobachtung erzeugt keinen zweiten
Zaehler, keine zweite Evidence und keinen kuenstlichen Mismatch. Eine neue
Beobachtung wird normal bewertet.

Die persisted Evidence-, Episode- und Stromlosigkeitssemantik wird erst nach
OD-24-01/02 festgelegt. Kein RAM-only Trick darf eine vom Owner geforderte
Persistenz ersetzen.

### 13.4 SAFE_BOOT

`SAFE_BOOT` bleibt aktorfrei. Normaler Neustart, Quittierung, Mute oder das
Verschwinden einer Ursache verlassen ihn nicht automatisch. Vor einer Rueckkehr
muessen Integritaet, Speicher, aktuelle Sicherheitsbedingungen, Ursache,
Restartsemantik und die fuer die konkrete Fehlerart geforderte Berechtigung
bestanden sein.

Ein kontrollierter Reboot nach Fehlerreset ist nicht generisch. Ein
`FaultResetBootIntent` darf nur fuer eine in der kanonischen Resetpolicy
begruendete Klasse/Fehlerart verwendet werden; OD-24-10 entscheidet die
konkrete Matrix.

## 14. CONFIGURATION_SAFETY_INTEGRATION_GATE

Das Gate ist Issue-Scope und kein optionales Follow-up. Es konsumiert die
realen Typen aus #56/#57:

| Producerergebnis | Safetywirkung |
|---|---|
| `ConfigurationRuntimeFailure` | persistente Systemverriegelung, sichere Bootprioritaet, keine normale Aktorfreigabe |
| `ConfigurationCommitIndeterminate` beziehungsweise `CommitOutcomeUnknown` | unaufgeloester Systemzustand, fail-closed, Recovery erst nach #24-Resetvertrag |
| `ConfigurationUnavailable` | keine Runtime-/Aktorfreigabe, sichere Boot-/Recoveryentscheidung |
| `ConfigurationIntegrityFailure` | persistente Systemverriegelung, `SAFE_BOOT`-Prioritaet, keine normale Aktorfreigabe |

Die reale Integrationsgrenze ist der Application-/Compositionpfad, nicht eine
Testfixture. R3 muss nach Ownerentscheidung OD-24-09 genau eine Stelle in
`FermentationApplication`/den Composition Roots benennen, an der:

1. #56/#57-Ergebnisse entstehen oder empfangen werden;
2. sie ohne zweiten Fehlerhaushalt den zentralen #24-Kern erreichen;
3. der Kern auf den bestehenden #23-`ActuatorSafetyGateInput` projiziert;
4. Planner und Sink diese Entscheidung konsumieren;
5. kein normaler `Allowed`-, Unresolved- oder Recoverypfad den Safety-Latch
   umgehen kann.

Die abstrakte native Gatepruefung ist Teil von #24. Der ESP-IDF-Store-,
Reset- und Aktoradapter bleibt in den jeweiligen E5-Gates. Daraus wird keine
pauschale harte Abhaengigkeit von #29/#90 fuer den abstrakten #24-Kern
abgeleitet. Wenn die fehlende Root-Komposition nicht in #24 gehoert, muss der
Owner ein bestehendes benanntes Gate dafuer festlegen; ein neues pauschales
Follow-up wird nicht erfunden.

## 15. Journalgrenze zu #19 und Ereignisprojektion

Der #24-Kern projiziert strukturierte Fault-/Safety-/Restartentscheidungen ueber
den bestehenden `IEventJournal`-Port. #19 bleibt Eigentuemer von Aufbewahrung,
Bereinigung, Backup, Import, Exportformat und Historienbudget.

Mindestens fachlich zu pruefen sind `FaultCreated`, `FaultEscalated`,
`FaultCauseCleared`, `FaultAcknowledged`, `FaultResetCommitted`,
`FaultResetRejected`, `RestartEpisodeAdvanced`, `RestartEpisodeClosed`,
`SafeBootEntered`, `SafeBootExitDecided`, `SafeBootExitRejected`,
`SafetyRecoveryAttempted`, `SafetyRecoveryAborted` und
`SafetyRecoverySucceeded`, soweit der jeweilige Vorgang im freigegebenen
Scope existiert. Codes, Faultinstanz, Primary-ID, Revision, Episode- und
Evidence-ID werden deterministisch serialisiert.

Ein Journalfehler darf weder sichere Reaktion noch den kanonischen Safety-
State-Commit in eine normale Freigabe umwandeln. Er wird als Diagnose-/
Persistenzbefund nach dem bestehenden Vertrag behandelt; kein zweites Journal
und kein paralleler Safety-Speicher wird gebaut.

## 16. Fail-closed-Regeln und Architekturgrenzen

- Boot, Reset, unbekannter Zustand, fehlende Quelle, unaufgeloester Commit,
  fehlende Berechtigung und offenes Safety-Gate liefern keine normale
  Aktorfreigabe.
- `SAFE_BOOT` besitzt keine leistungsbezogenen Aktortests und keinen normalen
  Serviceeinstieg.
- `device_platform` bleibt anwendungsneutral und native testbar.
- `device_platform_esp_idf` besitzt nur spaetere konkrete Adapter.
- `fermentation_app` besitzt Fault-/Safety-/Recoverylogik nur gegen Ports und
  bestehende Anwendungstypen.
- `device_platform_test_support` bleibt Produktions-abhaengigkeitsfrei.
- `src/main.cpp` bleibt der native Composition Root und `main/app_main.cpp`
  der ESP-IDF-Root; ihre konkrete Safety-Komposition wird erst nach OD-24-09
  und Planfreigabe umgesetzt.
- ADR-013/014 verhindern sowohl Hardwarekopplung im Fachkern als auch eine
  zweite Zustandsmaschine.

## 17. Reproduzierbare Fehlerinjektion und Testmatrix

Tests werden erst nach Ownerentscheidung und Planfreigabe angepasst. Die
folgende Matrix ist der Zielnachweis; sie kodiert keine offenen Zahlen.

### Matrix A – Faultkern

- alle ownerfreigegebenen Codes und vier Klassen;
- Dominanz hoechste Klasse, danach kanonische Detailprioritaet;
- gleichrangige Entstehungsreihenfolge;
- Primary-/Follow-up und unbekannte Producerdaten;
- Active/Acknowledged/CauseClearedLocked/Cleared;
- gleiche stabile Ursache reaktiviert korrekt oder erzeugt begruendet neue
  Instanz;
- bounded Capacity und Overflow fail-closed, ohne harte unbegruendete `8`;
- persistente monotone IDs, falls OD-24-04/Schema dies bestaetigt.

### Matrix B – Commands und Safety-Ausgang

- Ack != Reset und Mute != Ack/Reset;
- ziel- und revisionsgebundener Reset, aktive Ursache, fehlende Berechtigung,
  stale Ziel, weitere blockierende Ursache;
- caller-supplied positive Evaluation/Flags koennen nichts freigeben;
- Klasse 1/2/3/4, Allowed/ImmediateStop/Unresolved;
- #21-erlaubter Auto-Rearm getrennt von unzulaessigem pauschalem Re-Arm;
- S3-004 ohne #35-Qualifikation bleibt sicher gesperrt;
- SAFE_BOOT liefert keine normale Aktorfreigabe;
- #23-Planner/Sink kann den Safetykern nicht umgehen.

### Matrix C – Persistenz und Recovery

- Factory-New-Proof, NotFound ausserhalb Bootstrap, ReadError, CapacityError;
- WriteError und kanonischer RAM-Latch bleibt bis Neustart;
- minimaler persistenter Latch, dessen Fehlerpfad und `SAFE_BOOT`;
- `CommitOutcomeUnknown` vor/nach Commit, ReadbackMismatch, Korruption,
  Cut-before/after-Commit;
- kein Rollback in einen sicheren-looking, aber nicht persistierten RAMstand;
- Latch bleibt ueber Quittierung, Neustart und isolierten Writeversuch;
- Reset nur nach Read-/Write-Integritaet, Ursachefreiheit und Berechtigung;
- bounded ID-/Revisions-/Recordkapazitaet mit Ownerwerten.

### Matrix D – Restart und SAFE_BOOT

- PowerOn, Watchdog/Panic, Brownout, kontrollierter Restart und unbekannter
  Resetgrund;
- Schwelle/Zeitfenster exakt nach OD-24-01, inklusive Grenzwerten und
  Zeitbasis;
- gleiche Observation zweimal fuer Watchdog, Brownout und ControlledSafety:
  exactly once;
- neue Observation normal; Mismatch fail-closed ohne kuenstliche Wiederholung;
- Stromlosigkeit, normaler Neustart und Episode-Schluss gemaess OD-24-02;
- SAFE_BOOT normaler Reboot, falscher/staler Intent und verbleibender Latch;
- nur codebezogene, ownerfreigegebene Reset-/Exitpolicy.

### Matrix E – reale Integration und Journal

- echte #56/#57-Ergebnisse im Application-/Compositionpfad;
- alle vier Konfigurationsfehlerwirkungen und unbekannte Ergebnisse;
- Safety-State, Bootprioritaet, Aktorsperre, Recovery-Resetvertrag;
- #23-Watchdog-Evidence vollstaendig und ohne Trunkierung;
- Planner-/Sink-Nichtumgehung, Unknown/Unresolved;
- jedes freigegebene strukturierte Journalereignis deterministisch;
- fehlerhafter `IEventJournal` verhindert keinen sicheren Commit;
- keine Hardwareeigenschaft als bewiesen markieren.

Historische R2-Testausfuehrungen bleiben technische Nachweise des damaligen
Heads. Sie sind gegen diesen Plan `NOT_ACCEPTED_PENDING_R3`; Tests, die die
R2-RAM-Rollback-, 30-Minuten-, 8-Slot- oder Capability-Semantik voraussetzen,
muessen nach Ownerfreigabe korrigiert oder entfernt werden.

## 18. Kleine Umsetzungs- und Commit-Schnitte nach Planfreigabe

Keine dieser Aktionen erfolgt in diesem Plan-Reset. Nach Ownerfreigabe werden
die Schnitte gegen den dann aktuellen HEAD erneut validiert:

1. **Kanonische Entscheidungen:** Ownerantworten in Fachquellen/ADR festhalten;
   Codes, Restartpolicy, Recoverygrenze, Capacity und Rootbesitz synchronisieren.
2. **Minimaler Resetport, falls freigegeben:** nur anwendungsneutraler Port,
   native Fakes und #29-Abgrenzung; kein Fault-/Restartpolicy-Code im Port.
3. **Faultkern:** vier Klassen, ownerfreigegebene Codes, bounded Datensatz,
   Lifecycle, Dominanz, Primary-/Follow-up und stabile Produceridentitaet.
4. **#15-Commandintegration:** bestehende Ack/Mute/Resetsemantik erhalten;
   zentral berechnete Evaluation, keine positive Callerautoritaet; betroffene
   Konsumenten gemeinsam migrieren.
5. **Persistenz:** Safetyrecord gegen `IStateStore`, Write-before-Apply,
   kanonischer RAM-Latch bei Writefehler und sichere Bootrekonstruktion.
6. **Producer-/Aktor-Gate:** #20/#21/#23 sowie S3-004-Contract gegen den
   vorhandenen Planner-/Sinkpfad; #35-Werte nicht vorwegnehmen.
7. **Restart/SAFE_BOOT:** ownerfreigegebene Schwelle, Zeitbasis, Evidence-
   exactly-once und nur codebezogene kontrollierte Neustarts.
8. **#56/#57-Composition:** die in OD-24-09 ownerbestimmte reale
   Application-/Root-Grenze gegen echte Producerresultate verdrahten.
9. **Events, Injection und Konsumenten:** Journalprojektion, negative Matrix,
   direkt betroffene #15/#21/#23/#56/#57-Tests.
10. **Dokumentationsabschluss:** nur belegte Ergebnisse in Fachquellen,
    `ACCEPTANCE_TESTS.md`, Diagnose und Roadmap synchronisieren.

Nach jedem Schnitt werden nur die gezielten Tests des Schnitts ausgefuehrt.
Ein vollstaendiger nativer Lauf bleibt Owner-Gate nach Review; ESP-IDF,
Remote-CI und Hardware bleiben ihre bestehenden Gates.

## 19. Dokumentationswirkung dieses Plan-Resets

Dieser Commit darf keine neue materielle Safetyentscheidung als kanonisch
ausgeben. Die R2-Zusatzabschnitte im PR-Diff werden deshalb aus den
kanonischen Fachquellen entfernt beziehungsweise als historische technische
Nachweise klassifiziert; die bestehenden `main`-Vertraege mit offenen
Entscheidungen bleiben die Quelle.

`docs/ACCEPTANCE_TESTS.md` behaelt die tatsaechlich ausgefuehrten historischen
PR-Tests mit exakten Befehlen, kennzeichnet sie aber als
`NOT_ACCEPTED_PENDING_R3`. Die Testzahlen sind kein fachlicher PASS.

Nach Ownerantwort werden mindestens diese Quellen im selben
Planungs-/Dokumentationsstand synchronisiert, bevor Implementierung beginnt:

- Restartschwelle/-zeitfenster: `SYSTEM_SAFETY_AND_RECOVERY.md`;
- Codes, Klassen- und Resetpolicy: `SAFETY_AND_FAULTS.md`;
- S3-004-Grenze: `SAFETY_COMPONENT_FAULTS.md` und #35-Vertrag;
- Orakel: `ACCEPTANCE_TESTS.md`;
- echte Architekturentscheidung: akzeptierter ADR und `ARCHITECTURE.md`, falls
  ADR-Pruefung dies verlangt.

## 20. Grenzen zu anderen Issues und Gates

- **#15:** bestehender Command-/Messagevertrag; #24 zentralisiert Safety-
  Evaluation, ersetzt ihn nicht ohne Owner-Gate.
- **#17:** generischer Persistenzport; keine zweite Safety-Persistenzplattform.
- **#18:** Lauf-Recovery und Fortschritt; #24 setzt keinen zweiten Lauf-
  Recoveryautomaten.
- **#20/#21:** Sensorqualitaet, Fallback und Rueckkehr; #24 erfindet keine
  konkurrierende Sensorpolicy.
- **#23:** einziger Aktorplanner-/Sink-Gatepfad und Watchdog-Evidence.
- **#56/#57:** reale Producer und nachgelagertes Integrationsgate; keine
  pauschale zyklische Abhaengigkeit.
- **#19:** Journalaufbewahrung, Historie und Import/Export spaeter.
- **#29/#32/#33 / E5:** konkrete ESP-IDF-/Hardwareadapter und Messungen spaeter.
- **#35:** Commissioningwerte und Recoveryparameter spaeter; kein Testwert ist
  produktive Qualifikation.
- **#106:** produktive Per-Run-Parameter-/Recoverybindung spaeter.
- **E4:** UI/Web/Diagnoseprojektion spaeter, keine Safetyvoraussetzung.

## 21. Owner-Gates und Abschlusskriterien

Vor Implementierung muessen erledigt sein:

1. Antworten auf OD-24-01 bis OD-24-10 oder explizite Begruendung, warum eine
   Frage nicht gilt;
2. Synchronisierung der betroffenen kanonischen Quellen beziehungsweise ein
   akzeptierter ADR;
3. neue exakte Plan-SHA mit `PLAN_R3_PENDING_OWNER_APPROVAL` im PR-Body;
4. Revalidierung von Branch, main, PR-HEAD, Issue und Plan-SHA;
5. Ownerfreigabe exakt dieser SHA.

Nach der Freigabe werden keine alten R2-Komponenten blind fortgeschrieben.
Jeder Teil des eingefrorenen Bestands wird als `KEEP`, `SIMPLIFY_REUSE`,
`RECONCILE`, `REMOVE` oder `REIMPLEMENT` gegen den freigegebenen R3-Vertrag
bewertet. Es gibt keinen Reset, keinen Force-Push und keinen zweiten Issue-
PR.

Issue #24 darf erst als abgeschlossen bezeichnet werden, wenn der Fehlerkern,
die Simulationen, Journalereignisse, Dokumentation und das reale
`CONFIGURATION_SAFETY_INTEGRATION_GATE` gegen #56/#57 bestanden sind. Der PR
bleibt bis zur Ownerentscheidung Draft; der Agent setzt ihn nicht auf Ready
for review, merged nicht, aktiviert kein Auto-Merge, schliesst Issue #24 nicht
und loescht den Branch nicht.

## 22. Ende dieses Auftrags

Dieser Auftrag endet mit:

- dem vollständigen R3-Entwurf und der R2-Reconciliation-Matrix;
- der Ownerentscheidungsmatrix im Plan und PR-Body;
- der Korrektur irrefuehrender R2-Normativtexte und historischer PASS-
  Klassifikation;
- Roadmap-/PR-/Handover-Synchronisierung;
- `git diff --check` und relevanten Dokumentpruefungen.

Firmwaretests, Builds, produktive Tests, Hardware, Remote-CI und jede weitere
Implementierung sind in dieser Runde `NOT_RUN` und bleiben gesperrt.

**Naechster Schritt: Ownerentscheidungen und danach Ownerfreigabe der exakten
vollstaendig synchronisierten R3-SHA. Danach anhalten.**
