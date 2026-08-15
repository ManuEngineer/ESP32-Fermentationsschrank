# Akzeptanztests und Release-Gates

## Status

Dieses Dokument definiert die verbindlichen Testebenen, Fehlerinjektionen,
Hardwareabnahmen und Release-Gates fuer Release 1. Die Korrekturen aus den
Reviews von PR #38 sind integriert. Exakte thermische Grenzwerte,
Regelparameter und Ressourcenschwellen bleiben bis zu den jeweiligen Messungen
`TBD_COMMISSIONING` beziehungsweise `TBD_IMPLEMENTATION_BUDGET`.

## Grundsaetze

- Sicherheitskritische Funktionen werden gezielt unter Fehlerbedingungen getestet.
- Native Tests ersetzen keine Hardwaretests; Hardwaretests ersetzen keine
  deterministischen Softwaretests.
- Jeder formelle Test verweist auf eine Anforderung, Entscheidung, Fehlernummer
  oder Sicherheitsregel.
- Ein Test darf keine unkontrollierte Aktorfreigabe oder Umgehung der normalen
  Sicherheitslogik verlangen.
- Hardwaretests verwenden den bestaetigten Hardwarestand, die dokumentierte
  Verdrahtung und einen gespeicherten Servicebericht.
- Ein nicht ausgefuehrter Test ist `BLOCKED` oder `NOT_RUN`, nicht bestanden.
- Ein bestandener Einzeltest hebt keinen anderen aktiven Sicherheitsfehler auf.
- Ein Neustart gilt nie als Fehlerreset.
- `SAFE_BOOT` bleibt in allen Tests aktorfrei.

## Testebenen

### Ebene 1: Native Unit-Tests

Mindestens:

- Programm- und Konfigurationsvalidierung
- kanonische Zustandsuebergaenge
- Bootprioritaet fuer Bootschleifen, persistierte Sperren und Speicherfehler
- Wiederherstellung eines persistierten `COMPLETED`
- virtuelle monotone und absolute Zeit
- Ausfallzeit als Unter-/Obergrenze
- kein automatischer Phasenabschluss bei ueberlappendem Unsicherheitsintervall
- Zielqualifikation und Gnadenzeit
- PI-Reglerkern und Luftbegrenzung
- Impulsakkumulator
- Mindest-Einschaltzeit, Mindest-Ausschaltzeit und Totzeit
- Sensorstatus `VALID`, `STALE`, `FAILED`
- Regelsensorauswahl (Issue #21): vollstaendige Startmatrix ueber alle
  Programmpraeferenzen und Produktvaliditaeten; kanonische
  Entscheidungsfunktion fuer automatischen und manuellen Pfad identisch;
  laufzeitseitiger Auswahlzustand ausserhalb des Wireformats, fail-closed
  nach Restore; strukturell ungueltige externe Kompatibilitaetsevidenz
  blockiert nur die Rueckkehr, nicht unabhaengige Sicherheitsreaktionen
- Fehlerklassifikation, Quittierung und Fehlerreset
- Persistenzschema, atomare Revisionen und Rueckfall
- Transaktionsabsicht vor aktorwirksamer Zustandsaenderung
- Persistenzfehler-Latch und Bootauswertung
- kritischer Schreibfehler sperrt neue Aktoranforderungen vor weiteren
  Persistenzversuchen und setzt den RAM-seitigen Latch
- minimaler persistenter Latch wird ausserhalb des normalen Laufjournals versucht;
  auch sein Schreibfehler bleibt fail-closed
- unvollstaendiger Transaktionsmarker fuehrt beim Boot zu `SAFE_BOOT`
- Recovery-Aktorfreigabe erst nach bestandener Lesen-Schreiben-Pruefung und
  erfolgreich persistierter, wieder verifizierter Recoveryrevision
- Persistenzfehler-Latch bleibt bei Quittierung, Neustart und isoliert
  erfolgreichem Schreibversuch gesetzt
- Latch-Reset nur im geschuetzten Serviceablauf nach bestandener
  Speicherpruefung, aufgeloestem Transaktionsmarker und dokumentiertem Reset
- Aufbewahrung und Bereinigung
- PIN-unabhaengiger Vollreset-Ablauf als Zustands- und Berechtigungslogik
- Device-Shell mit Header, exakt vier festen Slots, Home-/Zurueck-Hierarchie
  und sichtbaren leeren Slots
- gemeinsame rendererunabhaengige View-Modelle, Commands, strukturierte
  Command-Ergebnisse, Bestaetigungen und Snapshotaktualisierung fuer Touch/Web
- Textfallback aktive Sprache -> Englisch -> sichtbarer technischer Schluessel,
  Theme-Standardfallback und 320-x-240-Textlaengenvertrag
- lokale Servicefreigabe: 10 Minuten Inaktivitaet, kein UI-Parameter, keine
  R1-Maximaldauer sowie Sperre bei Neustart, Abmelden und Safetyzustandswechsel

Tests sind reproduzierbar und unabhaengig von realer Uhrzeit, Netzwerk und
zufaelliger Taskplanung.

### Ebene 2: Simulierte Gesamt- und Fehlerablaeufe

Mindestens:

- Standby -> Vorheizen -> Produkt einsetzen -> Zielqualifikation -> Fermentation
- luftgefuehrter Lauf ohne Produktfuehler
- Produktfuehlerausfall, Luft-Ersatzbetrieb (manuell und automatisch nach
  Wartezeit) sowie manuelle und automatisch validierte Rueckkehr
  (Issue #21): im aktiven Luft-Ersatzbetrieb (`AirFallbackActive`) bleibt die
  Regelung ueber Luft weiterhin freigegeben, solange Schrankluft- und
  Kuehlkoerperfuehler gueltig sind - ein ungueltiger Produktfuehler allein
  sperrt dort nicht; erst die Rueckkehr zu `NormalProduct` verlangt Produkt-,
  Schrankluft- und Kuehlkoerperfuehler gemeinsam gueltig
  (Sicherheits-Vorrangregel). Ein einzelner Schrankluft-/Kuehlkoerperausfall
  waehrend Ersatzbetrieb sperrt dagegen sofort in den sicheren Zustand
  (`SafeLocked`); Re-Arm nach einem abgebrochenen Rueckkehrversuch nur bei
  neuer Evidenzgeneration (geaenderte Kompatibilitaetsrevision oder
  zwischenzeitlicher erneuter Produktausfall), kein unbegrenztes Wiederholen
- Heizen, Neutralbereich, Kuehlen und Richtungswechsel
- Stromunterbrechung in jeder Prozessphase
- fehlende NTP-Zeit ohne erfundenen Fortschritt
- spaeterer NTP-Abgleich mit Ausfallintervall
- Zeitintervall innerhalb einer Phase
- Zeitintervall ueber einer Abschluss- oder Haltegrenze
- persistierte Verriegelung plus Neustart -> `SAFE_BOOT`
- wiederholter Watchdog oder Bootschleife -> `SAFE_BOOT`
- unvollstaendige Persistenztransaktion -> `SAFE_BOOT`
- kritischer Persistenzschreibfehler -> sofortige Aktorsperre, sichere
  Abschaltung und RAM-Latch
- erfolgreicher minimaler Persistenzfehler-Latch -> Neustart bleibt verriegelt
- fehlgeschlagener minimaler Latch-Schreibversuch -> keine Fortsetzung in
  derselben Laufzeit und beim naechsten unklaren Boot `SAFE_BOOT`
- Recoveryfreigabe ohne bestandene Lesen-Schreiben-Pruefung oder ohne verifizierte
  neue Recoveryrevision wird abgelehnt
- verfruehter Latch-Reset ausserhalb des Serviceablaufs oder vor bestandener
  Speicherpruefung wird abgelehnt
- korrupter Kontrollpunkt mit sicherem Rueckfall
- `COMPLETED` bleibt nach Neustart `COMPLETED`
- kein Service- oder Aktortest aus `SAFE_BOOT`
- Quittierung ohne Fehlerreset
- Benutzerentscheidung bei `WARNING_REQUIRES_ACTION`
- Touchnavigation ohne Wischgeste, sichtbares Pressfeedback, keine
  Doppelausloesung und erster Wake-Touch ohne Command
- SAFE_BOOT mit reduziertem aktorfreiem Diagnose-/Recoveryzugang, getrennt von
  normalem PIN-Service und von Raw-Touch-Kalibrierungsrecovery

Die Simulation prueft erwartete Zustaende, Meldungen, Revisionen und abstrakte
Aktorbefehle. Eine verbotene Aktorfreigabe laesst den Test fehlschlagen.

### Historischer PR-#107-R2-Ausfuehrungsnachweis

Die folgenden Ergebnisse wurden auf einem frueheren PR-HEAD tatsaechlich
ausgefuehrt. Sie bleiben als technische Historie erhalten, sind gegen die
aktuelle Spezifikationspruefung aber **NOT_ACCEPTED_PENDING_R3**. Insbesondere
beweisen sie keine Freigabe der damals getesteten R2-Semantik.

| Bereich | Ausfuehrung | Akzeptanzstatus | Nachweis |
|---|---|---|---|
| Faultkern-/Restart-/Recovery-Suite | ausgefuehrt, 20/20 | `NOT_ACCEPTED_PENDING_R3` | `pio test -e native --filter test_issue24_safety` |
| #23-Safety-Gate und Planner | ausgefuehrt, 44/44 | `NOT_ACCEPTED_PENDING_R3` | `pio test -e native --filter test_actuator_planner` plus Issue-24-Integrationstest |
| #15-Command-/Faultprojektion | ausgefuehrt, 43/43 | `NOT_ACCEPTED_PENDING_R3` | `pio test -e native --filter test_run_commands` |
| Prozessautomat und SAFE_BOOT-Topologie | ausgefuehrt, 35/35 | `NOT_ACCEPTED_PENDING_R3` | `pio test -e native --filter test_process_state_machine` |
| #23-Persistenz-/Application-Pfad | ausgefuehrt, 112/112 | `NOT_ACCEPTED_PENDING_R3` | `pio test -e native --filter test_run_persistence_coordinator` |
| #56/#57-Producer-Tests | ausgefuehrt, 37/37 und 40/40 | `NOT_ACCEPTED_PENDING_R3` | `pio test -e native --filter test_configuration_recovery_service`; `pio test -e native --filter test_configuration_service` |
| Architektur, Secret, Format und Whitespace | damals ausgefuehrt | `NOT_ACCEPTED_PENDING_R3` | Architekturguard, Secret-Scan, clang-format dry-run, `git diff --check` |
| vollstaendiger nativer Lauf, ESP-IDF, CI, Hardware | nicht ausgefuehrt | `NOT_RUN` | Draft-/Owner-Gates |

### Aktueller R3-Korrektur- und Implementierungsnachweis

Die folgenden gezielten Draft-Laeufe pruefen den Korrekturstand gegen den
freigegebenen Plan `48f343ceb49d5a80239702241ae1fbf7d4ebfcd2`. Die direkten
nativen Host-Laeufe wurden als Unity-Link ohne PlatformIO ausgefuehrt, weil
PlatformIO im Draft-Umfeld bei `Platform Manager: Installing native` blockiert.
Der PlatformIO-Lauf ist deshalb kein PASS-Nachweis:

| Bereich | Ausfuehrung | Status | Nachweis |
|---|---|---|---|
| Issue-24-Safetykern, Record, Restart, Injection und Application-Grenze | 29/29 | `PASS` | direkter nativer Unity-Host-Link; PlatformIO gezielt `BLOCKED` bei `Platform Manager: Installing native` |
| #15-Command-/Faultprojektion | 43/43 | `PASS` | direkter nativer Unity-Host-Link |
| #23-Safety-Gate und Planner | 44/44 | `PASS` | direkter nativer Unity-Host-Link |
| #23-Persistenz-/Application-Pfad | 113/113 | `PASS` | direkter nativer Unity-Host-Link; SAFE_BOOT-Aktor-Evidenz ueber den realen Planner-/Sink-Orchestrator |
| reale #56/#57-Recovery-/Service-Konsumenten | 37/37 bzw. 40/40 | `PASS` | direkter nativer Unity-Host-Link |
| Architektur, Secret, Format, Whitespace und Gate-Selbsttest | ausgefuehrt | `PASS` | Architekturguard, Secret-Scan, clang-format dry-run, `git diff --check`, `selftest_quality_gates.py` |
| vollstaendiger nativer Lauf, ESP-IDF, Remote-CI, Hardware | nicht ausgefuehrt | `NOT_RUN` | Draft-/Owner-Gates; Remote-CI im Draft `SKIPPED` |

### R3-Zielorakel fuer Issue #24 (gezielt PASS; Gesamtgate NOT_RUN)

Die gezielten Orakel der Korrekturrunde sind auf dem aktuellen Arbeitsstand
ausgefuehrt. Das ersetzt weder das Ownerreview noch den vollstaendigen
nativen/ESP-IDF-/Hardware-Gesamtlauf.

- Die finale `P1-*`-, `O2-*`-, `S3-*`- und `Y4-*`-Matrix in
  `SAFETY_AND_FAULTS.md` wird vollstaendig gegen Producer, Sofortreaktion,
  Latch, Auto-Rearm, Berechtigung, Reboot-/SAFE_BOOT-Policy und
  Primaer-/Folgebezug geprueft. Unknown/Unresolved bleibt `Y4-008` und
  fail-closed. Jede Zeile ist entweder an einen heute oeffentlichen Producer,
  eine deterministische #24-interne Ursache oder einen stabilen
  Release-1-Contract-/Injection-Code gebunden; nicht vereinbarte
  Zukunftsfunktionen sind nicht enthalten. Die Matrix enthaelt 21 stabile
  Codes: 1 P1-, 2 O2-, 9 S3- und 9 Y4-Codes.
- Neun unabhaengige S3- und acht variable Y4-Latches koexistieren; der neunte
  Y4-Zustand `Y4-006` ist ein Basisrecord-Marker ohne Slot. Cleared-Historie
  zaehlt nicht als aktive Latchkapazitaet. Die Slot-Bound `17`, die neue
  Payload `1.216 Byte`, der neue Envelope-Record `1.253 Byte` und das bewusst
  gewaehlte application-spezifische Limit `2.048 Byte` werden statisch und
  dynamisch geprueft.
- Bei voll belegten aktiven Slots wird kein Latch evicted. Eine weitere
  unabhaengige Safetyursache setzt den Basisrecord-Overflowmarker ausserhalb
  der Slotliste, bleibt im RAM sofort fail-closed und fuehrt bei erfolgreichem
  Marker-Commit deterministisch zu `SAFE_BOOT`; ein Marker-Schreib- oder
  Readbackfehler bleibt ebenfalls fail-closed.
- S3-004 und danach S3-005 bleiben gleichzeitig aktiv nachvollziehbar; eine
  Recovery loescht S3-004 nicht. Die firmwarefeste Recoveryobergrenze `<=2`
  und der zulaessige Bereich `0..2` werden erhalten.
- `Y4-006` wird nicht durch Quittierung, Reboot oder einen isolierten
  erfolgreichen Write geloescht. Ein separater technisch autorisierter
  Marker-Recoverypfad verlangt aktuelle Read-, Write-/Readback-, Capacity- und
  Integritaetsevidenz und loescht den Marker erst nach bestaetigtem Commit.
- Restart-Evidence wird beim Boot gegen Intent, Faultinstanz, Faultrevision,
  Episode und Recordrevision gebunden. Stale-, fehlende, abgelehnte oder
  `OutcomeUnknown`-Evidence autorisiert keinen spaeteren unabhaengigen
  SoftwareRestart und bleibt fail-closed.
- SAFE_BOOT-Exit verlangt nach der einmaligen Restartbeobachtung neben
  technischer Autorisierung und qualifiziertem Configuration Gate aktuelle,
  stale-sichere Sensor-, Aktor-, Persistenz- und Integritaetsevidenz. Ein
  normaler Reboot, fehlende Evidence oder ein Y4-006-Marker beendet SAFE_BOOT
  nicht. Der positive Aktornachweis kommt im Produktionspfad ausschliesslich
  aus dem privaten Planner-/Sink-Handoff; unkonfigurierter, invalider oder
  watchdog-belasteter Output bleibt fail-closed.
- O2-002 wird pro Sicherheits-Sensorrolle korreliert aufgeloest; eine gueltige
  Rueckkehr von Schrankluft beendet nicht den weiterhin aktiven Kuehlkoerper-
  Fault. Die In-Memory-Bound umfasst deshalb 17 persistente Faults plus P1-001,
  O2-001 und zwei gleichzeitige O2-002-Rollen.
- Positive Fault-Reset-Safetyfelder sind kein Produktionsparameter mehr. Die
  SafetyFaultService bindet nur intern verwaltete, producerbezogene
  Evidenceprojektionen an Faultinstanz und Revision; der native Test nutzt dafuer
  einen explizit markierten Injection-Seam.
- Der Y4-006-Basisrecord persistiert eine bounded Fehlerart ohne Aenderung der
  128-Byte-Basis, des 1.216-Byte-Payloads oder des 1.253-Byte-Envelopes.
  Akzeptierte und abgelehnte Marker-Recoveryentscheidungen werden ueber den
  bestehenden `IEventJournal` projiziert; ein Journalfehler veraendert den
  Safetycommit nicht.

### Fault/Sensor-Injektionen

- `ProcessMessage::TargetReachTimeExceeded` aus dem bestehenden Prozessautomaten
  mit `ProcessRunSnapshot::maximumTargetReachMinutes`; keine neue #24-Zeitlogik;
- `AirLimitReduced` und `AirLimitBlocked` als normale #22-Regelbegrenzungen
  erzeugen allein weder einen P1- noch einen O2-Fault;
- Produktfuehler O2/Fallback gemaess #21;
- #22 `NoCommissioning`, `SensorUnavailable`, `InvalidConfiguration`,
  `InvalidSample`, `TimeInvalid` und `RequestIdentityExhausted` werden
  reason-spezifisch projiziert; #23 `NoValidRequest` allein erzeugt keinen
  O2-Fault, sondern bleibt die sichere Plannerklassifikation;
- Schrankluft `FAILED` -> `S3-001`;
- Kuehlkoerpersensor `FAILED` -> `S3-002`;
- `ThermalCompatibility::Incompatible` bei ansonsten gueltiger #21-Evidenz
  mit gueltiger Revision -> bestehende `ReturnValidationAborted`-/
  `AirFallbackActive`-Rueckkehrlogik; kein `S3-003`-Latch allein aus diesem
  Enum;
- explizite simulierte Ursache `persistent/safety-relevant sensor
  contradiction` -> `S3-003`, persistenter Latch, `ImmediateStop`, kein
  Auto-Rearm und Reset erst nach Ursachefreiheit und den vorgesehenen Checks;
- thermische Eingriffsgrenze -> `S3-004` und ohne #35 keine aktive Recovery;
- harte Notgrenze nach bestehendem S3-004-Latch -> beide Latches bleiben aktiv.

### Aktor-Injektionen

- #23 `ActuatorWatchdogFaultEvidence` -> `S3-008`;
- reproduzierbare Contract-/Injection-Cases fuer `S3-006`/`S3-007` und
  `S3-009`; diese Tests simulieren die spaeteren Producer und behaupten keine
  bereits implementierte reale Fan-, H-Bruecken-, Strom- oder
  Ausgangsdiagnose;
- Planner-/Sink-Bypassversuch -> keine Aktorfreigabe.

### Persistenz-Injektionen

- `WriteError`, `CapacityError`, `ReadError` und Korruption;
- `CommitOutcomeUnknown` mit Readback des neuen Stands, altem Stand,
  Readbackfehler und Mismatch;
- alle 17 aktiven Slots plus neue unabhaengige Safetyursache: kein Evict,
  persistenter Basis-Overflowmarker ausserhalb der Slots und `SAFE_BOOT`;
- RAM-Latch bleibt bei fehlgeschlagenem Safetywrite;
- ein spaeter isolierter erfolgreicher Write ist keine Entwarnung.

### Restart-/Brownout-Injektionen

- `PowerOn`, passender `SoftwareRestart` mit Application-Evidence,
  `WatchdogOrPanic`, `Brownout`, `ExternalOrOther` und `Unknown`;
- dieselbe bootlokale Observation wird nicht mehrfach als neuer Restart gezaehlt;
- dieselbe automatische Recoveryursache darf hoechstens einen kontrollierten
  Restart ausloesen; ein zweiter Versuch bleibt aus;
- dritter abnormaler Restart -> `SAFE_BOOT`;
- 29:59, 30:00 und >30:00 stabile monotone Laufzeit;
- normaler Reboot und Power-off schliessen die Episode nicht;
- abnormaler Restart waehrend der Stabilitaetsphase startet die Bewertung neu;
- normaler Reboot verlaesst `SAFE_BOOT` nicht; `Y4-009` erzeugt keinen
  generischen zweiten Reboot.

### Reale #56/#57-Application-Grenze

- `ConfigurationRuntimeFailure`;
- nicht aufloesbarer `CommitOutcomeUnknown` beziehungsweise realer
  Commitindeterminate-Status;
- `ConfigurationUnavailable`;
- `ConfigurationIntegrityFailure`;
- echter `FermentationApplication`-Pfad, genau eine zentrale Safetyinstanz,
  echte oeffentliche #56/#57-Resultate und bestehender #23-Planner-/Sinkpfad;
- `Unknown`/`Unresolved` -> niemals `Allowed`.

### Reset-, Journal- und Gate-Injektionen

- neutraler #15-`FaultResetRequest` ohne positive Caller-Safetyentscheidung;
- fehlende oder nicht passende typisierte Autorisierung -> fail-closed;
- codebezogene Reset-/Reboot-/SAFE_BOOT-Entscheidung fuer jeden S3/Y4-Code;
- Fault-, Restart-, Reset- und SAFE_BOOT-Ereignisse ueber das bestehende
  `IEventJournal`;
- Journalfehler darf Safetycommit oder sichere Reaktion nie in `Allowed`
  umdeuten;
- S3-004 ohne #35: keine Recovery, kein PI-/Planner-Bypass, Latch bleibt.

Der historische R2-Abschnitt oberhalb bleibt unveraendert als
`NOT_ACCEPTED_PENDING_R3`. Die gezielt ausgefuehrten R3-Orakel sind oben
ausgewiesen; Gesamt-, Firmware- und Hardwaregates bleiben `NOT_RUN`, der
Draft-CI-Eintrag bleibt `SKIPPED`.

### Ebene 3: Build- und statische Integrationstests

Mindestens:

- `native`, `esp32_bringup` und `esp32_release` bauen reproduzierbar
- reale Zielkonfiguration verwendet 4 MB Flash
- keine PSRAM-Abhaengigkeit
- dokumentierter Partitionsplan ohne Release-1-Web-OTA
- Firmware- und Ressourcenbericht
- Factory-Konfiguration und Schemaversionen
- Deutsch, Spanisch und Englisch
- konfiguriertes Branding, Sprach-/Theme-Pakete und gezielt erzeugte Fontassets
  innerhalb des 4-MB- und ohne-PSRAM-Budgets
- Web- und lokale UI-Ressourcen
- keine eingebetteten Geheimnisse
- keine produktiv verwendeten `TBD`-Werte
- keine unbestaetigten Pins, Pegel oder Controller als freigegebene Werte
- Zukunftsfunktionen bleiben deaktiviert

Ein Build ist keine Hardwarefreigabe.

### Ebene 4: Elektrische und Hardwaretests

Vor einer thermischen Belastung:

- GPIO-Zuordnung und aktive Pegel
- Boot-, Reset-, Brownout- und Bootloaderverhalten
- sichere H-Bruecken- und MOSFET-Zustaende
- BTS7960-Pulldowns, Enable, Richtungen und Abschaltung
- drei DS18B20 mit ROM-Zuordnung
- 1-Wire-Bustopologie und Produkt-Hot-Plug
- Displaycontroller, Touchcontroller, Rotation und Kalibrierung
- Raw-Touch-Kalibrierungsrecovery im 10-Sekunden-Fenster getrennt von
  PIN-unabhaengigem Vollreset; keine physische Bedienannahme
- Innen- und Aussenluefter
- Summer
- R_IS/L_IS nur bei nachgewiesener Nutzbarkeit
- PIN-unabhaengiger lokaler Vollreset ohne Aktorwirkung
- UART-Flash- und Recoveryweg

### Ebene 5: Thermische Inbetriebnahme

Mit leerem Schrank und definierten Testmassen:

- Aufheizen, Abkuehlen und Halten
- Temperaturverteilung
- Produkt-Luft-Differenz
- Kuehlkoerper- und Luefterreaktion
- PI-Parameter je Sensorrolle und Richtung
- Zielband, Qualifikation und Gnadenzeit
- Luftbegrenzungen
- Mindestimpuls, Mindestzeiten und Totzeit
- Sicherheits-Eingriffs- und harte Notgrenzen
- fehlende thermische Reaktion
- thermisches Modell fuer Unterbrechungen, sofern verwendet
- Temperatursicherung: Rating, Montageort und thermische Wirksamkeit

### Ebene 6: Praktische Fermentationslaeufe

Erst nach bestandenen Software-, Hardware- und thermischen Gates:

- Joghurt mild
- Joghurt stichfest
- Milchkefir
- Wasserkefir

Bewertet werden Bedienung, Vorheizen, Zielqualifikation, produkt- und
luftgefuehrter Betrieb, Temperaturverlauf, Abschluss, Kuehlen und Halten. Ein
gelungenes Produkt ersetzt keine technische Sicherheitspruefung.

## Automatische Pruefungen je relevantem PR

1. native Unit-Tests
2. simulierte Prozess- und Fehlerablaeufe
3. Konfigurations- und Schemavalidierung
4. Persistenz-, Transaktions-, Rueckfall- und Migrationspruefungen
5. PlatformIO-Builds
6. Geheimnis- und lokale-Konfigurationspruefung
7. Pruefung auf produktive `TBD`- oder unbestaetigte Hardwarewerte
8. Groessenbericht fuer Firmware und statische Ressourcen

Ein fehlgeschlagener Sicherheits-, Persistenz-, Recovery- oder Kernfunktionstest
blockiert den Merge und das Release.

## Release-Gates

### Gate 0: Spezifikation und Rueckverfolgbarkeit

Vor Implementierungsfreigabe:

- verbindliche Anforderung oder Entscheidung vorhanden
- zugehoerige Testidee vorhanden
- Hardware-, Inbetriebnahme- und Budget-TBDs sichtbar
- Reviewkorrekturen von PR #38 in aktuelle kanonische Fachquellen integriert
- keine sicherheitskritische Annahme als bestaetigte Tatsache

### Gate 1: Softwarekern

Vor realem Aktorbetrieb:

- Zustandsmaschine nativ getestet
- Bootreihenfolge und `SAFE_BOOT` getestet
- `COMPLETED`-Wiederherstellung getestet
- Sensor- und Fehlerlogik getestet
- Aktorfreigabelogik getestet
- Mindestzeiten, Totzeit und Watchdog getestet
- Persistenz, Transaktionsmarker und Rueckfall getestet
- kritischer Schreibfehler sperrt Aktoren und setzt den RAM-Latch
- minimaler persistenter Latch und dessen Fehlerpfad getestet
- Latch-Reset-Gate nach Speicherpruefung getestet
- Recoveryfreigabe verlangt verifizierte neue Revision
- Ausfallintervall und Zeitunsicherheit getestet
- kein Aktortest aus `SAFE_BOOT` erreichbar
- alle sicherheitsrelevanten automatischen Tests bestanden

### Gate 2A: Elektrische Freigabe ohne Peltier

Vor Anschluss beziehungsweise Bestromung des Peltiers:

- GPIOs und aktive Pegel bestaetigt
- sichere Boot-, Reset- und Bootloaderpegel bestaetigt
- BTS7960 ohne Peltier geprueft
- beide Richtungen koennen nie gleichzeitig aktiv sein
- Ausgang und Polaritaet mit Multimeter bestaetigt
- Schrankluft- und Kuehlkoerpersensor bestaetigt
- Aussenluefter und Nachlauf bestaetigt
- 7,5-A-Ueberstromsicherung installiert
- Kuehlkoerper und Waermetauscher montiert
- Servicebericht bis zu diesem Gate gespeichert

### Gate 2B: Erster realer Peltier-Puls

Vor **jedem ersten bestromten Peltier-Puls** muessen zusaetzlich erfuellt sein:

- einmalige Temperatursicherung installiert
- Temperatursicherung auf Durchgang geprueft
- Montageort dokumentiert
- Rating innerhalb der aktuellen Inbetriebnahmerevision freigegeben
- Aussenluefter unmittelbar zuvor erfolgreich getestet
- Pflichtsensoren aktuell `VALID`
- kein Fehler, keine Verriegelung und kein `SAFE_BOOT`
- validiertes `STANDBY` und PIN-geschuetzter Serviceablauf
- Leistung und Dauer firmwarefest begrenzt
- grosser jederzeit wirksamer Abbruch

Fehlt eine dieser Voraussetzungen, bleibt das Peltier spannungslos. Die
Temperatursicherung darf nicht erst nach ersten Pulsen nachgeruestet werden.

Nach dem Heizpuls folgen Peltier AUS, Nachlauf, Mindest-Ausschaltzeit und Totzeit,
bevor ein begrenzter Kuehlpuls erlaubt ist.

### Gate 3: Thermische Inbetriebnahme

Vor echten Fermentationslaeufen:

- leerer Schrank sowie kleine und grosse Testmasse vermessen
- Heizen, Kuehlen und Richtungswechsel abgestimmt
- Luftbegrenzungen festgelegt
- Sicherheits-Eingriffs- und harte Notgrenzen validiert
- Temperaturverteilung und kritischste Stellen bestimmt
- Temperatursicherung thermisch bewertet und dokumentiert
- keine unbekannte sicherheitsrelevante thermische Abweichung

### Gate 4: Dauer- und Belastungstest

Vor Release 1:

- mindestens sieben zusammenhaengende Tage
- keine unerklaerten Resets, Watchdogs oder Brownouts
- keine unerlaubte Aktorfreigabe
- keine relevante fortschreitende RAM-Leckage
- Speicherbereinigung innerhalb der Budgets
- kritische Persistenz und Sperren nach Unterbrechung wiederherstellbar
- Web, Display, Sensoren, Exporte und Regelung parallel stabil
- Fehler- und Resetjournal innerhalb des Budgets

### Gate 5: Releasekandidat

- Standardprogramme praktisch geprueft
- lokale und Webbedienung geprueft
- Stromunterbrechungs- und Recoveryablaeufe geprueft
- Exporte und Diagnose geprueft
- bekannte Abweichungen bewertet
- keine offene unbekannte Sicherheitsursache
- alle sicherheits- und kernfunktionsrelevanten Tests `PASS`

## Verpflichtende Fehlerinjektionen

### Sensoren

- Schrankluftfuehler in Standby, Vorheizen, Fermentation und Kuehlen ausfallen lassen
- Produktfuehler entfernen, Fallback und Rueckkehr pruefen
- Kuehlkoerpersensor im Peltierbetrieb ausfallen lassen
- CRC-Fehler, Busunterbrechung, `STALE` und unrealistische Spruenge
- widerspruechliche Produkt-, Luft- und Kuehlkoerperwerte

### Aktoren und Thermik

- veraltete Regelanforderung
- gleichzeitige Richtungsanforderung
- kurze und dauerhafte Gegenanforderung
- Mindest-Ausschaltzeit und Totzeit
- Aussen- und Innenluefterfehler
- fehlende thermische Peltierreaktion
- Sicherheits-Eingriffsgrenze und harte Notgrenze
- Abbruch eines Servicepulses
- Peltier-Test ohne Temperatursicherung muss blockiert werden
- Aktortest aus `SAFE_BOOT` muss blockiert werden

### Versorgung, Zeit und Boot

- Unterbrechung in jeder wesentlichen Phase
- Brownout und wiederholte Brownouts
- Watchdog und Bootschleife bis `SAFE_BOOT`
- Neustart mit persistierter Sicherheitsverriegelung
- Neustart mit persistiertem `COMPLETED`
- fehlende NTP-Zeit
- spaeterer NTP-Abgleich
- Ausfallintervall innerhalb und ueber einer Phasengrenze
- WLAN-Ausfall bei weiterlaufendem sicheren Prozess

### Persistenz und Speicher

- neuesten Kontrollpunkt beschaedigen
- neueste Konfigurationsrevision beschaedigen
- Rueckfallrevision pruefen
- Unterbrechung waehrend kritischem Schreibvorgang
- kritischen Schreibfehler bei aktiver Aktoranforderung injizieren und sofortige
  Sperre vor einem weiteren Aktorbefehl nachweisen
- unvollstaendigen Transaktionsmarker hinterlassen
- kritischen Speicher nicht lesbar oder nicht schreibbar simulieren
- Persistenzfehler-Latch setzen und Neustart ausfuehren
- Schreiben des minimalen persistenten Latches zusaetzlich fehlschlagen lassen;
  RAM-Latch und fail-closed-Verhalten muessen bestehen bleiben
- Recoveryentscheidung erfolgreich schreiben, aber Ruecklesen beziehungsweise
  Verifikation fehlschlagen lassen; Aktorfreigabe bleibt gesperrt
- Latch-Reset vor Speicherpruefung, ausserhalb des Serviceablaufs und bei
  verbleibendem Transaktionsmarker ablehnen
- Latch-Reset nach bestandener Lesen-Schreiben-Pruefung und dokumentiertem
  Serviceereignis zulassen
- Historienspeicher bis zur Bereinigung fuellen
- nichtkritischen RAM- oder Exportfehler erzeugen

### Bedienung und Berechtigungen

- Quittieren ohne Fehlerreset
- Resetversuch bei bestehender Ursache
- Servicefunktion ohne PIN
- Aktortest waehrend Lauf und `SAFE_BOOT`
- konfliktierende Display- und Webaktion
- alle Stopoptionen
- vergessene Service-PIN mit lokalem PIN-unabhaengigem Vollreset
- Versuch eines isolierten PIN-Resets muss abgelehnt werden

## Hardware-Abnahme

Jeder relevante Hardwarestand dokumentiert mindestens:

1. Hardwarekennung und Platinenrevision
2. Verdrahtungsreferenz und Fotos
3. Versorgungsspannungen
4. GPIOs, aktive Pegel und Bootverhalten
5. BTS7960-Enable, Richtungen und Abschaltung
6. Innen- und Aussenluefter inklusive Nachlauf
7. Summer
8. drei DS18B20 mit Rolle und ROM-Adresse
9. Bustopologie und Produkt-Hot-Plug
10. Display, Touch und Kalibrierung
11. 7,5-A-Sicherung und Leitungsquerschnitte
12. Temperatursicherung vor dem ersten Puls: Typ, Rating, Montageort und
    Durchgangspruefung
13. Peltierstrom, Heiz- und Kuehlrichtung
14. begrenzter Heiztest
15. Mindest-Ausschaltzeit und Totzeit
16. begrenzter Kuehltest
17. thermische Reaktion
18. gespeicherter Servicebericht
19. Abweichungen und Freigabestatus

Eine Aenderung an Leistungspfad, Sensorbussen, Lueftern, Temperatursicherung,
Controllerboard oder Peltier kann eine neue Teil- oder Vollabnahme verlangen.

## Siebentaegiger Dauer- und Belastungstest

Belastungsprofil:

- kontinuierliche Sensorerfassung
- Displaybetrieb und wiederholte Bedienung
- parallele Webzugriffe und Live-Aktualisierung
- wiederholte Exporte
- periodische Kontrollpunkte und Bereinigung
- mehrere Heiz-, Kuehl- und Richtungswechsel
- WLAN- und NTP-Ausfall
- mindestens eine kontrollierte Stromunterbrechung
- Meldungen, Quittierungen und Diagnoseabrufe

Aufzuzeichnen:

- freier und niedrigster Heap
- groesster zusammenhaengender Block
- Task-, Watchdog- und Resetereignisse
- Sensor- und Busfehler
- Regler- und Aktorereignisse
- Flash- und Historienbelegung
- Bereinigungen und Schreibfehler
- Web- und Exportfehler
- Temperaturstabilitaet und Richtungswechsel

Der Test besteht nur ohne unerlaubte Aktorfreigabe, unerklaerten Reset,
unbehandelten Watchdog, relevante RAM-Leckage, verlorene kritische Persistenz
oder unbekannte Sicherheitsabweichung.

## Testnachweis

Jeder formelle Test enthaelt:

```text
Test-ID
Titel
Anforderung / Entscheidung / Fehlercode
Testebene
Voraussetzungen
Hardwareversion
Firmwareversion und Commit
Konfigurations- und Tuningrevision
Testdaten und Referenzgeraete
Testschritte
erwartetes Ergebnis
gemessenes Ergebnis
Logs, Exporte, Bilder oder Servicebericht
PASS / FAILED / BLOCKED / NOT_RUN
Abweichungen
verantwortliche Person
Datum und Zeitbasis
```

Test-ID-Gruppen:

```text
UT-xxx    Native Unit-Tests
SIM-xxx   Simulation und Zustandsmaschine
BLD-xxx   Build und statische Integration
HW-xxx    Hardware und elektrische Abnahme
TH-xxx    Thermische Inbetriebnahme
FI-xxx    Fehlerinjektion
END-xxx   Dauer- und Belastungstest
FER-xxx   Praktische Fermentationslaeufe
REL-xxx   Release-Gates
```

`PASS_WITH_WARNINGS` kann in Serviceberichten vorkommen, ersetzt bei einem
formellen Gate aber kein `PASS`, wenn die Warnung eine Gate-Anforderung betrifft.

## Akzeptierte Entscheidungen

- [x] sechs Testebenen
- [x] automatische native, simulierte, Persistenz- und ESP32-Buildtests
- [x] sicherheits- und kernfunktionsrelevante Tests muessen bestanden sein
- [x] verpflichtende Fehlerinjektionen
- [x] dokumentierte Abnahme jedes relevanten Hardwarestands
- [x] Temperatursicherung vor dem ersten realen Peltier-Puls
- [x] `SAFE_BOOT` bleibt aktorfrei
- [x] Boot bewertet Verriegelungen und Persistenz vor Recovery
- [x] Ausfallzeit wird als Intervall getestet
- [x] `COMPLETED` wird nach Neustart wiederhergestellt
- [x] kritischer Persistenzfehler sperrt Aktoren und setzt RAM-/Persistenz-Latch
- [x] fehlgeschlagener minimaler Latch-Schreibversuch bleibt fail-closed
- [x] Latch-Reset nur nach bestandenem Service- und Speicher-Gate
- [x] PIN-unabhaengiger lokaler Vollreset wird getestet
- [x] mindestens siebentaegiger Dauer- und Belastungstest
- [x] formeller versionierter Testnachweis
