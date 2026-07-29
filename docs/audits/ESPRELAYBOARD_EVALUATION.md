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

Der Owner hat eine Reddit-Referenz gefunden, laut der die Firmware auf dem
konkret bestellten ESP32-32E-Quad-MOSFET-Board eingesetzt wurde. Diese Aussage
ist fuer die Kandidatenaufnahme relevant, aber noch kein technischer Nachweis:
Die genaue Reddit-URL, Boardbezeichnung, Revision, Bilder und Pinbelegung sind
noch nachzutragen und an der realen Hardware zu bestaetigen.

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
| konkretes Quad-MOSFET-Board | externe Ownerquelle behauptet Einsatz | Quelle und Hardware-Smoke-Test fehlen |
| einfache binaere GPIO-Ausgaenge | vorhanden | potenziell direkt nutzbar |
| aktive HIGH-/LOW-Pegel | konfigurierbar | reale Boardpolaritaet und Bootpegel messen |
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

- ESPRelayBoard nur fuer Boardidentifikation, Pinmapping, Pegel und
  Hardware-Bring-up verwenden;
- keinen GPLv3-Code kopieren;
- vorhandene Arduino-/PlatformIO-Architektur, Tests, Persistenz-, Safety- und
  Fachvertraege behalten;
- nur einen kleinen projektspezifischen GPIO-/MOSFET-Adapter erstellen.

### Weg B: GPLv3-Fork als Firmwarebasis

- ESPRelayBoard als ESP-IDF-C-Basis forken;
- Fermentationskern, Touchdisplay, Sensoren, BTS7960, Safety, Programme,
  Persistenz und Recovery integrieren oder portieren;
- vorhandenen C++-/PlatformIO-Projektkern entweder portieren, parallel anbinden
  oder verwerfen;
- Gesamtprojekt bei Verteilung GPLv3-kompatibel gestalten.

Die Behauptung, bei Weg B muesse "fast nichts mehr" entwickelt werden, ist vor
dem Feature-Gap- und Portierungsnachweis nicht belegt. Der Upstream deckt
mehrere Infrastruktur- und Komfortfunktionen ab, aber nicht die zentralen
Fermentations-, Display-, Sensor-, Peltier-, Safety- und Recoveryfunktionen.

## Verbindlicher Spike vor Auswahl

1. Reddit-URL und exakte Boardrevision dokumentieren.
2. Boardfotos, Aufdrucke, Schaltplan oder nachvollziehbares Pinmapping sichern.
3. Unveraenderten Upstreamstand reproduzierbar bauen und auf dem Zielboard
   flashen.
4. Alle vier Kanaele ohne Last messen:
   - GPIO-Zuordnung;
   - aktive Polaritaet;
   - Pegel bei Reset, Bootloader, Boot, Provisioning und Neustart;
   - Zustand nach Stromunterbruch;
   - Eignung fuer binaeres Schalten beziehungsweise PWM.
5. Keine reale Last anschliessen, bevor sichere Pegel und Strompfade bestaetigt
   sind.
6. Weg A und Weg B mit derselben Matrix vergleichen:
   - bereits nutzbarer Code;
   - neu zu entwickelnder oder zu portierender Code;
   - Safety- und Securityluecken;
   - Toolchain- und Architekturwechsel;
   - Flash, RAM, Heap und Wartungsaufwand;
   - Testbarkeit und Hardwareabhaengigkeit;
   - Lizenz- und Veroeffentlichungsfolgen.
7. Danach einen dokumentierten Ownerentscheid treffen:
   `REFERENCE_ONLY`, `BOARD_SUPPORT_REFERENCE`, `PARTIAL_ADOPTION` oder
   `GPLV3_FORK_BASE`.

## Vorlaeufige Entscheidung

`ESPRelayBoard` ist nicht mehr nur `REFERENCE_ONLY`, sondern ein ernsthaft zu
pruefender Board- und Firmwarebasiskandidat. Vor Quelle, Hardwaretest,
Feature-Gap-Vergleich und Lizenzentscheid wird jedoch weder Code uebernommen
noch die bestehende Device-Platform aufgegeben.
