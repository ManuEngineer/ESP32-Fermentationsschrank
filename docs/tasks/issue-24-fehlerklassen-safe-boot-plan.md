# Issue #24 - Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion

## Planrevision 3 - PLAN_R3_PENDING_OWNER_APPROVAL

| Feld | Verbindlicher Stand |
|---|---|
| Issue | #24 - `[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion` |
| Draft-PR | #107 |
| Branch | `agent/issue-24-fehlerklassen-safe-boot-plan` |
| Kontext-HEAD dieser R3-Pruefung | `ca1c6ebaaa08336552840bad96d9e8a94047b8ba` |
| Base/main | `b8eae5f4da5f2666b5a9bda333d115254c4db5b2` |
| historischer R2-Plan | `3b2befaf7595066cb8fcc0521b32e93212360ba5` - suspendiert und nicht normative Implementierungsgrundlage |
| aktueller R3-Status | `PLAN_R3_PENDING_OWNER_APPROVAL` |
| exakte SHA dieser vollstaendigen R3 | die SHA des Plan-/Dokumentationscommits; nach Commit im PR-Body und SESSION HANDOVER eingetragen |
| Produktionscode, Tests, Adapter, Build-/Toolchain und Hardware | in diesem Auftrag unveraendert und eingefroren |

Diese Revision ist eine vollstaendige, eigenstaendige Implementierungsgrundlage.
Sie verweist auf R2 nur als historischen, nicht akzeptierten Nachweis. Alle
OD-24-01 bis OD-24-10 sind in dieser Fassung technisch aufgeloest. Es bleibt
kein Owner-Gate fuer das Erfinden von Fehlercodes, Kapazitaetszahlen oder
Resetarchitektur. Das einzige Gate fuer den naechsten Schritt ist die
Freigabe der exakten SHA dieser gesamten R3 durch den Owner.

## 1. Gepruefte Baseline und Stop-Gate

Die erneute Live-Pruefung vor dieser Synchronisierung ergab:

- Branch ist `agent/issue-24-fehlerklassen-safe-boot-plan`.
- Lokaler und gepruefter `HEAD` ist `ca1c6ebaaa08336552840bad96d9e8a94047b8ba`.
- Arbeitsbaum war vor der Dokumentationsaenderung sauber.
- `origin/main` und der PR-Base-Stand sind `b8eae5f4da5f2666b5a9bda333d115254c4db5b2`.
- Issue #24 ist offen und verlangt die reale
  `CONFIGURATION_SAFETY_INTEGRATION_GATE`-Integration mit den echten #56/#57-
  Ergebnistypen vor dem Abschluss.
- PR #107 ist offen, Draft, auf dem genannten Branch und am geprueften HEAD.
- Der neueste SESSION HANDOVER bestaetigte den R3-Reset auf `ca1c6eb` und den
  suspendierten R2-Stand.
- `docs/ROADMAP.md`, der aktuelle Plan, die Root- und lokalen `AGENTS.md`,
  `docs/AGENT_WORKFLOW.md`, `docs/ENGINEERING_PRINCIPLES.md`,
  `docs/SPECIFICATION_REVIEW.md`, ADR-013/014/018 sowie die betroffenen
  Safety-, Recovery-, Command-, Persistenz- und Acceptance-Quellen wurden
  erneut gegen diesen Auftrag gelesen.

Es wurde keine materielle Abweichung bei Branch, HEAD, Issue, PR, main oder
kanonischer Quelle festgestellt. Der bestehende R2-Produktionscode bleibt
eingefroren. Vor der Ownerfreigabe der exakten R3-SHA werden keine
Implementierungsdateien, Tests, Adapter oder ADRs geaendert.

## 2. Ziel, Grenzen und wiederzuverwendende Architektur

### 2.1 Ziel

Issue #24 liefert einen einzigen zentralen, deterministischen und
fail-closed Safety-/Faultpfad fuer die Anwendung. Er klassifiziert die heute
begruendeten Ursachen, setzt die sichere Aktorreaktion vor jede
Komfortanforderung, haelt Latches ueber Neustarts, verarbeitet Restart- und
Persistenzevidenz, projiziert den Safetyzustand auf den bestehenden #23-
`ActuatorSafetyGateInput` und nimmt die echten #56/#57-Resultate am
Applicationpfad auf.

`Unknown` und `Unresolved` sind sicherheitsrelevante Ergebnisse: Sie duerfen
niemals `Allowed` erzeugen. Der einzige Aktorpfad bleibt

```text
#20/#21/#23/#56/#57-Ergebnisse
  -> eine zentrale SafetyFaultService-Instanz
  -> ActuatorSafetyGateInput
  -> ActuatorPlanner
  -> ActuatorPlanSinkDriver
```

### 2.2 Nicht-Ziele

Nicht Bestandteil dieses Plans sind:

- GPIO-, Pegel-, BTS7960-, ESP-IDF- oder konkrete Hardwareimplementierung;
- NVS-, Flash- oder reale Persistenzadapter;
- ESP-IDF-Resetcause-Mapping und `esp_restart()`;
- #35-Commissioning oder erfundene Recoveryparameter;
- Service-PIN-Verifikation, UI/Web, E4 oder #29/#90-Adapter;
- OTA, automatische Firmwaredownloads oder Release-1-Zukunftsfunktionen;
- ein zweiter Faultkern, Persistenzspeicher, Journalpfad oder eine zweite
  Safety-/Boot-State-Machine;
- eine Capability-, Token- oder Pointerarchitektur als Selbstzweck;
- ein globaler Reboot nach `FaultReset`;
- eine unbounded Historie oder ein unbegruendetes Latchlimit.

### 2.3 Kanonische Wiederverwendung

- `device_platform` bleibt auf anwendungsneutrale Ports beschraenkt.
- `device_platform_esp_idf` erhaelt erst spaeter konkrete ESP-IDF-Adapter.
- `fermentation_app` enthaelt die Fachlogik und konsumiert nur abstrakte Ports
  und reale Anwendungsergebnistypen.
- `device_platform_test_support` liefert nur deterministische native Testhilfen.
- ADR-013 traegt die Modul- und Portarchitektur; ADR-014 traegt die
  deterministische, persistenzfreie Zustandsauswertung; ADR-018 traegt die
  reale #56/#57-Integration und das Completion-Gate. Ein neuer ADR ist nicht
  erforderlich, solange die Detailpruefung keine neue langfristige
  Architekturentscheidung ausserhalb dieser Vertraege ergibt.

## 3. Abgeschlossene Ownerentscheidungen OD-24-01 bis OD-24-10

### 3.1 OD-24-01 und OD-24-02: Restart-Episode

Release 1 verwendet diese firmwarefeste Semantik:

1. Eine offene Restart-Streak/Episode zaehlt abnormale Neustarts.
2. Nach dem dritten abnormalen Neustart wird vor normaler Aktor- oder
   Lauffreigabe `SAFE_BOOT` erzwungen.
3. Die Episode wird nur durch 30 Minuten durchgehend stabilen,
   abnormal-restartfreien Betrieb geschlossen.
4. Die 30 Minuten werden ausschliesslich mit einer monotonen Laufzeitquelle im
   laufenden Boot gemessen. NTP, Netzwerkzeit, RTC und Wall-Clock-Zeit sind
   weder Voraussetzung noch Ersatz.
5. Die 30 Minuten sind firmwarefest und nicht servicekonfigurierbar.
6. Ein normaler Neustart vor Abschluss der Stabilitaetsphase schliesst die
   Episode nicht.
7. Ein abnormaler Neustart waehrend der Stabilitaetsphase verwirft die
   bisherige stabile Laufzeitbewertung; der naechste erfolgreiche Boot beginnt
   diese Bewertung neu.
8. Stromlosigkeit loescht die offene Episode nicht. Ohne stromausfallsichere
   Zeitbasis ist die Dauer des Power-off unbekannt und keine Entwarnung
   beweisbar.
9. Ein kontrollierter Neustart ist nur dann abnormal, wenn ihn die
   codebezogene #24-Policy als Safety-/Software-Recovery-Neustart klassifiziert.
   Ein autorisierter normaler Service-/Recovery-Neustart zaehlt nicht
   automatisch abnormal.
10. Unbekannter Resetgrund wird nicht als normal geraten, sondern fail-closed
    als Systemunsicherheit behandelt.
11. Ein normaler Neustart verlaesst `SAFE_BOOT` niemals. Ein Exit ist nur ueber
    die explizite, codebezogene und autorisierte SAFE_BOOT-Policy nach erneuten
    Integritaets-, Persistenz-, Sensor- und Aktorpruefungen moeglich.

Diese Regeln ersetzen die vorher offene Formulierung eines Wall-Clock-
Zeitfensters. Der episodebezogene Zustand wird persistiert; ein erfolgreicher
stabiler Lauf schliesst ihn erst nach dem nachgewiesenen monotonen Intervall.

### 3.2 OD-24-03 und OD-24-10: stabile Codes und codebezogene Resetpolicy

Der Namensraum `P1-*`, `O2-*`, `S3-*`, `Y4-*` wird verwendet. Die Nummerierung
ist lueckenlos innerhalb der heute begruendeten Matrix; alte R2-Nummern und der
R2-Fallback `Y4-011` sind keine normative Quelle. `Unknown`/`Unresolved` wird
als `Y4-008` mit eigenem fail-closed Systempfad behandelt.

Jede Tabellenzeile enthaelt Klasse, Producer/Ursache, unmittelbare Reaktion,
Latch, Auto-Rearm, Resetberechtigung, Reboot-/SAFE_BOOT-Policy und
Primaer-/Folgefaehigkeit. Ein Folgefehler darf die eigene Reaktion nie
unterdruecken. Die Code-/Producer-Matrix ist in Abschnitt 4 und kanonisch in
`docs/SAFETY_AND_FAULTS.md` festgeschrieben.

### 3.3 OD-24-04: Engineeringnachweis fuer die Latchkapazitaet

Die aktive Worst-Case-Matrix ergibt acht unabhaengige S3-Instanzen und neun
Y4-Instanzen, also `17` aktive persistente Latches. Cleared-Historie zaehlt
nicht. Die Ableitung, Wire-Groesse, Bound und das Overflowverhalten stehen in
Abschnitt 6. Die Zahl `8` aus R2 wird nicht uebernommen.

### 3.4 OD-24-05: S3-004 contract-only

S3-004 wird klassifiziert und schaltet sofort sicher ab. Der bestehende #23-
Planner-/Gatepfad wird fuer einen spaeter qualifizierten
`SAFETY_RECOVERY`-Erweiterungspunkt vorbereitet, aber ohne #35-Qualifikation
bleibt die produktive Recovery deaktiviert. Es werden keine Leistungs-, Puls-,
Trend-, Temperatur- oder Revisionswerte erfunden. Eine Recovery loescht den
S3-004-Latch nie automatisch und kann die normale PI-Freigabe nicht umgehen.

### 3.5 OD-24-06 und OD-24-07: minimale #15-Migration

Der bestehende #15-Commandpfad bleibt erhalten. Externes `FaultResetRequest`
enthaelt nur neutrale Befehlsdaten: `CommandEnvelope`, exaktes Faultziel und
die erwartete Faultrevision, sofern sie nicht schon im Envelope steht.

`FaultResetEvaluation` bleibt die fachliche Ergebnisprojektion, wird aber
zentral aus aktuellem Fault-/Latchzustand, Ursachenfreiheit, Sensor-/Aktor-,
Persistenz- und Integritaetschecks, blockierenden Fehlern, codebezogener
Resetpolicy und typisierter Autorisierungsevidenz berechnet. Positive Caller-
Felder sind keine Safetyautoritaet. Es gibt weder `bool
authorizationSatisfied=true` als Bypass noch Pointer-/Token-Capabilities.

Die reale PIN-Pruefung wird nicht in #24 neu implementiert. #24 definiert nur
eine schmale interne Evidenz, zum Beispiel einen typisierten
`FaultResetAuthorizationLevel`-Wert. Fehlende oder nicht passende Evidenz ist
fail-closed. Der spaetere UI-/Serviceproducer liefert die Evidenz ueber den
bestehenden Anwendungspfad; #24 haengt nicht von E4 ab.

### 3.6 OD-24-08: schmaler Resetport

Der auf dem aktuellen Branch bereits vorhandene
`lib/device_platform/src/reset_port.hpp` wird wiederverwendet; es wird kein
zweiter Port eingefuehrt. Der Port bleibt anwendungsneutral und nativ
testbar. Er beobachtet bootlokal eine stabile Resetursache und nimmt eine
kontrollierte Neustartanforderung mit kleinstmoeglichem neutralem Resultat an.
Er kennt weder Fermentationsbegriffe, Faultcodes, Restart-Episoden noch
`SAFE_BOOT`-Policy. `SimulatedResetController` bleibt die native Testhilfe.
ESP-IDF-Mapping und `esp_restart()` bleiben #29/E5.

### 3.7 OD-24-09: reale Application-Grenze

`CONFIGURATION_SAFETY_INTEGRATION_GATE` bleibt verpflichtender #24-Scope. Die
Integration sitzt im realen `fermentation_app`-Application-/Orchestrierungs-
pfad direkt unter `FermentationApplication`, nicht nur in einer isolierten
Testfixture. Eine zentrale Safetyinstanz konsumiert die echten oeffentlichen
#56/#57-Resultate und erzeugt daraus die bestehende #23-
`ActuatorSafetyGateInput`-Projektion. Planner und Sink bleiben der einzige
Aktorpfad.

`src/main.cpp` und `main/app_main.cpp` bleiben Skeletons und werden nicht mit
noch nicht vorhandenen ESP-IDF-, NVS- oder Hardwareadaptern aufgefuellt. Die
native End-to-End-Abnahme instanziiert trotzdem den echten
`FermentationApplication`-Pfad mit `ConfigurationRecoveryService`,
`ConfigurationSafetyIntegrationGate`, `ConfigurationRecoveryResult`,
`SafetyFaultService`, Planner und Testports. Ein test-only Ersatzmapper ist
nicht zulaessig. E5 wiederholt denselben Nichtumgehungsvertrag mit realen
Adaptern.

## 4. Finale Code-/Producer-/Policy-Matrix

Die folgenden Codes sind fuer Release 1 vorgeschlagen und in diesem R3 als
kanonischer Vertrag festgeschrieben. `Reset` bezeichnet den bewussten
Faultreset, nicht Quittierung. `SAFE_BOOT-Exit` bezieht sich nur auf einen
bereits aktiven SAFE_BOOT; ein normaler Reboot ist nie ein Exit.

### 4.1 P1 und O2

| Code - Producer/Ursache | Unmittelbare Reaktion | Latch / Auto-Rearm | Reset / Berechtigung | Reboot / SAFE_BOOT-Exit | Primaer-/Folgefaehigkeit |
|---|---|---|---|---|---|
| `P1-001` - #20/#22 langsame oder verfehlte Prozesszielerreichung | Warnung, Prozess nur bei weiterhin vollstaendiger Safetyfreigabe fortsetzen | kein Latch; keine Faultreset-Rearmaktion, Meldung endet nach neuer gueltiger Prozessbewertung | kein Faultreset; Quittierung ohne Freigabeaenderung | kein Reboot; kein SAFE_BOOT-Exit | Primaerfaehig; kann Folge einer Sensor-/Aktorstoerung sein, unterdrueckt deren Code nicht |
| `P1-002` - #20 degradierte, aber noch sichere Messqualitaet | Warnung und reduzierte Prozesskonfidenz; Safety-Sensorgrenze bleibt wirksam | kein Latch; Rueckkehr nur nach #20-validierter Qualitaet, nicht durch Zeitannahme | kein Faultreset; keine Serviceberechtigung | kein Reboot; kein SAFE_BOOT-Exit | Primaerfaehig; Folgebezug zu O2/S3 erlaubt |
| `P1-003` - nichtkritische Historien-/Journalpflege gestoert | Warnung, kritische Lauf- und Safety-Persistenz priorisieren | kein Latch, solange kritische Persistenz intakt ist; keine automatische Sicherheitsfreigabe | kein Faultreset; Quittierung/Neubewertung nach Speicherstatus | kein Reboot; bei kritischer Eskalation wird Y4-005/006 primaer | Primaerfaehig als Pflegewarnung; folgt bei Eskalation einem Y4-Systemfehler |
| `O2-001` - #20/#21 Produktfuehler stale/failed, validierter Luftfallback erlaubt | Peltier zunaechst AUS, Luft- und Sicherheitsfuehler pruefen, nur #21-Fallback verwenden | kein persistenter Latch; Auto-Rearm ausschliesslich gemaess konkreter #21-Policy | Bedienerreset oder #21-Policy nach Ursachefreiheit und Sicherheitschecks; keine Service-PIN | kein Reboot; kein SAFE_BOOT-Exit | Primaerfaehig; eskaliert bei fehlendem Fallback zu S3/Y4, ohne eigenen Code zu loeschen |
| `O2-002` - #20/#21 kurzzeitig stale bei Sicherheits-Sensorrolle | Peltier AUS, Luefternachlauf, begrenztes Wiedererkennungsfenster | kein persistenter Latch; automatische Rueckkehr nur vor `FAILED` nach stabilen gueltigen Messungen | kein manueller Reset waehrend Wiedererkennung; danach codebezogene Neubewertung | kein Reboot; kein SAFE_BOOT-Exit | Primaerfaehig; eine Eskalation zu S3-001/002 bleibt erhalten |
| `O2-003` - #20 ungeklaerte Sensorrollen-/Plausibilitaetsabweichung ohne bereits bewiesenen S3-Fall | Peltier AUS, keine Ersatzfreigabe bei unklarer Sicherheitslage, neue Evidenz anfordern | kein Auto-Rearm; nur neue eindeutige Evidenz kann den Zustand verlassen | Bedienerreset erst nach neuer Rollen-, Plausibilitaets- und Safetypruefung; keine Service-PIN fuer den O2-Fall | kein Reboot; kein SAFE_BOOT-Exit | Primaerfaehig; bei Persistenz wird S3-003 primaer, O2 bleibt als Folge sichtbar |
| `O2-004` - #23 fehlende thermische/Aktorreaktion beim ersten begrenzten Nachweis | Peltier AUS, Diagnose und nur durch #23 erlaubte begrenzte Neubewertung | kein pauschales Auto-Rearm; Wiederholung ist begrenzt und darf zu S3-008/009 eskalieren | Bedienerreset nach frischer Ursachefreiheits-, Sensor- und Aktorpruefung; keine Service-PIN fuer den ersten O2-Fall | kein Reboot; kein SAFE_BOOT-Exit | Primaerfaehig; Watchdog-/Elektrikfolge wird nicht unterdrueckt |

### 4.2 S3

Alle S3-Codes sind persistent gelatcht, werden nicht automatisch rearmed und
verlangen die jeweils genannte Service-/technische Evidenz. Ein Faultreset
loescht den Latch nur nach Ursachenfreiheit, allen codebezogenen Checks,
Integritaetspruefung und fehlenden gleich- oder hoeherklassigen Blockern.

| Code - Producer/Ursache | Unmittelbare Reaktion | Latch / Auto-Rearm | Reset / Berechtigung | Reboot / SAFE_BOOT-Exit | Primaer-/Folgefaehigkeit |
|---|---|---|---|---|---|
| `S3-001` - #20 Schrankluftfuehler `FAILED` | Peltier und beide H-Bruecken AUS, sicherer Aussenluefter-Nachlauf | persistenter Latch; kein Auto-Rearm | erlaubt nach stabiler Sensorvalidierung und Safetychecks; Service-PIN | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; Folgefehler werden separat gespeichert |
| `S3-002` - #20 Aussenwaermetauscher-/Kuehlkoerperfuehler `FAILED` | Peltier und beide Richtungen AUS, Waermeabfuhr gemaess sicherer Fanstrategie | persistenter Latch; kein Auto-Rearm | erlaubt nach stabiler Sensor-/Aktorpruefung; Service-PIN | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; Folgefehler unterdruecken die Sperre nicht |
| `S3-003` - #20 persistenter oder sicherheitsrelevanter Sensorwiderspruch | Peltier AUS, kein Fallback bei ungeklaerter Sicherheitslage | persistenter Latch; kein Auto-Rearm | erlaubt erst nach Ursachenfreiheit, Rollen-/Plausibilitaetsnachweis und Service-PIN | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; O2-003 bleibt als vorausgehende Folge sichtbar |
| `S3-004` - #20/#22 Sicherheits-Eingriffsgrenze | aktuelle Leistung AUS, Richtung sperren, Impuls/Integrator verwerfen, Luefterstrategie ausfuehren | persistenter Latch; keine produktive Auto-Recovery; #35 fehlt => `SAFETY_RECOVERY` unaufloesbar | Service-PIN nach Ursachefreiheit und Checks; Reset erzeugt keine Recovery und loescht den Latch nicht automatisch | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; Recoveryversuch darf Folgefehler erzeugen, aber nie diesen Latch loeschen |
| `S3-005` - #20/#22 harte thermische Notgrenze | sofort AUS, keine Gegenrichtung, sichere Luefter-/Restwaermebehandlung | persistenter Latch; kein Auto-Rearm | nur technische Serviceberechtigung nach Grenz- und Hardwarepruefung | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; alle Folgefehler bleiben erhalten |
| `S3-006` - #20/#23 externer Luefterausfall | Peltier AUS, erforderliche Restwaermeabfuhr fail-closed behandeln | persistenter Latch; kein Auto-Rearm | Service-PIN nach realer Fan-/Ausgangspruefung | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; thermische Folgecodes werden nicht verschluckt |
| `S3-007` - #20/#23 interner Luefterausfall | Peltier AUS oder richtungsbezogen sperren, Fanreaktion sicher begrenzen | persistenter Latch; kein Auto-Rearm | Service-PIN nach Fan-/Ausgangspruefung | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; thermische Folgecodes bleiben sichtbar |
| `S3-008` - #23 `ActuatorWatchdogFaultEvidence` | Peltier AUS, Planner-Safety-Gate auf Stop, vollstaendige 64-bit-Evidenz sichern | persistenter Latch; kein Auto-Rearm | Service-/technische Berechtigung nach #23-Evidenz-, Sensor- und Aktorchecks | kein zusaetzlicher Reboot standardmaessig; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; jede elektrische/thermische Folge bleibt separat |
| `S3-009` - #23 H-Bruecken-, Strom-, Ausgangs- oder Richtungsfehler | beide Richtungen AUS, Aktorplan verwerfen, sichere Luefterstrategie | persistenter Latch; kein Auto-Rearm | technische Serviceberechtigung nach Ausgangs-/H-Brueckenpruefung | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; kann Folge von O2-004 sein, unterdrueckt ihn nicht |

### 4.3 Y4

| Code - Producer/Ursache | Unmittelbare Reaktion | Latch / Auto-Rearm | Reset / Berechtigung | Reboot / SAFE_BOOT-Exit | Primaer-/Folgefaehigkeit |
|---|---|---|---|---|---|
| `Y4-001` - #56 `ConfigurationRuntimeFailure` | keine neue Konfigurationsfreigabe, Aktoren sicher stoppen, gueltige letzte Revision nur nach #56-Regel | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach gueltiger Konfiguration, Persistenz- und Integritaetschecks | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; Folgefehler bleiben einzeln erhalten |
| `Y4-002` - #56/#57 `ConfigurationCommitIndeterminate` oder `CommitOutcomeUnknown` | Anwendung der unklaren Revision sperren, Aktoren sicher stoppen, Ergebnis klaeren | persistenter System-Latch; kein Auto-Rearm | Service/technisch erst nach eindeutigem Commitstatus und neuer Revision | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; darf keine #56/#57-Resultate ueberschreiben |
| `Y4-003` - #57 `ConfigurationUnavailable` | keine unvollstaendige Konfiguration verwenden, Aktoren sicher stoppen | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach validierter Konfigurationsrevision und allen Safetychecks | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; Folgen aus Run-/Sensorpfad bleiben erhalten |
| `Y4-004` - #57 `ConfigurationIntegrityFailure` | Integritaetsfehler fail-closed, keine teilweise nutzbare Konfiguration, Aktoren AUS | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach Integritaets- und Persistenzpruefung | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; Folgefehler werden nicht zusammengelegt |
| `Y4-005` - #17/#18 kritischer Laufcheckpoint nicht atomar oder nicht rekonstruierbar | Peltier/H-Bruecken AUS, Lauf sicher beenden oder nicht wiederherstellbar markieren | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach neuer gueltiger Laufrevision und Rekonstruktionspruefung | kein zusaetzlicher Reboot; kein SAFE_BOOT-Exit durch Reset | Primaerfaehig; Folgefehler behalten ihre Ursache |
| `Y4-006` - #24 Safety-State-Read/Write/Commit/Capacity/Integrity unklar | RAM-Latch und Aktorsperre sofort setzen, minimalen Latch versuchen, bei Unsicherheit `SAFE_BOOT` | persistenter System-Latch; kein Auto-Rearm; kein Evict aktiver Latches | nur Service/technisch nach Lesen-Schreiben-, Kapazitaets- und Integritaetspruefung | kein zusaetzlicher Reboot; SAFE_BOOT-Exit nur ueber separate autorisierte Bootpolicy, nicht diesen Reset | Primaerfaehig; Folgefehler werden nicht zur Entwarnung verwendet |
| `Y4-007` - #24 interner Software-/Safety-Task-Watchdog oder fehlgeschlagene Recovery | sichere Aktorabschaltung, Persistenzversuch, kontrollierten technischen Restart nur nach Policy vorbereiten | persistenter System-Latch; kein Auto-Rearm | technische Berechtigung nach Ursachenfreiheit und vollstaendiger Integritaetspruefung | zusaetzlicher kontrollierter Neustart ist fuer diesen Code nach Reparatur erforderlich; SAFE_BOOT-Exit nur mit autorisiertem technischem Boot, nie normal | Primaerfaehig; #23-S3-008 bleibt ein eigener Folge-/Ursachencode |
| `Y4-008` - unbekannter Resetgrund, Evidenzmismatch oder unbekannter Safetyinput | `Allowed` verbieten, Peltier/H-Bruecken AUS, Diagnose und Persistenz fail-closed | persistenter System-Latch; kein Auto-Rearm | Reset bis zur geklaerten Ursache verboten; danach technische Berechtigung und alle Checks | kein Reboot als Loesung; kein SAFE_BOOT-Exit solange Ursache/Evidenz ungeklaert | immer primaer als Unresolved; Folgefehler bleiben erhalten |
| `Y4-009` - dritte abnormale Restartbeobachtung / erzwungener `SAFE_BOOT` | vor normaler Aktor-/Lauffreigabe `SAFE_BOOT`, alle Aktoren AUS | persistente Episode-/Systemverriegelung; kein Auto-Rearm durch Neustart | technischer Service nach Ursachenfreiheit, Persistenz-/Integritaets-/Sensor-/Aktorpruefung | zusaetzlicher kontrollierter Neustart ist fuer den autorisierten SAFE_BOOT-Exit erforderlich; normaler Reboot bleibt wirkungslos | Primaerfaehig; ausloesende S3/Y4-Ursachen bleiben als eigene Primaer-/Folgefehler erhalten |

## 5. Fehlerlebenszyklus, Dominanz und Ereignisse

- `ACTIVE_UNACKNOWLEDGED`, `ACTIVE_ACKNOWLEDGED`,
  `CAUSE_CLEARED_LOCKED` und `CLEARED` bleiben getrennte Zustaende.
- Quittierung beendet nur die Meldungswiederholung. Sie veraendert weder
  Ursache, Latch noch Aktorfreigabe.
- Klasse 4 dominiert Klasse 3, diese Klasse 2 und diese Klasse 1. Innerhalb
  einer Klasse entscheidet die stabile Codeprioritaet in der obigen Matrix;
  bei Gleichstand bleibt die fruehere diagnostische Entstehungssequenz
  Primaerfehler.
- Mehrere gleichzeitig aktive Ursachen werden alle bounded retained. Ein
  Folgefehler darf nur die Beziehung markieren, nicht seine eigene unmittelbare
  Safetyreaktion verlieren.
- Fault- und Ereignisrevisionen muessen vor dem Hochzaehlen auf Kapazitaet
  geprueft werden. Kein stiller Wraparound.
- `FaultInstanceId`, monotoner Zeitbezug, Producer-/Correlation-Schluessel,
  Status, Latch, Ursache, Reset- und Recoveryentscheidung sowie Primary-
  Bezug bleiben fuer aktive Records nachvollziehbar.
- `Unknown` oder `Unresolved` wird nicht normalisiert, um eine vermeintlich
  gueltige niedrige Klasse zu erhalten; der Pfad erzeugt Y4-008 bzw. ein
  gleichwertiges fail-closed Capacity-/Persistenzereignis.

Mindestens zu projizierende Ereignisse sind Erstellung, Eskalation,
Ursachenfreiheit, Quittierung, Resetversuch, Resetablehnung, Restart-
Beobachtung, Episodefortschritt, Episodenschluss, SAFE_BOOT-Eintritt,
SAFE_BOOT-Exitentscheidung, Recoveryversuch, Recoveryablehnung,
Recoveryabbruch und Recoveryerfolg. Die Aufbewahrung bleibt #19.

## 6. Persistenz, Kapazitaetsnachweis und Overflow

### 6.1 Worst Case

Aktive Cleared-Historie wird nicht in die Latchkapazitaet gerechnet. Die
maximal sinnvoll gleichzeitig aktiven S3-Instanzen sind:

1. Schrankluftfehler,
2. Aussenwaermetauscher-/Kuehlkoerperfehler,
3. Sensorwiderspruch,
4. eine der thermischen Grenzen (S3-004 oder S3-005, gegenseitig exklusive
   aktuelle Grenzrichtung),
5. externer Luefterfehler,
6. interner Luefterfehler,
7. Aktorwatchdogfehler,
8. H-Bruecken-/Strom-/Ausgangsfehler.

Die Y4-Matrix erlaubt je einen aktiven bounded Producer-/Ursachensatz fuer
vier getrennte #56/#57-Konfigurationsausgaenge, einen kritischen
Laufcheckpoint, den Safety-State, den internen Safety-Watchdog, unbekannte
Reset-/Safety-Evidenz und die Restart-Episode. Das sind neun Y4-Instanzen.
Mehrere unabhaengige Instanzen derselben Ursache werden nur zugelassen, wenn
der konkrete Producer getrennte Rollen, Quellen oder Correlation-Schluessel
gleichzeitig liefern kann. Die Bound darf bei einer neuen Producerdomane nicht
still ueberschritten werden; sie erfordert eine neue Planpruefung.

Damit gilt fuer Release 1:

```text
S3 worst case:  8
Y4 worst case:  9
aktive Latches: 17
Cleared-Historie: nicht enthalten
```

### 6.2 Recordgroesse

Der vorhandene feste Wire-Vertrag besitzt 80 Byte Basispayload und 48 Byte je
Latchrecord. Ohne UTC umfasst der Envelope 37 Byte einschliesslich CRC.

```text
Payload = 80 + 17 * 48 = 896 Byte
Record  = 37 + 896 = 933 Byte
Grenze  = 2048 Byte
Reserve = 2048 - 933 = 1115 Byte
```

Die Bound liegt damit innerhalb des vorhandenen 2048-Byte-Records und der
Release-1-Grenzen von 4 MB Flash ohne PSRAM. `TBD_IMPLEMENTATION_BUDGET`
bleibt fuer den spaeteren realen Ressourcenbericht sichtbar; die Rechnung ist
kein Hardware- oder Buildnachweis.

Die spaetere Implementierung setzt die Bound zentral und compile-time fest,
beispielsweise als `kMaximumPersistedLatches = 17`, mit einer statischen
Pruefung fuer `896 + 37 <= 2048`. Keine zweite Konstante in Testfixture oder
Adapter darf die Bound abweichend definieren.

### 6.3 Fail-closed Capacity-/Persistenzverhalten

- Vor jeder Mutation wird geprueft, ob ein neuer aktiver Record, eine Revision
  oder eine Instance-ID noch darstellbar ist.
- Bei voller Kapazitaet wird kein aktiver Record verdraengt, zusammengelegt
  oder als Cleared umetikettiert.
- Der neue Fehler wird als Y4-006 beziehungsweise als unaufgeloester
  Capacity-Fall behandelt; die RAM-Safety-Sperre und Aktorabschaltung haben
  Vorrang.
- Ein minimaler persistenter Latch wird versucht. Schreibfehler,
  Rueckleseunsicherheit, CRC-/Schemafehler oder ungeklaerte Transaktion bleiben
  fail-closed wirksam und fuehren beim naechsten unklaren Boot zu `SAFE_BOOT`.
- Ein einzelner spaeter erfolgreicher Schreibversuch, Quittierung oder normaler
  Neustart ist keine Entwarnung.
- Cleared-Historie darf nur ausserhalb der aktiven Latchbound und gemaess #19
  behandelt werden; sie kann nie einen aktiven Slot freigeben, bevor die
  fachliche Latchbedingung geloest und persistiert ist.

## 7. Restart- und SAFE_BOOT-Ablauf

Der bootlokale Ablauf ist:

```text
Boot
  -> alle Aktorausgaenge sicher AUS
  -> Resetursache genau einmal ueber IResetController beobachten
  -> Ursache als normal, klassifiziert abnormal oder Unknown pruefen
  -> Safety-/Konfigurations-/Persistenz-/Integritaetsstatus laden
  -> offene S3/Y4-Latches und Restart-Episode bewerten
  -> bei drittem abnormalem Restart SAFE_BOOT setzen
  -> reale #56/#57-Resultate ueber die Application-Gategrenze zufuehren
  -> Safetyprojektion erzeugen
  -> nur bei vollstaendigem und autorisiertem Gate weiter zum Planner
```

Ein erfolgreicher Boot startet die monotone Stabilitaetsmessung. Erst nach 30
Minuten ununterbrochenem, abnormal-restartfreiem Betrieb wird die Episode
geschlossen und als neue Revision persistiert. Power-off liefert keine
Stabilitaetszeit. Ein unbekannter Reset verursacht keine normale Freigabe.

Ein kontrollierter Neustart wird nur aus der codebezogenen Matrix angefordert.
`Y4-007` und `Y4-009` sind die in R3 vorgesehenen Faelle, in denen ein
zusatzlicher technischer Neustart nach Reparatur beziehungsweise fuer den
autorisierten SAFE_BOOT-Exit erforderlich ist. Alle anderen Codes verwenden
keinen generischen Bootintent.

## 8. Konkrete #15/#24-API-Migration

Die Umsetzung nach Planfreigabe betrifft diese vorhandenen Dateien und
Konsumenten:

- `lib/fermentation_app/src/run_commands.hpp/.cpp`: neutrales
  `FaultResetRequest` nur mit `CommandEnvelope` und Ziel; erwartete Revision
  aus dem Envelope; `FaultResetEvaluation` bleibt Ergebnis, nicht Eingabe;
  positive Evaluation-Felder, boolescher Bypass, private Token-/Pointer-
  Autorisierung und die legacy-Bool-Ueberladung werden entfernt.
- `lib/fermentation_app/src/safety_fault_service.hpp/.cpp`: zentrale
  Auswertung aller Ursachen-, Sensor-, Aktor-, Persistenz-, Integritaets- und
  Blockerchecks, codebezogener Reset-/Rebootpolicy und schmaler typisierter
  Autorisierungsevidenz; fehlende Evidenz ergibt Ablehnung.
- `test/test_run_commands/test_run_commands.cpp` und
  `test/test_issue24_safety/test_issue24_safety.cpp`: nach R3-SHA gezielte
  Konsumenten- und Negativtests der neuen Signaturen; in diesem Auftrag nicht
  geaendert.
- `docs/RUN_COMMANDS.md`: neutraler Request und zentral berechnete Evaluation.

Die Bewertung wird vor der Anwendung erzeugt und bleibt bis zur erwarteten
Faultrevision gueltig. Caller liefern keine Safetyentscheidung. Ein
autorisierter normaler Service-/Recovery-Neustart wird nicht durch einen
Faultreset in einen abnormalen Restart umklassifiziert.

## 9. S3-004-Contract und #23-Gate

S3-004 erzeugt sofort `ImmediateStop` und einen persistenten Latch. Der
vorhandene #23-Pfad fuer `ActuatorSafetyGateInput` bleibt die einzige Grenze:

- ohne vollstaendige #35-Qualifikation ist `SAFETY_RECOVERY` `Unresolved` und
  liefert keine aktive Gegenrichtung;
- ein normales `Allowed`-Request kann diesen Zustand nicht umgehen;
- ein spaeterer qualifizierter Recoveryproducer muss durch den vorhandenen
  Gate-/Plannervertrag laufen und darf keine normale PI-Freigabe direkt setzen;
- native Tests beweisen fehlende #35-Qualifikation => keine aktive Recovery,
  `Allowed` => kein Bypass und Recovery => S3-004-Latch bleibt aktiv;
- echte thermische Recovery mit realen Parametern gehoert erst in den
  abhaengigen #35-Commissioning-/Integrationspfad.

## 10. Reale #56/#57-Integration in der Application

Der minimale Integrationspunkt wird direkt unter `FermentationApplication`
festgelegt. Die vorhandene `ConfigurationSafetyIntegrationGate` wird als
Application-Orchestrierungsgrenze wiederverwendet, nicht als Testfixture:

1. `FermentationApplication::begin(...)` initialisiert beziehungsweise bindet
   genau eine `SafetyFaultService`-Instanz und die vorhandene
   `ConfigurationSafetyIntegrationGate`-Instanz.
2. Beim Boot wird `ConfigurationSafetyIntegrationGate::boot()` ausgefuehrt;
   der echte `ConfigurationRecoveryResult` wird in den zentralen Safetykern
   forwarded.
3. Laufende echte #56/#57-Resultate werden ueber dieselbe Grenze konsumiert;
   kein zweiter Mapper erzeugt eine parallele Safetyentscheidung.
4. `SafetyFaultService` projiziert den Gesamtzustand auf den bestehenden
   `ActuatorSafetyGateInput` und uebergibt ihn an `ActuatorPlanner::tick`.
5. Der Sink wendet ausschliesslich den Planneroutput an. `Allowed` ist nur bei
   vollstaendig geklärtem Safetyzustand moeglich.

Die konkrete C++-Signatur oder ein kleiner Contexttyp wird im freigegebenen
Implementierungsschnitt aus den bestehenden `FermentationApplication`-
Konsumenten abgeleitet. Das ist keine neue Ownerentscheidung und kein neuer
ADR. Der Composition Root bleibt frei von nicht vorhandenen Hardware- und
NVS-Adaptern.

## 11. Port- und Modulgrenzen

Der bestehende `IResetController`-Port wird nur so weit angepasst, wie die
neutrale Resultatsemantik es erfordert:

- `ResetCauseSnapshot` ist bootlokal stabil, hat eine Validitaetsanzeige und
  behandelt Unknown explizit;
- `requestRestart` liefert nur ein neutrales Annahme-/Ergebnisresultat;
- der Port kennt keine `FaultCode`, Klasse, Episode, SAFE_BOOT-Entscheidung,
  Sensoren, Aktoren oder Fermentationsbegriffe;
- native Tests verwenden den vorhandenen simulierten Controller;
- ESP-IDF-Mapping und `esp_restart()` bleiben ausserhalb von #24.

Die Application-Fachlogik bleibt in `fermentation_app`. Kein konkreter
ESP-IDF-Typ, keine GPIO-Information und kein Netzwerk-/Dateisystemzugriff
wandert in diesen Kern.

## 12. Implementierungsschnitt nach Ownerfreigabe

Die Umsetzung erfolgt erst nach Freigabe genau dieser R3-Commit-SHA und nach
erneuter Pruefung von Branch, lokalem/remote/PR-HEAD und Plan-SHA. Vorgesehen
sind in zusammenhaengenden, reviewbaren Schnitten:

1. zentrale Code-/Policy-/Latch-/Wire-Vertraege und die R3-Resetsemantik;
2. minimale #15-API-Migration und zentrale Safety-Auswertung;
3. Application-Grenze fuer reale #56/#57-Resultate und #23-Projektion;
4. neutrale Resetport-Konsumenten ohne ESP-IDF-Adapter;
5. S3-004-Contract-only-Gate und fail-closed Capacity-/SAFE_BOOT-Pfad;
6. gezielte native Konsumenten-, Application-End-to-End- und
   Negativ-/Fehlerinjektionstests.

Bei jeder materiellen Abweichung von diesem Plan wird die Umsetzung gestoppt,
der Plan aktualisiert, neu committed und erneut zur Ownerfreigabe vorgelegt.

## 13. R3-Akzeptanzorakel und Grenzen der aktuellen Pruefung

Nach Implementierung muessen native Tests mindestens beweisen:

- alle Codes und Producer-Mappings sind sprachunabhaengig und lueckenfrei
  gemaess Abschnitt 4; Unknown/Unresolved bleibt fail-closed;
- alle acht S3- und neun Y4-Worst-Case-Latches koexistieren ohne Eviction;
  Cleared-Historie wird nicht als aktive Kapazitaet gerechnet;
- 17 Records, 933 Byte Wiregroesse und 2048-Byte-Grenze werden statisch und
  dynamisch gegen Overflow geprueft;
- dritter abnormaler Restart => `SAFE_BOOT`, monotone 30-Minuten-Stabilitaet
  schliesst die Episode, Power-off schliesst sie nicht, normaler Reboot verlaesst
  `SAFE_BOOT` nicht, Unknown ist nicht normal;
- kontrollierter normaler Service-/Recovery-Reboot wird nur bei
  codebezogener Safety-/Softwareklassifikation abnormal gezaehlt;
- Quittierung, Reset, Auto-Rearm, zusaetzlicher Reboot und SAFE_BOOT-Exit
  folgen exakt der Code-/Policy-Matrix;
- positive Caller-Evaluation, boolesche Autorisierung und direkte
  Planner-/Sink-Anfragen koennen keinen Safetybypass erzeugen;
- fehlende Autorisierung, Persistenz-/Integritaetsunsicherheit und ungeklaerte
  Faultrevision werden abgelehnt;
- S3-004 bleibt ohne #35-Qualifikation `ImmediateStop`, ohne aktive Recovery,
  ohne PI-Bypass und ohne automatisches Latchloeschen;
- der echte `FermentationApplication`-Pfad konsumiert echte #56/#57-
  Ergebnistypen, eine zentrale Safetyinstanz und den realen #23-Planer-/Sinkpfad;
  kein test-only Ersatzmapper besteht die Abnahme;
- Primary-/Follow-up-Beziehungen unterdruecken nie die eigene Safetyreaktion.

Die historische R2-Suite bleibt in `docs/ACCEPTANCE_TESTS.md` klar als
`NOT_ACCEPTED_PENDING_R3` markiert. In diesem Planungsauftrag werden native
Suiten, ESP-IDF-Builds, Hardwaretests und Remote-CI nicht ausgefuehrt.

## 14. Kanonische Dokumentationssynchronisierung

Dieser Dokumentationscommit synchronisiert mindestens:

- `docs/SYSTEM_SAFETY_AND_RECOVERY.md`: 3-Restart-Episode, 30 Minuten
  monotone Stabilitaet, Power-off loescht nicht, normaler Reboot ist kein
  SAFE_BOOT-Exit;
- `docs/SAFETY_AND_FAULTS.md`: vollstaendige Code-/Producer-/Policy-Matrix
  und codebezogene Reset-/Rebootregeln;
- `docs/SAFETY_COMPONENT_FAULTS.md`: S3-004 contract-only bis #35, fail-closed
  Gate und keine Commissioningwerte;
- `docs/ACCEPTANCE_TESTS.md`: R3-Zielorakel getrennt von historischen R2-
  Laeufen, ohne ausgefuehrte Ergebnisse vorwegzunehmen;
- `docs/ROADMAP.md`: nur Status und naechstes Gate, keine duplizierten
  Anforderungen;
- `docs/RUN_COMMANDS.md`: neutrale #15-Requestform und zentrale #24-
  Evaluation.

Kein ADR wird als `accepted` markiert. Ein ADR-Entwurf wird nur dann ein
Owner-Gate, wenn die Detailpruefung nach Freigabe tatsaechlich eine neue
langfristige Architekturentscheidung ausserhalb ADR-013/014/018 aufdeckt.

## 15. Aktueller Dokumentationsabschluss und Owner-Gate

Fuer diesen Auftrag zulaessig und vorgesehen:

- `git diff --check`;
- Secret-Scan gemaess `docs/CI_AND_QUALITY_GATES.md`;
- notwendige Dokumentations- und Architektur-Konsistenzchecks;
- ein ausschliesslicher Dokumentationscommit und normaler Push auf den
  bestehenden Branch;
- PR #107 aktualisieren: exakte Plan-/Commit-SHA und Status
  `PLAN_R3_PENDING_OWNER_APPROVAL`, Draft unveraendert;
- genau ein neuer aktueller SESSION HANDOVER.

In diesem Auftrag ausdruecklich `NOT_RUN`:

- vollstaendige Native-Suite und Firmwaretests;
- ESP-IDF-Builds, PlatformIO-Builds und Toolchain-/Ressourcenabnahme;
- Remote-CI;
- Hardware-, Bring-up- und thermische Tests;
- Produktions-C++, Test-, Adapter- oder Hardwareaenderungen.

Nach Eintragung der exakten neuen R3-SHA lautet der naechste und einzige
Schritt: Ownerreview und Freigabe dieser SHA. Danach wird angehalten. Es gibt
keinen Ready-for-review-Wechsel, Merge, Auto-Merge, Issue-Abschluss,
Branchloeschung oder Force-Push durch den Agenten.
