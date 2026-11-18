# Issue #32 – Bootdiagnose: vorlaeufige Evidence

Stand 2026-11-18. Ownerfreigabe Plan `67f40f82b875fb4864247895971aee9fa1f4c180` fuer PR #212; Ausgangsbasis `main` `9233226da3cf624eeefce468c219dafc0653caae`. Allgemeine Hardwaretestfreigabe erteilt; Plan- und gesonderte Ownergates gelten.

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

## Bewertung und offene Nachweise

### GPIO-LOW-Funktionstest: elektrische Vorpruefung, 2026-11-18

**`LOW_TEST=BLOCKED_ELECTRICAL` fuer OUT1/GPIO16 und OUT4/GPIO27.** Kein GPIO wurde fuer diesen Test geschaltet; keine Portoeffnung, Registerschreibung oder Flashaktion. Laut neuem Ownerauftrag sind es `ESP32_MOS_X4 / 303E32NMOS4`, Luefter an OUT1+/− und OUT4+/− (rot an +, schwarz an −, keine weitere Lueftermasse). Owner berichtet bei gehaltenem EN-Reset am Header gegen Board-GND: G16 4,98 V, G17 4,98 V, G26 3,75 V, G27 4,11 V; stromlos zwischen 5 V und Header jeweils etwa 4,4–4,5 kOhm. Dies sind Owner-Messangaben, nicht hier erneut ausgefuehrte Messungen. Die Werte weisen auf Kopplung hin, aber belegen weder Schaltplan noch Chip-Pad-Potenzial; der fruehere Test mit Multimeter-Stromeingang beweist keinen LOW-Zustand und wird nicht wiederholt. Owner berichtet ferner, GPIO16/17/26/27 seien auf der Zusatzplatine nicht weiter beschaltet.

[ESP32-WROOM-32E-Datenblatt, Abschnitt 6.3](https://documentation.espressif.com/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.html) spezifiziert den HIGH-Eingangsbereich bei 3,3-V-Bedingungen bis VDD+0,3 V; diese Betriebsbereichsangabe ist kein allgemeiner Absolutgrenzwert und die Headerwerte sind keine Padmessung. Da der konkrete Pfad Header–Modulpad–5-V-Kopplung ungeklaert ist, sind Ueberspannung am Pad und Strom-/Backpower-Verhaeltnisse nicht ausgeschlossen. Aus dem ohmschen Messwert allein folgt keine sichere LOW-Belastbarkeit. Die Lieferantenabbildung zeigt keine Schaltung; im Auftrag genannte [MOS-X4-Vorlage](https://github.com/blakadder/templates/blob/master/_templates/diynow_ESP32_MOS_X4) und [Showduino-Ausgangsimplementierung](https://github.com/sumkindafreak/new_showduino/blob/main/firmware/mosfet-node-esp32/ShowduinoMosfetNode/src/MosfetOutputEngine.cpp) sind Kandidatenbelege und keine Verifikation dieser Platinenrevision.

[Espressif esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/advanced-commands.html) kann Speicher-/Registerwoerter schreiben; dies allein treibt keinen Pin nachweislich LOW. Das [ESP32-TRM, Kapitel 6](https://documentation.espressif.com/ESP32_Technical_Reference_Manual_en.pdf) nennt IO-MUX, Ausgangsmatrix und Output-Enable als getrennte Voraussetzungen. Registerreihenfolge, Ausgangszustand, Beschraenkung auf einen Kanal und Restore/ROM-Verlassen wurden nicht auf dem Geraet verifiziert. ROM ist damit **NOT_PROVEN** als sicherer reproduzierbarer Testpfad; die elektrische Sperre gilt ebenso fuer ihn und fuer den bestehenden ESP-IDF-Sink. Ein materieller produktionnaher Diagnoseschnitt bleibt nur ein Kandidat und braucht gesonderte Plan-/Ownerfreigabe.

**Kleinster naechster Schritt:** Bei vollstaendig spannungsfreiem Aufbau den konkreten Pfad vom Header G16 zum zugehoerigen WROOM-Modulpad und zur 5-V-Kopplung anhand eindeutiger Schaltungsunterlagen oder sicher zugaenglicher Durchgangs-/Bauteilidentifikation bestimmen; das Gleiche spaeter separat fuer G27. So wird festgestellt, ob das Modulpad direkt oder ueber welche Bauteile an der 5-V-Seite liegt. Falls das Pad ungeschuetzt gekoppelt ist, ist als Owner-Hardwareentscheidung eine zum nachgewiesenen Pfad passende 5-V-tolerante Trennung/Schutzbeschaltung erforderlich, die zugleich den Treibereingang definiert und den sicheren Reset-Zustand herstellt. Bauteil und Werte erst nach Schaltpfad-/Stromnachweis festlegen und Padspannung in allen Versorgungs-/Resetzustaenden innerhalb der Herstellerbedingungen sowie LOW-Funktion vor erneutem Test nachweisen. Ohne sichere Erreichbarkeit des Pads kein Messversuch und kein LOW-Test. Keine Annahme, dass eine solche Massnahme bereits eingebaut oder ausreichend ist.

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
| Aktuelles Image und tatsaechlicher Firmwarepfad | NOT_VERIFIED |
| Gate-/Treiberbias, aktive Polaritaet und Resetverhalten | NOT_VERIFIED |
| Ursachenklassen Image, Software oder vor Firmwarestart aktive Stufe | NOT_PROVEN; Kombination moeglich |
| Power-On vor/nach `app_main` | PARTIAL: Owner beobachtet laufende Luefter nach Wiedereinschalten; Zeitbezug zu `app_main` nicht belegt |
| Gehaltener EN-Reset, betroffene Luefter | **FAIL**: Owner beobachtet beide Luefter waehrend des gehaltenen Resets aktiv; vollstaendige Resetqualifikation offen |
| Sicherer stabiler Standby | NOT_VERIFIED |
| Aktive AUS/EIN-Pruefung | NOT_RUN; kein sicherer autorisierter vorhandener Aufrufpfad nachgewiesen |
| `SSOT_CONFORMANCE`, `FUNCTIONAL_HARDWARE_VERIFICATION` | PENDING |

Falls ein Ausgang bereits vor `app_main` aktiv ist, kann ein spaeteres `begin()` diese Zeit nicht absichern. Hardwareeingriff oder neuer produktionsnaher aktiver Pruefpfad erfordern die im Plan vorgesehenen separaten Entscheidungen. Keine Polaritaet oder Massnahme wird vorweggenommen.
