# PR #210 / Issue #28 D1 – Smoke-Protokoll auf dem realen ESP32 (HW-28-D1-01)

Stand: vorbereitet, **`NOT_RUN`** bis zur ausdruecklichen Ownerfreigabe `G-F`, `G-S`, `G-U`, `G-R`, `G-B` fuer das unten genannte Image (Plan §6.4a).
Kein Nachweis fuer DS18B20-/Aktorhardware, D9 oder RAM-Reserve. `ACTUATOR_RELEASE=NO`.

## Image und Rahmen
| Groesse | Wert |
|---|---|
| Quelle | PR-HEAD `ad8b075e89ec174ffabec73ed0c7dcca647928a7` (Produktcode = `dc191c3`), `esp32_release`, `--require-clean-source-tree` PASS |
| Image (uninstrumentiert, **kein** Messoverlay) | `esp32_fermentationsschrank.bin`, SHA-256 `7edf0c222d15d3cd09dff36888f553c50c10ae938e0e0b1b9b686e9aa1773441`, 1 779 136 B |
| `sdkconfig.h` / IDF | byteidentisch zum fruehren 8 704-B-Diagnoseimage, IDF `fff9895c…` → PHY-/System-NVS-Bedingungen unveraendert |
| Geraetezustand vor dem Lauf | Boot-Bereich, App (`3b907b25…`) und `state_store` (`fbdd8705…`) = `P0` (zuletzt gelesen heute: Boot und `state_store` gleich `P0`) |
| Schreibumfang | nur App `0x10000` (G-F); Login im Browser-Smoke schreibt `auth0` im `state_store` (G-S: byte-exakter Restore danach); kein Erase, kein Werksreset, kein Powercut |

## A. Touch-Smoke (manuell durch den Owner am Geraet; der Agent kann das Display nicht beruehren)
Der Agent startet das Image (EN-Reset), liest parallel die UART und protokolliert; der Owner bedient und meldet jeweils das Gesehene (Foto oder Text).
1. **Navigation:** Home → `Status` → `Diagnose` erreichbar; Rueckweg (Zurueck) funktioniert.
2. **Feste Zeilen (Fenster 0):** `Resetursache` (nach EN-Reset erwartet `Extern`/`External`, nach Power-on `Einschalten`; **kein** Wert darf erfunden sein), `Fehlercode: keiner`, `Zustand` (Bereit/Standby, kein `Boot`).
3. **Pager:** mit `Runter` neunmal um **eine** Zeile verschieben; Fenster zeigt immer drei Zeilen; am Ende nicht weiter; mit `Hoch` zurueck. Fensterpositionen 3–8 pruefen: je Rolle eine Zeile `Rolle · Qualitaet · Alter` und eine Wertezeile `Kurzrolle · Qualitaet · R/K/F`; **jede Wertezeile traegt Kurzrolle und Qualitaet auch allein im Fenster**.
4. **Ohne Sensoren (Board hat keine):** Qualitaet `veraltet` im Warnton, Alter `--`, alle Werte `--.-` – nie `0`, nie `gueltig`.
5. **Sprachen DE/EN/ES:** Header-Sprachwahl; Diagnoseseite jeweils komplett lesen. **Textdarstellung pruefen:** kein abgeschnittener oder ueberlappender Text, kein Zeichen ausserhalb der Zeile, laengste Texte (`Aire del armario`, `Spannungsstoerung`/`Subtension`, `obsoleto`) vollstaendig sichtbar; die hostseitige 7-px-Annahme wird damit am realen Display bestaetigt oder widerlegt.
6. **Unveraendert:** `Diagnose → Service` fuehrt wie bisher zur PIN-/Service-Seite (nur ansehen, **keine PIN eingeben, kein Werksreset, keine Aktion**); Zeilen der Diagnoseseite sind nicht beruehrbar (Druck bewirkt nichts).
Ergebnis je Punkt `PASS`/`FAIL`/`PARTIAL`; Abweichungen werden nicht als Messung umgedeutet.

## B. Browser-Smoke (bestehende Weboberflaeche, keine neue Funktion)
Skript [PR210_D1_BROWSER_SMOKE.mjs.txt](PR210_D1_BROWSER_SMOKE.mjs.txt) (Chromium headless, kein falsches Passwort, kein Run/Namens-Speichern): Login-Shell, Anmeldung, Navigation Uebersicht/Meldungen/Programme/Einstellungen, Status- und Temperaturanzeige (ohne Sensoren wahrheitsgemaess `Veraltet`/`--`, nie `0.0 °C`), DE/EN/ES, Abmeldung; Sessions werden immer beendet. Das Web-Passwort kommt aus einer privaten 600-Datei und wird danach vernichtet.

## Abbruch-/Restorebedingungen
STOP bei unerwarteter Persistenzaenderung ausser `auth0`, Panic/Reset-Schleife/OOM oder unsicherem Restore → nur autorisierte Recovery per `esptool` im ROM-Bootloader. Nach dem Lauf: `state_store` (RB-2) und Original-App (`factory_A.bin`) byte-exakt zurueck, Boot-Bereich gegen `P0` lesend pruefen, Hashkette und Passwortdatei-Vernichtung dokumentieren; **Kontrollboot der Original-Firmware bleibt `NOT_RUN`**, sofern nicht ausdruecklich beauftragt.
