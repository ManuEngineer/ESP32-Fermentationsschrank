# Issue #32 – Bootdiagnose: vorlaeufige Evidence

Stand 2026-11-17. Ownerfreigabe Plan `67f40f82b875fb4864247895971aee9fa1f4c180` fuer PR #212; Ausgangsbasis `main` `9233226da3cf624eeefce468c219dafc0653caae`. Allgemeine Hardwaretestfreigabe erteilt; Plan- und gesonderte Ownergates gelten.

## Beobachtung und Aufbau

Owner bestaetigt Luefter an MOSFET 1 und 4; MOSFET 2 und 3 sind nicht angeschlossen. Die Luefter an 1 und 4 liefen laut Owner waehrend der UART-Aufnahme unveraendert und laufen bei schwarzem Display weiter. Klemmen und Verhalten vor `app_main` sowie bei nachgewiesenem Reset sind `NOT_VERIFIED`. Vermutete Zuordnung MOSFET 1/4 zu GPIO16/27 ist kein Funktionsnachweis. Alle auffaelligen physischen Kanaele einschliesslich MOSFET 4 sind Gegenstand der Untersuchung; GPIO27 bleibt softwareseitig Reserve. `ACTUATOR_RELEASE=NO`.

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

Logversion `a9a1156` laesst sich lokal zu Commit `a9a1156a1d44d4679957526bbc34a30be6422e4e` aufloesen. Versionskennung, Buildzeit, kurzer ELF-Hash und Ausgabelog stimmen mit [historischem Kontrollboot](PR210_D1_CONTROL_BOOT_UART_20261025.txt) ueberein. Dies ist ein Indiz, aber kein vollstaendiger Flash-Hash oder Beweis quellgleichen Images bzw. des Buildprofils. Quellstand dieses Commits erzeugt die Ausgaenge 16/17/26 mit `Unconfirmed`; reserve GPIO27 ist nicht generiert. Verbraucher- und Displaybeobachtung des Owners fuer diese Aufnahme angefragt; physisches Bootverhalten bleibt offen.

## Quellenbefund

- Boardprofil und `main/generated/board_profile_r1.hpp` auf obiger Basis: GPIO16/17/26 als jeweilige Ausgaenge mit `Unconfirmed`; GPIO27 nicht generiert. `EspIdfBinaryOutputSink::begin()` ruft bei `Unconfirmed` keine GPIO-Funktion auf und `app_main` verbindet die drei Instanzen nicht mit dem Planner. Das beschreibt den Quellstand; physische Inaktivitaet folgt daraus nicht.
- Quelltextsuche nach direkten `GPIO_NUM_16/17/26/27`, generierten Ausgangskonstanten und GPIO-Zugriffen zeigt diese drei Composition-Root-Verwendungen; sie schliesst indirekte Pin-Konflikte, Bootloader-/IDF-Effekte und Zugriffe des installierten Images nicht aus. Realer Firmwarepfad und Pin-Konfliktfreiheit `NOT_VERIFIED`.
- Lieferanten-PDF `references/datasheets/esp32-32e-quad-mosfet-board-supplier.pdf`: Seite 1 nennt N-Kanal-MOSFETs; Seite 5 beschreibt einen softwaregesteuerten Testmodus der zweikanaligen Variante. Weder elektrische Polaritaet noch Gate-/Treiberbias oder Ausgangszustand der realen vierkanaligen Revision vor Firmwarestart sind damit belegt. PCB-Schaltpfad `NOT_VERIFIED`.
- Historischer Nachweis [PR210 D1 Smoke](PR210_D1_SMOKE_RESULT_20261025.md): am 2026-10-25 wurde Original-App mit Readback-Hash-Praefix `3b907b25…` wiederhergestellt und beim [Kontrollboot](PR210_D1_CONTROL_BOOT_UART_20261025.txt) `App version a9a1156` sowie fuer alle drei Ausgaenge `polarity_unconfirmed_no_gpio_access` protokolliert. Das belegt den damaligen Stand, nicht den aktuell installierten Flashinhalt; aktuelle Imageidentitaet und Buildprofil `NOT_VERIFIED`. Keine Gleichsetzung des damaligen Smoke-Test-Images mit dem danach wiederhergestellten Original.

## Bewertung und offene Nachweise

| Nachweis | Status |
|---|---|
| Luefter an MOSFET 1 und 4 bei UART-Oeffnung aktiv | Ownerbeobachtung, keine verifizierte GPIO-Zuordnung |
| Nach 12-V-Wiedereinschalten laufen Luefter 1 und 4; Display schwarz | Ownerbeobachtung; zeitliche Zuordnung zu `app_main` nicht verifiziert |
| MOSFET 2 und 3 ohne angeschlossene Last | Ownerbeobachtung |
| Klemmen und Bootphasen | NOT_VERIFIED |
| Aktuelles Image und tatsaechlicher Firmwarepfad | NOT_VERIFIED |
| Gate-/Treiberbias, aktive Polaritaet und Resetverhalten | NOT_VERIFIED |
| Ursachenklassen Image, Software oder vor Firmwarestart aktive Stufe | NOT_PROVEN; Kombination moeglich |
| Power-On vor/nach `app_main` | PARTIAL: Owner beobachtet laufende Luefter nach Wiedereinschalten; Zeitbezug zu `app_main` nicht belegt |
| Reset und stabiler Standby vor/nach `app_main` | NOT_RUN |
| Aktive AUS/EIN-Pruefung | NOT_RUN; kein sicherer autorisierter vorhandener Aufrufpfad nachgewiesen |
| `SSOT_CONFORMANCE`, `FUNCTIONAL_HARDWARE_VERIFICATION` | PENDING |

Falls ein Ausgang bereits vor `app_main` aktiv ist, kann ein spaeteres `begin()` diese Zeit nicht absichern. Hardwareeingriff oder neuer produktionsnaher aktiver Pruefpfad erfordern die im Plan vorgesehenen separaten Entscheidungen. Keine Polaritaet oder Massnahme wird vorweggenommen.
