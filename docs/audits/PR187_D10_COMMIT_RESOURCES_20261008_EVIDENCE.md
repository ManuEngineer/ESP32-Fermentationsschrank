# PR #187 / Issue #172 – D10 Commit-Ressourcenmessung auf realer Hardware (2026-10-08)

Schliesst `D10_COMMIT_RESOURCE_LOGS` aus `PR187_HW_SMOKE_3918C50_20261007_EVIDENCE.md`.
Bedient wurde das Geraet vom Owner; die Messwerte stammen aus der UART-Aufnahme.

```text
BASE_HEAD=3918c508e6c5d10fc09c80b32003585534990c23 (ESP-IDF v6.1, fff9895c82d744c7237be8847347bdd1b07c6643, Profil esp32_release)
PROBE=docs/audits/PR187_RAM_DIAG_AND_D10_PROBE_3918c50.patch (unveraendert angewendet, sha256 acbaf8eba433947416e6bce77499b40c6a7dbeedd86c7c4ca2f88d16bcaeee66), App version 3918c50-dirty
PROBE_APP_SHA256=4c17c10ce6e3300482f3914c583bd95d93cdcf6b3f2d92b57ab4a3811f53d2b5
FLASH=nur App bei 0x10000, kein Erase, kein Bootloader, Partitionstabelle byte-identisch (d7f180e4...ccd9d); NVS/State-Store unberuehrt
RESTORE=sauberer Neubau 3918c50 (--require-clean-source-tree), App-SHA256 8cf684842fcbd23e4f70c1a7b121ef7f71c971ca7f7f7cb337fe974f3df0ef16 (neuer Hash, Build nicht byte-reproduzierbar), nur App geflasht, Boot: App version 3918c50, application: ready
ACTUATOR_RELEASE=NO
MESSFENSTER=2026-10-08 19:33-19:40 (Hostzeit), Aufnahme ab Neustart des Probe-Builds
```

## Ergebnis je Commit-Pfad

```text
D10_S3_LANGUAGE_COMMIT=PASS   (3 Commits: DE>EN, EN>ES, ES>DE)
D10_S6_PROGRAM_COMMIT=PASS    (1 Commit)
D10_S10_DEVICE_NAME_COMMIT=PASS (2 Commits: Namensaenderung, spaetere Wiederherstellung)
HEAP_ALLOC_FAILED=0  PANIC=0  WATCHDOG=0  BROWNOUT=0  BAD_ALLOC=0  (Suche ueber die gesamte Aufnahme)
UNEXPECTED_RESETS=0  (1 Reset 19:37:48 POWERON_RESET vom Owner ausgeloest fuer den Persistenzcheck; 19:33:31 = Aufnahmestart)
```

PASS heisst hier: der Commit-Press erreichte den Owning-Pfad (`touch press dispatch:
outcome=2` = `OwningOutcome`), ohne Allokationsfehler, Panic, Watchdog oder
ungeplanten Reset. Es sind keine Grenzwerte definiert oder abgeleitet; die Werte
sind Messdaten.

## Messwerte der Commit-Presses (Bytes, vor / nach dem Press)

`min` = minimum_free_heap, `gross` = groesster 8-Bit-Block, `Stack` =
`uxTaskGetStackHighWaterMark` der Hauptschleife (Bytes frei), LVGL = Pool-Statistik.

| Uhrzeit | Pfad | frei | min | gross | Stack | LVGL-Pool frei | LVGL groesster | LVGL max genutzt |
|---|---|---|---|---|---|---|---|---:|
| 19:35:20 | S3 DE>EN | 44724 / 44732 | 39416 / 36732 | 43008 / 38912 | 11800 / 4760 | 35724 / 35724 | 33864 / 33864 | 13940 |
| 19:36:02 | S3 EN>ES | 44780 / 44772 | 36732 / 36732 | 38912 / 38912 | 4760 / 4440 | 35724 / 35724 | 33864 / 33864 | 13940 |
| 19:36:11 | S3 ES>DE | 44764 / 44632 | 36732 / 36532 | 38912 / 38912 | 4440 / 4440 | 35724 / 35724 | 33864 / 33864 | 13940 |
| 19:36:48 | S6 Programm | 44604 / 44496 | 36532 / 36228 | 38912 / 36864 | 4440 / 4440 | 34432 / 34432 | 31144 / 31144 | 16400 |
| 19:37:31 | S10 Name | 44500 / 44496 | 31240 / 31240 | 36864 / 36864 | 4440 / 4440 | 20584 / 20584 | 19632 / 19632 | 25912 |
| 19:38:56 | S10 Wiederherst. | 44700 / 44684 | 29092 / 29092 | 43008 / 38912 | 11464 / 4856 | 22448 / 22448 | 21616 / 21616 | 24220 |

Weitere Kennzahlen (`internal_8bit_*`, `dma_*`) stehen im Rohlog; auf diesem Board
sind sie mit den Gesamtwerten identisch. `LVGL max genutzt` sinkt nach dem Reset
(Zaehler zurueckgesetzt).

Extremwerte der gesamten Aufnahme: kleinstes `minimum_free_heap` 29092 B (nach
Neustart auf der Textseite, nicht im Commit-Fenster erreicht), kleinste
Stack-Reserve 4440 B, kleinster groesster Block 36864 B, LVGL-Pool-Auslastung
bis 55 %.

Sichtbare Muster (Messdaten, keine Bewertung gegen Grenzwerte):
- Der erste Commit nach Boot/Seitenaufbau verbraucht Stack (11800 -> 4760 bzw.
  11464 -> 4856 B Reserve); danach bleibt die Reserve bei 4440-4856 B.
- Das `minimum_free_heap` faellt beim Oeffnen der Textseite (36228 -> 31240 B),
  nicht im Commit-Press selbst.
- Der groesste Block schrumpft beim ersten Commit (43008 -> 38912 B) und nach dem
  Programm-Commit (38912 -> 36864 B).

Zwischen den Commits wurden weitere Presses derselben Seiten gemessen (Tastatur,
Navigation; 24 Probe-Paare ohne Dispatch, ausser den oben aufgefuehrten). Sie
zeigen keine Abweichung ausser den genannten Mustern.

## Einschraenkungen und offene Punkte

- Das Messfenster umfasst Dispatch und Persistenz im selben Touch-Tick; das
  anschliessende Neuzeichnen (Render) liegt ausserhalb.
- Der Probe-Build schreibt pro Press zusaetzliche UART-Zeilen. Der Owner schaetzt
  das Geraet im Probe-Build subjektiv als **weniger schnell reagierend als zuvor**
  („damit kann man leben“). Das ist nicht gemessen; ob die Probe-Logs, UART-Last oder
  etwas anderes die Ursache ist, ist nicht eingegrenzt. Mit dem zurueckgeflashten Exakt-Image meldet der Owner die Reaktion anschliessend
  als „sehr smooth“ (subjektiv, nicht gemessen); das spricht fuer einen Einfluss
  des Probe-Builds, ist aber kein Nachweis.
- Programm-Wiederherstellung: Waehrend der Aufnahme wurde das Programm nicht
  zurueckgesetzt (kein zweiter Programm-Commit im Log; die Owner-Meldung
  „alles zurueckgesetzt“ war insoweit unzutreffend). Der Owner hat das Programm
  anschliessend nach dem Rueckflash des Exakt-Images zurueckgesetzt (Owner-Aussage,
  nicht aufgezeichnet).
- Aufnahmedauer nach den Bedienhandlungen: ca. 20 s ohne Auffaelligkeit
  (Heartbeats), kein Langzeitnachweis.

## Provenienz

```text
feedd1bf940103cecb2a61f754e0a865111c4ae578ae7d602ecfe7d8885a00c5  uart_d10.raw.txt (Probe-Build, 4774 Zeilen, Host-Zeitstempel)
2572d139575a4a56fc156a79e9a7d2b0ac64086a9f5a0b2eadb7215bf8101aee  flash_probe.log
86c80ea144ff05fec6542c708d6d6819cbc352e391715ccae885e57608a5ba50  flash_exact.log (Rueckflash)
b6839d3fd4d19e2afc19554ff044351a28a174661676713874e362f529b40322  uart_exact_boot.raw.txt (Boot des wiederhergestellten Exakt-Images)
```

Rohlogs liegen lokal (nicht im Repository). Der Bericht enthaelt keine WLAN- oder
Auth-Daten.
