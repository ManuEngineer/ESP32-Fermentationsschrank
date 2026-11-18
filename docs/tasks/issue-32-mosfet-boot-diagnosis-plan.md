# Issue #32 – Boot-/Resetbefund der Onboard-MOSFET-Ausgaenge

Status: Ownerfreigabe fuer die Planfassung auf Commit
`67f40f82b875fb4864247895971aee9fa1f4c180`. Die dort festgelegten Grenzen und
separaten Ownerentscheidungen gelten weiter; diese Status- und
Beobachtungskorrektur erweitert den freigegebenen Scope nicht. Ausgangsbasis `main`
`9233226da3cf624eeefce468c219dafc0653caae`, Live-Issue #32 OPEN.
Der bisherige Softwareplan `issue-32-onboard-mosfet-outputs-plan.md`,
freigegeben auf `2c1d32b`, betrifft S1–S3 und autorisiert keine neuen
Polaritaets- oder Hardwareentscheidungen. PR #195 ist gemergt; die reale
Abnahme bleibt offen. Die Roadmap fuehrt #190 vor #32; dieser gezielte
Safety-Befund aendert weder diese Reihenfolge noch den Scope von #190.

## Ziel und Grenzen

Alle tatsaechlich auffaelligen physisch vorhandenen MOSFET-Kanaele und ihre
Verbraucher zeitlich und kanalbezogen untersuchen, die Firmwareidentitaet und Ursache soweit
moeglich belegen und eine minimale Massnahme fuer sichere Inaktivitaet bei
Power-On, Reset und Standby bestimmen. Keine Polaritaet oder Beschaltung raten.
Keine Peltier-Leistung, BTS7960-Freigabe, Planneranbindung, neue Pinliste,
neue Ausgangsabstraktion oder Wegwerf-Testfirmware. GPIO27 bleibt
softwareseitige Reserve und wird nicht zur Aktorsteuerung freigegeben; der
moeglicherweise betroffene physische MOSFET-4-Ausgang ist Teil der Diagnose.
`ACTUATOR_RELEASE=NO`. Allgemeine elektrische Pegelmessungen bleiben gemaess
ADR-002 nicht pauschal vorgeschrieben; eine konkret fuer die Ursachenfindung
erforderliche Messung bedarf geeigneter Mittel und Ownerentscheidung.

## Ausgangsbefund und Quellen

Der Owner bestaetigt laufende Luefter an den physischen MOSFET-Ausgaengen 1
und 4; 2 und 3 sind derzeit ohne Last. Die GPIO-Zuordnung dieser Ausgaenge ist
unbestaetigt; Klemmen und Zeitfenster sind nicht vollstaendig verifiziert.
GPIO27 bleibt softwareseitige Reserve. Die installierte
Firmware ist nicht verifiziert. Board 12 V; USB am Board nicht zugaenglich;
Owner bestaetigt `/dev/ttyUSB0` als verbundenen FTDI/UART-Zugang; BTS7960
ohne 12-V-Leistung und Peltier. Die externen 10-kOhm-Pulldowns von GPIO13/14/25
belegen nichts ueber die internen MOSFET-Gates. Eine fruehere Evidence
(`docs/audits/ISSUE192_FACTORY_RESET_HW_20261010_EVIDENCE.md`) dokumentiert
einen Reset beim Oeffnen des Ports; keine unkoordinierte Portoeffnung.

Boardprofil `config/board_profiles/esp32_32e_quad_mosfet_r1.yaml` und
generierter Header fuehren GPIO16/17/26 mit unbestaetigter Polaritaet; GPIO27
ist Reserve. `EspIdfBinaryOutputSink::begin()` fuehrt bei `Unconfirmed` keine
GPIO-Operation aus; `app_main` bindet die Ausgaenge nicht an den Planner.
Ein produktiver Aufrufer von `setEnabled(true)` ist dort nicht vorhanden;
ein anderer geeigneter, sicherer und autorisierter bestehender Aufrufpfad ist
bislang nicht nachgewiesen. Aktive AUS/EIN-Pruefungen sind daher `NOT_RUN`.
Dies beschreibt den Quellstand, nicht das installierte Image oder eine
physische Inaktivitaet. Das Lieferanten-PDF
`references/datasheets/esp32-32e-quad-mosfet-board-supplier.pdf` nennt auf
Seite 1 N-Kanal-MOSFETs, beweist damit aber weder Gatebias noch Polaritaet der
konkreten Platine. Relevante Quellen: `docs/HARDWARE.md`, `docs/OPEN_POINTS.md`,
`docs/SPECIFICATION_REVIEW.md`, ADR-002/013 in `docs/DECISIONS.md`,
`docs/ENGINEERING_PRINCIPLES.md`, `docs/CI_AND_QUALITY_GATES.md`, lokales
`lib/device_platform_esp_idf/AGENTS.md`, bestehende #32-Tests und Generator.

## Vorgehen und Gates

1. Owner bestaetigt sicheren Aufbau, konkreten UART-Zugang und Verbraucher-/
   Klemmenzuordnung. Vor Verkabelungsaenderungen Versorgung abschalten; keine
   unbeaufsichtigten Schalt- oder Resetaktionen. Bestehende Diagnosezugriffe
   nur lesend und ohne automatisches DTR/RTS-Reset verwenden. Bootlog nur mit
   koordiniertem Power-On/Reset sichern; Geheimnisse aus Evidence entfernen.
   Fuer jeden auffaelligen physischen Kanal Kanalnummer, Klemmen, geschaltete
   Last, exakte GPIO-Zuordnung und Zustand je Bootphase getrennt erfassen;
   Quelle, Zeitpunkt und Beobachtung vor `app_main`, waehrend Boot, bei Reset
   und im stabilen Standby dokumentieren. Ohne passenden Zugang
   oder Beobachtung `NOT_VERIFIED` statt eines Schlusses aus `main`.
2. Installiertes Image/Buildprofil anhand vorhandener zuverlaessiger
   Identitaetsmerkmale, vorhandener Flash-/Build-Evidence und gegebenenfalls
   sicherer, lesender Readback-Methode verifizieren; Tool, Resetwirkung und
   Zugang zuvor klaeren. Ein Logstring allein beweist keinen vollstaendigen
   Imagehash. Keine Flash-, NVS- oder Konfigurationsaenderung fuer Diagnose.
3. Referenzbilder und verfuegbare Schaltunterlagen auf konkreten Kanalpfad,
   Gate-/Treiberbias und Klemmen untersuchen und gegen reale Boardrevision
   abgleichen; Quellen und Grenzen angeben. GPIO-Belegung und alle erreichten
   Firmware-/Initialisierungspfade einschliesslich IDF-Konfiguration anhand
   des identifizierten Imagequellstands pruefen. Ohne Identitaet ist ein
   Ausschluss fuer die reale Firmware `NOT_VERIFIED`.
4. Klassifikation (a) Imageabweichung, (b) Softwarepolaritaet/Initialisierung,
   (c) vor Firmwarestart aktive Stufe jeweils mit Belegen oder `NOT_PROVEN`;
   Kombinationen bleiben moeglich. Vor `app_main` kann `begin()` nicht wirken;
   ein Spaetabschalten belegt kein Boot-Fail-Closed. Eine benoetigte
   Hardwarekorrektur mit konkreter Evidenz und Alternativen dem Owner als
   Entscheidung vorlegen. Neue Erkenntnisse, die Eingriff oder Teststrategie
   materiell bestimmen, erfordern eine vollstaendige Planrevision und erneute
   Freigabe der exakten SHA vor der Umsetzung.
5. Erst bei belegter Ursache, bestaetigter Polaritaet/Ruhelage und freigegebenem
   Eingriff kleinsten belegten Delta in bestehender Boardprofil-SSOT, bestehendem
   Headergenerator und gegebenenfalls Adapter/Composition Root umsetzen;
   vorhandene Tests erweitern. Keine angenommene Sicherheit im Resetfenster.
   Konkrete Aenderungspfade und Ownerentscheid im PR festhalten.
6. Gezielt Generatorpruefung und unmittelbar betroffene bestehende Hosttests
   nach einer Aenderung ausfuehren; Kommandos, Ergebnis und HEAD dokumentieren.
   Gesicherte Beobachtung von Power-On, ESP32-Reset und stabilem Standby sowie
   Image-/Quellstandszuordnung und ursachenbezogene Diagnose sind nach
   Planfreigabe mit abgestimmtem sicherem Aufbau durchfuehrbar. Fuer alle
   auffaelligen physischen Kanaele gegebenenfalls sicher angeschlossenen
   Einzelpruefverbraucher und Klemmen-/Kanalzuordnung dokumentieren.
   Beobachtung muss den Zeitraum vor `app_main` einschliessen;
   ungeeignete zeitliche Aufloesung oder fehlender Zugriff ist `NOT_PROVEN`,
   nicht PASS. Aktive Ausgaenge im geforderten AUS-Fenster sind FAIL.
   Aktive AUS/EIN-Funktionspruefungen bleiben ohne nachgewiesenen sicheren,
   vorhandenen und autorisierten Aufrufpfad `NOT_RUN`. Falls ein solcher Pfad
   festgestellt wird, vor Verwendung konkret mit Quelle und Autorisierung
   dokumentieren. Falls nicht, einen kleinsten produktionsnahen Pruefpfad als
   spaetere, separat konkret zu planende Ownerentscheidung vorlegen; keine
   pauschale Planner- oder Peltierfreigabe und keine Wegwerf-Testfirmware.
   `SSOT_CONFORMANCE` und `FUNCTIONAL_HARDWARE_VERIFICATION` nur fuer wirklich
   nachgewiesenen Umfang aktualisieren; restliche #32-Abnahmekriterien bleiben
   offen. Kein Pre-Ready-Lauf ohne dessen getrennte Ownerfreigabe und Review.

Evidence dokumentiert Image-/Quell-SHA, Testaufbau, Datenquelle und getrennte
PASS/FAIL/NOT_RUN/NOT_VERIFIED je Nachweis, offene Risiken und Ownerentscheide.
Vor Sessionende kanonischen PR-Handover pflegen. Materielle Planrevisionen
erfordern vor Umsetzung die Freigabe der exakten neuen Plan-SHA.
