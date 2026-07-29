# Evaluation: rpavlyuk/ESPRelayBoard

## Status

`SPIKE_REQUIRED` · `LICENSE_REVIEW_REQUIRED` · `FINAL_SELECTION_PENDING`

Diese Evaluation bindet keine Abhaengigkeit ein, uebernimmt keinen Code und
trifft noch keinen Architektur- oder Lizenzentscheid. Sie gehoert zu Issue #69.

## Anlass

[`rpavlyuk/ESPRelayBoard`](https://github.com/rpavlyuk/ESPRelayBoard) ist eine
aktiv gepflegte ESP-IDF-Firmware fuer ESP32-basierte Relayboards und allgemein
fuer per `HIGH`/`LOW` gesteuerte digitale Ausgaenge. Gepruefter Upstreamstand:
Commit `82c6d117ea4b0c26b7e83641fb246c508ed977a9` vom 2026-04-11.

Der Owner hat folgende Reddit-Quelle nachgereicht:

- [Unable to program ESP32-WROOM-32E relay board](https://www.reddit.com/r/esp32/comments/1czys44/unable_to_program_esp32wroom32e_relay_board/)

Die Quelle zeigt ein Vierfach-Relaisboard mit ESP32-WROOM-32E ohne integrierten
USB-Anschluss und beschreibt die Programmierung ueber einen externen
USB-/UART-Adapter. Sie zeigt **nicht** das bestellte ESP32-32E-Quad-MOSFET-Board,
nennt `rpavlyuk/ESPRelayBoard` nicht und belegt keine Ausfuehrung dieser Firmware.

Relais- und MOSFET-Variante koennen Layout-, Versorgungs-, Programmierheader-
oder GPIO-Gemeinsamkeiten besitzen. Aus optischer Aehnlichkeit und gleichem
ESP32-Modul folgt jedoch keine identische Leiterplatte, Kanalbelegung,
Ausgangspolaritaet, Schutzbeschaltung oder Bootpegelsemantik. Diese Punkte
bleiben bis zum Vergleich der realen Hardware unbestaetigt.

## Upstreamumfang

Der Upstream bietet unter anderem:

- konfigurierbare digitale Aktoren und invertierte Pegel;
- Trockenkontaktsensoren;
- ESP-IDF-SoftAP-Provisioning;
- Weboberflaeche und JSON-API;
- NVS-Einstellungen;
- MQTT und Home-Assistant-Discovery;
- OTA, Netzwerklogging und Speicherueberwachung.

Der Upstream ist eine vollstaendige, eng gekoppelte ESP-IDF-C-Firmware und
keine kleine Relaisbibliothek. GPIO, NVS, FreeRTOS, Web, MQTT und globale
Dienstzustaende sind direkt miteinander verbunden.

## Lizenz

Der Upstream steht unter `GPL-3.0`.

Eine unveraenderte private Nutzung ist von einer Verteilung zu unterscheiden.
Bei einer verteilten abgeleiteten Firmware waeren die konkreten GPLv3-Pflichten
und die Kompatibilitaet aller eingebundenen Bestandteile vorab vollstaendig zu
pruefen. Der Owner ist grundsaetzlich bereit, GPLv3 als Gesamtprojektlizenz zu
pruefen, falls dadurch ein wesentlicher Entwicklungsaufwand entfaellt. Dies ist
noch kein endgueltiger Lizenzentscheid.

Wichtig: GPLv3 verhindert keine kommerzielle Nutzung durch Dritte. Dritte duerfen
GPLv3-Software auch verkaufen, muessen dabei aber die Lizenzpflichten und die
Freiheiten der Empfaenger einhalten. Eine fruehere Anforderung, kommerzielle
Nutzung durch Dritte auszuschliessen, waere mit GPLv3 nicht vereinbar und muss
bei einer Auswahl bewusst aufgegeben werden.

## Funktionsueberdeckung

| Projektbedarf | ESPRelayBoard | Bewertung |
|---|---|---|
| konkretes Quad-MOSFET-Board | nicht nachgewiesen | Reddit zeigt separates Vierfach-Relaisboard; reale Hardwarepruefung erforderlich |
| externer USB-/UART-Programmierpfad | beim Reddit-Relaisboard beschrieben | moegliche Referenz fuer Programmierheader und Bootmodus, nicht fuer Ausgangsstufe |
| einfache binaere GPIO-Ausgaenge | vorhanden | potenziell nutzbar nach bestaetigter Pin- und Pegelzuordnung |
| aktive HIGH-/LOW-Pegel | konfigurierbar | reale MOSFET-Polaritaet und Bootpegel messen |
| WLAN-Onboarding | ESP-IDF-SoftAP-Provisioning vorhanden | alternative Basis, aber nicht unser bisheriger Arduino-Adapter |
| Weboberflaeche und JSON-API | vorhanden | fachlich nicht auf Fermentation zugeschnitten |
| MQTT/Home Assistant | vorhanden | fuer R1 nicht Kernanforderung |
| OTA | vorhanden | fuer R1 bisher verschoben |
| DS18B20 und drei Sensorrollen | nicht vorhanden | neu zu integrieren |
| Produkt-/Raum-/Schutzsensor-Semantik | nicht vorhanden | projektspezifisch |
| BTS7960 mit exklusiver Richtung und Totzeit | nicht vorhanden | projektspezifischer Adapter und Safety erforderlich |
| zwei Luefter und Summer mit Rollenlogik | nur generische Ausgaenge | Rollen, Nachlauf und Safety fehlen |
| ILI9341/XPT2046-Touchdisplay | nicht vorhanden | vollstaendig zu integrieren |
| Programme, Laufzustandsmaschine und Regelung | nicht vorhanden | bestehender Projektkern bleibt erforderlich |
| typisierte Safety- und Aktorfreigabe | nicht vorhanden | bestehende Vertraege duerfen nicht entfallen |
| versionierte Konfigurationsgraphen und Recovery | nicht vorhanden | Upstream-NVS-Modell ersetzt #54–#57 nicht gleichwertig |
| aktive/fallbackfaehige Persistenz | nicht vorhanden | bestehender Projektkern bleibt erforderlich |

## Zwei zu vergleichende Wege

### Weg A: bestehende Device-Platform fortsetzen

- Reddit-Relaisboard nur als begrenzte Referenz fuer externes Flashen und
  moegliche Familienaehnlichkeiten verwenden;
- `ESPRelayBoard` fuer generische Infrastrukturideen, nicht als bestaetigten
  Boardtreiber behandeln;
- keinen GPLv3-Code kopieren;
- vorhandene Arduino-/PlatformIO-Architektur, Tests, Persistenz-, Safety- und
  Fachvertraege behalten;
- nur einen kleinen projektspezifischen GPIO-/MOSFET-Adapter erstellen.

### Weg B: GPLv3-Fork als Firmwarebasis

- `ESPRelayBoard` als ESP-IDF-C-Basis forken;
- Fermentationskern, Touchdisplay, Sensoren, BTS7960, Safety, Programme,
  Persistenz und Recovery integrieren oder portieren;
- vorhandenen C++-/PlatformIO-Projektkern entweder portieren, parallel anbinden
  oder verwerfen;
- Gesamtprojekt bei Verteilung GPLv3-kompatibel gestalten.

Die Behauptung, bei Weg B muesse "fast nichts mehr" entwickelt werden, ist vor
dem Feature-Gap- und Portierungsnachweis nicht belegt. Der Upstream deckt
mehrere Infrastruktur- und Komfortfunktionen ab, aber nicht die zentralen
Fermentations-, Display-, Sensor-, Peltier-, Safety- und Recoveryfunktionen.
Die Reddit-Quelle aendert daran nichts, weil sie weder diese Firmware noch unser
MOSFET-Board nachweist.

## Verbindlicher Spike vor Auswahl

1. Produktlink, Boardaufdrucke, Vorder-/Rueckseitenfotos und Revision des
   bestellten Quad-MOSFET-Boards dokumentieren.
2. Reddit-Relaisboard und MOSFET-Board nur anhand belegbarer Merkmale vergleichen:
   ESP32-Modul, Versorgung, Programmierheader, Taster, Leiterplattenlayout,
   Bestueckung, Ausgangsstufe und Kanalbeschriftung.
3. Schaltplan oder nachvollziehbares Pinmapping fuer das MOSFET-Board sichern.
4. Programmierzugang zuerst mit einem kleinen neutralen Testprogramm pruefen;
   die Reddit-Quelle darf als Hinweis auf externen USB-/UART-Adapter und
   Bootmodus dienen.
5. Einen unmodifizierten `ESPRelayBoard`-Build erst nach geklaertem Pinmapping
   ohne Last und mit sicher deaktivierten Ausgaengen flashen.
6. Alle vier MOSFET-Kanaele ohne Last messen:
   - GPIO-Zuordnung;
   - aktive Polaritaet;
   - Pegel bei Reset, Bootloader, Boot, Provisioning und Neustart;
   - Zustand nach Stromunterbruch;
   - Eignung fuer binaeres Schalten beziehungsweise PWM.
7. Keine reale Last anschliessen, bevor sichere Pegel und Strompfade bestaetigt
   sind.
8. Weg A und Weg B mit derselben Matrix vergleichen:
   - bereits nutzbarer Code;
   - neu zu entwickelnder oder zu portierender Code;
   - Safety- und Securityluecken;
   - Toolchain- und Architekturwechsel;
   - Flash, RAM, Heap und Wartungsaufwand;
   - Testbarkeit und Hardwareabhaengigkeit;
   - Lizenz- und Veroeffentlichungsfolgen.
9. Danach einen dokumentierten Ownerentscheid treffen:
   `REFERENCE_ONLY`, `PROGRAMMING_REFERENCE`, `PARTIAL_ADOPTION` oder
   `GPLV3_FORK_BASE`.

## Vorlaeufige Entscheidung

`ESPRelayBoard` bleibt ein ernsthaft zu pruefender allgemeiner
Firmwarebasiskandidat. Die Reddit-Quelle stuetzt nur die Existenz und den
externen Programmierweg eines aehnlichen Vierfach-Relaisboards mit
ESP32-WROOM-32E. Sie liefert keinen Nachweis fuer unser Quad-MOSFET-Board und
keinen Nachweis, dass `ESPRelayBoard` darauf laeuft. Vor Hardwaretest,
Feature-Gap-Vergleich und Lizenzentscheid wird weder Code uebernommen noch die
bestehende Device-Platform aufgegeben.
