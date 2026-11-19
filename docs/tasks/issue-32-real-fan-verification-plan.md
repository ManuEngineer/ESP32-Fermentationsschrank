# Issue #32 – realer Luefter-Nachweis OUT1/OUT2 (Restplan, Revision 1)

Status: **Plan zur Review und Ownerfreigabe der exakten Plan-Commit-SHA.** Keine
Implementierung, kein Testimage, kein Flash, keine Lastaktivierung vor Freigabe.
Baseline: `origin/main` `17c278ad577fe7b0ca359b154737351221924b99` (PR #215
gemergt; PR #214 gemergt `29c9f51217ebc2dd4cd75e8b3ecb385c4ec3bb5d`).

Gilt nur fuer #32. #33 (BTS7960/Peltier) wird nicht begonnen; GPIO13/14/25 bleiben
inaktiv, Peltier abgeklemmt, `ACTUATOR_RELEASE=NO`. Bestehende #32-Plaene und die
Evidence (`docs/audits/ISSUE32_BOOT_DIAGNOSIS_EVIDENCE.md`) bleiben unveraendert;
dieser Plan ergaenzt nur den realen Restumfang.

## 1. Ziel und Nicht-Ziele

Ziel: den noch fehlenden realen Nachweis fuer den **Aussenluefter OUT2/GPIO17**
(Zuordnung, AUS/EIN/AUS) mit dem kleinstmoeglichen Eingriff erbringen, die
Zuordnung des **Innenluefters OUT1/GPIO16** als realen Verbraucher bestaetigen,
und die verbleibenden #32-Nachweise (Nachlauf, Dauerlauf, Summer) ehrlich
abgrenzen.

Nicht-Ziele: keine Planner-/Regelschleifen-Komposition (#35/#90); keine
Peltier-/BTS7960-Freigabe; kein neuer Treiber, keine neue Abstraktion, kein
zweiter Testvertrag; kein Summer-/PWM-Entwurf; keine pauschalen Spannungs- oder
Bootpegelmessungen; die bereits bestandenen OUT1/OUT4-Pulse werden nicht als
Pflicht wiederholt.

## 2. Belegt vs. offen (Kriterien aus Issue #32)

| Kriterium | Stand | Quelle |
|---|---|---|
| ActiveHigh-SSOT OUT1–OUT4, Reserve AUS-Init, Generator/Selftest | belegt (`SIM-32-05`) | PR #214 |
| Sichere Initialisierung nach `app_main` (alle vier AUS) | belegt | Boot-Log PR #214, `POST_BOOT_OUT1_OFF=PASS` (PR #212) |
| AUS→EIN→AUS OUT1/GPIO16 und OUT4/GPIO27 mit temporaerer Testlast | belegt `HW-32-04=PASS` (2026-11-19) | PR #214, Evidence |
| OUT4/GPIO27 | bleibt **Reserve**, kein produktiver Aussenluefter | HARDWARE.md |
| Boot/gehaltener EN-Reset mit Luefter physisch laufend | **akzeptierte R1-Abweichung**, historischer FAIL bleibt FAIL, kein Konformitaets-PASS; gilt nicht fuer BTS7960/Peltier | Issue #32 R1-Ownerentscheidung |
| **OUT2/GPIO17 realer Aussenluefter: Zuordnung, AUS/EIN** | **offen** (`NOT_RUN`) | – |
| OUT1 als realer Innenluefter (Zuordnung/Versorgung) | offen: bisher temporaere Testlast | §4 (a) |
| Produktionsnaher Pfad fuer beide Luefter | **offen**, siehe §3 | – |
| Innenluefter-Dauerlauf, Aussenluefter-Nachlauf (Hardware) | **nicht ohne #35/#90 end-to-end nachweisbar**, siehe §5 | – |
| OUT3/GPIO26 Summer | **separater offener #32-Punkt**, siehe §6 | O3 |

## 3. Befund Bestand (Reuse-Pruefung)

- `EspIdfBinaryOutputSink` / `IBinaryOutputSink`: vorhanden, alle vier Kanaele in
  `main/app_main.cpp` instanziiert (`innerFanOutput`, `outerFanOutput`,
  `buzzerOutput`, `reserveOutput`), AUS initialisiert.
- `ActuatorPlanSinkDriver` (`lib/fermentation_app/src/actuator_plan_sink_driver.*`)
  uebersetzt ein Planner-Ergebnis auf `IBidirectionalActuatorSink& peltier`,
  `outerFan`, `innerFan`. Er **braucht zwingend einen Peltier-Sink**; im Produktivbau
  waere das der reale BTS7960-Adapter (GPIO13/14/25). Seine Verwendung fuer einen
  Luefterversuch beruehrt damit den Leistungspfad und ist wegen #33 ausgeschlossen;
  ein Nullsink waere ein Ersatzpfad.
- `ActuatorPlanner` / `TemperatureControlApplicationOrchestrator`: werden in
  `app_main.cpp` nicht komponiert (kein Aufruf von `ActuatorPlanSinkDriver` in
  `main/`); die Komposition gehoert zu #35/#90.
- Service-/Diagnosewege #25/#26/#31: die UI-/Run-Kommandos
  (`fermentation_ui_commands.hpp`) schalten keine Einzelausgaenge; eine
  Web-Manuelloberflaeche ist `R2_VORGEMERKT` (PR #211). Ein bestehender, sicherer
  Bedienpfad fuer reale Einzel-EIN-Impulse ist im Bestand **nicht vorhanden**.
- Vorhanden und wiederverwendbar: der in PR #214 geprueft eingefuehrte
  Bring-up-only-Pulsharness (`main/issue_32_output_pulse_harness.*`,
  `scripts/build_issue32_output_pulse.py`, `APP_ISSUE_32_OUTPUT_PULSE`): UART
  `pulse out1|pulse out4|off`, 3-s-Puls, fail-closed, nur im Bring-up-Profil. Er
  nutzt dieselben `IBinaryOutputSink`-Instanzen, die der Driver spaeter als
  `outerFan`/`innerFan` erhaelt. Er enthaelt **keinen** Kanal fuer OUT2.

Folgerung: Der produktionsnahe Weg (Planner → Driver → Sinks) ist erst mit der
#35/#90-Komposition real durchlaufbar. Bis dahin beweist ein Hardwarepuls ueber
denselben Sink genau die Adapter-/Verdrahtungsseite (Kanal, Pegel, Verbraucher);
die Driver-/Planner-Logik ist durch Host-Vertraege belegt (§5).

## 4. Umsetzungspfad

### Code-Delta C1 (eng, einzig noetig)

Den bestehenden Pulsharness um **einen** Kanal erweitern: Konstruktor erhaelt
zusaetzlich `outerFanOutput` (OUT2), Kommando `pulse out2` im selben Muster wie
`pulse out1`/`pulse out4` (gleiche 3-s-Begrenzung, BUSY-/Abbruch-/Timeout-Regeln,
gleiches Log `ISSUE32_PULSE channel=out2 ...`); `app_main.cpp` uebergibt die
bereits vorhandene Instanz. `pulse out4` und das Reservekanal-Verhalten bleiben
unveraendert (siehe D5). Kein neuer Treiber, kein Release-Code, keine neue Datei.

### Hardware-Gates (jeweils separate Ownerfreigabe, mit Owner am Geraet)

| Gate | Inhalt |
|---|---|
| G1 Verdrahtung | Owner steckt einen Test-/Aussenluefter von OUT4 nach **OUT2** um (Plattform spannungsfrei); Versorgung und Luefterdaten werden vom Owner genannt; BTS7960/Peltier abgeklemmt bestaetigt |
| G2 Flash | Bring-up-#32-Image aus sauberem Source-HEAD (`scripts/build_issue32_output_pulse.py`, ohne `--allow-dirty`), App-only bei `0x10000`, Hash-Verifikation, kein Erase/NVS/State-Store/Bootloader/Partitionstabelle, DEV-Image ohne Backup (D4-Praezedenz #190, Freigabe je Flash) |
| G3 Pulse | UART-Pulse gemaess §4-Tests, Owner beobachtet |

### Testfaelle

(a) **Nachvollziehen:** realer Anschluss und Versorgung von OUT1/OUT2 (welcher
Luefter, welche Spannung, welche Last) vom Owner dokumentieren; Zuordnung
Innenluefter = OUT1, Aussenluefter = OUT2 laut SSOT.
(b) **T1** `pulse out2` einmal: AUS→EIN (~3 s)→AUS am realen Aussenluefter, richtiger
Verbraucher (OUT1/OUT4-Last bleibt aus). **T2** nur falls OUT1 bis dahin noch kein
realer Innenluefter war: ein `pulse out1` am realen Innenluefter; sonst entfaellt
T2 (bestehender `HW-32-04` OUT1-Nachweis).
(c) **T3** Boot/EN-Reset mit angeschlossenem OUT2-Luefter **beobachten und
dokumentieren** unter der akzeptierten R1-Abweichung (kein neues PASS/FAIL gegen die
historische Anforderung, keine Spannungsmessung).
(d) Nachlauf/Dauerlauf: **offen kennzeichnen** (§5).

### Stopkriterien

Unerwartete Einschaltung (auch beim Boot ohne Kommando, ausser der bekannten
R1-Abweichung), falscher Kanal/Verbraucher, verweigertes AUS (`DISABLE_FAILED`,
Luefter bleibt an), Reset/Panic/WDT, ungeklaerte reale Versorgung, keine sichere
Abschaltmoeglichkeit (Versorgung nicht trennbar). Bei Stopp: Versorgung trennen,
Diagnose, keine Wiederholung ohne neue Freigabe.

## 5. Nachlauf und Dauerlauf (Abgrenzung)

Host-/Planner-Vertraege **vorhanden**: `test_actuator_planner` (Aussenluefter-
Nachlauf, Repuls waehrend Nachlauf, Force-Stop-Deadlines, Innenluefter-Phase),
`test_actuator_plan_sink_driver` (Treiberreihenfolge, fail-closed, verweigertes
Aussenluefter-EIN). **Nicht durchfuehrbar** ohne produktive Regelungs-/Safety-
Komposition: ein Hardware-End-to-End-Nachweis von Innenluefter-Dauerlauf und
Aussenluefter-Abschalt-/Nachlaufverhalten. Er bleibt `NOT_RUN` (`HW-32-03`-Teil)
und wird nach der #35/#90-Integration nachgeholt; kein vorgetaeuschtes PASS, kein
Ersatzpfad.

## 6. OUT3/GPIO26 Summer

Aktiver Summer derzeit nicht vorhanden/entschieden (Issue #32, O3, `HARDWARE.md`
Summer). Eigener offener #32-Punkt: weder PASS noch Entwurf von Summer/PWM/Mustern
in diesem Plan; benoetigt eine Ownerentscheidung (Summerbestueckung und -typ).

## 7. Abgrenzung zu #33

Kein #33-Hardwaretest, keine BTS7960-Adapter-Safety-Pruefung, kein Peltierpuls.
Die gemergte #33-Software (PR #196) wird nicht angefasst. Erst nach ausreichend
belegtem Aussenluefterpfad folgt ein eigener #33-Plan zunaechst ohne Peltier;
spaetere Peltierpulse brauchen zusaetzlich Schutz-/Luefter-/Sensor-
voraussetzungen nach #33/`HARDWARE.md`. Restnachweise von #190 bleiben dort.

## 8. Dokumentation und Evidence

Nach Freigabe: `HW-32-05` (neu, `NOT_RUN` bis Lauf) fuer OUT2-Puls in
`docs/ACCEPTANCE_TESTS.md`; `HW-32-03` bleibt `NOT_RUN` mit Teilstand;
Evidence als **neuer Abschnitt** in `docs/audits/ISSUE32_BOOT_DIAGNOSIS_EVIDENCE.md`;
`docs/HARDWARE.md`/`OPEN_POINTS.md` nur fuer belegte Punkte; ROADMAP-Zeile #32.
Tests: nur gezielt (Harness-Build bring-up/release, Generator/Architektur-Check,
Release-Trennung `ISSUE32` 0 Treffer); der Harness hat keinen Hosttest.

## 9. Owner-Entscheidungen

- **D1:** Freigabe der exakten Plan-SHA.
- **D2:** Code-Delta C1 (`pulse out2` im bestehenden Harness) als kleinster
  produktionsnaher Hardwarepfad; Alternative waere Warten auf die #35/#90-Komposition.
- **D3:** Angaben zu Versorgung/Last/Identitaet der Luefter an OUT1/OUT2 (§4 a)
  und welcher Luefter nach OUT2 umgesteckt wird.
- **D4:** DEV-Flash ohne Backup auch fuer #32 (Freigabe je Flash-Vorgang).
- **D5:** Reserve-Sink und `pulse out4` (aus PR #214) belassen oder entfernen
  (Entscheid war offen; Vorschlag: belassen, keine Aenderung in diesem Schnitt).
- **D6:** Summer (OUT3) bleibt separater offener Punkt bis zu einer Ownerentscheidung.
