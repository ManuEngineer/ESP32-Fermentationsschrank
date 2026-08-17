# [E3.5] Issue #24 – Safety Core nach dem Owner Scope Reset

## 1. Planstatus und verifizierte Ausgangslage

Dieser Plan ersetzt den bisherigen Plan in PR #110 vollständig. Er ist eine
eigenständige, umsetzbare Planrevision und keine Ergänzung einer historischen
Revision. Der bisherige Plan und sein Commit
`58d9accb44da528701bdf78e4cc6382f38b6173a` sind
`SUPERSEDED / NOT APPROVED`. Vor einer Umsetzung ist die exakte neue
Plan-Commit-SHA ausdrücklich durch den Owner freizugeben.

```text
Repository: ManuEngineer/ESP32-Fermentationsschrank
Issue: #24 – [E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion
PR: #110 – [E3.5] Issue #24 safety core replan from main
PR-Branch: agent/issue-24-safety-core-replan-v2
Base: main
Verifizierte origin/main-SHA: b8eae5f4da5f2666b5a9bda333d115254c4db5b2
PR-HEAD vor dieser Planrevision: 58d9accb44da528701bdf78e4cc6382f38b6173a
Issue-/PR-Stand verifiziert: 2026-08-17
Planpfad: docs/tasks/issue-24-safety-core-replan.md
Implementation: NOT_STARTED
```

Live verifiziert wurden Repository, Branch, `origin/main`, PR #110 und Issue
#24. PR #110 ist offen und Draft. Issue #24 ist offen und enthält den
Owner-Status `REPLAN_REQUIRED_OWNER_SCOPE_RESET`. PR #107, #108 und #109
sind historische, superseded Referenzen und werden nicht geändert oder als
normative Quelle verwendet. `origin/main` ist gegenüber der bisherigen
PR-Basis der verbindliche Prüfstand; ein weiterer Main-Lauf wurde für diese
Revision nicht angenommen.

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
| Safety/Fault | Es gibt noch keinen zentralen Issue-24-Fault-/Safety-Service. FSM, Planner und Dokumente enthalten jedoch SafeBoot-, Fault-, Watchdog- und Gate-Projektionen. | Es wird genau eine kleine mutable Safety-Autorität in `fermentation_app` benötigt. Sie aggregiert Ursachen und erzeugt die bestehende `ActuatorSafetyGateInput`-Projektion. |
| SAFE_BOOT / Resetcause / Watchdog | `ProcessState::SafeBoot` und der Aktor-Request-Watchdog existieren. Ein kanonischer Resetcause-Port und eine ESP-IDF-Adaptergrenze für wiederholte abnormale Neustarts existieren noch nicht. | Ein app-neutraler Resetcause-/Abnormal-Restart-Port wird gemäß ADR-013 ergänzt. ESP-IDF bleibt ausschließlich Adapter; konkrete Pins, Pegel und Hardwareverhalten werden nicht geraten. |
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
- `SAFE_BOOT`, persistente Sperren und Reset-/Watchdog-Bewertung;
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
| `LatchedOrSafeBoot` | Aktoren AUS, persistente Sperre oder `SAFE_BOOT`, je nach Ursache | nur über die für den Code definierte, fachlich zulässige Behebung und vollständige Neubewertung; unbekannt bleibt gesperrt |

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

### 4.3 Minimal notwendige Safety-Persistenz

Eine kleine Safety-Persistenz ist nur für zwei konkrete Anforderungen nötig:

1. Eine nach Neustart weiter geltende kritische Sperre darf nicht durch einen
   Neustart verschwinden und dadurch Aktoren freigeben.
2. Wiederholte abnormale Neustarts müssen begrenzt und bei Überschreitung
   fail-closed nach `SAFE_BOOT` geführt werden können.

Dafür wird ein einzelner, begrenzter Safety-Zustand über die bestehende
   generische `IStateStore`-/Envelope-/Readback-Grundlage geplant. Er enthält
   nur versionierte, CRC-/Integritätsgeprüfte Safety-Sperr-/Resetdaten und
   keinen Run-Snapshot, keine Recovery-Lineage, keine Aktorhistorie und keine
   neue Run-Persistenzschema-Version. Die konkrete Feldbreite und die
   Resetfenster-Grenzen werden aus den vorhandenen Plattform-/Safety-Verträgen
   abgeleitet und nicht aus Hardwareannahmen geraten.

Ein Read-, Integritäts- oder Readback-Fehler dieses Safety-Zustands ist selbst
`unknown` und führt zu `SAFE_BOOT` bzw. `ImmediateStop`. Ein unbestimmter
Safety-Write wird nicht durch den alten Wert, einen neuen Wert oder eine
volatile Freigabe ersetzt. Safety-Persistenz ist damit eine kleine
Fail-Closed-Sperre, nicht ein zweiter Recovery-Store.

### 4.4 Resetcause und Watchdog

`fermentation_app` erhält nur einen app-neutralen Resetcause-/Abnormal-
Restart-Port. Der Port liefert eine endliche, bekannte Ursache oder
`Unknown`; er kennt weder ESP-IDF-Typen noch GPIOs. Der ESP-IDF-Adapter mappt
die öffentlichen ESP-IDF-Reset- und Watchdog-Verträge. Native Tests injizieren
bekannte, unbekannte und wiederholte Ursachen.

Bei jedem Boot gilt:

1. Aktoren hart auf den sicheren Aus-Zustand bringen, bevor fachliche
   Validierung und Restore beginnen.
2. Resetcause und Safety-Zustand lesen. Unbekannte Ursache, abnormale
   Neustartfolge außerhalb des zulässigen Fensters oder unklare Persistenz
   führen zu `SAFE_BOOT`/`Unresolved`.
3. Konfiguration und ihre Producer-Gates prüfen.
4. Run-Persistenz nur lesen und klassifizieren.
5. Erst nach vollständiger Validierung einen expliziten Standby-,
   Resume-Angebots- oder SafeBoot-Zustand darstellen. Kein Boot-Schritt
   erzeugt allein `Allowed`.

Der bereits vorhandene Aktor-Request-Watchdog und seine latched Stop-Reaktion
bleiben Bestandteil des Planner-Vertrags. #24 ergänzt Resetcause-/Boot-
Bewertung, ersetzt aber nicht den #23-Watchdog und erfindet keine
Hardware-Watchdogparameter.

## 5. Boot-, Restore- und Run-Vertrag

### 5.1 Zustandsablauf

| Eingang | Klassifikation | Persistenz-/FSM-Ausgang | Aktor-Gate |
|---|---|---|---|
| gültige Konfiguration, kein aktiver Lauf | sicherer leerer Start | `Boot -> Standby`, kanonisch kein aktiver Lauf | `Unresolved` während Boot, danach weiterhin `ImmediateStop`/`Allowed` nur bei explizitem neuen Start und gültigem Live-Gate |
| gültige Konfiguration, aktiver Lauf vollständig und eindeutig | Resume-Angebot | `Boot -> RecoveryEvaluation` oder äquivalente explizite Angebotsprojektion; keine automatische Weiterführung | AUS; `Allowed` erst nach expliziter Resume-Entscheidung, aktueller Safety-/Sensorprüfung und normalem Planner-Gate |
| aktiver Lauf unvollständig, widersprüchlich, beschädigt, mehrdeutig oder nicht beweisbar | sicherer Abbruch | alter Lauf sicher beenden/verwerfen und kanonisches `NoActiveRun` schreiben; danach `Standby`, wenn dieser Commit eindeutig erfolgreich ist | AUS; kein Fault-Resume |
| NoActiveRun-Abschluss nicht sicher schreibbar oder Ergebnis unbestimmt | unbekannter kritischer Persistenzzustand | Safety-Latch/`SAFE_BOOT`; kein normaler Standby als Ausweichbehauptung | `ImmediateStop` |
| Konfiguration/Safety unbekannt oder ungültig | kritischer/unknown Zustand | `SAFE_BOOT` oder latched Safety-Zustand; aktiver Lauf wird nicht gerettet | `ImmediateStop` |
| explizit bestätigter Resume oder neuer Lauf | neuer Fachbefehl | persistierte Fachentscheidung und regulärer Live-Startpfad | erst nach Sensor-, Konfigurations-, Planner- und Safety-Gate `Allowed` |

`RecoveryEvaluation` bedeutet nur „vollständige Eindeutigkeit wurde
festgestellt, eine explizite fachliche Entscheidung steht aus“. Es bedeutet
nicht „Aktorfreigabe vorbereiten“. Ein Ablehnen, Ablauf oder Nichtbestätigen
des Angebots beendet den alten Lauf sicher in `NoActiveRun`, sofern der
kanonische Write eindeutig gelingt; es erzeugt nicht automatisch einen
latched Safety-Fault.

### 5.2 Kriterium „vollständig und eindeutig rekonstruierbar“

Die spätere Implementierung verwendet ausschließlich bereits kanonische Daten
aus dem vorhandenen #17-/Prozess-/Sensor-/Konfigurationsvertrag. Alle
folgenden Punkte müssen gleichzeitig erfüllt sein:

- Head, Slot, Recordtyp, CRC, Schema und Revision sind gültig und eindeutig
  zueinander referenziert;
- die Snapshot-Variante ist ein aktiver, nicht terminaler Run und enthält die
  für den aktuellen Prozesszustand erforderlichen Felder;
- Programm-/Run-Kontext, Prozesszustand, Revision, Sensorrollen und die
  erforderlichen #20/#21-Auswahl-/Qualitätsdaten sind vorhanden und
  widerspruchsfrei;
- keine erforderliche Information ist nur aus einer unbestimmten
  Ausfallzeit, einem gewichteten Fortschrittswert, einem beschädigten
  Fallback oder einer nicht verifizierten Annahme ableitbar;
- der Datensatz ist mit dem aktuellen, gültigen Konfigurationsgraphen und den
  zulässigen Release-1-Verträgen vereinbar;
- jeder für die explizite Resume-Entscheidung erforderliche Readback ist
  erfolgreich und eindeutig.

Fehlt ein Punkt oder liefern Current und Fallback mehrere nicht eindeutig
auflösbare Möglichkeiten, ist das Ergebnis „nicht rekonstruierbar“. Es wird
nicht durch Wahrscheinlichkeiten, Zeitgewichtung, historische Lineage oder
eine neue Evidence-Struktur ergänzt.

### 5.3 Persistenzfehler und `CommitOutcomeUnknown`

Die vorhandenen `IStateStore`- und `RunPersistenceStore`-Semantiken bleiben
maßgeblich:

- `WriteError` und `CapacityError`: keine neue fachliche Wahrheit behaupten;
  Safety bleibt AUS/gesperrt.
- `CommitOutcomeUnknown`: kein „alt“ und kein „neu“ behaupten, keinen Slot
  wiederverwenden und keine Aktorfreigabe geben; Safety-Latch/`SAFE_BOOT` bis
  zu einem zulässigen, explizit nachgewiesenen Zustand.
- erfolgreicher Write ohne erforderlichen Readback: nicht als erfolgreich
  behandeln.
- erfolgreicher `NoActiveRun`-Write mit eindeutigem Readback: alter Run ist
  beendet; ein neuer Run darf nach normaler Konfigurations-/Safety-Prüfung
  angeboten werden.

Die Entscheidung bleibt fail-closed, ohne einen zusätzlichen Run-
Recovery-Evidence- oder Capability-Vertrag.

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
| Resetcause-/Abnormal-Restart-Evidenz und persistente Safety-Sperre | Wiederholte abnormale Neustarts und kritische Sperren müssen Neustarts überstehen. | Einen kleinen app-neutralen Port plus eine begrenzte Safety-Record-Nutzung planen; keine Run-Schema-Erweiterung. |

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

### C – nur für die alte komplexe automatische Laufrettung

| Mechanismus | Befund | Release-1-Entscheidung |
|---|---|---|
| `S3RunRecovery`, `S3RecoveryExclusiveMode`, `RecoveryWriteCapability` | Nur historische Konstrukte des alten PR-#110-Plans; nicht auf `origin/main` implementiert. | Nicht implementieren. Keine historische Capability-, Handoff- oder Lineage-Semantik übernehmen. |
| Zusätzliche `RunPersistenceRecoveryEvidence`, historische Recovery-Lineage und spezielle Recovery-Schreibrechte | Nur zur Begründung/Absicherung einer weitergehenden alten Charge-Rettung; im aktuellen Main nicht als #24-Vertrag vorhanden. | Nicht implementieren. Bestehende normale Readback-/Revision-Verträge genügen für „eindeutig“ versus „nicht rekonstruierbar“. |
| Fallback-Promotion, Rollback und `FallbackRecoveryPending` als automatische Rettung eines alten aktiven Runs | Der generische Fallback-Scan bleibt nützlich, die automatische Promotion/der Rückrollpfad würde aber gerade eine alte unterbrochene Charge retten. | Keine automatische Promotion/Rollback für aktives Run-Recovery. Ein unabhängig vollständiger Fallback darf höchstens read-only qualifiziert werden; bei erforderlicher Reparatur oder Mehrdeutigkeit sicher `NoActiveRun`/`SAFE_BOOT`. |
| `PendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis`, `RecoveryTemperatureEvidence`, `lastRecoveryEpisodeEvidence`, `priorBootPhaseElapsed`, `nominalRecoveryAdjustment`, `recoveryEpisodeRevision` | Bereits auf `main`, aber ihre Semantik dient der Zeit-/Episode-/Carry-Forward-Rettung über Neustarts. | Keine neuen Writes oder aktiven R1-Entscheidungen daraus. Alte Schema-3-Daten werden, falls erforderlich, decoderseitig erkannt und als nicht eindeutig klassifiziert; keine Zeit-/Progress-Gutschrift. |
| `run_recovery_time.*`, `RunRecoveryCoordinator::reevaluateRecoveryTime` und zugehörige Verdict-/Ausfallintervallmodelle | Berechnen gerade die nicht mehr geschuldete transparente Unterbrechungszeit. | Aktive Pfade und APIs gezielt entfernen oder auf reine Ungültigkeitsklassifikation reduzieren; keine neue Ersatzzeitrechnung. |
| `run_progress_weighting.*`, `RecoveryProgressWeightingModel`, gewichtete Coverage/Provenienz und `applyRecoveryProgressWeighting` | Dient der automatischen Fortschrittsrettung und ist für R1 nicht erforderlich. | Aktive Produzenten, Mutatoren und Recovery-Orakel entfernen. Ein alter optionaler Wert bleibt nur als „nicht für Resume verwendbar“ lesbar, falls Decoderkompatibilität nötig ist. |
| `RunPersistenceCoordinator::activateFallbackRecoveredRun`, `resolveRecoveryOutcome` und ähnliche Recovery-Write-APIs | Erlauben Fallback-/Resume-/Episode-Schreibentscheidungen für den alten Lauf. | Für R1 auf explizite Qualifikation, explizite Resume-Entscheidung und sicheren `NoActiveRun`-Abschluss reduzieren; automatische Aktivierung, Promotion und Rollback entfallen. |
| Zeitintervall-, NTP-/UTC-Abwesenheits-, gewichtete-Fortschritts- und transparente-Reboot-Orakel aus #18 | Testen Komfort-/Rettungsverhalten statt Safety-Anforderung. | Gezielte Tests entfernen oder in negative „keine automatische Rettung“-/Legacy-Klassifikationstests umwandeln. |

Die C-Entscheidung ist eine gezielte Vereinfachung, keine pauschale Löschung:
Der vorhandene Schema-3-Decoder darf Legacy-Daten erkennen, damit ein alter
Datensatz nicht versehentlich als gültiger Resume behandelt wird. Er darf
diese Daten aber nicht mehr zur normalen Aktorfreigabe, Zeitgutschrift,
Fallback-Promotion oder automatischen Recovery verwenden. Nach der
Abhängigkeitsprüfung werden nicht mehr benötigte aktive C-Dateien und Tests in
einem eigenen Umsetzungsschnitt entfernt; der sichere Legacy-Decode bleibt
nur so lange bestehen, wie er für einen eindeutigen fail-closed Übergang zu
`NoActiveRun` erforderlich ist.

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
  Resetcause-/Restart-Evidence-Port hinzu. Keine Fault-Codes, Run-Modelle,
  GPIOs, ESP-IDF-Enums oder Safety-Entscheidungen.
- `lib/device_platform_esp_idf`: ausschließlich Mapping der öffentlichen
  ESP-IDF-Reset-/Watchdog-Verträge auf den abstrakten Port und die sichere
  Initialausgabe. Keine Fachlogik und kein Run-Recovery.
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
lib/device_platform/src/              Resetcause-/Restart-Port, falls Audit bestätigt
lib/device_platform_esp_idf/src/      öffentlicher ESP-IDF-Adapter
lib/device_platform_test_support/src/ deterministische Reset-/Store-Injektion
lib/fermentation_app/src/             SafetyCore, Boot-/Run-/Config-Verknüpfung
  process_state_machine.*              RecoveryReject/Standby-Semantik
  run_persistence_contract.*            nur notwendige Legacy-/R1-Klassifikation
  run_persistence_codec.*               kein neues Schema, sichere Altdecodierung
  run_persistence_coordinator.*         explicit resume / NoActiveRun / indeterminate
  run_recovery*.*/run_progress_*.       C-Abbau bzw. Legacy-Decode-Abgrenzung
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
kanonischen Typ getragen werden kann.

## 9. Explizite Nicht-Ziele

Release 1 dieses Plans enthält nicht:

- automatische transparente oder verlustfreie Charge-Recovery nach Brownout,
  Watchdog, Crash oder Stromausfall;
- Zeitintervallberechnung, gewichteten Fortschritt, Temperatur-Evidenz-
  Carry-Forward oder Recovery-Lineage zur Rettung eines alten Runs;
- neue Run-Persistenzschema-Version, neue Run-Keys, Fallback-Promotion,
  Rollback oder besondere Recovery-Schreibberechtigungen;
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
| `docs/SYSTEM_SAFETY_AND_RECOVERY.md` | Automatische Wiederaufnahme, wiederholte Recovery und Charge-Erhalt als Leitbild | Boot all-off, Resetcause/SAFE_BOOT, explizites Resume-Angebot, sichere `NoActiveRun`-Beendigung und kein transparenter Brownout-/Crash-Erhalt. |
| `docs/STATE_MACHINE.md` | `RecoveryEvaluation`/`RecoveryReject` begünstigen Recovery-Fortsetzung bzw. Fault statt sicherem Abbruch | `RecoveryEvaluation` als nicht freigebendes Angebot; Reject/Timeout/unklare Rekonstruktion führt bei erfolgreichem Abschluss zu `NoActiveRun`/Standby, Persistenzunknown zu SafeBoot. |
| `docs/RUN_PERSISTENCE.md` | Schema-3-Recoveryfelder, automatisch aktivierende Reihenfolge und Fallback-Rettung | #17-Kern und Schema-3-Legacy-Decode klar von R1-Resume trennen; C-Felder nicht schreiben/interpretieren, unklare Snapshots sicher beenden, kein Boot-Release. |
| `docs/RECOVERY_AND_INTERRUPTION.md` | Ausfallintervall, NTP-/Zeitbewertung und gewichteter Fortschritt als R1-Orakel | Alte Rettungsorakel entfernen oder als nicht normative historische Kompatibilität kennzeichnen; nur vollständige/eindeutige Qualifikation und explizite Entscheidung behalten. |
| `docs/ACCEPTANCE_TESTS.md` | Automatische Recovery-, Zeit- und gewichtete-Progress-Orakel | Veraltete Orakel gezielt korrigieren; die vollständige Pflichtmatrix aus Abschnitt 11 dieses Plans ergänzen. |
| `docs/SPECIFICATION_REVIEW.md` | „phasenbezogener sicherer Wiederanlauf“ kann automatische Aktorfreigabe nahelegen | Release-1-Scope auf allgemeine Persistenz plus explizites, nicht automatisch freigebendes Resume-Angebot präzisieren. |
| `docs/REQUIREMENTS.md` | Vier Klassen und automatische Recovery ohne Owner-Gate | Minimalreaktionen, SafeBoot, NoActiveRun-Abbruch und klare Gate-Reihenfolge normieren. |
| `docs/ARCHITECTURE.md` | Safety-Core-Beschreibung enthält historische vier Klassen und Recovery-Komplexität | Eine Safety-Autorität, zentrale Planner-Grenze, Platform-Trennung und kleine Safety-Persistenz dokumentieren. |
| `docs/CONFIGURATION_PERSISTENCE.md` | Der #24-Anschluss ist korrekt, aber die neue Run-Abgrenzung fehlt | `CONFIGURATION_SAFETY_INTEGRATION_GATE` unverändert erhalten und explizit auf Boot/Restore/no-normal-release beziehen. |
| `docs/IMPLEMENTATION_ISSUES.md` | #24-Abschlussbeschreibung kann alte Recoveryannahmen enthalten | Abhängigkeit #56/#57 -> #24 und #20 -> #21 -> #22 -> #23 -> #24 mit Scope-Reset aktualisieren; keine neue Zyklusabhängigkeit. |
| `docs/ROADMAP.md` | Status nennt PR #110, aber den neuen Owner Scope Reset noch nicht | Draft-/NOT_STARTED-Status, superseded alte Planrevision und Owner-Gate für die neue exakte Plan-SHA sichtbar halten. |
| `docs/DECISIONS.md` / ADR-018 | Muss gegen Config- und Persistenzänderung geprüft werden | Keine inhaltliche Abschwächung; nur falls die Umsetzung eine echte ADR-Abweichung findet, Ownerentscheidung vor Code. |
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
| 3. unvollständiger, widersprüchlicher oder nicht eindeutiger Lauf | Aktoren AUS; alter Lauf wird sicher zu `NoActiveRun` beendet; bei eindeutigem Write ist neue Charge möglich; bei unklarem Write SafeBoot | Persistence + FSM + Store fault injection |
| 4. kritischer/unknown Safetyzustand | `ImmediateStop`/SafeBoot, keine normale Freigabe, persistente Sperre falls erforderlich | SafetyCore + Planner + Restart |
| 5. Sensorfehler | sensorrollenspezifisch AUS/failover gemäß #20/#21; keine Freigabe aus stale/failed Daten | Sensor Quality/Selection + Planner |
| 6. Aktor-/Plannerfehler | unmittelbares AUS, latched Verhalten gemäß Code, kein zweiter Bypass-Pfad | Planner + sink + SafetyCore |
| 7. Persistenz-/Readbackfehler | kein neuer Zustand behauptet; AUS; safe abort oder SafeBoot je nach betroffener Persistenz | Store/codec/coordinator |
| 8. unbestimmter Commitzustand | `CommitOutcomeUnknown` bleibt unbekannt; keine Slot-Wiederverwendung, kein Run-/Config-Release | Simulated Store + Run/Config service |
| 9. Brownout | Resetcause erkannt/unknown-safe; Boot all-off; kein transparenter aktiver Resume | Reset mock + application + sink |
| 10. Watchdog/Resetcause und wiederholte abnormale Neustarts | einzelner Restart bleibt AUS; Wiederholung erreicht definierte SafeBoot-Sperre; keine Charge-Rettung | Reset port + safety persistence + ESP-IDF adapter contract |
| 11. `ConfigurationRuntimeFailure` | Config-Gate blockiert normale Aktoren und erzeugt passende SafeBoot/latched Reaktion | Config recovery + SafetyCore + sink |
| 12. `ConfigurationUnavailable` | SafeBoot/ImmediateStop, keine Factory-/Defaultfreigabe als Laufstart | Config recovery + boot |
| 13. `ConfigurationIntegrityFailure` | SafeBoot/ImmediateStop, keine Quittierungsfreigabe | Config recovery + boot |
| 14. nicht auflösbarer `CommitOutcomeUnknown` | weder alte noch neue Config behaupten, keine Publish-/Aktorfreigabe | Config service + simulated store |
| 15. reale Aktoradapter | Safety-Gate kann den finalen Idle-/Stop-Pfad nicht umgehen; unbekannte/malformed Gatewerte bleiben AUS | Sink driver, ESP-IDF adapter integration gate |

Zusätzliche gezielte Negativnachweise:

- Boot mit gültigem Run, aber altem Schema-3-Recoveryfeld: kein Zeit- oder
  Progresskredit und kein automatischer Aktorstart;
- beschädigter Current-Record mit mehrdeutigem Fallback: keine Promotion,
  kein Rollback-Resume, sicherer Abschluss oder SafeBoot;
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
Fallback-Rettung werden gezielt entfernt oder als negative R1-Orakel
umgeschrieben. Hardware-Brownout, echter Watchdog, reale Resetcause und
physische Aktorabschaltung sind keine nativen PASS-Nachweise.

## 12. Umsetzungs- und Commitstruktur nach Ownerfreigabe

Keiner dieser Umsetzungsschnitte darf vor Freigabe der exakten neuen
Plan-SHA begonnen werden.

1. **Safety-Verträge und minimale Persistenz**
   - stabile Fault-Codes/Dispositionen, SafetyView und einzige SafetyCore-
     Autorität;
   - app-neutraler Resetcause-Port und kleiner Safety-Sperr-/Restart-Record
     über vorhandene Store-/Envelope-/Readback-Verträge;
   - `Unresolved` als Boot-Default und fail-closed Write-/Readback-Semantik.

2. **Boot-/Config-/Reset-Integration**
   - sichere Initialausgabe vor Validierung;
   - reale #56/#57-Producer-Gates und Config-Indeterminate-Abbruch;
   - Resetcause-/Watchdog-Auswertung, wiederholte Neustarts und SafeBoot;
   - ESP-IDF-Adapter sowie native Test-Injektion gemäß ADR-013.

3. **Vereinfachung der Run-Persistenz-/FSM-Nutzung**
   - vollständige/eindeutige Read-only-Qualifikation;
   - explizites Resume-Angebot ohne Boot-Release;
   - sichere `NoActiveRun`-Beendigung nicht rekonstruierbarer Runs;
   - `CommitOutcomeUnknown` und Readbackfehler als unknown-safe;
   - Entfernung der aktiven C-Write-/Zeit-/Progress-/Promotionpfade bei
     Erhalt nur notwendiger Legacy-Decode-Kompatibilität.

4. **Aktorpfade und Fehlerreaktionen**
   - SafetyCore als einzige Quelle für die bestehende Planner-Gate-Eingabe;
   - route-spezifische Allow-/Deny-Integration für Start, Resume, Sensor-,
     Planner- und reale Sinkpfade;
   - Quittierung, Recheck und persistente Sperre sauber trennen.

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
- echte Watchdog-/Resetcause-/Wiederholungsnachweise auf Zielhardware;
- Sensor-/Aktor-Inbetriebnahme, thermische Parameter und Belastungsgrenzen;
- #56/#57- und das separate produktive #106-Integrationsgate;
- Speicher-/Zeitbudgets für die minimale Safety-Persistenz und den
  SafetyCore;
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
