# PR #203 – Single-Flight G2-Web: Flash-/Browser-Gate

**Stand: `HOLD_NOT_READY_FOR_FLASH`.** Dieses Gate ist die read-only Vorbereitung
für genau einen App-only-Flash und einen anschliessenden manuellen B0–B4-Lauf.
Die Firmware wurde nicht geflasht; kein Browser-/Gerätezugriff, UART-Port,
Reset oder Testlauf wurde in dieser Vorbereitung gestartet.

## Geprüfte Softwarebasis und Image

| Gegenstand | Geprüfter Stand |
|---|---|
| Live-PR | #203 offen/Draft, Branch `agent/issue-27-r1-web-dashboard-plan` |
| PR-HEAD bei Vorbereitung | `f7ba3d4f207d87f6c29e7c2157698932a1872ef0` |
| Freigegebener Single-Flight-Plan | `8094b7f1a149269b0ad3e1c49cf4366bdfc40e7c` |
| Produkt-/Implementierungsquelle | `a9a1156a1d44d4679957526bbc34a30be6422e4e` |
| Profil / Plattform | `esp32_release` / ESP32, vorhandene Partitionierung |
| App-Datei | `build/esp32_release/esp32_fermentationsschrank.bin`, 1 756 544 B |
| App SHA-256, lokal nachgerechnet | `2d069d2db793359177379f492559c69fa949cb470351d57f60dd095889619305` |
| ELF-Datei | `build/esp32_release/esp32_fermentationsschrank.elf`, 36 246 576 B |
| ELF SHA-256, lokal nachgerechnet | `dca3cc664f3c143797f1caf29ca51bd3b9afe88eb081f914bec595f065523601` |
| Offline-Build-/Quellnachweis | `docs/audits/PR203_SINGLE_FLIGHT_POLLING_20261016/VERIFICATION.json`; sauberer Quellbuild auf `a9a1156`, Image-Info PASS |

Die nachgerechneten Binärhashes stimmen mit dem versionierten Buildnachweis
überein. Es gab keinen Neubuild. Die implementierte Änderung ist die geprüfte
Single-Flight-Polling-Implementierung; dieser Auftrag nimmt keine weiteren
Produktcodeänderungen vor.

## Board, Installationsziel und bekannte Identität

- Ziel ist das dauerhaft angeschlossene, aktor- und sensorlose ESP32-Devboard;
  vorherige Evidence identifiziert ESP32-D0WD-V3 rev 3.1. Boardkennung und
  MAC bleiben im privaten Preflight; diese Angaben sind **historische**
  Evidence und werden vor Ausführung read-only erneut geprüft.
- Installationsziel: ausschliesslich App-Partition ab `0x10000`; genau ein
  App-only-Flash ohne Erase. Bootloader, Partitionstabelle, NVS, PHY und
  Touchkalibrierung bleiben unangetastet. Vorher/nachher werden vorhandene
  Nicht-App-Bereiche gemäss freigegebenem Verfahren verglichen.
- Letzter dokumentierter Image-Stand: C5-Produktimage aus Quelle
  `162a6670526a10b2fd957fe90ab665a1fae837b7`, App-SHA-256
  `ebb296c08b79045e6e081f328c9f5fd7e576e20722670968b131e722a8b36633`,
  1 756 528 B. Die Identität stammt aus vorhandener Flash-/Readback-/UART-
  Evidence; es gibt **keine frische Geräteidentifikation** in dieser
  Vorbereitung.
- **Rückfallartefakt: HOLD.** In den geprüften lokalen und versionierten
  Artefakten wurde keine verfügbare, bytegenau identifizierte C5-App-Datei als
  Rückfallimage gefunden. Das vorhandene ältere Produkt-Rückfallimage wird
  nicht ohne Kompatibilitäts-/Ownerentscheidung an Stelle von C5 eingesetzt.
  Ein privater vollständiger Readback ist nicht im Repository und wird nicht
  veröffentlicht. Vor Freigabe muss der Owner bzw. der zuständige Artefakt-
  Speicher die C5-App-Datei samt SHA/Grösse bereitstellen oder bestätigen;
  danach Hash offline nachrechnen und mit C5-Evidence abgleichen.

## Gate vor jeglichem Schreibvorgang

Unmittelbar vor einem freigegebenen Flash muss ein read-only Preflight frisch
bestätigen: Board/MAC/USB-UART, Partitionstabelle, derzeit installierte App,
Quelle/Hash des Rückfallimages, Aktortrennung, Image-/ELF-Hash des Kandidaten
und unveränderte Nicht-App-Bereiche. Eine Abweichung, fehlende laufende UART-
Evidence oder ungeklärte Identität bedeutet `HOLD`; keine Reparatur, kein
Ersatzimage und kein Flash.

Vorgesehener Bootnachweis nach dem einzelnen Flash: Kandidaten-Source-SHA
`a9a1156a1d44d4679957526bbc34a30be6422e4e`, Profil `esp32_release`, erwartete
App-Version, `real actuators: disabled`, `ready` sowie frische Heartbeats.
Flash-/Digest- und Bootnachweise werden lokal gesichert und nur bereinigt
veröffentlicht. Flash- oder Identitätsfehler führen zu `STOP/HOLD`; kein
automatischer Retry, Rollback, Reset oder weiteres Image.

## Browserablauf, erst nach Ausführungsfreigabe

Chrome `154.0.8037.97` im Selkies-Docker, ein ESP32-Tab, lokales LAN
`http://192.168.1.68/`. Andere bekannte Browser/Testclients schliessen;
unbekannte Clients als Einschränkung festhalten. Keine Browserautomation,
Netzwerkprobe, Mehrfachclients oder künstlich verlangsamten Requests. Vor B0
praktikable lokale Sicherung der Request-Metadaten/Fehlerklasse festlegen;
keine öffentlichen HAR-Originale, Header, Cookies, Passwörter, Tokens oder
Bodies.

| Phase | Umfang und Dauer |
|---|---|
| B0 | Zeitreferenz, Browser/Image, Host-/UniFi-Zustand und Trace vorbereiten; einzelne Navigation |
| B1 | Einmal Shell öffnen und normal anmelden; anonyme 401 getrennt werten; nach Login Screenshot und fehlende/stale Sensoren dokumentieren |
| B2 | Genau 15 min ein Tab mit normalem 5-s-Polling, keine Mutationen; Status/Fehler/Latenzen bei 0/5/10/15 min; Requestgruppen anhand Start, Ende und Bodyabschluss prüfen |
| B3 | Trace sichern, Tab vollständig schliessen, 20 min keinerlei Geräte-HTTP, Ping oder Erreichbarkeitstest; UniFi read-only bei 0/10/20 min |
| B4 | Nach 20 min Tab einmal öffnen, falls nötig einmal normal anmelden; erste Dokumentfehlerklasse vor jedem weiteren Versuch sichern; bei Erfolg normales Polling bis 10 min mit 0/5/10-Nachweisen |

Bekannte alte Funkbeobachtung: 2,4 GHz / `AC Pro Wohnzimmer`, RSSI etwa
−84…−87 dBm und erhöhte Retries. Das ist ein Repräsentativitätsrisiko, keine
Ursachenfeststellung oder RSSI-Abnahmegrenze. Aktuelle UniFi-Werte müssen der
Owner zu Testbeginn read-only ablesen und mit Zeit dokumentieren. Keine
Änderung an AP, Kanal, WLAN-Powersave, TCP oder HTTPD.

Der frühere Favicon-404 wurde für den vorherigen Lauf akzeptiert, gilt für
diesen neuen Lauf jedoch **nicht automatisch**: `/favicon.ico`-404 darf nur
mit erneuter ausdrücklicher Ownerausnahme als Ausnahme behandelt werden.
Unerwartete HTTP-/Transport-/Konsolenfehler, Timeout, Nichterreichbarkeit,
Heap-Allokationsfehler, Panic/Abort, WDT, Brownout, Reset oder andere
sicherheitsrelevante Anomalie: sofort Trace/Uhrzeit sichern, Owner schliesst
den Tab, `STOP/HOLD`; kein Retry, Reset, Flash, Rollback oder Folgelauf.

## Fehlende Freigabe und Abnahmerahmen

Der Auftraggeber hat die Vorbereitung und den Flash grundsätzlich gestattet;
die Tests sollen bis zur Rückkehr des Owners warten. Das beigefügte Verfahren
verlangt zusätzlich das konkrete Gate und ausdrückliche Freigabe für genau
dieses Image und den einzelnen B0–B4-Lauf. Der neue Favicon-404 benötigt eine
separate erneute Ausnahme. In diesem Gate sind diese Ausführungsschritte noch
nicht freigegeben.

Vor Gatefreigabe noch erforderlich:

1. Passendes, unverändert gesichertes C5-App-Rückfallartefakt mit geprüftem
   Hash/Grösse verfügbar machen.
2. Frischen read-only Board-/Image-/Partitions-/Aktor-Preflight vorlegen.
3. Zu Testbeginn aktuelle UniFi-Beobachtung und sichere Trace-Sicherung
   festlegen.
4. Owner bestätigt konkret: genau ein App-only-Flash dieses SHA auf `0x10000`,
   danach genau ein B0–B4-Lauf; Favicon-404-Ausnahme separat ja/nein.

Statusgrenzen bleiben unverändert: `B4_RECONNECT=FAIL_OBSERVED`,
`G1_QUALIFICATION=NOT_PROVEN`, `F4_THRESHOLD=PROVISIONAL_PENDING_G1`,
`PR203_FINAL_GO=HOLD`, `ACTUATOR_RELEASE=NO`. Ein erfolgreicher Lauf wäre
höchstens `G2_WEB=PASS_PARTIAL`; F4, Touch/Hold/Skip, Heap-Referenz/
90-%-Erholung, autonomer Betrieb und Release bleiben damit unqualifiziert.
