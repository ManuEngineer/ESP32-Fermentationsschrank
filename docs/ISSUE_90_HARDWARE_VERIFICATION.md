# Issue #90 Hardware-Verifikation

## Status

- Gesamtstatus: `BLOCKED/NOT_RUN`.
- Grund: In dieser Draft-Phase war kein verifizierter ESP32, kein serieller
  Port und kein externer Power-Cut-Controller fuer den vollstaendigen
  Standard-Flash-/Partitionslauf verfuegbar.
- Quellstand der geplanten Ausfuehrung: `0573096dcc77bd98013b7a3f5be6dc123d90436b`.
- Herstellerbasis: ESP-IDF `v6.0.2 @
  7101770dc6db2667b3c477cc31365dd1acd6db4e`.

Der Bring-up-Harness wurde in einem getrennten Buildpfad mit
`APP_ISSUE_90_NVS_HARDWARE_TEST=1` kompiliert. Das ist ein Buildnachweis, kein
Hardware-PASS. Das Releaseprofil enthaelt die Testlogik nicht.

## Ausfuehrbarer On-Target-Pfad

Nach Bereitstellung des freigegebenen 4-MB-Ziels wird die Firmware aus dem
Bring-up-Profil geflasht. Der Harness besitzt den Partition-Lifecycle solange
noch kein produktiver `IStateStore`-Verbraucher verdrahtet ist: normaler
`nvs_flash_init_partition("state_store")`, Konstruktion des echten
`NvsStateStore`, danach explizite Deinitialisierung beim Harness-Ende. Der
Adapter selbst besitzt weiterhin keine Init-/Deinit-Verantwortung.

Der Runner wird mit dem realen UART und dem externen Hook gestartet:

```bash
python3 scripts/issue_90_nvs_hardware_verification.py \
  --port <serial-port> \
  --power-cut-hook 'python3 scripts/issue_90_power_cut_hook.py' \
  --repetitions 10 \
  --clean-reboots 3
```

Der deterministische UART-Vertrag lautet:

1. Firmware meldet `READY` mit Partition-/NVS-Status und Baseline-
   Ressourcenwerten.
2. Runner sendet `ARM token=<opaque-token>` an den externen Hook und erwartet
   exakt `ARMED`; anschliessend sendet er `CUT_ARM token=<token>` an die
   Firmware und erwartet `CUT_ARMED`.
3. Runner sendet `ROTATE key=<key> max_writes=<n>`, wartet auf den passenden
   `ROTATE_BEGIN`-Marker und loest erst danach per Hook `TRIP` aus. Der Hook
   muss exakt `TRIPPED` bestaetigen.
4. Der Runner weist UART-Verlust vor `ROTATE_RESULT` nach, setzt danach mit
   `RESTORE` fort und erwartet exakt `RESTORED`, einen neuen `READY`-Marker,
   Partition-/NVS-Status sowie vollstaendigen Readback.
5. Fuer jeden Abschluss werden pro Schluessel alter oder neuer exakter Wert,
   Laenge und SHA-256 protokolliert. Ein Mischwert, fehlender Wert oder
   abweichender Hash ist `FAIL`; ein nicht eindeutig wiederherstellbarer
   Zustand bleibt fail-closed und ist kein PASS.

Die Matrix umfasst mindestens zehn Wiederholungen je kalibriertem Fenster
`blob_data`, `blob_index`, `old_removal` und `gc_erase` sowie drei saubere
Neustartkontrollen ohne Unterbruch. Sie umfasst ausserdem die deterministische
Vorbefuellung aller 22 Inventarschluessel und Rotationen bis zum GC-/Erase-
Fall. Die Ergebnisse werden mit Lauf-ID, Firmware-/Source-SHA, Profil,
Partitionstabelle, UART-Log, Hook-Log, Readback-Hashes und Ressourcenlogs
archiviert.

## GC-/Erase-Orakel

`GC_ERASE_DETECTED` darf erst ausgegeben werden, wenn die Firmware gemeinsam
folgende Evidenz erfasst und protokolliert:

- eine vorher gueltige, nichtleere NVS-Seite mit Status, Sequenz, belegten
  Entries und Seitenhash wird spaeter tatsaechlich geloescht;
- eine gueltige neuere Seite weist konsistente Status-/Sequenz-/Entry-Struktur
  und die kopierten lebenden Inventarrecords auf;
- NVS-Statistik, seitenweise Vorher-/Nachher-Hashes und vollstaendiger
  Alt-oder-Neu-Readback stimmen damit ueberein.

Eine reine Statistik- oder Hashaenderung ist kein GC-/Erase-Nachweis. Die
Evidenz wird seitenweise/streamend mit einem festen 16-KiB-Scratch-Budget
gesammelt (zwei Arrays von `PageEvidence` plus 256-B-Readbuffer); es werden
keine zwei vollstaendigen 69-Seiten-Abbilder im No-PSRAM-Heap gehalten. Die
Firmware meldet zusaetzlich Heap frei, groessten freien Block, Stack-HWM und
die Deltas zwischen Baseline und Harnessbetrieb. Die reale Messung ist bis
zum On-Target-Lauf `BLOCKED/NOT_RUN`.

## Noch offene Nachweise

| Nachweis | Status | Pass-Orakel |
|---|---|---|
| reale `state_store`-Initialisierung und Partitionstatus | `BLOCKED/NOT_RUN` | exakt erwartetes Label, Groesse und NVS-Statistik |
| 22-Schluessel-Vorbefuellung und vollstaendiger Readback | `BLOCKED/NOT_RUN` | alle Werte exakt, Hashes reproduzierbar |
| saubere Neustarts | `BLOCKED/NOT_RUN` | alle alten Werte nach Neustart unveraendert |
| Blob-Daten-/Index-/Altwert-Power-Cuts | `BLOCKED/NOT_RUN` | UART-Verlust vor Ergebnis und exakt alter oder neuer Wert |
| vorbefuellter GC-/Page-Erase-Power-Cut | `BLOCKED/NOT_RUN` | vollstaendiges GC-/Erase-Orakel plus alter oder neuer Wert |
| Heap-/Largest-Block-/Stackwirkung des Harnesses | `BLOCKED/NOT_RUN` | begrenzte Deltas im Ressourcenlog, keine Grenzwertverletzung |
| Flash-/App-/Partitionsreserve auf dem realen Ziel | `BLOCKED/NOT_RUN` | ausgelesene 4-MB-Partition und reale Flashreserve |

Der ESP-IDF-BDL-Hostlauf bleibt davon getrennt: Er prueft Storage-/Recovery-
Semantik der gepinnten NVS-Implementation; nur der reale ESP32 belegt den
Standard-Flash-/Partitionspfad.
