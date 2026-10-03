# R1-RAM-Baseline S2 – Precheck Testzustand-Wiederherstellung

Auftrag: `PR174_S2_TestState_Restore_Precheck`. Nur Suche und Dokumentation:
am Gerät nichts geschrieben oder gelöscht, kein Produktcode geändert, keine
S3–S7-Arbeit, kein Issue angelegt.

```text
PRECHECK=DONE
VERIFIED_PRODUCTION_BACKUP_FILE_AVAILABLE_LOCALLY=NO
DOCUMENTED_BACKUP_HASH_CANDIDATES=1_WITHOUT_FILE_1_WITHOUT_HASH
CURRENT_FAULTY_STATE_PRESERVED=YES
DEVICE_CHANGED=NO
```

## 1. Aktueller fehlerhafter Stand (aufbewahrt)

Read-only gelesen mit `esptool read-flash`, dazu der Vergleich vor/nach der
Diagnose (bitgleich). Kopie unter `~/esp32-fermentationsschrank-backups/
20261002_state_store_ServiceRequired_UnsupportedNewerConfigurationSchema/`
(Dateien schreibgeschützt, bewusst **nicht** im Repository, da ein NVS-Abbild
Konfigurationsdaten wie WLAN-Zugangsdaten enthalten kann):

| Datei | Bereich | SHA-256 |
|---|---|---|
| `state_store_0x300000_1MiB.bin` | `state_store`, 0x300000, 1 MiB | `f6c932be690bb6ef1d1e87e5659bc784d9cc1de5d10fae9fbefe039830cfbbad` |
| `nvs_phy_0x9000_28KiB.bin` | Default-NVS/PHY, 0x9000, 28 KiB | `06a875d0b89d33f3266f7ff9bcec9870b7b1e8749487d0c322778db5dc2af1f5` |

Gerät: ESP32-D0WD-V3, MAC `20:50:0d:1b:2f:34`, Aufnahme 2026-10-02 vor dem
Diagnose-Harness. Zugehörige Evidenz:
`R1_RAM_BASELINE_S2_BLOCKER_DIAGNOSIS.md` (Status
`UnsupportedNewerConfigurationSchema`, `ServiceRequired`). Die Firmware war
`esp32_release`; der zuletzt laufende Stand meldet `0de006a…` (Code
`5bfc9bc…`). Der Stand wurde nicht vor einem Harness-/Power-Cut-Test,
sondern vor dem read-only STATUS-Harness aufgenommen.

## 2. Suche nach Produktions-Backups

Durchsucht: gesamtes Dateisystem (`find /`, Namensmuster
`*state_store*`, `production_state_store*`, `*backup*`, `*pre_harness*`,
Verzeichnisse `production_release` und `*issue90*backup*`) sowie alle
Dateien mit exakt 1 MiB unter `/var/lib/docker/data`, `/home/manuel`, `/tmp`,
`/mnt`, `/media`; ferner Repository (Dokumente, Roadmap, Skripte, `git log
-S`), `Agent-Auftraege/`.

**Ergebnis: Es existiert lokal keine Backup-Datei eines Produktions-
`state_store` außer den beiden Dateien des aktuellen fehlerhaften Standes.**
Insbesondere fehlen `production_state_store.bin`,
`production_partition_table.bin` und `production_release/` aus dem
Issue-90-Runner (`scripts/issue_90_slice7_product_runner.py`,
`--backup-dir`).

## 3. Dokumentierte Kandidaten (nur Provenienz, keine Datei)

| Kandidat | Pfad | SHA-256 | Zeit / Evidenz | Firmware-/Repo-Stand | vor Harness/Power-Cut? | `application: ready` belegt? |
|---|---|---|---|---|---|---|
| A: Backup aus Issue-#159-Vorbereitung (`--phase prepare`) | **nicht vorhanden**; laut Evidenz „lokal erhalten, nicht Teil dieses Commits“ | `3d678c449760e7772901870930efe492c81d99a48abfc8246d6cd23215b95b9a` (State-Store); Partitionstabelle `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d` | 2026-09-15, `docs/audits/ISSUE_159_ESP_IDF_6_1_UPGRADE_EVIDENCE.md` (Abschnitt „Owner-Entscheidung: Issue-90-Power-Cut-Kampagne nicht wiederholt“) | zuletzt real geflasht `esp32_release` auf `fc306c4428a2bd770866e5f23ce0881f38bc1baf` (v6.1) | ja: `PRE_HARNESS_BACKUP=PASS`; Harness danach **nicht** geflasht, kein Power-Cut, kein Restore | für genau diesen Stand nicht im Repository belegt; `HARDWARE_SMOKE_RELEASE` auf dem v6.1-HEAD ist laut Evidenz PASS, ohne Bezug auf dieses Backup |
| B: Backup der Issue-#90-/PR-#128-Kampagne (v6.0.2) | **nicht vorhanden**, kein Hash im Repository | **unbekannt** | Kampagne bis 2026-08-30, Roadmap-Commits `593fb0c`/`12e7809` | PR #128, Merge `c1f5fbb5f19ab8e7d2c25708fe79777d523217d4` | ja (Pre-Harness-Backup war verpflichtend) | Roadmap: `PRODUCTION_RESTORE=PASS`, `POST_RESTORE_PRODUCT_BOOT=PASS`, `RESTORE_STATE=COMPLETE`, `production_restore_required=false` |

Der historische Repo-Nachweis ist damit bestätigt: Issue #90 verwendete
denselben Flashbereich, Pre-Harness-Backup und vollständiger Restore waren
verpflichtend, und `PRODUCTION_RESTORE=PASS` ist für B dokumentiert.
Weder A noch B ist als Datei auf dieser Maschine verifizierbar; die Datei zu
A müsste anhand ihres SHA-256 identifiziert werden. B hat keinen
dokumentierten Hash.

## 4. Schlussfolgerung für den Owner-Entscheid

- Ein **eindeutiger, verifizierter Backupstand liegt lokal nicht vor.**
  Ein Restore ist aus dem Repository und diesem Rechner heraus nicht
  möglich; ein Kandidat (A) ließe sich nur wiederherstellen, wenn die Datei
  mit SHA-256 `3d678c44…` an anderer Stelle (z. B. auf einem anderen Rechner)
  gefunden wird.
- Auch ein solcher Restore würde den Zustand vom 2026-09-15 herstellen, der
  den aktuellen Stand überschreibt. Der aktuelle fehlerhafte Stand ist
  gesichert und kann danach zurückgespielt werden.
- Offen bleibt, ob der inkompatible Schema-Datensatz von einem aktuellen
  Produktpfad erzeugt wurde. Das ist nicht untersucht; ohne diesen Beleg wird
  kein Issue angelegt.
- Entscheid des Owners: (1) Restore eines verifizierten Backupstands, falls
  die Datei zu A gefunden wird, oder (2) kontrollierte Neuinitialisierung des
  Testgeräts mit gesicherter/reprovisionierter Touchkalibrierung. Für (2)
  ist die aktuelle Kalibrierung `active_status=Available` Teil dieses
  `state_store`; sie ist in der Sicherung oben enthalten.

Es wurde nichts am Gerät geändert; dieser Precheck stoppt hier.
