# PR #170 – Hardwarebefund LOCAL_WEB_PROVISIONING_TOUCH_PATH_UNREACHABLE

```text
ISSUE=27
PR=170
FINDING=LOCAL_WEB_PROVISIONING_TOUCH_PATH_UNREACHABLE
TESTED_HEAD=dab2831cc2723a6cb4baab134971f196d870a07c
PROFILE=esp32_release
FIX_COMMIT=612feeadeaf2d62bf71cd955b7e79a2c13a7c581
FIX_SOFTWARE_TESTS=PASS_91_OF_91
FIX_HARDWARE_VERIFICATION=TOUCH_PATH_PASS_GATE_STOPPED
FINAL_HARDWARE_RESOURCE_GATE=BLOCKED_HTTPD_STACK_OVERFLOW
STOP_REASON=HTTPD_STACK_OVERFLOW_DURING_PROVISIONING_AFTER_STABLE_POWER_RETEST
EVIDENCE=docs/audits/PR170_HW_GATE_20261005/
ACTUATOR_RELEASE=NO
```

## Befund

Auf dem Devboard (Stand `dab2831`) war der genehmigte Pfad
`HeaderLanguage` → Slot 3 `web-access` → `HeaderWebAccess` →
`web-access-open` nicht erreichbar: `targetAt()` traf nur `HeaderNetwork` und
die vier BottomSlots, der Sprachcode im Header war nicht antippbar. Die
Provisionierung blieb korrekt gesperrt (Browser: „Noch nicht freigegeben“),
das Geraet blieb `Unprovisioned`. Beim Test wurde stattdessen auf der
WLAN-Seite `network-reconfigure` ausgeloest (erwartetes SetupAccessPoint-
Verhalten). Im Mitschnitt: kein Panic/Watchdog/Brownout, kein
`heap_alloc_failed`.

## Minimalfix

`HeaderLanguage`-Trefferzone x=176..219, y=0..31 (links vom unveraenderten
`HeaderNetwork`-Rect ab x=220, rechts vom Logo x=4..172). Keine Clock-Zone,
keine Navigations-, Auth- oder WLAN-Aenderung. Endgueltige UX (`Webzugang`
unter `Einstellungen`) bleibt #172.

## Status

Der Befund gilt erst nach realer Hardware-Fix-Verification auf dem
committeten PR-HEAD als geschlossen. Das finale Hardware-/Resource-Gate ist
nicht als PASS deklariert.

## Hardware-Fix-Verification (Stand `d3038f1`)

- Touchpfad **bestanden**: Sprache → `Webzugang` → `Web-Setup` oeffnete das
  10-Minuten-Fenster (UART: `touch press dispatch: outcome=2`, Display
  „Web-Setup frei (10 Min)“). Die Befundursache ist damit behoben.
- Provisionierung: erster Versuch mit zu kurzem Passwort lokal abgelehnt
  (Policy: 15–64 Zeichen, PIN genau 4 Ziffern; `invalid-credentials`).
- **Gate gestoppt:** Brownouts bei +258 s, +2783 s und +2794 s (Log), vom Owner
  dem provisorischen Stromaufbau zugeordnet (Kabelberuehrung). Danach liefert
  das Geraet `/api/v1/status` mit 503 `authentication-unavailable` und die
  Login-Shell; das ist weder `Unprovisioned` noch `PasswordProtected`/
  `PasswordDisabled`, sondern `RecoveryRequired` oder `Indeterminate`.
  Vermutet (nicht belegt): Stromverlust waehrend der Provisionierung.
  Es wurde nichts zurueckgesetzt oder repariert.
- Nicht ausgefuehrt: vier Sessions, fuenfte Session, Read-only-Polling,
  Ressourcenvergleich unter Last. Kein `heap_alloc_failed` (auch nicht
  `size=1696`), kein Panic/Watchdog im gesamten Mitschnitt.
- Der Hardware-Nachweis fuer das finale Gate steht aus; er braucht stabile
  Stromversorgung und einen vom Owner ausgeloesten Recovery-Schritt.

## Wiederholung nach Full-Reset (Stand `f8fcd6c`) – BLOCKER

Nach vom Owner freigegebenem Full-Erase, Kalibrier-Provisionierung
(`WRITE=COMMITTED`, `READBACK=PASS`), Neuflash von `f8fcd6c` ohne Erase und
neuem Heimnetz-Setup wurde das Fenster geoeffnet und das Formular ohne
Stromunterbruch abgeschickt. Ergebnis:

- UART +193,370 s: `***ERROR*** A stack overflow in task httpd has been
  detected.` → `abort` → `SW_CPU_RESET`. Der Owner erhielt keine Bestaetigung;
  das Display zeigte ca. 10 s spaeter den Neustart.
- Backtrace (addr2line): `vApplicationStackOverflowHook` ←
  `vTaskSwitchContext`, unterbrochen in
  `FermentationApplication::webAuthenticationStateUnlocked()`
  (`fermentation_application.cpp:1174`).
- Der HTTP-Server nutzt `HTTPD_DEFAULT_CONFIG()` ohne `stack_size`-Override
  (`esp_idf_http_server_lifecycle.cpp:190`); die Provisionierung (zwei
  PBKDF2-Laeufe, JSON, Antwort) laeuft im httpd-Task.
- Nach dem Neustart antwortet `/api/v1/status` mit 401 und die Login-Shell
  erscheint: die Provisionierung wurde persistiert, die Antwort ging verloren.
- Laut Testauftrag ein Blocker („Stack-Overflow“). Keine Reparatur und keine
  Optimierung in diesem Lauf. Fix und Neuverifikation sind eine eigene
  Entscheidung.
- Kein `heap_alloc_failed` (auch nicht 1696 B); Heap-Minimum 16668 B waehrend der
  WLAN-Moduswechsel vor der Provisionierung.
- Die frueheren drei Brownouts (provisorische Stromversorgung) sind damit nicht
  die Ursache des Gate-Blockers.
