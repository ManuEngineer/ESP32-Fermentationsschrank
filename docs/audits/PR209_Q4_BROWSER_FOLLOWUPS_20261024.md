# PR #209 – Browser-Follow-ups B-1/B-2: Fix-Evidence und Rest-Gate-Übersicht

Auftrag: `PR209_Browser_Followups_Abschlussauftrag.md` (Basis `e95f3735359268c3915cca8a8728553f5c539d45`). Produktcommit
`a00bd2564011d69c204f964a3d4481ef56baf9be`. **Kein Flash, Boot oder Hardwaremessung** in diesem Delta; B-3 (Sessionlimit nach
nicht abgemeldeten Tests) wird nicht als Produktfehler behandelt.

## 1. Produktdelta (nur `lib/fermentation_app/src/web_application_asset.hpp`)
| Befund | Änderung im öffentlichen JS-Asset |
|---|---|
| **B-1** falsches Webpasswort blieb ohne Rückmeldung (`if(r.ok)location.reload();`) | Der Login-Handler fängt Netzwerk-/Requestfehler ab (kein unhandled rejection) und schreibt bei jedem Misserfolg in die vorhandene Statusanzeige `#note`: **401/sonstiger Fehlerstatus** → `T.pl + ' · ' + T.rejected` („Passwort · Abgelehnt“ / „Password · Rejected“ / „Contrasena · Rechazado“), **503 (`session-unavailable`)** → `T.busy` („Belegt“/„Busy“/„Ocupado“), **Netzwerkfehler** → `T.failed` („Fehler“/„Failed“/„Fallo“). Erfolg lädt wie bisher neu. Passwort/Cookie/CSRF erscheinen nirgends in der Ausgabe. |
| **B-2** leerer Gerätename → stilles 400 + Reload auf den alten Wert | Clientseitige Abweisung **vor** dem POST in `sb.onclick`: `T.name + ' · ' + T.rejected` („Geraetename · Abgelehnt“ …), keine Bestätigungsstufe, kein POST, kein Reload. Bestehende zweistufige Bestätigung, Revisions-/CSRF-/Sequenzschutz, gültige Namensänderung und serverseitige Validierung unverändert. |

Texte ausschließlich über vorhandene Tabellen-Einträge (`pl`, `rejected`, `busy`, `failed`, `name`) und `textContent` (keine
HTML-Injektion); keine neue Fehler-/State-Architektur. Backend, Passwortprüfung, Lockout, Sessionlimits, Cookies, API-Antworten,
Stack/Partitionen/Persistenz unverändert.

## 2. Bytebudget (JS-Asset, `kMaximumWebAssetBytes` 16 384 B, C5-Abschlussreserve ≥ 1 024 B → Asset ≤ 15 360 B)
| Stand | Asset | Reserve bis 16 384 | Reserve über Mindestboden (15 360) |
|---|---|---|---|
| Ausgang (`e95f373`) | 15 311 B | 1 073 B | 49 B |
| mit B-1/B-2 ohne Einsparung (Zwischenwert) | 15 435 B | 949 B | −75 B (nicht zulässig) |
| **final** | **15 330 B** | **1 054 B** | **30 B** |

Das Delta B-1/B-2 kostete +124 B; ausgeglichen durch zwei eng lokale Einsparungen im vorhandenen Asset, **ohne** Minifier,
Bibliothek, Refactoring oder Grenzänderung: (1) `T.unavailable` (15 Verwendungen) → lokaler Alias `un()` (−108 B; die
Tabelle und `T.unavailable` bleiben, die Tests lesen sie weiter); (2) sechs `catch(e){}` → `catch{}` (−18 B; das siebte in
`localStorage.lang` bleibt, weil ein bestehender Test den Wortlaut prüft). Das Budget wurde nicht gesenkt; der Mindestboden ist
eingehalten, aber knapp (30 B).

## 3. Verifikation (fokussiert, keine H-RAM-2-Wiederholung)
| Prüfung | Ergebnis |
|---|---|
| `node scripts/test_web_shell_polling.mjs` (echtes Asset-JS in `vm`, Fixture) | **PASS, 95 Fälle** (vorher 89; +6: B-1 und B-2 je DE/EN/ES) |
| B-1: 401 → Passwort-Abgelehnt-Text, 503 → Belegt-Text, Netzwerkfehler → Fehler-Text, je ohne Reload, ohne `unhandledRejection`, Passwort nur im Request-Body; Sprachwechsel DE/EN/ES → nächster Fehler in der neuen Sprache; Erfolg → genau ein Reload, Text unverändert; kein POST auf `/internal/ui/run` | PASS |
| B-2: leerer Name → sichtbare Abweisung, 0 POSTs, kein Reload, keine Bestätigungsstufe; gültiger Name → „Sicher?“, ein POST mit `r:{u:'1'}`, Ergebnis „Erledigt · Hostname/WLAN…“; danach erneut leer → wieder abgewiesen, weiterhin nur ein POST | PASS (DE/EN/ES) |
| Gegenprobe: neue Tests gegen das **alte** Asset | schlagen fehl (AssertionError) – Tests sind wirksam |
| `pio test -e native -f test_web_application_routes` | **120/120 PASS** (Asset-Budget-/CRC-/Shell-Tests inklusive; Zeichenketten-Assertion zum Login-Handler auf den neuen Wortlaut angepasst) |
| Beide ESP-IDF-Profile (`build_esp_idf_profiles.py all`) | PASS |
| `clang-format --dry-run -Werror` (Asset, Native-Test) | PASS |
| clang-tidy / Pre-Ready-Gate | **nicht ausgeführt** (geänderte Datei besteht im Wesentlichen aus einem Rohstring-Literal; Pre-Ready ohne Ownerauftrag nicht zulässig) |

**Vorbestehender, hier korrigierter Testfehler:** `scripts/test_web_shell_polling.mjs` („C5 closed Application dispatch
architecture“) schlug bereits auf `e95f373` fehl (erwartete `confirmPrepared`, der Handler ruft seit Q4-Option-A
`confirmPreparedInPlace`) und verhinderte damit den Lauf der folgenden Fälle. Nur die Soll-Liste wurde angepasst
(`confirmPreparedInPlace`); kein Produktcode. Ohne diese Korrektur wären die letzten Fälle des Skripts nie gelaufen.

**Grenzen:** Kein Gerät, kein echter Browserlauf mit dem neuen Asset (die Verifikation ist die Asset-JS-/DOM-Regression mit
Fixture-Fetch); die reale Darstellung im Chromium auf dem Gerät ist bis zu einer separaten Ausführungsfreigabe `NOT_RUN`. Eine
Sprachumschaltung übersetzt eine bereits angezeigte Meldung nicht nachträglich (bestehendes Verhalten aller `#note`-Texte); die
nächste Meldung erscheint in der neuen Sprache.

## 4. Rest-Gate-Übersicht für den Owner (aus bestehender Evidence, keine neuen Tests, keine Umdeklaration)
Einstufungen unverändert aus Plan §15, [H-RAM-2](PR209_Q4_HRAM2_RESULTS_20261024.md) und
[8 704-B-Evidence](PR209_Q4_HTTPD_8704_HW_20261024.md). „Vorschlag“ ist eine faktenbasierte Einordnung; **die Entscheidung
über offene Gates liegt beim Owner und beim anschließenden unabhängigen Review.**

| Bedingung | Stand | Beleg | Einordnung (Vorschlag) |
|---|---|---|---|
| Asset ≤ 16 384 B, Abschlussreserve ≥ 1 024 B | **erfüllt** (1 054 B) | §2, native Budgettest | – |
| B-1/B-2 Browser-Feedback | **behoben** (Fixture-Regression PASS), Fix Verification offen | §3 | Review-Punkt |
| HTTPD-Stack 8 704 B, 512-B-Planungsboden | rechnerisch erfüllt (Rest 720 B; gemessen Rest 2 236 B auf erreichten Pfaden) | Plan §15.10, 8 704-B-Evidence | – |
| `HTTPD_FULL_CHAIN_STACK` | **NOT_PROVEN** | zwei virtuelle Aufrufe unaufgelöst; tiefster Run-/Run-Store-Pfad nie gemessen | Run-Pfad: spätere Run-Qualifikation (`G-RUN`, #30/#35); unaufgelöste Aufrufe: Modell-Restpunkt für Review |
| `HTTPD_STACK_RESERVE` | **NOT_PROVEN** | wie oben | wie oben |
| `ESP32_HEAP_PEAK` / `ESP32_LARGEST_FREE_BLOCK` | **PARTIAL** | Normalbetrieb gemessen (min. 22 428 B; Browserphase 17 104 B / Block 18 432 B ohne Vergleichsbasis); Run-Store/Checkpoint/Maximalkatalog `NOT_RUN`; Host-Spitze nach M-H1 45 094 → 14 937 B | Normalbetrieb: Merge-relevant (Ownerentscheid zur Browserphase-Reserve); Run-/Maximalkatalogpfade: spätere Qualifikation |
| `TARGET_OOM_BEHAVIOR` | **FAIL** (vorbestehend: Abort + Neustart) | Plan §15.2 | bestehende Ownerentscheidung, nicht Teil von H-RAM-2 |
| D9 (aktorfreie Browser-/Gerätemessung vor Ready) | **PARTIAL** | desktop-Chromium: Login, Navigation, Programme, Meldungen, Einstellungen, DE/EN/ES, Heap/HWM gemessen; **nicht** gelaufen: Mobil, Cache-Treffer, Versionswechsel, Asset-Ausfall; Messung auf Diagnoseimage, nicht auf Produktimage; B-1/B-2-Fix noch nie auf dem Gerät | Rest-D9 laut Plan §9 vor Ready; braucht gesonderte Freigabe |
| Kontrollboot der wiederhergestellten Original-Firmware | **NOT_RUN** | Restore nur hash-gleich P0 | Ownerentscheid |
| `C5_RESOURCE_GATE`, `PR209_MERGE` | **HOLD** | – | durch Review/Owner |
| `ACTUATOR_RELEASE` | **NO** | – | unverändert |

Nicht in diesem Auftrag geprüft und im Plan weiterhin als offen geführt: `G2_WEB`, `R1-U`.
