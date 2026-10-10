# Issue #192 – Werksreset-Hardwareverifikation: N1–N8, P2b und P3 auf `main` 21082de (2026-10-10)

Plan: `docs/tasks/issue-192-factory-reset-hardware-verification-plan.md` (Revision 3).
Die Evidence vom 2026-10-08 (`ISSUE192_FACTORY_RESET_HW_20261008_EVIDENCE.md`,
Baseline `df5a3dd`) bleibt historisch und wird nicht uebernommen.

Ownerfreigaben (Stand 2026-10-10, nach Diagnose): **G1 voll** fuer diesen
Plan-/Firmwarestand. **G2 wurde im Verlauf ausdruecklich fuer P2b + P3 erteilt**
(ein Vollreset-Versuch ausgefuehrt; G2 damit verbraucht, Befund siehe 3a/3b).
**G5 wurde erteilt und ausgefuehrt** (App-only-Recovery-Flash `b9564e6`, P3b: stabiler Boot, Display normal und Einstellungen zurueckgestellt laut Ownerbeobachtung; G5/P3b `PASS`, Abschnitt 3d). **G3 und G4 nicht erteilt**, G2-R3 gesperrt. `ACTUATOR_RELEASE=NO`; Aktoren physisch
getrennt/deaktiviert, kein Aktortest. Die Abschnitte 1–3 beschreiben den jeweils
datierten Stand ihres Laufs (N1–N8 vor G2); der aktuelle Stand steht in 3a, 3b
und 4.

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
  *(Stand N1–N8-Lauf; P2b wurde danach nach G2 geplant ausgefuehrt = PASS, siehe 3a.)*

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
- *(Stand bei Befundaufnahme, Commit `7ab43c4`: Backtrace nicht aufgeloest, kein ELF zu `f859ef6` zur Hand, Ursache nicht untersucht.)* **Inzwischen offline diagnostiziert, siehe 3b.**
- Es erfolgte nach dem Befund kein weiterer Geraetezugriff (kein Power-Cycle, kein Flash, kein Erase, keine Tasteingaben), wie im Auftrag gefordert (bei Unerwartetem stoppen).

## 3b. Offline-Diagnose P3 (2026-10-10, ohne Geraetezugriff, ohne Firmwarefix)

Umfang: nur Auswertung der lokalen Rohlogs und des zum geflashten Image passenden
ELF. Kein Geraetezugriff, kein Flash, kein Erase, kein Powercycle, keine
Produktcodeaenderung. Hilfsskripte (Disassembly-Auswertung) sind lokal und nicht
Teil dieses PR.

### Belegte Provenienz des ELF (Hashes gegen HW-188-A01)

```text
ELF_QUELLE=lokales Artefakt der HW-188-A01-Session (nicht im Repository): esp32_fermentationsschrank.elf, SOURCE_HEAD f859ef6, esp32_release, ESP-IDF v6.1
APP_ELF_SHA256=93535f2bb41f10c663aa4116ec16daf61279dee25f736a7c0c2bc25cdcec260b   == HW-188-A01 APP_ELF_SHA256 (identisch)
APP_BIN_SHA256=445a460a83c64cce99cb402e9c6299641dca8f5b6a60a3526a75d84134cd23b2   == HW-188-A01 APP_BIN_SHA256 (identisch, SHA256SUMS des Artefakts OK)
GERAET_MELDET=jeder Boot der Rohlogs: "app_init: ELF file SHA256: 93535f2bb..." (Praefix, in allen Bootzyklen)
CONFIG=CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576, CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY=y, Main-Task auf CPU0
```

Das ELF ist damit das zum Geraet passende Image (`f859ef6`); `21082de` wurde
**nicht** als Ersatz verwendet. Alle Zahlen unten gelten fuer `f859ef6`.

### Dekodierter Backtrace (belegt)

Beide Fehlerstellen (erster Absturz 07:56:28.356 und 91 Folgeabstuerze ab
07:56:29.762, insgesamt 92 `A stack overflow in task main`-Meldungen bei 92
`app_init`-Bootzeilen inkl. des normalen Boots 07:52:19) liefern dieselben sechs
aufloesbaren Frames; sie zeigen die **Erkennungsstelle**, nicht die
ueberlaufende Funktion (addr2line gegen das verifizierte ELF):

```text
panic_abort -> esp_system_abort -> vApplicationStackOverflowHook (port.c:577)
  -> vTaskSwitchContext (tasks.c:3698) -> _frxt_dispatch -> _frxt_int_exit
```

- Die Canary-Pruefung laeuft erst beim Kontextwechsel. Die ueberlaufende
  Aufrufstelle ist deshalb **nicht aus dem Backtrace ableitbar**
  (`NOT_RESOLVED` fuer die exakte Callsite des ersten Schreibzugriffs ausserhalb
  des Stacks).
- Erster Absturz: siebter Frame `0x40097655` = `memspi_host_read_status_hs`
  (spi_flash/memspi_host_driver.c:126); die Backtrace-Zeile ist `|<-CORRUPTED`
  (SP-Wert dieses Frames `0x3ff0005c`). Beobachtung: Der zuletzt unterbrochene
  Kontext befand sich in einer Flash-Statusabfrage (Flash-Operation). Das ist
  mit einer laufenden Persistierung vereinbar, beweist aber nicht welche.
- 91 Folgeabstuerze: byte-identische Backtrace-Zeile (gleiche SP-Werte
  `0x3ffc9550`..`0x3ffc9630`), endet nach dem sechsten Frame mit
  `0x4008d53a:0x00000014 |<-CORRUPTED`; keine Aussage ueber den unterbrochenen
  Code. Das Muster (immer direkt nach `after_platform_begin`, byte-identisch)
  ist deterministisch und spricht fuer einen reproduzierbaren Zustand.
- Zeitbezug: Das Halten des Touch-Ziels wird im UART nicht protokolliert (nur
  Press-Dispatch; letzte `touch press dispatch`-Zeile 07:56:15.520). Die
  Heartbeats liefen im Sekundentakt bis 07:56:27.554, der Absturz folgte um
  07:56:28.356; Ownerbeobachtung: Anzeige friert nach ca. 6 s ein. Ein
  Logeintrag, der Hold-Start, Hold-Ablauf (5000 ms) und Absturz verknuepft,
  existiert nicht. Zeitbezug daher `NOT_RESOLVED`.

### Gemessener Stackverbrauch (belegt, Main-Task, 24576 B)

`stack_hwm_bytes` ist die gemessene Mindest-Restgroesse. Aus den Rohlogs:

| Messpunkt | stack_hwm_bytes | belegter Verbrauch |
|---|---|---|
| `after_platform_begin` (N1–N8-Lauf, Boot 07:2x) | 18632 | 5944 B |
| `after_application_begin`, `after_ui_init`, `idle`/`periodic_30s` (vor PIN-Nutzung) | 11800 | 12776 B |
| `idle_120s` nach PIN-Seiten, PIN-Pruefung und Sperre (2 Messpunkte, N1–N8) | 4312 | 20264 B |

Schon die normale Bedienung (N1–N8) liess nur noch 4312 B Reserve. In den
Bootloop-Zyklen liegt `after_platform_begin` bei 18408..18728 B HWM (92 Boots,
meist 18712/18728); eine Messung nach dem Reset-Versuch oder eine
`idle`-Zeile nach dem Absturz existiert nicht. Die Zeile `after_platform_begin`
steht vor `application.begin()`; ihr `network_mode=UNSELECTED` ist deshalb kein
Hinweis auf einen Persistenz-/Epochenzustand und wird nicht dafuer verwendet.

### Statische Framegroessen (belegt aus Disassembly, `entry a1, N`)

Reine Framegroessen der betroffenen Funktionen (Xtensa, Bytes; ohne Callees):

| Funktion | Frame |
|---|---|
| `makeAuthorizedEpochHandoffTarget` (`run_persistence_coordinator.cpp`, einzige Aufrufstelle in `prepareAuthorizedEpochHandoff`) | **17520** (`0x4470`) |
| `RunPersistenceCoordinator::prepareAuthorizedEpochHandoff` | **12160** |
| `RunPersistenceCoordinator::finalizeAuthorizedEpochHandoff` (make inline) | 8512 |
| Lambda `validateReferenced` in prepare | 3984 |
| `app_main` | 4928 |
| `updateProductUi` / `processWorkspaceTouch` / `updateFactoryResetHold` / `beginAuthorizedFactoryReset` | 816 / 2576 / 112 / 816 |
| `FermentationApplication::begin` / `beginPersistent` / `completeAuthorizedEpochHandoff` | 64 / 672 / 400 |

Treiber der Groessen (DWARF): `RunPersistenceRawRecord` = 3952 B,
`RunPersistenceSnapshot` = 3904 B. `AuthorizedEpochHandoffTarget` haelt zwei
`RunPersistenceRawRecord` (ca. 7,9 kB); `makeAuthorizedEpochHandoffTarget` haelt
zusaetzlich `RunCommandState`, `RunPersistenceSnapshot` und das Target als lokale
Objekte; `prepareAuthorizedEpochHandoff` haelt Target (optional) und
`RunPersistenceRawRecord record` lokal.

Direkte Aufrufketten (nur Summe der Frames der unmittelbar geschachtelten
Funktionen; Untergrenze, ohne weitere Callees, virtuelle Aufrufe in den
Statestore/NVS/Flash und Interrupt-Frames):

```text
Reset-Hold:  app_main 4928 -> updateProductUi 816 -> processWorkspaceTouch 2576
             -> updateFactoryResetHold 112 -> beginAuthorizedFactoryReset 816
             -> prepareAuthorizedEpochHandoff 12160 -> makeAuthorizedEpochHandoffTarget 17520
             = 38 928 B   (Budget 24 576 B; Ueberschreitung >= 14 352 B)
Boot-Resume: app_main 4928 -> application.begin 64 -> beginPersistent 672
             -> completeAuthorizedEpochHandoff 400 -> prepareAuthorizedEpochHandoff 12160
             -> makeAuthorizedEpochHandoffTarget 17520
             = 35 744 B   (Ueberschreitung >= 11 168 B)
Beide Pfade: prepare + make allein = 29 680 B > 24 576 B, unabhaengig vom Aufrufer.
```

Der Boot-Resume-Pfad ruft `prepareAuthorizedEpochHandoff` nur bei
`AuthorizedRunEpochHandoffPhase::Pending` (`completeAuthorizedEpochHandoff`,
`fermentation_application.cpp`); die Phase `Committed` geht direkt in
`finalizeAuthorizedEpochHandoff` (Frame 8512, kein 17520-B-Aufruf: ca. 14,6 kB
direkt geschachtelt, unter dem Budget).

Grenzen der statischen Analyse: Frames stammen aus `entry`-Instruktionen;
Callees ueber virtuelle Aufrufe (`callx8`), Funktionszeiger und Interrupt-Frames
fehlen, die Summen sind daher Untergrenzen. Die oben genannten Ketten benutzen nur
im Disassembly verifizierte direkte `call8`-Kanten
(`updateFactoryResetHold` -> `beginAuthorizedFactoryReset` (`400ef6d0`) ->
`prepareAuthorizedEpochHandoff` (`40104be8`) -> `makeAuthorizedEpochHandoffTarget`
(einzige Aufrufstelle `40104de1`); `beginPersistent` -> `completeAuthorizedEpochHandoff`
(`400e88ad`) -> `prepare`). Zusaetzlich ruft `beginAuthorizedFactoryReset` die
`ConfigurationRecoveryService::beginAuthorizedFactoryReset` (Frame 2096) als
getrennten, nicht zur Kette addierten Aufruf. Eine fruehere Zwischenauswertung
mit abgeschnittenen Template-Namen hat diese Kante verloren und ist verworfen.

### Bewertung

**Belegt:** (1) Identisches ELF/Bin gegen HW-188-A01, vom Geraet gemeldeter
ELF-Praefix. (2) Der Fehler ist ein Stack-Overflow des Main-Tasks, erkannt per
Canary beim Kontextwechsel. (3) `prepareAuthorizedEpochHandoff` benoetigt in
diesem Build allein mit seinem Callee `makeAuthorizedEpochHandoffTarget` 29 680 B
Frames und damit mehr als den gesamten Main-Task-Stack (24 576 B); die
Reset-Hold- und die Boot-Resume-Kette (Phase `Pending`) enthalten diese Kombination.
(4) Normale Bedienung hatte nur 4312 B Reserve.

**Hypothese (nicht bewiesen):** Es liegt **ein gemeinsames Stack-Budget-Problem**,
keine zwei unabhaengigen Fehler vor: Der Hold-Abschluss ruft
`beginAuthorizedFactoryReset()` -> `prepareAuthorizedEpochHandoff()` ->
`makeAuthorizedEpochHandoffTarget()` und ueberschreitet den Stack deutlich
(Speicher unterhalb des Main-Stacks wird beschrieben, Erkennung erst beim
naechsten Kontextwechsel). Die 91 deterministischen Folgeabstuerze an derselben
Stelle direkt nach `after_platform_begin` sind mit demselben Mechanismus
vereinbar, falls beim Reset ein offener Run-Epochen-Handoff (`Pending`)
persistiert wurde und `application.begin()` ihn per
`completeAuthorizedEpochHandoff` -> `prepareAuthorizedEpochHandoff` wieder
aufnimmt. Das Boot-Verhalten aenderte sich bei identischem Image zwischen
07:52 (normaler Boot) und 07:56:29 (Absturz); damit hat sich **persistenter
Zustand geaendert**, welcher, ist aus den Logs nicht ableitbar.

**Nicht belegt / `NOT_RESOLVED`:** exakte Callsite des Schreibzugriffs ausserhalb
des Stacks; welche Persistenzschritte des Resets (Recoverykern `Resetting`,
Epoche, Handoff-Phase, Slot-/Head-Schreibvorgaenge) vor dem Absturz abgeschlossen
wurden; ob der Bootloop tatsaechlich im `Pending`-Handoff liegt; Wirkung der
Speicherueberschreibung unterhalb des Stacks; ob die beobachtete
Anzeige-Erstarrung vom selben Mechanismus stammt oder eine eigene Ursache hat. Der erreichte Persistenz-/Epochenzustand wird
ausdruecklich **nicht** aus `network_mode=UNSELECTED` oder fehlenden
Erfolgsmeldungen abgeleitet.

**Nebenbefund (statisch, `UNVERIFIED_STATIC`):** Im selben ELF gibt es einen
weiteren tiefen direkten Pfad: `app_main` 4928 -> `updateProductUi` 816 ->
`processWorkspaceTouch` 2576 -> `dispatchWorkspacePress` 2096 ->
`applyConfirmedPrepared` 10880 -> `FermentationUiCommandBridge::decidePrepared`
48 -> Visit-Lambda-Varianten mit je 20240 B Frame. Die Summe der Frames liegt
bei ca. 41,6 kB und damit ueber 24 576 B. Frames werden vom Compiler fuer die
gesamte Funktion reserviert; ob der Pfad auf dem Geraet tatsaechlich so tief
belegt wird, ob ein Lambda-Frame real beschrieben wird und ob der Pfad bei der
Qualifizierung erreicht wurde, ist **nicht untersucht**. Er gehoert nur in die
Ownerentscheidung zum Umfang einer Stack-Absicherung.

### Kleinster KISS-Fixkandidat (Vorschlag; umgesetzt in C2, siehe 3c)

Nur Stackverbrauch der Handoff-Funktionen senken, **ohne** Task-Stack oder
Worker-Task zu aendern:

1. `makeAuthorizedEpochHandoffTarget` liefert das Target per Nothrow-Heap
   (`std::unique_ptr`, wie bereits `RunPersistenceLoadResult` und
   `RunPersistenceCoordinator` in `beginPersistent`) statt als
   `std::optional` per Wert; die lokalen Grossobjekte (`RunCommandState`,
   `RunPersistenceSnapshot`) liegen in einem kurzlebigen Heap-Scratch.
2. Die lokalen `RunPersistenceRawRecord record` in `prepareAuthorizedEpochHandoff`
   (Hauptschleife und Lambda) nutzen einen gemeinsamen Heap-Scratch.
3. Allokationsfehler enden fail-closed (`Blocked`, bestehender Grund
   `InvalidProjection`/`CodecError`); keine neue Zustands- oder Vertragsflaeche.

Stack-Bedarf von `prepare`/`make`/`finalize` sinkt damit auf Zeiger-/Kleinobjekt-
Frames (Zielgroesse im Fix-Plan festzulegen). Der transiente Heap-Spitzenbedarf
ist im Fix-Plan zu begrenzen (sequentielle Record-Erzeugung, ein
wiederverwendetes Scratch) und **zu messen**: belegt ist nur der knappe freie
Heap im Betrieb (Rohlog N1–N8: frei 43 184 B, groesster Block 40 960 B, Minimum
bis 34 532 B). Eine Erhoehung von `CONFIG_ESP_MAIN_TASK_STACK_SIZE` um >= 15 kB
oder ein Worker-Task ist **nicht** vorgesehen: sie wuerde einen grossen Teil des
verbleibenden DRAM binden und das Problem nur verschieben; ein begruendeter
Befund dafuer liegt nicht vor.

Gezielte Regressionsnachweise (Vorschlag):

- **Statisch/offline** am Release-ELF: Framegroesse von `prepare`, `make`,
  `finalize` und die direkt geschachtelte Kette von `updateFactoryResetHold` und
  `completeAuthorizedEpochHandoff` unter Budget abzueglich vereinbarter Reserve
  (Skript/Gate; Umfang Ownerentscheid).
- **Host:** bestehende Handoff-Tests (`Pending`/`Committed`-Wiederaufnahme,
  Orphan, Kapazitaet, Idempotenz) unveraendert gruen; ein Test fuer den
  Allokationsfehlerpfad, soweit ohne neue Test-Infrastruktur moeglich.
  Host-Tests beweisen den Stack **nicht**.
- **Hardware (neue Freigabe):** P3 wiederholen mit Logzeile
  `stack_hwm_bytes` unmittelbar nach dem Reset und nach dem Folgeboot
  (HWM-Reserve gegen den zu vereinbarenden Wert); Ergebnisseite, Ersteinrichtung,
  Touchkalibrierung wie in P3 geplant.

### Ownerentscheidungen (Stand Planrevision 3)

1. **Fixscope/Issue (entschieden):** Der Owner hat angeordnet, den Fix im
   bestehenden Draft-PR #200 fortzufuehren (kein neues Issue, kein neuer PR).
   Der begrenzte Fix am autorisierten Run-Epochen-Handoff ist in Plan
   Revision 3 (Abschnitt 4) beschrieben; der Owner hat den exakten Plan-Commit
   `43a65d3` freigegeben, die Software-Umsetzung C1–C4 liegt vor (Abschnitt 3c,
   Review und Fix Verification akzeptiert; G5 ausgefuehrt, Abschnitt 3d); die allgemeine Stack-Absicherung (Gate, UI-Command-Pfad) bleibt
   FOLLOW-UP.
2. **Geraeterecovery (neue Freigabe G5 nach Software-Review, G1 deckt sie nicht;
   Plan Revision 3, Abschnitt 5.1; Ausgangsoptionen):** (A) Geraet bleibt im
   fail-closed Bootloop, bis eine korrigierte App-Firmware (ohne Erase) geflasht
   wird; sie wuerde, falls die Hypothese stimmt, den offenen Handoff
   wiederaufnehmen und zugleich als Nachweis dienen. (B) Erase von State/NVS plus
   App-Flash: Daten inkl. Touchkalibrierung (`tc0`/`tc1`) gehen verloren,
   Ersteinrichtung noetig. (C) Den bestehenden `main`-Stand erneut flashen hilft
   nicht (gleicher Fehler). Empfehlung: (A).
3. **PR #200:** bleibt Draft und enthaelt nach der Planfreigabe (`43a65d3`) zusaetzlich den Fix;
   Wiederholungstest nur nach G5 und G2-R3 (Plan Revision 3, Abschnitt 5).

## 3c. Software-Fix und statischer Stacknachweis (2026-10-10, ohne Geraetezugriff)

Plan Revision 3 (Commit `43a65d3`, vom Owner freigegeben) wurde in C1–C3
umgesetzt: Golden-Bytes-Test (C1, `79ef0ce`), Heap-Scratch im autorisierten
Handoff (C2, `48bd1f3`), Messung und Evidence (C3, dieser Abschnitt). Wireformat,
State-Store-/Epoch-/`Pending`-/`Committed`-/Idempotenzvertraege sind unveraendert
(Golden-Test aus C1 unveraendert gruen). Fallback D2 war nicht noetig. Die
Messung ist **statisch** (ELF-Disassembly); der Nachweis am Geraet steht aus und
setzt G5/G2-R3 voraus (Abschnitt 5).

Methode: `scripts/analyze_issue_192_handoff_stack.py ELF` (Frame `entry a1, N`
je Funktion, direkte `call0/4/8/12`-Kanten, laengster Pfad `app_main` -> Kette).
Kalibrierung mit `--calibrate` gegen das ELF von `f859ef6` (Hash-verifiziert,
Abschnitt 3b): reproduziert 17520 / 12160 / 8512 B und 35 744 B (K-BOOT-P)
exakt. K-HOLD ergibt im Callgraph **38 960 B** statt der manuellen 38 928 B aus
3b, weil der Release-Pfad der Hauptschleife zusaetzlich
`main_ui::releaseFactoryResetHold` (32 B) durchlaeuft; die Differenz ist
erklaert, die 3b-Zahl war eine Untergrenze.

### Framegroessen vorher/nachher (Release-ELF `48bd1f3`, Bringup identisch)

| Funktion | vorher (`f859ef6`) | nachher |
|---|---|---|
| `makeAuthorizedEpochHandoffTarget` | 17 520 B | **5 680 B** |
| `prepareAuthorizedEpochHandoff` | 12 160 B | **608 B** |
| `finalizeAuthorizedEpochHandoff` | 8 512 B (make inline) | **656 B** |

### Ketten gegen Stackbudget 24 576 B (Reserve-Ziel 6 144 B, Kettengrenze 18 432 B)

| Kette | direkt benannt vorher | direkt benannt nachher | inkl. tiefstem Callee vorher | inkl. tiefstem Callee nachher | Reserve nachher |
|---|---|---|---|---|---|
| K-HOLD (`beginAuthorizedFactoryReset` -> `prepare`) | 38 960 B | **15 568 B** | 39 920 B | **16 528 B** | **8 048 B** |
| K-BOOT-P (`completeAuthorizedEpochHandoff` -> `prepare`, Phase `Pending`) | 35 744 B | **12 352 B** | 36 704 B | **13 312 B** | **11 264 B** |
| K-BOOT-C (`completeAuthorizedEpochHandoff` -> `finalize`, Phase `Committed`) | 14 576 B | **12 400 B** | 15 280 B | **13 360 B** | **11 216 B** |

Release und Bringup liefern identische Werte; alle drei Ketten liegen unter der
Kettengrenze (18 432 B). Das Stackgate aus Plan 4.5 ist **statisch erfuellt**.
Die schwerste Kette K-HOLD besteht aus `app_main` 4 928, `updateProductUi` 816,
`processWorkspaceTouch` 2 576, `releaseFactoryResetHold` 32,
`updateFactoryResetHold` 112, `beginAuthorizedFactoryReset` 816, `prepare` 608,
`make` 5 680 B; der tiefste Callee unterhalb von `make` (Encode/Validate) addiert
weitere 960 B.

Grenzen (explizit, die Summen sind Untergrenzen): Das Skript zaehlt nur direkte
Kanten. Unaufgeloeste `callx`-Kanten auf den Ketten (Release): K-HOLD 148 in 17,
K-BOOT-P 110 in 14, K-BOOT-C 112 in 14 Funktionen (u. a. State-Store-Port,
`ConfigurationRecoveryService`); Bringup 150 / 113 / 113. Calls ins Symbol-Innere
(geteilter/ausgelagerter Code) werden nicht als Kanten gezaehlt: 458 (Release),
496 (Bringup). Interrupt-Frames und ROM-Callees ohne `entry` sind nicht erfasst.
Die Aussage ist daher "unter der Kettengrenze nach direktem Callgraph", nicht
"Stackverbrauch am Geraet gemessen". Der Ursprungsueberlauf bleibt Hypothese
(Callsite `NOT_RESOLVED`, Abschnitt 3b). Der separat gefundene UI-Command-Stackpfad
(ca. 41,6 kB, `UNVERIFIED_STATIC`) ist nicht Teil dieses Fixes und bleibt FOLLOW-UP;
das Skript ist bewusst nicht in `run_pre_ready_gates.sh`/CI verdrahtet
(Ownerentscheid ausstehend).

### Heap-Spitze und Reihenfolge Netzwerkstopp/Handoff

Der Fix legt je `prepare`/`finalize` genau einen `RunPersistenceRawRecord`
(3 952 B) per `new (std::nothrow)` an, sequentiell und kurzlebig; Spitze wie in
Plan 4.4 (ca. 4,4 kB, groesster Block 3 952 B). Ein Allokationsfehler endet vor
jedem Schreibzugriff mit `reject(CandidateApply, InvalidProjection)`,
`durability == Unchanged` (Host-Test `test_issue192_handoff_scratch_allocation_failure_fails_closed`).

Reihenfolge in `beginAuthorizedFactoryReset` (`fermentation_application.cpp`):
`closeAndDrain()` -> `ConfigurationRecoveryService::beginAuthorizedFactoryReset`
-> `prepareAuthorizedEpochHandoff` -> `commitAuthorizedRunEpochHandoff`.
`endNetworkAfterFactoryReset()` laeuft erst im Aufrufer nach Core-Reset **und**
Handoff. Das Netzwerk ist beim Handoff-`prepare` daher noch aktiv; massgeblich ist
die Netzwerkmodus-Heap-Evidenz (`R1_RAM_S8_EVALUATION.md`: kleinster gesampelter
groesster Block 7 168 B, Low-Water 4 124 B), nicht die Betriebswerte
(40 960 B Block). Die Reserve im groessten Block betraegt dort ca. 3,2 kB; der
Low-Water-Wert ist fuer den Reset-Zeitpunkt **nicht belegt**. Die Heap-Spitze am
Geraet wird erst im Retest (Plan 5.2) gemessen.

## 3d. G5 – App-only-Recovery-Flash und P3b (2026-10-10, 10:2x UTC)

Owner-Freigabe G5: ausschliesslich der App-only-Flash des Images aus
`SOURCE_HEAD=b9564e67a2d9dc067bc4d6a873c376cb7f3ca4b3` (SHA-256 unten) mit dem
vorgelegten esptool-Kommando auf Offset `0x10000`, danach P3b; kein NVS-/State-Erase
(Entwicklungsboard ohne wichtige Benutzerdaten, aber der vorhandene Recoveryzustand
sollte geprueft werden). G2-R3, G3, G4 und `ACTUATOR_RELEASE` bleiben gesperrt.

Korrektur der Freigabevorlage: Das Image-Ende war dort falsch mit `0x1B5260`
angegeben. Richtig: Laenge 1 739 104 B = `0x1A8960`, Bereich `0x10000`–`0x1B895F`
(Ende exklusiv `0x1B8960`), innerhalb der Partition `factory` (bis `0x300000`).
esptool meldete `Flash will be erased from 0x00010000 to 0x001b8fff` (4-KiB-Sektor-
Rundung), also nur Bereich der App.

```text
SOURCE_HEAD=b9564e67a2d9dc067bc4d6a873c376cb7f3ca4b3, Profil esp32_release, ESP-IDF v6.1
IMAGE_SHA256=cc0f0ffffec6a76447c377a91cf2303033e86089925b812814710f751eb38308 (vor dem Flash erneut geprueft: OK, 1 739 104 B)
ELF_SHA256=fc837421c2ea87f3a5355f3c5679218655b1329421399a3dd32ceb22e739c910
FLASH=genau 1x, esptool 5.4.0, 115200 Baud, --before default-reset --after hard-reset, write-flash 0x10000, Exit 0,
  "Hash of data verified.", kein Erase-Befehl, kein weiterer Flashbereich
CHIP=ESP32-D0WD-V3 rev v3.1, Flash 4MB dio 40m
FLASHLOG=lokal (~/.cache/ev192-build/g5_flash_b9564e6.log), UARTLOG=lokal (g5_p3b_uart_b9564e6.log, 573 Zeilen), nicht im Repository
```

Boot-Beleg (UART, eine Aufnahme ca. 150 s):

```text
Bootloader: ESP-IDF v6.1, Partitionstabelle unveraendert (factory 0x10000/0x2f0000, state_store 0x300000/0x100000),
  "Loaded app from partition at offset 0x10000"
app_init: App version b9564e6, Compile time Oct 10 2026 12:06:00, ELF file SHA256 fc837421c... (= ELF_SHA256)
app_main: profile esp32_release, source git sha b9564e67a2d9dc067bc4d6a873c376cb7f3ca4b3
app_main: hardware state HARDWARE_UNVERIFIED, actuator policy REQUIRE_VERIFIED_HARDWARE, real actuators: disabled
app_main: application: ready (nach application_begin, ca. 1,4 s Uptime)
PANIC/GURU/TASK_WDT/BROWNOUT/OOM/STACK_OVERFLOW=0; weiterer Reset nach dem Boot=0; Herzschlag 148 Zeilen bis uptime 148,8 s
stack_hwm_bytes (Main-Task, kleinste freie Reserve): 18632 (after_platform_begin) -> 11800 (after_application_begin, unveraendert bis idle_120s)
Heap (after_application_begin): frei 135232, Minimum 131260, groesster Block 110592
Heap (idle_120s): frei 108644, Minimum 104700, groesster Block 98304 (nach UI-Init; LVGL-Pool 29 % belegt)
```

Einordnung (nur Belegtes):

- **Boot-/Recovery-Pfad nicht aus dem Log ableitbar.** Die Firmware loggt weder den
  persistierten Handoff-Zustand noch einen Wiederaufnahmepfad. Der Boot erreicht
  `application: ready`; ob ein `Pending`-Handoff vorlag und abgeschlossen wurde, ob
  `Committed`/`Idle` vorlag oder der Zustand anders war, ist **nicht belegt**. Die
  Hypothese aus Plan 5.1 ist damit weder bestaetigt noch widerlegt; belegt ist nur,
  dass der Start mit der korrigierten Firmware den Bootloop nicht reproduziert.
- Die Messung `stack_hwm_bytes=11800` entspricht einer Spitzennutzung von
  12 776 B des 24 576-B-Stacks bis zum Messpunkt. Sie deckt den Bootpfad der
  Hauptaufgabe, **nicht** den Hold-/Reset-Pfad (K-HOLD); der bleibt P3c vorbehalten.
- **Aufnahme-Artefakte, nicht bewertet:** Die Aufnahme setzt beim Start absichtlich
  einen RTS-Resetpuls; das protokollierte Booten ist daher der Neustart durch diesen
  Puls und nicht notwendig der erste Start nach dem esptool-`hard-reset`. Dieser
  erste Start ist nicht aufgezeichnet. Die ersten ca. 200 ms der Datei enthalten
  144 identische, abgeschnittene Zeilen (`...fe test mode, uptime_ms=3804`) und 101
  ROM-Bannerzeilen mit identischem Empfangszeitstempel; das sind beim Oeffnen des
  Ports ausgelesene Puffer-/Restdaten (nicht als echte Resets interpretierbar, da
  101 ROM-Boots nicht in 200 ms Platz haben). Die Restzeile `uptime_ms=3804`
  entspricht der Heartbeat-Form eines frueheren Starts, ist aber kein Beleg fuer
  dessen Verlauf.
- Nicht beobachtet vom Agenten: die physische Trennung der Aktoren (Ownervorgabe
  seit G1, unveraendert; Firmwareseite: `real actuators: disabled`, Policy
  `REQUIRE_VERIFIED_HARDWARE`). Die Display-/Startanzeige ist durch die
  Ownerbeobachtung (unten) belegt, nicht durch UART.
- Vertrauliche Inhalte: Die UART-Zeilen enthalten keine Zugangsdaten; die Rohdatei
  bleibt dennoch lokal.

Ownerbeobachtung (2026-10-10, Chat, nach dem G5-Flash), Wortlaut: **«Display
normal. Einstellungen zurückgestellt. Alles wie erwartet.»** Kennzeichnung:
`PASS_OWNER_OBSERVED` (keine UART- oder automatische Messung, kein Foto/Video).
Darueber hinausgehende Detailpruefungen (einzelne Einstellungen,
Kalibrierungswerte, Ergebnisbildschirm) wurden weder durchgefuehrt noch werden sie
behauptet.

Status G5/P3b:

```text
G5_FLASH=PASS (UART-/Flash-Evidence oben)
P3b_BOOT=PASS (stabiler Boot b9564e6, kein Panic/WDT/Reset in ca. 150 s)
P3b_DISPLAY_SETTINGS=PASS_OWNER_OBSERVED
G5/P3b=PASS (abgeschlossen)
HANDOFF_PFAD_Pending/Committed=NOT_RESOLVED (nicht geloggt; zurueckgestellte Einstellungen sind nur ein Indiz, kein Beweis des exakten Recoveryverlaufs)
```

Folge: Das Geraet ist wiederhergestellt (kein Bootloop mehr) und bootet stabil mit
`b9564e6`. Das ist **kein** Nachweis fuer einen Werksreset am neuen Stand;
`HW-19-R01` bleibt `FAIL` bis zum separat freizugebenden P3c-Retest (G2-R3, nicht
freigegeben). `HW-19-R02=NOT_RUN`, `HW-19-R03=BLOCKED`. Kein zweiter Flash, kein
Erase, kein Powercycle-Experiment, kein Hold-Test.

## 4. Status je Akzeptanztest

```text
HW-19-R01=FAIL (P3; P3b-Recovery mit b9564e6 belegt, 3d, aber kein Reset-Nachweis; FAIL bis P3c): P1/N1-N8 und P2b belegt (Ownerbeobachtung), aber der volle 5000-ms-Hold fuehrt zu
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

- **Befund P3** (Stack Overflow, Bootloop): Offline-Diagnose liegt vor (3b; gemeinsames Stack-Budget-Problem als Hypothese, belegt: `prepare` + `make` = 29 680 B Frames > 24 576 B Stack). **G5/P3b ist abgeschlossen (3d, `PASS`): das Geraet bootet mit `b9564e6` stabil** (`application: ready`, kein Panic/WDT/Reset in ca. 150 s; Display normal und Einstellungen zurueckgestellt laut Ownerbeobachtung, `PASS_OWNER_OBSERVED`). Der interne Handoff-Pfad bleibt `NOT_RESOLVED` (3d). Offen: **G2-R3 / P3c** (erneuter 5000-ms-Werksreset mit `stack_hwm_bytes` und Heap-Spitze, Plan 5.2) nur nach separater Ownerfreigabe.
- **G2** ist verbraucht (ein Vollreset ausgefuehrt); ein weiterer Hold-Test erst nach Befundanalyse und neuer Freigabe.
- **G3** Powercut-Cutpoints, **G4** SAFE_BOOT/NoRuntime: nicht erteilt.
- Heap-Minimum-Entwicklung im Folgelauf mitmessen.
