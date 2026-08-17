# [E3.5] Issue #24 – Safety Core nach dem Owner Scope Reset

## Planrevision – Owner-Review-Korrektur F1–F4

Diese Revision ist der vollständige, eigenständig ausführbare Plan für PR #110.
Sie schließt die vier materiellen Owner-Befunde F1–F4. Alle fachlichen
Anforderungen, Verträge, Ablaufregeln, Tests und Gates stehen in diesem
Dokument; keine frühere Planrevision ist für eine Umsetzung erforderlich.

## 1. Planstatus und verifizierte Ausgangslage

Der vorherige PR-Stand `8351b93d8d4bf394debb50d5c6b471fcddf76228` ist
`SUPERSEDED / NOT APPROVED` und wird nur als Provenienz dieses
Korrekturauftrags genannt. Vor einer Umsetzung ist die exakte Commit-SHA
dieser Revision ausdrücklich durch den Owner freizugeben.

```text
Repository: ManuEngineer/ESP32-Fermentationsschrank
Issue: #24 – [E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion
PR: #110 – [E3.5] Issue #24 safety core replan from main
PR-Branch: agent/issue-24-safety-core-replan-v2
Base: main
Verifizierte origin/main-SHA: b8eae5f4da5f2666b5a9bda333d115254c4db5b2
PR-HEAD vor dieser Planrevision: 8351b93d8d4bf394debb50d5c6b471fcddf76228
Issue-/PR-Stand verifiziert: 2026-08-17
Planpfad: docs/tasks/issue-24-safety-core-replan.md
Implementation: NOT_STARTED
```

Live verifiziert wurden Repository, Branch, `origin/main`, PR #110 und Issue
#24. PR #110 ist offen und Draft. Issue #24 ist offen und enthält den
Owner-Status `REPLAN_REQUIRED_OWNER_SCOPE_RESET`. PR #107, #108 und #109
sind historische, superseded Referenzen und werden nicht geändert oder als
normative Quelle verwendet. `origin/main` am verifizierten Base-SHA ist der
verbindliche Prüfstand für diese Revision.

Der Branch enthält vor dieser Revision nur die historische Plan-/Roadmap-
Dokumentation. Der Vergleich gegen `origin/main` zeigt keine Änderung an
Produktionscode, Testcode, Adaptern, Persistenzschemas, Builddateien oder
Hardwarekonfiguration. Es gibt auf dem Zielbranch somit keine Issue-24-
Implementation, die versehentlich weitergeführt werden könnte.

### 1.1 Verbindliche Quellenreihenfolge

Für die spätere Umsetzung gelten in dieser Reihenfolge:

1. der Live-Text und die aktuelle Ownerentscheidung in Issue #24;
2. `origin/main` am oben genannten Base-SHA;
3. die akzeptierten ADRs und die in Abschnitt 4 genannten kanonischen
   Fachverträge;
4. dieser Plan nach Freigabe der exakten Plan-SHA;
5. erst danach konkrete Implementierungsdetails.

Der alte PR-#110-Plan liefert höchstens historische Lernhinweise. Er liefert
keine Anforderungen, keine Freigaben, keine Codes, keine Lineage und keine
Capability-Verträge für diesen Plan.

## 2. Owner Scope Reset – normative Release-1-Policy

Die Live-Ownerentscheidung vom 2026-08-17 ist vollständig bindend:

- Beim Boot sind alle Aktoren AUS und bleiben während der gesamten
  Bootvalidierung AUS.
- Ungültige, mehrdeutige oder nicht vertrauenswürdige Konfiguration bzw.
  Safety-Evidenz bleibt fail-closed und führt erforderlichenfalls nach
  `SAFE_BOOT`.
- Ohne gespeicherten aktiven Lauf wird der sichere Standby-Zustand erreicht.
- Ein gespeicherter aktiver Lauf darf nur dann als wiederaufnehmbar gelten,
  wenn er aus den bereits vorhandenen kanonischen Daten vollständig und
  eindeutig rekonstruiert werden kann.
- Auch bei einer eindeutigen Rekonstruktion gibt es keine automatische
  Aktorfreigabe allein durch Boot oder Restore. Es gibt höchstens einen
  expliziten Resume-Pfad.
- Ist der alte Lauf nicht vollständig und eindeutig rekonstruierbar, bleiben
  die Aktoren AUS und der Lauf wird sicher beendet/verworfen oder in den
  kanonischen `NoActiveRun`-Zustand überführt. Danach darf eine neue Charge
  möglich sein, wenn Konfiguration und Safety wieder gültig sind.
- Stromausfall, Brownout, Watchdog-Reset und Crash müssen eine Charge nicht
  transparent oder verlustfrei überleben.
- Safety hat Vorrang vor dem Erhalt einer Charge. Kritische, unbekannte oder
  nicht eindeutig bewertbare Zustände führen niemals zu normaler
  Aktorfreigabe.
- Device Platform, ESP-IDF-Trennung, native Testbarkeit und allgemein
  sinnvolle bestehende Persistenz werden nicht pauschal zurückgebaut.

Die Policy bedeutet ausdrücklich: Release 1 optimiert die sichere
Entscheidung, nicht die automatische Rettung einer unterbrochenen Charge.

### 2.1 Resetcause- und Persistenzentscheidung für R1 (F1)

Die Notwendigkeitsprüfung für jede neue persistente Safety-Information ergibt
keinen zusätzlichen R1-Safety-Record:

| gewünschte Information | unsichere Freigabe ohne sie | bereits ausreichender Schutz | R1-Entscheidung |
|---|---|---|---|
| „Nach Neustart war eine Safety-Sperre aktiv“ | Ein Boot könnte einen alten Lauf oder einen alten Gate-Zustand als `Allowed` behandeln. | Boot all-off, `Unresolved` bis zur vollständigen Live-Prüfung, keine automatische Resume-Freigabe und der bestehende Store-/Config-Vertrag für unbestimmte Zustände. | Kein neuer allgemeiner Safety-Latch-Record. Bestehende kanonische Sperr-/Commitzustände bleiben unverändert. |
| „Der alte Lauf wurde beim Neustart eindeutig beendet“ | RAM könnte ohne bestätigten Store-Commit `Standby` als dauerhafte Wahrheit behaupten. | Der bestehende Run-Persistence-Write-before-Apply-Vertrag schreibt `NoActiveRun` und bestätigt den Readback, bevor RAM/FSM Standby behauptet. | Kein neuer Safety-Record; `NoActiveRun` bleibt der vorhandene Run-Lebenszykluszustand. |
| „N abnormale Neustarts in T“ | Ohne Zähler würde eine wiederholte Störung eventuell nicht erkannt. | Jeder Boot bleibt all-off; Resume ist nie automatisch; unbekannte oder nicht vertrauenswürdige System-/Config-/Persistence-Evidenz bleibt `SAFE_BOOT`/gesperrt. Ein konkreter zusätzlicher Hazard wurde im Bestandsaudit nicht nachgewiesen. | Keine generische Restart-Zählung, kein Zeitfenster, kein Restart-Latch in R1. |

Damit wird kein neuer persistenter Safety-Zustand, kein neuer Key und kein neues
Run-Schema geplant. Ein Resetgrund darf als flüchtige Diagnose- und Boot-Evidenz
aus dem bestehenden beziehungsweise einem kleinen app-neutralen Plattformport
verwendet werden; er erzwingt weder einen Write noch eine automatische Sperre.
Der Port liefert nur eine endliche Ursache oder `Unknown`, niemals eine
fachliche Entscheidung.

Die Bootklassifikation ist verbindlich und hat keine versteckte Zähl- oder
Zeitfenstersemantik:

| Reset-/Bootursache | R1-Bootverhalten | persistenter Nebeneffekt |
|---|---|---|
| Power-on, externer Reset, Brownout | Aktoren AUS, vollständige normale Config-/Persistence-/Sensor-/Safety-Revalidierung, kein automatischer Resume. | Kein Safety-Write und keine Sperre allein aus der Ursache. |
| Watchdog oder Panic | Aktoren AUS, keine automatische Wiederaufnahme; nur nach vertrauenswürdiger vollständiger Neubewertung ist ein nicht freigebendes Resume-Angebot zulässig. | Kein Zähler und kein Zeitfenster. Bei untrusted System-/Config-/Persistence-Zustand `SAFE_BOOT`/unknown-safe. |
| unbekannte, nicht lesbare oder widersprüchliche Ursache | Aktoren AUS, `Unresolved`; keine automatische Wiederaufnahme. | Keine Ableitung eines neuen Latches; bleibt die Gesamtbewertung untrusted, `SAFE_BOOT`/unknown-safe. |

Die Ursache wird nicht als „normal“, „abnormal“ oder „unknown“ in einen neuen
persistenten Vertrag übersetzt. Diese Begriffe sind nur die obige flüchtige
Bootdiagnose. Ein späterer Implementationsbefund, der einen konkreten Hazard
mit einem persistenten Latch nachweist, beendet die Umsetzung an dieser Stelle
und benötigt eine neue Owner-Planfreigabe mit exakt einem begründeten Record,
Feldsatz, Read-/Write-/Readback- und Clear-Vertrag; er wird nicht still in die
Implementierung verschoben.

## 3. Vollständiger Bestandsaudit gegen `origin/main`

### 3.1 Fach- und Codebestand

| Bereich | Tatsächlicher Bestand auf `origin/main` | Konsequenz für #24 |
|---|---|---|
| Issue #17 / Run-Persistenz | `run_persistence_contract.*`, `run_persistence_codec.*`, `run_persistence_store.*` und `run_persistence_coordinator.*` bilden Head-/Slot-/Readback-Verträge, aktive Snapshots, Revisionen, `Completed` und `NoActiveRun`. `IStateStore` unterscheidet `WriteError`, `CapacityError` und `CommitOutcomeUnknown`. | Diese allgemeine, atomare und native-testbare Grundlage bleibt. #24 nutzt sie für sichere Klassifikation und den kanonischen `NoActiveRun`-Abschluss. |
| Issue #18 / Recovery | `run_recovery.*`, `run_recovery_time.*`, `run_progress_weighting.*`, `FallbackRecoveryPending`, `PendingRecoveryAnchor`, `RecoveryTemperatureEvidence`, `PriorBootPhaseElapsed`, `NominalRecoveryAdjustment`, `RecoveryEpisodeEvidence` und `recoveryEpisodeRevision` sind bereits auf `main`. Der aktuelle Pfad berechnet Wiederanlaufzeit, gewichteten Fortschritt und kann einen Lauf weiterführen. | Die technische Decoder-/Kompatibilitätsprüfung bleibt, soweit sie alte Daten sicher erkennen muss. Aktive automatische Zeit-, Gewichts-, Episode-, Fallback-Promotion- und Rettungslogik wird für R1 nicht fortgeführt. |
| Issue #20 / Sensorqualität | `VALID`, `STALE`, `FAILED`, CRC-/Bus-/Bereichs-/Ratenprüfung, Filterung und Re-Identifikation sind kanonische Sensorverträge. | Sensorqualität bleibt eine Safety-Eingabe. Eine klar erlaubte, nachweislich behobene Betriebsstörung darf nach Live-Neubewertung automatisch wieder freigeben; Boot/Restore selbst darf das nicht. |
| Issue #21 / Sensorwahl | Produkt-, Luft- und Kühlrollen, konfigurierte Fallbacks, `AirFallbackActive` und fail-closed Produkt-/Sicherheits-Sensorregeln sind vorhanden. | #24 dupliziert keine Sensor-FSM. Das Safety-Gate konsumiert die kanonische Auswahl-/Qualitätsprojektion und sperrt bei unbekanntem oder unzulässigem Ergebnis. |
| Issue #22 / Temperaturregelung | `ControlRequest`, PI-/Luftbegrenzungslogik und Sensorrollen sind auf `main` integriert. | #24 bewertet die Regelanforderung nur über die Safety-Grenze. #24 ändert keine PI-Regelung und keine Grenzwerte. |
| Issue #23 / Aktorplaner | `ActuatorSafetyGateStatus` startet mit `Unresolved`; `ActuatorPlanner` verwirft unbekannte, unverlässliche oder sofort stoppende Gates, hat Request-Watchdog-Logik und erzeugt Idle-/Stop-Pläne. `ActuatorPlanSinkDriver` schaltet die H-Brücke fail-closed und deaktiviert die Gegenrichtung. | Dieser zentrale Planer-/Sink-Pfad ist die bestehende Aktorgrenze. #24 liefert die einzige kanonische Safety-Direktive; kein Caller darf `Allowed` als Vertrauensbeweis einschleusen. |
| Safety/Fault | Es gibt noch keinen zentralen Issue-24-Fault-/Safety-Service. FSM, Planner und Dokumente enthalten jedoch SafeBoot-, Fault-, Watchdog- und Gate-Projektionen. | Es wird genau eine kleine mutable Safety-Autorität in `fermentation_app` benötigt. Sie aggregiert Ursachen und erzeugt die bestehende `ActuatorSafetyGateInput`-Projektion. Ein neuer persistenter Safety-Record ist nach der F1-Notwendigkeitsprüfung nicht erforderlich. |
| SAFE_BOOT / Resetcause / Watchdog | `ProcessState::SafeBoot` und der Aktor-Request-Watchdog existieren. Ein kanonischer Resetcause-Port und eine ESP-IDF-Adaptergrenze für Diagnose-/Boot-Evidenz existieren noch nicht. | Ein app-neutraler Resetcause-Port wird gemäß ADR-013 nur für flüchtige Diagnose und die oben definierte Bootklassifikation ergänzt. Kein Restart-Zähler, Zeitfenster, automatischer Latch oder Hardwareparameter. |
| Konfiguration #56/#57 | `ConfigurationService`, `ConfigurationRecoveryService` und die Producer-Verträge unterscheiden `ConfigurationRuntimeFailure`, `ConfigurationUnavailable`, `ConfigurationIntegrityFailure` und `ConfigurationCommitIndeterminate` bzw. den zugrunde liegenden unbestimmten Commit. | Das bestehende `CONFIGURATION_SAFETY_INTEGRATION_GATE` bleibt unverändert verbindlich. Jede dieser Ursachen sperrt normale Aktorfreigabe und kann `SAFE_BOOT` erfordern. |
| Composition Root | `src/main.cpp` und `main/app_main.cpp` bauen Platform/Application; die App hängt gegen abstrakte Plattformdienste. Fachliche Safety-Entscheidungen liegen noch nicht in einem zentralen Root. | Safety bleibt Fachlogik in `fermentation_app`. Composition Roots verdrahten nur Ports und stellen den sicheren Startzustand her. |
| Native Test Support | `SimulatedPersistentStateStore` injiziert Read-/Write-/Corruption-/Power-Cut-/`CommitOutcomeUnknown`-Fälle. Zeit-, Sensor-, Aktor- und Notification-Mocks sind vorhanden. | Die vorhandenen Testhilfen werden wiederverwendet. Kein Produktionsmodul hängt von `device_platform_test_support` ab. |

### 3.2 Dokument- und ADR-Bestand

- `docs/SAFETY_AND_FAULTS.md` und `docs/ARCHITECTURE.md` schreiben derzeit
  vier Fehlerklassen, Latches und automatische Reaktionsdetails teilweise als
  allgemeine Architektur fest. Das ist mit der Owner-Policy nur insoweit
  vereinbar, wie unterschiedliche Reaktionen tatsächlich benötigt werden.
- `docs/SYSTEM_SAFETY_AND_RECOVERY.md`, `docs/STATE_MACHINE.md`,
  `docs/RUN_PERSISTENCE.md` und `docs/RECOVERY_AND_INTERRUPTION.md` tragen
  die #18-Semantik eines automatischen bzw. zeitlich gewichteten
  Wiederanlaufs weiter. Insbesondere `RecoveryReject -> Fault` darf nicht
  der generelle R1-Ausgang für einen nicht zu rettenden alten Lauf sein.
- `docs/ACCEPTANCE_TESTS.md` enthält Orakel für automatische Recovery,
  Ausfallintervalle und gewichteten Fortschritt. Diese Orakel sind gegen die
  neue Ownerentscheidung zu bereinigen; Sensor-, Planner-, Persistenz- und
  Konfigurationsorakel bleiben fachlich relevant.
- `docs/SPECIFICATION_REVIEW.md` und `docs/REQUIREMENTS.md` müssen den
  Unterschied zwischen allgemeiner Run-Persistenz und nicht geschuldeter
  transparenter Charge-Recovery ausdrücklich machen.
- `docs/CONFIGURATION_PERSISTENCE.md` dokumentiert das reale #56/#57-Gate
  bereits detailliert. Es bleibt fachlich bindend und erhält nur die
  R1-Folgerung „keine normale Aktorfreigabe“ als Anschlussvertrag.
- `docs/IMPLEMENTATION_ISSUES.md` bleibt die Abhängigkeitsübersicht:
  #20 -> #21 -> #22 -> #23 -> #24 sowie #56/#57 ->
  `CONFIGURATION_SAFETY_INTEGRATION_GATE`. Sie darf keine zweite
  Konfigurations- oder Safety-Wahrheit erzeugen.
- ADR-018 ist mit dem Scope Reset vereinbar: ein kanonischer aktiver
  Konfigurationsgraph, ein nutzbarer Fallback, vorbereitete Ressourcen und
  unresolved Commit fail-closed. ADR-018 wird nicht abgeschwächt und erhält
  keinen Run-Recovery-Sonderweg.
- ADR-013 bleibt vollständig verbindlich. Die Platform liefert nur portable
  Ports/Dienste, ESP-IDF liefert Adapter, `fermentation_app` enthält die
  Fachentscheidung und Test Support bleibt Produktionsfremd.

## 4. Zielvertrag für Release 1

### 4.1 Ein zentraler Safety-Entscheider und ein zentrales Gate

Die spätere Umsetzung erhält genau eine mutable Safety-Autorität in
`fermentation_app` (Arbeitsname `SafetyCore`). Der Name ist kein zusätzlicher
Fachvertrag. Diese Autorität ist verantwortlich für:

- stabile Fault-Codes und die Zuordnung zur kanonischen Quelle;
- den aktiven Safety-Zustand und die sichere Reaktion;
- `SAFE_BOOT`, die Interpretation bestehender kanonischer Sperr-/Commitzustände
  und Reset-/Watchdog-Bewertung; kein neuer persistenter Safety-Zustand;
- die Trennung von Quittierung/Anzeige und fachlicher Wiederfreigabe;
- die aus allen Eingaben aggregierte `ActuatorSafetyGateInput`-Projektion.

`RunPersistenceCoordinator`, `ConfigurationService`, `SensorQuality`,
`SensorSelection`, `ProcessStateMachine` und `ActuatorPlanner` bleiben Besitzer
ihrer Fachzustände. Sie liefern Evidenz oder erhalten eine typisierte
Entscheidung; keiner von ihnen erzeugt eine zweite Safety-Wahrheit.

Die bestehende Planner-Reihenfolge bleibt die letzte anwendungsseitige
Sperre:

```text
kanonische Producer-Evidenz
        -> SafetyCore (einzige Safety-Entscheidung)
        -> ActuatorSafetyGateInput
        -> ActuatorPlanner / ActuatorPlanSinkDriver
        -> abstrakte Aktor-Ports
```

Der Default vor abgeschlossener Validierung ist `Unresolved`. Nur der
SafetyCore darf aus einem intern erzeugten, vollständigen Live-Kontext
`Allowed` ableiten. Ein Request-, UI-, Test- oder Transportfeld
`allowed=true` ist keine Evidenz. Die reale Sink-Grenze bleibt zusätzlich
fail-closed; `device_platform` erhält keine Fachlogik.

### 4.2 Minimale Reaktionsdisposition statt historischer Vierklassenpflicht

Die Fehlerregistrierung erhält stabile, menschenlesbare Codes und eine
maschinenlesbare Reaktionsdisposition. Für R1 sind nur diese drei
Dispositionen erforderlich:

| Disposition | Sichere Reaktion | Wiederfreigabe |
|---|---|---|
| `Informational` | Journal/Anzeige, keine Änderung am Gate | bereits erlaubter Betrieb bleibt nur nach normalem Live-Gate erlaubt |
| `OperationalBlocked` | betroffene Aktoren sofort AUS, Zustand wird neu bewertet | automatisch nur bei ausdrücklich dafür freigegebenem Code und nach nachweislich gültiger Live-Evidenz; niemals durch Quittierung oder Boot |
| `LatchedOrSafeBoot` | Aktoren AUS, eine bereits kanonisch vorgesehene persistente Sperre oder `SAFE_BOOT`, je nach Ursache | nur über die für den Code definierte, fachlich zulässige Behebung und vollständige Neubewertung; unbekannt bleibt gesperrt; #24 erfindet keinen neuen Latch |

Die Dispositionen sind keine neue historische Fault-Lineage. Jede Fault-Quelle
hat einen stabilen Code, eine Quelle, `active/cleared/acknowledged`-Projektion
und eine feste Dispositionsregel. Die Implementierung führt nicht automatisch
vier Klassen, Primary-/Follow-up-Lineage oder eine unbeschränkte Fault-Historie
ein.

Quittierung ändert nur Anzeige/Journaling. Sie löscht weder eine fachliche
Sperre noch erzeugt sie `Allowed`. Automatische Wiederfreigabe ist auf klar
erlaubte, nachweislich behobene Betriebsfehler begrenzt, etwa eine gültig
reaktivierte Sensorqualität nach #20/#21. Kritische, unbekannte, Integritäts-,
unbestimmte Commit- und nicht verifizierbare Persistenzzustände bleiben
gesperrt.

### 4.3 Notwendigkeitsprüfung statt neuer Safety-Persistenz (F1)

Die Prüfung aus Abschnitt 2.1 ist ein verbindlicher Planbestandteil und kein
späteres Design-Detail. Für R1 wird **kein** allgemeiner Safety-Record mit
Restartzähler, Resetfenster, Latch-Flag, Resetzeitquelle oder eigener
Clear-Regel eingeführt. Es gibt daher auch keinen neuen Safety-Key, kein
zusätzliches Persistenzschema und keinen zweiten Safety-Store.

Der unsichere Zustand nach jedem Neustart wird bereits durch die Kombination
aus Boot-all-off, `ActuatorSafetyGateStatus::Unresolved`, vollständiger
Config-/Persistence-/Sensor-/Planner-Prüfung, explizitem Start/Resume und dem
bestehenden `CommitOutcomeUnknown`-Vertrag verhindert. Ein persistierter alter
Run ist damit keine Freigabeinformation. Ein sicherer Abbruch schreibt den
bestehenden kanonischen `RunCheckpointVariant::NoActiveRun` und darf erst nach
bestätigtem Readback in RAM/FSM als Standby erscheinen.

Ein bestehender Config-/Store-Zustand, der bereits selbst `SAFE_BOOT`,
Unverfügbarkeit oder unbestimmtes Commit bedeutet, bleibt unverändert
maßgeblich. #24 interpretiert ihn fail-closed, kopiert ihn aber nicht in einen
neuen Safety-Record. Ein Resetjournal beziehungsweise eine vorhandene
Diagnoseprojektion darf die Ursache anzeigen; das ist keine dauerhafte
Safety-Wahrheit und erzwingt keinen Write.

### 4.4 Resetcause und Watchdog ohne Restart-Latch

`fermentation_app` erhält, falls der Audit des aktuellen Composition Roots dies
für die vorhandene Diagnose benötigt, nur einen schmalen app-neutralen
Resetcause-Port. Der Port liefert eine endliche bekannte Ursache oder
`Unknown`, kennt weder ESP-IDF-Typen noch GPIOs und trifft keine
Safety-Entscheidung. Der ESP-IDF-Adapter mappt ausschließlich den öffentlichen
ESP-IDF-Reset-/Watchdog-Vertrag; native Tests injizieren jede Ursache einzeln.

Die Bootreihenfolge ist immer:

1. Aktoren hart auf den sicheren Aus-Zustand bringen, bevor Validierung oder
   Restore beginnt.
2. Ursache flüchtig als Diagnose-/Boot-Evidenz lesen; es gibt keine
   Neustartzählung und kein Zeitfenster.
3. Konfiguration und die realen #56/#57-Producer-Gates prüfen.
4. Run-Persistenz read-only laden und mit dem exakten Prädikat aus Abschnitt
   5.2 klassifizieren.
5. Erst nach vollständiger Validierung `Standby`, ein nicht freigebendes
   `RecoveryEvaluation`-Angebot oder `SAFE_BOOT` darstellen. Kein Boot- oder
   Restore-Schritt erzeugt `Allowed` oder ein Aktorcommand.

Bei `PowerOn`, externem Reset und Brownout ist das eine normale vollständige
Revalidierung ohne automatischen Resume. Bei Watchdog, Panic und unbekannter
Ursache bleibt die Wiederaufnahme ebenfalls nicht automatisch; ein
untrusted System-/Config-/Persistence-Zustand führt zu `SAFE_BOOT`/unknown-
safe. Ein vertrauenswürdiges, vollständig qualifiziertes Resume-Angebot bleibt
nicht freigebend und benötigt weiterhin die explizite Nutzerentscheidung.

Der bereits vorhandene Aktor-Request-Watchdog und seine latched Stop-Reaktion
bleiben Bestandteil des Planner-Vertrags. #24 ergänzt Resetcause-/Boot-
Bewertung, ersetzt aber nicht den #23-Watchdog und erfindet keine
Hardware-Watchdogparameter.

## 5. Boot-, Restore- und Run-Vertrag

### 5.1 Zustandsablauf und nicht freigebendes Resume-Angebot

| Eingang | Klassifikation | Persistenz-/FSM-Ausgang | Aktor-Gate |
|---|---|---|---|
| gültige Konfiguration, kein aktiver Lauf | sicherer leerer Start | bestehender Boot-Ready-Pfad, danach `Standby` mit `NoActiveRun` | `Unresolved` bis zum Ende der Bootprüfung; kein Aktorcommand |
| aktiver Lauf erfüllt das Prädikat aus 5.2 | Resume-Angebot | `BootRecoverRun`/`RecoveryEvaluation` als nicht freigebende Projektion; kein automatischer Resume | AUS; kein `Allowed`, kein Planner-Command |
| aktiver Lauf ist unvollständig, widersprüchlich, beschädigt, hat offene Recovery-Evidenz oder ist nicht beweisbar | sicherer Abbruch | zuerst kanonisches `NoActiveRun`; erst nach bestätigtem Commit `Standby` | AUS; kein Fault-Resume und keine Fallback-Promotion |
| Nutzer lehnt Resume ab oder Angebot läuft ab | sicherer Abbruch desselben alten Runs | `RecoveryReject`/`RecoveryRejected` über den unten definierten Write-before-Apply-Pfad zu `NoActiveRun`, danach `Standby` | AUS bis zur bestätigten Persistenz, danach Standby ohne Aktorfreigabe |
| `NoActiveRun`-Write, Readback oder Commitstatus nicht eindeutig | unbekannter kritischer Persistenzzustand | kein normaler Standby; bestehender Store-/Bootvertrag bleibt gesperrt, erforderlichenfalls `SAFE_BOOT` | `ImmediateStop`/`Unresolved` |
| Konfiguration oder Safety unbekannt/ungültig | kritischer/unknown Zustand | `SAFE_BOOT` oder bestehender latched Zustand; kein alter Run wird gerettet | `ImmediateStop` |
| Nutzer bestätigt Resume | expliziter Fachbefehl | aktuelle Sensor-/Config-/Safety-Prüfung, dann persistente Resume-Entscheidung; erst bei bestätigtem Commit FSM-Aktivierung | bis dahin `Unresolved`/AUS; danach erst normales Planner-Gate |

`RecoveryEvaluation` bedeutet ausschließlich: Der alte Zustand darf als
nicht freigebendes Angebot angezeigt beziehungsweise fachlich bewertet werden.
Es bedeutet weder `Allowed` noch „Aktorfreigabe vorbereiten“. Ein Resume-
Angebot darf auch nach Boot, Brownout, Watchdog oder Panic niemals automatisch
in einen aktiven Plannerpfad übergehen.

### 5.2 Exaktes Rekonstruktionsprädikat für Schema 1/2/3 (F3)

Die R1-Entscheidung wird als ein kanonisches Prädikat geplant:

```text
r1SimpleResumeEligible(snapshot, selectedRecord) =
    knownSchema(selectedRecord.schemaVersion)
 && selectedRecord.schemaVersion != 0
 && validHeadSlotRecordCrcAndRevision(selectedRecord)
 && selectedRecord is the unique current record
 && activeVariant(snapshot)
 && validRequiredRunAndProcessFields(snapshot)
 && validCurrentSensorSelection(snapshot)
 && configAndProducerGateCanBeRevalidated(snapshot)
 && noSemanticOpenRecoveryEvidence(snapshot)
 && noUnresolvedOrOrphanedStoreState(selectedRecord)
```

`schemaVersion == 3` ist ausdrücklich zulässig. Die reguläre Existenz der
Schema-3-Felder ist ebenfalls ausdrücklich zulässig. Ein Fallback wird nur zur
Integritätsprüfung gelesen; er wird nicht automatisch promoted, gerollbackt
oder als zweiter Resume-Kandidat gewählt. Sind Current und Fallback nicht
eindeutig auf denselben kanonischen Zustand reduzierbar, ist das Prädikat
false.

Die Feldsemantik ist verbindlich:

| Feld/Gruppe im aktuellen `RunPersistenceSnapshot` | R1-Klassifikation | Verwendung im Prädikat/Resume |
|---|---|---|
| `schemaVersion == 1`, `2` oder `3`, Recordtyp, Slot, CRC, Storage-Epoch, Head-/Checkpoint-Revision | erforderlich | Muss gültig, bekannt und eindeutig sein. `3` ist kein Ablehnungsgrund. |
| `variant`, `activeRunId`, Programm-/Manual-Snapshot, `processState`, `processRunSnapshot`, `runRevision`, Revisionen und persistierte Command-IDs | für eindeutige Rekonstruktion erforderlich | Müssen strukturell gültig, widerspruchsfrei und dem aktiven Run zugeordnet sein. |
| `activeRunSensorMode`, `sensorSelection` und die #20/#21-Projektion | für die aktuelle Live-Prüfung erforderlich | Kein Resume ohne aktuelle Neubewertung; #24 erfindet keine Sensorqualität oder Sensorwahl. |
| `pendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis` | offene/alte Recovery-Evidenz | Wenn eines vorhanden ist, blockiert es den einfachen R1-Resume. Es wird nicht als Zeitanker verwendet; der alte Run geht in den `NoActiveRun`-Abbruch. |
| `lastRecoveryEpisodeEvidence`, `priorBootPhaseElapsed`, `nominalRecoveryAdjustment` | offene/alte Recovery-/Carry-Forward-Evidenz | Wenn eines vorhanden ist, blockiert es den einfachen R1-Resume. Keine Zeit-, Temperatur- oder Korrekturgutschrift. |
| `runProgress.weightedProgress` oder `runProgress.basis == PartialUnknownHistory` | aktive komplexe Progress-/Legacy-Evidenz | Blockiert den einfachen R1-Resume; kein gewichteter Fortschritt, keine Promotion und kein Rollback. |
| `runProgress.basis == KnownTotal`, `weightedProgress == null`, `observedRunSeconds` beliebig | neutraler/normaler R1-Bestand | Die Felder dürfen vorhanden sein. `observedRunSeconds` wird nur auf Struktur/Overflow geprüft und niemals als automatische R1-Gutschrift oder Aktorfreigabe verwendet. |
| `recoveryTemperatureEvidence` mit allen Rollen `quality == Stale` und ohne `filteredCelsius` | neutraler Schema-3-Default | Für Resume ignorierbar; kein Ausschluss wegen seiner nicht-optionalen Existenz. |
| `recoveryTemperatureEvidence` mit nichtneutralem Last-known-Wert | reine Legacy-/Diagnoseinformation | Darf gelesen/angezeigt werden, blockiert allein nicht; es ersetzt keine frische #20/#21-Evidenz und wird nie als Resume- oder Zeitbeweis verwendet. |
| `recoveryEpisodeRevision == 0` oder ein nicht mit offenen Markern gekoppelter Wert > 0 | reine Legacy-/Diagnoseinformation | Allein kein Ablehnungsgrund und keine Restartzählung. Bei gekoppelter offener Recovery gilt die Zeile der offenen Marker. |
| vorbereiteter Head, beschädigter Current-Record, widersprüchlicher Fallback, `CommitOutcomeUnknown` oder nicht bestätigter Readback | nicht rekonstruierbarer Storezustand | Prädikat false; keine automatische Reparatur oder Promotion, sondern `NoActiveRun` beziehungsweise unknown-safe. |

`validRequiredRunAndProcessFields` verlangt zusätzlich, dass der gespeicherte
Prozesszustand seine aktuelle Run-Projektion trägt und ein neuer Boot keine
abgelaufene Phase aus einer alten monotone Zeit, UTC-Differenz,
`PriorBootPhaseElapsed`, `observedRunSeconds` oder gewichteten Coverage
ableiten müsste. Wenn eine Phase ohne so eine alte Zeit-/Progressgutschrift nicht
eindeutig weitergeführt werden kann, ist das Ergebnis `not reconstructable`.
Die einfache R1-Rekonstruktion verwendet nur den kanonisch gespeicherten
Prozess-/Programmkern und frische Live-Prüfungen; alte Recoverywerte werden
weder angewendet noch zu Aktorfreigaben umgerechnet.

Damit gilt insbesondere: Ein ansonsten vollständiger und eindeutiger aktiver
Run mit Schema 3, neutralen Defaultfeldern, `recoveryTemperatureEvidence`-
Default und normalem `RunProgressState` wird nicht allein wegen dieser Felder
verworfen. Nur die oben ausdrücklich benannte semantisch aktive offene
Recovery-/Unknown-Evidenz blockiert den einfachen Resume.

### 5.3 Exakte Write-before-Apply-Semantik für Resume und Abbruch (F4)

Die vorhandenen `RecoveryEvaluation`, `RecoveryResume`, `RecoveryReject`,
`RecoveryResumed` und `RecoveryRejected` werden wiederverwendet. Es entsteht
keine zweite FSM und kein neuer Event/Reason. Die Topologie wird verbindlich
angepasst:

```text
RecoveryEvaluation --RecoveryResume/RecoveryResumed--> valid active phase
RecoveryEvaluation --RecoveryReject/RecoveryRejected--> Standby
```

`RecoveryRejected` gilt ausschließlich für ein fachlich entschiedenes
Angebot, einen Ablauf oder eine unklare Rekonstruktion nach dem kanonischen
`NoActiveRun`-Commit. Es führt nicht mehr von `RecoveryEvaluation` nach
`Fault`; `Fault` bleibt für echte Safety-/Fachfehler. Der Reason darf erst als
autoritativer FSM-Übergang sichtbar werden, wenn die Persistenzreihenfolge
erfolgreich abgeschlossen ist.

#### Resume-Angebot und bestätigter Resume

1. Der read-only Audit erfüllt 5.2 vollständig. `RecoveryEvaluation` bleibt
   nicht freigebend; es gibt kein `Allowed` und kein Aktorcommand.
2. Der Nutzer sendet einen expliziten Resume-Befehl. Der Aufrufer prüft die
   aktuelle Konfiguration einschließlich #56/#57, frische #20/#21-Sensorbasis,
   SafetyCore und die Prozess-/Planner-Voraussetzungen. Der Planner bleibt
   `Unresolved`/AUS.
3. Aus `RecoveryResume` wird ein vollständiger detached Kandidat für die
   gültige aktive Zielphase gebildet. Die bestehende
   `RunPersistenceCoordinator`-Write-before-Apply-Grenze
   (`persistRecoveryCandidate` oder der gleichwertige bestehende
   `persistTransition`-Pfad) schreibt Prepared-Head, Zielsnapshot und
   Committed-Head und bestätigt jeden erforderlichen Readback exakt.
4. Nur das Ergebnis `Applied` mit eindeutig bestätigtem Commit darf den
   autoritativen RAM-Zustand durch die bestehende FSM-Anwendung auf den
   `RecoveryResumed`-Zielzustand aktualisieren. Erst danach wird ein normaler
   Live-Planner-Tick mit allen Gates erlaubt; der Persistenzcommit selbst ist
   kein `Allowed`.
5. Bei Write-, Capacity-, Codec-, Readback- oder
   `CommitOutcomeUnknown`-Ergebnis bleibt der bisherige RAM-/FSM-Zustand
   `RecoveryEvaluation` beziehungsweise unknown-safe; es gibt keinen aktiven
   Plannerpfad. Ein indeterminater Storezustand führt nach dem bestehenden
   Vertrag zu `SAFE_BOOT`/gesperrt.

#### Ablehnung, Ablauf oder nicht rekonstruierbarer Run

1. Aktoren bleiben `AUS`; SafetyCore liefert `Unresolved`/`ImmediateStop`.
2. Ein detached Kandidat wird mit der bestehenden
   `RecoveryReject`-Entscheidung nach `Standby` und mit
   `clearActiveRunState` als kanonischer `RunCheckpointVariant::NoActiveRun`
   Projektion gebildet. Die autoritative `current`-FSM und der laufende
   Snapshot bleiben bis zum Commit unverändert.
3. Zuerst wird genau dieser `NoActiveRun`-Kandidat über den bestehenden
   atomaren Run-Persistence-Vertrag geschrieben. `Applied` bedeutet erst:
   Prepared-Head, Payload, Committed-Head und der erforderliche exakte
   Readback sind bestätigt. Kein „Write erfolgreich“ ohne Readback genügt.
4. Erst nach diesem bestätigten Ergebnis wird der detached Kandidat
   autoritativ angewendet: `RecoveryRejected` führt `RecoveryEvaluation` nach
   `Standby`, die aktiven Run-Felder werden gelöscht und der kanonische
   `NoActiveRun`-Zustand ist die neue RAM-/Persistenzwahrheit.
5. Bei `WriteError`, `CapacityError`, Readbackfehler oder
   `CommitOutcomeUnknown` wird kein normales `Standby` behauptet. Die
   Aktoren bleiben gesperrt; bei unbestimmtem Ergebnis bleibt der Boot- und
   Safetyzustand nach dem bestehenden Storevertrag `SAFE_BOOT`/unknown-safe.
   Ein späterer Boot entscheidet ausschließlich aus neu gelesenem,
   eindeutigem Storezustand.

Der initiale Bootfall „nicht rekonstruierbar“, in dem noch kein
`RecoveryEvaluation`-Angebot autoritativ aktiv ist, verwendet denselben
detached `NoActiveRun`-Write-before-Apply-Pfad und wendet danach den bereits
vorhandenen `BootReady`-/`Standby`-Übergang an. Nutzerablehnung, Timeout und
ein nachträglich unklar gewordener Resume verwenden ausdrücklich
`RecoveryReject`/`RecoveryRejected`. In keiner Variante wird zuerst RAM-
Standby behauptet und anschließend versucht, den alten aktiven Snapshot zu
löschen.

## 6. Bestandsmatrix A/B/C und konkrete Entscheidungen

### A – für die vereinfachte Release-1-Policy benötigt

| Mechanismus | Warum benötigt | Entscheidung |
|---|---|---|
| `ProcessState` mit `Boot`, `SafeBoot`, `Standby`, `RecoveryEvaluation`, aktiven Phasen und terminalen Zuständen | Sichere Boot-/Standby-/Angebots-/Fault-Projektion ist bereits der kanonische Prozessvertrag. | Behalten; `RecoveryEvaluation` wird explizites Angebot, nicht automatische Freigabe. |
| `ActuatorSafetyGateStatus::Unresolved/Allowed/ImmediateStop` und Planner-Auswertung | Zentrale, nicht umgehbare Anwendungsgrenze gegen normale Aktorfreigabe. | Behalten und an SafetyCore binden; kein caller-supplied `Allowed`. |
| `ActuatorPlanner`-Stop-/Watchdog- und `ActuatorPlanSinkDriver`-Idle-/Gegenrichtungslogik | Sofortige sichere Reaktion und letzte Sink-Grenze. | Behalten; route-spezifische Allow-/Deny-Tests ergänzen. |
| #17 Head-/Slot-/CRC-/Schema-/Revision-/Readback-Validierung | Vollständige und eindeutige Rekonstruktionsprüfung sowie sicherer `NoActiveRun`-Abschluss. | Behalten; keine automatische Rettung aus unklaren Daten. |
| `RunCheckpointVariant::NoActiveRun`, aktiver Snapshot und terminale Run-Zustände | Kanonischer leerer Zustand, sichere Beendigung und allgemeine Persistenz. | Behalten; `NoActiveRun` ist der R1-Abbruchausgang. |
| `RunPersistenceCoordinator`-Load-/Write-Ergebnisse einschließlich Read-/Write-/Indeterminate-Fällen | Erkennen, ob ein alter Lauf eindeutig ist und ob sein sicherer Abschluss commitbar ist. | Behalten; API-Semantik auf explizite Qualifikation/Abbruch ausrichten. |
| #20 `SensorQuality` und #21 Sensorwahl/Fallback | Aktorfreigabe darf nur mit gültiger, aktueller und eindeutig ausgewählter Sensorbasis erfolgen. | Behalten; keine parallele #24-Sensor-FSM. |
| #22 `ControlRequest` und #23 Planner-Verträge | SafetyCore muss die reale fachliche Anforderung sperren, nicht eine zweite Regelung erfinden. | Behalten; keine Grenzwert- oder PI-Änderung. |
| #56/#57 Configuration-Safety-Producer | Konfigurationsintegrität ist unabhängig vom vereinfachten Run-Recovery und muss fail-closed bleiben. | Vollständig integrieren; alle vier benannten Producer-Zustände sperren normale Aktoren. |
| SafeBoot-/Fault-Projektion und stabile Fehlercodes | Unknown/critical darf nie normal freigeben; Diagnose muss nachvollziehbar bleiben. | Als kleine Safety-Autorität ergänzen, ohne historische Lineage. |
| Resetcause-/Boot-Evidenz | Bootdiagnose und reproduzierbare Resetcause-Fehlerinjektion sind für die sichere Erstbewertung relevant. | Einen kleinen app-neutralen Port nur für flüchtige Ursache/Diagnose planen; keine Restart-Zählung, kein Zeitfenster und kein neuer persistenter Safety-Record. |

### B – allgemein sinnvoll und zu behalten

| Mechanismus | Nutzen außerhalb komplexer Recovery | Entscheidung |
|---|---|---|
| `device_platform`-Ports, `IPlatformServices`, `IStateStore`, `StorageEnvelope`, Slot-Kandidaten und CRC | Portable, anwendungsneutrale Persistenz- und Plattformbasis. | Behalten; keine Safety-Fachlogik in `device_platform`. |
| `device_platform_test_support` mit deterministischen Zeit-, Sensor-, Aktor- und Store-Injektionen | Native, reproduzierbare Nachweise für Fail-Closed-Verhalten. | Behalten; keine Produktionsabhängigkeit. |
| Atomarer Active-/Fallback-Store sowie eindeutige Head-/Slot-Prüfung | Allgemeine Schutzfunktion gegen Teilwrites und Readbackfehler, auch ohne Charge-Recovery. | Behalten; Fallback bleibt Integritätswerkzeug, nicht automatische Rettungsfreigabe. |
| Normale Run-Revisionen, Checkpoints, Programm-Snapshot und `observedRunSeconds`, soweit für einen laufenden Run benötigt | #17-Run-Persistenz und Diagnose bleiben sinnvoll, auch wenn Neustart nicht verlustfrei sein muss. | Behalten, aber keine unbewiesene alte Zeit als R1-Fortschritt gutschreiben. |
| Terminale `Completed`-/`NoActiveRun`-Records und sichere neue-Run-Initialisierung | Allgemeine Lebenszyklus- und Bedienlogik. | Behalten und für den sicheren Abbruch verwenden. |
| #20 Re-Identifikation und #21 erlaubter Fallback | Nachweislich behobene Betriebsfehler können ohne Neustart wieder bewertet werden. | Behalten; automatische Wiederfreigabe nur gemäß expliziter Code-Policy und Live-Gate. |
| Event-Journal und User-Notification-Port | Diagnose und Quittierung bleiben von fachlicher Freigabe getrennt. | Behalten; kein zweiter Eventbus und keine Anzeige als Safety-Quelle. |
| ESP-IDF-/native Composition-Root-Trennung | Plattformwechsel und Testbarkeit ohne Safety-Abhängigkeit von Netzwerk/Web. | Behalten gemäß ADR-013; spätere Adapter liefern nur Ports. |
| Bestehender Planner-Request-Watchdog | Schutz gegen stale/missing live control requests. | Behalten; von Resetcause-Bewertung getrennt testen. |

### C – historische Recovery-Bestandteile mit C1/C2-Trennung (F2)

Kategorie C wird nicht als pauschaler Cleanup-Auftrag behandelt. Entscheidend
ist zuerst, ob ein Baustein noch einen aktiven Produktpfad beeinflusst.

#### C1 – zwingend aus dem aktiven #24-Produktpfad entfernen/deaktivieren

| Aktiver Pfad/Baustein | Hazard | R1-Entscheidung |
|---|---|---|
| automatische Recovery-Aktivierung aus Boot, Restore oder Fallback, einschließlich `activateLoadedRun`/`activateFallbackRecoveredRun` soweit sie ohne explizite Nutzerentscheidung weiterführen | Alter Run kann trotz Reset/unklarer Zeit wieder aktiv werden. | Auf read-only Qualifikation und explizites Resume-Angebot begrenzen; kein aktives Recovery aus Boot/Restore. |
| `run_recovery_time.*`, `reevaluateRecoveryTime` und Zeit-/UTC-Ausfallintervallpfade, soweit sie im Produktpfad eine Phase, Zeit oder Abschlussentscheidung automatisch weiterführen | Alte Unterbrechungszeit kann als fachlicher Fortschritt oder als automatische Freigabe wirken. | Auf die in 5.2 benötigte Klassifikation reduzieren oder Aufrufe entfernen; keine Ersatzzeitrechnung. |
| `run_progress_weighting.*`, `RecoveryProgressWeightingModel` und `applyRecoveryProgressWeighting`, soweit sie Snapshot-/RAM-Werte mutieren oder gutschreiben | Alter gewichteter Wert kann einen Lauf oder Aktorpfad fördern. | Aktive Produzenten/Mutatoren aus dem R1-Produktpfad entfernen; Felder höchstens decoderseitig erkennen. |
| Fallback-Promotion, Rollback, `FallbackRecoveryPending`-Auflösung und `resolveRecoveryOutcome`-Varianten mit automatischer Entscheidung | Fallback kann einen alten oder widersprüchlichen Run aktivieren. | Fallback nur read-only zur Eindeutigkeitsprüfung; keine Promotion, kein Rollback-Resume, bei Unklarheit `NoActiveRun`/unknown-safe. |
| jede Recovery-Write-API, die ohne explizite Bestätigung `RecoveryResume`, Zeitgutschrift, Fortschritt, Fallback oder Aktorfreigabe persistiert | Persistenz könnte eine Recoveryentscheidung vor der Nutzer-/Live-Prüfung festschreiben. | Nur der explizite Resume- und der `NoActiveRun`-Write-before-Apply-Pfad aus 5.3 bleiben aktiv. |

C1 umfasst nur die Aufrufstellen, Mutatoren und Orakel, die tatsächlich einen
der genannten Produktwirkungen erzeugen. Die Umsetzung muss für jeden
Call-Site nachweisen, dass Boot, Restore, Start, Sensorwahl, Planner und Sink
keinen C1-Pfad mehr zur automatischen Run-/Aktoraktivierung erreichen.

#### C2 – nach C1 ungenutzter Legacy-/Deprecated-Bestand

| Bestand | Einstufung nach C1 | R1-Entscheidung |
|---|---|---|
| `PendingRecoveryAnchor`, `RecoveryTemperatureEvidence`, `lastRecoveryEpisodeEvidence`, `priorBootPhaseElapsed`, `nominalRecoveryAdjustment`, `recoveryEpisodeRevision` und ihre Schema-3-Codec-Felder | Teilweise reguläre Schema-3-Wirefelder; aktive Marker und neutrale Defaults sind gemäß 5.2 verschieden zu behandeln. | Decoder-/Wire-Kompatibilität und sichere Klassifikation nur soweit nötig behalten; keine pauschale Löschung und keine aktive R1-Gutschrift. |
| `run_recovery_time.*`, `run_progress_weighting.*`, historische Hilfsmodelle und zugehörige Tests, nachdem alle C1-Aufrufe entfernt sind | Toter oder ungenutzter Legacy-Code ohne aktiven Produktpfad. | Als `Legacy/Deprecated` dokumentieren und in #24 nicht pauschal löschen. Ein kleiner separater Follow-up-Cleanup wird nach stabiler #24-Integration empfohlen. |
| alte Zeitintervall-, gewichtete-Progress-, Fallback- und transparente-Reboot-Orakel ohne produktive Aufrufstelle | Nicht mehr normative Tests; keine R1-Sicherheitsbeweise. | Nicht als PASS verwenden. In #24 nur dort korrigieren/entfernen, wo sie den aktiven Testvertrag oder Architekturguard verfälschen; übrige Bereinigung als Follow-up. |

Für C2 sind ein statischer/Architektur-Negativnachweis der erlaubten
Produktionsaufrufe und mindestens ein Anwendungstest vorzusehen, der bei
Schema-3-Legacy-/Open-Recoverydaten ausschließlich `NoActiveRun`/unknown-safe
und keinen C2-Mutator/Plannerpfad erreicht. Nur wenn ein konkreter C2-Baustein
technisch nicht sicher abtrennbar ist oder einen Parallelvertrag erzeugt, darf
seine Entfernung in #24 verbleiben; der Plan muss dann Baustein, Call-Site,
Abtrennungsversuch und den konkreten Safetygrund nennen. Diese Ausnahme ist
nicht pauschal für ganze Dateien oder Testverzeichnisse erteilt.

## 7. Konfigurations- und Producer-Gate #56/#57

Das `CONFIGURATION_SAFETY_INTEGRATION_GATE` bleibt ein unveränderter
Querschnittsvertrag. Der SafetyCore übernimmt die realen Producer-Ausgänge,
ohne neue parallele Fehlernamen oder eine zweite Config-Integritätslogik zu
schaffen:

| Producer-Zustand | R1-Safetyreaktion |
|---|---|
| `ConfigurationRuntimeFailure` | `LatchedOrSafeBoot`/`ImmediateStop`; keine normale Aktorfreigabe. Erst nach dem bestehenden fachlichen Recovery-/Readback-Vertrag und vollständiger Neubewertung weiter. |
| `ConfigurationUnavailable` | `SAFE_BOOT` oder latched Sperre; keine Annahme von Default-/Factorywerten als produktive Freigabe. |
| `ConfigurationIntegrityFailure` | `SAFE_BOOT`/`ImmediateStop`; Integritätsfehler wird nicht quittierungs- oder UI-seitig freigegeben. |
| nicht auflösbarer `CommitOutcomeUnknown` bzw. `ConfigurationCommitIndeterminate` | Zustand bleibt unbestimmt; weder alte noch neue Konfiguration behaupten, keine Publish-/Aktorfreigabe, keine Slot-Wiederverwendung. |

Diese Zustände beeinflussen die Run-Policy unabhängig davon, ob ein aktiver
Run rekonstruierbar wäre. Ein eindeutig gespeicherter Run darf bei
ungültiger/unbekannter Konfiguration nicht normal freigegeben werden. Der
Scope Reset vereinfacht nur die Laufrettung, nicht die Konfigurations-
integrität.

## 8. Modul- und Adapterarchitektur

Die spätere Änderung hält ADR-013 und alle lokalen `AGENTS.md` ein:

- `lib/device_platform`: nur app-neutrale Ports/Dienste. Falls der Audit die
  fehlende Resetcause-Fähigkeit bestätigt, kommt hier ein kleiner abstrakter
  Resetcause-/Boot-Evidence-Port für flüchtige Diagnose hinzu. Keine
  Restart-Zählung, Safety-Persistenz, Fault-Codes, Run-Modelle, GPIOs,
  ESP-IDF-Enums oder Safety-Entscheidungen.
- `lib/device_platform_esp_idf`: ausschließlich Mapping der öffentlichen
  ESP-IDF-Reset-/Watchdog-Verträge auf den abstrakten Port und die sichere
  Initialausgabe. Keine Fachlogik, keine Restartzählung und kein Run-Recovery.
- `lib/device_platform_test_support`: deterministische Resetcause-,
  Persistence-, Sensor- und Aktorsimulation. Keine Produktionsreferenz und
  keine Safety-Fachtypen als Plattformvertrag.
- `lib/fermentation_app`: SafetyCore, stabile Fault-/Reaktionsverträge,
  Boot-/Restore-Entscheidung, #56/#57-Gate und die Verbindung zur vorhandenen
  Planner-Safety-Grenze. Die App kennt weder ESP-IDF noch konkrete GPIOs.
- `src/main.cpp` und `main/app_main.cpp`: nur Composition Root. Sie erzeugen
  Ports, übergeben die Safety-/Persistence-Abhängigkeiten und starten mit
  sicheren Ausgaben; sie treffen keine eigenen Prozess- oder Faultentscheidungen.

Geplante betroffene Bestandsdateien und neue Zielbereiche sind mindestens:

```text
lib/device_platform/src/              Resetcause-/Boot-Evidence-Port, falls Audit bestätigt
lib/device_platform_esp_idf/src/      öffentlicher ESP-IDF-Adapter
lib/device_platform_test_support/src/ deterministische Reset-/Store-Injektion
lib/fermentation_app/src/             SafetyCore, Boot-/Run-/Config-Verknüpfung
  process_state_machine.*              RecoveryReject/Standby-Semantik
  run_persistence_contract.*            nur notwendige Legacy-/R1-Klassifikation
  run_persistence_codec.*               kein neues Schema, sichere Altdecodierung
  run_persistence_coordinator.*         explicit resume / NoActiveRun / indeterminate
  run_recovery*.*/run_progress_*.       C1-Abschaltung; C2 bleibt Legacy/Deprecated
  run_commands.*                        keine caller-supplied Safetyfreigabe
  actuator_plan_types.*                 SafetyView/Gate nur falls bestehender Typ es erfordert
  actuator_planner.* / sink driver       zentrale Endgrenze und Stop-Orakel
  fermentation_application.*            Boot-Reihenfolge und Composition-Verbindung
src/main.cpp, main/app_main.cpp         nur Verdrahtung und sichere Startausgabe
test/...                                 gezielte native Vertrags-/Integrationsnachweise
docs/...                                 in Abschnitt 10 definierte Vertragskorrekturen
```

Die genaue Dateiliste des Umsetzungsschnitts wird vor jedem Codecommit gegen
den dann aktuellen Branch geprüft. Neue Dateien sind nur zulässig, wenn der
konkrete Vertrag in diesem Plan nicht sauber durch einen vorhandenen
kanonischen Typ getragen werden kann. Die Existenz eines C2-Legacy-Bausteins
ist allein kein Löschmandat für #24.

## 9. Explizite Nicht-Ziele

Release 1 dieses Plans enthält nicht:

- automatische transparente oder verlustfreie Charge-Recovery nach Brownout,
  Watchdog, Crash oder Stromausfall;
- Zeitintervallberechnung, gewichteten Fortschritt, Temperatur-Evidenz-
  Carry-Forward oder Recovery-Lineage zur Rettung eines alten Runs;
- neue Run-Persistenzschema-Version, neue Run-Keys, Fallback-Promotion,
  Rollback oder besondere Recovery-Schreibberechtigungen;
- ein zusätzlicher allgemeiner Safety-Record, Restart-Zähler, Resetzeitfenster
  oder automatischer Restart-Latch;
- OTA, Firmwaredownload, Netzwerk-, Web-, Anzeige- oder Remote-Safetylogik;
- neue Hardware-, GPIO-, Pegel-, Sensor-, Grenzwert- oder
  Inbetriebnahmeentscheidungen;
- Änderung der #20-Sensorqualität, #21-Sensorwahl, #22-PI-Regelung oder
  #23-Mindestzeiten/Totzeit als fachliche Nebenbaustelle;
- direkte Aktor-/H-Brücken-/GPIO-Zugriffe aus `fermentation_app`;
- Quittierung, UI-Autorisierung oder ein `allowed`-Requestfeld als
  Freigabequelle;
- eine allgemeine Fault-Historie, automatische Fault-Lineage, ein zweiter
  Eventbus oder eine zweite Safety-/Persistenzautorität;
- vollständige Hardwareverifikation im nativen Plan-PR. Reale
  Aktor-/Reset-/Brownout-/Watchdog-Nachweise bleiben Integrationsgates.

Spätere Komfortfunktionen dürfen einen ausdrücklich beweisbaren Resume-Flow
mit mehr Nutzerinformation ergänzen, aber nicht die R1-Invarianten aufweichen.
Zeitgewichtete Recovery, automatische Charge-Rettung oder verbesserte
Historienführung benötigen ein eigenes Owner-Issue und einen eigenen Plan.

## 10. Kanonische Dokumentkorrekturen

Die Umsetzung muss die folgenden Dokumente im selben fachlichen Änderungs-
umfang konsistent machen. Die Korrekturen werden erst nach Ownerfreigabe des
Plans implementiert; dieser Plan ist die Arbeitsliste, kein stiller Ersatz
für die kanonischen Dokumente.

| Dokument | Widerspruch/Alt-Orakel | Geplante Korrektur |
|---|---|---|
| `docs/SAFETY_AND_FAULTS.md` | Vier Klassen und historische Latch-/Recovery-Verallgemeinerung | Drei minimale Reaktionsdispositionen, stabile Codes, unmittelbares AUS, Ack getrennt von Freigabe, automatische Wiederfreigabe nur pro Code; unknown/critical bleibt gesperrt. |
| `docs/SAFETY_COMPONENT_FAULTS.md` | Teilweise Klassen-3/2-Orakel und automatische Reaktion ohne klare Boot-/Restore-Grenze | Sensor-/Aktorreaktionen gegen #20/#21/#23 beibehalten, aber Gate-/Recheck-Voraussetzungen und die R1-Ausnahme „kein Auto-Release durch Boot/Restore“ explizit machen. |
| `docs/SYSTEM_SAFETY_AND_RECOVERY.md` | Automatische Wiederaufnahme, wiederholte Recovery und Charge-Erhalt als Leitbild | Boot all-off, Resetcause/SAFE_BOOT ohne Restart-Latch, explizites Resume-Angebot, sichere `NoActiveRun`-Beendigung und kein transparenter Brownout-/Crash-Erhalt. |
| `docs/STATE_MACHINE.md` | `RecoveryEvaluation`/`RecoveryReject` begünstigen Recovery-Fortsetzung bzw. Fault statt sicherem Abbruch | `RecoveryEvaluation` als nicht freigebendes Angebot; `RecoveryReject`/`RecoveryRejected` führt nach bestätigtem `NoActiveRun`-Write zu `Standby`, Persistenzunknown nicht zu normalem Standby. |
| `docs/RUN_PERSISTENCE.md` | Schema-3-Recoveryfelder, automatisch aktivierende Reihenfolge und Fallback-Rettung | Exaktes 5.2-Prädikat dokumentieren: Schema 3 und neutrale Felder sind zulässig; nur offene Recovery-Evidenz blockiert einfachen R1-Resume. `NoActiveRun`-Write-before-Apply und C1/C2-Grenze festhalten. |
| `docs/RECOVERY_AND_INTERRUPTION.md` | Ausfallintervall, NTP-/Zeitbewertung und gewichteter Fortschritt als R1-Orakel | C1-Aufrufe und automatische Gutschriften aus dem aktiven Produktpfad entfernen; verbleibende C2-Kompatibilität als Legacy/Deprecated kennzeichnen; nur vollständige/eindeutige Qualifikation und explizite Entscheidung behalten. |
| `docs/ACCEPTANCE_TESTS.md` | Automatische Recovery-, Zeit- und gewichtete-Progress-Orakel | Veraltete Orakel gezielt korrigieren; die vollständige Pflichtmatrix aus Abschnitt 11 dieses Plans ergänzen. |
| `docs/SPECIFICATION_REVIEW.md` | „phasenbezogener sicherer Wiederanlauf“ kann automatische Aktorfreigabe nahelegen | Release-1-Scope auf allgemeine Persistenz plus explizites, nicht automatisch freigebendes Resume-Angebot präzisieren. |
| `docs/REQUIREMENTS.md` | Vier Klassen und automatische Recovery ohne Owner-Gate | Minimalreaktionen, SafeBoot, NoActiveRun-Abbruch und klare Gate-Reihenfolge normieren. |
| `docs/ARCHITECTURE.md` | Safety-Core-Beschreibung enthält historische vier Klassen und Recovery-Komplexität | Eine Safety-Autorität, zentrale Planner-Grenze, Platform-Trennung und die F1-Entscheidung „kein neuer allgemeiner Safety-Record“ dokumentieren. |
| `docs/CONFIGURATION_PERSISTENCE.md` | Der #24-Anschluss ist korrekt, aber die neue Run-Abgrenzung fehlt | `CONFIGURATION_SAFETY_INTEGRATION_GATE` unverändert erhalten und explizit auf Boot/Restore/no-normal-release beziehen. |
| `docs/IMPLEMENTATION_ISSUES.md` | #24-Abschlussbeschreibung kann alte Recoveryannahmen enthalten | Abhängigkeit #56/#57 -> #24 und #20 -> #21 -> #22 -> #23 -> #24 mit Scope-Reset aktualisieren; keine neue Zyklusabhängigkeit. |
| `docs/ROADMAP.md` | Status nennt PR #110, aber den neuen Owner Scope Reset noch nicht | Draft-/NOT_STARTED-Status, superseded alte Planrevision und Owner-Gate für die neue exakte Plan-SHA sichtbar halten. |
| `docs/DECISIONS.md` / ADR-018 | Muss gegen Config- und Persistenzänderung geprüft werden | Keine inhaltliche Abschwächung; insbesondere keine neue Restart-/Safety-Persistenz aus #24. Nur falls die Umsetzung eine echte ADR-Abweichung findet, Ownerentscheidung vor Code. |
| `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md` | Darf durch Reset-/Safety-Port nicht abgeschwächt werden | Unverändert verbindlich halten; neuen Port nur app-neutral und native-testbar ergänzen. |

Doppelte Verträge werden vermieden: Fehlercodes und Safety-Disposition gehören
zum SafetyCore, Sensorqualität bleibt #20/#21, Config-Produzenten bleiben #56/
#57, Aktorfreigabe bleibt der vorhandene Planner-/Sink-Pfad und Persistenz-
Integrität bleibt bei den bestehenden Store-/Codec-Verträgen.

## 11. Gezielte Teststrategie nach Ownerfreigabe

Es werden keine Testfälle als Implementationsersatz vorweggenommen. Nach
Freigabe müssen die bestehenden gezielten Suites erweitert und, wo
fachlich veraltet, korrigiert werden. Der Nachweis muss Anwendungsergebnis,
Persistenzresultat und Aktorsink-Verhalten zusammen betrachten; ein
interner Flag allein ist kein PASS.

| Pflichtfall | Erwartetes Orakel | Primäre Testfläche |
|---:|---|---|
| 1. gültige Konfiguration, kein aktiver Lauf | Boot endet in sicherem Standby; kein Actor-Command vor explizitem Start | Boot/Application + Run-Persistence + Sink |
| 2. vollständig/eindeutig rekonstruierbarer aktiver Lauf | Resume-Angebot vorhanden; Restore allein erzeugt kein `Allowed` und keinen Aktor-Command | Boot/Run-Persistence + Process FSM + Sink |
| 3. unvollständiger, widersprüchlicher oder nicht eindeutiger Lauf | Aktoren AUS; zuerst `NoActiveRun` mit bestätigtem Readback; erst danach `RecoveryRejected`/`Standby`; bei unklarem Write kein normaler Standby und unknown-safe | Persistence + FSM + Store fault injection |
| 4. kritischer/unknown Safetyzustand | `ImmediateStop`/`SAFE_BOOT`, keine normale Freigabe; kein neuer Safety-Record wird vorausgesetzt | SafetyCore + Planner + Boot |
| 5. Sensorfehler | sensorrollenspezifisch AUS/failover gemäß #20/#21; keine Freigabe aus stale/failed Daten | Sensor Quality/Selection + Planner |
| 6. Aktor-/Plannerfehler | unmittelbares AUS, latched Verhalten gemäß Code, kein zweiter Bypass-Pfad | Planner + sink + SafetyCore |
| 7. Persistenz-/Readbackfehler | kein neuer Zustand behauptet; AUS; safe abort oder SafeBoot je nach betroffener Persistenz | Store/codec/coordinator |
| 8. unbestimmter Commitzustand | `CommitOutcomeUnknown` bleibt unbekannt; keine Slot-Wiederverwendung, kein Run-/Config-Release | Simulated Store + Run/Config service |
| 9. Brownout | Resetcause erkannt/unknown-safe; Boot all-off; kein transparenter aktiver Resume | Reset mock + application + sink |
| 10. Power-on/externer Reset/Brownout sowie Watchdog/Panic/unknown | jede Ursache startet all-off; vollständige Revalidierung; kein automatischer Resume; untrusted System-/Config-/Persistence-Zustand `SAFE_BOOT`; kein Zähler und kein Zeitfenster | Resetcause-Port + Application + ESP-IDF adapter contract |
| 11. `ConfigurationRuntimeFailure` | Config-Gate blockiert normale Aktoren und erzeugt passende SafeBoot/latched Reaktion | Config recovery + SafetyCore + sink |
| 12. `ConfigurationUnavailable` | SafeBoot/ImmediateStop, keine Factory-/Defaultfreigabe als Laufstart | Config recovery + boot |
| 13. `ConfigurationIntegrityFailure` | SafeBoot/ImmediateStop, keine Quittierungsfreigabe | Config recovery + boot |
| 14. nicht auflösbarer `CommitOutcomeUnknown` | weder alte noch neue Config behaupten, keine Publish-/Aktorfreigabe | Config service + simulated store |
| 15. reale Aktoradapter | Safety-Gate kann den finalen Idle-/Stop-Pfad nicht umgehen; unbekannte/malformed Gatewerte bleiben AUS | Sink driver, ESP-IDF adapter integration gate |

Zusätzliche gezielte Negativnachweise:

- Boot mit gültigem Schema-3-Run, dessen `recoveryTemperatureEvidence` und
  `RunProgressState` nur neutrale Defaults/normalen `KnownTotal`-Bestand
  enthalten: der Run wird nicht allein wegen Feldexistenz verworfen;
- Boot mit gültigem Run und nichtneutralem Last-known-
  `recoveryTemperatureEvidence` allein: Diagnose bleibt möglich, aber es gibt
  keinen Resume-/Zeitbeweis und keinen automatischen Aktorstart;
- Boot mit jedem offenen Marker aus 5.2 (`pendingRecoveryAnchor`,
  `recoveryBootAnchorMonotonicMillis`, Episode-/Prior-/Adjustment-Evidenz oder
  gewichteter/partiell unbekannter Progress): kein einfacher Resume, keine
  Zeit-/Progressgutschrift, sicherer `NoActiveRun`-Pfad;
- `schemaVersion == 3` mit vollständig eindeutiger aktueller Run-Struktur:
  Schema 3 selbst führt nicht zu `not reconstructable`;
- beschädigter Current-Record mit mehrdeutigem Fallback: keine Promotion,
  kein Rollback-Resume, sicherer Abschluss oder SafeBoot;
- Resume-Bestätigung: aktueller Sensor-/Config-/Safety-Check, persistenter
  Commit mit exactem Readback, erst danach `RecoveryResumed`/FSM-Aktivierung;
- Reject/Timeout/unklare Rekonstruktion: zuerst `NoActiveRun`-Commit und
  Readback, erst danach `RecoveryRejected` nach `Standby`; jeder Write-/Readback-
  Fehler lässt RAM in Recovery/unknown-safe und nicht in normalem Standby;
- Quittierung vor und nach jedem kritischen Fault ändert nicht das Gate;
- ein erlaubter Sensor-Recheck kann nur mit frischer kanonischer Evidenz und
  ohne aktiven Latch wieder freigeben;
- öffentliche Start-/Stop-/Completion-/Adjust-/Sensor-Requests können keine
  Safety-Boolean-Felder als Freigabe einschleusen;
- Application-/Persistence-Tests prüfen die realen Sink-Kommandos, nicht nur
  SafetyCore-RAM-Zustände.

Die vorhandenen Tests für #17, #20, #21, #22, #23, Config-Recovery,
ActuatorPlanner, Sink und Simulated Store bleiben die Regressionbasis. Tests
für automatische Recoveryzeit, gewichteten Fortschritt und alte
Fallback-Rettung werden nur für C1 aus dem aktiven Testvertrag entfernt oder
als negative R1-Orakel umgeschrieben; verbleibende C2-Tests werden als
Legacy/Deprecated gekennzeichnet und nicht pauschal im selben PR gelöscht.
Hardware-Brownout, echter Watchdog, reale Resetcause und physische
Aktorabschaltung sind keine nativen PASS-Nachweise.

## 12. Umsetzungs- und Commitstruktur nach Ownerfreigabe

Keiner dieser Umsetzungsschnitte darf vor Freigabe der exakten neuen
Plan-SHA begonnen werden.

1. **Safety-Verträge und flüchtige Boot-Evidenz**
   - stabile Fault-Codes/Dispositionen, SafetyView und einzige SafetyCore-
     Autorität;
   - app-neutraler Resetcause-Port nur für Ursache/Diagnose, ohne
     Restart-Zähler oder Safety-Persistenz;
   - `Unresolved` als Boot-Default und fail-closed Write-/Readback-Semantik.

2. **Boot-/Config-/Reset-Integration**
   - sichere Initialausgabe vor Validierung;
   - reale #56/#57-Producer-Gates und Config-Indeterminate-Abbruch;
   - Resetcause-/Watchdog-Auswertung und SafeBoot ohne Restartfenster-/Latch-
     Vertrag;
   - ESP-IDF-Adapter sowie native Test-Injektion gemäß ADR-013.

3. **Vereinfachung der Run-Persistenz-/FSM-Nutzung**
   - vollständige/eindeutige Read-only-Qualifikation;
   - explizites Resume-Angebot ohne Boot-Release;
   - sichere `NoActiveRun`-Beendigung nicht rekonstruierbarer Runs;
   - `CommitOutcomeUnknown` und Readbackfehler als unknown-safe;
   - C1-Abschaltung aller aktiven Write-/Zeit-/Progress-/Promotionpfade;
   - C2 als Legacy/Deprecated abgrenzen, nicht pauschal löschen, und
     separaten Follow-up-Cleanup empfehlen.

4. **Aktorpfade und Fehlerreaktionen**
   - SafetyCore als einzige Quelle für die bestehende Planner-Gate-Eingabe;
   - route-spezifische Allow-/Deny-Integration für Start, Resume, Sensor-,
     Planner- und reale Sinkpfade;
   - Quittierung, Recheck und vorhandene Store-/Config-Sperrsemantik sauber
     trennen; keinen neuen Safety-Record einführen.

5. **Gezielte Regression und Dokumentkonsistenz**
   - die Matrix aus Abschnitt 11 gegen Anwendung, Persistenz und Sink;
   - obsolete Recovery-Orakel entfernen/korrigieren;
   - Dokumente aus Abschnitt 10 synchronisieren;
   - vollständiger aktueller Diff gegen diesen freigegebenen Plan vor dem
     späteren Review.

Jeder Schnitt erhält einen eigenen fachlich begrenzten Commit. Bei einer
materiellen Abweichung wird zuerst dieser Plan aktualisiert, neu committed und
erneut vom Owner freigegeben. Kein Umsetzungsschnitt wird in den Plan-Commit
gemischt.

## 13. Hardware-, Integrations- und spätere Gates

Der Plan entscheidet keine Hardwaredetails. Offen bleiben insbesondere:

- reale GPIO-/Pegel-/Treiber-/Verdrahtungsbestätigung für alle Aktoren;
- Nachweis der sicheren physischen Ausgabestellung bei Power-On, Reset,
  Brownout und ESP-IDF-Adapterstart;
- echte Watchdog-/Resetcause-Nachweise auf Zielhardware; native Einzelursachen-
  Injektion ersetzt keinen Hardwarebeweis und begründet keine Zählung;
- Sensor-/Aktor-Inbetriebnahme, thermische Parameter und Belastungsgrenzen;
- #56/#57- und das separate produktive #106-Integrationsgate;
- Speicher-/Zeitbudgets für den SafetyCore und den bestehenden Store-/Readback-
  Pfad;
- reale Verifikation, dass kein Adapterpfad das Planner-Gate umgehen kann.

Diese Nachweise sind nach dem Code-Review gesonderte Owner-/Hardware-Gates.
Native Builds und Tests beweisen weder die reale Aktorabschaltung noch
Brownout-/Watchdog-Hardwareverhalten.

## 14. Abschlussgates für diese Planrevision

Für diesen Auftrag gelten ausschließlich leichte Plan-/Markdown-Gates:

- Plan vollständig eigenständig, ohne normative Rückverweisung auf alte
  Revisionen;
- Base, PR-Status, Branch, Issue und fehlende Implementation live verifiziert;
- nur Plan und notwendige Roadmap-/Planstatus-Dokumentation geändert;
- `git diff --check` ohne Befund;
- Markdown-/Link-/Strukturprüfung der geänderten Dokumente;
- bestehender PR #110 bleibt Draft;
- PR-Body wird nach dem Plancommit auf den neuen Status, Planpfad und die
  exakte Plan-SHA synchronisiert;
- genau ein aktueller `SESSION HANDOVER`-Kommentar wird nach der
  Synchronisierung veröffentlicht.

Nicht Teil dieses Auftrags und daher nicht als PASS auszugeben sind Firmware-
Builds, native Firmwaretests, clang-tidy, Hardwaretests, echte
ESP-IDF-Reset-/Brownout-/Watchdogtests, `Ready for review`, Merge oder
Issue-Schließung. `Implementation: NOT_STARTED` bleibt bestehen.

Der neue Plan wartet nach seinem Commit auf die ausdrückliche Ownerfreigabe
genau dieser vollständigen Plan-SHA.
