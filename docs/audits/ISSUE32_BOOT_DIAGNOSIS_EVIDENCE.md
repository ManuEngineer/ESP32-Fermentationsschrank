# Issue #32 – Bootdiagnose: vorlaeufige Evidence

Stand 2026-11-17. Ownerfreigabe Plan `67f40f82b875fb4864247895971aee9fa1f4c180` fuer PR #212; Ausgangsbasis `main` `9233226da3cf624eeefce468c219dafc0653caae`. Allgemeine Hardwaretestfreigabe erteilt; Plan- und gesonderte Ownergates gelten.

## Beobachtung und Aufbau

Owner bestaetigt Luefter an MOSFET 1 und 4; MOSFET 2 und 3 sind nicht angeschlossen. Die Luefter an 1 und 4 liefen laut Owner waehrend der UART-Aufnahme unveraendert und laufen bei schwarzem Display weiter. Klemmen und Verhalten vor `app_main` sowie bei nachgewiesenem Reset sind `NOT_VERIFIED`. Vermutete Zuordnung MOSFET 1/4 zu GPIO16/27 ist kein Funktionsnachweis. Alle auffaelligen physischen Kanaele einschliesslich MOSFET 4 sind Gegenstand der Untersuchung; GPIO27 bleibt softwareseitig Reserve. `ACTUATOR_RELEASE=NO`.

Owner bestaetigt `/dev/ttyUSB0` als verbundenen UART-Adapter und Beobachtungsbereitschaft. [Fruehere Evidence](PR210_D1_SMOKE_RESULT_20261025.md) dokumentiert einen Reset beim Oeffnen. Am 2026-11-17 wurde der Port nach Koordination fuer 20 Sekunden lesend mit 115200 Baud geoeffnet; kein expliziter DTR-/RTS-Puls, aber ein Reset durch das Oeffnen ist nicht ausgeschlossen. Empfang: 278 Nullbytes, keine lesbare Boot-/Firmwaremeldung. Ob ein Reset stattfand, ist aus dem Mitschnitt nicht bestimmbar; Ursache der Nullbytes unbekannt. **Ownerbeobachtung:** Luefter an MOSFET 1 und 4 liefen waehrend der UART-Oeffnung ohne erkennbare Aenderung weiter; das Display wurde schwarz und blieb schwarz. Diese Beobachtung belegt weder einen Reset noch das Verhalten vor `app_main`; Zustand des Displays vor der Aufnahme ist nachgefragt. Ein ROM-Bootloader ist eine Moeglichkeit, aber nicht belegt.

Der Builder oeffnete den Port anschliessend unbeabsichtigt erneut durch `stty -F /dev/ttyUSB0 -a` zur Abfrage der Einstellungen; kein Steuerimpuls wurde bewusst gesendet, ein durch Oeffnung ausgeloester Reset ist nicht ausgeschlossen. Diese zweite Oeffnung war nicht separat mit dem Owner koordiniert. Einstellungen zeigten 115200 Baud, 8 Datenbits, kein Parity, ein Stopbit; kein Zustands- oder Image-Nachweis. Gueltiger Bootlog und nachgewiesener neuer Boot/Reset `NOT_VERIFIED`. BTS7960 laut Auftrag ohne 12-V-Leistungsversorgung und ohne Peltier.

## Quellenbefund

- Boardprofil und `main/generated/board_profile_r1.hpp` auf obiger Basis: GPIO16/17/26 als jeweilige Ausgaenge mit `Unconfirmed`; GPIO27 nicht generiert. `EspIdfBinaryOutputSink::begin()` ruft bei `Unconfirmed` keine GPIO-Funktion auf und `app_main` verbindet die drei Instanzen nicht mit dem Planner. Das beschreibt den Quellstand; physische Inaktivitaet folgt daraus nicht.
- Quelltextsuche nach direkten `GPIO_NUM_16/17/26/27`, generierten Ausgangskonstanten und GPIO-Zugriffen zeigt diese drei Composition-Root-Verwendungen; sie schliesst indirekte Pin-Konflikte, Bootloader-/IDF-Effekte und Zugriffe des installierten Images nicht aus. Realer Firmwarepfad und Pin-Konfliktfreiheit `NOT_VERIFIED`.
- Lieferanten-PDF `references/datasheets/esp32-32e-quad-mosfet-board-supplier.pdf`: Seite 1 nennt N-Kanal-MOSFETs; Seite 5 beschreibt einen softwaregesteuerten Testmodus der zweikanaligen Variante. Weder elektrische Polaritaet noch Gate-/Treiberbias oder Ausgangszustand der realen vierkanaligen Revision vor Firmwarestart sind damit belegt. PCB-Schaltpfad `NOT_VERIFIED`.
- Historischer Nachweis [PR210 D1 Smoke](PR210_D1_SMOKE_RESULT_20261025.md): am 2026-10-25 wurde Original-App mit Readback-Hash-Praefix `3b907b25…` wiederhergestellt und beim [Kontrollboot](PR210_D1_CONTROL_BOOT_UART_20261025.txt) `App version a9a1156` sowie fuer alle drei Ausgaenge `polarity_unconfirmed_no_gpio_access` protokolliert. Das belegt den damaligen Stand, nicht den aktuell installierten Flashinhalt; aktuelle Imageidentitaet und Buildprofil `NOT_VERIFIED`. Keine Gleichsetzung des damaligen Smoke-Test-Images mit dem danach wiederhergestellten Original.

## Bewertung und offene Nachweise

| Nachweis | Status |
|---|---|
| Luefter an MOSFET 1 und 4 bei UART-Oeffnung aktiv | Ownerbeobachtung, keine verifizierte GPIO-Zuordnung |
| MOSFET 2 und 3 ohne angeschlossene Last | Ownerbeobachtung |
| Klemmen und Bootphasen | NOT_VERIFIED |
| Aktuelles Image und tatsaechlicher Firmwarepfad | NOT_VERIFIED |
| Gate-/Treiberbias, aktive Polaritaet und Resetverhalten | NOT_VERIFIED |
| Ursachenklassen Image, Software oder vor Firmwarestart aktive Stufe | NOT_PROVEN; Kombination moeglich |
| Power-On, Reset und Standby vor/nach `app_main` | NOT_RUN |
| Aktive AUS/EIN-Pruefung | NOT_RUN; kein sicherer autorisierter vorhandener Aufrufpfad nachgewiesen |
| `SSOT_CONFORMANCE`, `FUNCTIONAL_HARDWARE_VERIFICATION` | PENDING |

Falls ein Ausgang bereits vor `app_main` aktiv ist, kann ein spaeteres `begin()` diese Zeit nicht absichern. Hardwareeingriff oder neuer produktionsnaher aktiver Pruefpfad erfordern die im Plan vorgesehenen separaten Entscheidungen. Keine Polaritaet oder Massnahme wird vorweggenommen.
