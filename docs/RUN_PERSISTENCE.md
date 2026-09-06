# Laufpersistenz und Wiederherstellbarkeit

## Ziel

Ein aktiver Prozess muss nach einem vollständigen Spannungsverlust soweit
rekonstruierbar sein, wie gueltige fachliche Daten vorhanden sind. Direkte GPIO-,
MOSFET- oder H-Bruecken-Zustaende werden nie wiederhergestellt.

## Grundsaetze

- Nach jedem Boot bleiben Peltier und Aktoren zunaechst AUS.
- Resetursache, aktueller Loadstatus und kritische Speicherintegritaet werden
  vor `STANDBY` oder einem Resume-Angebot bewertet; #24 zaehlt keine Bootschleifen.
- Laufdaten, Sensoren, Sicherheitszustand und Wiederanlaufaktion werden vor jeder
  Freigabe validiert.
- Wichtige Ereignisse werden sofort gespeichert.
- Periodische Kontrollpunkte ergaenzen die ereignisgesteuerten Speicherungen.
- Es wird nicht in jedem Sensorzyklus geschrieben.
- Kritische Laufdaten und Sicherheitsjournale haben Vorrang vor Historien.
- Release 1 funktioniert mit 4 MB Flash ohne PSRAM.
- Release 1 reserviert keine dualen OTA-Slots.
- Ein Neustart ist kein Fehlerreset; #24 fuehrt keine allgemeine persistente
  Persistenz- oder Safety-Sperre ein.

## Issue #124 Release-1-Persistenzgrenze

Issue #124 fuehrt keine allgemeine Safety-Persistenz, keinen Restart-Zaehler,
kein Resetzeitfenster und keinen persistenten Watchdog-/Sensor-Latch ein. Der
kanonische #17-Loadstatus unterscheidet vertrauenswuerdige `Current`-/Tombstone-
Zustaende von technisch untrusted Persistenz. Ein vollstaendig validierter
Current-`FERMENTING`-Run wird nach dem exakten R1-Zeitvertrag logisch
automatisch fortgesetzt; technisch untrusted bleibt `SAFE_BOOT` und wird
nicht blind ueberschrieben. Die automatische logische Recovery gibt allein
keine Aktorfreigabe.

`PreparedHead -> CheckpointSlot -> CommittedHead` bleibt eine einzige
Gesamttransaktion. Einzel-Key-`Success` ist definitiv und benoetigt keinen
zweiten Readback; nur `CommitOutcomeUnknown` wird durch `writeExact()`
aufgeloest. RAM/FSM wird erst nach dem Gesamtstatus `Applied` geaendert.

Die historischen #18-Recovery-/Progressabschnitte dieses Dokuments sind C2-
Legacy. Sie werden von #124 nicht als aktiver gewichteter oder biologischer
Produktpfad aufgerufen; R1 fuehrt kein automatisches Fallback-Resume, keine
automatische Promotion und keine Charge-Rettungsrechnung ein. Der aktuelle
Current-`FERMENTING`-Pfad ist davon getrennt und verwendet ausschliesslich
`priorBootPhaseElapsed` sowie die Wandzeit seit dem exakt validierten
Checkpointrecord.

### R5.9-Record- und Recoverygrenze

Der bestehende Run-Port verwendet genau drei kanonische Records: `rh0` ist der
Head mit Current-/Fallback-Referenzen und Transaktionszustand; `rc0` und `rc1`
sind die beiden Checkpointslots. Erst der vollstaendig validierte Head- und
Checkpointgraph bestimmt, ob ein Current, Fallback oder kein sicherer Runstand
vorliegt.

`NotFound` bei einem konkreten Read ist nur die Beobachtung, dass dieser Key in
diesem Read keinen Record lieferte. Ein spaeterer Read-/Readbackfehler oder ein
nach Beginn einer Transaktion verlorener Record ist kein urspruengliches
`NoPersistedRun` und darf nicht als solches, als `NoActiveRun` oder als stiller
Loeschpfad behandelt werden. `NoActiveRun` ist nur fuer einen vertrauenswuerdig
bestimmten, semantisch nicht resumefaehigen Current beziehungsweise den
kanonisch bestimmten leeren Stand zulaessig; unklare, Prepared-, Orphan-,
Partial-, Mixed-, Corrupt- und indeterminierte Zustaende bleiben
Recovery-/Abort-required und fail-closed.

Die vorhandenen `RunPersistenceLoadStatus`,
`RunPersistenceCoordinatorState`, `RunPersistenceResultStatus` und die
stateless `ActuationInterlock`-Bewertung sind dafuer ausreichend. Recoverystatus bleibt als
technische/produktliche Beobachtung erhalten (`RECOVERY_STATUS_OBSERVABLE`),
ohne UI- oder Aktorfreigabe zu implizieren. Die logische Freigabe bleibt bis
zu vollstaendig validiertem Graph, `Applied`, FSM-Anwendung und frischer
Evidenz gesperrt.

### R5.9: aelterer Fallback als nicht-aktivierendes Resume-Angebot

`OLDER_VALID_CHECKPOINT_RESUME` ist in #90 eine Produktklassifikation und ein
nicht-aktivierendes Resume-Angebot, kein automatischer Boot-Resume und keine
Fallback-Promotion in RAM oder FSM. Sie ist nur zulaessig, wenn der Current-
Pfad unbrauchbar und der kanonisch referenzierte aeltere Fallback vollstaendig
gegen Head, Slot, CRC, Schema, `StorageEpoch` und Referenzen validiert ist.

Bis zur Benutzerentscheidung bleibt der logische Gatezustand
`UNRESOLVED`/`actuator_allowed=false`. Die weitere Kette lautet verbindlich:

```text
explizite Resume-Entscheidung
-> bestehender Write-before-Apply-Pfad
-> Gesamtstatus Applied
-> FSM-Anwendung
-> frische Config-/Sensor-/Planner-/Persistenzevidenz
-> erneute `ActuationInterlock`-Bewertung mit frischer Evidenz
-> erst dann eventuell ActuatorSafetyGateStatus::Allowed
```

Alte Zeit-/Progressgutschrift, Charge-Rettung, automatische Promotion und
Allowed allein aus einem Fallbackrecord bleiben verboten. Unklare oder nicht
vollstaendig validierte Fallbacklagen bleiben Recovery-/Abort-required und
fail-closed.

## Persistierter Laufzustand

Der aktive R1-Vertrag verwendet die kanonischen Run-/Transaktionsfelder. Die
nachfolgend mit Recovery-/Progressbezug genannten Schema-3-Felder bleiben als
C2-Legacy lesbar. `priorBootPhaseElapsed` wird dabei als neutraler exakter
Phasen-Timeroffset fuer den #124-Current-`FERMENTING`-Vertrag verwendet;
gewichtete/biologische Recoveryfelder sind kein R1-Resume- oder
Charge-Recovery-Vertrag.

Mindestens enthalten:

- eindeutige Lauf-ID
- unveraenderlicher Programmschnappschuss
- effektive Laufwerte und Laufrevisionen
- aktuelle Prozessphase
- Regelmodus und primaerer Regelsensor
- dokumentierte Sensorwechsel
- nominelle Dauer
- ehrliche beobachtete Fortschrittsbasis; `runProgress.observedRunSeconds`
  bleibt sicher beobachtete Laufzeit und enthaelt keine Ausfallzeit
- `priorBootPhaseElapsed` als neutraler, boot-unabhaengiger und bei Bedarf
  exakt getaggter Phasen-Timeroffset fuer den R1-FSM-Handoff
- Verlaengerungen und Korrekturen nur ueber bestehende kanonische Commands
- letzter monotoner Zeitstand
- Zeitqualitaetsstatus als Diagnose
- letzte gueltige Temperaturen und Qualitaetszustaende von:
  - Schrankluft
  - Produkt, sofern vorhanden
  - Kuehlkoerper/Aussenwaermetauscher
- Zielqualifikationsfortschritt
- Kuehl- oder Haltezustand
- Abschlussverhalten
- relevante Meldungsreferenzen
- letzte Zustandsaenderung
- Revisionsnummer, Schema und Integritaetsinformation
- Transaktionsstatus fuer sicherheits- und aktorwirksame Zustandsaenderungen

### Schema 5: ManualTimed-Quellvertrag

Schema 5 unterscheidet im `ProgramRun`-Checkpoint explizit zwischen
gespeicherten Programmen und der katalogunabhaengigen `ManualTimed`-Quelle.
Gespeicherte Factory-/User-Programme persistieren weiterhin ihr
`RunProgramSourceRevision` und das unveraenderliche `ProgramDocument`.
`ManualTimed` persistiert stattdessen nur den validierten fluechtigen
Laufschnappschuss aus Zieltemperatur, Dauer, Vorheizen, Produktwartezeit,
Zielqualifikation und Abschlussverhalten; seine Source-Revision ist absent.

Die aktuelle Schema-Semantik wird nicht in Schema 4 hineingepresst: neue
Writes verwenden Schema 5. Die bestehenden Schema-1-bis-4-Records bleiben
lesbar und werden als gespeicherte ProgramRun-Quellen mit ihrer echten
historischen Revision interpretiert. Ein unbekanntes neueres Schema bleibt
fail-closed. Es gibt keine Migration alter Records nur fuer diesen Fachfall.
Der gespeicherte ProgramCatalog wird durch einen ManualTimed-Lauf nicht
geaendert.

Nach dem Decode wird derselbe bestehende ProgramRun-/Timed-
Recoveryvertrag verwendet. `PREHEATING`, `WAITING_FOR_PRODUCT`,
`REACHING_TARGET` und `QUALIFYING_TARGET` folgen der bestehenden
Nicht-resumierbarkeits-/Resume-Matrix; `FERMENTING` verwendet die bestehende
Current-FERMENTING-/trusted-UTC-Regel einschliesslich
`WaitingForTrustedTime`, `COOLING`/`COOL_HOLDING` das bestehende
Completion-Recovery, und `COMPLETED` bleibt terminal. Untrusted Persistenz
und unqualifizierte/unklare Phasen bleiben fail-closed. Es wird keine zweite
Recoverypolicy eingefuehrt.

Schema-3-Felder wie UTC-Anker, Recovery-Episode, Boot-Anker, nominale
Zeitkorrektur und gewichtete Progressbasis bleiben darunter als C2-Legacy
kompatibel lesbar. `weightedProgress`, Temperatur-Evidenz und
`nominalRecoveryAdjustment` erzeugen in R1 weder Resume-Gutschrift noch
Charge-Recovery. `priorBootPhaseElapsed` ist die Ausnahme: Sein bestehender
neutraler Vertrag wird fuer die exakte R1-Phasenkontinuitaet verwendet. Nur
`lowerBoundSeconds == upperBoundSeconds` mit `taggedState == FERMENTING` ist
automatisch zulaessig; fehlende Bounds oder partielle Historie bleiben
fail-closed.

### Exakter R1-Current-`FERMENTING`-Checkpointvertrag

Die UTC des exakt validierten `RunPersistenceRawRecord` ist der Recordanker.
Payload, Checkpointrevision, monotone Checkpointzeit und
`utcUnixSeconds` werden gemeinsam gegen den Current-Head, CRC, Schema, Epoch
und Referenzen validiert. Die Phasenzeit wird schemafrei so berechnet:

```text
prior_phase_elapsed_seconds =
    0, falls priorBootPhaseElapsed fehlt
    exact lowerBoundSeconds, falls FERMENTING getaggt und lower == upper

current_live_segment_to_checkpoint =
    (checkpointMonotonicMillis - stateEnteredAtMillis) / 1000

phase_elapsed_at_checkpoint =
    prior_phase_elapsed_seconds
    + current_live_segment_to_checkpoint

wall_clock_since_checkpoint_seconds =
    trusted_current_utc - checkpoint_record_utc

recovered_phase_elapsed =
    phase_elapsed_at_checkpoint
    + wall_clock_since_checkpoint_seconds
```

Alle Operationen und das Narrowing auf bestehende Feldbreiten sind checked.
Ein negativer UTC-Abstand, ungueltige Zeitordnung, Overflow oder untrusted
Recordgraph fuehren fail-closed zu `RecoveryRejectedOrFailClosed`.
`wall_clock_since_checkpoint_seconds` ist keine hergeleitete exakte
physische Ausfalldauer: Der Zeitraum kann auch noch powered-on gelaufene Zeit
vor dem Stromausfall enthalten.

Die beobachtete Laufzeit wird separat und ausschliesslich um den sicher
beobachteten Live-Abschnitt erhoeht:

```text
observedRunSeconds =
    previous_observedRunSeconds + current_live_segment_to_checkpoint
```

Ausfallzeit, rekonstruierte Phasenzeit und der Priorwert werden nie in
`observedRunSeconds` geschrieben. Unterhalb der nominellen Dauer wird der
Kandidat mit `stateEnteredAtMillis = current_monotonic_millis` und einem
exakten `priorBootPhaseElapsed` des wiederhergestellten Phasenwertes
persistiert. Die FSM erhaelt diesen Wert ueber ihren bestehenden
`priorElapsed`-Parameter und bleibt frei von Persistenz, UTC und `ITimeSource`.

Fehlt die aktuelle UTC, bleibt der geladene Current-Checkpoint unveraendert;
`RecoveryEvaluation/WaitingForTrustedTime` ist eine RAM-/Application-
Disposition ohne Tombstone oder Schreibvorgang nur fuer das Warten. Spaetere
Neubewertung nutzt dieselbe Revision; eine veraenderte Evidenzbasis wird
fail-closed abgelehnt.

Nicht gespeichert werden:

- direkte H-Brueckenpegel
- letzte Heiz- oder Kuehlfreigabe als Bootbefehl
- rohe GPIO-Zustaende
- blinde Aktor-Wiederherstellungsbefehle
- fluechtige `ControlRequest`-Sequenzen, PI-Integratorwerte,
  Feedbackfenster, `lastActiveDirection` oder Qualifikator-Episoden

Nach Recovery beginnt der #22-PI-/Qualifikatorzustand leer. Eine alte
`qualificationValidSinceMillis`-Markierung bleibt ausschliesslich ein
Prozess-/Diagnosemarker und erzeugt keinen Qualifikationskredit. In R1 gibt es
keine Recovery-Rebase von `QUALIFYING_TARGET` nach `REACHING_TARGET`; ein
solcher historischer #18-Pfad ist ausschliesslich C2-Legacy.

Der Qualifikatorzustand bleibt fluechtig und wird nicht persistiert. Seine
Evaluation erzeugt zunaechst einen Kandidaten; bei einer persistierbaren
Prozess- oder Markeränderung wird dieser erst nach erfolgreichem
Write-before-Apply und `applyProcessTransition()` uebernommen. Bei einem
fehlgeschlagenen Schreiben oder Anwenden bleibt der alte Evaluatorzustand
wirksam. Ohne persistierbare Aenderung darf der Kandidat RAM-only uebernommen
werden. Ein Retry bewertet deshalb den unveraenderten Live-Zustand neu und
schreibt keine Qualifikationszeit doppelt gut. Jede Decision ist dabei
single-use: ein stale oder fehlgeschlagener Kandidat wird verworfen und kann
nicht durch eine spaetere Statusumdeutung erneut angewendet werden. Der
hardwarefreie Orchestrator bildet `decision.progress` in `ProcessSignals`,
ruft ausschliesslich `decideProcessTransition()` auf und verwendet fuer den
Commit den bestehenden `RunPersistenceCoordinator`; er fuehrt weder eine
zweite Prozessmaschine noch einen zweiten Persistence-Coordinator ein.
Der Stale-Kontext bindet die bestehende `runRevision` und
`processTransitionSequence`, ohne dadurch allein die fachliche Episode zu
resetten. Die Sample-Zeit des Qualifikators muss dabei exakt der
`RunCheckpointTime.monotonicMillis` entsprechen; Abweichungen werden stale
verworfen und erzeugen keinen Write.

Die erfolgreichen Write-before-Apply-Ergebnisse tragen ausserhalb des
Persistenzschemas einen fluessigen Handoff-Hinweis: `persistCommand()` fuer
Ziel-/Kontextaenderungen, `persistTransition()` fuer Ziel-/Coolingwechsel und
`persistSensorSelection()` fuer einen persistierten `RunSensorMode`-
Wechsel, der nach erfolgreichem Write-before-Apply einen
`SensorSelectionEvent`/Moduswechsel tragen kann. Der #22-Handoff entsteht
daraus nur bei einem echten effektiven `ControlSensorRole`-Wechsel
`Air <-> Product`; ein effektiver `Air -> Air`-Pfad in
`PREHEATING`/`WAITING_FOR_PRODUCT` erzeugt keinen `SensorRoleChange`.
Notices, fehlgeschlagene Writes und fehlgeschlagene RAM-Applies tragen
keinen solchen Hinweis. Die einzige
Anwendungsgrenze dafuer ist
`TemperatureControlApplicationOrchestrator`: Sie konsumiert Hinweise nur bei
`RunPersistenceResultStatus::Applied`, hoechstens einmal, und uebergibt sie
erst danach an den PI-Kern. Sie leitet ausserdem Vollresets aus dem kanonischen
Before-/After-Laufzustand sowie der Recovery-Grenze ab. Daraus wird weder ein
zweiter Prozesszustand noch ein Persistenzfeld.

## Speicherereignisse

Eine neue atomare Revision entsteht mindestens bei:

- Laufstart
- jedem Prozessphasenwechsel
- Bestaetigung `Produkt eingesetzt`
- Aenderung des primaeren Regelsensors
- laufrelevantem Sensorausfall oder validierter Rueckkehr
- manueller Laufanpassung
- laufrelevanter, explizit persistierter Fortschrittsanpassung ueber einen
  bestehenden kanonischen Command
- neuer Warnung oder neuem Fehler mit Laufwirkung
- Quittierung oder einem expliziten Start-/Resume-Entscheid mit
  Zustandswirkung
- Start oder Ende von Kuehlen und Halten
- Abbruch
- Abschluss
- einer expliziten, ueber den bestehenden #17-Pfad persistierten
  Start-/Resume-Entscheidung; der enge #124-Current-`FERMENTING`-Sonderfall
  mit exakter Validierung und trusted UTC ist aktueller R1-Vertrag,
  historische gewichtete/biologische/Charge-/Fallback-Autorecovery ausserhalb
  dieses Vertrags bleibt C2-Legacy. Automatisches Fallback-Resume und
  automatische Fallback-Promotion bleiben ausgeschlossen.

Zusammengehoerige Aenderungen duerfen in einer atomaren Revision gebuendelt
werden. Ein Schritt, der spaeter Aktoren freigeben kann, wird erst angewendet,
nachdem Transaktionsabsicht und neue Revision erfolgreich gespeichert wurden.

## Periodische Kontrollpunkte

```text
Minimum:           1 Minute
Werkseinstellung:  5 Minuten
Maximum:          60 Minuten
```

Der Wert ist eine PIN-geschuetzte Serviceeinstellung innerhalb firmwarefester
Grenzen. Zustandswechsel werden unabhaengig davon sofort gespeichert.

Das Kontrollpunktintervall ist zugleich eine Grenze fuer die Unsicherheit des
Stromausfallzeitpunkts. Es darf nicht mit der exakten Ausfalldauer verwechselt
werden.

## C2-Legacy: Fortschritts- und Zeitmodell, nicht #124-R1

Die folgenden Felder und Berechnungen bleiben als Schema-3-/#18-Bestand
lesbar, werden aber im aktiven #124-R1-Pfad weder fuer Resume noch fuer eine
Charge-/Zeitgutschrift verwendet. Neutral vorhandene Schema-3-Felder duerfen
ein einfaches Resume nicht pauschal ablehnen; aktive alte Recovery-Evidenz
fuehrt stattdessen zu `NoActiveRun`.

Die Wiederherstellung verlaesst sich nicht auf einen einzelnen Countdown.
Kombiniert gespeichert werden:

- kumulierter temperaturgewichteter Fortschritt
- nominelle Dauer
- bisherige Verlaengerungen und Korrekturen
- monotone Laufzeit
- letzter UTC-Anker
- Sensor- und Zeitqualitaet

Die biologische Wirkung wird nicht durch eine erfundene Kurve behauptet. Ein
Produktionsprovider fuer die Gewichtung bleibt ohne freigegebenes
Commissioning-Modell `unavailable`; die Firmware schreibt dann keinen
biologischen Beitrag gut. Die reine Modellgrenze akzeptiert nur eine
Fermenting-Episode mit bekannten Ausfallgrenzen und gueltiger, gefilterter
Vor-/Nach-Ausfall-Evidenz. Der Produktfuehler hat die Vertrauensstufe
`ProductPreferred`, der ausdruecklich verwendete Luftfuehler `AirReduced`.

### Historischer Recovery-/Fortschrittsvertrag (Schema 3, C2-Legacy)

Der persistierte Vertrag besteht aus den getrennten Feldern
`pendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis`,
`lastRecoveryEpisodeEvidence`, `priorBootPhaseElapsed`,
`nominalRecoveryAdjustment`, `recoveryEpisodeRevision` und
`runProgress`. `observedRunSeconds` bleibt die monotone beobachtete Laufzeit;
nominale Recovery-Korrekturen und gewichtete Beitraege werden nicht in diesen
Wert hineingeschrieben. Der Wert faltet die sicher beobachtete Zeit bei jedem
echten Live-Phasenwechsel aus `Fermenting`, bei einer echten
`AdjustRun`-Restdauer-Neubaseline und bei jedem echten Hop 1 aus `Fermenting`
genau einmal. Beim Hop 1 wird ausschliesslich
`thisHopAltBootLocalSeconds` dieses konkreten Boots verwendet; ein
Episode-Refresh erzeugt keinen Fold.

`ApplyRecoveryTimeCorrection` ist kumulativ und nur fuer die passende
Recovery-Episode zulaessig. Bounds, Episodenrevision, Idempotenz und
Counter-Overflow werden vor dem Schreiben geprueft. Eine wirksame Aenderung
faltet bei `AdjustRun` die seit dem Eintritt beobachtete Fermenting-Zeit genau
einmal in die Zeitbasis und setzt die Recovery-Baseline neu. Persistenz ist
Write-before-Apply; erst nach erfolgreichem atomarem Recovery-Schreiben wird
der RAM-Zustand geaendert.

`RunRecoveryCoordinator::activate` ist die schmale Orchestrierungsgrenze fuer
geladenen aktiven Lauf und Fallback. Sie delegiert die bestehende
Recovery-/Regelsensorauswahl und enthaelt keine eigene Prozessschleife.
`reevaluateRecoveryTime` leitet eine spaetere Zeitverbesserung aus demselben
Anker ab und persistiert sie als Recovery-Revision. Im Hop-1-only-Fall
`RecoveryEvaluation` mit urspruenglichem `WaitingForProduct` wird
`DefinitelyExpired` automatisch ohne Gate A als bestehender Tombstone-Pfad
beendet. `DefinitelyStillValid` darf nur mit frischem Gate-A-Kontext nach
`WaitingForProduct` resumieren; der No-context-Aufruf bleibt fail-closed.
`Uncertain` schreibt und mutiert nichts. Ein produktiver Aufrufer oder eine
allgemeine Prozessschleife ist in Release 1 nicht Bestandteil dieses Vertrags.

Die gewichtete Buchung prueft erwartete Lauf- und Episodenrevision,
Segmentkennung, Sensorberechtigung, Modellrevision und checked Bounds. Ein
Segment wird hoechstens einmal gebucht. Ein nicht gebuchtes, abgeloestes
Segment setzt die Coverage konservativ auf `PartialUnknown` und entfernt die
Obergrenze; ein gebuchtes Segment bleibt unveraendert. Eine atomare Buchung
aktualisiert nur den gewichteten Zustand und die Laufrevision. Ohne Modell,
ohne Evidenz oder bei ungueltiger Evidenz bleibt der Provider unavailable-
beziehungsweise not-eligible und erzeugt keinen Fortschritt.
`lastSourceRole` bezeichnet dabei immer die Quelle des zuletzt gebuchten
Beitrags. `confidence` bezeichnet die kumulative, konservative Vertrauensstufe:
ein einziges `AirReduced` bleibt auch nach einem spaeteren
`ProductPreferred`-Beitrag `AirReduced` und wird nie hochgestuft.

## C2-Legacy: Zeitanker und Ausfallintervall, nicht #124-R1

Nach dem NTP-Abgleich ist

```text
aktuelle UTC - UTC des letzten Kontrollpunkts
```

nur die obere Grenze der moeglichen Ausfalldauer. Das Geraet kann nach dem letzten
Kontrollpunkt noch bis zum naechsten planmaessigen oder ereignisbezogenen
Speicherzeitpunkt gelaufen sein.

Mindestens zu speichern beziehungsweise abzuleiten sind:

- UTC und Zeitqualitaet des letzten Kontrollpunkts
- monotone Laufzeit an diesem Kontrollpunkt
- konfiguriertes Kontrollpunktintervall
- Zeitpunkt und Art der letzten ereignisbezogenen Speicherung
- Hinweise auf ausgelassene oder verspaetete Kontrollpunkte

Recovery berechnet:

```text
obere Grenze = aktuelle UTC - letzter verlaesslicher UTC-Kontrollpunkt
untere Grenze = max(0, obere Grenze - maximal moeglicher Kontrollpunktabstand)
```

Die Berechnung verwendet bei einer laufenden Recovery-Episode den
unveraenderlichen Ursprungsanker und den aktuellen Recovery-Boot-Anker.
Carry-forward-Zeit aus einem frueheren Boot wird in der bekannten Zeitbasis
gefuehrt; sie erzeugt keine kuenstliche neue Ausfall-Untergrenze. Ein spaeterer
UTC-Abgleich darf nur eine echte Verbesserung der bereits persistierten unteren
Grenze oder die Aufloesung einer offenen Obergrenze nachtragen.

Fuehren beide Grenzen nicht zur gleichen Fortschritts- oder Phasenentscheidung,
wird kein automatischer Abschluss ausgeloest. Beide Grenzen und die Konfidenz
werden angezeigt und exportiert.

## Messhistorie

### Live-Ringpuffer im RAM

- feste Maximalgroesse
- haeufige aktuelle Werte
- fluechtig
- keine notwendige Wiederherstellungsgrundlage

### Dauerhafte Aggregate

Fuer begrenzte Zeitfenster werden kompakt gespeichert:

- Mittelwert
- Minimum
- Maximum
- Sensor- beziehungsweise Gueltigkeitsstatus

Ereignisse wie Phasenwechsel, Unterbrechungen, Warnungen, Fehler und Sensorwechsel
werden getrennt mit genauer verfuegbarer Zeit markiert.

Ein Minutenfenster ist der bevorzugte Ausgangspunkt fuer den aktuellen Lauf, aber
keine Garantie fuer unbegrenzte minutengenaue Langzeitarchivierung.

## Aufbewahrung

Werkseinstellung innerhalb eines festen Budgets:

- aktiver Lauf vollstaendig
- letzte 5 abgeschlossene Laeufe detailliert
- letzte 50 Laeufe als Zusammenfassung

Die Werte sind innerhalb fester Obergrenzen konfigurierbar. Bei Platzmangel gilt:

1. temporaere Exportdaten entfernen
2. aelteste nichtkritische Diagrammdetails verdichten oder entfernen
3. aeltere detaillierte Laeufe zu Zusammenfassungen reduzieren
4. aelteste nichtkritische Zusammenfassungen entfernen
5. Sicherheits-, Reset- und Fehlerereignisse laenger erhalten
6. aktiven Lauf und Rueckfallrevisionen niemals fuer Komfortdaten opfern

Bereinigung erfolgt proaktiv vor einer Ressourcenwarnung.

## Rueckfall bei beschaedigten Daten

Ist die aktuelle Revision technisch untrusted, wird sie nicht durch eine
Fallback-Promotion oder eine geschaetzte Zeit-/Progressrechnung ersetzt: Das
Geraet bleibt in `SAFE_BOOT`. Ein trusted, semantisch nicht resumefaehiger
Current wird nur nach seinem jeweils geltenden kanonischen Pfad behandelt.
Ein vollstaendig validierter Current-`FERMENTING`-Run wird nach #124 mit
vertrauenswuerdiger UTC automatisch logisch fortgesetzt; fehlt diese UTC,
bleibt er als `RecoveryEvaluation/WaitingForTrustedTime` unveraendert im RAM.
Nicht eindeutig rekonstruierbare Evidenz wird nicht als `NoActiveRun`
umetikettiert.

## Kritischer Persistenzfehler

Schlaegt ein kritischer Schreibvorgang fehl:

1. neue Aktoranforderungen sperren
2. Peltier AUS und erforderlichen Luefternachlauf ausfuehren
3. den bestehenden #17-Coordinator unknown-safe/blockiert halten
4. Fehlerzustand anzeigen und protokollieren; keine neue persistente Safety-
   oder Persistenz-Latch-Wahrheit schreiben

Beim Boot gilt:

- unvollstaendige Transaktionsabsicht -> `SAFE_BOOT`
- nicht les- oder schreibbarer kritischer Speicher -> `SAFE_BOOT`
- Recovery-/Fresh-Start-Freigabe erst nach aktuellem kanonischem Producerpfad,
  `Applied`, FSM-Anwendung und frischer Evidenz

## Meldungen nach Neustart

1. Meldungshistorie laden
2. Sensoren, Producer-/Persistenzstatus und System neu pruefen
3. fruehere aktive Meldungen gegen aktuelle Ursachen bewerten
4. weiterhin aktive Meldungen wieder anzeigen
5. beseitigte Ursachen als erledigt kennzeichnen
6. historische Ereignisse erhalten

Ein Neustart laedt keine nicht mehr bestehende Warnung blind als aktiv. #24
persistiert dafuer keinen allgemeinen Safety-/Watchdog-/Sensor-Latch; aktuelle
untrusted Load-Zustaende bleiben `SAFE_BOOT`. Der exakt rekonstruierbare
Current-`FERMENTING`-Fall wird nach #124 logisch automatisch fortgesetzt,
ohne dadurch Aktoren freizugeben; fehlende UTC fuehrt zu
`RecoveryEvaluation/WaitingForTrustedTime` statt zu einem Tombstone.

## Wiederanlaufreihenfolge im #124-R1-Pfad

```text
Boot
-> alle Ausgaenge AUS
-> Resetcause diagnostisch erfassen
-> Config und Load-/Coordinatorstatus frisch validieren
-> Completed erhalten, NoPersistedRun/NoActiveRun als Standby projizieren
-> Current-`FERMENTING` gegen den exakten #124-Zeitvertrag pruefen
-> bei fehlender UTC `RecoveryEvaluation/WaitingForTrustedTime` RAM-only halten
-> bei gueltiger UTC R1-Current-Kandidaten write-before-apply persistieren
-> untrusted Load als SAFE_BOOT sperren
-> nach logischer Recovery oder explizitem Start/Resume stets aktuelle
   Config-/Sensor-/Planner-/Safety-Evidenz fuer den Gatepfad pruefen
```

Der Wiederanlauf blockiert nicht auf NTP. Ohne absolute Zeit wird kein
Phasenfortschritt behauptet und keine Persistenz nur fuer `TimePending`
geschrieben. Die spaetere Bewertung verwendet denselben geladenen
revisionsgebundenen Checkpoint. Die Differenz von aktueller UTC und
Checkpoint-UTC ist `wall_clock_since_checkpoint_seconds` fuer die
Phasenrechnung, aber keine exakte physische Ausfalldauer.

## Historische Recovery-API und Regelsensorauswahl bei Reaktivierung (C2)

Issue #21 (Regelsensorauswahl, Ersatzbetrieb, Rueckkehrlogik) liefert den
persistierten und den laufzeitseitigen Auswahlzustand. Die
Die `RunRecoveryCoordinator`-Grenze bleibt als bestehender #18/C2-Code
erhalten, wird aber vom aktiven #124-R1-Produktpfad nicht aufgerufen. #124
verwendet stattdessen die aktuelle #20/#21-Projektion, das enge
Resume-Angebot und den kanonischen `NoActiveRun`-Abbruch.

Bereits vorhanden:

- der persistierte Auswahlzustand (Herkunft, letzte Ursache, letzte
  Laufrevision der Entscheidung) wird mit jedem aktiven Lauf gespeichert und
  beim Laden unveraendert uebernommen; ein Schema-1-Bestand ohne dieses Feld
  wird auf einen expliziten, unbestimmten Herkunftswert abgebildet, nie als
  fehlend behandelt;
- der laufzeitseitige Auswahlzustand (Phase, Peltier-Freigabe, laufende
  Wartezeiten) ist ausdruecklich ausserhalb des Wireformats und wird bei
  einem geladenen aktiven Lauf fail-closed auf den Zustand "Neubewertung nach
  Neustart erforderlich" mit gesperrter Freigabe gesetzt; jede laufende
  Wartezeit oder Rueckkehrvalidierung wird dabei verworfen;
- eine reine, seiteneffektfreie Funktion berechnet aus dem persistierten
  Auswahlzustand und dem konfigurierten Programmkontext eine Empfehlung fuer
  den reaktivierten Zustand, ohne selbst etwas zu veraendern oder zu
  speichern.

Fuer die Reaktivierung (geladener aktiver Lauf -> betriebsbereit) gilt:

- die Empfehlung wird angewendet und der laufzeitseitige Auswahlzustand
  gesetzt, bevor die Regelung fuer diesen Lauf bewertet wird;
- **Reihenfolge beachten:** ein aktiver Lauf verlangt beim Schreiben
  zwingend einen vorhandenen persistierten Auswahlzustand (`sensorSelection`)
  - dieser ist nach dem Laden bereits vorhanden und wird durch jede
  nachfolgende Speicherung (auch periodische Kontrollpunkte) unveraendert
  erneut mitgefuehrt. `sensorSelectionRuntime` ist davon unabhaengig: er ist
  ausserhalb des Wireformats, wird von keinem Kontrollpunkt mitgeschrieben
  und fliesst nicht in die Schreibvoraussetzung ein. Solange die
  Reaktivierung aussteht, bleibt er einfach fail-closed im RAM stehen -
  "Neubewertung nach Neustart erforderlich" (`RestartRevalidationPending`)
  mit gesperrter Peltier-Freigabe (`Blocked`); periodische Kontrollpunkte
  laufen davon unberuehrt normal weiter;
- die Peltier-Freigabe bleibt bis zum Abschluss dieser Neubewertung gesperrt;
  eine vorherige Freigabe aus dem Lauf vor dem Neustart wird nie blind
  uebernommen. Bei fehlender oder ungueltiger Live-Evidenz bleibt die
  Entscheidung fail-closed.

Test- und Zustaendigkeitsgrenze: Schema-1- und Schema-2-Bestaende bleiben
kompatibel, ausserhalb des bewusst abgelehnten aktiven Fault-Falls. Die
Reaktivierungs- und Fallback-Delegation prueft die bestehenden
Recovery-/Sensorselektionsregeln; die Persistenzkoordination bleibt Eigentuemer
von Gate A, Write-before-Apply und der atomaren Recovery-Revision.

## Flashstrategie

- atomare, versionierte Revisionen
- mindestens aktuelle und letzte gueltige Revision
- Transaktionsabsicht vor aktorwirksamen Zustandsaenderungen
- kein zusaetzlicher #24-Safety-Latch; #17-Transaktionsstatus ist die
  bestehende Persistenzwahrheit
- rotierende Speicherplaetze oder Wear-Leveling
- kritischer Laufdatensatz getrennt von Messhistorie
- keine Speicherung im Zwei-Sekunden-Sensorzyklus
- kompakte Binaer- oder vergleichbar effiziente kritische Datensaetze
- definierte Reaktion bei vollem oder beschaedigtem Speicher

Die konkrete Aufteilung zwischen NVS, LittleFS und eigenen Ringstrukturen wird in
#9, #10, #19 und #29 anhand realer Build- und Hardwarewerte festgelegt.

## Ressourcenbudget

Verbindlich:

- 4 MB Flash als Planungsbasis
- keine PSRAM-Abhaengigkeit
- Single-App-Layout fuer Release 1 zulaessig
- keine OTA-Reserve fuer Release 1
- getrennte Budgets fuer:
  - Firmware
  - Webressourcen und Sprachen
  - Konfiguration
  - aktiven Lauf und Rueckfall
  - Sicherheitsjournal und Persistenzfehler-Latch
  - Historie und Zusammenfassungen
  - temporaere Exporte
- Sicherheits- und Regelungslogik haben Vorrang

Der aktuelle Laufvertrag bleibt innerhalb fester Grenzen: maximal 8.192 Bytes
Payload, 8.240 Bytes Checkpoint-Record und 256 Bytes Head-Record; jeweils
maximal 32 Laufrevisionen und persistierte Command-IDs. Die neuen Recovery-
und Progressfelder nutzen ausschliesslich den bestehenden Schema-3-Datensatz,
keine unbounded Historie, keine neue Prozessschleife, keinen Netzwerk- oder
Anzeige-Puffer und keine OTA-/PSRAM-Reserve. Ein zusaetzlicher
Commissioning-Provider ist in Release 1 nicht eingebaut; ein
`TBD_IMPLEMENTATION_BUDGET` bleibt daher kein Laufzeitwert.
