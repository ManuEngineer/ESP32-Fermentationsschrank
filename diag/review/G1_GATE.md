> **REVIEW ONLY / NICHT ZUM MERGE.** Bereinigte Veröffentlichung des vorbereiteten
> Gates; keine Imagefreigabe. Nur dieser Dokumentationsstand wird ergänzt;
> Firmware-/Artefaktstand bleibt unverändert auf `bb4f43a69740f234d26a951707991f792cc3ffde`.
> Binärartefakte und private Readbacks werden nicht veröffentlicht; das Manifest
> nennt ausschließlich ihre Namen, Größen, Hashes und Verfügbarkeit.

# Konkretes Owner-G1-Gate PR203_RES01_G1_20261014_IMAGE01

**VORBEREITET – konkrete Imagefreigabe AUSSTEHEND. Kein Hardwarelauf erfolgt.**

Produktbasis: `d35ee85edf4f93f20e9edbe13f8851e406d1b82b` (Owner berichtet unabhängigen Software-Review PASS/GO).
Genehmigter Plan: `6222efce976dc3bea3cf397c2dca56a5fb21859d` plus vorläufige Owner-F4-Option1.
Separater sauberer Diagnose-Commit: `bb4f43a69740f234d26a951707991f792cc3ffde`; Overlay niemals Produkt-PR/main.

| Artefakt | SHA-256 | Bytes |
|---|---|---:|
| app | `da9e868874649bd274d21a4f1c5715afd9293288497027004c460efc46df8cf0` | 1760560 |
| elf | `24d8557f965053d566ba6ef3c4bfe745044a505994835e5831b85ff0dbe8c369` | 36281568 |
| partition | `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d` | 3072 |
| sdkconfig | `9ab3490da37da6bd5a9689eb067c9739276974c0b3cfe20d8c40def2d925c3ea` | 95147 |
| bootloader_NOT_FOR_FLASH | `e5a16dcc941d80b0eb475aecc52fea7c81f4e2e2d7406c61e18704953b0d207c` | 26176 |

**Freizugebender Umfang:** notwendiger kontrollierter Preflight (Identität/Partition/
aktuelles C0-App-Image/privater Nicht-App-Readback), genau EIN App-only-Flash des
oben gepinnten App-Images auf **0x10000**, notwendiger Kandidatenboot/UART-Portöffnung,
danach EIN P-A-Lauf mit vier frischen Sessions (60s Idle,1200s Last,300s Recovery)
plus einzeln bestätigte reale, nicht destruktive Touch-/Hold-/Referenzfälle.
Keine Aktorfreigabe, kein Erase-all, kein Bootloader-/Partition-/NVS-/PHY-Schreiben,
keine Kalibrierung, kein tatsächlicher Werksreset, kein automatischer Rollback/G2.
Bootloaderdatei dient nur Provenienz, NICHT flashen. Vor/nach App-Schreiben
Nicht-App-Regionen0..0xffff UND state_store0x300000..0x3fffff vergleichen;
private Rohdaten nicht auf GitHub. ROM-Operationen sind kontrollierte Preflight-
Phasen, keine fingierten Heartbeat-Lastphasen. Nach Beginn des beobachteten
App-Boots durchgehend frischer Heartbeat<=5s, einschließlich Warten/Bedienung.

Board: USB FT232R A5069RR4 bei Vorbereitung per OS enumeriert; erwartete MAC
20:50:0d:1b:2f:34, Partitionhash wie Tabelle. MAC/Partition/C0-Image bisher
historisch G0-R3; unter diesem Gate VOR Schreiben frisch bestätigen. Abweichung
oder unklarer Zustand => HOLD ohne improvisierte Reparatur. Aktortrennung vor Ort
bestätigen, UART muss real actuators disabled / Source / Release / Ready zeigen.

**Rückfall vorhanden und vollständig offline verifiziert (keine Ausführung):**
- `diag/c0/image_c0_9a9a8bd_release_app.bin`: `334623253a70eb6fe11d0be93e6ee0897775ae53773291c2616679cb38b33806`
- `diag/c0/elf_c0_9a9a8bd_release.elf`: `5cd325c3772c566e559cc18c0dd45e08db1823962a39be05de042cdc6299d18e`
- `image_ed29c9c_release_app.bin`: `c0757d83f9c15940723b4d65a48282cd41db7defa492784cfa0603a438221e0d`
- `flash_factory_app_backup_b9564e6.bin`: `4708f474a97f9a37edecea2d07b0a74890e0597f87b45fe3bffba3b95857d4da`

**Feste Mess-/Abnahmeregeln:** [Bereinigtes Testblatt](G1_TEST_SHEET.md),
[Artefaktmanifest mit Pins und Auswertungsdefinitionen](G1_ARTIFACT_MANIFEST.json),
[Grenzen](NOT_RUN_AND_LIMITS.md). Recovery-Referenz: Median beider aktuellen
internen8-Bit-Metriken aus finalen30s des Vier-Session-Idle; vergleichbare Erholung
beider Werte>=90% innerhalb120s ab letzter Antwort/geschlossenen Clientsockets.
Fehlende Referenz/Zustandsvergleichbarkeit => NOT_PROVEN/HOLD. Socketzustände
per Host-ss, UART/HomeConnected, gleiche nicht ausgeloggte vier Sessions; keine
zusätzlichen Geräteanfragen zur Zustandsprüfung. TIME_WAIT separat dokumentieren.
Main>=11160 B, HTTPD>=2080 B; Fehlerfreiheit zwingend. Login8s, GET3s Socketbudget,
max5s frische Heartbeat-Lücke. Keine nachträglichen Latenzgrenzen; G0-min_free5228 B
und fünf GET-Gesamtdauern>3s/max5431.081ms bleiben sichtbar.

**Erste Anomalie:** heap_alloc_failed, Panic/Abort/WDT/Brownout/unerwarteter Reset,
HTTP-/Transport-/Timeoutfehler, UART-Ausfall>5s, Identitäts-/Stackfehler oder
Probe-Evidenzverlust => sofort FAIL/HOLD, keine neuen Requests, aktive Sockets
schließen, Evidence sichern; danach KEINE weitere Geräteabfrage/Reset/Flash/
Rollback/Folgephase ohne neue Freigabe. Uptime-Rücksprung/Hostabbruch ebenfalls Fail.

**Touch:** jeweils konkrete Anleitung und Ownerbestätigung abwarten. Keine
Kalibrierungs-/NVS-Reparatur und kein echter Werksreset; ungültige Bedienbarkeit
oder nicht erreichbare Zustands-/Sprach-/Textfälle => NOT_PROVEN. Skip ist keine
Bedarf0-Messung; Gate wird nicht umgangen. Checkpoints verpassen Zwischenpeaks;
fehlende belastbare Peak-/Pfad-Coverage hält Schwellenqualifikation HOLD, auch
bei fehlerfreiem HTTP-Lauf. Analyzer liefert nur eine nicht qualifizierte
Rechengröße2×max(6656, Stichprobenbedarf), keine OOM-Sicherheitszusage.

**Umfangsgrenze:** Dieses erste Gate ist keine vollständige G1-/PR-Abnahme.
Gesamtmatrix P-A3×20min/P-A2/P-B/P-R bleibt unverändert; fehlende diagnostische
Nachweise OPEN_G1, reine Produktbestätigung OPEN_G2, spätere genaue Laufzuordnung
und Runner/Browserversionen vor deren Gate fixieren. Nach G1 Ergebnisreview,
kein automatisches G2. Pre-Ready erst nach Hardwarequalifikation auf finalem HEAD.

**Builderbelege:** Release/Profilvalidierung,29/29 Offline-Tests, nativer Probe-Test,
ADR-013, Secretcheck, Self-Check PASS. Keine Hardware-/Touch-/Recovery-PASS-Behauptung,
keine Produktänderung. [exakter Overlay-Diff d35ee85...bb4f43a](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/compare/d35ee85edf4f93f20e9edbe13f8851e406d1b82b...bb4f43a69740f234d26a951707991f792cc3ffde), [Befehle](COMMANDS.md).

OWNER_G1_IMAGE_AUTHORIZATION=PENDING; RES-01=BLOCKER; PR203_FINAL_GO=HOLD;
G2=NOT_AUTHORIZED; ACTUATOR_RELEASE=NO.
