# PR #211 – genehmigte K1-/R1-R2-Vertragsbereinigung: Umsetzungsnachweis

Stand: 2026-10-26. `ACTUATOR_RELEASE=NO`.
**DOCUMENT_PATCH=APPLIED_EXACTLY** · **ISSUE_SYNC=VERIFIED** ·
**INDEPENDENT_IMPLEMENTATION_DIFF_REVIEW=PENDING**.

## Freigabe und Provenienz

- Owner hat in der direkten Anweisung dieser Sitzung Plan Revision 2
  `346585a1c51814af7d7a41bef7d42d8a54c79822` und **O-R3/K1=B ausdruecklich
  freigegeben** sowie Dokument- und bestehende Issueaenderungen beauftragt.
  Keine neue Feature-/Hardware-/Ready-/Mergefreigabe.
- [Independent Plan Review](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/211#issuecomment-6413889024):
  `PLAN_REVIEW=PASS/GO`, `OPEN_BLOCKERS=0` auf exakt dieser Plan-SHA.
  Das ist kein Review des danach angewendeten Dokument-/Issuediffs.
- [Freigegebener vollstaendiger Plan](../tasks/issue-19-k1-journal-r1-boundary-plan.md)
  und [freigegebener Patch](../tasks/issue-19-k1-r1-r2-proposed-contracts.patch)
  bleiben unveraendert. Patch SHA-256
  `be82b3cdeb5e6bba63367ac361a4f96f268f175526de43d33bac04c30a38e6ac`.
- `CONTEXT_BASELINE_BRANCH=origin/main`, Baseline
  `adfbb34b60c290206660ac338af833966a3f8e7c` (nach #210, enthaelt #209);
  Fortsetzung auf `346585a1c51814af7d7a41bef7d42d8a54c79822`, unveraenderte
  Quellen/Regeln inkrementell wiederverwendet. GitHub/main/PR-HEAD und
  kanonischer Handover vor der Anwendung live gelesen.
- Dokumentcommit
  [`4cf4a95dbdbbe27f78110beebe9ed06d128a010d`](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/commit/4cf4a95dbdbbe27f78110beebe9ed06d128a010d):
  18 Fach-/Scope-Dokumente **bytegleich** zum genehmigten Patch, separat
  Roadmap auf den genehmigten Ist-Scope nachgefuehrt. Gegenprobe durch
  Anwendung desselben Patches auf eine isolierte Kopie der exakten Baseline.
  Der neue Scope ist im Draft-PR, noch nicht auf main gemergt.

## Umgesetzte Grenze und erhaltene Pflichten

[SPECIFICATION_REVIEW](../SPECIFICATION_REVIEW.md#r1r2-zuschnitt-nach-yagni)
ist die Release-Scope-SSOT; [FUTURE_SCOPE](../FUTURE_SCOPE.md) bleibt eine
nicht normative Referenz. Verschobene Funktionen bleiben in bestehenden
Issues sichtbar, ohne R2-Lieferzusage oder technischen Vorbau. Kein neues
Tracking-Issue, Label, Meilenstein oder zweiter Plan/zweite Roadmap.

Webeditor und zusaetzliche Web-Manuelloberflaeche, volle Webdiagnose/Detail-APIs,
Journal, Historie/Bereinigung/Archiv, Charts, alle drei Produkt-Exportklassen
sowie portables Backup/Import sind R2 vorgemerkt. Zusatz-Boot-Dashboard und
gefuehrte Service-UI/Mocksequenz sind nur Kandidaten bei konkretem Nutzen;
Favicon ist Future bei Bedarf. B0/O-BI ist kein R1-Gate mehr; historisches 5/50
bleibt Referenz zur erneuten Bewertung. Lizenz #177 bleibt echtes Gate vor
offizieller R1-Distribution, keine Lizenzentscheidung in diesem PR.

Erhalten sind alle bestehenden sicheren Funktionen, lokalen Editoren und
Bedienpfade, vorhandene Webcommands, Auth/CSRF/Replay, Sessions/Sprachen,
atomare Config-/Run-Gesamttransaktionen, Write-before-Apply, aktuelle Fault- und
Interlock-Wahrheit, SAFE_BOOT, Boot all-off, Watchdog und positive Clear-Regeln.
Echte Sensor-/Aktor-/Peltier-/Thermik-/Powercut-/Recovery-/Ressourcen- und
Siebentagegates werden nicht reduziert. Externe versionierte Testnachweise
bleiben erforderlich; sie verlangen keinen neuen Produkt-Serviceberichtsexport.

**Planreview-FOLLOW-UP bleibt offen:** Der tatsaechliche autorisierte, minimal
notwendige geschuetzte reale R1-Testpfad fuer erste Peltier-/Servicepulse muss
bei #33/#36-Commissioning vorhanden sein. [ACCEPTANCE_TESTS Gate 2B](../ACCEPTANCE_TESTS.md#gate-2b-erster-realer-peltier-puls)
und bestehende Schutz-/Bring-up-Vertraege bleiben unveraendert. Kein Nachweis
dieses realen Pfades und keine neue Komfort-Serviceplattform durch diesen PR.
Der Folgepunkt ist in der Roadmap bei #36/#37 sichtbar, kein neuer R1-Safety-Owner.

Nur der bekannte `GET /favicon.ico` mit `404` wird fuer kuenftige Browserabnahmen
separat als optionale Nebenressource eingeordnet. Unbekannte URLs duerfen 404
liefern; unerwartete Fehler benoetigter Shell/Login, Status, Assets und Commands
bleiben Befunde. Historische Stopps und FAIL/PARTIAL/NOT_RUN bleiben unveraendert.
Runner-/Testcode und der bestehende #27-D9-Cache-FOLLOW-UP werden nicht veraendert.

## Verifizierter Issue-Schritt

Vor jeder Mutation wurde der aktuelle Body erneut gelesen und gegen den
vorbereiteten Ausgangsbody verglichen. Nach jeder Mutation wurde der Body
erneut ueber den GitHub-Connector gelesen und auf exakte Gleichheit geprueft.
Titel, OPEN-Status und Labels blieben erhalten. #19/#27/#28 wurden nicht DONE.
Die bisherigen vollstaendigen Bodies stehen bei #19/#27/#28/#37/#204 in einem
als historisch gekennzeichneten Detailblock bytegleich weiterhin im Issue;
Kommentare und historische PRs/Plaene wurden nicht geaendert.

| Issue | Tatsaechliches Delta | Nachlesen (UTC) | Ergebnis |
|---|---|---|---|
| [#19](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/19) | Aktueller K1=B-/R1-/R2-Block; Werksresetbestand erhalten, Backup/Import und 5/50 neu eingeordnet | 2026-10-26T11:21:20Z | PASS: Body exakt nachgelesen, OPEN |
| [#27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27) | R1-Webbestand/Evidencegrenzen erhalten, Webeditor und zusaetzliche Manuelloberflaeche R2 | 2026-10-26T11:21:22Z | PASS: Body exakt nachgelesen, OPEN |
| [#28](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/28) | D1 MERGED; Webdiagnose/Charts/Exporte R2, Boot-/Service-Zusatz nur Kandidaten; #19-Abhaengigkeit praezisiert | 2026-10-26T11:21:25Z | PASS: Body exakt nachgelesen, OPEN |
| [#36](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/36) | Ausschliesslich die #28-Abhaengigkeitszeile eingegrenzt; uebrige Hardware-/Abnahmekriterien bytegleich | 2026-10-26T11:21:27Z | PASS: Body exakt nachgelesen, OPEN |
| [#37](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/37) | Vorhandene verbindliche R1-Last statt Produkt-Archiv-/Exportpflicht; sieben Tage und alle realen Gates erhalten | 2026-10-26T11:21:29Z | PASS: Body exakt nachgelesen, OPEN |
| [#204](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/204) | Future bei Bedarf und enges kuenftiges 404-Orakel; kein Faviconbau | 2026-10-26T11:21:32Z | PASS: Body exakt nachgelesen, OPEN |
| [#177](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/177) | Bereits konsistentes Lizenz-/Distributions-Gate | unveraendert nachgelesen | NO_ACTION: kein Body-Write |

Body-SHA-256 (UTF-8, exakte Connector-Texte; kein Screenshot-/Hardwarebeweis):

| Issue | Vorher | Nachher |
|---|---|---|
| #19 | `f0938856569f6dfb5e3ab8b880bbf31e15a0e831f3b495188427017c5efcf1e0` | `d5e6bb057db1439b2eda884df763dd656569e3c36095152b50c8be950731014f` |
| #27 | `9cf948d7f2e3b50cbec0c4a53ece8af668f382e8702a35f67ec0763da243cd27` | `6bd6e8fe24e0a406b0fde36756cf432431763ce5d9f018c60d5bcdc6ac51edfb` |
| #28 | `f3a5b373bd1b2f742fd98de81b19e50b60e2fdb25c91e184894fa692d83a55a3` | `b0b020a6d27c80049bbcaa310aa28eaaa68f511a6d0e565d3e2acdad805a4a8e` |
| #36 | `6e5509bee86f633a3f6722761cf5fd434d0e1a827af9fe50343a355f89b74b5d` | `519a6d66edf8bac5e52b1809e8f107353e4cc15e25177be2035bd5f905040b00` |
| #37 | `48669573516224c2f1d343632050ede520e0fcb93f96af9df5a08289970ab828` | `bcbd32b23bc9fe46fc9eaf5fd7a7722122cf92729d0cc0c6fb259ef634127372` |
| #204 | `cbf252f7470324c31e7eb3d6eab345b8781551d9d438f4be726a05244552a921` | `e48a00dda1460110a3cb4d397b8b3c87298a792b671cb0be49cf41799f464317` |
| #177 | `218be40c9d2175745a5569ea771d9b2edc389e471fe6f3f8d50e3d68a481971a` | `218be40c9d2175745a5569ea771d9b2edc389e471fe6f3f8d50e3d68a481971a` (unveraendert) |

Die SHA-Werte binden den Remote-Diff an diese Umsetzungssitzung. Spaetere
Issueaenderungen muessen neu gelesen werden; kein ungeprueftes Ueberschreiben
auf Basis dieser Momentaufnahme.

## Validierung und naechstes Gate

- Patchidentitaet und 18 erwartete Ergebnisdateien gegen freigegebenen Patch:
  **PASS**; `git apply --reverse --check` bestaetigt exakte Anwendung.
- **PASS:** 55 relative Dokumentlinks einschliesslich Anker; alle 208
  bestehenden SIM-/HW-Referenzzeilen erhalten. Konkreter Peltier-Gate-2B-Text
  bytegleich; Siebentage-/kritische Persistenz-/Ressourcengates erhalten.
  Gesamter PR ohne Produkt-/Test-/Runner-/Build-/CI-Diff.
- **PASS:** `git diff --check`; finaler HEAD im PR-Handover gebunden.
- Kanonischer Builder-Self-Check wird auf dem finalen HEAD ueber den bestehenden
  Runner ausgefuehrt. Sein verifizierter finaler Status steht im PR/Handover;
  dieses Dokument erzeugt keinen vorweggenommenen Gate-PASS.
- [K1-Hostevidence](ISSUE19_K1_JOURNAL_R1_EVIDENCE_20261026.md): bestehende
  487/487 PASS ausschliesslich auf unveraenderter Produktbaseline `adfbb34`;
  keine neue Ausfuehrung oder Hardware-/Durabilityqualifikation durch Normtexte.
- **NOT_RUN:** neue Fach-/Gesamttests, Profilbuilds, Browser-/Hardwaretests,
  Powercuts, Erase/Flash, Aktor-/Peltierpruefungen, Siebentagetest und Pre-Ready.
  Historische Audits/Testergebnisse und alle Produkt-/Test-/Runnerdateien bleiben
  unveraendert. Reale Safety-/Hardware-/Ressourcenqualifikation bleibt separat
  **NOT_PROVEN**, soweit nicht durch vorhandene konkrete Evidence belegt.

**Uebergabe:** Vollstaendiger aktueller PR-/Issue-Diff an den unabhaengigen
Reviewer. Planreview-PASS gilt nicht als Implementierungsreview-PASS;
`INDEPENDENT_IMPLEMENTATION_DIFF_REVIEW=PENDING`. Kein Ready-, Merge-,
Auto-Merge- oder Pre-Ready-Auftrag durch diese Uebergabe.
