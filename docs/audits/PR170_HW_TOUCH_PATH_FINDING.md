# PR #170 – Hardwarebefund LOCAL_WEB_PROVISIONING_TOUCH_PATH_UNREACHABLE

```text
ISSUE=27
PR=170
FINDING=LOCAL_WEB_PROVISIONING_TOUCH_PATH_UNREACHABLE
TESTED_HEAD=dab2831cc2723a6cb4baab134971f196d870a07c
PROFILE=esp32_release
FIX_COMMIT=612feeadeaf2d62bf71cd955b7e79a2c13a7c581
FIX_SOFTWARE_TESTS=PASS_91_OF_91
FIX_HARDWARE_VERIFICATION=PENDING
FINAL_HARDWARE_RESOURCE_GATE=NOT_COMPLETE
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
