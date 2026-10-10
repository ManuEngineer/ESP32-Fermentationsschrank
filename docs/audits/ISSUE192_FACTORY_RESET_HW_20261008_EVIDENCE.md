# Issue #192 – Werksreset-Hardwareverifikation: Stand nach P1 und nicht destruktivem R01 (2026-10-08)

> **HISTORISCHE EVIDENCE (Stand 2026-10-08, Baseline `df5a3dd`).** Diese Datei
> dokumentiert unveraendert den damaligen Lauf und ist **nicht** der aktuelle
> Zustand. Seither gilt: PR #199 (Service-PIN-Einstieg, #188 A) ist in `main`
> gemergt (`21082de`); der unten genannte Zugangsblocker (Service-Menue/PIN-Seite
> am Geraet nicht aufrufbar) ist im Produktcode behoben. Die Ergebnisse
> `HW-19-R01=BLOCKED`, `HW-19-R03=BLOCKED`, `HW-19-R02=NOT_RUN` gelten nur fuer
> `df5a3dd` und werden **nicht** rueckwirkend geaendert oder auf den neuen Stand
> uebertragen. Die Aussagen zu Baseline, Geraetefirmware, Hashes und "Naechster
> Schritt" sind ueberholt. Auf dem aktuellen `main` sind `HW-19-R01..R03`
> `NOT_RUN`; der fortgeschriebene Testablauf steht in
> `docs/tasks/issue-192-factory-reset-hardware-verification-plan.md`
> (Revision 2). PR #194 wurde geschlossen und nicht gemergt.

Plan: `docs/tasks/issue-192-factory-reset-hardware-verification-plan.md`. Ownerentscheid
(PR #194): G1 freigegeben, G4 = `BLOCKED` belassen, G2/G3 nicht freigegeben.

```text
FIRMWARE=df5a3ddf41889b46f9b4d3c64085092a88e25a0a (main, PR #191), esp32_release, sauberer Build (--require-clean-source-tree), ESP-IDF v6.1 fff9895c82d744c7237be8847347bdd1b07c6643
APP_SHA256=06e08f0caa80b9cb949c49009732a492290f5a949b8ead150e00435f45428bd8
FLASH=nur App bei 0x10000, kein Erase, kein NVS-/State-Store-Erase, Bootloader/Partitionstabelle unveraendert (d7f180e4...)
VORHERIGES_IMAGE=3918c50 (Downgrade unter Schema 3 nicht abgesichert, Risiko vom Owner akzeptiert)
ACTUATOR_RELEASE=NO

P1_FLASH_UND_BOOT=PASS
  Boot: App version df5a3dd, source git sha df5a3dd..., application: ready
  Touchkalibrierung: active_status=Available fallback_status=NotFound (identisch zum 3918c50-Boot)
  Konfigurationszustand: WLAN-Heimnetz verbindet (HomeConnected); Owner: Startseite, Sprache, Programm, Geraetename unveraendert/normal (Owner-Beobachtung)
  Panic/Watchdog/Brownout: 0; 1 Reset (POWERON, Flash-Reset); Aufnahme 40 min ohne weiteren Reset
  Heap stabil: frei ca. 43,4 kB (3918c50: ca. 44,7 kB), Minimum 38112 B, groesster Block 40960 B

HW-19-R01=BLOCKED
  Zugang "PIN vergessen?": Owner: das Service-Menue ist am Geraet nicht aufrufbar
  (bekannt offen, Service/PIN #28). Die PIN-Seite und damit der Zugang wurden nicht
  erreicht. Warnung, Bestaetigung, Halteseite, Textbreiten DE/EN/ES, Loslassen/
  Verschieben/Abbruch vor 5000 ms: NOT_RUN (nachgelagert zum Zugang).
  Zugang SAFE_BOOT-Eintrag: BLOCKED (Owner G4: keine Korruption, keine NVS-Manipulation,
  kein neuer Einstieg).
  Aktoren AUS: physisch getrennt/deaktiviert, kein Aktortest (ACTUATOR_RELEASE=NO).
HW-19-R02=NOT_RUN   (verlangt vollstaendigen Reset, G2 nicht freigegeben; Powercut G3 nicht freigegeben)
HW-19-R03=BLOCKED   (kein sicherer SAFE_BOOT-/ResetEligibleNoRuntime-Eintritt, G4)
```

Die Quelle sieht den Pfad Diagnostics -> Service -> PIN vor; am Geraet ist er laut
Owner nicht nutzbar. Das ist eine Beobachtung, kein Produktfix; es wurde nichts geaendert.
Es gab in der Aufnahme keine `touch press dispatch`-Zeile (reine Navigation erzeugt
keine).

Es wurde kein Hold ausgeloest, keine Daten geloescht, nichts zurueckgeflasht. Das
Geraet laeuft weiter mit `df5a3dd`.

## Provenienz

```text
1afc04eccd05525f44ac73c26c86f03c6c603574d0a28759ad4424af49708327  uart192_r01.raw.txt (40 min, 2509 Zeilen)
0b33a4e8feb8fadb22d1f4be70b45193a52bd45385d11aff86ebca5543a23f81  flash_df5a3dd.log
```

Rohlogs lokal, nicht im Repository.

## Naechster Schritt (Ownerentscheid noetig)

Da beide Zugaenge nicht erreichbar sind, ist ein Werksreset am Geraet mit dem
Produktpfad nicht ausfuehrbar. Ein Hardware-Reset-Nachweis (R02) setzt entweder den
produktiven Service-/PIN-Zugang (#28) oder einen vom Owner benannten sicheren Einstieg
voraus. G2/G3 werden nicht angefragt, solange kein Einstieg existiert.
