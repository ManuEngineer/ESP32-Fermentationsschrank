# PR #170 S2 – Heap-Evidence des `AuthOperationGate` (ESP32)

```text
ISSUE=27
PR=170
SLICE=S2_AUTH_LIFETIME_GATE
CODE_COMMIT=ff8ea9bba73c2e56843ac0555bd2172d86b2fa78
BOARD=ESP32-D0WD-V3 / WROOM-32E family, no PSRAM
PROFILE=esp32_release
CONFIG_LV_MEM_SIZE=49152
CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576
S2_RAM_EVIDENCE=PASS
AUTH_OPERATION_GATE_HEAP_FREE=NO
AUTH_OPERATION_GATE_SYNC_HEAP=MEASURED
AUTH_OPERATION_GATE_UNBOUNDED_GROWTH=NO
```

## Zweck und Methode

Der freigegebene Plan verlangt eine ehrliche, fokussierte Messung des
Heap-Footprints der `std::mutex`/`std::condition_variable`-Synchronisation
(ESP-IDF 6.1 allokiert die pthread-Objekte lazy intern). Gemessen wurde mit
einer **temporären Instrumentierung in `main/app_main.cpp`, die nicht
committet ist** (`PR170_S2_AUTH_GATE_RAM_PROBE.patch`, letzter Stand; leere Kontextzeilen sind
von Trailing-Whitespace befreit, Anwendung mit `git apply --ignore-whitespace`):

- Die Probe läuft im Hauptloop bei `periodic_30s`, also im stabilen
  `HOME_WIFI`-Betrieb nach `HomeConnected`.
- Verglichen wird **ein frisches `AuthOperationGate`**, das auf dem Stack
  konstruiert, benutzt (`tryBegin`/Token/`closeAndDrain`/`reopen`, in einer
  zweiten Variante zusätzlich mit einem wartenden `std::thread`) und wieder
  zerstört wird, mit unmittelbar davor und danach gelesenem
  `esp_get_free_heap_size()` (`free0` vor Konstruktion, `free1` solange das
  benutzte Gate lebt, `free2` nach Zerstörung). 2 Varianten × 10
  Wiederholungen.
- Wachstumsprüfung: ein langlebiges Gate führt 6 Fenster × 5000 Zyklen
  (`tryBegin`, Token-Ende, `closeAndDrain`, `reopen`) aus; jedes Fenster hat
  ein gleich langes Kontrollfenster ohne Gate-Operation.
- Der erste Probelauf (Messpunkte beim Boot, `ConnectingHome`) wurde
  **verworfen**: das gleichzeitig verbindende WLAN änderte den Heap um mehrere
  KB, sodass sich das Gate-Delta nicht isolieren ließ. Die Paarmessung ist die
  belastbare Methode. Gemessene Roh-/Logzeilen:
  `PR170_S2_AUTH_GATE_RAM_UART_EXCERPT.txt`.

Die Instrumentierung wurde nach der Messung verworfen (`git checkout`); auf dem
Gerät läuft wieder die committete S2-Firmware (`esp32_release`, SHA256 der
`esp32_fermentationsschrank.bin`
`b2c864e2260c887a47b9eb64a7e70117e953cf8f5d58491441b00fd8d5f7e9c3`; der
Image-Versionsstring lautet `234ac9e-dirty`, weil der Build vor dem S2-Commit
`ff8ea9b` aus identischem Quelltext erfolgte).

## Ergebnisse

```text
sizeof(AuthOperationGate) ESP32 (Xtensa, libstdc++/pthread) = 16 B
sizeof(AuthOperationGate) native x86-64 Host                = 96 B
```

| Messung | Ergebnis |
|---|---|
| Heap, den ein benutztes Gate hält (`free0 - free1`) | **212 B**, in 20 von 20 Paaren identisch (beide Varianten) |
| Rückgabe nach Zerstörung (`free2 - free1`) | **+212 B**, in 20 von 20 Paaren identisch (kein Leck pro Konstruktion/Zerstörung) |
| Variante mit wartendem Thread vs. ohne | identisch (212 B); Thread-Stack/TCB sind transient und werden zurückgegeben |
| Wachstum: 6 Fenster × 5000 Zyklen auf einem Gate | `gate_delta=0` in allen 6 Fenstern (30 000 Zyklen); `control_delta=0` in allen Kontrollfenstern |
| `minimum_free_heap_bytes` / `largest_free_block_8bit_bytes` | während der Probe unverändert (25 964 B / 27 648 B) |
| Allocation-Failures, Aborts, Resets | keine (`heap_alloc_failed`, `abort`, Panic nicht beobachtet; Heartbeats liefen weiter) |
| `CONFIG_LV_MEM_SIZE`, Main-Task-Stack | unverändert (49152, 24576); `stack_hwm_bytes=12800` |

Das produktive Gate ist ein Member der einen `FermentationApplication`: es
kostet dauerhaft **die 16 B Objektgröße (im Applikationsobjekt) plus ca. 212 B
lazy pthread-Heap** (Mutex- und Condition-Variable-Objekt), einmalig und ohne
Wachstum. Im Vergleich zu den Referenzpunkten der PR-#174-Basis
(`after_ui_init`: ca. 33,5 KB freier Heap, größter Block 27 648 B,
`docs/audits/R1_RAM_LVGL48_HW_EVIDENCE.md`) ist das etwa 0,6 % des freien Heaps
und keine Änderung des größten zusammenhängenden Blocks. Es wird keine neue
harte RAM-Grenze behauptet.

## Einordnung und Grenzen

- Gemessen ist der Footprint der Synchronisationsprimitive, nicht das gesamte
  #170-Delta gegenüber `main` (der Branch enthält weitere Web/Auth-Komponenten).
- Der Wert gilt für den Zustand `Unprovisioned` ohne Sessions; er ist unabhängig
  von Sessions, da das Gate keine Nutzdatenpuffer hält.
- Beim Rückflash der Release-Firmware lieferte der LVGL-Pool-Wert bei
  `after_ui_init` einmal `pool=unavailable` (Pfad `lvgl_port_lock(100)` im
  Boot-Fenster); die beiden Probeläufe zeigten `pool_total_bytes=46724`. Das
  hängt nicht am Gate und wurde hier nicht weiter untersucht.

## Nachtrag: S2-Fix Session-Issuance

Der Folge-Fix (Session-Erzeugung gegen Trust-Boundaries serialisieren) fügt
`FermentationApplication` genau einen 64-Bit-Zähler (`webTrustGeneration_`) und
Methoden hinzu, aber **keine** Synchronisations-, Heap- oder Langzeitobjekte.
Die Messung oben bleibt daher unverändert gültig (`S2_RAM_EVIDENCE=PASS_UNCHANGED`);
eine erneute Hardwaremessung war nicht erforderlich.
