# R1-RAM – S8 Auswertung (Analyse und Entscheidungsvorbereitung)

Reine Auswertung der vorhandenen Evidenz S2–S7; **keine neue Messung, keine
Codeänderung, keine neue Optimierung**. Quellen: S6-Messreihe
([R1_RAM_S6_HW_CAMPAIGN_EVIDENCE.md](R1_RAM_S6_HW_CAMPAIGN_EVIDENCE.md),
[Punkte-CSV](R1_RAM_S6_HW_CAMPAIGN_20261003_POINTS.csv)), die Pre-S6-Reihen
([R1_RAM_CLEAN_NETWORK_SWITCH_EVIDENCE.md](R1_RAM_CLEAN_NETWORK_SWITCH_EVIDENCE.md),
[R1_RAM_CLIENT_LOAD_COMMIT_EVIDENCE.md](R1_RAM_CLIENT_LOAD_COMMIT_EVIDENCE.md)),
[R1_RAM_S4_HARDWARE_REMEASURE_EVIDENCE.md](R1_RAM_S4_HARDWARE_REMEASURE_EVIDENCE.md),
[R1_RAM_S7_SOURCE_AUDIT.md](R1_RAM_S7_SOURCE_AUDIT.md). Alle Zahlen sind
Messwerte; Empfehlungen sind Empfehlungen an Review und Owner.

```text
S3=PASS
S4=PASS
S5=NO_ACTION_OWNER_DECISION
S6=PASS_CODE_AND_HARDWARE
S7=NO_ACTION
S8=EVALUATION_FIXED_PENDING_INDEPENDENT_VERIFICATION

FATAL_R1_NETWORK_COMMIT_RAM_BLOCKER=CLOSED_MEASURED_SCOPE
FATAL_R1_NETWORK_COMMIT_RAM_BLOCKER_SCOPE=RELEASE_PROFILE_ONE_CLIENT_5_OF_5_MATRIX_CASES_MARGIN_NOT_PROVEN_FOR_MORE_CLIENTS_OR_MORE_SWITCHES
MAKE_REPRESENTATIVE_SCREEN_OOM=CLOSED

LOWEST_NETWORK_MODE_SWITCH_LOW_WATER_MARK_BYTES=4124
NETWORK_MODE_SWITCH_LOW_WATER_SUBPHASE=NOT_INSTRUMENTED
LOWEST_MAIN_STACK_HWM_BYTES=5936
S6_LOWEST_MAIN_STACK_HWM_BYTES=6256
LOWEST_CURRENT_LARGEST_FREE_BLOCK_BYTES=7168

MEASURED_CURRENT_FREE_HEAP_FLOOR_BYTES=12288
MEASURED_CURRENT_INTERNAL_8BIT_FREE_FLOOR_BYTES=12288
MEASURED_CURRENT_DMA_FREE_FLOOR_BYTES=12288
MEASURED_CURRENT_LARGEST_8BIT_BLOCK_FLOOR_BYTES=7168
MEASURED_CURRENT_INTERNAL_8BIT_LARGEST_BLOCK_FLOOR_BYTES=7168
MEASURED_CURRENT_DMA_LARGEST_BLOCK_FLOOR_BYTES=7168
MEASURED_PRE_NETWORK_COMMIT_CLIENT_FREE_HEAP_FLOOR_BYTES=12288
MEASURED_PRE_NETWORK_COMMIT_CLIENT_INTERNAL_8BIT_FREE_FLOOR_BYTES=12288
MEASURED_PRE_NETWORK_COMMIT_CLIENT_DMA_FREE_FLOOR_BYTES=12288
MEASURED_PRE_NETWORK_COMMIT_CLIENT_LARGEST_8BIT_BLOCK_FLOOR_BYTES=8192
MEASURED_PRE_NETWORK_COMMIT_CLIENT_INTERNAL_8BIT_LARGEST_BLOCK_FLOOR_BYTES=8192
MEASURED_PRE_NETWORK_COMMIT_CLIENT_DMA_LARGEST_BLOCK_FLOOR_BYTES=8192

IDLE_1696B_FAILURE=OPEN
IDLE_1696B_CLASSIFICATION=FOLLOW_UP_NON_BLOCKING_FOR_PR174
IDLE_1696B_FOLLOW_UP=DIAGNOSE_BEFORE_PR170_NETWORK_EXPANSION

S9_RECOMMENDATION=NOT_NEEDED_FOR_PR174
S9_FUTURE_CONDITION=ONLY_IF_CALLER_DIAGNOSIS_PROVES_WIFI_LWIP_MEMORY_PROFILE_RELEVANT
S10_RECOMMENDATION=NOT_NEEDED
S11_RECOMMENDATION=NOT_NEEDED

O4=OWNER_DECISION_REQUIRED
O5=OWNER_DECISION_REQUIRED

PR170_DEPENDENCY=KEEP_BLOCKED_ON_IDLE_1696B_DIAGNOSIS_OR_EXPLICIT_OWNER_WAIVER
PR174_FINAL_GO=NO_UNTIL_OWNER_O4_O5
```

## A. Gemessene R1-Reserve nach S6

Drei Größen werden getrennt geführt: **aktueller freier Heap**, **aktueller
größter freier Block** und die **historische Tiefstmarke
`minimum_free_heap`** (Low-Water-Mark seit Boot). Quelle: Release-Build
`0beafdf`, ein Boot, ~84 min, Heimnetz-Start.

| Messpunkt | aktueller freier Heap | aktueller größter Block | `minimum_free_heap` (Low-Water-Mark) |
|---|---:|---:|---:|
| `after_ui_init` | 20468 B | 14848 B | 13228 B |
| `stable_home_wifi` (Boot) | 18624 B | 14848 B | 13228 B |
| `stable_home_wifi` (nach Wechseln, 5 Punkte) | 18888–19308 B | 10240 → 7168 B | 7880 → 4124 B |
| `stable_ap_only` ohne Client (5 Punkte, jeweils vor dem Client-Beitritt) | 15284–15744 B | 12800 → 8192 B | 10972 → 4124 B |
| `network_page_press_before` ohne Client (A, B) | 15844 / 15936 B | 12800 / 10752 B | 10972 / 7880 B |
| `network_page_press_before` mit Client (C, D, E) | 12620 / 12456 / 12288 B | je 8192 B | 7856 / 4124 / 4124 B |
| `network_page_press_after` (`HOME_WIFI`, 5 Commits) | 19472–20260 B | 10240 → 7168 B | 7880 → 4124 B |

Ein eigener Messpunkt `stable_ap_only` *mit* Client existiert nicht (der
Beitritt erzeugt keinen Punkt); der Zustand mit Client ist durch
`network_page_press_before` der Fälle C–E erfasst.

Extremwerte der Serie:

| Größe | Wert | Ort |
|---|---:|---|
| niedrigster **aktueller freier Heap** | 12288 B | `network_page_press_before`, Fall E (Client) |
| niedrigster **aktueller größter Block** | 7168 B | `network_page_press_after`, Fall E (`HOME_WIFI`) |
| **Low-Water-Mark** (`minimum_free_heap`) | 4124 B | während des synchronen Netzwerkmoduswechsel-Fensters von Fall C (Client), danach nicht mehr unterschritten; die Teilphase (Configuration-Commit oder Transport-/Lifecycle-Aktivierung) ist nicht instrumentiert |
| niedrigster Main-Task-Stack-HWM | **5936 B** über alle Läufe S2–S6 (Pre-S6, Clean-Lauf); in der S6-Serie 6256 B (von 24576 B) | S6-Serie ab erstem `AP_ONLY` |
| LVGL-Pool maximal belegt | 15856 B (24 %, Pool 63384 B) | unverändert gegenüber S2–S4 |

Der Heap-Gewinn durch S6 beträgt gemessen +4,8…5,0 kB beim freien Heap
(Details in der S6-Evidenz).

### Gemessene Böden getrennt nach Capability-Sicht

Aus der S6-Punkte-CSV (alle 40 Punkte bzw. die drei Client-Pre-Commit-Punkte
C, D, E). INTERNAL|8BIT und DMA sind überlappende Sichten desselben internen
DRAM (kein PSRAM) und werden nicht addiert; sie stimmen in dieser Serie in
jedem Punkt überein.

| Boden | allgemein (8BIT) | INTERNAL\|8BIT | DMA | Ort |
|---|---:|---:|---:|---|
| aktueller freier Heap, alle Punkte | 12288 B | 12288 B | 12288 B | `network_page_press_before`, Fall E |
| aktueller größter Block, alle Punkte | 7168 B | 7168 B | 7168 B | `network_page_press_after`, Fall E |
| freier Heap unmittelbar vor dem Moduswechsel mit Client (C, D, E) | 12288 B | 12288 B | 12288 B | Fall E |
| größter Block unmittelbar vor dem Moduswechsel mit Client (C, D, E) | 8192 B | 8192 B | 8192 B | Fälle C, D, E |

Das sind gemessene Böden des Scopes dieser Serie und keine zusätzlichen
Sicherheitsabstände.

## B. Configuration-Commit

```text
PRE_S6_CLIENT_COMMIT_ABORT=3_OF_3
POST_S6_CLIENT_COMMIT_PASS=3_OF_3
POST_S6_ALL_MATRIX_CASES=5_OF_5_PASS
POST_S6_COMMIT_OOM=0
LOWEST_POST_S6_NETWORK_MODE_SWITCH_LOW_WATER_MARK=4124_B
```

**Zur Benennung von 4124 B:** `network_page_press_before` wird vor
`processWorkspaceTouch()`, `network_page_press_after` danach geloggt. Dazwischen
liegt beim Netzwerkmoduswechsel nicht nur `ConfigurationService::confirmPreview()`,
sondern anschließend auch die Transport-/Network-Lifecycle-Aktivierung. 4124 B
ist daher die Low-Water-Mark des **gesamten synchronen
Netzwerkmoduswechsel-Fensters**, kein exklusiv dem Configuration-Commit
zuzuordnender Wert; die Teilphase ist nicht instrumentiert (keine neue
Instrumentierung vorgesehen). Der Pre-S6-Abort war dagegen per Backtrace im
Configuration-Commit (`validationScan`/`scanGroupMetadata`) lokalisiert; das
bleibt unverändert.

Zustand unmittelbar vor dem Rückwechsel `AP_ONLY` → `HOME_WIFI`
(aktueller freier Heap / größter Block / Low-Water-Mark seit Boot):

| Fall | Zustand | Ergebnis |
|---|---|---|
| Pre-S6, mit Client, Seite offen (Clean-Lauf) | 7428 / 5632 / 2192 B | Abort, 192 B |
| Pre-S6, mit Client, ohne Seite | 7756 / 6400 / 2576 B | Abort, 192 B |
| Pre-S6, mit Client, Seite offen (Client-Last-Lauf) | 7300 / 4864 / 2956 B | Abort, 192 B |
| Pre-S6, ohne Client (2×) | 10888 / 7936 / 6740 B und 10752 / 8192 / 6140 B | ok |
| S6, ohne Client (A, B) | 15844 / 12800 / 10972 B und 15936 / 10752 / 7880 B | ok |
| S6, mit Client ohne Seite (C, D) | 12620 / 8192 / 7856 B und 12456 / 8192 / 4124 B | ok |
| S6, mit Client, Seite mehrfach geladen (E) | 12288 / 8192 / 4124 B | ok |

Zwischen fehlgeschlagenen und bestandenen Zuständen liegen im aktuellen freien
Heap 4,5 kB (7756 → 12288 B) und im größten Block 1,8 kB (6400 → 8192 B).
Die Low-Water-Mark trennt die Fälle **nicht**: Pre-S6 bestanden auch
Moduswechsel ohne Client bei einer Tiefstmarke von 2872–2956 B.

**Bewertung:** Der bekannte fatale R1-Commit-Blocker (192-B-Abort im
Configuration-Commit beim Rückwechsel nach `HOME_WIFI` mit Client) ist im
**gemessenen Scope als geschlossen zu werten**: 0 von 5 Matrixfällen, 0
Commit-OOM, kein Abort (niedrigste Low-Water-Mark im Moduswechsel-Fenster 4124 B); derselbe Pfad scheiterte vor S6 3 von 3 mit Client.
Der Scope ist eng: Release-Profil, höchstens ein Client, fünf Durchläufe,
Rückwechsel `AP_ONLY` → `HOME_WIFI` (und die vorgelagerten Wechsel nach
`AP_ONLY`), kein Mehr-Client- und kein Dauerwechsel-Test. Der Erfolg lässt
sich nicht allein auf den Wegfall des Puffers zurückführen (Reserve und
Fragmentierung sind gleichzeitig verändert).

## C. Fragmentierung

Verlauf an den `stable_home_wifi`-Punkten der Serie (Zeit in s, freier Heap,
größter Block):

| s | 4 | 177 | 524 | 838 | 1028 | 1367 |
|---|---:|---:|---:|---:|---:|---:|
| frei | 18624 | 19308 | 19244 | 18968 | 18888 | 19068 |
| größter Block | 14848 | 10240 | 8192 | 7424 | 7680 | 7168 |

Alle 17 Punkte im Zustand `HOME_WIFI`/`HomeConnected` liegen beim freien Heap
zwischen 18624 und 19492 B, ohne Abwärtstrend. Der größte Block sinkt
stufenweise mit den ersten Commits (14848 → 10240 → 8192 → 7424) und bewegt
sich danach um 7,2–7,7 kB (7424, 7680, 7168); an den `stable_ap_only`-Punkten
sinkt er von 12800 über 10752 auf 8192 B und bleibt für drei Punkte bei
8192 B.

- Sinkt der gesamte freie Heap dauerhaft? **Nein** in dieser Serie (±0,4 kB
  um 19 kB, ~84 min).
- Bleibt `free_heap` stabil und nur der größte Block sinkt? **Ja.** Das passt
  zu Fragmentierung (Verschiebung der freien Fläche in kleinere Stücke), nicht
  zu einem dauerhaften Verlust von Gesamtspeicher.
- Reicht die Blockgröße für die bekannten R1-Allokationen? Die größte der
  bekannten fehlgeschlagenen Anforderungen ist 2048 B (S2/S3,
  `makeRepresentativeScreen`); die Commit-Allokation ist 192 B, der
  Idle-Fehlschlag 1696 B. Der kleinste aktuelle Block (7168 B) liegt beim
  3,5-Fachen von 2048 B. Der Display-Zeichenpuffer (12800 B) wird einmalig
  beim Start allokiert. Die Idle-Fehlschläge zeigen allerdings, dass ein hoher
  letzter Messwert einen Fehlschlag zum Fehlerzeitpunkt nicht ausschließt
  (Punkt D).
- Evidenz für einen echten Leak? **Nein, nicht belegt.** Der freie Heap bleibt
  über 84 min und fünf Commits stabil; für einen Leak spräche ein stetiger
  Abfall, der nicht vorliegt. Ob der größte Block bei sehr vielen Wechseln
  weiter fällt oder bei ~7 kB ein Plateau hält, ist mit fünf Commits nicht
  entschieden. Es wird keine Defragmentierungsarchitektur vorgeschlagen.

## D. 1696-B-Idle-Failure (separate Klassifikation)

Bekannt: `size=1696`, `caps=0x1800`, `HOME_WIFI`-Idle, nichtfatal, kein Abort,
kein Reset, **4 Ereignisse in 3 Läufen** (2× vor S6: Idle ~57 min und ~40 s nach
einem Rückwechsel im Clean-Lauf; 2× in der S6-Serie bei 1745,9–1746,9 s und
3370,4–3371,4 s Log-Tick, Herleitung siehe S6-Evidenz), Aufrufer unbekannt.
(Die zwei 1532-B-Ereignisse im S4-Hardwarelauf nach Browserlast sind eine
andere Größe und hier nicht gezählt.)

Zur Wirkung aus der vorhandenen Evidenz:

- Keines der vier Ereignisse wurde von einem Abort, Reset, einer
  WLAN-Trennung oder einer sichtbaren Zustandsänderung gefolgt (S6-Serie:
  ~12 min Log nach beiden Ereignissen ohne Folgeereignis; im Clean-Lauf lief
  das Gerät danach weiter).
- Die Allokation wurde von ihrem Aufrufer behandelt (kein C++-
  `operator new`-Abort); der Aufrufer ist unbekannt.
- Eine belastbare Ereignisrate lässt sich aus den vorhandenen Läufen nicht ableiten (verschiedene Laufdauern und Bedingungen; ein Pre-S6-Ereignis trat bereits etwa 40 s nach einem Rückwechsel auf).
- Nach Projektregel (`AGENTS.md`) benötigen Regelung und Safety weder Netzwerk
  noch Web noch Anzeige; ein nichtfataler Fehlschlag im Netzwerk-Idle hat
  damit auf den Regelpfad keinen Durchgriff. Das ist eine Architekturaussage,
  keine Messung des Aufrufers.

Es wird **nicht** behauptet, der Aufrufer sei WLAN oder lwIP.

Empfohlene Klassifikation: **`FOLLOW_UP_NON_BLOCKING_FOR_PR174`**.
Begründung: 4 bekannte nichtfatale Ereignisse in 3 Läufen; kein
Abort/Reset/`ServiceRequired`; kein beobachteter Durchgriff auf den
Regel-/Safety-Kern; Aufrufer und tatsächliche Komfort-/Netzwerkauswirkung
unbekannt. Der fatale RAM-Blocker ist unabhängig davon geschlossen.
Einschränkung: Die Ursache ist ungeklärt, die tatsächliche Wirkung (z. B. ein
verlorener Netzwerkframe) ist aus dem UART-Log nicht beurteilbar. Das
Follow-up ist daher **`DIAGNOSE_BEFORE_PR170_NETWORK_EXPANSION`** und nicht Teil
des Scopes von PR #174 (siehe O5). Ob dafür ein eigenes Issue anzulegen ist,
entscheidet der Owner.

## O4 – Empfehlung Mindestabstand / RAM-Budget (Ownerentscheidung erforderlich)

Abgeleitet ausschließlich aus den Messdaten, mit getrennter Zuordnung von
**gemessenem Boden**, **Qualifikationsgrenze** und **Warnschwelle**:

| Größe | Fehler beobachtet bei | gemessener Boden (S6-Scope) | Pre-S6-PASS-Vergleichswert (ohne Client) |
|---|---|---|---|
| aktueller freier Heap unmittelbar vor dem Moduswechsel mit Client | ≤ 7756 B | 12288 B | 10752 B |
| aktueller größter Block unmittelbar vor dem Moduswechsel mit Client | ≤ 6400 B | 8192 B | 7936 B |
| aktueller freier Heap, systemweit | – | 12288 B | – |
| aktueller größter Block, systemweit | – | 7168 B | – |
| Low-Water-Mark im Moduswechsel-Fenster | 2192–2956 B vor dem Abort | 4124 B | 2872 B |
| Main-Task-Stack-HWM | kein Stack-Fehler beobachtet | 5936 B (alle Läufe), 6256 B (S6) | – |

Die Pre-S6-PASS-Werte (10752 B und 7936 B) sind historische Vergleichswerte
ohne Client und **keine Warnschwellen**.

**Empfohlene Ownerentscheidung für R1:**

- **Systemweite gemessene Qualifikationsuntergrenze** (Regression gegen den
  nachgewiesenen Scope): aktueller freier Heap **≥ 12288 B**, aktueller größter
  zusammenhängender Block **≥ 7168 B**.
- **Zusätzliche Qualifikationsbedingung unmittelbar vor dem kritischen
  Netzwerkmoduswechsel mit einem Client:** freier Heap **≥ 12288 B**, größter
  Block **≥ 8192 B**.
- Diese Werte sind Regression-/Qualifikationsgrenzen des nachgewiesenen
  R1-Scopes. Sie sind **kein statistisch nachgewiesener zusätzlicher
  Safety-Abstand**: Der niedrigste bestandene Wert ist nur der niedrigste
  nachgewiesene Wert, nicht mehr.
- Die Low-Water-Mark (4124 B im Moduswechsel-Fenster) trennt bestandene und
  fehlgeschlagene Fälle nicht und bleibt Beobachtungsgröße; der Stack-HWM
  liefert keine Stop-Schwelle (kein Stack-Fehler, 5936 B niedrigster Wert).
- **Keine separate numerische Warnschwelle** wird aus den vorhandenen Daten
  abgeleitet. Falls der Owner später eine Frühwarnschwelle möchte, muss sie
  logisch **oberhalb** der kritischen Grenze liegen und als zusätzliche Policy
  begründet werden.

Die Zahl ersetzt, falls der Owner sie beschließt, den Begriff
„nachgewiesene Mindestreserve“ in `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`;
sie wird hier nicht eigenmächtig gesetzt.

## O5 – bedingte weitere Schnitte (Empfehlung, keine Implementierung)

Für **PR #174**:

```text
S9_RECOMMENDATION=NOT_NEEDED_FOR_PR174
S10_RECOMMENDATION=NOT_NEEDED
S11_RECOMMENDATION=NOT_NEEDED
IDLE_1696B_FOLLOW_UP=DIAGNOSE_BEFORE_PR170_NETWORK_EXPANSION
S9_FUTURE_CONDITION=ONLY_IF_CALLER_DIAGNOSIS_PROVES_WIFI_LWIP_MEMORY_PROFILE_RELEVANT
```

- **S9 (WLAN-Speicherprofil):** Der fatale Blocker ist ohne WLAN-Tuning
  geschlossen; es gibt keinen Aufrufer-Nachweis für den 1696-B-Befund. Damit
  wird kein WLAN-Tuning auf Verdacht Teil von PR #174. S9 käme nur in Betracht,
  wenn eine spätere Aufrufer-Diagnose das WLAN-/lwIP-Speicherprofil als
  relevant belegt.
- **S10 (Stack/Commit):** Der Moduswechsel lief in 5 von 5 Fällen; der
  Stack-HWM liegt bei mindestens 5936 B von 24576 B, es gab keinen
  Stack-Fehler. Aus der Evidenz besteht kein Bedarf für einen Stack- oder
  Commit-Umbau.
- **S11 (struktureller UI-Umbau):** `makeRepresentativeScreen()` trat seit S4
  in keiner Messung mehr als OOM auf (S4-Hardware, Clean-Läufe, S6-Serie); der
  LVGL-Pool ist mit 24 % unverändert. Der in S4 benannte Rest (By-Value-Kopie
  der AP-Strings bei einem Redraw) ist ereignisgebunden.

**PR #170:** `PR170_DEPENDENCY=KEEP_BLOCKED_ON_IDLE_1696B_DIAGNOSIS_OR_EXPLICIT_OWNER_WAIVER`
nach Abschluss von PR #174. Das erlaubt PR #174 zu konvergieren, ohne den
unbekannten Netzwerk-/Idle-Befund in dessen Scope zu ziehen. Es wird keine
Ownerentscheidung simuliert; O4 und O5 bleiben `OWNER_DECISION_REQUIRED`,
`PR174_FINAL_GO=NO_UNTIL_OWNER_O4_O5`.

## Offene Grenzen der Auswertung

- Je Bedingung 1–2 Durchläufe, ein Boot, ein Client (ein Handy), 84 min.
- Margen bei mehreren Clients, sehr vielen Wechseln oder anderem Profil
  (`esp32_bringup`) sind nicht gemessen.
- Der Aufrufer des 1696-B-Fehlschlags bleibt unbekannt.
- Rotationsprüfung des direkten Draw-Pfads ist nicht belegt (Owner konnte sie
  nicht beurteilen).
