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
FINAL_HARDWARE_RESOURCE_GATE=MEASURED_PENDING_OWNER_ASSESSMENT_AND_INDEPENDENT_FIX_VERIFICATION
HTTPD_STACK_FIX=HARDWARE_VERIFIED_8192_ON_PROVISIONING_PATH
HTTPD_STACK_FIX_CODE_COMMIT=bcf370dea0ff8835a6f5243e68da048dc394ebba
HARDWARE_TESTED_HEAD=6d4c87b9314af9de6343fcd3c1e23e730c87d1a2
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

## Abschluss des Mitschnitts

Der Mitschnitt wurde auf Ownerwunsch beendet (`UART_STAMPED.txt`, bereinigt;
Hashes der unbereinigten Originale in `CAPTURE_HASHES_raw_unsanitized.txt`).
Einordnung durch den Owner: Die frueheren Brownouts stammen sicher vom
provisorischen Stromaufbau; der spaetere Stack-Overflow im httpd-Task trat bei
stabiler Versorgung auf und ist ein eigener Befund.

## Stackfix-Verifikation und Gate (Stand `6d4c87b`, `kHttpServerTaskStackBytes=8192`)

Ablauf (Owner-freigegebener Full-Erase, Kalibrier-Provisionierung
`WRITE=COMMITTED`/`READBACK=PASS`, Neuflash des exakten HEAD ohne weiteren
Erase, HOME_WIFI-Setup, stabile Stromversorgung):

```text
B3_PROVISIONING_SUCCESS_RESPONSE=PASS   (Owner-Screenshots: Login-Shell, Anmeldung, Statusdaten)
B3_STACK_OVERFLOW=NOT_OBSERVED
B3_AUTH_STATE=PASSWORD_PROTECTED
B3_PROVISION_REQUEST_DURATION=NOT_MEASURED_HTTP_NOT_LOGGED   (Login: 3.03..3.08 s je Request)
B4_SESSIONS_1_TO_4=PASS   (HTTP 200, status 200 mit jeder Session)
B4_FIFTH_SESSION=PASS_FAIL_CLOSED   (HTTP 503 session-unavailable)
B4_LOGOUT_THEN_ONE_NEW_LOGIN=PASS   (nach Logout genau ein neuer Login, weiterer 503)
B4_READ_ONLY_POLLING_600S=PASS_WITH_OBSERVATION   (3 aktive Sessions + 1 ruhende)
B4_POLL_REQUESTS=2019  OK=2009  CLIENT_TIMEOUT_10S=10  LATENCY_P50_MS=221  LATENCY_MAX_MS=10648
B4_DISPLAY_TOUCH_DURING_LOAD=OWNER_BROWSED_ALL_PAGES_NO_REPORTED_WHITESCREEN_OR_HANG
UNEXPECTED_RESET_STACK_OVERFLOW_PANIC_WATCHDOG_BROWNOUT=NONE_IN_RUN
HEAP_ALLOC_FAILED=0
HEAP_ALLOC_FAILED_SIZE_1696=0
HEARTBEATS=9072  LARGEST_GAP_MS=3605   (Login/PBKDF2)
MIN_FREE_HEAP_BYTES_OVER_RUN=8148
MAIN_STACK_HWM_BYTES_SEEN=6056   (Boot-Werte 19560, 9576)
FREE_HEAP_STABLE_HOME_WIFI_AFTER_PROVISION=27428
LARGEST_FREE_BLOCK_8BIT_MIN_SEEN=12800
```

Einordnung gegen PR #174 (`docs/audits/R1_RAM_LVGL48_HW_EVIDENCE.md`,
ohne neue harte Grenze): #174 maass beim Moduswechsel ein
`minimum_free_heap` von 20612 B und einen niedrigsten Main-Stack-HWM von
6256 B. Mit Webzugang, vier Sessions und Polling liegt das Minimum bei 8148 B
(rund 12 KB niedriger) und der Main-Stack-HWM bei 6056 B (200 B niedriger).
Das ist ein gemessener Unterschied durch Websession-/HTTP-Betrieb, kein
Fehler; die Bewertung gegen die R1-Integrationsqualifikation liegt beim Owner.

Beobachtungen/Grenzen der Messung:

- 10 von 2019 Pollinganfragen liefen in den 10-s-Client-Timeout (zwei
  `httpd recv error 104` im Log); die Last (ca. 10 Anfragen/s) liegt weit ueber
  dem Browser-Polling (alle 5 s). Kein Geraetefehler im Log.
- Eine vierte Session blieb durch einen Skriptfehler (Logout mit Body, HTTP 400
  `body-not-allowed`, vom Geraet korrekt abgewiesen) ohne Cookie zurueck und
  belegte einen Platz; Polling lief daher mit 3 aktiven + 1 ruhenden Session.
- Stack-HWM des httpd-Tasks und Dauer des Provisionierungsrequests wurden nicht
  gemessen (nicht im Produktlog); die 8192 B sind nur durch das Ausbleiben des
  Overflows belegt.
- Kein erneuter 87,7-min-Lauf noetig; der Mitschnitt umfasst 9088 s ohne
  1696-B-Ereignis.

Der Gate wird hier nicht als PASS deklariert; Independent Fix Verification und
Ownerbewertung stehen aus.
