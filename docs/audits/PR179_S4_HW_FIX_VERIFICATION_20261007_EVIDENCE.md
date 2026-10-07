# PR #179 – S4 Hardware-Fix-Verification nach Issue #181 (2026-10-07)

```text
TESTED_HEAD=2ce9fedaf301d09f42f848b353213fd2edafd311
PROFILE=esp32_release
BASE=PR182_MERGED_2d9fc52172d56edb5807d8550e424af4605d1ee2
S4_HARDWARE_FIX_VERIFICATION=PASS
S4_HEADER_LOCAL_TIME=PASS
S4_CLOCK_SCREEN_TRUSTED=PASS
S4_FAIL_CLOSED_BEFORE_NTP=PASS_OWNER_OBSERVED
S4_ZONE_EUROPE_ZURICH=PASS
COLD_BOOT=OWNER_POWER_CUT
PANIC=0
WATCHDOG=0
BROWNOUT=0_RUNTIME_BOD_AT_OWNER_POWER_CUT_ONLY
HEAP_ALLOC_FAILED=0
S1_PROGRAM_LIST_IDENTITY=NOT_RUN
S3_D10_COMMIT_SCOPED_RESOURCE_LOG=NOT_RUN_FOLLOW_UP
ACTUATOR_RELEASE=NO
```

## Firmware

- `esp32_release`, gebaut mit `scripts/build_esp_idf_profiles.py release` aus `2ce9fed`
  (nach Merge von `main` inkl. PR #182), Flash ohne Erase, keine RTC.
- Firmware meldet `source git sha: 2ce9fedaf301d09f42f848b353213fd2edafd311`.
- App-Bin SHA256 `288e0961d32915ea7c02077306b57c65a36b6c1ed6ff90fa5fedd8267b62b3a7`.
- Vorher auf demselben Stand: native Suite 1523/1523 PASS, `esp32_bringup` und
  `esp32_release` PASS (Main-Sync-Verifikation).

## Beobachtung

- Der Owner trennte die Versorgung vollstaendig und schaltete wieder ein (echter
  Cold Boot, kein EN-Reset).
- Header-Uhr zeigte 06:27 bei Referenz 06:27:44 Lokalzeit (CEST, UTC+2).
- Foto des Owners (06:28): Header `06:28`; `HeaderClock`-Screen mit Titel `Uhrzeit`,
  `Zeit vertrauenswuerdig`, `Europe/Zurich` und `06:28`, identisch zum Header.
  Abweichung zur Referenz unter einer Minute. Vor dem Fix #181 zeigte derselbe
  Pfad Uptime seit 1970 (`01:02`).
- UART: kein Panic, Watchdog oder `heap_alloc_failed` seit dem Boot. Die einzige
  BOD-Zeile steht vor dem Kaltstart-Header und stammt vom Power-Cut des Owners.
- Fail-closed (Owner-Beobachtung, nicht UART-belegt): direkt nach dem Einschalten
  war kurz keine Zeit sichtbar, danach erschien die korrekte Lokalzeit. Das
  Release-Log enthaelt keine SNTP-Zeilen.

## Weiterhin offen (nicht Teil dieser Verifikation)

- S1-Programmlisten-Identitaetsnachweis (Testdaten) und S3-D10-Ressourcenlog:
  separat durch den Owner zu entscheiden.
- Independent Review S4 und Fix Verification S3 B1 laut ROADMAP.

## Zusatzbeobachtung nach dem Hauptlauf (nicht Teil des PASS)

- Nach einem weiteren Power-Cut des Owners (EN-Draht wurde wieder angeschlossen)
  zeigte das Geraet laenger `Zeit nicht vertrauenswuerdig` und `--:--`
  (Foto des Owners, fail-closed, keine falsche Zeit). Die Dauer wurde nicht
  gemessen; im Release-Log sind keine SNTP-Zeilen sichtbar.
- Beim Versuch, passiv mitzulesen, hat das Oeffnen von `/dev/ttyUSB0` das Geraet
  per EN resettet (`rst:0x1`, Uptime ca. 16 s). Danach zeigte das Geraet wieder
  `Zeit vertrauenswuerdig`, `Europe/Zurich`, `06:32` (Foto des Owners). Ob die Zeit
  ohne diesen Reset von selbst gekommen waere, ist nicht belegt.
- Die Ursache der laengeren Wartezeit ist ungeklaert (Vermutung ohne Beleg:
  SNTP-Erstanfrage vor WLAN/DNS und Retry-Backoff). Kein Befund gegen den
  Trust-Gate-Pfad aus #181. Einordnung `FOLLOW-UP` (Zeit bis zum ersten Sync nach
  Cold Boot schwankt), Owner entscheidet ueber ein Issue; passt zum
  Netzwerk-Lebenszyklus (#89).
