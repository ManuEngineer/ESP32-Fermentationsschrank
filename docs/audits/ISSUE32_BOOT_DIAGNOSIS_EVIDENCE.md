# Issue #32 – Bootdiagnose: vorlaeufige Evidence

Stand 2026-11-18. Ownerfreigabe Plan `67f40f82b875fb4864247895971aee9fa1f4c180` fuer PR #212; Ausgangsbasis `main` `9233226da3cf624eeefce468c219dafc0653caae`. Allgemeine Hardwaretestfreigabe erteilt; Plan- und gesonderte Ownergates gelten.

## DEV-Flash und POST_BOOT_OUT1_OFF, 2026-11-18

Der Owner gab DEV-Flash und `POST_BOOT_OUT1_OFF` gesondert frei und bestaetigte Beobachtungs-/Abschaltbereitschaft, FT232-Anschluss und getrennte BTS7960-/Peltier-Leistungsversorgung. Firmware-Source-HEAD `de521fbe108aeab90b4096e1eaf8ca2cf590e266`, freigegebener Plan `34527abf1eac59099e5072a752a9c6dcde4b9b17`. ESP-IDF v6.1, Release-Profil `esp32_release`: Build PASS; App-Image SHA-256 `2955e35e1f7e123ba115fb16d895720bb34326b37c88bac5134d444f2cf5af9b`.

Vor dem Flash: ESP32-D0WD-V3, Flash-Verschluesselung und Secure Boot laut esptool aus; installierte Partitionstabelle mit Build bytegleich, Factory-App `0x10000`/`0x2f0000`. Vollstaendige Factory-Partition zweimal gelesen und mit der gespeicherten Datei verglichen: SHA-256 `3b907b251b32b034f2ac112f25862aaa317c827707837ccf2982232e9dc05cc9`. Lokale, fluechtige Sicherung `/tmp/issue32-dev-flash-backup/factory-app.bin`, Tabelle `/tmp/issue32-dev-flash-backup/partition-table.bin`. Restoreweg bei Bedarf nach separater Koordination: ROM-Modus und `esptool --chip esp32 --port /dev/ttyUSB0 --before no-reset --after no-reset write-flash 0x10000 /tmp/issue32-dev-flash-backup/factory-app.bin`; die Sicherung und Zielpartition zuvor erneut pruefen. Keine NVS- oder State-Store-Loeschung, kein `erase_flash`, kein OTA. Nur App-Image geschrieben; esptool-Hashverifikation und zusaetzlicher bytegenauer Readback PASS mit obigem Imagehash.

Der automatische RTS-Resetimpuls erzeugte zunaechst keinen sichtbaren Boot; nach manuellem EN-Kontakt des Owners startete das Board. UART: `App version: de521fb`, `inner_fan=ready_inactive`, `real actuators: disabled` und stabile Heartbeats. Owner: Display normal, OUT1-Luefter AUS, OUT4-Luefter weiterhin EIN. **`POST_BOOT_OUT1_OFF=PASS`** fuer diesen beobachteten DEV-Firmwarestart. Rohlog lokal `/tmp/issue32-dev-flash-backup/boot-uart.raw`, SHA-256 `13877a8f01032f8adf4300d0fae137b184fb5c01ff9f9229aba4ed796d2eb64d`; es kann Netzangaben enthalten und wird nicht publiziert. Keine analoge Pegelmessung, kein HIGH-/EIN-Test und keine Aussage ueber alle Reset- oder Fehlerzustaende. Historischer gehaltener EN-Reset-FAIL bleibt bestehen. GPIO27 bleibt Reserve; `ACTUATOR_RELEASE=NO`, `SSOT_CONFORMANCE=PENDING`, `FUNCTIONAL_HARDWARE_VERIFICATION=PENDING`.

## Beobachtung und Aufbau

Owner bestaetigt Luefter an MOSFET 1 und 4; MOSFET 2 und 3 sind nicht angeschlossen. Die Luefter an 1 und 4 liefen laut Owner waehrend der UART-Aufnahme unveraendert und liefen auch beim zwischenzeitlich schwarzen Display weiter. Klemmen und Verhalten vor `app_main` sowie die vollstaendige Resetqualifikation sind `NOT_VERIFIED`. Vermutete Zuordnung MOSFET 1/4 zu GPIO16/27 ist kein Funktionsnachweis. Alle auffaelligen physischen Kanaele einschliesslich MOSFET 4 sind Gegenstand der Untersuchung; GPIO27 bleibt softwareseitig Reserve. `ACTUATOR_RELEASE=NO`.

Owner bestaetigt `/dev/ttyUSB0` als verbundenen UART-Adapter und Beobachtungsbereitschaft. [Fruehere Evidence](PR210_D1_SMOKE_RESULT_20261025.md) dokumentiert einen Reset beim Oeffnen. Am 2026-11-17 wurde der Port nach Koordination fuer 20 Sekunden lesend mit 115200 Baud geoeffnet; kein expliziter DTR-/RTS-Puls, aber ein Reset durch das Oeffnen ist nicht ausgeschlossen. Empfang: 278 Nullbytes, keine lesbare Boot-/Firmwaremeldung. Ob ein Reset stattfand, ist aus dem Mitschnitt nicht bestimmbar; Ursache der Nullbytes unbekannt. **Ownerbeobachtung:** Luefter an MOSFET 1 und 4 liefen waehrend der UART-Oeffnung ohne erkennbare Aenderung weiter; das zuvor normal laufende Display wurde schwarz und blieb schwarz. Diese Beobachtung belegt weder einen Reset noch das Verhalten vor `app_main`. Ein ROM-Bootloader ist eine Moeglichkeit, aber nicht belegt.

Der Builder oeffnete den Port anschliessend unbeabsichtigt erneut durch `stty -F /dev/ttyUSB0 -a` zur Abfrage der Einstellungen; kein Steuerimpuls wurde bewusst gesendet, ein durch Oeffnung ausgeloester Reset ist nicht ausgeschlossen. Diese zweite Oeffnung war nicht separat mit dem Owner koordiniert. Einstellungen zeigten 115200 Baud, 8 Datenbits, kein Parity, ein Stopbit; kein Zustands- oder Image-Nachweis. Gueltiger Bootlog und nachgewiesener neuer Boot/Reset `NOT_VERIFIED`. BTS7960 laut Auftrag ohne 12-V-Leistungsversorgung und ohne Peltier.

Nach Aufforderung zur koordinierten Spannungsunterbrechung meldet der Owner: Luefter schalten aus; nach Wiedereinschalten laufen sie, Display bleibt schwarz. Dies belegt den beobachteten Betrieb nach Power-On, nicht dessen exakte zeitliche Lage vor `app_main` oder einen funktionierenden Standby. Owner fragt, ob am FT232 `EN` und `RTS` vertauscht seien. Historische [Issue-29-Messung](../ISSUE_29_MEASUREMENTS.md) beschreibt RTS-Reset ueber ESP32-EN und DTR/IO0 im damaligen Aufbau; die aktuelle Verdrahtung und die Bedeutung der Beschriftung `EN` am konkreten FT232-Adapter sind nicht verifiziert. Kein Umstecken oder neue Portoeffnung durchgefuehrt.

## UART nach Owner-Korrektur, 2026-11-18

Owner meldet korrigierte FT232-Verbindung und fordert Konnektivitaetspruefung an. `/dev/ttyUSB0`, 115200 Baud, pyserial mit `dtr=False` und `rts=False` vor `open()`, keine expliziten Resetpulse; 20 Sekunden Aufnahme ergaben 9 377 Bytes (106 Nullbytes) mit lesbarem Boot und Heartbeats. Das Oeffnen kann einen Reset ausgeloest haben; Resetursache laut Log `rst:0x1 (POWERON_RESET)`, Bootmodus `boot:0x13 (SPI_FAST_FLASH_BOOT)`. Keine Flash- oder NVS-Aktion. UART-Empfang von ESP zum Host ist **PASS**; bidirektionaler Datentransfer ist **NOT_RUN**. Rohlog lokal `/tmp/issue32-uart-20261118.raw`, nicht veroeffentlicht; folgende Auszuege ohne Netz-/Zugangsdaten:

```text
I (771) app_init: App version:      a9a1156
I (774) app_init: Compile time:     Oct 16 2026 16:40:57
I (779) app_init: ELF file SHA256:  dca3cc664...
I (784) app_init: ESP-IDF:          v6.1
I (841) app_main: onboard outputs: inner_fan=polarity_unconfirmed_no_gpio_access outer_fan=polarity_unconfirmed_no_gpio_access buzzer=polarity_unconfirmed_no_gpio_access
I (1581) app_main: real actuators: disabled
```

Logversion `a9a1156` laesst sich lokal zu Commit `a9a1156a1d44d4679957526bbc34a30be6422e4e` aufloesen. Versionskennung, Buildzeit, kurzer ELF-Hash und Ausgabelog stimmen mit [historischem Kontrollboot](PR210_D1_CONTROL_BOOT_UART_20261025.txt) ueberein. Dies ist ein Indiz, aber kein vollstaendiger Flash-Hash oder Beweis quellgleichen Images bzw. des Buildprofils. Quellstand dieses Commits erzeugt die Ausgaenge 16/17/26 mit `Unconfirmed`; Reserve GPIO27 ist nicht generiert. **Ownerbeobachtung:** Display zuvor schwarz, nach der UART-Pruefung wieder normal; beide Luefter liefen durchgehend ohne Unterbruch. Der Log weist einen Boot nach; die Ownerbeobachtung ist nicht zeitlich fein mit `app_main` synchronisiert. Exakter Zustand vor `app_main` und elektrische Ursache bleiben offen.

**Funktionales Nicht-Aktivierungsverhalten: FAIL.** Die beiden an physischen
MOSFET-Ausgaengen 1 und 4 angeschlossenen Luefter liefen laut Owner durchgehend,
waehrend das UART-Log `polarity_unconfirmed_no_gpio_access` und
`real actuators: disabled` meldete. Das ist ein beobachteter Hardware-
Funktionsfehler; er belegt keinen gemessenen GPIO- oder Gatepegel und keine
abschliessende Root Cause. Der genaue Einschaltzeitpunkt vor `app_main`,
Bias und Polaritaet, vollstaendige aktuelle Imageidentitaet, funktionale
GPIO-Zuordnung sowie Reset- und Standby-Qualifikation bleiben offen.

## Quellenbefund

### Koordinierter Kaltstart mit abgetrenntem RTS/DTR, 2026-11-18

Owner: Board-12-V und FT232-USB zum Umstecken spannungsfrei; RTS und DTR am ESP32 entfernt, TXD/RXD/GND verbunden, FT232-USB wieder angeschlossen, Board-12-V zunaechst aus. Bei bereits offener UART-Aufnahme hat der Owner die 12-V-Versorgung eingeschaltet. UART meldet `rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)`, `App version: a9a1156`, alle drei `polarity_unconfirmed_no_gpio_access`, `real actuators: disabled` und fortlaufende Heartbeats. Rohlog lokal `/tmp/issue32-cold-20261118.raw`; kein Flash. Owner beobachtet: Luefter an physischem MOSFET 1 und 4 schalten unmittelbar ein und bleiben eingeschaltet; Display folgt nach etwa einer Sekunde. Funktionaler Nicht-Aktivierungsbefund **FAIL**. Diese Beobachtung beweist keinen genauen Zeitbezug zu `app_main`, keinen Gatepegel und keine Ursache. Ein Multimeter war nicht verfuegbar.

Owner drueckte einmal kurz die IO0-Taste und beobachtete keine Aenderung. Die Kaltstartaufnahme wurde nach 191 Heartbeats (letzte `uptime_ms=201725`) beendet; sie enthaelt einen einzigen Reset-Header und keine Nullbytes. Rohlog-SHA-256 `2a2b57d6315a30b99e3ab8ff902db93143416274230e1d51cbcb9fb47a5688cf` (lokales Rohlog kann sensitive Netzangaben enthalten).

### Kontrollierter EN-Reset

Owner bestaetigte einen selbst eingerichteten direkten EN-/Board-GND-Kontakt ohne Spannungsquelle oder aktiven Treiber. In einer anschliessend separat gestarteten UART-Aufnahme schloss und oeffnete der Owner den Kontakt. Ownerbeobachtung: Solange der Kontakt geschlossen war, blieb das Display schwarz; nach Oeffnen startete das Board und das Display erschien wieder. Beide Luefter liefen dabei durchgehend. Das UART-Log zeigt einen Boot `rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)`, App-Version `a9a1156`, `polarity_unconfirmed_no_gpio_access` und `real actuators: disabled`. Rohlog `/tmp/issue32-en-reset-20261118.raw`, SHA-256 `fdab45caab628a6ebe3e035187fe9b092f0ddd88372a8ab9fcc19d57deaab9b5`, nicht veroeffentlicht.

**Reset-Fail-Closed: FAIL** fuer die beiden angeschlossenen Luefter: Sie liefen laut Owner auch bei gehaltenem EN-Reset. Das belegt ein aktives Verbraucher-/Ausgangsverhalten waehrend des gehaltenen Resets, nicht den elektrischen Pegel, die exakte Transistortopologie, Kanal-GPIO-Zuordnung oder ob ein Verdrahtungsfehler beteiligt ist. Eine Aenderung von `begin()` nach Resetfreigabe kann den gehaltenen Reset nicht absichern; notwendige hardwareseitige Inaktivitaet ist noch nicht nachgewiesen. Reset- und Standbyqualifikation insgesamt bleiben offen.

### Konkrete offene Entscheidung zur Schaltungsdiagnose

Fuer die betroffenen physischen Kanaele 1 und 4 zuerst die tatsaechlichen Klemmenbezeichnungen, Luefterleitungen, Boardrevision und Gate-/Treiberbauteile anhand realer Platinenansicht/Unterlagen identifizieren; GPIO16/27 sind nur SSOT-Kandidaten. Wenn zugangssicher und ein Multimeter vorhanden ist: im Gleichspannungsbereich Board-GND als Bezug und die eindeutig identifizierten Lastklemmen sowie zugehoerigen GPIO-/Treiberpunkte im stabilen Zustand und bei gehaltenem EN-Reset beobachten. Klemmenmessungen unterscheiden Versorgung/Lastwirkung, die GPIO-/Treiberwerte zeigen den Zustand dieser Punkte; beides beweist allein noch keine genaue Gatebeschaltung oder Polaritaet. Keine Messspitze an unidentifizierte SMD-Pins, keine Widerstandsmessung an versorgter Schaltung, keine Verbindung zu 12 V an ESP-Pins. Ist ein Punkt nicht sicher erreichbar, `NOT_VERIFIED`.

Ownerentscheidung nach dieser Identifikation: ob die nachgewiesene Ursache eine Korrektur an der konkreten Gate-/Treiber-Bias-Beschaltung oder an der identifizierten Lastverdrahtung erfordert, oder ob ein geeignetes Board mit nachgewiesen inaktiver Resetlage ersetzt werden muss. Widerstandswert, Anschluss und Bauteil werden erst aus identifizierter Schaltung und Messung festgelegt; kein pauschaler Pulldown. Eine Firmwarekorrektur allein kann den beobachteten gehaltenen Reset nicht beheben.

Fuer den fehlenden aktiven Funktionsnachweis ist ein konkreter Kandidat ein einzelner lokal bestaetigter, zeitlich begrenzter Service-Pruefimpuls ueber den bestehenden `EspIdfBinaryOutputSink` fuer jeweils genau einen zuvor funktional identifizierten Kanal; bei Ende oder Fehler best effort AUS, keine Peltier-/Planneranbindung und keine dauerhafte offene Diagnoseschnittstelle. Die vorhandenen Servicevoraussetzungen in `docs/DIAGNOSTICS_AND_MAINTENANCE.md` bleiben verbindlich; insbesondere validierter Standby, bestaetigter PIN-Servicezustand und bestaetigte GPIO-/Funktionszuordnung. Ein verwendbarer Gesamtpfad ist derzeit nicht nachgewiesen. Der physische MOSFET 4 ist allenfalls Gegenstand einer gesondert freigegebenen Diagnose, GPIO27 bleibt produktiv Reserve. Bedienweg, Zeitlimit, sichere Abschaltung und die tatsaechliche Erfuellbarkeit der Voraussetzungen muessen als materielles konkretes Plan-Delta geklaert und vom Owner exakt freigegeben werden; dieser Kandidat ist keine implementierte oder autorisierte Freigabe.

- Boardprofil und `main/generated/board_profile_r1.hpp` auf obiger Basis: GPIO16/17/26 als jeweilige Ausgaenge mit `Unconfirmed`; GPIO27 nicht generiert. `EspIdfBinaryOutputSink::begin()` ruft bei `Unconfirmed` keine GPIO-Funktion auf und `app_main` verbindet die drei Instanzen nicht mit dem Planner. Das beschreibt den Quellstand; physische Inaktivitaet folgt daraus nicht.
- Quelltextsuche nach direkten `GPIO_NUM_16/17/26/27`, generierten Ausgangskonstanten und GPIO-Zugriffen zeigt diese drei Composition-Root-Verwendungen; sie schliesst indirekte Pin-Konflikte, Bootloader-/IDF-Effekte und Zugriffe des installierten Images nicht aus. Realer Firmwarepfad und Pin-Konfliktfreiheit `NOT_VERIFIED`.
- Lieferanten-PDF `references/datasheets/esp32-32e-quad-mosfet-board-supplier.pdf`: Seite 1 nennt N-Kanal-MOSFETs; Seite 5 beschreibt einen softwaregesteuerten Testmodus der zweikanaligen Variante. Weder elektrische Polaritaet noch Gate-/Treiberbias oder Ausgangszustand der realen vierkanaligen Revision vor Firmwarestart sind damit belegt. PCB-Schaltpfad `NOT_VERIFIED`.
- Historischer Nachweis [PR210 D1 Smoke](PR210_D1_SMOKE_RESULT_20261025.md): am 2026-10-25 wurde Original-App mit Readback-Hash-Praefix `3b907b25…` wiederhergestellt und beim [Kontrollboot](PR210_D1_CONTROL_BOOT_UART_20261025.txt) `App version a9a1156` sowie fuer alle drei Ausgaenge `polarity_unconfirmed_no_gpio_access` protokolliert. Das belegt den damaligen Stand, nicht den aktuell installierten Flashinhalt; aktuelle Imageidentitaet und Buildprofil `NOT_VERIFIED`. Keine Gleichsetzung des damaligen Smoke-Test-Images mit dem danach wiederhergestellten Original.

## Software-Schnitt nach Planfreigabe

Ownerfreigabe `34527abf1eac59099e5072a752a9c6dcde4b9b17`: Im Boardprofil ist ausschliesslich `gpio16.active_level` auf `high` gesetzt; der bestehende Generator erzeugt `kInternalFanPolarity=ActiveHigh`. Der Adapter bleibt unveraendert und setzt im bestaetigten Pfad LOW vor Output-Enable. GPIO17/26 bleiben `Unconfirmed`, GPIO27 Reserve. LOW=AUS bei OUT1 ist funktional belegt; aktives HIGH=EIN ist weiterhin `NOT_RUN`.

Gezielt: Generator `--check` PASS, `--selftest` PASS (18 Faelle), ESP-IDF-6.1-Linux-CMock-Ausgabeadaptertest 12 Tests PASS, `git diff --check` PASS. Der gepushte Implementierungs-HEAD und der Builder-Self-Check werden im PR-Handover gebunden. Zum damaligen Implementierungszeitpunkt noch kein Flash oder Hardwaretest des neuen Images: `POST_BOOT_OUT1_OFF=NOT_RUN`; aktueller Nachweis siehe oben. Damalige und weiterhin geltende Gesamtgates: `SSOT_CONFORMANCE=PENDING`, `FUNCTIONAL_HARDWARE_VERIFICATION=PENDING`, `ACTUATOR_RELEASE=NO`.

Der Owner akzeptiert fuer R1 am vorhandenen MOS-X4 das elektrische Restrisiko und Luefter-EIN bei Boot sowie gehaltenem EN-Reset. Historischer Reset-FAIL bleibt Befund; kein elektrischer Konformitaetsnachweis und kein Beleg fuer Luefter-EIN bei beliebigem MCU-Ausfall. Die fruehere Bezeichnung als nur DEV-begrenzte Abweichung ist historisch und wird durch diese R1-Ownerentscheidung konkretisiert.

## Bewertung und offene Nachweise

### Einmaliger GPIO16-/OUT1-LOW-Test und DEV-Ownerentscheid, 2026-11-18

Der Owner autorisiert den begrenzten DEV-Test trotz bekannten elektrischen Restrisikos, ohne dass dies elektrische Konformitaet oder Zuverlaessigkeit nachweist. Luefterbetrieb waehrend Boot und gehaltenem EN-Reset, auch bei anhaltendem MCU-Ausfall, akzeptiert der Owner fuer diesen DEV-Luefter-Scope als **OWNER_ACCEPTED_DEVIATION**. Der bisher beobachtete Reset-FAIL bleibt historisch FAIL und wird nicht als PASS umetikettiert. Fuer diesen begrenzten DEV-Schnitt fordert der Owner keinen Umbau auf Luefter-AUS bei Reset. Keine allgemeine Safety-, BTS7960-, Peltier- oder Produktfreigabe; `ACTUATOR_RELEASE=NO`.

Owner bestaetigte 12 V und UART angeschlossen, BTS7960-/Peltier-Leistung getrennt und Beobachtungs-/Abschaltmoeglichkeit. Einmaliger lokaler ROM-Helfer `/tmp/issue32_gpio16_rom_once.py`, SHA-256 `e396bc351c7f14e10b83f091d6db26ab79314c56ea7a6a7829a80e9657a342ec`, esptool 5.4.0, nach Owner-IO0-/EN-Sequenz eine bestehende UART-Verbindung; kein Flash. Offline-Fake-ROM-Pruefung fuer GPIO16-Bit, Reihenfolge, Hold-Abbruch und Cleanup PASS. Waehrend der ROM-Eintrittsvorbereitung wurde laut Owner der UART kurz getrennt; ROM-Synchronisierung und Registerzugriff danach erfolgreich.

Vorher: `GPIO_OUT=0x00000000`, `GPIO_ENABLE=0x00000000`, `PIN16=0x00000000`, `FUNC16=0x00000100`, `IO_MUX16=0x00000a00`, `DIG_HOLD=0x00000000`, `DIG_ISO=0xaaaa5000`. Aktiv: `GPIO_OUT=0x00000000`, `GPIO_ENABLE=0x00010000`, `GPIO_IN=0x08628feb` (Bit 16 LOW), `PIN16=0x00000000`, `FUNC16=0x00000500`, `IO_MUX16=0x00002a00`; Holdwerte unveraendert. Nach 20 s `GPIO_ENABLE=0x00000000`, Ausgang deaktiviert. Kein HIGH-/PWM-Befehl und keine analoge Pegelmessung.

**`GPIO16_LOW=CONFIRMED`, `OUT1_LOW=PASS`:** Der Owner beobachtete OUT1 AUS und anschliessend wieder EIN; OUT4 lief weiter. Der genaue Zeitpunkt des Wiedereinschaltens relativ zum Cleanup ist nicht separat synchronisiert. Nach erneuter UART-Oeffnung wurde normaler Boot `SPI_FAST_FLASH_BOOT`, App-Version `a9a1156`, unbestaetigte Polaritaeten und Heartbeats erfasst; Owner bestaetigt Display normal. Rohlog `/tmp/issue32-out1-restore-20261118.raw`, SHA-256 `3881337df9209f22b3baa58a6b06879dc3c42081bada15256fbe5e5df49adaab`; Rohlog nicht veroeffentlichen. Kein Flash; vollstaendiger Image-Hash nach Test nicht verifiziert.

Historischer Stand vor dem oben dokumentierten DEV-Flash: Funktionaler LOW=AUS-Nachweis ist auf OUT1/GPIO16 und zuvor OUT4/GPIO27 begrenzt. HIGH=EIN, OUT2/GPIO17, Summer/GPIO26 und vollstaendiges AUS/EIN/AUS sind `NOT_RUN`; GPIO27 bleibt produktive Reserve. `POST_BOOT_OUT1_OFF=NOT_RUN`: die noch installierte Firmware meldet weiterhin fuer den Innenluefter `polarity_unconfirmed_no_gpio_access`. Die vorgeschlagene SSOT-Aenderung auf `gpio16.active_level: high` und erzeugter Header erfordern Freigabe der exakten Planrevision; separater Flash-/Hardwaregate folgt.

### Einmaliger GPIO27-/OUT4-LOW-Test, 2026-11-18

Owner autorisierte ausdruecklich einen einmaligen DEV-Board-Test und akzeptierte das bekannte elektrische Risiko. Owner bestaetigte bei getrennter 12-V- und FT232-USB-Versorgung ca. 0,2 Ohm jeweils G26-Header zu Modulpad 11 sowie G27-Header zu Modulpad 12; G16/G17 wurden wegen schlechter Erreichbarkeit nicht gemessen. Der zuvor gemeldete G27-Headerwert bei gehaltenem Reset betraegt 4,11 V; die Messung ist keine Produktfreigabe oder Aussage ueber Langzeitbelastbarkeit.

Test nur GPIO27/OUT4, mit Nutzerbeobachtung und ohne Flash: vorbereiteter lokaler ROM-Helfer `/tmp/issue32_gpio27_rom_once.py` (SHA-256 `ff7a54df4410d7296d2a420281df3160a4dbd1c66a94747898d51fc76bae63c6`), ausgefuehrt mit `/var/lib/docker/data/engineering/home/manuel/.espressif/python_env/idf6.1_py3.13_env/bin/python -u /tmp/issue32_gpio27_rom_once.py`. Esptool 5.4.0, ein geoeffneter UART-Port, Owner fuehrte nach Portoeffnung IO0-/EN-Sequenz fuer ROM aus. Vorher waren automatische Verbindung (`No serial data received`) und separater manueller `no-reset`-Verbindungsversuch (`Invalid head of packet (0x55)`) vor Registerzugriff gescheitert. Offline-Fake-ROM-Pruefung fuer Ausgangsreihenfolge und RTC-/Hold-Abbruch PASS; kein Hardware-PASS daraus.

Vorher `GPIO_OUT=0x00000000`, `GPIO_ENABLE=0x00000000`, `PIN27=0x00000000`, `FUNC27=0x00000100`, `IO_MUX27=0x00000a00`, `RTC_TOUCH_PAD7=0x42000000`. Bei LOW: `GPIO_OUT=0x00000000`, `GPIO_ENABLE=0x08000000`, `GPIO_IN=0x00638feb` (Bit 27 LOW), `PIN27=0x00000000`, `FUNC27=0x00000500`, `IO_MUX27=0x00002a00`, `RTC_TOUCH_PAD7=0x42000000`. Nach 20 Sekunden `GPIO_ENABLE=0x00000000`; kein HIGH gesetzt. GPIO-IN ist eine digitale Eingangsruecklesung, keine analoge Pegelmessung (`GPIO27_VOLTAGE=NOT_MEASURED`).

**`GPIO27_LOW=CONFIRMED`, `OUT4_LOW_TEST=PASS`:** Owner beobachtete, dass OUT4 fuer einige Sekunden ausging und danach wieder anlief; OUT1 lief durchgehend. Owner sah den Computerbildschirm nicht und kann den Zeitpunkt des Wiederanlaufens nicht exakt dem Ende des 20-Sekunden-Fensters zuordnen. Nach Cleanup bestaetigte der Owner auf normalen EN-Reset wieder normalen Displaystart. Der UART-Restoremitschnitt blieb leer und belegt den Boot nicht; keine Flash-Schreibaktion, vollstaendiger Flash-Hash nachher `NOT_VERIFIED`.

Dieser begrenzte Funktionsnachweis zeigt LOW=AUS fuer den getesteten Pfad unter diesen Bedingungen. Er beweist keinen sicheren Power-On-/Resetzustand: der gehaltene EN-Reset mit laufenden Lueftern bleibt **FAIL**; `ACTUATOR_RELEASE=NO`, `SSOT_CONFORMANCE=PENDING`, `FUNCTIONAL_HARDWARE_VERIFICATION=PENDING`. Kein produktiver GPIO27-Einsatz.

### GPIO-LOW-Funktionstest: elektrische Vorpruefung, 2026-11-18

**Historischer Vorpruefungsstatus:** `LOW_TEST=BLOCKED_ELECTRICAL` fuer OUT1/GPIO16 und OUT4/GPIO27; damals kein GPIO geschaltet. Fuer den spaeteren einmaligen DEV-Test von GPIO27/OUT4 hat der Owner das Risiko ausdruecklich akzeptiert (Ergebnis oben); fuer OUT1/GPIO16 bleibt der aktive LOW-Test `NOT_RUN`. Laut Ownerauftrag sind es `ESP32_MOS_X4 / 303E32NMOS4`, Luefter an OUT1+/− und OUT4+/− (rot an +, schwarz an −, keine weitere Lueftermasse). Owner berichtet bei gehaltenem EN-Reset am Header gegen Board-GND: G16 4,98 V, G17 4,98 V, G26 3,75 V, G27 4,11 V; stromlos zwischen 5 V und Header jeweils etwa 4,4–4,5 kOhm. Dies sind Owner-Messangaben, nicht hier erneut ausgefuehrte Messungen. Die Werte weisen auf Kopplung hin, aber belegen weder Schaltplan noch Chip-Pad-Potenzial; der fruehere Test mit Multimeter-Stromeingang beweist keinen LOW-Zustand und wird nicht wiederholt. Owner berichtet ferner, GPIO16/17/26/27 seien auf der Zusatzplatine nicht weiter beschaltet.

[ESP32-WROOM-32E-Datenblatt, Abschnitt 6.3](https://documentation.espressif.com/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.html) spezifiziert den HIGH-Eingangsbereich bei 3,3-V-Bedingungen bis VDD+0,3 V; diese Betriebsbereichsangabe ist kein allgemeiner Absolutgrenzwert und die Headerwerte sind keine Padmessung. Da der konkrete Pfad Header–Modulpad–5-V-Kopplung ungeklaert ist, sind Ueberspannung am Pad und Strom-/Backpower-Verhaeltnisse nicht ausgeschlossen. Aus dem ohmschen Messwert allein folgt keine sichere LOW-Belastbarkeit. Die Lieferantenabbildung zeigt keine Schaltung; im Auftrag genannte [MOS-X4-Vorlage](https://github.com/blakadder/templates/blob/master/_templates/diynow_ESP32_MOS_X4) und [Showduino-Ausgangsimplementierung](https://github.com/sumkindafreak/new_showduino/blob/main/firmware/mosfet-node-esp32/ShowduinoMosfetNode/src/MosfetOutputEngine.cpp) sind Kandidatenbelege und keine Verifikation dieser Platinenrevision.

[Espressif esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/advanced-commands.html) kann Speicher-/Registerwoerter schreiben; dies allein treibt keinen Pin nachweislich LOW. Das [ESP32-TRM, Kapitel 6](https://documentation.espressif.com/ESP32_Technical_Reference_Manual_en.pdf) nennt IO-MUX, Ausgangsmatrix und Output-Enable als getrennte Voraussetzungen. Registerreihenfolge, Ausgangszustand, Beschraenkung auf einen Kanal und Restore/ROM-Verlassen wurden nicht auf dem Geraet verifiziert. ROM ist damit **NOT_PROVEN** als sicherer reproduzierbarer Testpfad; die elektrische Sperre gilt ebenso fuer ihn und fuer den bestehenden ESP-IDF-Sink. Ein materieller produktionnaher Diagnoseschnitt bleibt nur ein Kandidat und braucht gesonderte Plan-/Ownerfreigabe.

**Historischer naechster Schritt vor der Planfreigabe:** GPIO16-SSOT-/Header-Schnitt nur nach Freigabe der exakten Planrevision; anschliessender Flash und DEV-Beobachtung nur nach dem gesonderten Gate. Die Owner-Messung von G27-Header zu Modulpad 12 (ca. 0,2 Ohm) ist oben dokumentiert; eine Wiederholung wird nicht verlangt. Die akzeptierte DEV-Abweichung beseitigt weder den historischen Reset-FAIL noch das ungeklaerte elektrische Restrisiko.

| Nachweis | Status |
|---|---|
| Funktionales Nicht-Aktivierungsverhalten bei geloggter Aktorsperre | **FAIL**: Owner beobachtet beide Luefter durchgehend laufend; kein Pegel- oder Ursachennachweis |
| 12-V-Kaltstart mit FT232 RTS/DTR abgetrennt | **FAIL**: laut Owner beide Luefter unmittelbar und danach aktiv; genauer Zeitbezug zu `app_main` nicht nachgewiesen |
| Luefter an MOSFET 1 und 4 bei UART-Oeffnung aktiv | Ownerbeobachtung, keine verifizierte GPIO-Zuordnung |
| Luefter an MOSFET 1 und 4 waehrend UART-Boot 2026-11-18 durchgehend aktiv | Ownerbeobachtung; Log-Boot belegt, exakte Synchronisierung zu `app_main` nicht belegt |
| UART-Empfang 2026-11-18 | PASS; bidirektionaler Austausch NOT_RUN |
| Nach 12-V-Wiedereinschalten laufen Luefter 1 und 4; Display schwarz | Ownerbeobachtung; zeitliche Zuordnung zu `app_main` nicht verifiziert |
| MOSFET 2 und 3 ohne angeschlossene Last | Ownerbeobachtung |
| Klemmen und Bootphasen | NOT_VERIFIED |
| Aktuelles DEV-Image | App-Image und Factory-Readback SHA-256 gleich; Bootversion `de521fb` |
| Gate-/Treiberbias und vollstaendige Resetqualifikation | NOT_VERIFIED; LOW=AUS fuer OUT1/OUT4 funktional beobachtet |
| Ursachenklassen Image, Software oder vor Firmwarestart aktive Stufe | NOT_PROVEN; Kombination moeglich |
| Power-On vor/nach `app_main` | PARTIAL: Owner beobachtet laufende Luefter nach Wiedereinschalten; Zeitbezug zu `app_main` nicht belegt |
| Gehaltener EN-Reset, betroffene Luefter | **FAIL**: Owner beobachtet beide Luefter waehrend des gehaltenen Resets aktiv; vollstaendige Resetqualifikation offen |
| Sicherer stabiler Standby | NOT_VERIFIED |
| OUT4/GPIO27 LOW schaltet Luefter AUS | PASS, einmaliger Owner-autorisierter DEV-Test; GPIO-IN LOW, Ownerbeobachtung |
| OUT1/GPIO16 LOW schaltet Luefter AUS | PASS, einmaliger Owner-autorisierter DEV-Test; GPIO-IN LOW, Ownerbeobachtung |
| OUT1/GPIO16 HIGH und vollstaendiges AUS/EIN/AUS | NOT_RUN |
| `POST_BOOT_OUT1_OFF` | PASS beim oben dokumentierten DEV-Firmwarestart |
| `SSOT_CONFORMANCE`, `FUNCTIONAL_HARDWARE_VERIFICATION` | PENDING |

Falls ein Ausgang bereits vor `app_main` aktiv ist, kann ein spaeteres `begin()` diese Zeit nicht absichern. Hardwareeingriff oder neuer produktionsnaher aktiver Pruefpfad erfordern die im Plan vorgesehenen separaten Entscheidungen. Keine Polaritaet oder Massnahme wird vorweggenommen.

## PR #214: DEV-Flash Bring-up-Image und HIGH=EIN-/LOW=AUS-Test OUT1/OUT4, 2026-11-19

Source-HEAD des Images `11da33b1ba4f916c718468ce4df67f0d4f2d4888`, Plan `b21d563161e73210b64ef2ff6d46f212bb100bda` (Rev. 2), Independent Implementierungsreview PASS fuer `eb0ad67`. Owner bestaetigte vor Ort: DEV-Board am FT232 `/dev/ttyUSB0`, Testlaeufer an OUT1 und OUT4, 12-V-Abschaltung zugaenglich, BTS7960-/Peltier-Leistung getrennt, nichts an OUT2/OUT3.

**Image:** Profil `esp32_bringup` (`scripts/build_issue32_output_pulse.py`, `SOURCE_TREE_CLEAN=YES`), `esp32_fermentationsschrank.bin` SHA-256 `02a662ea88fadf18dda68db93f31fef72c3fe09c55a4789047953183b63c348f`. Release-Build desselben Standes enthaelt keinen `ISSUE32`-String. **Flash:** ESP32 (MAC-Endung `…2f:34`), installierte Partitionstabelle byte-identisch mit dem Build (nvs `0x9000`, phy_init `0xf000`, factory `0x10000`/`0x2f0000`, state_store `0x300000`); nur `factory` bei `0x10000` geschrieben, `esptool` "Hash of data verified" (zweimal; beim ersten Flash zusaetzlich bytegenauer Readback mit identischem SHA-256). Kein Backup (Ownerentscheidung DEV-Image), kein `erase_flash`, keine NVS-/State-Store-Loeschung, kein OTA, kein Bootloader-/Partitionswechsel.

**Zwischenfaelle (ehrlich):** Die erste Verbindung scheiterte, weil beim Aufbau RTS und DTR am FT232 vertauscht waren (Ownerbefund); Autoreset-/Downloadversuche und Portoeffnungen haben das Board dadurch mehrfach unbeabsichtigt zurueckgesetzt bzw. in den Downloadmodus gebracht. Ein `pulse out1`-Versuch waehrend eines solchen Resets wurde vom Harness fail-closed mit `ISSUE32_COMMAND result=FAIL reason=REJECTED` abgewiesen (kein `state=ON`). Nach korrigierter Verdrahtung wurde von vorn getestet; die gezaehlten Ergebnisse stammen ausschliesslich aus dem folgenden Lauf.

**Boot nach Reset (RTS, ein Portzugriff, Rohlog lokal nicht publiziert):** `rst:0x1 (POWERON_RESET)`, `App version: 11da33b`, `profile: esp32_bringup`, `source git sha: 11da33b1ba4f916c718468ce4df67f0d4f2d4888`, `onboard outputs: inner_fan=ready_inactive outer_fan=ready_inactive buzzer=ready_inactive reserve=ready_inactive`, `real actuators: disabled`, `ISSUE32_UART_RX_READY result=PASS`, `ISSUE32_READY bring_up_only=YES … commands="pulse out1|pulse out4|off" pulse_ms=3000`. **Owner:** nach `app_main` beide Testlaeufer AUS; bei EN liefen sie nur ganz kurz an (akzeptiertes R1-Verhalten, kein Fehler).

**UART-Auszug, durchgehend offener Port, je ein Befehl nach Owner-Freigabe:**

```text
#SEND pulse out1
I (723724) issue32_output_pulse: ISSUE32_PULSE channel=out1 state=ON
I (726734) issue32_output_pulse: ISSUE32_PULSE channel=out1 state=OFF result=PASS reason=TIMEOUT_OFF
#SEND pulse out4
I (762814) issue32_output_pulse: ISSUE32_PULSE channel=out4 state=ON
I (765824) issue32_output_pulse: ISSUE32_PULSE channel=out4 state=OFF result=PASS reason=TIMEOUT_OFF
#SEND off
I (785064) issue32_output_pulse: ISSUE32_OFF result=PASS reason=ALREADY_OFF
```

**Owner-Beobachtung (massgeblich):** OUT1-Testlast AUS → EIN (~3 s) → AUS wie erwartet; OUT4-Testlast AUS → EIN (~3 s) → AUS wie erwartet. Kein Reset, keine Abweichung waehrend der beiden Zyklen; je Kanal genau ein Zyklus, keine Wiederholung.

**Ergebnis:** `HW-32-04=PASS`, `HIGH_EIN=PASS` fuer OUT1/GPIO16 und OUT4/GPIO27 (OUT4 ist ein **Reservekanal-Hardwaretest** und beweist keine produktive Aussenluefterfunktion von OUT2). OUT2/GPIO17 und OUT3/GPIO26 sind **nicht einzeln getestet**; ihre `ActiveHigh`-Polaritaet beruht auf der Ownerakzeptanz gleichartiger PCB-Kanaele. Keine elektrische Konformitaet, keine Aktor-/Peltierfreigabe: `ACTUATOR_RELEASE=NO`, `SSOT_CONFORMANCE` und `FUNCTIONAL_HARDWARE_VERIFICATION` bleiben auf diesen Umfang beschraenkt; die Fan-Sinks sind weiterhin nicht mit Planner/Driver verbunden (keine geregelte Luefterfunktion).

**Endzustand:** Auf dem DEV-Board laeuft das **Bring-up-Image `11da33b`** (Profil `esp32_bringup`, SHA-256 `02a662ea…348f`), **nicht** die Release-Firmware. Kein Rueckflash, keine Wiederherstellung; Bootzustand AUS, Heartbeats stabil. Bei Bedarf ist ein quellgebundener Stand neu bau- und flashbar.

## PR #216: DEV-Flash Bring-up-Image und OUT2-Puls (Aussenluefter), 2026-11-19

Plan `docs/tasks/issue-32-real-fan-verification-plan.md` Rev. 2, Plan-SHA
`f93afa4bd01f7856a0e654a5e3d766cfdeb07c5a` (D1 Ownerfreigabe); Ownergates G1 (Verdrahtung
bestaetigt), G2 (Flash, Kommentar PR #216) und G3 (`pulse out2`, Ownerfreigabe im Chat).
`ACTUATOR_RELEASE=NO`, BTS7960/Peltier-Leistung getrennt.

**G1 (Owner-reported):** Innenluefter an OUT1/GPIO16, Aussenluefter an OUT2/GPIO17; das
reale Geraet war bereits vor G2 von OUT4 nach OUT2 umgesteckt (nicht erneut ausgefuehrt),
OUT4 unbelegt (Reserve). Die Luefter aus PR #214 sind laut Owner die realen Endgeraete
(die damalige Evidence nannte sie temporaere Testlasten und bleibt unveraendert). Vor G2/G3
vom Owner bestaetigt: BTS7960/Peltier-Leistung getrennt, Luefterversorgung sofort
trennbar, Owner beobachtet am Geraet.

**Image:** PR-HEAD `234ce8c096914c5b0c5f74d1726fc9650d307c5d` (sauber, `SOURCE_TREE_CLEAN=YES`),
`scripts/build_issue32_output_pulse.py` ohne `--allow-dirty-source-tree` nach `--clean`,
Profil `esp32_bringup` + `APP_ISSUE_32_OUTPUT_PULSE`, `esp32_fermentationsschrank.bin`
1 796 304 B, SHA-256 `5cf00469645570afa0f29980a5ab81ab02f92466f197ce30a1f6f49a53c83913`,
ELF-SHA (Boot-Log) `29212f342...`.

**Flash (G2):** Zielgeraet `ESP32-D0WD-V3 (revision v3.1)`, installierte Partitionstabelle
byteidentisch zum Build (`factory app 0x10000/3008K`). Nur App bei `0x10000`:
`Hash of data verified.`, `verify-flash`: `Verification successful (digest matched).`
Kein `erase_flash`, kein NVS-/State-Store-/Bootloader-/Partitionstabellen-/OTA-Zugriff, kein
Backup, kein Restore (Ownerentscheid).

**Boot (ein EN-Reset, Port durchgehend offen):**

```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
I boot: Loaded app from partition at offset 0x10000
I app_main: onboard outputs: inner_fan=ready_inactive outer_fan=ready_inactive buzzer=ready_inactive reserve=ready_inactive
I app_main: peltier bridge: ready_all_off_not_connected_to_planner
I issue32_output_pulse: ISSUE32_UART_RX_READY result=PASS
I issue32_output_pulse: ISSUE32_READY bring_up_only=YES real_actuators_enabled=NO commands="pulse out1|pulse out2|pulse out4|off" pulse_ms=3000 note=no_automatic_on_at_boot
```

Owner-Beobachtung nach dem Boot (vor dem Puls): beide Luefter AUS. Das Verhalten beim
EN-Reset wurde in diesem Lauf **nicht** gezielt untersucht; die akzeptierte R1-Abweichung
(Luefter koennen bei Boot/gehaltenem EN-Reset physisch laufen) bleibt bestehen; kein
`HW-32-02`-PASS, keine elektrische Konformitaetsaussage.

**G3 – einmaliger Puls OUT2:**

```
#SEND pulse out2
I issue32_output_pulse: ISSUE32_PULSE channel=out2 state=ON
I issue32_output_pulse: ISSUE32_PULSE channel=out2 state=OFF result=PASS reason=TIMEOUT_OFF
```

Ablauf ca. 3,0 s (Log-Ticks 30 814 ms → 33 824 ms); kein Reset/Panic/WDT, keine `E`-Zeilen.
**Owner-Beobachtung (massgeblich):** Aussenluefter an OUT2 AUS → EIN (~3 s) → AUS wie
erwartet; Innenluefter (OUT1) blieb AUS; OUT4 unbelegt.

**Ergebnis:** `HW-32-05=PASS` (realer Aussenluefter OUT2/GPIO17: Zuordnung und
AUS/EIN/AUS, ein Zyklus, Owner-beobachtet). Nicht belegt bleiben: Innenluefter-Dauerlauf
und Aussenluefter-Nachlauf in Hardware (erst nach #35/#90 end-to-end), Boot-/Reset-
Verhalten als eigener Nachweis (`HW-32-02`), OUT3/GPIO26-Summer (separat offen),
Stromaufnahme/Anlauf. Es wurde kein OUT1-/OUT4-Puls wiederholt (Plan T2 entfaellt).

**Endzustand Board:** laeuft das **Bring-up-Image `234ce8c`** (SHA-256 `5cf00469…3913`,
Profil `esp32_bringup`), **nicht** die Release-Firmware; Partitionen/NVS/State-Store
unveraendert; beide Luefter AUS; BTS7960/Peltier getrennt. Kein Rueckflash.
