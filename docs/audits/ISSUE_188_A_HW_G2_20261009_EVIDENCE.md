# Issue #188 A / PR #199 – Hardware-Evidence HW-188-A01 (G2), 2026-10-09

```text
SOURCE_HEAD=f859ef632def5c4f2167192904fda5e9f339d3af
PRE_READY_LOCAL_ON_SOURCE_HEAD=PASS (pre-ready/local=success, host+esp, 2026-10-09T16:58:45Z)
FIRMWARE=esp32_release, App version f859ef6, real actuators: disabled
APP_BIN_SHA256=445a460a83c64cce99cb402e9c6299641dca8f5b6a60a3526a75d84134cd23b2
APP_ELF_SHA256=93535f2bb41f10c663aa4116ec16daf61279dee25f736a7c0c2bc25cdcec260b
BOOTLOADER_SHA256=2c1b4979942b2e218fa6d132280ea01d424253040eeff21c3036e08bcb85001f
PARTITION_TABLE_SHA256=d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d
FLASH=PASS (esptool write-flash ohne Erase, Hash verified je Image)
ERASE_ALL=NO
NVS_ERASE=NO
DATE_UTC=2026-10-09
DEVICE=Owner-ESP32, FTDI FT232R A5069RR4, WLAN HomeConnected
WEB_AUTH_STATE=password-protected (vor und nach allen Phasen unveraendert)
ACTUATOR_RELEASE=NO
FACTORY_RESET_EXECUTED=NO (nie Bestaetigen/Halten; nur Abbrechen)
POWER_CUT=NO
SAFE_BOOT_TEST=NO
```

## Messmethode G2

Firmware unveraendert, keine Zusatzinstrumentierung. Alle Zeiten stammen aus dem
Zeitstempel `(ms)` der UART-Logzeilen (eine Uhr). Die Hauptschleife loggt
`heartbeat`, sobald seit dem letzten mindestens 1000 ms vergangen sind;
`touch press dispatch: outcome=N` wird direkt nach dem Dispatch und vor dem
Neuzeichnen geloggt. Eine synchrone PIN-Pruefung (PBKDF2) blockiert die
Schleifeniteration. Fuer einen Dispatch bei `t_d` mit vorherigem Herzschlag `h_p`
gilt fuer die Blockadedauer `B`: `(t_d - h_p) - 1000 < B <= (t_d - h_p)`.
Ein Dispatch mit `outcome=2` ist allein kein PIN-Signal (Abmelden, Abbrechen
und Reset-Beginn ergeben ebenfalls 2); die PIN-Dispatches ergeben sich aus der
festen Tippfolge der Phase (P2 bis P4). Task-Watchdog:
`CONFIG_ESP_TASK_WDT_TIMEOUT_S=5`, Idle-Tasks CPU0/CPU1 ueberwacht, kein Panic.
Es wurde kein Grenzwert festgelegt; die Bewertung traf der Owner.

Die Auswertung `since` ist `t_d - h_p`, `Luecke` die einschliessende
Herzschlagluecke, jeweils in ms. Rohlogs liegen lokal beim Tester
(`hw188_20261009T194614Z_READABLE.log`, Marker `P1 start` bis `P5 skipped`);
sie sind nicht Teil dieses Commits.

## Boot und Gesamtlauf

```text
I (763) app_init: App version:      f859ef6
I (1883) app_main: profile: esp32_release
I (1913) app_main: source git sha: f859ef632def5c4f2167192904fda5e9f339d3af
I (1923) app_main: hardware state: HARDWARE_UNVERIFIED
I (1933) app_main: actuator policy: REQUIRE_VERIFIED_HARDWARE
I (1933) app_main: real actuators: disabled
I (1943) app_main: application: ready
```

Gesamtlauf (ca. 1 497 s Geraetezeit): 1 433 Herzschlaege, 26 Dispatches.
Nach `application: ready` keine Signatur fuer Reset-Header, ROM-Banner, Panic,
`task_wdt`, Brownout, Stack-Overflow oder `heap_alloc_failed`. Groesste
Herzschlagluecke im Gesamtlauf 5190 ms (P4), kleinster freier Heap
26 960 B (`minimum_free_heap_bytes`, Gesamtlauf).

## Testmatrix

Die Spalte "Quelle" trennt Ownerbeobachtung am Display von der Log-Auswertung.

| # | Schritt | Ergebnis | Quelle |
|---|---|---|---|
| B1 | Boot: Version, Profil, Aktoren aus, ready, keine Fehlersignatur | PASS | UART-Log |
| T1-T8 | Bedienweg P1 (Einstieg, Maske, Entf/Leeren, Abbrechen, falsche/richtige PIN, erneuter Einstieg, Abmelden, Diagnoseweg) | PASS, Owner meldete keine Abweichung; Reihenfolge wich vom Runbook ab (richtige PIN zuerst), alle Schritte ausgefuehrt | Owner; Log: keine Fehlersignatur |
| G2a | PIN-Pruefung ohne Web-Anmeldung (P2, 3 Messungen) | gemessen, Werte unten | UART-Log |
| L1 | Sperre nach 3 Fehlversuchen (P3) | PASS: Owner sah "Zu viele Versuche"; Dispatch 4 in der Sperre ohne Verzoegerung (150 ms) | Owner + Log |
| L2 | "PIN vergessen?" -> Warnung -> Abbrechen trotz Sperre, kein Reset | PASS: Owner bestaetigt; kein Reset-/Panic-Signal, Web-Zustand unveraendert | Owner + Log |
| L3 | nach Ablauf der Sperre wirkt die richtige PIN | PASS: Owner bestaetigt Zugang; Dispatch 8 mit PBKDF2-Dauer | Owner + Log |
| G2b | PIN-Pruefung mit gleichzeitiger Web-Anmeldung (P4, 3 Messungen) | gemessen, Werte unten | UART-Log, Host-Log |
| I1 | Sperre nach 10 min Inaktivitaet | `NOT_RUN`, Owner akzeptiert ohne diesen Test (2026-10-09) | Ownerentscheid |

## Messwerte G2 (Rohwerte, ms)

### P1 Bedienweg (5 Dispatches, Zuordnung zu Tippschritten nicht belegt)

| Dispatch | since | Luecke |
|---|---|---|
| 1 | 3200 | 3390 |
| 2 | 660 | 1010 |
| 3 | 3110 | 3390 |
| 4 | 2870 | 2990 |
| 5 | 130 | 1010 |

Die Reihenfolge der Tipps wich vom Runbook ab; deshalb wird aus P1 keine
Blockadeschranke je PIN-Pruefung abgeleitet. Kein Dispatch blockierte laenger
als 3200 ms, kein Watchdog-/Resetsignal.

### P2 ohne Web-Anmeldung (3 PIN-Pruefungen, 3 Abmelden; Dispatch 1, 3, 5 = PIN)

| Dispatch | Bedeutung | since | Luecke |
|---|---|---|---|
| 1 | PIN | 3510 | 3760 |
| 2 | Abmelden | 360 | 1010 |
| 3 | PIN | 3410 | 3600 |
| 4 | Abmelden | 360 | 1060 |
| 5 | PIN | 3280 | 3480 |
| 6 | Abmelden | 820 | 1160 |

Aggregierte Schranke je PIN-Pruefung: 2510 < B <= 3280 ms (Annahme aehnlicher B).
Groesste Herzschlagluecke 3760 ms. Abmelden blockiert nicht.

### P3 Sperre und "PIN vergessen?" (9 Dispatches)

| Dispatch | Bedeutung | since | Luecke |
|---|---|---|---|
| 1 | falsche PIN | 2830 | 3110 |
| 2 | falsche PIN | 3320 | 3700 |
| 3 | falsche PIN, Sperre | 3530 | 3990 |
| 4 | Eingabe in der Sperre | 150 | 1090 |
| 5 | Pruefung nach Ablauf der Sperre | 3340 | 3780 |
| 6 | PIN vergessen? / Abbrechen | 520 | 1010 |
| 7 | PIN vergessen? / Abbrechen | 600 | 1430 |
| 8 | richtige PIN nach Ablauf | 3760 | 4020 |
| 9 | Abmelden | 880 | 1080 |

Groesste Herzschlagluecke 4020 ms. Eine gemeinsame Schranke wird wegen der
Streuung (2830 bis 3760 ms) nicht gebildet.

Zu Dispatch 5: Der Owner gab an, die Eingabe in der Sperre "sogar 2 mal" gemacht
zu haben (so verstanden: ein zweiter Versuch nach dem ersten in der Sperre).
Dispatch 3 (3. Fehlversuch) lag bei Geraetezeit 691 143 ms, Dispatch 4 bei
718 383 ms (innerhalb der Sperre, schnell), Dispatch 5 bei 729 743 ms mit einer
Pruefdauer von ca. 3,3 s, also einem Pruefbeginn rund 35 s nach dem 3.
Fehlversuch. Die erste Sperre dauert nach `AuthenticationDomain::verifyServicePin`
30 s (`30 s << (Stufe-1)`, Stufe 1). Dispatch 5 liegt damit nach Ablauf der Sperre
und ist eine regulaere Pruefung. Das ist aus Zeitpunkten und Code abgeleitet; das
Display wurde dabei nicht aufgezeichnet, die genaue Eingabe von Dispatch 5 ist
nicht belegt. Ein Zugang ohne korrekte PIN trat nicht auf (Owner: Zugang erst mit
der richtigen PIN, Dispatch 8).

### P4 mit gleichzeitiger Web-Anmeldung (Dispatch 1, 3, 5 = PIN; 3 Durchlaeufe)

| Dispatch | Bedeutung | since | Luecke |
|---|---|---|---|
| 1 | PIN | 3690 | 3940 |
| 2 | Abmelden | 860 | 1090 |
| 3 | PIN | 4930 | 5190 |
| 4 | Abmelden | 1410 | 1650 |
| 5 | PIN | 2920 | 3070 |
| 6 | Abmelden | 170 | 1000 |

Groesste Herzschlagluecke 5190 ms, das sind 190 ms ueber dem Watchdog-Timeout
von 5 s. Es erschien keine `task_wdt`-Meldung und kein Reset.

Web-Last (Host-Log, 90 s, Login und Logout im Wechsel, Marker `P4 los` bis
`P4 end`): 32 Logins, alle HTTP 200, alle Logouts HTTP 200; Login-Dauer minimal
2592, Mittel 2790, maximal 5291 ms. Web-Zustand danach `password-protected`.

Testwerkzeug-Befund (kein Firmware-Befund): Der erste P4-Lauf stoppte nach einer
Anmeldung, weil das Skript beim Logout den Body `{}` sendete und die Route
`/api/v1/logout` einen leeren Body verlangt (`body-not-allowed`, HTTP 400). Das
Skript wurde korrigiert und neu gestartet (Marker `P4 restart`); die Messung
beginnt mit `P4 los`. Die dabei geoeffnete Session lief von selbst ab.

## Ownerentscheid zur Blockade

Der Owner bewertete die gemessene Hauptschleifenblockade der synchronen
PIN-Pruefung (bis 3510 ms ohne und bis 4930 ms mit gleichzeitiger
Web-Anmeldung, groesste Herzschlagluecke 5190 ms) am 2026-10-09 im Chat als
**vertretbar**. Damit ist das Runbook-STOPP-Kriterium "unvertretbare Blockade"
ownerseitig verneint. Ein separater Worker-Task ist nicht beauftragt.

## Verbleibende Risiken / Befunde

- Die Herzschlagluecke 5190 ms uebersteigt den Task-Watchdog-Timeout von 5 s. Dass
  kein `task_wdt` erschien, ist beobachtet, aber nicht erklaert; die Annahme, dass
  die Idle-Tasks waehrenddessen Rechenzeit erhalten, ist nicht verifiziert.
- Die Schranken beruhen auf dem 1-s-Herzschlag und gelten je Messung nur auf 1 s
  genau; die Annahme gleicher Blockadedauer traegt wegen der Streuung nicht.
- I1 (Sperre nach 10 min Inaktivitaet) ist auf Hardware nicht ausgefuehrt; die
  Wirkung ist ueber die Simulationstests SIM-188-A* abgedeckt, nicht ueber
  Hardware. Der Owner nimmt das ausdruecklich in Kauf.
- Der Bedienweg T1-T8 beruht auf der Ownerbeobachtung ohne Display-Aufzeichnung.

## Nicht ausgefuehrt

Werksreset, Powercut, SAFE_BOOT, Aktoren (`ACTUATOR_RELEASE=NO`), I1.

Dieses Dokument erteilt keine Mergefreigabe.
