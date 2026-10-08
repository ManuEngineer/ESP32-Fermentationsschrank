# Issue #30 – statische Ressourcendifferenz des Softwarepfads

Statische Differenz der fertigen Builds gegen die Base `main`
`7b16dbeb95ab09fdbe13d6c524c7a09ec721fb7d` (`esp32_release`, ESP-IDF v6.1, gleiche
Messmethode wie `ISSUE30_S0_STAGE1_EVIDENCE.md`). Gemessen mit
`xtensa-esp32-elf-size -A` auf den ELF-Dateien. **Heap, Task-Stack, RMT-Laufzeitpuffer
und Zyklusdauer sind nicht gemessen** (kein Flashen, `HW-30-03` `NOT_RUN`); der
Sampling-Task ist budget-gesperrt, im Softwarepfad wird weder Task noch RMT-Bus erzeugt.

```text
SOFTWARE_BUILD_HEAD=C3 (Commit 8bb4482) bzw. C4-Dokumentationscommit darauf
PROFILES=esp32_release PASS, esp32_bringup PASS, esp32_bringup + APP_ISSUE_30_SENSOR_COMMISSIONING PASS
HEAP_STACK_RMT_MEASURED=NO
ACTUATOR_RELEASE=NO
```

## esp32_release gegen Base

| Groesse (Byte) | Base `7b16dbe` | Release mit Softwarepfad | Delta |
|---|---:|---:|---:|
| `esp32_fermentationsschrank.bin` | 1'687'280 | 1'721'840 | +34'560 |
| `.flash.text` | 1'349'402 | 1'375'234 | +25'832 |
| `.flash.rodata` | 219'000 | 225'356 | +6'356 |
| `.iram0.text` | 98'551 | 100'815 | +2'264 |
| `.dram0.data` | 18'867 | 18'975 | +108 |
| `.dram0.bss` | 73'680 | 74'472 | +792 |

Das Delta enthaelt den RMT-/1-Wire-Treiber (S0 Stufe 1: +23'792 B Image fuer die
Komponenten allein), Engine, Adapter, Sampler, Schema-3-Codec und Application-Eintrag.
Die statische RAM-Zunahme (`.dram0.bss` +792 B, `.dram0.data` +108 B) ist der einzige
Dauer-RAM-Anteil des Softwarepfads bei gesperrtem Task. Die App-Partition (3 MB)
hat ausreichend Reserve. Im Release-Image sind **keine** Harness-Symbole
(`issue_30_commissioning`: 0) enthalten.

## esp32_bringup

| Groesse (Byte) | Bring-up | Bring-up + Commissioning-Option | Delta durch den Harness |
|---|---:|---:|---:|
| `.bin` | 1'733'648 | 1'743'376 | +9'728 |
| `.flash.text` | 1'385'054 | 1'388'638 | +3'584 |
| `.flash.rodata` | 227'340 | 230'532 | +3'192 |
| `.iram0.text` | 100'815 | 103'787 | +2'972 |
| `.dram0.bss` | 74'480 | 74'480 | 0 |

Ein Bring-up-Base-Build ohne den Softwarepfad wurde nicht gemessen; die
Bring-up-Werte stehen als Absolutwerte. Die Commissioning-Option ersetzt dort die
Issue-29-Diagnose und installiert den UART-Treiber (daher der IRAM-Anteil).

## Nicht gemessen (Hardware-Folgeissue)

Task-Stack-HWM, Heap und groesster freier Block mit laufendem Task, RMT-Kanal-/
Pufferbedarf, blockierte CPU-Zeit der Hauptschleife, Zyklusdauer beider Busse,
Reset-/Watchdog-Verhalten (`HW-30-02`, `HW-30-03`, `HW-30-06`).
