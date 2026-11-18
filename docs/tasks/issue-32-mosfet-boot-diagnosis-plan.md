# Issue #32 – Boot-/Resetbefund der Onboard-MOSFET-Ausgaenge

Status: Ownerfreigabe fuer die Planfassung auf Commit
`67f40f82b875fb4864247895971aee9fa1f4c180`. Die dort festgelegten Grenzen und
separaten Ownerentscheidungen gelten weiter, soweit sie durch die unten
ausdruecklich dokumentierte R1-Luefterentscheidung nicht angepasst sind.
Revision fuer die R1-Vertraege und den GPIO16-SSOT-/Firmware-Schnitt:
**Freigabe dieser neuen
Plan-Commit-SHA ausstehend**; die bisherige SHA gibt diesen Schnitt nicht frei. Ausgangsbasis `main`
`9233226da3cf624eeefce468c219dafc0653caae`, Live-Issue #32 OPEN.
Der bisherige Softwareplan `issue-32-onboard-mosfet-outputs-plan.md`,
freigegeben auf `2c1d32b`, betrifft S1–S3 und autorisiert keine neuen
Polaritaets- oder Hardwareentscheidungen. PR #195 ist gemergt; die reale
Abnahme bleibt offen. Die Roadmap fuehrt #190 vor #32; dieser gezielte
Safety-Befund aendert weder diese Reihenfolge noch den Scope von #190.

## Ziel und Grenzen

Die beobachteten Luefterkanaele untersuchen und OUT1 nach App-Start im
normalen Standby ohne Aktorfreigabe AUS erreichen. Der Owner hat fuer R1 am
vorhandenen MOS-X4-Aufbau Luefter-EIN beim Boot und gehaltenem EN-Reset
akzeptiert; Luefter-AUS in diesen beiden Phasen ist keine Pflicht dieses
Scopes. Keine Polaritaet oder Beschaltung raten.
Keine Peltier-Leistung, BTS7960-Freigabe, Planneranbindung, neue Pinliste,
neue Ausgangsabstraktion oder Wegwerf-Testfirmware. GPIO27 bleibt
softwareseitige Reserve und wird nicht zur Aktorsteuerung freigegeben; der
moeglicherweise betroffene physische MOSFET-4-Ausgang ist Teil der Diagnose.
`ACTUATOR_RELEASE=NO`. Allgemeine elektrische Pegelmessungen bleiben gemaess
ADR-002 nicht pauschal vorgeschrieben; eine konkret fuer die Ursachenfindung
erforderliche Messung bedarf geeigneter Mittel und Ownerentscheidung.

## Ausgangsbefund und Quellen

Der Owner bestaetigt Luefter an den physischen MOSFET-Ausgaengen 1 und 4;
2 und 3 sind derzeit ohne Last. GPIO16-LOW schaltet OUT1 und GPIO27-LOW OUT4
im jeweils beobachteten ROM-Test AUS; HIGH/EIN und vollstaendige Kanal- und
Zeitverifikation bleiben offen.
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

Die bereits erfolgten LOW-/UART-Nachweise stehen in der Evidence und werden
fuer den GPIO16-Schnitt nicht erneut verlangt. Die folgenden allgemeinen
Diagnoseschritte gelten nur soweit noch erforderlich und nicht im Widerspruch
zu der konkretisierten R1-Ownerentscheidung und dem unten begrenzten Schnitt.

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
   ein Spaetabschalten belegt kein physisches Luefter-AUS im Boot-/Resetfenster.
   Ein Luefter-AUS in diesen Phasen ist aufgrund der unten dokumentierten
   R1-Ownerentscheidung nicht verpflichtend. Benoetigte Hardwarekorrekturen
   fuer andere, unveraenderte Safety-Gates bleiben getrennt. Neue Erkenntnisse, die Eingriff oder Teststrategie
   materiell bestimmen, erfordern eine vollstaendige Planrevision und erneute
   Freigabe der exakten SHA vor der Umsetzung.
5. Fuer allgemeine Korrekturen erst bei belegter Ursache, bestaetigter
   Polaritaet/Ruhelage und freigegebenem
   Eingriff kleinsten belegten Delta in bestehender Boardprofil-SSOT, bestehendem
   Headergenerator und gegebenenfalls Adapter/Composition Root umsetzen;
   vorhandene Tests erweitern. Keine angenommene Sicherheit im Resetfenster.
   Konkrete Aenderungspfade und Ownerentscheid im PR festhalten. Fuer den
   ausschliesslich LOW-inaktiven GPIO16-Schnitt gilt die unten konkret begrenzte
   Ownerentscheidung; HIGH/EIN bleibt dabei unbewiesen.
6. Gezielt Generatorpruefung und unmittelbar betroffene bestehende Hosttests
   nach einer Aenderung ausfuehren; Kommandos, Ergebnis und HEAD dokumentieren.
   Gesicherte Beobachtung von Power-On, ESP32-Reset und stabilem Standby sowie
   Image-/Quellstandszuordnung und ursachenbezogene Diagnose sind nach
   Planfreigabe mit abgestimmtem sicherem Aufbau durchfuehrbar. Fuer alle
   auffaelligen physischen Kanaele gegebenenfalls sicher angeschlossenen
   Einzelpruefverbraucher und Klemmen-/Kanalzuordnung dokumentieren.
   Beobachtung muss den Zeitraum vor `app_main` einschliessen;
   ungeeignete zeitliche Aufloesung oder fehlender Zugriff ist `NOT_PROVEN`,
   nicht PASS. Ein Luefter-EIN beim Boot oder gehaltenem EN-Reset ist nach
   Ownerentscheidung fuer diesen Aufbau erlaubt; fuer andere Ausgaenge und
   geforderte AUS-Zustaende bleiben die jeweiligen Safety-Vertraege bestehen.
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

## Revision: R1-Luefterverhalten und GPIO16-Firmware-Schnitt

Diese vollstaendige Planfassung fuehrt den bisherigen Diagnosescope fort. Der
Owner fordert auf Basis des realen OUT1-LOW-Tests nur `gpio16.active_level:
high` in der Boardprofil-SSOT und den daraus generierten Header. Diese materielle
Produkt-Aenderung wird erst nach Freigabe der exakten SHA dieser Revision
implementiert. Keine Freigabe von Planner, Peltier, BTS7960-Leistung oder
`ACTUATOR_RELEASE`; der reale aktive HIGH-/EIN-Funktionstest bleibt offen.

### Nachweise und R1-Ownerentscheidung

- Owner bestaetigt Luefter an physischen OUT1 und OUT4; OUT2/3 unbelastet.
  ROM-LOW-Tests: GPIO27/OUT4 und GPIO16/OUT1 jeweils digitales LOW mit
  Output-Enable und Ownerbeobachtung Luefter AUS; OUT4 lief beim GPIO16-Test
  weiter. LOW=AUS ist fuer die jeweils beobachteten Kanaele belegt. Die
  HIGH=EIN-Wirkung wurde nicht aktiv getestet; `active_level: high` ist der
  fuer den LOW-inaktiven binaeren Adapter gewaehlte SSOT-Wert, kein Nachweis
  eines vollstaendigen AUS/EIN/AUS-Tests. Quelle:
  `docs/audits/ISSUE32_BOOT_DIAGNOSIS_EVIDENCE.md`.
- **Geaenderte R1-Verhaltensanforderung fuer die Luefter am vorhandenen
  ESP32-MOS-X4-Aufbau:** Der Owner erlaubt deren Betrieb beim Boot und bei
  gehaltenem EN-Reset und akzeptiert das bekannte elektrische Restrisiko
  dieses Boards. Der historische Reset-FAIL gegen die damalige Anforderung
  und die tatsaechlichen Mess-/Beobachtungsbefunde bleiben unveraendert;
  es entsteht kein elektrisches Mess-PASS oder generelles Hardware-GO. Ein
  Umbau nur zum Erzwingen von Luefter-AUS bei Boot/gehaltenem EN-Reset ist
  weder Voraussetzung noch R1-Blocker allein aufgrund dieses Befundes.
  Fuer andere MCU-Ausfaelle ist Luefter-EIN nicht nachgewiesen: Bei einem
  CPU-Haenger kann ein zuletzt aktiv getriebener LOW-Pegel bestehen bleiben
  und den Luefter AUS halten. Kein behaupteter automatischer Luefterbetrieb
  bei beliebigem MCU-Ausfall.
- **Unveraenderte Safetygrenze:** Peltier/BTS7960-Leistung bleibt bei Boot,
  Reset, unbekanntem Zustand und ohne Safety-/Sensorfreigabe AUS. Eine
  Luefter-Ruhelage EIN gibt das Peltier nicht frei. Keine allgemeinen
  Aktor-Gates oder fremde Ausgaenge lockern; `ACTUATOR_RELEASE=NO`.
- Der an OUT4/GPIO27 angeschlossene zweite Pruefluefter ist nicht der
  produktiv zugeordnete OUT2/GPIO17; GPIO27 bleibt Reserve. OUT2 und der
  produktive zweite Luefter benoetigen eigenen Funktionsnachweis. OUT3/GPIO26
  hat derzeit keinen angeschlossenen Piezo und wird in diesem Schnitt nicht
  benutzt; GPIO17 und GPIO26 bleiben `Unconfirmed`, GPIO26 und GPIO27 werden
  nicht produktiv aktiviert oder umbelegt. Ob die Aussage, OUT3 werde nicht
  genutzt, eine R1-Scopeaenderung fuer den Summer bedeutet, ist eine
  **gesonderte explizite Ownerentscheidung**, derzeit nicht getroffen. Die
  bestehende Summeranforderung und Ownerentscheid O3=B bleiben bis dahin
  bestehen; kein stilles Streichen oder Ersetzen.

### Konkretes Delta an R1-Vertraegen nach Planfreigabe

Zusammen mit dem Firmware-Schnitt, vor seiner Review-Uebergabe:

- `docs/SPECIFICATION_REVIEW.md`: R1-Safetytext unterscheidet logisch
  gesperrte Aktorfreigabe und erlaubten physischen Luefterbetrieb beim Boot
  und gehaltenem EN-Reset am vorhandenen MOS-X4; bewusst akzeptiertes
  elektrisches Restrisiko ohne Konformitaetsbehauptung. Peltier-/BTS7960-AUS
  und alle Safety-/Sensorfreigaben unveraendert. Kein Versprechen fuer
  beliebigen MCU-Ausfall.
- `docs/HARDWARE.md`: Board- und sichere-Boot-/Resetabschnitte auf diese
  R1-Luefterentscheidung abstimmen, historischen FAIL und offene Mess- und
  Funktionsnachweise erhalten; normaler Standby und verbleibende Safety-Gates
  unveraendert. Keine automatische Peltierfreigabe.
- `docs/OPEN_POINTS.md`: pauschalen Boot-/Reset-AUS-Nachweis nur fuer die
  betroffenen Luefter im erlaubten Fenster begrenzen; Kanal-/OUT2-, gezielte
  EIN/AUS-, Boot-/Resetbeobachtungs- und Peltier-Safety-Nachweise offen halten.
- Issue #32: Akzeptanztext gezielt fuer die Luefter bei Boot/gehaltenem Reset
  anpassen; historische FAIL-Evidence erhalten, Status-/Gates nicht schliessen.
  Den konkreten Issue-Body vor der Aenderung erneut live lesen und nach der
  Aenderung erneut auslesen. Keine Summer-Scopeaenderung ohne gesonderte
  ausdrueckliche Entscheidung.

`docs/ACCEPTANCE_TESTS.md` HW-32-01..03 fuehrt bestehende Nachweise; nur bei
tatsaechlicher Statuswirkung mit SHA-gebundener Evidence nachfuehren, keine
neue Akzeptanzbehauptung. Wenn weitere normative Widersprueche entdeckt werden,
den betroffenen konkreten Vertrag und die Abweichung dem Owner vorlegen;
keine stillschweigende Scopeausweitung.

### Umsetzung nach Freigabe der exakten Plan-SHA

1. Nur `config/board_profiles/esp32_32e_quad_mosfet_r1.yaml` bei GPIO16
   `active_level: TBD_HARDWARE` auf `high` setzen, keine sonstige Profil- oder
   Bias-/Bootpegelkorrektur. `safe_boot_level`, `external_bias`, Zuordnung und
   globale Hardwareverifikationsfelder unveraendert. Generator
   `python3 scripts/generate_board_profile_header.py` nutzen und generierten
   `main/generated/board_profile_r1.hpp` mitcommitten. Bestehender Adapter und
   `begin()` bleiben unveraendert; HIGH-Aktorfreigabe wird nicht komponiert.
2. `python3 scripts/generate_board_profile_header.py --check` und `--selftest`
   sowie bestehenden ESP-IDF-Linux-CMock-Test
   `test/esp_idf_binary_output_sink_host` gemaess dessen README ausfuehren;
   bestehende Tests bei Bedarf um gezielte Erwartung fuer die neue SSOT-
   Ableitung ergaenzen. `git diff --check` und Builder-Self-Check gemaess
   `docs/CI_AND_QUALITY_GATES.md` auf Implementierungs-HEAD; keine vollstaendige
   Pre-Ready-Ausfuehrung ohne getrenntes Review-/Owner-Gate.
3. Fuer den gesondert autorisierten DEV-Flash App-Partition und Restoreweg
   vorher per lesendem Readback/Hash und passender Flash-/Partitionsinformation
   nachweisen; keine NVS-Loeschung, kein `erase_flash`, keine OTA. Buildprofil,
   Imagehash, Flash-/Readback-Ergebnis, Source-HEAD und Log dokumentieren.
   Ohne zusaetzliche konkrete Flashfreigabe oder Restorevoraussetzungen bleibt
   Flash `NOT_RUN`/`BLOCKED`.
4. Nach gesondert freigegebenem Flash Owner-beobachtetes OUT1-AUS im stabilen
   Firmwarezustand ohne Aktorfreigabe und Log `ready_inactive` fuer Innenluefter
   pruefen; UART-Reset/Start koordiniert. Beim Reset darf der Luefter gemaess
   R1-Ownerentscheidung laufen, nach App-Start soll er AUS sein. `POST_BOOT_OUT1_OFF`
   nur bei beobachtetem Erfolg PASS; EN-Reset-AUS nie als PASS behaupten.
   Im Fehlerfall Ausgabe deaktivieren/Versorgung sicher trennen, Ursache und
   Restoreweg dokumentieren. Aktive HIGH-/EIN- und weitere Kanaele nicht testen.
5. Evidence und PR auf genaue Test-/Source-SHA binden; global
   `SSOT_CONFORMANCE=PENDING`, `FUNCTIONAL_HARDWARE_VERIFICATION=PENDING`,
   `ACTUATOR_RELEASE=NO`; unabhaengige Fix Verification und weitere Issue-32-
   Gates bleiben offen. Der Builder haelt fuer Review/Ownerfreigaben an.
