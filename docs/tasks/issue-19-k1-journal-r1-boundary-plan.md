# Issue #19 / K1 – konsolidierter R1/R2-YAGNI-Aenderungsplan

**Revision 2 · 2026-10-26 · PLAN_ONLY / AWAITING_INDEPENDENT_PLAN_REVIEW_AND_OWNER_APPROVAL**

Ersetzt Revision 1 (`1483cdbf0e169719d4bcd259dd9d962f707f0535`) desselben Plans
vollstaendig; diese alte SHA war nicht freigegeben. Genau dieser Markdown-Plan
ist die ausfuehrbare Planfassung. Der beigefuegte Patch ist sein konkreter
Normtextvorschlag, kein zweiter Plan und **nicht angewendet**.
`ACTUATOR_RELEASE=NO` · `PRODUCT_IMPLEMENTATION=NO` · `NORMATIVE_APPLY=NOT_AUTHORIZED`.

## 1. K1-Nachweis zuerst und unveraenderte Grenzen

[K1-Evidence-Matrix](../audits/ISSUE19_K1_JOURNAL_R1_EVIDENCE_20261026.md) liegt
auf unveraendertem `main` nach #210 vor: kein journalabhaengiger Start-/Safety-/
Reset-/Recoverypfad und kein durch das fehlende Journal fehlender notwendiger
Config-/Run-Durability-Schutz gefunden; **487/487 gezielte Hosttests PASS**.
Diese Ausfuehrung stammt aus Revision 1, wird nicht neu datiert und beweist
weder reale Abschaltung noch Hardwarequalifikation. Produktive Gesamtkette
`FermentationApplication -> Interlock -> Planner -> reale Ausgaenge` weiter
nicht durchgaengig angebunden; Hardwarequalifikation **NOT_PROVEN**.

**K1 ist weiter offen/unerfuellt.** Empfehlung **O-R3/K1=B** bleibt gesondert:

> R1 verlangt kein persistentes Ereignisjournal als Start- oder Safety-Gate. Zulässige Starts und Recovery richten sich ausschliesslich nach den bestehenden vertrauenswürdigen Config-/Run-/Transaktionszuständen und den erforderlichen aktuellen Sensor-/Planner-/Safetyfreigaben. Kann eine notwendige kritische Persistenz oder diese Evidenz nicht sichergestellt werden, bleibt der bestehende fail-closed-Pfad verbindlich. Journal/Historie werden nicht als vorhanden oder zuverlässig behauptet.

Bewusster Nachteil: keine vollstaendige Fehler-/Brownout-/Watchdog-Chronik
ueber mehrere Neustarts. Aktuelle Resetursache, Faultanzeige und vorhandene
Run-/Recoverydaten ersetzen kein Archiv. `IEventJournal` bleibt als vorhandener
Port erhalten; weder Mockrecord noch `criticalSafetyEventPending` sind ein
persistenter Journaldienst oder Durability-Beweis.

Fail-closed, atomare Config-/Run-Gesamttransaktion, Write-before-Apply,
SAFE_BOOT, Boot all-off, frische Sensor-/Planner-/Safety-Evidenz, Watchdog und
positive Clear-Regeln bleiben verbindlich. Reale Sensor-/Aktor-/Peltier-/Thermik-,
Powercut-/Recovery- und Inbetriebnahmepflichten werden nicht reduziert.

## 2. Live-Basis, Auftrag und Quellen

```text
CONTEXT_BASELINE_BRANCH: origin/main
CONTEXT_BASELINE_SHA: adfbb34b60c290206660ac338af833966a3f8e7c
CONTEXT_HEAD_SHA: 1483cdbf0e169719d4bcd259dd9d962f707f0535 (vor dieser Planrevision)
CONTEXT_PLAN_SHA: NONE (Revision 1 nicht freigegeben; Revision 2 neu vorzulegen)
CONTEXT_REFRESH_MODE: INCREMENTAL
CONTEXT_DELTA: keine neuen main-/Produktcommits; neuer R1/R2-Auftrag und dessen
  unmittelbar betroffene Release-/Web-/Diagnose-/Abnahme-/Phasenvertraege
SOURCE_OF_TRUTH_CONFLICT: noch geltende R1-Muss-Texte gegen Owner-YAGNI-Zuschnitt;
  konkrete K1-Normentscheidung und Planfreigabe stehen aus
```

Repository `ManuEngineer/ESP32-Fermentationsschrank`, Branch
`agent/issue-19-k1-journal-r1-plan`, bestehender **Draft-PR #211**, genau dieser
Worktree: `/var/lib/docker/data/ESP32-Projekte/_worktrees/issue19-k1-journal-r1`.
Remote-main, PR-HEAD/Draftstatus und kanonischer Handover `6413356871` live
geprueft; vor dieser Revision sauber. Kein Rebase, keine gestapelte Basis.
Urspruenglicher Checkout und andere Worktrees bleiben erhalten.

**Auftrag:** Zusatzauftrag „R1/R2-YAGNI-Scope-Bereinigung“ und direkte
Owneranweisung dieser Sitzung: langfristigen Scope bewahren, zuerst konkreten
Aenderungsplan zur Freigabe vorlegen. Originalanlage SHA-256
`42031b2e288d41a65437bebc4a90b74617f50ca60c78795b7bd05ff18dfa5c80`.
Das akzeptiert die gewuenschte Scope-Richtung, erteilt aber weder die exakte
Planfreigabe noch den konkreten K1-Normentscheid oder eine Implementierung.

Live #19/#27/#28/#37/#204/#177 samt vorhandenen Kommentaren gelesen, alle OPEN.
Zusaetzlich #36 gelesen, weil dessen formale #28-Abhaengigkeit R2-Scope wieder
zum R1-Gate machen wuerde. #209/#210 sind gemergt (main enthaelt beide);
#27-D9 bleibt PARTIAL, C5 nur ACCEPTED_PARTIAL_FOR_PR209, OOM=FAIL und
unbewiesene/NOT_RUN-Pfade bleiben unveraendert. #210-D1-Evidence wird nicht
zu einer Sensor-/Aktor-/Ressourcenqualifikation erweitert.

Verbindliche Basis: Root-/lokale AGENTS, AGENT_WORKFLOW, ENGINEERING_PRINCIPLES,
CI_AND_QUALITY_GATES, Dokumentprioritaet in SPECIFICATION_REVIEW, relevante
akzeptierte ADRs im DECISIONS-Register (insbesondere 013/014/018/019).
Fachquellen sind die 18 konkret im Vorschlag betroffenen Dokumente sowie
LICENSE_STATUS, ROADMAP und die Live-Issues. #19-Plan Rev. 8 bleibt historische
Quelle fuer K1=A bedingt/K2–K7; #27-Restplan D6=a und #28-D1-Plan D-28-2=A
bleiben erhalten. Ihre alten Freigaben autorisieren diesen Normtextdiff nicht.

## 3. Kurze Gap-Matrix

Alle nachstehenden Dateilinks zeigen den tatsaechlichen alten R1-Vertrag auf
`main`, nicht bereits geaenderte Normtexte. Der neue Ownerauftrag ersetzt die
bisher bedingte R1-Priorisierung von Backup/Import und die 5/50-Pflichtannahme.

| Anforderung | Bisherige R1-Quelle | Vorgesehener R1/R2/Future-Status | Konkrete Zielstelle / Tracking |
|---|---|---|---|
| Kernbedienung / Bestand | [SPECIFICATION_REVIEW:77](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/SPECIFICATION_REVIEW.md#L77); [REQUIREMENTS:27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/REQUIREMENTS.md#L27) | R1 bleibt: vier Programme, lokaler Editor/Manualbetrieb, Touch, vorhandene Webslices/Start-Stop, DE/EN/ES; keine Implementierung entfernen | REQUIREMENTS / WEB_UI / PRODUCT_VISION; #27 bleibt offen soweit reale R1-Gates offen |
| Web-Programmeditor | [WEB_UI:114](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/WEB_UI.md#L114); #27-Plan D6=a, §2/§3 | R2_VORGEMERKT, kein neuer R1-Editor | SPECIFICATION_REVIEW / WEB_UI / REQUIREMENTS / FUTURE_SCOPE; #27 |
| Zusaetzliche Web-Manuelloberflaeche | [WEB_UI:66](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/WEB_UI.md#L66); [WEB_UI:108](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/WEB_UI.md#L108) | R2_VORGEMERKT; lokale Funktion/Commands und #34/#35-Laufgrenzen bleiben | WEB_UI / REQUIREMENTS / PRODUCT_VISION; #27 |
| Vollstaendige Webdiagnose / neue Detail-APIs | [REQUIREMENTS:192](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/REQUIREMENTS.md#L192); [DIAGNOSTICS_AND_MAINTENANCE:61](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/DIAGNOSTICS_AND_MAINTENANCE.md#L61) | R2_VORGEMERKT (D-28-2=A); neues Budget vor Aktivierung, D1/Webstatus bleiben | REQUIREMENTS / WEB_UI / DIAGNOSTICS_AND_MAINTENANCE / FUTURE_SCOPE; #28 |
| Zusaetzliches Boot-Dashboard/-modell | [DIAGNOSTICS_AND_MAINTENANCE:152](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/DIAGNOSTICS_AND_MAINTENANCE.md#L152); [FIRMWARE_UPDATE_AND_ROLLBACK:105](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/FIRMWARE_UPDATE_AND_ROLLBACK.md#L105) | R2_KANDIDAT bei Zusatznutzen; vorhandene Boot-/Config-/Run-/Sensor-/Planner-/Safety-Revalidierung bleibt R1 | DIAGNOSTICS_AND_MAINTENANCE / FIRMWARE_UPDATE_AND_ROLLBACK; #28 |
| Persistentes Ereignis-/Fehler-/Resetjournal | [RESOURCE_BUDGET_AND_MAINTENANCE:210](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/RESOURCE_BUDGET_AND_MAINTENANCE.md#L210); [SYSTEM_SAFETY_AND_RECOVERY:342](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/SYSTEM_SAFETY_AND_RECOVERY.md#L342) | R2_VORGEMERKT, rein diagnostisch; K1-Gate bleibt bis ausdruecklichem Normentscheid offen/unerfuellt | SYSTEM_SAFETY_AND_RECOVERY / RESOURCE_BUDGET / RUN_PERSISTENCE / SAFETY_AND_FAULTS / ARCHITECTURE; #19/#28 |
| Lauf-/Messhistorie, Bereinigung/Archiv, 5/50 | [BACKUP_SECURITY_RETENTION:172](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/BACKUP_SECURITY_RETENTION.md#L172); [RUN_PERSISTENCE:477](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/RUN_PERSISTENCE.md#L477) | R2_VORGEMERKT; 5/50 nur historische, erneut zu pruefende Referenz; kein R1-Budget | BACKUP_SECURITY_RETENTION / RUN_PERSISTENCE / RESOURCE_BUDGET / REQUIREMENTS / ARCHITECTURE; #19 |
| Aktueller Web-Laufchart / historische Diagramme | [WEB_UI:736](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/WEB_UI.md#L736); [WEB_UI:777](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/WEB_UI.md#L777) | R2_VORGEMERKT; keine erfundenen Verlaeufe und keine R1-Reserve | WEB_UI / SPECIFICATION_REVIEW / FUTURE_SCOPE; #28/#19 |
| JSON/CSV-Lauf-, Diagnose-, Serviceberichtsexporte | [DIAGNOSTICS_AND_MAINTENANCE:293](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/DIAGNOSTICS_AND_MAINTENANCE.md#L293); [REQUIREMENTS:195](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/REQUIREMENTS.md#L195) | R2_VORGEMERKT; Geheimnisschutz bleibt. Versionierte externe Testnachweise sind weiter Pflicht | DIAGNOSTICS / BACKUP_SECURITY_RETENTION / REQUIREMENTS / STATE_MACHINE / ACCEPTANCE_TESTS / IMPLEMENTATION_PLAN; #19/#28/#37 |
| Portables Backup / atomarer Import | [BACKUP_SECURITY_RETENTION:69](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/BACKUP_SECURITY_RETENTION.md#L69); alte #19-Priorisierung B0/O-BI | R2_VORGEMERKT; ersetzt bedingt R1-erwuenscht, B0/O-BI kein R1-Gate; neuer Ressourcen-/Security-/Schema-Plan spaeter | BACKUP_SECURITY_RETENTION / SPECIFICATION_REVIEW / RESOURCE_BUDGET / IMPLEMENTATION_PLAN / ROADMAP; #19 |
| Zusaetzliche Service-UI / Mock-Aktor-Sequenz | [IMPLEMENTATION_PLAN:94](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/IMPLEMENTATION_PLAN.md#L94); [DIAGNOSTICS_AND_MAINTENANCE:199](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/DIAGNOSTICS_AND_MAINTENANCE.md#L199) | R2_KANDIDAT bei Zusatznutzen; echte Hardware-Service-/Peltier-/Thermiktests sowie vorhandene PIN-/Resetpfade bleiben R1 | DIAGNOSTICS / WEB_UI / ARCHITECTURE / IMPLEMENTATION_PLAN; #28, unveraenderte Gates #32–#37 |
| Web-Favicon / bekannter 404 | [DEVICE_UI_ARCHITECTURE_DECISIONS:71](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/DEVICE_UI_ARCHITECTURE_DECISIONS.md#L71); #204 Einordnung/Stopregel | FUTURE_BEI_BEDARF; nur bekannter GET /favicon.ico=404 ist kuenftig kein funktionaler R1-Fehler | UI-05 / SPECIFICATION_REVIEW / FUTURE_SCOPE / ACCEPTANCE_TESTS; #204; alte Evidence unveraendert |
| Abnahme / versteckte Scope-Abhaengigkeiten | [ACCEPTANCE_TESTS:738](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/ACCEPTANCE_TESTS.md#L738); [IMPLEMENTATION_ISSUES:166](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/IMPLEMENTATION_ISSUES.md#L166); #36/#37 live | Sieben Tage und alle echten Safety-/Hardware-/Powercut-/Recovery-/Ressourcengates bleiben; kein Warten auf R2-Journal/Export | ACCEPTANCE_TESTS / IMPLEMENTATION_ISSUES / IMPLEMENTATION_PLAN / ROADMAP; #37, gezielte #36-Abhaengigkeitspraezisierung |
| Projektlizenz / Distribution | [LICENSE_STATUS:43](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/adfbb34b60c290206660ac338af833966a3f8e7c/docs/LICENSE_STATUS.md#L43); #177 live | R1-Release-/Distributions-Gate vor offizieller Weitergabe bleibt; keine Firmwarefunktion oder Lizenzwahl hier | SPECIFICATION_REVIEW / ROADMAP; #177-Body und LICENSE_STATUS bereits konsistent, kein Normdelta erforderlich |

`R2_VORGEMERKT` ist eine benannte spaetere Bewertung, **keine automatische
R2-Pflicht oder Implementierungsfreigabe**. `R2_KANDIDAT` verlangt konkreten
Zusatznutzen; `FUTURE_BEI_BEDARF` enthaelt keine Release-Zusage. Keine Ports,
NVS-Ringpuffer, Event-Sourcing-, Record-/Schema-/Flashaenderungen, Bibliotheken
oder vorsorglichen RAM-/Flashreserven. Sichere bestehende Features bleiben.
Lokale Pflichtdaten, sichere Commands, Auth/CSRF/Replay, Browser-/Session- und
Sprachvertraege werden nicht aus R1 entfernt; D1 allein beweist nicht die
vollstaendige Diagnose-/Hardwareabnahme.

## 4. Konkreter Normtextdiff – nur Vorschlag

[Pruefbarer Patch fuer 18 bestehende Dokumente](issue-19-k1-r1-r2-proposed-contracts.patch)
gegen `adfbb34b60c290206660ac338af833966a3f8e7c`, SHA-256
`be82b3cdeb5e6bba63367ac361a4f96f268f175526de43d33bac04c30a38e6ac`.
Der Patch ersetzt die neun eingebetteten Rev.-1-Diffs vollstaendig. Kein
historischer Plan oder Audit wird bearbeitet. Fuer spaetere Anwendung sind
**beide** Ownergates in Abschnitt 6 Voraussetzung, auch wenn einzelne Hunks
nur Releasezuordnungen betreffen. Nicht vorzeitig teilweise anwenden.

| Dokumentgruppe | Genau vorgesehenes Delta |
|---|---|
| SPECIFICATION_REVIEW | Ein knapper kanonischer R1/R2/Future-Abschnitt mit bestehenden Detail-/Issue-Ownern und Lizenzgate; keine zweite vollstaendige Produktspezifikation. |
| FUTURE_SCOPE | Nicht normative Referenztabelle aller verschobenen Funktionen, bestehende Issues; keine R2-SSOT und keine Lieferzusage. Vorhandener anderer Future-Scope unveraendert. |
| WEB_UI / REQUIREMENTS | Web-UI-Paritaetsannahme auf gemeinsame Fach-/Safetyvertraege begrenzen; neue Editor-/Manual-/Diagnose-/Service-/Systemseiten und Charts nicht als R1-Muss. Vorhandene sichere Slices und Smartphone/Tablet/Computer-Gleichwertigkeit bleiben. Nur betroffene angenommene Phase-5B-Pflichthaken in R2/Future-Text umstellen, keine historischen Testergebnisse. |
| DIAGNOSTICS_AND_MAINTENANCE | Vollstaendige Webdiagnose, Detail-APIs und drei Exportklassen als R2; Zusatz-Bootmodell und Service-UI/Mocksequenz nur Kandidaten. Bestehende Revalidierung, D1 und echte Hardwaretests bleiben. |
| BACKUP_SECURITY_RETENTION | Portables Backup/Import aus bedingt R1-erwuenscht nach R2; historische 5/50 als neu zu pruefende Referenz; aktuelle Run-/Recoverydaten/Reset unveraendert. |
| RESOURCE_BUDGET_AND_MAINTENANCE / SYSTEM_SAFETY_AND_RECOVERY / RUN_PERSISTENCE / SAFETY_AND_FAULTS / STATE_MACHINE | K1-Diff nur nach gesondertem B-Entscheid; Journalarchiv nicht Safety-Owner. Kritische Fehler-/Runrevisionen und aktuelle Anzeigen erhalten. Keine R1-Reserve, keine Pflicht zu Journal-/Historien-/Exportfunktion. |
| ACCEPTANCE_TESTS | R1 sieben Tage unter echter vorhandener Web-/Touch-/WLAN-/Regel-/Speicherlast und alle unveraenderten Hardware-/Safety-/Powercutgates; Heap/Stack/Flash-Reserven bleiben. Archiv/Exportlast entfällt als Produktpflicht. Versionierter externer Testnachweis bleibt trotz verschobenem Produkt-Serviceexport erforderlich. Exakte Favicon-Regel, keine breite HTTP-Fehlerausnahme. |
| PRODUCT_VISION / ARCHITECTURE / IMPLEMENTATION_PLAN / IMPLEMENTATION_ISSUES | Nur gefundene Rueckverweise praezisieren: langfristiger Webumfang bleibt sichtbar, fehlende Journaldurability nicht suggerieren, SW2/SW5/M2/C3 und Issue-/Meilenstein-Kanten duerfen verschobenen Scope nicht wieder R1-pflichtig machen. Reale Hardware- und Safetyreihenfolge unveraendert. |
| DEVICE_UI_ARCHITECTURE_DECISIONS (UI-05) | Favicon vom R1-Branding-Muss ausnehmen; vorhandene Brandingassets und Architektur erhalten, keine neue Infrastruktur. Konkrete Owner-Scopeausnahme, kein stiller Bruch des ADR-019-Verweises. |
| FIRMWARE_UPDATE_AND_ROLLBACK | Nur aktuelle R1-Export-/Boot-Dashboard-/Speicherreserve-Annahmen korrigieren. Vor destruktiver Migration/Erase braucht es weiter einen konkret geeigneten freigegebenen Sicherungs-/Recoveryweg; fehlt er, nicht loeschen. Historischer/Future-OTA-Entwurf bleibt unangetastet. |

**Browser-/#204-Orakel fuer kuenftige Abnahmen:** Bekannter
`GET /favicon.ico` + `404` wird separat als bekannte optionale Nebenressource
erfasst. Unbekannte URL-Pfade duerfen weiterhin 404 liefern. Fehler von
benoetigter Shell/Login, Status, Assets oder Commands werden nicht freigestellt;
erwartete Negativtests folgen ihrem konkreten Orakel. Alte Browser-Stopps und
D9-Ergebnisse bleiben unveraendert. Kein Runner-/Testcodefix hier; vor
Wiederverwendung alter Runner deren konkrete Regel pruefen. Der bekannte
D9-Cache-Runner-FOLLOW-UP bleibt im bestehenden #27-Tracking, keine neue
Schattenaufgabe und kein neuer Browser-/Geraetelauf.

## 5. Vorbereitete Issue- und Roadmapbereinigung nach Freigabe

Noch keine Issue-Bodies/-Labels/-Abhaengigkeiten aendern. Bestehende Issues
bleiben das Tracking; keine Schatten-Issues, kein neues R2-Projekt/Meilenstein,
keine erfundene Labeltaxonomie. #177-Body ist bereits konsistent und braucht
kein fachliches Delta. #19/#27/#28 bleiben OPEN; Dokumentationsentscheid bedeutet
weder DONE noch eine vollstaendige R1-, Ressourcen- oder Hardwareabnahme.

Nach Freigabe folgende aktuellen Scopebloecke in den bestehenden Bodies
pflegen und vorangehende Status-/Scopefassungen eindeutig als abgeloeste
historische Referenz kennzeichnen; historische Kommentare/PRs unveraendert.
Die bestehenden technischen Detailanforderungen bleiben Referenz fuer spaetere
Planung, werden nicht geloescht. Bei zwischenzeitlichem Bodydelta zuerst lesen
und nur diesen freigegebenen Scope aktualisieren. Beschluss-/Planlink beifuegen.

**#19 – aktueller Scopeblock:**

```text
R1: vorhandener lokaler Werksreset A/B bleibt; reale Hardwareevidence in #192
separat, keine neue Qualifikation durch Scopebereinigung. Kritische Config-/
Run-Persistenz und Recovery unveraendert.
R2_VORGEMERKT: persistentes Ereignis-/Fehler-/Resetjournal, Lauf-/Messhistorie,
Bereinigung/Archiv, JSON/CSV-Laufexport, portables Backup und atomarer Import.
5/50 bleibt historische Referenz zur Neubewertung; B0/O-BI ist kein R1-Gate mehr.
K1: erst nach ausdruecklichem O-R3/K1=B und freigegebenem Normtext umgestellt;
fehlendes Journal ist kein Nachweis eines verlaesslichen Journaldienstes.
Issue bleibt OPEN, kein Featurebau oder generischer Abschluss dieses Auftrags.
```

**#27 – aktueller Scopeblock:**

```text
R1-Bestand nach Merge #209: vorhandene sichere Webslices, Start/Stop und weitere
vorhandene Laufcommands, Programme/Status/Meldungen/Settings, Auth/CSRF/Replay,
Sessions und DE/EN/ES erhalten.
R2_VORGEMERKT: Webeditor fuer Benutzerprogramme (D6) und zusaetzliche
Web-Manuelloberflaeche. Kein neuer R1-Slice fuer diese Oberflaechen.
Keine pauschale R1-Web-Vollstaendigkeit: D9 bleibt PARTIAL, C5 nur akzeptierte
Teilabnahme; OOM-/Ressourcen-/Run-/Hardwarebefunde und Runner-FOLLOW-UP bleiben.
Issue bleibt OPEN; echte Sensor-/Aktor-/Commissioningqualifikation unveraendert.
```

**#28 – aktueller Scope- und Abhaengigkeitsblock:**

```text
R1-Bestand: D1-Touchdiagnose aus #210 MERGED; bestehende Webstatuswerte erhalten.
R2_VORGEMERKT: vollstaendige technische Webdiagnose und Detail-APIs (D-28-2=A,
neues Budget), aktueller Web-Laufchart/historische Diagramme, Diagnose- und
Serviceberichtsexport sowie Journal-/Historienanbindung an #19.
R2_KANDIDAT bei konkretem Zusatznutzen: Zusatz-Boot-Dashboard/-modell und
gefuehrte Service-UI/Mock-Aktor-Sequenz. Reale Hardware-/Peltier-/Safetytests
bleiben R1-Pflicht. Kein neuer Safety-Owner.
#19-Abhaengigkeit nur fuer spaetere konkret benoetigte Journal-/Historien-/
Exportdaten; #20/#22/#23/#24/#25 bleiben fuer benoetigte R1-Datenpfade.
Issue bleibt OPEN, kein Vollstaendigkeits- oder Hardware-PASS durch D1.
```

**#37 – aktueller Abnahmeblock:**

```text
R1: mindestens sieben zusammenhaengende Tage reale Regelung mit vorhandener
verbindlicher Web-/Touch-/WLAN- und Speicherlast, Heiz-/Kuehl-/Richtungswechsel,
periodische kritische Kontrollpunkte, kontrollierte Stromunterbrechung/Recovery.
Heap, groessten freien Block, Stack-/Flashreserven, Resets/Watchdogs/Brownouts,
Sensor-/Schreibfehler und Thermik versioniert extern nachweisen. Keine
unerlaubte Aktorfreigabe, fortschreitende relevante Leckage/Fragmentierung,
verlorene kritische Persistenz oder unbekannte sicherheitsrelevante Ursache.
R2-Journal-/Historienbelegung, Exportparallelitaet und Bereinigung/Archiv sind
keine R1-Produktpflichten. Fehlende Funktionen bekommen kein kuenstliches PASS.
Reale Sensor-/Aktor-/Safety-/Inbetriebnahmegates bleiben unveraendert verbindlich.
```

**#204 – aktueller Einordnungsblock:**

```text
FUTURE_BEI_BEDARF, keine R1-Lieferpflicht oder alleiniger Releaseblocker.
Bekannter GET /favicon.ico=404 ist kuenftig separat erfasste optionale
Nebenressource nach ACCEPTANCE_TESTS; unbekannte Pfade duerfen 404 liefern.
Die bisherige pauschale Stop-/Ausnahmeregel wird nur fuer diesen bekannten
Befund durch den expliziten neuen Vertrag ersetzt; historische Stopps und
Ergebnisse bleiben unveraendert. Funktionale HTTP-Fehler bleiben Befunde.
Kein Faviconbau, keine LittleFS-/Branding-Infrastruktur in diesem Auftrag.
```

**#177:** vorhandenen Body und Lizenz-/Distributions-Gate erhalten. Keine
Lizenzwahl oder Lizenzdatei aendern; vor offizieller R1-Distribution klaeren.
**#36:** nur die nachgewiesene Abhaengigkeitszeile `- #28` praezisieren zu
`- #28: erforderliche vorhandene Diagnose; R2/Future-Funktionen sind keine
Abschlussvoraussetzung`. Alle uebrigen Hardware-/Fehlerinjektions-/Programm-
Abnahmekriterien bleiben unveraendert.

**ROADMAP nach Normfreigabe:** #19 Backup/Import von `R1-ERWUENSCHT` und
B0/O-BI-Wartegate zu `R2_VORGEMERKT` umstellen; Journal/Historie/Export ebenso
mit konkretem Beschlusslink. #27/#28 verbleibenden R1-Bestand/Gates von
R2-vorgemerktem Scope und Kandidaten trennen, #209/#210-Merges erhalten.
#37 echte R1-Abnahme behalten; #204 Future bei Bedarf; #177 explizit als
Release-/Distributions-Gate. Keine alten FAIL/PARTIAL/NOT_RUN-Werte umwerten.
In der jetzigen Planphase nur den Verweis auf Revision 2/Reviewgate nachfuehren.

## 6. Reihenfolge, Freigaben und Commit-Schnitte

1. **Jetzt Planabgabe:** K1-Evidence unveraendert referenzieren, Gap-Matrix,
   konkreten Normtextpatch und Issuebloecke konsolidieren. Plan/Patch und engen
   Roadmap-Planstatus im selben Branch/Draft-PR committen, exakte neue Plan-SHA
   vorlegen, kanonischen Handover aktualisieren und **STOP**.
2. **Independent Plan Review** des ganzen aktuellen Diffs gegen beide Auftraege,
   Dokumentprioritaet, ADRs, unveraenderte Safety-/Hardwarepflichten und alle
   betroffenen Restpflichten aus #19/#27/#28. Builder ist nicht sein eigener
   unabhaengiger Reviewer; Reviewstatus wird nicht erfunden.
3. **Ownergates:** ausdrueckliche Freigabe der exakten Revision-2-Plan-SHA und
   ihrer Scope-/Normtext-/Issueaenderungen **sowie gesondert O-R3/K1=B**.
   Allgemeiner YAGNI-Zuschnitt, Rev.-1-SHA oder alte #19/#27/#28-Freigaben sind
   kein Ersatz. Vorher keine Normdatei, Issue oder Testregel anwenden.
4. **Dokumentcommit nach beiden Gates:** main/HEAD/Live-Review und betroffene
   Normstellen inkrementell revalidieren; nur den freigegebenen Patch sowie
   ROADMAP-Status anwenden. Alte #19-/#27-/#28-Plaene/Audits/PRs nicht editieren;
   der neue kanonische Releasevertrag benennt die abgeloesten Scopeannahmen.
5. **Separater autorisierter Issue-Schritt:** beschriebene Bodies/#36-Kante im
   bestehenden zustaendigen Ownerprozess aktualisieren, jede Remoteaenderung
   erneut lesen. Kein Schliessen/Ready/Merge/Auto-Merge/Force-Push/Branchloeschen.
6. **Builder-Dokument-Self-Check, dann STOP fuer Independent Review:** den
   gesamten finalen Dokument-/Issue-Diff auf Widerspruchsfreiheit pruefen.
   Kein Produkt-C++, NVS-/Schema-/Record-/Port-/Dependency-/Flash-/Hardware-
   oder ausfuehrbarer Test-/Runnercode. Pre-Ready ausschliesslich nach dem
   vorhandenen Workflow und separater Owneranweisung.

Materielle neue Widersprueche, noetiger Code-/Testumbau oder journalabhaengiger
Safety-/Start-/Recoverypfad beziehungsweise fehlender notwendiger Durability-
Schutz => **STOP mit Befund**, Planrevision und neue Freigabe. Kein stiller
Gatebypass oder Umbau unter YAGNI. Fehlender Sicherungsweg autorisiert kein Erase.

## 7. Pruefung und Exit

Jetzt nur Diff-/Link-/Dokument-/Patchpruefung. Der Patch muss gegen die exakte
Baseline anwendbar sein; relative Links und Releaseanker muessen existieren.
Vorgeschlagenen Endstand ausserhalb des Checkouts auf verbleibende relevante
R1-Muss- und `[x]`-Formulierungen zu den verschobenen Funktionen pruefen; deren
Geltung muss R2/Kandidat/Future sein. Nicht betroffene historische/Future-
Anforderungen werden nicht mechanisch ersetzt.

Nach spaeterer Umsetzung: keine unbeabsichtigte R1-Pflicht fuer verschobene
Funktionen, keine verlorene Safety-/Hardwarepflicht, langfristiger Bedarf in
vorhandenen Issues sichtbar, nur SPECIFICATION_REVIEW als Release-SSOT und
FUTURE_SCOPE nicht normativ. Externe versionierte Hardware-/Siebentagebelege
bleiben noetig; kein neues Produkt-Dashboard/-Export als Voraussetzung.

**Diese Planrevision:** keine neuen Tests/Builds, kein Self-Check-/Pre-Ready-
Gesamtlauf und keine Browser-/Hardware-/Flash-/Erase-/Aktoraktion; **NOT_RUN**.
Die vorherigen 487 Hosttests gelten nur fuer die unveraenderte Produktbaseline,
nicht als Review-/Normtext-/Hardwarefreigabe dieser Revision.

**Pruefstand dieser Abgabe:** 18 Vorschlagsdateien gegen unveraendertes `main`
geprueft; Patch in isolierter Kopie erfolgreich angewendet. 26 Planlinks und
36 relative Vorschlagslinks einschliesslich Releaseanker geprueft. Bestehende
SIM-/HW-Referenzzeilen und alle Produkt-/Test-/Runner-/Auditdateien unveraendert;
R1-Siebentage-, Hardware-, kritische Persistenz- und Ressourcengates im
Vorschlag gezielt kontrolliert. Dies ist Builder-Dokumentpruefung, kein
Independent Review und kein neues Test-/Hardware-PASS.
