# PR #210 / Issue #28 D1 – Smoke-Ergebnis auf dem realen ESP32 (HW-28-D1-01)

Ausfuehrung 2026-10-25 (Flash ca. 15:40, Browser-Smoke 15:41, Touch-Smoke 17:34–19:33, Restore danach) unter der Ownerfreigabe `G-F`, `G-S`, `G-U`, `G-R`, `G-B` fuer genau dieses Image. Protokoll: [PR210_D1_SMOKE_PROTOCOL_20261025.md](PR210_D1_SMOKE_PROTOCOL_20261025.md). **Builder-Berichte und Owner-Beobachtungen sind getrennt gekennzeichnet.** Kein Nachweis fuer Sensor-/Aktorhardware, D9, RAM-Reserve; `ACTUATOR_RELEASE=NO`.

## Image
Produktimage (uninstrumentiert) `esp32_release`, Produktcode-HEAD `ad8b075e89ec174ffabec73ed0c7dcca647928a7`, SHA-256 `7edf0c222d15d3cd09dff36888f553c50c10ae938e0e0b1b9b686e9aa1773441`, 1 779 136 B; Readback = Image. Vorher dritte Vorlesung = `P0`.

## Ergebnisse
| # | Pruefung | Ergebnis | Quelle / Beleg |
|---|---|---|---|
| B | Browser-Smoke (Login-Shell, Anmeldung, Navigation, Status/Temperatur, DE/EN/ES, Abmeldung) | **PASS** | Builder (Skript [PR210_D1_BROWSER_SMOKE.mjs.txt](PR210_D1_BROWSER_SMOKE.mjs.txt), alle Pruefungen PASS, keine Seitenfehler); Sensoren wahrheitsgemaess `Veraltet`, nie `0.0 °C` |
| 1 | Navigation `Home → Status → Diagnose` und zurueck | **PASS** | Owner („Alle 3 Punkte pass“), Fotos `de_diag_01of09` ff. |
| 2 | Feste Zeilen: `Resetursache: Einschalten` (Power-on), `Fehlercode: keiner`, `Zustand: Bereit`, kein `Boot` | **PASS** | Owner-Fotos 1/9 (Builder-Transkription), Owner-Verdikt PASS |
| 3 | Pager: Zaehler 1/9…9/9, Fenster um eine Zeile (bei 8/9 zwei, bei 9/9 eine Zeile), weiterer `Ab`-Druck bei 9/9 wirkungslos, `Auf` schrittweise zurueck bis 1/9 | **PASS** | Owner-Fotos 2/9–9/9, Owner-Verdikt PASS |
| 4 | Sensoren fehlen: alle drei Rollen `veraltet`, Alter `--`, `R--.- K--.- F--.-`; isolierte Wertezeilen mit Kurzrolle (`Luft`/`Prod`/`Kuehl`) und Qualitaet; kein `0`/`gueltig` | **PASS** | Owner-Fotos 3/9–9/9, Owner-Verdikt PASS |
| 5 | Darstellung DE/EN/ES | **PASS (nur Owner-Aussage fuer EN/ES)** | DE mit Fotos; EN und ES: Owner „ist auf de en und es alles richtig“, **keine Fotos**, daher keine unabhaengige visuelle Evidenz |
| 5a | `Spannungsstoerung`/`Subtension` (PowerGlitch) | **NOT_OBSERVABLE** | nur bei diesem Reset-Grund sichtbar; kein Reset/Fehler dafuer ausgeloest |
| 6 | Tippen auf Diagnose-Datenzeilen ohne Aktion; `Diagnose → Service` ansehen und zurueck (keine PIN, keine Aktion) | **PASS** | Owner („Alles pass wie beschrieben“); sichtbarer Service-Text nicht transkribiert, kein Foto |

**Kosmetische Builder-Beobachtung (DE-Fotos 7–9):** `Kuehl veraltet` hat nur einen sehr kleinen Abstand zwischen den Woertern, ohne Ueberlappung; nicht als Defekt bewertet, keine Aenderung.
Die hostseitige 7-px-Annahme (Textrechtecke) ist damit am realen Display fuer **DE** belegt (kein Abschneiden, keine Ueberlappung, laengste DE-Texte sichtbar); fuer EN/ES gilt nur die Owner-Aussage.

## UART und Geraetezustand
UART-Aufnahme 17:34:36–19:33:23 (7 127 s): kein Panic, kein Watchdog, kein `heap_alloc_failed`/`ui_skipped_low_memory`, kein Fehlerlog. **Befund:** Das Oeffnen des seriellen Ports zu Beginn dieser Aufnahme loeste einen Neustart des laufenden Images aus (Power-on-Reset, Laufzeit davor 1 h 54 min); gleiches Image, nichts geflasht. Der Browser-Smoke lief im Boot davor (15:41).

## Persistenz und Restore
`state_store` nach dem Lauf `ecaa22eb652ae0481f8e2cddce2a68530f3895113efde9a0e2ae672f205e5a59`; Offline-Diff gegen `P0` (23/23 Schluessel): `auth0` (Envelope 82 → 83, ein Login) und die Konfigurationsrecords `cm0–2`, `cr0–1`, `uc1–3` (Sprachwechsel DE/EN/ES); Boot-Bereich (`nvs`/`phy_init`) gleich `P0`. Restore: `state_store` byte-exakt (RB-2, Readback = `P0`), Original-App `factory_A.bin` (Readback-Hash `3b907b25…` = `P0`), Abschlusspruefung Boot `2244ca13…`, `state_store` `fbdd8705…`, App = `P0`. Passwortdatei vernichtet.
Nach dem Restore stand der Chip im **ROM-Bootloader** (kein App-Start, schwarzes Display, vom Owner bemerkt; erwartetes Verhalten von `esptool --after no-reset`).

## Bewertung HW-28-D1-01
`PASS` fuer die Punkte 1–4 und 6 (Owner-Verdikte und Fotos), Punkt 5 mit eingeschraenkter Evidenz (EN/ES nur Owner-Aussage), 5a `NOT_OBSERVABLE`. Kein Pre-Ready-/Ready-/Merge-Gate daraus abgeleitet.

## Kontrollboot der wiederhergestellten Original-Firmware (2026-10-25, 22:08–22:16, auf Owner-Anweisung)
Normaler EN-Reset (RTS-Puls ueber die UART-Aufnahme, kein Flash/Erase):
- **Builder (UART, 411 s, 532 Zeilen, bereinigter Auszug: [PR210_D1_CONTROL_BOOT_UART_20261025.txt](PR210_D1_CONTROL_BOOT_UART_20261025.txt)):** `rst:0x1 (POWERON_RESET)`, `boot:0x13`, `App version a9a1156`, `app_main` laeuft, `real actuators: disabled`, Touch-Kalibrierung `active_status=Available`, UI-Init abgeschlossen, WLAN `HomeConnected`, Heartbeat durchgehend; **kein** Panic, Watchdog, Abort, `heap_alloc_failed`, `ui_skipped_low_memory` und kein `E (`-Log; Web antwortet `HTTP 200`.
- **Owner-Beobachtung (Chat):** „Alles normal. Ok“ – Display und Touch funktionieren wieder normal (Home, Status und Rueckweg).
- **Persistenz nach dem Kontrollboot (Builder, Readback):** Boot-Bereich `2244ca13…`, App `3b907b25…` und `state_store` `fbdd8705…` sind **byteidentisch zu `P0`**; der normale Boot der Original-Firmware hat nichts geschrieben. Hinweis: Das Auslesen endete mit `--after hard-reset`, das Geraet startete danach nochmals; dieser zweite Boot wurde nicht erneut per Hash geprueft.
**Kontrollboot: `PASS`** (Builder-UART und Hashes, Owner-Beobachtung getrennt gefuehrt). Das beweist den Start der wiederhergestellten Original-Firmware, nicht die Funktion der D1-Diagnoseseite oder der Sensor-/Aktorhardware.
