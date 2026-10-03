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

FATAL_R1_NETWORK_COMMIT_RAM_BLOCKER=CLOSED
FATAL_R1_NETWORK_COMMIT_RAM_BLOCKER_SCOPE=RELEASE_PROFILE_ONE_CLIENT_5_OF_5_MATRIX_CASES_MARGIN_NOT_PROVEN_FOR_MORE_CLIENTS_OR_MORE_SWITCHES
MAKE_REPRESENTATIVE_SCREEN_OOM=CLOSED

LOWEST_MEASURED_COMMIT_MIN_HEAP_BYTES=4124
LOWEST_MAIN_STACK_HWM_BYTES=6256
LOWEST_CURRENT_LARGEST_FREE_BLOCK_BYTES=7168

IDLE_1696B_FAILURE=OPEN
IDLE_1696B_CLASSIFICATION=FOLLOW_UP_NON_BLOCKING

S9_RECOMMENDATION=ONLY_IF_1696_DIAG_REQUIRES
S10_RECOMMENDATION=NOT_NEEDED
S11_RECOMMENDATION=NOT_NEEDED

O4=OWNER_DECISION_REQUIRED
O5=OWNER_DECISION_REQUIRED

PR170_DEPENDENCY=KEEP_UNTIL_OWNER_O4_O5_AND_S8_REVIEW
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
| **Low-Water-Mark** (`minimum_free_heap`) | 4124 B | Fall C (Commit mit Client), danach nicht mehr unterschritten |
| niedrigster Main-Task-Stack-HWM | 6256 B (von 24576 B) | ab erstem `AP_ONLY`; über alle Läufe (S2–S6) niedrigster Wert 5936 B (Pre-S6) |
| LVGL-Pool maximal belegt | 15856 B (24 %, Pool 63384 B) | unverändert gegenüber S2–S4 |

Der Heap-Gewinn durch S6 beträgt gemessen +4,8…5,0 kB beim freien Heap
(Details in der S6-Evidenz).

## B. Configuration-Commit

```text
PRE_S6_CLIENT_COMMIT_ABORT=3_OF_3
POST_S6_CLIENT_COMMIT_PASS=3_OF_3
POST_S6_ALL_MATRIX_CASES=5_OF_5_PASS
POST_S6_COMMIT_OOM=0
LOWEST_POST_S6_COMMIT_MINIMUM_FREE_HEAP=4124_B
```

Zustand unmittelbar vor dem Rückwechsel `AP_ONLY` → `HOME_WIFI`
(aktueller freier Heap / größter Block / Low-Water-Mark):

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
Commits ohne Client bei einer Tiefstmarke von 2872–2956 B.

**Bewertung:** Der bekannte fatale R1-Commit-Blocker (192-B-Abort im
Configuration-Commit beim Rückwechsel nach `HOME_WIFI` mit Client) ist im
**gemessenen Scope als geschlossen zu werten**: 0 von 5 Matrixfällen, 0
Commit-OOM, kein Abort; derselbe Pfad scheiterte vor S6 3 von 3 mit Client.
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
- Die Frequenz ist niedrig: höchstens ein Ereignis je ~25–30 min Idle.
- Nach Projektregel (`AGENTS.md`) benötigen Regelung und Safety weder Netzwerk
  noch Web noch Anzeige; ein nichtfataler Fehlschlag im Netzwerk-Idle hat
  damit auf den Regelpfad keinen Durchgriff. Das ist eine Architekturaussage,
  keine Messung des Aufrufers.

Es wird **nicht** behauptet, der Aufrufer sei WLAN oder lwIP.

Empfohlene Klassifikation: **`FOLLOW_UP_NON_BLOCKING`** für das Mergen von
PR #174. Begründung: begrenzte, in der vorhandenen Evidenz folgenlose
Wirkung; der fatale RAM-Blocker ist unabhängig davon geschlossen. Einschränkung:
Die Ursache ist ungeklärt, die tatsächliche Produktwirkung (z. B. ein verlorener
Netzwerkframe) ist aus dem UART-Log nicht beurteilbar. Die Klärung (Aufrufer-
Nachweis, falls der Review sie fordert, über das bedingte S9/Diagnose) sollte
vor einer Erweiterung der Netzwerkfunktionen (PR #170) erfolgen. Ob dafür ein
eigenes Issue anzulegen ist, entscheidet der Owner.

## O4 – Vorschlag Mindestabstand / RAM-Budget (Ownerentscheidung erforderlich)

Abgeleitet ausschließlich aus den Messdaten (Abschnitt B, Messscope: Release,
höchstens ein Client). Der **aktuelle** Heap unmittelbar vor dem
Netzwerk-Commit trennt bestandene und fehlgeschlagene Fälle; die Low-Water-Mark
trennt sie nicht.

| Größe | Fehler beobachtet bei | niedrigster bestandener Wert (Scope) | Vorschlag |
|---|---|---|---|
| aktueller freier Heap vor dem Netzwerk-Commit in `AP_ONLY` | ≤ 7756 B | 12288 B (S6, mit Client); 10752 B (Pre-S6, ohne Client) | Mindestwert **12288 B** (niedrigster mit Client nachgewiesener Wert); Warnschwelle **10752 B** (niedrigster nachgewiesener Wert ohne Client) |
| aktueller größter Block vor dem Commit | ≤ 6400 B | 8192 B (S6); 7936 B (Pre-S6, ohne Client) | Mindestwert **8192 B**; Warnschwelle **7936 B** |
| `minimum_free_heap` (Low-Water-Mark) im Commit-Test | 2192–2956 B vor dem Abort | 4124 B (S6, mit Client); 2872 B (Pre-S6, ohne Client) | nur als Beobachtungsgröße, **kein Gate** (trennt nicht); Referenz 4124 B |
| Main-Task-Stack-HWM | kein Stack-Fehler beobachtet | 5936 B über alle Läufe, 6256 B in S6 | Mindestwert **5936 B** (niedrigster beobachteter), keine Stop-Schwelle ableitbar |

Die Werte sind die tatsächlich gemessenen Pass-Grenzen, keine gerundeten
Wunschwerte. Offen für den Owner: ob der Mindestabstand die nachgewiesene
Pass-Grenze selbst oder eine zusätzliche Reserve darüber sein soll, und ob eine
Warn-/Stop-Schwelle im Produkt (nicht nur im Test) eingeführt wird. Das erfordert
laut `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md` eine Zahl statt der Formulierung
„nachgewiesene Mindestreserve“; sie wird hier nicht eigenmächtig gesetzt.

## O5 – bedingte weitere Schnitte (Empfehlung, keine Implementierung)

```text
S9  WLAN memory profile = ONLY_IF_1696_DIAG_REQUIRES
S10 stack/commit        = NOT_NEEDED
S11 structural UI       = NOT_NEEDED
```

- **S9 (WLAN-Speicherprofil):** Der fatale Blocker ist ohne WLAN-Tuning
  geschlossen; S9 ist nur gerechtfertigt, wenn der 1696-B-Befund eine
  Diagnose verlangt und diese WLAN-/lwIP-Puffer als Quelle belegt. Ohne
  Aufrufer-Nachweis wird S9 nicht begründet.
- **S10 (Stack/Commit):** Der Configuration-Commit lief in 5 von 5 Fällen; der
  Stack-HWM liegt stabil bei 6256 B von 24576 B und es gab keinen Stack-Fehler.
  Es besteht aus der Evidenz kein Bedarf für einen Stack- oder
  Commit-Umbau.
- **S11 (struktureller UI-Umbau):** `makeRepresentativeScreen()` trat seit S4
  in keiner Messung mehr als OOM auf (S4-Hardware, Clean-Läufe, S6-Serie); der
  LVGL-Pool ist mit 24 % unverändert. Der in S4 benannte Rest (By-Value-Kopie
  der AP-Strings bei einem Redraw) ist ereignisgebunden.

## Offene Grenzen der Auswertung

- Je Bedingung 1–2 Durchläufe, ein Boot, ein Client (ein Handy), 84 min.
- Margen bei mehreren Clients, sehr vielen Wechseln oder anderem Profil
  (`esp32_bringup`) sind nicht gemessen.
- Der Aufrufer des 1696-B-Fehlschlags bleibt unbekannt.
- Rotationsprüfung des direkten Draw-Pfads ist nicht belegt (Owner konnte sie
  nicht beurteilen).
