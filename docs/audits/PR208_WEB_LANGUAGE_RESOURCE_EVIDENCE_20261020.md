# PR #208 / Issue #27 – Browserindividuelle Sprachwahl, Offline-Ressourcenevidence, 2026-10-20

Plan [`a671c33`](../tasks/issue-27-r1-web-language-plan.md) (Ownerfreigabe der exakten SHA;
D1 = A, Limit 11 456 B, D2 `localStorage`, D3 Auswahl am Seitenanfang). Offline-Messquelle:
sauberer Implementierungscommit `d37b375` (`build_esp_idf_profiles.py all
--require-clean-source-tree`: `esp32_bringup` und `esp32_release` PASS, Profil-/Driftpruefung
PASS). Vergleichsbasis: `main` `43f0338` (Code identisch zu `f09d937`, dem Messstand der
[PR-207-Evidence](PR207_STANDBY_INFO_RESOURCE_EVIDENCE_20261020.md); seit `f09d937` nur
Dokumente). Messverfahren fuer die Budgetwahl:
[`WEB_LANGUAGE_BUDGET_20261020/`](WEB_LANGUAGE_BUDGET_20261020/).

`RAM_PASS=NOT_PROVEN`; Ergebnis hoechstens `RAM_OFFLINE_ESTIMATED`. Keine Hardware-,
Browser-, Flash-, UART- oder Reset-Aktion. `ACTUATOR_RELEASE=NO`.

## Shell-Budget (MEASURED, native `test_web_application_routes`, 109/109 PASS)

Authentifiziert **11 312 B**, Passwortschutz aus 11 311 B; Limit **11 456 B**; Reserve
144 B = 128 B Normalreserve (inklusive 19 B Sequenzaufschlag) + 16 B. Das entspricht exakt
dem Messkandidaten des Plans (A2 / Endonyme, +296 B gegenueber 11 016 B; Suffix 9 797 B,
Kopf +118 B). Kopf mit maximaler Sequenz 1 534 B (Testlimit 1 536 B, unveraendert). Die
gewaehlte Grenze wurde nicht ueberschritten; `kMaximumWebShellBytes` ist (neben Golden und
Test-`kShellLimit`) die einzige Limitaenderung. Setup-Shell: 3 886 B (Kopf 1 029 B,
Suffix 2 857 B; vorher 3 561 B), weit unter jeder Grenze.

## Sektionsdeltas (MEASURED, `esp32_release`, `xtensa-esp32-elf-size -A`)

| Sektion | Basis `43f0338` | Implementierung | Delta | Grund |
|---|---:|---:|---:|---|
| `.dram0.data` | 18 975 | 18 975 | **0** | keine neuen statischen Objekte |
| `.dram0.bss` | 74 488 | 74 488 | **0** | dito |
| `.iram0.text` | 100 815 | 100 815 | **0** | kein neuer Code |
| `.flash.rodata` | 237 212 | 237 836 | +624 | Shell-Suffix +178 B, Kopf-Literal +118 B, Setup-Shell +325 B (Literale, Ausrichtung) |
| `.flash.text` | 1 405 758 | 1 405 758 | **0** | keine neue C++-Logik |
| `.bin` | 1 764 224 | 1 764 848 | +624 | Summe, rund 57 % der 3 080 192-B-Partition |

## Statische Stack-Frames (MEASURED, GCC `-fstack-usage`, `esp32_release`)

`WebReadOnlyApiHandler::handle` 624 B (unveraendert), `WebRouteDispatcher::handleShell`
352 B (unveraendert), `provisioningShell` 48 B (unveraendert). Kein Codec-, Handler-,
Application- oder Display-Pfad wurde geaendert; die HTTPD-Kette und die +96-B-Stoppregel
des Vorplans sind nicht betroffen. Heap: nur die +118 B im vorhandenen Kopf-String je
Shell-Abruf (transient, unter `reserve(1536)`). Statische Frame-Rechnung, kein
Laufzeitnachweis; HWM bleibt **NOT_PROVEN**.

## Weitere Nachweise (Builder-Self-Check)

- JS: `scripts/test_web_shell_polling.mjs` **66/66 PASS** (bisherige 56 plus 10 neue
  Sprachfaelle, Fake-DOM, kein Browser). Mutationsgegenproben schlagen fehl:
  ungeschuetztes Schreiben (`ReferenceError`), alte `ui()`-Regel (Bestaetigungstext in alter
  Sprache), fehlende Validierung in `sl()`.
- Native: `test_web_application_routes` 109/109, `test_fermentation_ui_models`,
  `test_renderer_boundary`, `test_ui_steady_state_allocations`, `test_run_detail_projection`,
  `test_device_ui_contracts`, `test_web_session`, `test_web_browser_policy` (gesamt 279/279);
  `check_architecture_boundaries.py` PASS.
- Eingefrorener Generator (`shell_golden.hpp`) stimmt byteweise mit Dashboard- und
  Setup-Shell ueberein.

## Nicht bewiesen

Browser-/Geraetelauf (Auswahlverhalten in echten Browsern, assistive Technik, Verhalten bei
blockiertem Speicher), HTTPD-/Main-HWM, Heap-Spitzen auf dem Geraet, Mehrclient, Touch,
Langzeit; Produktimage nicht hardwarequalifiziert. `RAM_PASS=NOT_PROVEN`.
