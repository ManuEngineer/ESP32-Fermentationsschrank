# Issue #32 – Boot-/Resetbefund der Onboard-MOSFET-Ausgaenge

Status: Ownerfreigabe fuer die Planfassung auf Commit
`67f40f82b875fb4864247895971aee9fa1f4c180`. Die dort festgelegten Grenzen und
separaten Ownerentscheidungen gelten weiter. Revision fuer den unten konkret
beschriebenen ROM-LOW-Helfer: **Freigabe dieser neuen Plan-Commit-SHA ausstehend**;
die bisherige SHA gibt diesen zusaetzlichen Testpfad nicht frei. Ausgangsbasis `main`
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

## Revision: begrenzter ROM-LOW-Diagnosehelfer

Der Owner beauftragt die offline testbare Vorbereitung eines LOW-Tests ohne
Flash. Das ist ein neuer registerschreibender Hardwaretestpfad und eine
materielle Aenderung der Teststrategie; die exakte SHA dieser vollstaendigen
Planrevision ist vor dessen Implementierung freizugeben. Die frueheren
Abschnitte gelten weiter. `LOW_TEST=BLOCKED_ELECTRICAL`, bis elektrische
Eignung nachgewiesen und der reale Eingriff gesondert freigegeben ist.

### Konkretes Delta

1. `scripts/issue32_rom_gpio_low.py`: eng begrenzter Python-Helfer fuer den
   klassischen ESP32, ausschliesslich GPIO16 oder separat GPIO27, esptool-ROM
   statt Stub, kein Flash, HIGH, PWM, anderer GPIO oder universelle Register-CLI.
   GPIO27 bleibt produktive Reserve; die Verwendung ist nur eine getrennte
   Diagnose. Verfuegbaren esptool-API-Stand und Herkunft nachweisen. Kein
   automatisches RTS/DTR-Reset; Anleitung fuer manuelles Halten von IO0 LOW,
   EN-Reset und Loslassen von IO0 gemaess Espressif-Dokumentation, nur nach
   sicherer bestaetigter Verdrahtung und koordiniert mit dem Owner.
2. Standard ist lesend: klassisches Chipziel/ROM bestaetigen, relevante
   Register lesen und geplante Schreiboperationen mit Adresse, Maske, Wert,
   Reihenfolge und erwarteter Wirkung ausgeben. `--execute-low` ist eine
   zusaetzliche bewusst gesetzte Option, kein Ersatz fuer die zuvor
   nachgewiesene elektrische Freigabe und separate Owner-Testfreigabe.
   Fuer GPIO27 RTC-IO-MUX-/Hold-Konflikte explizit pruefen: unbekannte oder
   nicht sicher auswertbare Zustaende fail-closed abbrechen, keine anderen
   Pins oder unbekannte Register veraendern.
3. Reihenfolge aus den offiziellen ESP-IDF-6.1-Registern und ESP32-TRM
   verifizieren: gewaehltes Output-Enable gezielt sperren; Ausgangsdatenbit
   ueber W1TC LOW vorladen; nur gewaehlte Pin-MUX-/Matrixfelder und
   PAD_DRIVER/Pull-/Inversionszustand sicher setzen; erst danach gewaehltes
   Output-Enable ueber W1TS setzen. GPIO16 IO-MUX Offset `0x4c`, GPIO27
   `0x2c`; GPIO-Ausgangsmatrix Offsets `0x570` und `0x59c`; GPIO-PIN-Offsets
   `0xc8` und `0xf4`. W1TS/W1TC-Aliase nicht durch eine Maskenoperation
   behandeln, die sie unerwartet liest oder fremde Bits setzt. Vor/nachher
   geeignete lesbare Statusregister erfassen und Abweichungen fail-closed
   melden; Registerstatus allein belegt keinen elektrischen LOW-Pegel.
   Quelle: [gpio_reg.h](https://raw.githubusercontent.com/espressif/esp-idf/v6.1/components/soc/esp32/register/soc/gpio_reg.h),
   [io_mux_reg.h](https://raw.githubusercontent.com/espressif/esp-idf/v6.1/components/soc/esp32/register/soc/io_mux_reg.h),
   [rtc_io_reg.h](https://raw.githubusercontent.com/espressif/esp-idf/v6.1/components/soc/esp32/register/soc/rtc_io_reg.h),
   [ESP32-TRM Kapitel 6](https://documentation.espressif.com/ESP32_Technical_Reference_Manual_en.pdf).
4. Zeitlich begrenzter, Owner-beobachteter Einzelkanallauf: Pegel am
   bestaetigten relevanten Punkt und Luefterwirkung dokumentieren. Ein
   Registerzustand ohne physische LOW-Bestaetigung ist kein LOW-PASS oder
   Widerlegungsbeweis. Cleanup mit gezielter Output-Disable-Operation und
   manueller Rueckkehr durch EN-Reset; beim Aufgeben von LOW koennen Ausgaenge
   wieder aktiv werden. Verbindungsabbruch, Host-Absturz und Stromausfall
   erlauben kein garantiertes Cleanup: sichere Abschaltmoeglichkeit des
   Versorgungs-/Lastpfads vorab durch Owner sicherstellen. Kein automatisches
   Wiederherstellen unbekannter Ausgangszustaende oder Firmwareflash.
5. Offline-Testdatei `test/test_issue32_rom_gpio_low.py` mit Fake-ROM und
   fest verdrahteten Referenzvektoren: Default ruft keinen GPIO-Write auf,
   nur Pins 16/27, Werte/Masken/Adressen/Abfolge, unveraenderte fremde Bits,
   keine HIGH-Werte, Konflikt- und Fehlerpfade, GPIO27 RTC-/Hold-Guards und
   Cleanup; keine Hardware oder seriellen Port oeffnen. Gezielte Tests und
   `git diff --check`; bei erfolgter Implementierung Builder-Self-Check gemaess
   Workflow, danach unabhaengige Fix Verification/Review. Kein Pre-Ready ohne
   separates Gate.

### Elektrisches Gate und Owner-Arbeitsschritt

Ownerangaben: Header G16/G17 4,98 V, G26 3,75 V, G27 4,11 V bei gehaltenem
Reset; stromlos 4,4–4,5 kOhm zwischen 5 V und Header. Das zeigt weder Padpegel
noch zulässige Senkenbelastung. Modulpad `27=GPIO16`, `12=GPIO27` laut
[WROOM-32E-Datenblatt](https://documentation.espressif.com/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.html).
Bei vollstaendig abgeschalteter/getrennter 12-V- und USB-Versorgung und
eindeutig sicher erreichbaren Modul-Loetpads: Widerstand/Durchgang G16-Header
zu Modulpad 27 sowie G27-Header zu Modulpad 12 einzeln messen und Werte melden;
keine benachbarten Kontakte beruehren. Nicht erreichbare/identifizierbare Pads:
Messung abbrechen, Hindernis dokumentieren. Ein Stromlos-Durchgangstest ist
allein kein elektrisches GO: Kopplung, Senkenstrom, Padspannung inklusive
Versorgungs-/Resetzustaenden und sicherer Testpfad muessen belastbar belegt
sein. Bei unzulaessiger Kopplung ist eine an identifiziertem Pfad ausgelegte
5-V-tolerante Trennung/Schutzbeschaltung mit definiertem sicheren Resetzustand
eine konkrete Ownerentscheidung; Bauteilwerte nicht raten.
