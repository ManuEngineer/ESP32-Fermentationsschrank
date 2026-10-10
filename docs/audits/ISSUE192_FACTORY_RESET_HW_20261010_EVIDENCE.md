# Issue #192 – Werksreset-Hardwareverifikation: N1–N8, P2b und P3 auf `main` 21082de (2026-10-10)

Plan: `docs/tasks/issue-192-factory-reset-hardware-verification-plan.md` (Revision 2).
Die Evidence vom 2026-10-08 (`ISSUE192_FACTORY_RESET_HW_20261008_EVIDENCE.md`,
Baseline `df5a3dd`) bleibt historisch und wird nicht uebernommen.

Ownerfreigabe: **G1 voll** fuer diesen Plan-/Firmwarestand (Ownerantwort am
2026-10-10). **G2, G3, G4 nicht erteilt.** `ACTUATOR_RELEASE=NO`; Aktoren physisch
getrennt/deaktiviert, kein Aktortest.

## 1. Geraete- und Firmwarestand

```text
SOURCE_BASELINE=21082de766a0b7b52108448c85ae51ba8d84b3f1 (main, enthaelt PR #199)
GERAET_FIRMWARE=f859ef6 (UART-Boot-Log des Geraets; Flash HW-188-A01 vom 2026-10-09)
FLASH_HEUTE=KEINER (G1 erteilt, aber nicht noetig: Produktquellen f859ef6 == 21082de, Unterschied nur docs/)
BUILD_21082de=sauber (--require-clean-source-tree), Profil release, ESP-IDF v6.1 -> PASS, nicht geflasht
APP_BIN_SHA256_21082de=9416e64013d37503257f1818614889767eae39cb9f51668f0f910d8ac5599ec7 (1 740 064 Byte)
APP_ELF_SHA256_21082de=d3196493f3b8f734a0d7c1fd24e6fbbaef0b34fc7e8c971921ad6823c7e2af10
PARTITION_TABLE_SHA256_21082de=d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d
BOOTLOADER_SHA256_21082de=d70a1e164a87b2950cb569fba92ab123fab45f770b8814010f2926968416902f (weicht vom Wert in HW-188-A01 ab; Ursache nicht untersucht; nicht geflasht, daher ohne Wirkung auf diesen Lauf)
```

Einschraenkung: Das auf dem Geraet laufende Image ist **nicht** der oben
gebaute Hash (`APP_VERSION_STRING=21082de`), sondern das Image `f859ef6`. Die
Gleichheit der Produktquellen ist ueber den Quelldiff belegt, nicht ueber einen
Binaervergleich. Die Ergebnisse gelten damit fuer `f859ef6` und sind fuer
`21082de` quellgleich, aber nicht binaergleich.

## 2. UART-Provenienz

```text
ROHLOG=lokal (nicht im Repository), uart192_n1n8.raw.txt, 1063 Zeilen, SHA256=593c7b851dfd4159d7770b7f6a56befdcf836b340e4e47257f6aa799ba3a0322
ZEITRAUM=2026-10-10 07:20:31 .. 07:32:51 UTC (eine durchgehende Aufnahme, ca. 12 min)
RESETS=1 (POWERON_RESET zu Aufnahmebeginn durch Oeffnen des Ports); danach keiner
PANIC/GURU/TASK_WDT/BROWNOUT/OOM/STACK_OVERFLOW=0
FACTORY-RESET-AUSFUEHRUNGSZEILEN=0
TOUCH_PRESS_DISPATCH=23 Zeilen, alle outcome=2 (nicht PIN-spezifisch; keine Zuordnung zu Einzelschritten)
HERZSCHLAG_MAX_LUECKE=3510 ms (07:29:40, beim Zaehlen der PIN-Fehlversuche; PBKDF2); Task-WDT 5 s nicht ausgeloest
NETZWERK=HOME_WIFI / HomeConnected durchgehend
HEAP (idle_120s): frei ca. 43,2 kB; Minimum 34532 B (07:22) -> 33372 B (07:28) -> 29272 B (07:32); groesster Block 38912 B
```

Beobachtung ohne Bewertung: Das Heap-Minimum sank im Lauf um ca. 5,3 kB
(34532 -> 29272 B), ohne dass ein Fehler auftrat; die Ursache wurde nicht
untersucht (Kandidaten: PIN-Fehlversuche/PBKDF2, UI-Seitenwechsel). Es ist kein
Befund gegen einen Grenzwert, sondern ein Messwert fuer den Folgelauf.

Nicht im Log belegt: UART enthaelt keine Zeilen fuer Warnung/Confirm/Hold-Seite
oder Abbruch; die Zustaende N1–N7 sind ausschliesslich Ownerbeobachtung.

## 3. Ablauf und Ownerbeobachtungen (N1–N8)

Die Ownerantworten stammen aus dem Chat; es gibt keine Bild-/Videoaufnahme.

| Schritt | Ownerbeobachtung | Bewertung |
|---|---|---|
| N1 Einstellungen -> Service (PIN) | "ja" (PIN-Seite oeffnet sich) | Owner-bestaetigt, UART ohne Fehlersignatur |
| N2 "PIN vergessen?" ohne PIN-Eingabe | "ja" (sichtbar, Warnung erscheint) | Owner-bestaetigt |
| N3 Warnung, Texte DE/EN/ES | "ja" (lesbar, nicht abgeschnitten) | Owner-bestaetigt, keine Messung der Textbreiten |
| N4 Warnung -> Abbrechen | "ja" | Owner-bestaetigt |
| N5 Confirm-Seite, Text, Abbrechen | "wirklich alles loeschen = ok" | Owner-bestaetigt |
| N6 Hold-Seite nur Anzeige, Abbrechen | "Fortschritt geht" (Halteseite mit Fortschrittsanzeige) | siehe Abweichung A1 |
| N7 PIN-Sperre (3 Fehlversuche), "PIN vergessen?" erreichbar | Sperre mit Hinweis "zu viele Versuche, warten"; "ja"; ein weiterer Versuch zeigte "bitte warten" | Owner-bestaetigt. Teil "richtige PIN nach Ablauf der Sperre" in diesem Lauf nicht ausgefuehrt; Owner: bereits bei HW-188-A01 (PR #199) gemacht, hier obsolet |
| N8 Daten vorher/nachher | "ja" (unveraendert) | Owner-bestaetigt; UART: kein Reset, kein Panic/WDT/Brownout/OOM |

### Abweichung A1 (nicht von G1 gedeckt)

Der Auftrag forderte, das Halteziel nicht zu beruehren. Der Owner hat in N6
das Halteziel selbst gedrueckt, bis der Fortschrittsbalken ca. 40 % zeigte, und
dann losgelassen. Das war eine Ownerhandlung ohne G2 und nicht Teil des
freigegebenen Umfangs. Folgen:

- Kein 5000-ms-Hold, kein Werksreset: Das Log enthaelt keine Ausfuehrungszeilen
  und keinen Reset; der Owner meldet unveraenderte Daten (N8).
- Der Fortschritt lief sichtbar an; **Ownernachtrag:** nach dem Loslassen bei ca. 40 %
  fiel er auf 0 % zurueck (einmalige Ownerbeobachtung, ohne Video/UART-Beleg).
- Dies ist **kein PASS fuer P2b**: ungeplant, einmalig, nur ein Abbruchweg (Loslassen),
  kein Verschieben, keine UART-Zeile. Das Ergebnis wird als Einzelbeobachtung gefuehrt;
  P2b bleibt `NOT_RUN`, bis es nach G2 geplant wiederholt wird.

## 3a. Folgelauf nach G2: P2b und P3 (2026-10-10, 07:52–07:58 UTC) – **BEFUND: Stack Overflow und Bootloop**

G2 wurde vom Owner ausdruecklich fuer P2b + P3 erteilt (Datenverlustliste bestaetigt).
G3/G4 nicht erteilt. Geraet unveraendert `f859ef6` (Boot-Log: `source git sha f859ef632def5c4f2167192904fda5e9f339d3af`, kein Flash).

```text
ROHLOG=lokal, uart192_p2b_p3.raw.txt, 6968 Zeilen, SHA256=ccbe55312ea708b73b1ec451bca2afa16906c0eaf7dc949e6ae38c75c886bee4
ZEITRAUM=07:52:18 .. 07:58:33 UTC (Aufnahme manuell beendet); Start mit RTS-Reset-Puls (POWERON_RESET)
```

**P2b (Hold-Abbruch vor 5000 ms) = PASS (Ownerbeobachtung, UART ohne Auffaelligkeit):**
Loslassen bei ca. 30–50 % zweimal: Fortschritt fiel auf 0 %. Wegziehen vom Halteziel bei gedrueckt gehaltenem Finger: Fortschritt fiel auf 0 %. Neueintritt der Hold-Seite begann bei 0 %. Bis 07:56:27 UTC nur Heartbeats und Dispatches, kein Reset, kein Panic.

**P3 (voller Hold) = FAIL:**

- Owner: Halteziel ca. 6 s durchgehend gehalten; die Anzeige bleibt danach **eingefroren** (Hold-Seite "Werksreset / Zum Bestaetigen Taste halten", Balken fast voll; Foto vom Owner im Chat, nicht im Repository).
- UART: Heartbeats bis 07:56:27.554; **07:56:28.356 `***ERROR*** A stack overflow in task main has been detected`**, Backtrace, `Rebooting...`, `rst:0xc (SW_CPU_RESET)`.
- Danach **Bootloop**: bis 07:58:33 UTC 92 Stack Overflows und 93 Resets in der Aufnahme. In 91 der 92 Zyklen tritt der Fehler an derselben Stelle auf: unmittelbar nach `app_main: resources: point=after_platform_begin network_mode=UNSELECTED network_state=Stopped` (Backtrace `0x4008ced1:0x3ffc9550`, identisch); der erste Absturz (07:56:28, Backtrace-Frame `0x3ffc8850`) trat dagegen nach einem Heartbeat auf, also im laufenden Betrieb beim Abschluss des Holds.
- Das Log enthaelt **keine** Werksreset-Ausfuehrungs-/Ergebniszeile; ob der Reset persistiert wurde, ist nicht belegt. `network_mode=UNSELECTED` im Bootloop deutet auf einen ungueltigen/zurueckgesetzten Netzwerkzustand hin (Vermutung, nicht verifiziert).
- Fail-closed: Bei jedem Boot `inner_fan/outer_fan/buzzer=polarity_unconfirmed_no_gpio_access`, `peltier bridge: ready_all_off_not_connected_to_planner`; Aktoren physisch getrennt, `ACTUATOR_RELEASE=NO`.
- Nicht ausgefuehrt/nicht belegt: Ergebnisseite, Netzwerk-/HTTP-Stopp, Ersteinrichtung, Erhalt der Touchkalibrierung, Zustand nach Reset.
- Backtrace nicht aufgeloest (kein ELF zu `f859ef6` zur Hand; der Build `21082de` ist kein binaergleiches Image). Ursache **nicht untersucht**.
- Es erfolgte nach dem Befund kein weiterer Geraetezugriff (kein Power-Cycle, kein Flash, kein Erase, keine Tasteingaben), wie im Auftrag gefordert (bei Unerwartetem stoppen).

## 4. Status je Akzeptanztest

```text
HW-19-R01=FAIL (P3): P1/N1-N8 und P2b belegt (Ownerbeobachtung), aber der volle 5000-ms-Hold fuehrt zu
  Stack Overflow in task main (07:56:28 UTC) und anschliessendem Bootloop; kein Ergebnisbildschirm.
  belegt:  Touchweg Einstellungen -> Service (PIN) -> PIN-Seite -> "PIN vergessen?" (N1/N2),
           Warnungs-/Confirm-Seite DE/EN/ES (N3/N5), Abbrechen (N4/N5), Hold-Seite mit Fortschritt (N6),
           Hold-Abbruch Loslassen/Wegziehen -> 0 % (P2b), "PIN vergessen?" bei PIN-Sperre (N7, Ownerbeobachtung),
           Daten bis P3 unveraendert (N8). Aktoren AUS (physisch getrennt).
  nicht belegt: Ergebnisseite, Persistenz des Resets, Ersteinrichtung, Erhalt der Touchkalibrierung, Zugang SAFE_BOOT.
HW-19-R02=NOT_RUN (Netzwerk-/HTTP-Stopp beim Reset, Powercut; setzt G2/G3 voraus)
HW-19-R03=BLOCKED (kein sicherer SAFE_BOOT-/NoRuntime-Einstieg benannt; G4 nicht erteilt)
```

## 5. Offen / naechste Gates

- **Befund P3** (Stack Overflow, Bootloop) ist dem Owner vorzulegen; Geraet bleibt im Bootloop bis zur Ownerentscheidung. Wiederherstellung (Neu-Flash, ggf. NVS-/State-Erase) erfordert eine neue Ownerfreigabe (G1 gilt nur fuer App-Flash ohne Erase).
- **G2** ist verbraucht (ein Vollreset ausgefuehrt); ein weiterer Hold-Test erst nach Befundanalyse und neuer Freigabe.
- **G3** Powercut-Cutpoints, **G4** SAFE_BOOT/NoRuntime: nicht erteilt.
- Heap-Minimum-Entwicklung im Folgelauf mitmessen.
