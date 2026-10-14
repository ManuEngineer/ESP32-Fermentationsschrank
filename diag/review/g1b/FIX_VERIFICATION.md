# G1b S0 – Fix Verification (gegen Korrekturauftrag zu `3f6b1d0`)

Nur Host-/Testpaket; keine Firmware-, Image- oder Hardwareänderung. Originale G1-IMAGE01-Evidence unverändert. Die Tests laufen offline ohne Gerät, Netz und Privilegien.

| Auftrag | Änderung | Nachweis (`diag/test_*.py`) |
|---|---|---|
| BLOCKER 1: PCAP-Lebenszyklus | `g1_pcap.py capture` endet bei `--stop-file` (G1: `ABORT`, `STOP`), `PCAP_STOP`, hartem `--max-seconds`, Interrupt; tcpdump wird immer beendet (SIGINT → terminate → kill); Exitcodes 0/2/3/4/5/6, `pcap_result.json`; nur Exitcode 0 = PASS; ungültige Dateien gelöscht, gültige bleiben privat; Paketzahl muss der tcpdump-Meldung entsprechen | `CaptureLifecycleTests` (10 Tests): Abort-Datei beendet und PASS; Start-/Berechtigungsfehler; Prozessende im Lauf; hartes Limit; Interrupt; SIGINT ignoriert → terminate+kill; Exitcode ≠ 0; fehlend/leer/ohne Meldung/falsche Zahl ⇒ nie PASS; nicht payloadfrei ⇒ gelöscht |
| BLOCKER 2: strenger Nachweis | `packets()`/`parse()`/`verify()`: keine Restbytes, keine abgeschnittenen Records, Länge exakt `min(orig, 54)`, IHL 5, keine Fragmente (MF/Offset), TCP-Header ≥ 20 B, IP-Gesamtlänge konsistent, gespeicherte Bytes enden im TCP-Header, leer ⇒ Fehler; Speicherort in Git-Worktree verweigert | `PayloadFreeTests` (7 Tests): Positiv; Länge > 54; VLAN/UDP/IP-Optionen/Linktyp; Fragmente (3 Varianten); ungültige/inkonsistente Längen; Restbytes, Teil-Recordheader, abgeschnitten, zu kurz, leer, 0 Byte; falsche gespeicherte Länge |
| BLOCKER 3: Preflight-Vertrag | Testblatt Abschnitt 3: nummerierte Geräteaktionen P1 (Reset in Bootloader), P2 (nur lesen), P3 (Reset in Anwendung), R1–R3 (Port-Öffnungsreset = Testboot); Trennung vom Testboot; ohne genehmigte P1–P3 oder bei Abweichung **HOLD** | Dokumentprüfung |
| FOLLOW-UP 1: payloadfreier Mitschnitt | siehe BLOCKER 2; Methode und Datalink-/Headerannahmen im Testblatt Abschnitt 2 | s. o. |
| FOLLOW-UP 2: A/B-Referenzen | Testblatt Abschnitt 5: zulässige A-Referenzen (vollständig / früh-FAIL nur im Fenster), Stufe-D-Läufe keine Tagesreferenz für E, A'-Läufe im E-Gate, harte B-Kriterien nie durch das Fenster relativiert, 1,25× nur Bewertungsgrösse, Ownerbestätigung `PENDING` | Dokumentprüfung |
| FOLLOW-UP 3: Instrumentierungs-Overhead | Testblatt Abschnitt 5a: ≈ 1,4 KB / +1 452 B als Vergleichsgrenze, keine monokausale Zuordnung, C0-R3 nur Vergleichsgrenze | Dokumentprüfung |
| Evidence-Delta `abort_induced` | wahr nur nach erfolgreichem `shutdown` eines aktiven Sockets durch den Guard; im Vertrag als abort-assoziiert, nicht beweisend kausal | `test_abort_induced_needs_a_socket_the_guard_actually_shut_down` (inaktiv, `shutdown` schlägt fehl, Positivfall) |
| Evidence-Delta Review-Index/Hashes | Index auf gepushten Stand; `HOST_TOOLS_SHA256SUMS`; Skripte schreiben Tool-Hashes in `capture.json`/`pcap_result.json`; H0-Pflicht im Testblatt | `test_partial_anomaly_keeps_reading…` (Tool-Hashes in `capture.json`), `…abort_file…` (Tool-Hash in `pcap_result.json`) |
| Evidence-Delta Firmware | keine Änderung; `FLASH_REFS=0` am gepinnten ELF (`HOOK_STATIC_CHECK_G1_ELF.txt`); bei Identitäts-/Tooldrift HOLD statt Nachbesserungsflash | – |

**Regression:** `cd diag && python3 -m unittest discover -p "test_*.py"` → 54 Tests OK (zuvor 40; alle 29 ursprünglichen Tests unverändert grün). `python3 scripts/check_secrets.py` PASS. `sha256sum -c diag/review/g1b/HOST_TOOLS_SHA256SUMS` OK.

**Grenzen:** Der Mitschnitt wurde nie mit echtem tcpdump ausgeführt (keine Berechtigung, kein Geräteaufbau); das Verhalten gegenüber echtem tcpdump/pcap-Ausgaben ist durch Offline-Tests mit synthetischen Dateien belegt und vor dem Gate durch den `lo`-Trockentest (Testblatt Abschnitt 3, Schritt 2) zu bestätigen. Keine Hardwarefreigabe, kein G1b-Lauf, kein O1, kein G2.


---

# Zweite Fix Verification (gegen Delta zu `a818e1c7ea8bcfb8124ebccded547a1a020fd2be`)

Nur Host-Werkzeug und Testblatt; keine Firmware-/Produktänderung. Nicht erneut umgebaut: Backtrace-Hook, strenge PCAP-Validierung, P1–P3, `abort_induced`, A/B-Referenzen, Instrumentierungsgrenzen.

| Blocker | Änderung | Nachweis (`diag/test_g1_pcap.py`) |
|---|---|---|
| 1: Start und 1 800-s-Limit | Testblatt Abschnitt 3 Schritt 4: feste Reihenfolge `g1_capture` (Bereit ≤ 120 s) → `g1_pcap capture` → `PCAP_RUNNING` (≤ 30 s) → **erst dann** `START_LOAD` → Endsignal (`ABORT` bei Anomalie, `STOP` ≤ 60 s nach Recovery-Ende). `g1_pcap.py`: `RUN_BUDGET_SECONDS` (60+180+5+36+60+1200+300+60+60) ⇒ `DEFAULT_MAX_SECONDS` **1961 s ab tatsächlichem tcpdump-Start**; `PCAP_RUNNING` erst nach bestandener 1-s-Lebendprüfung (veralteter Marker wird entfernt). Verworfen: Start mit dem Capture (≈ 431 s Vorlauf). | `test_the_run_budget_covers_a_full_run_and_still_holds_an_orphaned_capture` (virtuelle Uhr): regulärer Lauf mit spätestem Stopp bei 60+180+1575+60 s ⇒ Exitcode 0 `stop-signal`; verwaister Mitschnitt ⇒ Exitcode 6 `hard-time-limit`; der alte 1 800-s-Wert hätte einen 1 931-s-Lauf abgebrochen (Exitcode 6); Budget gegen `c0_load`-Konstanten (Login-Timeout, 60/1200/300 s) abgeglichen. `test_running_marker_exists_only_while_tcpdump_really_runs` (abwesend bei der Startprüfung, danach gesetzt; nie bei sofort beendetem tcpdump) |
| 2: UTC-Anker der Bins | `analyze`: `time_base`, `first_packet_epoch`/`_utc`, `last_packet_epoch`/`_utc`, `bin_definition`, je Bin `start_epoch`/`end_epoch`; `event_window(result, event_epoch)` / `analyze --event-epoch` trennt `before`/`straddling`/`after`, `covered` für Ereignisse ausserhalb der Spanne. Keine Adressen, Ports oder Payload im Ergebnis; Validierung und alle bisherigen Zähler unverändert. | `TimeAnchorTests`: synthetische Pakete ab bekannter Epoch, UTC-Strings und Bin-Grenzen exakt; bekannter UART-Fehlerzeitpunkt mitten im Bin ⇒ eindeutige Vor-/Nachgruppen und `straddling`; Zeitpunkt auf der Bin-Grenze ⇒ kein `straddling`; Zeitpunkt ausserhalb ⇒ `covered=false`; Ergebnis ohne Host-/Geräteadresse und Port |

**Regression:** `cd diag && python3 -m unittest discover -p "test_*.py"` → 58 Tests OK (zuvor 54). `python3 scripts/check_secrets.py` PASS. `sha256sum -c diag/review/g1b/HOST_TOOLS_SHA256SUMS` OK (Manifest aktualisiert).

**Grenzen:** wie zuvor (kein echter tcpdump-Lauf; `lo`-Trockentest vor dem Gate). `G1B_AUTHORIZATION=PENDING`; keine Portöffnung, kein Reset, kein Flash, keine Last, keine Einrichtung erhöhter Hostrechte.
