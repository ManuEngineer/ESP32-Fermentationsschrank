# Plan: Espressif-first-Synchronisierung des bestehenden Adopt-or-Build-Audits

CONTEXT_BASELINE_SHA: 8d65b50326c4419dc45bbc024615c0a1c592e1aa (main, = Merge-Commit PR #84)
CONTEXT_HEAD_SHA: 8d65b50326c4419dc45bbc024615c0a1c592e1aa
CONTEXT_REFRESH_MODE: FULL (Toolchainwechsel-Synchronisierung; volle Erstorientierung dieser Sitzung)
PLAN_STATUS: DRAFT – wartet auf Ownerfreigabe dieses Plan-Commits

## Ziel

Die bestehenden Adopt-or-Build-Auditdokumente, das Third-Party-Register und
die drei betroffenen Live-Issues (#30, #31, #27) gezielt auf die inzwischen
verbindliche Produktionsbasis synchronisieren:

```text
Produktionsbasis: ESP-IDF 6.0.2
Commit: 7101770dc6db2667b3c477cc31365dd1acd6db4e
PlatformIO: ausschliesslich nativer Hosttestpfad
Arduino-ESP32: keine aktive Produktionsbasis
Quelle der Migration: Issue #71 (closed) / PR #79 (merged 2026-08-03)
```

Kein neuer Gesamt-Audit entsteht. Die bestehende Auditmethodik, Safety-Grenze,
Adapterstrategie und Hardware-Spike-Logik bleiben erhalten. Zusaetzlich werden
zwei neue kleine Live-Issues erstellt (WLAN-Onboarding-Evaluation,
ESP-IDF-NVS-`IStateStore`-Adapter) und `docs/ENGINEERING_PRINCIPLES.md` erhaelt
eine Espressif-first-Regel.

## Nicht-Ziele

- kein neuer Gesamt-Audit, kein paralleles Komponentenregister;
- keine Firmwareaenderung, kein neuer Code, keine neue Abhaengigkeit im Build;
- keine Aenderung an PR #84, an Schema 1 oder am `IStateStore`-Vertrag;
- keine endgueltige Produktivauswahl fuer DS18B20-Stack, Display/Touch-Treiber,
  Webserver, WLAN-Onboarding oder UI-Framework — alle bleiben
  `SPIKE_REQUIRED`/`FINAL_SELECTION_PENDING`, keine vorweggenommene
  Ownerentscheidung;
- keine Aenderung an Issue #17 (bereits separat als `COMPLETED`/`closed`
  abgeschlossen, siehe unten);
- keine neuen Sensor-, Display-, Touch-, LVGL- oder Webserver-Issues ausser den
  zwei explizit im Auftrag benannten;
- kein vollstaendiges Neuschreiben der bestehenden Audit-Prosa; nur
  nachgewiesen veraltete Toolchainannahmen, Kandidatenprioritaeten und die im
  Auftrag benannten Backloglücken werden korrigiert.

## Verbindliche Quellen und Entscheidungen

Vollstaendig gelesen (diese Sitzung):

- `AGENTS.md` (Root)
- `docs/ENGINEERING_PRINCIPLES.md`
- `docs/ENGINEERING_LEARNINGS.md`
- `docs/ADOPT_OR_BUILD.md`
- `docs/THIRD_PARTY_COMPONENTS.md`
- `docs/audits/RELEASE_1_ADOPT_OR_BUILD_AUDIT.md` (1363 Zeilen)
- `docs/audits/COMPONENT_EVALUATIONS.md` (757 Zeilen)
- `docs/audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md` (101 Zeilen)
- `docs/audits/PROPOSED_RELEASE_1_ROADMAP.md` (653 Zeilen)
- `docs/audits/HARDWARE_SPIKE_PLAN.md` (1113 Zeilen)
- `docs/audits/OPEN_BACKLOG_CLASSIFICATION.md` (131 Zeilen)
- `docs/audits/RELEASE_1_FUNCTION_MATRIX.md` (97 Zeilen)

Die sechs grossen Auditdokumente wurden zusaetzlich durch einen dedizierten
Recherche-Agenten vollstaendig (nicht nur per Stichwortsuche) gegengelesen, um
Formulierungen ausserhalb des anfaenglichen Stichwortsatzes
(`Arduino-ESP32`, `2.0.17`, `WiFiManager`, `WebServer`, `ESPAsyncWebServer`,
`DallasTemperature`, `OneWire`, `LovyanGFX`, `TFT_eSPI`, `LCDWiki`, `LVGL`)
zu erfassen, insbesondere `espressif32@7.0.1`, generische
"fixierte/Ziel-Toolchain"-Formulierungen und die veralteten Basis-Commits.
Ergebnis: vollstaendige, zeilengenaue Fundliste je Datei (siehe Abschnitt
"Betroffene Module und Dateien").

Live-Issues gelesen:

- **#17** (Laufpersistenz): `state: closed`, `state_reason: completed`;
  eigener `## Abschluss`-Abschnitt nennt PR #84 (Head
  `b59d463c91b4ba99ec922ceca5c2b1b7a9709052`, Merge-Commit
  `8d65b50326c4419dc45bbc024615c0a1c592e1aa`) als Umsetzung. Kein
  `STOP_BEFORE_BRANCH` noetig; Issue #17 wird von dieser Aufgabe nicht
  angefasst.
- **#19** (Journal/Historie/Backup/Import): `OPEN`, `PLANNED_SPEC_PENDING`;
  nur Kontext, keine geplante Aenderung.
- **#27** (Web-API/Weboberflaeche): `OPEN`, `PLANNED_SPEC_PENDING`; Body
  benennt keinen konkreten technischen Kandidaten (der lebt im Audit) —
  Ergaenzung siehe unten.
- **#29** (ESP32-Bring-up): `OPEN`, `BLOCKED_HARDWARE`; nur Kontext, keine
  geplante Aenderung.
- **#30** (DS18B20): `OPEN`, `BLOCKED_HARDWARE`; Body benennt keinen
  konkreten Kandidaten — Ergaenzung siehe unten.
- **#31** (Display/Touch): `OPEN`, `BLOCKED_HARDWARE`; Body nennt bereits
  "die nach main uebernommene ESP-IDF-6.0.2-Plattform aus #74" als
  Abhaengigkeit und "ESP-IDF-6.0.2-Kompatibilitaet" als Vergleichskriterium
  (bereits teilweise synchronisiert), aber keine konkreten
  `esp_lcd`-Kandidatennamen — Ergaenzung siehe unten.

Live-Verifikation der Espressif-Kandidaten (heute, via ESP Component Registry
und github.com/espressif; Einzelnachweise/URLs im Recherche-Agentenbericht
dieser Sitzung):

| Kandidat | Version | Mindest-ESP-IDF | Lizenz | Bemerkung |
|---|---|---|---|---|
| `espressif/onewire_bus` | `1.1.1` | `>=5.0` | Apache-2.0 | durch ESP-IDF 6.0.2 erfuellt; die bisherige `INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN`-Sorge (IDF 4.4) entfaellt |
| `espressif/ds18b20` | `0.4.0` | transitiv `>=5.0` (via `onewire_bus ^1.1.0`, zusaetzlich `sensor_hub ^0.1.4`) | Apache-2.0 | wie oben |
| ESP-IDF `esp_lcd` | Bestandteil ESP-IDF 6.0.2 | – | Apache-2.0 (ESP-IDF) | Panel-/IO-Abstraktion, kein separates Registrypaket |
| `espressif/esp_lcd_ili9341` | `2.0.2` | `>=4.4` | Apache-2.0 | erfuellt |
| `espressif/esp_lcd_touch` | `1.2.1` | `>=4.4.2` | Apache-2.0 | generische Touch-Abstraktion, kein XPT2046-Treiber selbst |
| `atanisoft/esp_lcd_touch_xpt2046` | `1.0.6` | `>=4.4` + `esp_lcd_touch >=1.0.4` | MIT | kein offizieller `espressif/*`-XPT2046-Treiber vorhanden; dies ist der am besten belegte kompatible Registry-Kandidat, wie im Auftrag als Rueckfall vorgesehen |
| `espressif/esp_lvgl_port` | `2.8.0~1` | `>=5.2` | Apache-2.0 | erfuellt; unterstuetzt LVGL `>=8, <10` |
| `espressif/network_provisioning` | `1.2.4` | `>=5.1` | Apache-2.0 | `wifi_provisioning` wurde in der ESP-IDF-6.0-Linie vollstaendig entfernt und durch dieses Out-of-Tree-Registrypaket ersetzt (`wifi_prov_*` → `network_prov_*`); SoftAP+HTTP-Provisioning bestaetigt, kein eingebauter DNS-Captive-Redirect |
| `protocomm` | nicht als eigenstaendiges Registrypaket auffindbar (404) | – | – | als interner Baustein von `network_provisioning` referenziert, nicht separat versioniert auffindbar; **ungeklaert, in Phase 2 als Fussnote statt harter Versionsangabe aufnehmen** |
| ESP-IDF `esp_http_server` | Bestandteil ESP-IDF 6.0.2 | – | Apache-2.0 (ESP-IDF) | synchroner eingebetteter HTTP-Server |

## Aktuelle Ausgangslage

- `main` = `8d65b50326c4419dc45bbc024615c0a1c592e1aa` (PR #84 gemergt,
  enthaelt die komplette Laufpersistenz aus Issue #17).
- Die ESP-IDF-6.0.2-Migration ist bereits abgeschlossen und akzeptiert
  (Issue #71 geschlossen, PR #79 gemergt 2026-08-03) — diese Aufgabe trifft
  **keine neue Entscheidung**, sie traegt eine bereits getroffene Entscheidung
  in die aelteren, vor der Migration geschriebenen Auditdokumente nach.
- `docs/THIRD_PARTY_COMPONENTS.md` ist teilweise bereits synchron: die Zeile
  "ESP32-Plattform" nennt bereits korrekt ESP-IDF `v6.0.2`
  (`7101770dc6db2667b3c477cc31365dd1acd6db4e`) als aktive Produktionsbasis und
  vermerkt "PlatformIO/Arduino ist keine aktive Produktionsbasis"; ebenso
  wurden die Zeilen "Persistenz" und die erste "Webserver"-Zeile bereits auf
  "unter ESP-IDF zu evaluierend" umgestellt. Die uebrigen Zeilen (Display/
  Touch, DS18B20, WLAN-Onboarding, zweite Webserver-Zeile) benennen weiterhin
  ausschliesslich Arduino-Kandidaten ohne Espressif-Gegenstueck.
- Die sechs `docs/audits/*`-Dokumente und
  `docs/audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md` stammen fachlich von
  vor der Migration (Basis-Commit `7713a66cbf51eb078bd0f5e43c1163d1e0f47e1f`,
  Stand `2026-07-27`) und behandeln Arduino-ESP32 `2.0.17` auf
  `PlatformIO espressif32@7.0.1` (ESP-IDF-4.4-Generation) durchgaengig als
  aktuelle Produktionsbasis.
- `docs/ADOPT_OR_BUILD.md` definiert die Adopt-or-build-Reihenfolge noch als
  4-stufige Liste beginnend mit "mit der fixierten ESP32-/Arduino-Toolchain
  gelieferte Frameworkfunktion" (Zeile 19) — abweichend von der im Auftrag
  vorgegebenen 5-stufigen Espressif-first-Reihenfolge.
- `docs/ENGINEERING_PRINCIPLES.md` enthaelt noch keine Espressif-first-Regel.
- **Wichtiger, nicht mechanisch aufloesbarer Befund** (siehe "Offene
  Entscheidungen" Punkt 1): Root-`AGENTS.md` schliesst einen
  Arduino-Produktionspfad ausdruecklich aus ("Ein Arduino-Produktionspfad
  besteht nicht"). Ein grosser Teil der bisherigen Rueckfallkandidaten
  (LovyanGFX, TFT_eSPI, Arduino_GFX, Adafruit GFX/ILI9341,
  XPT2046_Touchscreen, DallasTemperature+OneWire, WiFiManager, ArduinoJson,
  ESPAsyncWebServer, Arduino PID/QuickPID) sind Arduino-Kernbibliotheken. Ob
  sie unter reinem ESP-IDF 6.0.2 ueberhaupt technisch nutzbar waeren (z. B.
  nur ueber die offizielle "Arduino als ESP-IDF-Komponente"-Integration),
  ist eine neue, bisher ungeklaerte Architekturfrage, die dieser
  Synchronisierungsauftrag nicht entscheidet.

## Betroffene Module und voraussichtlich betroffene Dateien

Ausschliesslich Dokumentation und Backlog, keine Firmware-/Testdateien:

1. `docs/ADOPT_OR_BUILD.md` — Grundsatz-Reihenfolge (Zeile 19), Abschnitt
   "Lokaler HTTP-Transport und Weboberflaeche" (Zeilen 137–155), Abschnitt
   "WLAN-Onboarding" (Zeilen 92–111).
2. `docs/THIRD_PARTY_COMPONENTS.md` — Zeilen Display/Touch (40–43), Touch
   (45), DS18B20 (46–47, Prioritaet tauschen), Webserver (50–51), WLAN-
   Onboarding (52); ESP32-Plattform-/Persistenz-Zeilen bleiben unveraendert
   (bereits synchron).
3. `docs/audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md` — Zeile 50
   ("Zielframework", `FRAMEWORK_PRESENT`, faelschlich "bestehende
   Buildgrundlage"), Zeile 51 (Arduino-ESP32-WebServer-Registrierung), Zeile
   52 ("ESP-IDF-Referenz", faelschlich `AUDIT_ONLY` mit "kein
   Toolchainwechsel im Audit"), Zeile 62–63 (Espressif onewire_bus/ds18b20,
   Prioritaet anheben), Zeile 67 (mbedTLS-Pfad auf ESP-IDF direkt statt via
   Arduino-ESP32), neue Zeilen fuer `esp_lcd_ili9341`, `esp_lcd_touch`,
   `atanisoft/esp_lcd_touch_xpt2046`, `esp_lvgl_port`, `network_provisioning`.
4. `docs/audits/RELEASE_1_ADOPT_OR_BUILD_AUDIT.md` — Kopfzeilen (Basis-Commit/
   Toolchain, Zeilen 19–23), Abschnitt 3 "Toolchainkompatibilitaet"
   (Zeilen 145–152, die `onewire_bus`/`ds18b20`-Inkompatibilitaetsaussage),
   Abschnitt 7 "Frameworkserver" (Zeilen 234–261), Abschnitt 8 "OD-06"
   (Zeilen 303–353), generische "fixierte Toolchain"-Stellen (Zeilen 31, 74,
   88–89, 189, 378, 844, 960) per gebuendelter Ersetzung.
5. `docs/audits/COMPONENT_EVALUATIONS.md` — Kopfzeilen (Zeilen 7–17),
   Aktivitaetstabelle (Zeile 40), Displaykandidaten (Zeilen 73–74), DS18B20-
   Abschnitt (Zeile 133, Prioritaet/Status), Webserver-Abschnitt (Zeilen
   380–461), WiFiManager-Abschnitt (Zeilen 487–591), generische
   Toolchain-Build-Ziel-Stellen (Zeilen 184, 200, 237, 309–310, 527–528, 750).
6. `docs/audits/HARDWARE_SPIKE_PLAN.md` — Gate-1/2-Beschreibung (Zeilen 87,
   100–104, inklusive der Aussage "Ein Toolchain- oder Frameworkwechsel ist
   nicht zulaessig"), "Gemeinsame Spikebedingungen" (Zeile 136) und alle
   Wiederholungen des Build-Ziel-Strings (Zeilen 149, 223–224, 748–749, 848),
   Spike B/DS18B20-Akzeptanzkriterium (Zeilen 492–495, die
   `INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN`-Klausel), Spike C/WiFiManager
   (Zeilen 728–817), Spike-Kandidatenlisten Display (Zeilen 191–193) und
   DS18B20 (Zeile 486), Webserver-Baselineprototyp-Abschnitt (Zeilen
   696–733), Auth-Abschnitt (Zeilen 1009, 1027).
7. `docs/audits/PROPOSED_RELEASE_1_ROADMAP.md` — Webserver-/WiFiManager-
   Schritte 5–6 (Zeilen 265–293), OD-04/OD-06-Statuszeilen (Zeilen 350–360,
   625–638), generische "fixierte Toolchain"-Stellen (Zeilen 238, 286, 297,
   300).
8. `docs/audits/OPEN_BACKLOG_CLASSIFICATION.md` — Zeile 66 (#29-Zeile,
   "fixierte PlatformIO-/Arduino-Toolchain"), Zeile 67 (#30-Zeile,
   "Toolchain-Scheitern ist keine generelle Untauglichkeit" —
   Formulierung bleibt als Prinzip, Bezug auf konkreten Konflikt entfaellt),
   Zeile 59 (#27-Zeile, "WebServer/ArduinoJson zuerst" → `esp_http_server`
   zuerst), Zeile 111 (Punkt 9, WiFiManager-Alleinstellung).
9. `docs/audits/RELEASE_1_FUNCTION_MATRIX.md` — Kopfzeile (Basis-Commit,
   Zeilen 7–8), Zeile 19 (Baseline), Zeile 20 (Display-Kandidaten), Zeile 30
   (WLAN, "Arduino-ESP32 WiFi"), Zeile 31 (WLAN-Onboarding, WiFiManager
   alleinig), Zeile 32 (Webserver, Arduino `WebServer` als
   `FIRST_EVALUATION_CANDIDATE`), Zeile 39 (UART-Update, "PlatformIO"
   unqualifiziert).
10. `docs/ENGINEERING_PRINCIPLES.md` — neuer Abschnitt Espressif-first-Regel
    (Ende der Datei, nach "KISS").
11. `AGENTS.md` (Root) — **voraussichtlich keine Aenderung**; die bestehende
    Bindung im Abschnitt "Verbindliche Software-Engineering-Grundsaetze"
    verweist bereits verbindlich auf `docs/ENGINEERING_PRINCIPLES.md` fuer die
    ausfuehrliche Erlaeuterung. Endgueltige Bestaetigung erfolgt am Anfang von
    Phase 2 durch erneuten Blick auf den dann aktualisierten Abschnitt.
12. Issue #30 (GitHub, `gh issue edit`) — Ergaenzung eines Abschnitts mit den
    Primaer-/Rueckfallkandidaten und Vergleichskriterien (Entwurf unten).
13. Issue #31 (GitHub, `gh issue edit`) — Ergaenzung der konkreten
    `esp_lcd`-Kandidatenmatrix (Entwurf unten).
14. Issue #27 (GitHub, `gh issue edit`) — Ergaenzung: `esp_http_server` als
    technischer HTTP-Primaerkandidat, WLAN-Onboarding-Abgrenzung (Entwurf
    unten).
15. Zwei neue Live-Issues (GitHub, `gh issue create`) — WLAN-Onboarding-
    Evaluation, ESP-IDF-NVS-`IStateStore`-Adapter (Entwuerfe unten).

Nicht angefasst: PR #84, jede Datei unter `lib/`, `src/`, `main/`, `test/`,
`scripts/`; Issue #17; Issues #19/#29 (nur Kontext gelesen, keine
Auftragsvorgabe zur Aenderung).

## Abhaengigkeiten und Gates

- Owner-Freigabe dieses Plan-Commits vor jeder Umsetzung (Draft-PR haelt an).
- Keine Abhaengigkeit von offenen Hardware-Gates; reine Dokumentenarbeit.
- `git diff --check` und vorhandene Dokumentations-/Quality-Gates
  (`python3 scripts/check_secrets.py`,
  `python3 scripts/check_architecture_boundaries.py`,
  `python3 scripts/selftest_quality_gates.py`) muessen unveraendert gruen
  bleiben, da keine der drei Pruefungen inhaltlich von Markdown-Aenderungen
  betroffen ist; sie dienen hier als Nachweis, dass ausschliesslich
  Dokumentation geaendert wurde und keine Firmwaredatei versehentlich
  beruehrt wird.
- GitHub Actions (`ESP-IDF + Native CI`) muss auf dem finalen Head gruen
  sein — reine Dokumentenaenderung, aber Nachweis wie bei jedem PR verlangt.

## Geplanter kleiner PR-/Commit-Schnitt

Ein Draft-PR (`docs/espressif-first-audit-sync`), Umsetzung nach Freigabe in
kleinen, je Datei/Thema getrennten Commits:

1. `docs: add Espressif-first rule to engineering principles`
   (`docs/ENGINEERING_PRINCIPLES.md`)
2. `docs: correct adopt-or-build search order to Espressif-first`
   (`docs/ADOPT_OR_BUILD.md`)
3. `docs: sync third-party component register to ESP-IDF 6.0.2 candidates`
   (`docs/THIRD_PARTY_COMPONENTS.md`)
4. `docs: sync license register to ESP-IDF 6.0.2 baseline`
   (`docs/audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md`)
5. `docs: sync adopt-or-build audit to ESP-IDF 6.0.2 baseline`
   (`docs/audits/RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`)
6. `docs: sync component evaluations to ESP-IDF 6.0.2 baseline`
   (`docs/audits/COMPONENT_EVALUATIONS.md`)
7. `docs: sync hardware spike plan to ESP-IDF 6.0.2 baseline`
   (`docs/audits/HARDWARE_SPIKE_PLAN.md`)
8. `docs: sync proposed roadmap to ESP-IDF 6.0.2 baseline`
   (`docs/audits/PROPOSED_RELEASE_1_ROADMAP.md`)
9. `docs: sync backlog classification to ESP-IDF 6.0.2 baseline`
   (`docs/audits/OPEN_BACKLOG_CLASSIFICATION.md`)
10. `docs: sync function matrix to ESP-IDF 6.0.2 baseline`
    (`docs/audits/RELEASE_1_FUNCTION_MATRIX.md`)
11. `docs: confirm AGENTS.md needs no direct change` (nur falls Phase-2-Check
    dies bestaetigt; sonst entfaellt dieser Commit ersatzlos)
12. Live-Issue-Aktualisierungen #30/#31/#27 (`gh issue edit`, kein Commit)
13. Zwei neue Live-Issues (`gh issue create`, kein Commit)

Jeder Dokumenten-Commit wendet zwei Arten von Aenderungen an:

- eine **kleine Menge wiederkehrender, textuell identischer Ersetzungen**
  fuer generische Toolchain-Zielbeschreibungen (z. B.
  "PlatformIO espressif32@7.0.1, Arduino-ESP32 2.0.17 (dcc1105b)" →
  "ESP-IDF 6.0.2 (7101770dc6db2667b3c477cc31365dd1acd6db4e); PlatformIO
  ausschliesslich nativer Hosttestpfad"), wo diese Formulierung ein
  *aktuelles* Build-/Reproduzierbarkeitsziel behauptet;
- **gezielte inhaltliche Korrekturen** an den oben einzeln aufgefuehrten
  Stellen (Prioritaets-/Statusaenderungen, neue Kandidatenzeilen,
  Abschnittsueberschriften), die nicht mechanisch, sondern nur mit
  inhaltlichem Verstaendnis korrekt sind.

Historische Auditdaten (urspruengliches Auditdatum `2026-07-27`, urspruenglicher
Basis-Commit `7713a66c...`) werden nicht geloescht, sondern durch eine neue
"Synchronisiert am `<Datum>` gegen Baseline `8d65b50...`"-Zeile direkt daneben
ergaenzt — Widersprueche werden offen benannt statt still aufgeloest
(`docs/ENGINEERING_PRINCIPLES.md`, Abschnitt "Repository als Quelle der
Wahrheit").

## Entwuerfe fuer Issue-Ergaenzungen (Phase 2, nach Freigabe unveraendert zu uebernehmen)

### Issue #30 – neuer Abschnitt "## Espressif-first-Kandidaten (Sync)"

```text
Primaerkandidat (Espressif-first, ESP-IDF 6.0.2 nativ):
- espressif/onewire_bus 1.1.1 (ESP-IDF >=5.0, Apache-2.0)
- espressif/ds18b20 0.4.0 (transitiv >=5.0, Apache-2.0)

Rueckfallkandidat (Arduino-Bibliothek; Nutzbarkeit unter reinem ESP-IDF
6.0.2 ungeklaert, siehe Auditfussnote):
- DallasTemperature 4.0.6 + OneWire 2.3.8 (beide MIT)

Zu vergleichen: ESP-IDF-6.0.2-/ESP32-Kompatibilitaet, RMT-/UART-Backend,
Mehrbus-/Mehrsensorfaehigkeit, ROM-Enumeration, CRC, 12-Bit-Messung,
Hot-Plug, Trennen/Wiedererkennen, Timeout-/Fehleruebersetzung,
Flash/DRAM/Heap/Stack, Lizenz/Notices, schmaler Plattformadapter ohne
Bibliothekstypen im Fachkern. Kein neues Sensor-Issue; Details im Audit
(`docs/audits/COMPONENT_EVALUATIONS.md`, `docs/audits/HARDWARE_SPIKE_PLAN.md`).
```

### Issue #31 – Ergaenzung im bestehenden Scope-Abschnitt

```text
Espressif-first-Kandidatenmatrix (Sync):
- Display: ESP-IDF esp_lcd (eingebaut) + espressif/esp_lcd_ili9341 2.0.2
  (ESP-IDF >=4.4, Apache-2.0)
- Touch-Grundlage: espressif/esp_lcd_touch 1.2.1 (ESP-IDF >=4.4.2,
  Apache-2.0)
- XPT2046: kein offizieller espressif/*-Treiber vorhanden;
  atanisoft/esp_lcd_touch_xpt2046 1.0.6 (ESP-IDF >=4.4 + esp_lcd_touch
  >=1.0.4, MIT) ist der am besten belegte kompatible Registry-Kandidat
- UI-Framework: espressif/esp_lvgl_port 2.8.0~1 (ESP-IDF >=5.2, Apache-2.0,
  LVGL 8/9) ueber LVGL als bevorzugten, nicht vorentschiedenen Kandidaten

Weiterhin ergebnisoffen gegen vorhandene Rueckfallkandidaten (LovyanGFX,
TFT_eSPI, LCDWiki) und gegen schlanke eigene Views vergleichen; LVGL nicht
automatisch auswaehlen. Details im Audit
(`docs/audits/COMPONENT_EVALUATIONS.md`, `docs/audits/HARDWARE_SPIKE_PLAN.md`).
```

### Issue #27 – Ergaenzung im bestehenden Scope-Abschnitt

```text
Technischer HTTP-Primaerkandidat (Sync): ESP-IDF esp_http_server
(eingebaut, ESP-IDF 6.0.2). Arduino-ESP32 WebServer ist keine aktive
Produktionsrichtung mehr. ESPAsyncWebServer bleibt hoechstens konditionaler
Rueckfall nach einem konkret belegten Problem.

WLAN-Onboarding ist ausdruecklich nicht Bestandteil von #27 (siehe separates
neues Issue).
```

### Neues Issue A – WLAN-Onboarding-Evaluation

```text
Titel: [E5.x] ESP-IDF-WLAN-Onboarding und Provisionierung evaluieren

## Status

`PLANNED_SPEC_PENDING`

## Scope

- browserbasierter SoftAP-/Captive-Portal-Vertrag
- ESP-IDF network_provisioning (ESP-IDF >=5.1, Apache-2.0; Nachfolger des in
  der ESP-IDF-6.0-Linie entfernten wifi_provisioning) als offizieller
  Pflichtkandidat, ergebnisoffen gegen WiFiManager (Arduino-Bibliothek;
  Nutzbarkeit unter reinem ESP-IDF 6.0.2 ungeklaert) verglichen
- kleiner eigener SoftAP-/DNS-/HTTP-Adapter auf ESP-IDF als Gegenkandidat
- ausdruecklicher Portalstart; individuelle SoftAP-Zugangsdaten; WLAN-QR;
  sichtbarer direkter IP-Rueckfall
- Credential-Kandidat, Validierung und atomarer Commit; Secret-Redaction
- Neustart, Fehler und Recovery
- Android-, iOS- und Windows-Clients
- Flash-/RAM-/Heap-/Stack-/Jittermessung
- Lizenz und Notices
- keine Cloud- oder Apppflicht fuer den gewaehlten R1-Pfad

Keine Auswahl vor identischem Spike und Ownerentscheid.

## Abhaengigkeiten

- getrennt von #27 (Web-API/Weboberflaeche)
- Epic: [passendes E5-Epic, siehe #29–#31]
```

### Neues Issue B – ESP-IDF-NVS-`IStateStore`-Adapter

```text
Titel: [E5.x] ESP-IDF-NVS-Adapter fuer IStateStore implementieren und
verifizieren

## Status

`PLANNED_SPEC_PENDING`

## Scope

- produktiver Adapter von ESP-IDF NVS auf vorhandenes IStateStore
- Namespace- und Schluesselabbildung; begrenzte Blobgroessen; nvs_commit
- exakter Ruecklesevertrag; Uebersetzung aller NVS-Fehler in bestehende
  Store-Ergebnisse
- Kapazitaet und Partition; Flashverschleiss
- Stromunterbruch und CommitOutcomeUnknown
- reale ESP32-Hardwaretests
- Flash-, DRAM-, Heap- und Stackmessung
- Lizenz und Notices

## Ausdrueckliche Grenze

- keine Aenderung der Fachlogik
- kein Ersatz des Active-/Fallback-/Head-Vertrags ohne separaten belegten
  Ownerentscheid
- keine Aenderung des Schema-1-Wireformats aus PR #84
- keine CBOR- oder LittleFS-Migration in diesem Issue

## Abhaengigkeiten

- #29 (ESP32-Bring-up), bestehende Persistenzgrundlagen aus PR #84
- Epic: [passendes E5-Epic]
```

Owner entscheidet in der Planfreigabe ueber exakten Epic-Bezug, finalen
Titel-Praefix (`[E5.x]`) und ob weitere Abhaengigkeiten (z. B. #17) ergaenzt
werden sollen; obige Entwuerfe sind Vorschlaege, keine endgueltige Fassung.

## Daten-, Zustands- und Schnittstellenvertraege

Nicht betroffen. Diese Aufgabe aendert keinen Firmwarevertrag. Ausdruecklich
dokumentiert wird an mehreren Stellen:

```text
PR #84: MERGED
IStateStore-Vertrag: BESTEHEND
Schema-1-Wireformat: BESTEHEND UND UNVERAENDERT
Produktiver NVS-Adapter: SEPARATES FOLGEISSUE (neues Issue B oben)
```

## Fehler-, Recovery-, Security- und Safetygrenzen

Nicht betroffen (reine Dokumentation). Keine der Aenderungen aktiviert,
deaktiviert oder veraendert eine Safety-, Security- oder Recoverygrenze der
Firmware.

## Teststrategie

- `git status --short` vor und nach jedem Commit: nur die geplanten
  Markdown-/Issue-Aenderungen sichtbar, keine Datei unter `lib/`, `src/`,
  `main/`, `test/`, `scripts/`.
- `git diff --check` nach jedem Commit.
- `python3 scripts/check_secrets.py`,
  `python3 scripts/check_architecture_boundaries.py`,
  `python3 scripts/check_architecture_boundaries.py --selftest`,
  `python3 scripts/selftest_quality_gates.py` am Ende von Phase 2 (muessen
  unveraendert gruen bleiben; sie dienen als Nachweis "keine Firmwaredatei
  geaendert", nicht als inhaltlicher Test der Dokumentation).
- kein `pio test -e native` erforderlich (keine Test- oder Produktionsdatei
  geaendert); wird trotzdem einmal am finalen Head laufen gelassen, um
  `REPOSITORY_CHANGED`-Nachweise fuer den Abschlussbericht zu erhaeten, falls
  der Owner das verlangt.
- finaler Remote-CI-Lauf (`ESP-IDF + Native CI`) auf dem letzten Commit vor
  Ready-for-Review muss gruen sein.

## Dokumentationsaenderungen

Siehe "Betroffene Module und voraussichtlich betroffene Dateien" oben —
diese Aufgabe besteht vollstaendig aus Dokumentationsaenderungen.

## Offene Entscheidungen

1. **Arduino-Rueckfallkandidaten unter "kein Arduino-Produktionspfad".**
   Root-`AGENTS.md` schliesst einen Arduino-Produktionspfad ausdruecklich aus.
   Mehrere bisherige Rueckfallkandidaten (LovyanGFX, TFT_eSPI, LCDWiki,
   Arduino_GFX, Adafruit GFX/ILI9341, XPT2046_Touchscreen,
   DallasTemperature+OneWire, WiFiManager, ArduinoJson, ESPAsyncWebServer,
   Arduino PID/QuickPID) sind Arduino-Kernbibliotheken. Ob sie unter reinem
   ESP-IDF 6.0.2 ueberhaupt technisch nutzbar waeren (z. B. nur ueber die
   offizielle "Arduino als ESP-IDF-Komponente"-Integration von Espressif),
   ist eine neue Architekturfrage. **Vorschlag fuer diese Synchronisierung:**
   Diese Kandidaten bleiben im Audit als Rueckfall/`AUDIT_ONLY` sichtbar,
   erhalten aber an jeder relevanten Stelle eine kurze Fussnote, dass ihre
   technische Nutzbarkeit unter dem reinen ESP-IDF-6.0.2-Produktionspfad
   ungeklaert ist und vor einem Spike zuerst zu pruefen waere. Diese
   Aufgabe trifft **keine** Auswahl oder Ausschlussentscheidung — das bleibt
   dem Owner vorbehalten. Owner-Feedback zu diesem Vorschlag wird mit der
   Planfreigabe erbeten.
2. **`protocomm` als Registrypaket nicht auffindbar.** Fuer das neue
   WLAN-Onboarding-Issue wird deshalb nur `network_provisioning` mit
   versionierter Quelle genannt; `protocomm` wird als intern gebuendelter
   Baustein erwaehnt, nicht mit einer eigenen (moeglicherweise falschen)
   Versionsnummer versehen.
3. **AGENTS.md-Aenderung.** Vorlaeufige Einschaetzung: keine Aenderung noetig,
   da die bestehende Bindung an `docs/ENGINEERING_PRINCIPLES.md` ausreicht.
   Wird zu Beginn von Phase 2 am dann aktuellen Text erneut bestaetigt.
4. **Epic-Zuordnung und Titel-Praefix der zwei neuen Issues.** Vorschlag
   `[E5.x]` in Anlehnung an #29–#31 (Epic #7); Owner kann bei Freigabe einen
   anderen Epic-Bezug vorgeben.
5. **Umgang mit dem urspruenglichen Auditdatum/-Basis-Commit.** Vorschlag:
   nicht loeschen, sondern durch eine zusaetzliche Sync-Zeile ergaenzen (siehe
   "Geplanter kleiner PR-/Commit-Schnitt"). Owner kann stattdessen ein
   vollstaendiges Ersetzen verlangen.

## Bewertung gegen SOLID, DRY und KISS

- **KISS:** Es wird bewusst *kein* neuer Gesamt-Audit und kein Parallelregister
  erzeugt; die vorhandene Struktur (ein Audit, ein Register, ein
  Lizenznachweis) bleibt die einzige Quelle. Die vorgeschlagene
  "Ersetzungsmuster + gezielte Korrekturen"-Strategie vermeidet ein
  vollstaendiges Neuschreiben von ueber 4000 Zeilen Audit-Prosa, ohne
  inhaltliche Falschaussagen (aktive Arduino-Produktionsbasis) stehen zu
  lassen.
- **DRY:** Die Toolchain-Fakten (ESP-IDF 6.0.2, Commit, PlatformIO-Rolle)
  haben durch `docs/THIRD_PARTY_COMPONENTS.md` bereits eine kanonische
  Quelle; diese Aufgabe uebernimmt dieselben Werte woertlich in alle
  betroffenen Dokumente, statt sie neu zu erfinden oder zu variieren.
- **SOLID (Dependency Inversion sinngemaess auf Dokumentation uebertragen):**
  Die Auditmethodik (Statuswerte, Such-/Bewertungsreihenfolge) bleibt die
  stabile Abstraktion; nur die konkreten, darunterliegenden
  Toolchain-/Kandidatenfakten werden ausgetauscht.
- Bewusste Abweichung: Diese Aufgabe aendert drei Live-Issues direkt, obwohl
  `docs/audits/OPEN_BACKLOG_CLASSIFICATION.md` selbst festhaelt "Der Audit
  veraendert die Live-Issues und ADRs nicht" (Zeile 129). Das ist eine
  ausdruecklich vom Owner angeordnete Ausnahme fuer genau diese drei Issues
  und die zwei neuen Issues in diesem Auftrag, keine stille Abweichung von
  der Auditkonvention.

## Verbleibende Marker

Unveraendert `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING`, `TBD_HARDWARE`,
`TBD_COMMISSIONING`, `EVALUATE_BEFORE_RELEASE`, `EVALUATE_LATER`,
`DEFER_AFTER_R1` an allen bisherigen Stellen; diese Aufgabe aendert keinen
dieser Marker inhaltlich, nur die darunterliegenden Toolchain-/
Kandidatenfakten.

## Ausdruecklich verbotene Vorwegnahmen

- keine Produktivauswahl fuer DS18B20-Stack, Display-/Touch-Treiber,
  Webserver, WLAN-Onboarding, UI-Framework oder Auth-KDF;
- keine Entscheidung ueber die Arduino-Rueckfallkandidaten-Frage (offene
  Entscheidung 1);
- kein Commit oder keine Aenderung auf dem Branch von PR #84;
- keine ADR-Erstellung oder -Aenderung;
- keine Firmware-, Test- oder Build-Aenderung.

## Abnahmekriterien

- alle geaenderten Dokumente widerspruchsfrei (keine aktive
  Arduino-Produktionsbasis mehr behauptet, urspruengliche Auditdaten bleiben
  nachvollziehbar);
- #30, #31 und #27 korrekt ergaenzt (nicht neu geschrieben);
- genau zwei neue Issues erstellt, danach mit realer GitHub-Nummer in den
  Dokumenten referenziert (nicht vorher erfunden);
- Lizenzen/Quellen aller neu aufgenommenen Kandidaten dokumentiert (siehe
  Tabelle oben);
- keine Aenderung an PR #84, keine Firmwaredatei geaendert;
- `git diff --check`, vorhandene Dokumentations-/Quality-Gates und finaler
  Remote-CI-Lauf gruen;
- PR bleibt Draft, kein Merge, kein Ready-for-Review, kein Rebase, kein
  Force-Push, Branch nicht geloescht.
