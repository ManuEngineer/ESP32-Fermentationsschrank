# G1b Stufe D – S0-Review-Einstieg (kein Hardwarezugriff erfolgt)

Plan: `docs/tasks/issue-27-res01-f2-escalation-plan.md` @ `3bb29481495c4baca540ada1f3ae5433a5b46a34` (Ownerfreigabe F2-D1/D2/D3). **Nicht zum Merge.** Massgeblicher Host-Werkzeugstand ist der **gepushte** Stand dieses Branches (`agent/pr203-res01-g1-review-nonmerge`); der frühere lokale Commit `f9c2f63` ist nicht Bezugspunkt. Erster geprüfter Stand: `3f6b1d077b4b61e2b83cce24c2d68047d4287f1d` (Review: CHANGES REQUIRED für das Host-/Testpaket). Korrekturen und Nachweis: [FIX_VERIFICATION.md](FIX_VERIFICATION.md).

| Gegenstand | Ort |
|---|---|
| Testblatt (Entwurf, Freigabe offen) | `diag/G1B_TEST_SHEET.md` |
| Image | **unverändert G1 IMAGE01** (App `da9e8868…df8cf0`, ELF `24d8557f…8c369`); Hook mit Backtrace bereits enthalten, kein Hook-Delta, kein neuer Build, kein Flash |
| Statische Hookprüfung am gepinnten ELF (`FLASH_REFS=0`) | `diag/review/g1b/HOOK_STATIC_CHECK_G1_ELF.txt` |
| Host-Werkzeuge | `diag/c0_load.py`, `diag/g1_capture.py`, `diag/g1_pcap.py` |
| Hosttests | `diag/test_c0_load.py`, `diag/test_g1_capture.py`, `diag/test_g1_pcap.py` |
| Hashes des geprüften Werkzeugstands (Gate-Vorbedingung H0) | `diag/review/g1b/HOST_TOOLS_SHA256SUMS` |

`G1_IMAGE01=FAIL/HOLD` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO` · `G1B_AUTHORIZATION=PENDING` · `OWNER_CONFIRMATION_F2_D4=PENDING`
