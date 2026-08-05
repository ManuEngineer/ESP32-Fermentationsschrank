# Plan: Espressif-first-Synchronisierung des bestehenden Adopt-or-Build-Audits

CONTEXT_BASELINE_SHA: 8d65b50326c4419dc45bbc024615c0a1c592e1aa (main, = Merge-Commit PR #84)
CONTEXT_HEAD_SHA: 8d65b50326c4419dc45bbc024615c0a1c592e1aa
CONTEXT_REFRESH_MODE: INCREMENTAL (bestehender Audit wird inkrementell auf die bereits gemergte ESP-IDF-6.0.2-Produktionsbasis synchronisiert; kein neuer Gesamtaudit)
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
| `protocomm` | offizielle ESP-IDF-Komponente, an ESP-IDF 6.0.2 gebunden (keine eigene Component-Registry-Version) | ESP-IDF 6.0.2 (Bestandteil) | Apache-2.0 (ESP-IDF) | `network_provisioning` nutzt `protocomm` fuer sichere Sessions; `protocomm` kann zusaetzlich direkt fuer einen eigenen SoftAP-/HTTP-/DNS-Provisionierungsweg evaluiert werden (siehe neues Issue `[E5.6]`, Pfad 2) |
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
- **Durch Ownerklarstellung entschiedene Rueckfallkandidatenregel** (siehe
  "Offene Entscheidungen" Punkt 1): Root-`AGENTS.md` schliesst einen
  Arduino-Produktionspfad ausdruecklich aus ("Ein Arduino-Produktionspfad
  besteht nicht"). Ein grosser Teil der bisherigen Rueckfallkandidaten
  (LovyanGFX, TFT_eSPI, Arduino_GFX, Adafruit GFX/ILI9341,
  XPT2046_Touchscreen, DallasTemperature+OneWire, WiFiManager, ArduinoJson,
  ESPAsyncWebServer, Arduino PID/QuickPID) sind Arduino-Kernbibliotheken.
  Espressif-first bestimmt jedoch nur die Recherche-/Pruefprioritaet, nicht
  das Ergebnis: diese Kandidaten bleiben ergebnisoffene
  Evaluationskandidaten, solange ihre technische Nutzbarkeit mit dem
  fixierten ESP-IDF-6.0.2-Produktionspfad nachweisbar ist oder ein
  dokumentierter Integrationsweg ohne stilles Wiedereinfuehren eines
  Arduino-Produktionspfads besteht; ein Kandidat, der Arduino als produktive
  ESP-IDF-Komponente benoetigt, ist deshalb keine automatische Ablehnung,
  sondern eine separate, vor Auswahl ownerpflichtige Architekturabweichung.
  Die konkrete Auswahl eines einzelnen Kandidaten bleibt weiterhin offen und
  wird von diesem Synchronisierungsauftrag nicht getroffen.

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
   `atanisoft/esp_lcd_touch_xpt2046`, `esp_lvgl_port`, `network_provisioning`
   und `protocomm` (als ESP-IDF-6.0.2-Bestandteil ohne eigene
   Registry-Version, siehe Tabelle oben).
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
11. `AGENTS.md` (Root) — **keine Aenderung** (durch Ownerklarstellung
    entschieden); die bestehende Bindung im Abschnitt "Verbindliche
    Software-Engineering-Grundsaetze" verweist bereits verbindlich auf
    `docs/ENGINEERING_PRINCIPLES.md` fuer die ausfuehrliche Erlaeuterung und
    deckt damit die neue Espressif-first-Regel ab. Kein Commit auf dieser
    Datei.
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
11. Live-Issue-Aktualisierungen #30/#31/#27 (`gh issue edit`, kein Commit)
12. Zwei neue Live-Issues `[E5.6]`/`[E5.7]` (`gh issue create`, kein Commit)

`AGENTS.md` erhaelt keinen eigenen Commit (siehe "Offene Entscheidungen"
Punkt 3: keine Aenderung erforderlich, kein leerer Bestaetigungscommit). Die
Nichtaenderung wird ausschliesslich in der PR-Beschreibung und der
Abschlussmatrix (`AGENTS_CHANGE_REQUIRED: NO`) dokumentiert.

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

Jeder unten neu genannte Kandidat erhaelt in Phase 2 zusaetzlich in den
betroffenen kanonischen Repositorydokumenten (`docs/THIRD_PARTY_COMPONENTS.md`,
`docs/audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md`,
`docs/audits/COMPONENT_EVALUATIONS.md`) einen dauerhaften Eintrag mit:
kanonischer Component-Registry-/ESP-IDF-Dokumentations-/Repositoryquelle,
konkret evaluierter Version bzw. ESP-IDF-Built-in-Zuordnung, Lizenz und
Notice-Pflicht, ESP-IDF-6.0.2-Kompatibilitaetsangabe und
Pruef-/Synchronisierungsdatum. Der Recherche-Agentenbericht dieser Sitzung ist
nur Arbeitsgrundlage fuer Phase 2, keine dauerhafte Repositoryquelle.

### Issue #30 – neuer Abschnitt "## Espressif-first-Kandidaten (Sync)"

```text
Primaerkandidat (Espressif-first, ESP-IDF 6.0.2 nativ):
- espressif/onewire_bus 1.1.1 (ESP-IDF >=5.0, Apache-2.0)
- espressif/ds18b20 0.4.0 (transitiv >=5.0, Apache-2.0)

Ergebnisoffener Evaluationskandidat (Evaluationsgate: direkter Build/Betrieb
mit ESP-IDF 6.0.2 oder dokumentierter Integrationsweg ohne stilles
Wiedereinfuehren eines Arduino-Produktionspfads; sonst ownerpflichtige
Architekturabweichung):
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
TFT_eSPI, LCDWiki) und gegen schlanke eigene Views vergleichen (gleiches
Evaluationsgate wie bei #30: ESP-IDF-6.0.2-Build/-Betrieb oder dokumentierter
Integrationsweg ohne Arduino-Produktionspfad); LVGL nicht automatisch
auswaehlen. Details im Audit (`docs/audits/COMPONENT_EVALUATIONS.md`,
`docs/audits/HARDWARE_SPIKE_PLAN.md`).
```

### Issue #27 – Ergaenzung im bestehenden Scope-Abschnitt

```text
Technischer HTTP-Primaerkandidat (Sync): ESP-IDF esp_http_server
(eingebaut, ESP-IDF 6.0.2). Arduino-ESP32 WebServer ist keine aktive
Produktionsrichtung mehr. ESPAsyncWebServer bleibt ergebnisoffener
konditionaler Rueckfallkandidat nach einem konkret belegten Problem
(gleiches Evaluationsgate wie bei #30/#31).

WLAN-Onboarding ist ausdruecklich nicht Bestandteil von #27 (siehe neues
Issue [E5.6]).
```

### Neues Issue [E5.6] – WLAN-Onboarding-Evaluation

```text
Titel: [E5.6] ESP-IDF-WLAN-Onboarding und Provisionierung evaluieren

## Status

`PLANNED_SPEC_PENDING`

## Epic

#7 – ESP32- und Hardwareintegration

## Abhaengigkeiten

Harte Grundlagen:
- #29 – ESP32-Bring-up, Partition und reale Ressourcen
- #57 – StorageEpoch, Reset- und Recoveryvertrag fuer den ersten realen
  Connectivity-/Credential-Konsumenten

Verwandt, aber getrennt (keine Sammelzustaendigkeit):
- #27 – Web-API/Weboberflaeche

## Scope

- browserbasierter SoftAP-/Captive-Portal-Vertrag
- drei gleichwertig zu messende technische Pfade:
  1. `espressif/network_provisioning` 1.2.4 (ESP-IDF >=5.1, Apache-2.0;
     Nachfolger des in der ESP-IDF-6.0-Linie entfernten `wifi_provisioning`)
     auf Basis von `protocomm` (offizielle ESP-IDF-6.0.2-Komponente ohne
     eigene Registry-Version, an die ESP-IDF-Version gebunden)
  2. direkter `protocomm`-/ESP-IDF-SoftAP-/HTTP-/DNS-Ansatz ohne
     `network_provisioning`
  3. kleiner eigener nativer ESP-IDF-SoftAP-/DNS-/HTTP-Adapter
- WiFiManager (Arduino-Bibliothek) bleibt zusaetzlicher konditionaler
  Drittanbieter-Evaluationskandidat (Evaluationsgate wie bei #30/#31/#27);
  ersetzt weder die offiziellen Espressif-Pflichtkandidaten (Pfad 1/2) noch
  den nativen Eigenbau-Gegenkandidaten (Pfad 3)
- ausdruecklicher Portalstart; individuelle SoftAP-Zugangsdaten; WLAN-QR;
  sichtbarer direkter IP-Rueckfall
- Credential-Kandidat, Validierung und atomarer Commit; Secret-Redaction
- Neustart, Fehler und Recovery
- Android-, iOS- und Windows-Clients
- Flash-/RAM-/Heap-/Stack-/Jittermessung
- Lizenz und Notices
- keine Cloud- oder Apppflicht fuer den gewaehlten R1-Pfad

Keine Auswahl vor identischem Spike und Ownerentscheid.
```

### Neues Issue [E5.7] – ESP-IDF-NVS-`IStateStore`-Adapter

```text
Titel: [E5.7] ESP-IDF-NVS-Adapter fuer IStateStore implementieren und
verifizieren

## Status

`PLANNED_SPEC_PENDING`

## Epic

#7 – ESP32- und Hardwareintegration

## Abhaengigkeiten

Harte Grundlagen:
- #29 – ESP32-Bring-up, Partition und reale Ressourcen
- #54 – IStateStore-, Schluessel-, Wire- und technischer Storevertrag

Verbraucher-/Kompatibilitaetsreferenz (kein Umsetzungsgrund fuer
laufpersistenzspezifische Anpassungen):
- gemergter PR #84 / abgeschlossenes Issue #17

## Scope

- produktiver Adapter von ESP-IDF NVS auf vorhandenes IStateStore
  (generischer Adapter fuer den bestehenden Vertrag, nicht
  laufpersistenzspezifisch)
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
```

`[E5.6]`/`[E5.7]` sind vom Owner vorgegebene Backlogpraefixe, keine
GitHub-Issue-Nummern; die tatsaechliche GitHub-Nummer wird erst nach
`gh issue create` in Phase 2 bekannt und danach in den betroffenen
Dokumenten nachgetragen — keine erfundene Nummer vor Zuteilung.

Epic-Bezug (#7), Titel-Praefixe (`[E5.6]`/`[E5.7]`) und harte Abhaengigkeiten
sind durch Ownerklarstellung entschieden (siehe Issue-Entwuerfe oben); Issue
#17/PR #84 dienen dabei ausdruecklich nur als Verbraucher-/
Kompatibilitaetsreferenz fuer `[E5.7]`, nicht als Grund fuer eine
laufpersistenzspezifische Anpassung des generischen Adapters.

## Daten-, Zustands- und Schnittstellenvertraege

Nicht betroffen. Diese Aufgabe aendert keinen Firmwarevertrag. Ausdruecklich
dokumentiert wird an mehreren Stellen:

```text
PR #84: MERGED
IStateStore-Vertrag: BESTEHEND
Schema-1-Wireformat: BESTEHEND UND UNVERAENDERT
Produktiver NVS-Adapter: SEPARATES FOLGEISSUE (neues Issue [E5.7] oben)
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
- Die Abschlussmatrix am Ende von Phase 3/4 verwendet ebenfalls
  `CONTEXT_REFRESH_MODE: INCREMENTAL`, nicht `FULL` — diese Aufgabe erstellt
  keinen neuen Gesamtaudit, sondern traegt eine bereits gemergte
  Toolchainentscheidung inkrementell in die betroffenen Dokumente nach.
- finaler Remote-CI-Lauf (`ESP-IDF + Native CI`) auf dem finalen Head muss vor
  dem vollstaendigen Ownerreview gruen sein. PR #88 bleibt dabei durchgehend
  Draft; "finaler Head vor dem Ownerreview" ist kein Ready-for-Review-Schritt.

## Dokumentationsaenderungen

Siehe "Betroffene Module und voraussichtlich betroffene Dateien" oben —
diese Aufgabe besteht vollstaendig aus Dokumentationsaenderungen.

## Offene Entscheidungen (durch diese Nachkorrektur entschieden)

1. **Rueckfallkandidatenregel (durch Ownerklarstellung entschieden, nicht
   mehr offen).** Root-`AGENTS.md` schliesst einen Arduino-Produktionspfad
   ausdruecklich aus. Espressif-first bestimmt jedoch nur die Recherche- und
   Pruefpriorisierung, nicht das Ergebnis:

   ```text
   Espressif-first bestimmt Recherche- und Pruefprioritaet, nicht das
   Ergebnis. Geeignete Drittkomponenten und bisherige Rueckfallkandidaten
   bleiben ergebnisoffene Evaluationskandidaten, sofern ihre technische
   Nutzbarkeit mit dem fixierten ESP-IDF-6.0.2-Produktionspfad nachweisbar
   ist.
   ```

   Fuer WiFiManager, DallasTemperature/OneWire, LovyanGFX, TFT_eSPI,
   ESPAsyncWebServer und aehnliche bisherige Rueckfallkandidaten gilt als
   erstes Evaluationsgate:

   ```text
   - direkter Build und Betrieb mit ESP-IDF 6.0.2 oder
   - klar dokumentierter Integrationsweg ohne stilles Wiedereinfuehren eines
     Arduino-Produktionspfads.
   ```

   Wuerde ein Kandidat Arduino als produktive ESP-IDF-Komponente benoetigen,
   ist das keine automatische Ablehnung, aber eine separate
   Architekturabweichung, die vor Auswahl einen ausdruecklichen
   Ownerentscheid benoetigt. Diese Aufgabe trifft weiterhin **keine**
   endgueltige Auswahl- oder Ausschlussentscheidung fuer einen konkreten
   Kandidaten — nur die Evaluationsgate-Regel selbst ist mit dieser
   Klarstellung entschieden. In Phase 2 erhaelt jede betroffene Kandidatenzeile
   (Display/Touch, DS18B20-Rueckfall, WLAN-Onboarding-WiFiManager, Webserver-
   ESPAsyncWebServer) einen kurzen Verweis auf dieses Evaluationsgate statt
   einer Ausschluss- oder `AUDIT_ONLY`-Abwertung.
2. **`protocomm` fachlich eingeordnet (nicht mehr offen).** `protocomm` ist
   keine fehlende oder unklare Komponente, sondern eine offizielle
   ESP-IDF-Komponente der fixierten ESP-IDF-Version 6.0.2 ohne eigene
   Component-Registry-Version (ihre Version ist an ESP-IDF 6.0.2 gebunden,
   daher kein 404 im Sinne einer fehlenden Quelle, sondern eine andere
   Vertriebsform als ein `espressif/*`-Registrypaket). `network_provisioning`
   nutzt `protocomm` fuer sichere Sessions; `protocomm` kann zusaetzlich
   direkt fuer einen eigenen SoftAP-/HTTP-/DNS-Provisionierungsweg evaluiert
   werden. Das neue WLAN-Onboarding-Issue `[E5.6]` nennt deshalb drei
   gleichwertig messbare Pfade (siehe Issue-Entwurf unten), nicht nur
   `network_provisioning` allein.
3. **AGENTS.md-Aenderung (durch Ownerklarstellung entschieden, nicht mehr
   offen): keine Aenderung erforderlich.** Root-`AGENTS.md` bindet
   `docs/ENGINEERING_PRINCIPLES.md` bereits ausdruecklich und verbindlich ein;
   diese Bindung deckt die neue Espressif-first-Regel ab. `AGENTS.md` bleibt
   in Phase 2 unveraendert; es gibt dafuer keinen eigenen Commit (siehe
   "Geplanter kleiner PR-/Commit-Schnitt").
4. **Epic-Zuordnung und Titel-Praefix der zwei neuen Issues (durch
   Ownerklarstellung entschieden, nicht mehr offen).** `[E5.6]`
   (WLAN-Onboarding) und `[E5.7]` (NVS-`IStateStore`-Adapter), beide Epic #7,
   mit den in den Issue-Entwuerfen oben genannten harten Abhaengigkeiten.
5. **Umgang mit dem urspruenglichen Auditdatum/-Basis-Commit (durch
   Ownerklarstellung bestaetigt).** Urspruengliches Auditdatum und
   urspruenglicher Basis-Commit bleiben erhalten; zusaetzlich werden
   Synchronisierungsdatum und neue `main`-Baseline ergaenzt (siehe "Geplanter
   kleiner PR-/Commit-Schnitt"). Keine historische Grundlage wird still
   ueberschrieben.

Damit bestehen aus dieser Nachkorrektur heraus keine offenen Punkte mehr; alle
fuenf vorherigen Punkte sind durch die Ownerklarstellung vom Owner selbst
entschieden. Verbleibend offen bleibt ausschliesslich die in Punkt 1
beschriebene *konkrete* Kandidatenauswahl je Funktionsbereich (z. B. welcher
DS18B20- oder Display-Stack am Ende gewaehlt wird) — das ist keine
Planungsluecke, sondern der beabsichtigte, im Auftrag selbst mehrfach
betonte Spike- und Ownerentscheid-Vorbehalt fuer jede einzelne
Produktivauswahl.

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
- keine Entscheidung, welcher konkrete Rueckfallkandidat (WiFiManager,
  DallasTemperature/OneWire, LovyanGFX, TFT_eSPI, ESPAsyncWebServer o. ae.)
  am Ende gewaehlt wird — nur die Evaluationsgate-Regel selbst ist entschieden
  (siehe "Offene Entscheidungen" Punkt 1);
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
