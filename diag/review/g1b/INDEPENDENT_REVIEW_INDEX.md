# G1b Stufe D – S0-Review-Einstieg (kein Hardwarezugriff erfolgt)

Plan: `docs/tasks/issue-27-res01-f2-escalation-plan.md` @ `3bb29481495c4baca540ada1f3ae5433a5b46a34` (Ownerfreigabe F2-D1/D2/D3). Diagnosequelle der Host-Werkzeuge: Commit `f9c2f63` auf dem lokalen Diagnosebranch `agent/pr203-res01-g1-diag-local`; hier unverändert in `diag/` kopiert. **Nicht zum Merge.**

| Gegenstand | Ort |
|---|---|
| Testblatt (Entwurf, Freigabe offen) | `diag/G1B_TEST_SHEET.md` |
| Image | **unverändert G1 IMAGE01** (App `da9e8868…df8cf0`, ELF `24d8557f…8c369`); Hook mit Backtrace bereits enthalten, kein Hook-Delta, kein neuer Build, kein Flash vorgesehen |
| Statische Hookprüfung am gepinnten ELF (`FLASH_REFS=0`) | `diag/review/g1b/HOOK_STATIC_CHECK_G1_ELF.txt` |
| Host-Änderungen | `diag/c0_load.py`, `diag/g1_capture.py`, `diag/g1_pcap.py` (neu) |
| Hosttests (40, grün, ohne Gerät/Netz) | `diag/test_c0_load.py`, `diag/test_g1_capture.py`, `diag/test_g1_pcap.py` |

Prüffragen: (1) Ist die payloadfreie Erfassung (`-s 54` + fail-closed `verify`) belastbar? (2) Ist der UART-Nachlauf rein lesend und die Lastbeendigung unverändert? (3) Ist die Klassifikation `abort_induced`/`error_phase` korrekt und ändert keine bestehende Fehlerklasse? (4) Ist die Vergleichsdefinition (Testblatt Abschnitt 5) vor dem ersten Gate eindeutig?

`G1_IMAGE01=FAIL/HOLD` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO` · `G1B_AUTHORIZATION=PENDING`
