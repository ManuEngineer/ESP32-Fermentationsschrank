# PR203 G1′ – Review-Einstieg (a9, IMAGE01)

**G1_PRIME_GATE_READY_FOR_OWNER_APPROVAL** – Vorbereitung abgeschlossen,
Hardware nicht freigegeben/nicht ausgeführt. Das ist keine F4-Qualifikation.

1. [Konkretes Gate mit Quell-/Image-/ELF-Hashes, Aktionen und Stops](G1_GATE.md)
2. [Gezielte Evidence-/Coverage-Tabelle und Messvorbehalt](COVERAGE.md)
3. [Testblatt: 18 Minuten reale Touch-/Locale-Coverage](G1_TEST_SHEET.md)
4. [Artefaktmanifest](G1_ARTIFACT_MANIFEST.json), [Offline-Verifikation](OFFLINE_VERIFICATION.json), [Testausgabe](OFFLINE_TESTS.txt), [SHA256SUMS](SHA256SUMS)

Diagnosequelle 203d4d6ad45feb0081110d8e98bec48577da14bc auf separatem
Branch agent/pr203-res01-g1prime-a9-nonmerge, Produktbasis a9a1156.
Firmwareoverlay unverändert aus bb4f43a, korrigierte Hostüberwachung aus
893f3bb, sauberer Release-Build samt Profil-/Provenienzprüfung PASS.
Keine Produktverhaltensänderung, Hardwareaktion oder B0–B4-Wiederholung.

Zur konkreten Ownerentscheidung gehören der einmalige App-only-Flash
0x10000 mit drei esptool-Sitzungen/einer Monitoröffnung, strict-Stackpolicy,
ein normaler Browser, kurze nicht destruktive Touchfälle sowie die regulär
persistierten Sprachwechsel und Rückkehr zur Ausgangssprache. Keine
direkte NVS-/Kalibrierungsänderung, kein 5-s-Hold/Reset oder Programmstart.

Das Overlay misst gemeinsame Heap-Checkpoints, kein isoliertes UI-b und
keine vollständigen transienten Spitzen. Ohne natürliche Skip-/Resume-
Hold-Ereignisse bleiben diese Hardwarefälle NOT_PROVEN. Daher keine neue
Schwelle/33536-B-Ableitung; F4_THRESHOLD=PROVISIONAL_PENDING_G1,
G1_QUALIFICATION=NOT_PROVEN. Historische FAIL/HOLDs unverändert.

Review dieses konkreten Gates und Ownerfreigabe vor jeder Geräteaktion.
RES-01=BLOCKER; PR203_FINAL_GO=HOLD; ACTUATOR_RELEASE=NO.
