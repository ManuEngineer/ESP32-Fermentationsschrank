# Issue #19 / K1 – Ereignisjournal-R1-Grenze: Plan und Ownerentscheid

Revision 1 · 2026-10-26 · **PLAN_ONLY / AWAITING_OWNER_DECISION**.
`ACTUATOR_RELEASE=NO`. Noch kein fuer diesen K1-Folgevorgang freigegebener
Plan-Commit. Dieser Plan ist die einzige ausfuehrbare Planfassung des Vorgangs;
sein Commit ist dem Owner mit exakter SHA vorzulegen.

## 1. Evidence zuerst / Entscheidungsempfehlung

[K1-Evidence-Matrix mit Code, vorhandenen Regressionen und Grenzen](../audits/ISSUE19_K1_JOURNAL_R1_EVIDENCE_20261026.md):
**487/487 gezielte Hosttests PASS** auf `main` nach #210. Kein produktiver
Journaladapter/-konsument und kein journalabhaengiges Safety-/Start-/Recoverygate
gefunden. Kein durch das fehlende Journal fehlender notwendiger Config-/Run-
Durability-Schutz gefunden. **Produktive Gesamtkette bis zu realen Aktoren und
Hardwarequalifikation NOT_PROVEN**; die bestehende Integrationsgrenze ist im
Nachweis explizit dokumentiert. Kein pauschales Safety-PASS.

Empfehlung: **O-R3/K1 = B (gezielte R1-SSOT-Anpassung)**. Das ist eine neue
Entscheidungsvorlage, keine Umdeutung der bisherigen bedingten Variante A.
Bis zur ausdruecklichen Entscheidung bleibt K1 offen und unerfuellt.

## 2. Verifizierte Basis und Quellen

```text
CONTEXT_BASELINE_BRANCH: origin/main
CONTEXT_BASELINE_SHA: adfbb34b60c290206660ac338af833966a3f8e7c
CONTEXT_HEAD_SHA: adfbb34b60c290206660ac338af833966a3f8e7c (Analyse/Teststand)
CONTEXT_PLAN_SHA: NONE (neuer K1-Plan, noch nicht freigegeben)
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: main nach Merge #210; nur dieser Plan, Evidence und Roadmapstatus
SOURCE_OF_TRUTH_CONFLICT: Journal-Startbedingung und R1-Journal-/Historienpflichten
  gegen Owner-Deferral; konkrete K1-Entscheidung steht aus
```

Repository `ManuEngineer/ESP32-Fermentationsschrank`. Separater Branch
`agent/issue-19-k1-journal-r1-plan` direkt auf kanonischem `main`, keine gestapelte
Basis. Der vorgefundene saubere Checkout `agent/issue-28-readonly-diagnostics-plan`
auf `a292a2d14f3d987ff289bca28906902af1435c63` und andere Worktrees bleiben erhalten.

Live am 2026-10-26 gelesen:

- [#210](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/210)
  MERGED, Merge `adfbb34b60c290206660ac338af833966a3f8e7c`; letzter kanonischer
  Handover `issuecomment-6390566165` gelesen. Er enthaelt ueberholte parallele
  NOT_RUN-/Reviewangaben; der Live-Merge und die verlinkte konkrete D1-Evidence
  sind fuer den aktuellen Status massgebend. Kein neuer Hardwarebeleg daraus.
- [#19](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/19),
  [#28](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/28),
  [#37](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/37)
  OPEN; Bodies und vorhandene Kommentare gelesen. #19 hat veraltete
  Ablauf-A-/Merge-/Hardwareangaben; #28 hat weiter formale #19-Abhaengigkeit;
  #37 verlangt weiter Journal/Historie. Keine Issue-Body-Aenderung autorisiert.
- Bisheriger #19-Plan Rev. 8, Freigaben `6fbf1304566472c10b7cc52439475a5743960c0e`
  und `aa665f1d00fb363db6049b8b9df3af1c1a8be89c`; K1 = A bedingt,
  K2–K7 genehmigt, breite Reconciliation ausdruecklich separat. Diese Freigaben
  erlauben keine neue K1-Normentscheidung. #210-Plan-SHA `81f1804` betrifft D1.

Konkret erforderliche Quellen: Root-/lokale AGENTS, AGENT_WORKFLOW,
ENGINEERING_PRINCIPLES, CI_AND_QUALITY_GATES, SPECIFICATION_REVIEW (Prioritaet),
DECISIONS (insbesondere ADR-010/013/014/016/018), ADR-013-Modulzuordnung,
ROADMAP; #19-Plan Rev. 8 Abschnitte Ownerentscheid/7; #28-D1-Plan;
RESOURCE_BUDGET_AND_MAINTENANCE, SYSTEM_SAFETY_AND_RECOVERY, RUN_PERSISTENCE,
SAFETY_AND_FAULTS, REQUIREMENTS, DIAGNOSTICS_AND_MAINTENANCE, STATE_MACHINE,
BACKUP_SECURITY_RETENTION und ACCEPTANCE_TESTS. Code-/Teststellen und
Suchumfang stehen in der Evidence. Keine neue Abstraktion, kein neuer ADR noetig:
Entscheidung nach Freigabe im zustaendigen Fachvertrag verankern.

## 3. Konkreter zur Freigabe vorgelegter K1-Entscheid

> R1 verlangt kein persistentes Ereignisjournal als Start- oder Safety-Gate. Zulässige Starts und Recovery richten sich ausschliesslich nach den bestehenden vertrauenswürdigen Config-/Run-/Transaktionszuständen und den erforderlichen aktuellen Sensor-/Planner-/Safetyfreigaben. Kann eine notwendige kritische Persistenz oder diese Evidenz nicht sichergestellt werden, bleibt der bestehende fail-closed-Pfad verbindlich. Journal/Historie werden nicht als vorhanden oder zuverlässig behauptet.

**Bewusster Nachteil:** keine vollstaendige historische Fehler-/Brownout-/
Watchdog-Chronik ueber mehrere Neustarts. Aktuelle Resetursache und vorhandene
Daten sind kein Ersatz fuer ein Archiv. Ein fehlendes Journal und ein
hypothetischer Journalfehler sind kein Beweis eines verlaesslichen Journaldienstes.

**Unveraendert:** fail-closed bei untrusted Config/Persistenz; atomare Run-
Gesamttransaktion und Write-before-Apply; aktuelle Sensor-/Planner-/Safety-
Evidenz; SAFE_BOOT; Boot all-off als verbindliche Zielanforderung (Hardwarebeleg
weiter offen); Watchdog-RAM-Latch und positive Clear-Regeln. Keine neue
Aktorpermission. Elektrische/thermische Inbetriebnahme, Aktor-/Powercut-Gates
und mindestens sieben zusammenhaengende Tage Belastungstest bleiben Pflicht.

**K2–K7 werden beachtet:** Abnahme bezieht sich auf vorhandene Funktionen;
#19 bleibt fuer Teilfunktionen offen; #28 wird fachlich von spaeterem Journal/
Historie entkoppelt; keine erfundenen Budgets; 5/50 bleibt Future-Anforderung;
Backup/Import nur nach B0/O-BI und bestehenden #27-Pfaden. Dieser Vorgang
implementiert weder diese Features noch anderen #19/#27/#28-Scope.

## 4. Umsetzungs- und Commit-Schnitte nach Freigabe

1. **Jetzt:** read-only Produktanalyse, ausdruecklich beauftragte bestehende
   Regressionen, Evidence, dieser vollstaendige Plan mit Textdiff und enger
   Roadmap-Statuspflege. Plan-Commit im Draft-PR ausweisen und **STOP**.
2. **Nur nach ausdruecklichem O-R3/K1=B und Freigabe der exakten Plan-SHA:**
   Baseline/HEAD/Live-Kommentare inkrementell pruefen. Nur die unten stehenden
   Normtextaenderungen im selben Branch/Draft-PR uebernehmen; die bisherigen
   K1-Ownerentscheidungen historisch erhalten und im alten #19-Plan oben mit
   Verweis auf den neuen Beschluss als abgeloest kennzeichnen. Roadmap K1-Status
   und #28-Fachentkopplung dann auf die konkrete Entscheidung referenzieren.
   Keine neue Safety-/Persistenz-/Port-/Schema-/Record-/Flash-/Codeaenderung.
3. Betroffene Dokumente gegen K1–K7 und Suchtreffer pruefen, Markdown-/Diff-/
   Linkpruefung; keine neuen Tests fuer reine Textkorrekturen. Builder-Self-Check
   des Dokumentdiffs, **STOP fuer Independent Review**. Keine eigenstaendige
   formale Full-Review-Freigabe durch den Builder; keine Agentendelegation in
   diesem Auftrag. Pre-Ready nur nach Workflow und eigener Owneranweisung.

Materielle Abweichung oder neuer journalabhaengiger Pfad/fehlender notwendiger
Durability-Schutz => **STOP**, Befund und neue Planfreigabe. Keine Umdefinition
unter dem Etikett YAGNI. Kein Hardware-, Erase-, Flash- oder Aktortest.

## 5. Minimaler SSOT-Textdiff zur Entscheidung (NICHT angewendet)

Die folgenden Diffs sind ausschliesslich Vorschlaege innerhalb dieses Plans.
Sie aendern heute keine normative Datei. Basis ist der oben genannte main-SHA.
Bereits vorhandene aktuelle Fehleranzeige/Run-/Recoverydaten und Geheimnisschutz
bleiben erhalten. Journal-/Historien-Detailregeln erhalten einen klaren Future-
Geltungsbereich; Records, Aufbewahrungsdesign und neue Budgets werden nicht
weiter spezifiziert. Historische Testresultate bleiben wortgetreu erhalten.

````diff
--- a/docs/RESOURCE_BUDGET_AND_MAINTENANCE.md
+++ b/docs/RESOURCE_BUDGET_AND_MAINTENANCE.md
@@ -23,4 +23,17 @@
 FRAMEWORK_DEFAULT_IS_PRODUCT_BUDGET=NO
 ```
+
+## R1-Grenze Journal und Historie
+
+Fuer Start-/Safety-/Recovery-Gates gilt der R1-Journalvertrag in
+`SYSTEM_SAFETY_AND_RECOVERY.md`, Abschnitt Fehler- und Resetprotokoll.
+
+
+
+Journal, Laufhistorie, deren Bereinigung und Laufexport sind Future. Die
+nachfolgenden Journal-/Historienbudgets, Aufbewahrungs-, Bereinigungs- und
+Wear-Leveling-Regeln beschreiben diesen spaeteren Ausbau; sie erzeugen keine
+R1-Reserve und kein R1-Abnahmekriterium. Vorhandene kritische Persistenz,
+aktuelle Fehleranzeige, Ressourcenmessung und Inbetriebnahmegates bleiben Pflicht.

 ## Grundsaetze
@@ -70,7 +83,5 @@
 Belegung der Konfiguration
 Belegung der kritischen Laufpersistenz
-Belegung des Fehler- und Resetjournals
-Belegung der Laufzusammenfassungen
-Belegung der Diagrammhistorie
+Belegung tatsaechlich vorhandener R1-Daten
 ```

@@ -208,5 +219,6 @@
 - kein sicherer aktiver Laufkontrollpunkt garantiert werden kann
 - keine gueltige Konfigurations- und Rueckfallrevision erhalten werden kann
-- das Fehlerjournal fuer verriegelte Ereignisse nicht verlaesslich ist
+- erforderliche aktuelle Config-/Run-/Transaktions- oder Safety-Evidenz nicht
+  verlaesslich ist; ein persistentes Ereignisjournal ist kein R1-Startgate
 - der freie Heap oder groesste zusammenhaengende Block die nachgewiesene
   Mindestreserve unterschreitet
@@ -223,5 +235,7 @@
 - Regel- oder Sicherheitsaufgaben nicht mehr verlaesslich ausgefuehrt werden
 - kritische Laufpersistenz nicht mehr garantiert ist
-- eine notwendige Fehlerrevision nicht mehr sicher gespeichert werden kann
+- eine fuer die sichere Run-/Recovery-Transaktion erforderliche Revision nicht
+  mehr sicher gespeichert werden kann; ein spaeteres Diagnosearchiv gehoert
+  nicht zu dieser kritischen Persistenz
 - Speicherbeschaedigung die aktive Konfiguration oder Sicherheitsdaten betrifft

@@ -242,8 +256,8 @@
 3. aktive Konfiguration und letzte gueltige Rueckfallrevision
 4. aktiver Lauf, Kontrollpunkte und Wiederherstellungsdaten
-5. Fehler-, Brownout-, Watchdog- und Resetjournal
+5. notwendige aktuelle Fehleranzeige und vorhandene Diagnose-/Recoverydaten
 6. notwendige lokale Bedienung und Weboberflaeche
-7. abgeschlossene Laufzusammenfassungen
-8. detaillierte alte Diagramm- und Komfortdaten
+7. abgeschlossene Laufzusammenfassungen (Future)
+8. detaillierte alte Diagramm- und Komfortdaten (Future)
 9. optionale Betriebs- und Wartungsstatistiken

@@ -325,6 +339,6 @@
 ### Stufe C: keine persistenten Komfortzaehler

-Nur bereits sicherheits- und fehlerbedingt notwendige Ereignisse werden im
-bestehenden Journal gespeichert.
+R1 besitzt kein persistentes Ereignisjournal. Stufe C fuehrt keine persistenten
+Komfortzaehler ein; vorhandene kritische Run-/Recoverypersistenz bleibt erhalten.

 Entscheidungsregel:
@@ -406,5 +420,5 @@
 - [x] laufenden Prozess bei reiner Historienknappheit nicht stoppen
 - [x] neuen Lauf bei gefaehrdeter kritischer Persistenz nicht starten
-- [x] Firmware, aktive Konfiguration, Laufwiederherstellung und Fehlerjournal vor
+- [x] Firmware, aktive Konfiguration, Laufwiederherstellung und aktuelle Fehlerdaten vor
       Komforthistorie priorisieren
 - [x] keine OTA-Slots im Release-1-Flashbudget
````

````diff
--- a/docs/SYSTEM_SAFETY_AND_RECOVERY.md
+++ b/docs/SYSTEM_SAFETY_AND_RECOVERY.md
@@ -331,5 +331,6 @@
 - Prozess darf weiterlaufen, sofern kritische Laufpersistenz weiterhin sicher ist,
 - sichtbare Warnung,
-- Ereignis im verbleibenden Fehlerjournal erfassen,
+- aktuelle Warnung und vorhandene Run-/Recoveryinformation erhalten; ein
+  spaeteres Diagnosejournal ist dafuer keine R1-Voraussetzung,
 - Historienaufzeichnung begrenzen oder deaktivieren,
 - aktiven Laufkontrollpunkt priorisieren.
@@ -340,5 +341,4 @@

 - aktueller Laufkontrollpunkt kann nicht mehr atomar gespeichert werden
-- Fehlerjournal fuer verriegelte Ereignisse ist nicht mehr verlaesslich
 - Konfigurationsspeicher oder Rueckfallrevision ist kritisch beschaedigt
 - keine sichere Speicherrevision mehr aktivierbar
@@ -383,5 +383,16 @@
 keinen neuen persistenten Latch ein.

-## Fehler- und Resetprotokoll
+## Fehler- und Resetprotokoll (Future, kein R1-Safety-Gate)
+
+R1 verlangt kein persistentes Ereignisjournal als Start- oder Safety-Gate. Zulässige Starts und Recovery richten sich ausschliesslich nach den bestehenden vertrauenswürdigen Config-/Run-/Transaktionszuständen und den erforderlichen aktuellen Sensor-/Planner-/Safetyfreigaben. Kann eine notwendige kritische Persistenz oder diese Evidenz nicht sichergestellt werden, bleibt der bestehende fail-closed-Pfad verbindlich. Journal/Historie werden nicht als vorhanden oder zuverlässig behauptet.
+
+Ein spaeterer Ausfall oder Platzmangel des Diagnosearchivs allein ist kein
+kritischer R1-Persistenzfehler. Die vorstehenden fail-closed-Regeln fuer
+notwendige Config-/Run-/Transaktionsdaten bleiben verbindlich. Ohne Archiv
+existiert keine vollstaendige Fehler-/Brownout-/Watchdog-Chronik ueber mehrere
+Neustarts; aktuelle Resetursache und vorhandene Daten ersetzen kein Archiv.
+
+Die folgenden Record-, Aufbewahrungs- und Exportregeln sind spaeter zu
+spezifizieren und keine Behauptung eines vorhandenen R1-Dienstes.

 Das Fehlerprotokoll ist begrenzt und verwendet ein append-only-, Journal- oder
@@ -475,5 +486,5 @@
 - [x] unvollstaendige Transaktionen sperren Recovery; kein neuer Safety-Latch
 - [x] #23-Watchdog-Reset nur explizit im aktuellen Boot nach frischer Evidenz
-- [x] begrenztes priorisiertes Fehler- und Resetjournal
+- Future: begrenztes priorisiertes Fehler- und Resetjournal (kein R1-Gate)
 - [x] nach jedem relevanten Neustart vollstaendig validierter Wiederanlauf
 - [x] alte GPIO-Zustaende werden niemals wiederhergestellt
````

````diff
--- a/docs/RUN_PERSISTENCE.md
+++ b/docs/RUN_PERSISTENCE.md
@@ -14,8 +14,11 @@
 - Laufdaten, Sensoren, Sicherheitszustand und Wiederanlaufaktion werden vor jeder
   Freigabe validiert.
-- Wichtige Ereignisse werden sofort gespeichert.
+- Erforderliche kritische Run-Zustandsaenderungen werden transaktional
+  gespeichert; ein separates Ereignisarchiv ist fuer R1 nicht erforderlich.
 - Periodische Kontrollpunkte ergaenzen die ereignisgesteuerten Speicherungen.
 - Es wird nicht in jedem Sensorzyklus geschrieben.
-- Kritische Laufdaten und Sicherheitsjournale haben Vorrang vor Historien.
+- Kritische Laufdaten haben Vorrang vor Komfortdaten. Journal, Laufhistorie,
+  deren Bereinigung und Laufexport sind Future; die darauf bezogenen
+  Detailregeln erzeugen kein R1-Start-, Recovery- oder Abnahmegate.
 - Release 1 funktioniert mit 4 MB Flash ohne PSRAM.
 - Release 1 reserviert keine dualen OTA-Slots.
@@ -643,6 +646,6 @@
   - Konfiguration
   - aktiven Lauf und Rueckfall
-  - Sicherheitsjournal und Persistenzfehler-Latch
-  - Historie und Zusammenfassungen
+  - Diagnosearchiv und allgemeiner Persistenzfehler-Latch (Future, nicht R1)
+  - Historie und Zusammenfassungen (Future)
   - temporaere Exporte
 - Sicherheits- und Regelungslogik haben Vorrang
````

````diff
--- a/docs/SAFETY_AND_FAULTS.md
+++ b/docs/SAFETY_AND_FAULTS.md
@@ -22,5 +22,6 @@
 - Der stateless `ActuationInterlock` bewertet aktuelle Evidenz gegen die
   endliche, typisierte FaultCode-Menge und liefert daraus die Permission.
-  Ack ist nur Anzeige/Journaling und loescht weder Ursache noch Gate.
+  Ack ist in R1 Anzeige und loescht weder Ursache noch Gate; persistentes
+  Journaling ist Future und keine Voraussetzung fuer die Safety-Reaktion.
 - Es gibt in #24 keinen generischen persistenten Safety-Latch, keinen
   Restart-Zaehler, kein Resetzeitfenster, keine Service-PIN-Pflicht und keine
@@ -48,6 +49,7 @@
   Fehlerzustandsmaschine.
 - Aktive Ursachen bleiben in einer endlichen, festen FaultCode-Maske
-  nachvollziehbar; Historie und Journaling bleiben beim bestehenden
-  `IEventJournal`, nicht im `ActuationInterlock`.
+  nachvollziehbar. Persistente Historie und Journaling sind Future; der
+  vorhandene Port `IEventJournal` ist kein implementierter oder qualifizierter
+  Journaldienst und kein kritischer Durability-Contract.
 - Sicherheitsreaktionen funktionieren ohne WLAN, Weboberflaeche, Netzwerkzeit
   oder Heimserver.
@@ -78,5 +80,7 @@
 veraendert weder Gate noch Clear-Zustand. Safety-Reaktion und Gate werden vor
 Journal/Notification bestimmt; deren Fehler koennen keine Abschaltung
-rollbacken.
+rollbacken. R1 verlangt kein persistentes Journal als Start-, Abschalt- oder
+Recovery-Gate. Die aktuelle Fault-/Sensorevidenz, kritische Persistenz und
+positiven Clear-Regeln bleiben unveraendert verbindlich.

 ## C2-Legacy/Future – historische Vierklassenbeschreibung, nicht #24-R1
````

````diff
--- a/docs/REQUIREMENTS.md
+++ b/docs/REQUIREMENTS.md
@@ -193,6 +193,7 @@
 - Lesende Diagnose bleibt waehrend eines Laufes verfuegbar.
 - Hardwaretests sind nur im Standby, PIN-geschuetzt und zeitlich begrenzt.
-- Lauf-, Diagnose- und Servicebericht-Exporte enthalten keine Passwoerter, PINs,
-  Tokens oder erfundenen Werte.
+- Laufexport aus #19 ist Future. Fuer angebotene Exporte bleiben Passwoerter,
+  PINs, Tokens und erfundene Werte verboten; daraus folgt keine Zusage eines
+  vorhandenen Journal-/Historienexports.
 - UART/FT232RL ist der verbindliche Update- und Wiederherstellungsweg fuer
   Release 1.
@@ -202,8 +203,10 @@

 - Ziel sind 4 MB Flash ohne vorausgesetzte PSRAM.
-- Firmware, Konfiguration, Laufpersistenz, Journal und Historie erhalten feste
-  Budgets.
+- Ressourcenbudgets gelten fuer tatsaechlich vorhandene R1-Funktionen und
+  deren gemessene Systemreserven. Journal, Laufhistorie, deren Bereinigung und
+  Laufexport sind Future; daraus entstehen keine vorsorglichen R1-Reserven.
 - Puffer, Programme, Meldungen und Exporte besitzen feste Maximalgroessen.
-- Alte nichtkritische Protokolle und Diagrammdaten werden proaktiv bereinigt.
+- Proaktive Bereinigung alter Protokoll-/Historien-/Diagrammdaten gehoert zum
+  spaeteren Ausbau. R1 behaelt kritische Persistenz und aktuelle Fehleranzeige.
 - Kritische Daten haben immer Vorrang vor Komfortdaten.
 - Heap, niedrigster Heap, groesster freier Block, Firmwaregroesse und
````

````diff
--- a/docs/DIAGNOSTICS_AND_MAINTENANCE.md
+++ b/docs/DIAGNOSTICS_AND_MAINTENANCE.md
@@ -69,5 +69,6 @@
 - Luefterzustand und Nachlauf
 - Programmschnappschuss und Laufrevision
-- Fehler-, Persistenz-, Reset-, Brownout- und Watchdogjournal
+- aktueller Fehler-/Persistenz-/Recoverystatus und aktuelle Resetursache;
+  persistentes Fehler-/Brownout-/Watchdog-/Resetjournal ist Future
 - Netzwerk-, NTP-, Flash-, Heap- und Ressourcenwerte
 - erlaubte Exporte
@@ -79,5 +80,6 @@
 Erlaubt:

-- Temperaturen und Historie ansehen
+- aktuelle Temperaturen und vorhandene Run-/Recoveryinformationen ansehen
+  (persistente Laufhistorie ist Future)
 - Sensorstatus und Messwertalter ansehen
 - Regleranforderung und Aktorfreigabe ansehen
@@ -159,5 +161,5 @@
 4. Konfigurationsintegritaet und Rueckfallrevision
 5. Laufkontrollpunkt, Transaktionsmarker und Persistenzgesundheit
-6. Fehler- und Resetjournal
+6. aktuelle Fehler-/Resetdiagnose (kein persistentes Journal als R1-Gate)
 7. 1-Wire-Busse und feste Sensoridentitaeten
 8. aktuelle Sensorwerte und Plausibilitaet
@@ -293,5 +295,10 @@
 ## Exporte

-### Laufexport
+### Laufexport (Future)
+
+Journal, Laufhistorie, deren Bereinigung und Laufexport aus #19 sind fuer R1
+zurueckgestellt. Aktuelle Fehleranzeige, Resetursache und vorhandene
+Run-/Recoveryinformationen bleiben erhalten; sie sind kein historisches Archiv.
+Die folgenden Laufexportregeln erzeugen kein R1-Abnahmekriterium.

 - Laufkennung und Programmschnappschuss
````

````diff
--- a/docs/STATE_MACHINE.md
+++ b/docs/STATE_MACHINE.md
@@ -207,6 +207,8 @@

 - passive Diagnose
-- Fehler- und Resetjournal lesen
-- Berichte exportieren
+- aktuelle Fehleranzeige, Resetursache und vorhandene Run-/Recoverydaten lesen
+  (persistentes Journal/Historie sind Future und kein SAFE_BOOT-Ausgangsgate)
+- vorhandene Berichte exportieren; kein fuer R1 vorausgesetzter
+  Journal-/Historien-/Laufexport aus #19
 - Netzwerkwiederherstellung ohne Aktorwirkung
 - passive Diagnose und Export ohne Aktorwirkung
````

````diff
--- a/docs/BACKUP_SECURITY_RETENTION.md
+++ b/docs/BACKUP_SECURITY_RETENTION.md
@@ -114,5 +114,5 @@
 - darf nicht über die Weboberfläche angeboten werden

-## Laufexport
+## Laufexport (Future, kein R1-Gate)

 Jeder Lauf kann einzeln exportiert werden.
@@ -170,5 +170,10 @@
 - das interne binaere Revisionsformat ist kein portables Backupformat

-## Aufbewahrungsmodell
+## Aufbewahrungsmodell (Future)
+
+Die Regeln fuer 5 detaillierte und 50 zusammengefasste Laeufe sowie die
+folgende Historienbereinigung bleiben spaetere Anforderungen. R1 besitzt kein
+persistentes Journal-/Historienarchiv und reserviert dafuer keinen Speicher.
+Aktiver Run und notwendige Recoverydaten bleiben kritische R1-Persistenz.

 Werkseinstellung:
@@ -215,7 +220,7 @@
 - Service-PIN
 - Sitzungen und Tokens
-- Laufhistorie und Zusammenfassungen
+- Laufhistorie und Zusammenfassungen, sobald spaeter vorhanden
 - aktive Laufdaten
-- Fehler- und Komforthistorie soweit für den Reset vorgesehen
+- Fehler- und Komforthistorie, sobald spaeter vorhanden und fuer Reset vorgesehen

 Er stellt wieder her:
@@ -257,8 +262,8 @@
 2. gültige Konfiguration und Rückfallrevision
 3. aktiver Lauf und Wiederherstellung
-4. Sicherheits- und Resetjournal
+4. notwendige aktuelle Fehleranzeige und vorhandene Diagnose-/Recoverydaten
 5. notwendige Web- und UI-Ressourcen
-6. Laufzusammenfassungen
-7. alte detaillierte Historie
+6. Laufzusammenfassungen (Future)
+7. alte detaillierte Historie (Future)

 Web-OTA ist eine mögliche Zukunftsfunktion und erhält in Release 1 weder
````

````diff
--- a/docs/ACCEPTANCE_TESTS.md
+++ b/docs/ACCEPTANCE_TESTS.md
@@ -744,8 +744,10 @@
 - keine unerlaubte Aktorfreigabe
 - keine relevante fortschreitende RAM-Leckage
-- Speicherbereinigung innerhalb der Budgets
+- Ressourcen und tatsaechlich vorhandene R1-Daten innerhalb nachgewiesener Budgets
 - kritische Persistenz und Sperren nach Unterbrechung wiederherstellbar
-- Web, Display, Sensoren, Exporte und Regelung parallel stabil
-- Fehler- und Resetjournal innerhalb des Budgets
+- Web, Display, Sensoren und Regelung sowie tatsaechlich angebotene Exporte
+  parallel stabil; #19-Journal/-Historie/-Laufexport bleiben Future
+- aktuelle Fehler-/Resetdiagnose und vorhandene Run-/Recoverydaten geprueft;
+  kein Journal-PASS fuer einen nicht implementierten Dienst

 ### Gate 5: Releasekandidat
@@ -754,5 +756,6 @@
 - lokale und Webbedienung geprueft
 - Stromunterbrechungs- und Recoveryablaeufe geprueft
-- Exporte und Diagnose geprueft
+- aktuelle Diagnose und tatsaechlich angebotene Exporte geprueft;
+  zurueckgestellte #19-Funktionen sind kein R1-Gate
 - bekannte Abweichungen bewertet
 - keine offene unbekannte Sicherheitsursache
@@ -808,5 +811,5 @@
 - `Success` ohne zweiten Readback sowie `CommitOutcomeUnknown` mit allen drei
   vorhandenen `writeExact()`-Aufloesungen pruefen
-- Historienspeicher bis zur Bereinigung fuellen
+- Future: Historienspeicher bis zur Bereinigung fuellen (kein R1-Gate)
 - nichtkritischen RAM- oder Exportfehler erzeugen

@@ -872,6 +875,6 @@
 - Displaybetrieb und wiederholte Bedienung
 - parallele Webzugriffe und Live-Aktualisierung
-- wiederholte Exporte
-- periodische Kontrollpunkte und Bereinigung
+- wiederholte tatsaechlich angebotene Exporte (kein vorausgesetzter #19-Laufexport)
+- periodische kritische Laufkontrollpunkte; Historienbereinigung ist Future
 - mehrere Heiz-, Kuehl- und Richtungswechsel
 - WLAN- und NTP-Ausfall
@@ -886,6 +889,6 @@
 - Sensor- und Busfehler
 - Regler- und Aktorereignisse
-- Flash- und Historienbelegung
-- Bereinigungen und Schreibfehler
+- Flashbelegung der vorhandenen R1-Daten; Historienbelegung erst im spaeteren Ausbau
+- kritische Schreibfehler; Historienbereinigungen erst im spaeteren Ausbau
 - Web- und Exportfehler
 - Temperaturstabilitaet und Richtungswechsel
````

## 6. Vorbereitete Issue-Vertragsdeltas (Owner/zustaendiger Prozess)

Keine Issues in dieser Planphase aendern. Nach dem konkreten Beschluss sind
folgende eng begrenzte Deltas zur bestehenden Bodyfassung vorbereitet; der
Owner/zustaendige Prozess publiziert sie mit Plan-/Beschlusslink:

| Issue | Konkretes Vertragsdelta nach Freigabe |
|---|---|
| #19 | Status: Werksreset A/B softwareseitig gemergt (#191/#202), Hardwareevidence ausschliesslich nach #192. Scope/Tests/DoD: Journal, Laufhistorie, deren Bereinigung und Laufexport als Future markieren; 5/50 bleibt spaetere Anforderung, Backup/Import bleibt B0/O-BI-bedingt. K1 auf den konkreten B-Beschluss und neuen Fachvertrag verlinken. Tests fuer volle Historie/Bereinigung nicht als R1-PASS behaupten. Issue bleibt offen. |
| #28 | Formale pauschale Abhaengigkeit `#19` durch `#19: nur konkret benoetigte vorhandene Daten/Ownerpfade; kein Warten auf Future-Journal/-Historie/-Laufexport` praezisieren; #20/#22–#25 bleiben. Status D1 MERGED (#210). Journal/Historie/Laufexport sind Future; andere Diagnose-/Web-/Boot-Selbsttest-/Serviceanteile werden durch diesen Vorgang weder geliefert noch umdefiniert. |
| #37 | Scope `parallele ... Export-, Speicher- ...` auf tatsaechlich angebotene Exporte und vorhandene R1-Speicherpfade beziehen; `Speicherbereinigung und Kontrollpunkte` -> `kritische Laufkontrollpunkte; Historienbereinigung Future`. Kriterium `Journal und Historie bleiben innerhalb des Budgets` -> `Vorhandene R1-Daten und Ressourcen bleiben innerhalb nachgewiesener Budgets; aktuelle Fehler-/Reset-/Recoverydiagnose geprueft`. Externe versionierte Testaufzeichnung fuer Heap/Resets/Watchdogs/Sensor-/Schreibfehler bleibt Pflicht. Sieben Tage, Stromunterbrechung/Recovery, Safety-, Aktor- und Release-Gates unveraendert. |

## 7. Abgabe, Pruefung, offenes Gate

Aktuelle Abgabe umfasst nur Evidence, Plan/Textvorschlag und Roadmapstatus.
Die **487/487 PASS** gehoeren zur unveraenderten Produktbaseline, nicht zu einem
spaeteren Normtext- oder Hardwarestand. Uebrige Tests/Builds/Pre-Ready und
Hardware **NOT_RUN** (Evidence). Dokumentdiff per `git diff --check`, interne
Links und Textvorschlag gegen Baseline pruefen; nur Markdown im PR-Diff.

Offen bleibt die ausdrueckliche Ownerentscheidung **O-R3/K1 = B** samt Freigabe
der **exakten Plan-Commit-SHA**. Die allgemeine YAGNI-Zustimmung und K2–K7 sind
keine vorweggenommene K1-Freigabe. Nach Planabgabe kanonischen SESSION HANDOVER
pflegen und anhalten. Kein Ready-/Draft-Wechsel, Merge, Auto-Merge, Force-Push,
Branchloeschen oder Issueabschluss.
