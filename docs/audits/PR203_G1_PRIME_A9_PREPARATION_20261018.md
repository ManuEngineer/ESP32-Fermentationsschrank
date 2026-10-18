# PR #203 – G1′/F4 auf a9: Offline-Vorbereitung

**G1_PRIME_GATE_READY_FOR_OWNER_APPROVAL**. Keine Hardwareaktion ausgeführt.
Der Owner hat die abgeschlossene B0–B4-Ergebnis-Fix-Verification als
Ausgangspunkt dieses Auftrags benannt; deren Ergebnis-/Nachweisgrenzen
bleiben unverändert. Kein erneuter B0–B4-Lauf, G2 oder Pre-Ready.

| Stand/Nachweis | Ergebnis |
|---|---|
| Live-PR vor Beginn | offen/Draft, 2e37b0e3deee3f5770e3f0df3e174074c78711d6 |
| Produktquelle / vorhandenes Release | a9a1156a1d44d4679957526bbc34a30be6422e4e; App 2d069d2db793359177379f492559c69fa949cb470351d57f60dd095889619305 |
| Quellenvergleich | F4/Touch/Render/Partition/Defaults seit d35ee85 unverändert, nur Web-Shell geändert; aktueller PR-Laufzeitcode identisch zu a9 |
| Separates Diagnoseimage | Quelle 203d4d6ad45feb0081110d8e98bec48577da14bc, Branch agent/pr203-res01-g1prime-a9-nonmerge; niemals in Produkt-PR/main übernehmen |
| App / ELF SHA-256 | 69575dd82e4eda123d90e0b873eeec68ed48a29815e94de341c38c6a2268eaa1 / 6a336ef8da29a9f11ab880d96187d9683d2ea870bde02463fc235670f49ff16a |
| Offline | 55 Pythonregressionen, native Null-Allokations-Probe, sauberer kanonischer Release-Build/Profil-/Provenienzguard, offline Image-Info PASS |
| Rückfall | vorhandenes a9-Release samt ELF hashgeprüft privat gesichert; kein Ersatz-Neubuild |
| Neue Hardware-/Touch-/Skip-/Hold-/Peak-Evidence | NOT_RUN / NOT_PROVEN |

[Review-Einstieg und vollständiges Gate](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/b4f745382ea856c0d5e0e5908ba5a784082b3d9b/diag/review/g1prime_a9/README.md)
auf dem bestehenden nicht zu mergenden Reviewbranch; enthält konkrete
Aktionen/Stops, Testblatt, Coverage-Matrix, Artefaktmanifest und Prüfsummen.
[Diagnosediff zur a9-Basis](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/compare/a9a1156a1d44d4679957526bbc34a30be6422e4e...203d4d6ad45feb0081110d8e98bec48577da14bc).
Firmwareoverlay bytegleich aus bb4f43a, Hostmonitor/-helfer aus 893f3bb;
kein neues Framework, NETPROBE1, Stressprofil oder Produktverhalten.

Geplant erst nach konkreter Ownerfreigabe: ein App-only-Flash 0x10000,
notwendige private Vor-/Nach-Lesung, ein kontrollierter Monitorboot,
18 Minuten gezielte reale Touch-/Locale-Coverage bei einem normalen
5-s-Browser; kein Programmstart oder Sensor-/Commissioning-Simulation.
Regulär persistierte Sprachwahl mit Rückkehr zur Ausgangssprache ist im
Gate ausdrücklich benannt. Hersteller-NVS/PHY/Touchkalibrierung und
Netzkonfiguration werden nicht geändert.

G1′-Stack bleibt strict 11160/2080 B; die Owner-Trend-Ausnahme vom 15.10.
gilt nur G2-Web. Historische FAIL/HOLDs bleiben unverändert. Gemeinsame
Heap-Checkpoints belegen kein kausal isoliertes UI-b und keine vollständigen
transienten Spitzen. 16768 B begründen keine 33536-B-Schwelle. Keine
künstliche OOM-/Skip-Erzeugung: ohne natürliche Skip-/Resume-/Hold-Ereignisse
bleibt Hardwarecoverage offen. Die Formel max(6656,b)×2 wird nicht mit
ungesicherter Basis angewendet; keine neue F4-Zahl.

Nächster Schritt ist Review/Ownerentscheidung über genau das vorbereitete
Gate und Image, keine erneute Planungsschleife. Bis dahin kein UART-Portöffnen,
Flash, Reset, Browser- oder Hardwarelauf. D9 bleibt unabhängig offen.

RES-01=BLOCKER; F4_THRESHOLD=PROVISIONAL_PENDING_G1;
G1_QUALIFICATION=NOT_PROVEN; PR203_FINAL_GO=HOLD; ACTUATOR_RELEASE=NO.
