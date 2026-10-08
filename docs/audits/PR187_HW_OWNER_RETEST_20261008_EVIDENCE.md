# PR #187 / Issue #172 – Owner-Nachtest S7–S10 (2026-10-08)

Nachtrag zu `PR187_HW_SMOKE_3918C50_20261007_EVIDENCE.md`. Die dortige
`NOT_RUN`-Aufzeichnung vom 07./08.10.2026 bleibt unveraendert; dieser Nachtrag
ersetzt sie nicht rueckwirkend, sondern ergaenzt sie um die Owner-Meldung vom
08.10.2026.

```text
FIRMWARE_UNDER_TEST=3918c508e6c5d10fc09c80b32003585534990c23 (exaktes Produkt-Image, App version 3918c50)
ACTUATOR_RELEASE=NO
EVIDENCE_TYPE=OWNER_OBSERVED (keine UART-, Ressourcen- oder sonstige instrumentierte Aufnahme)
```

## Owner-Meldung

Der Owner meldet am 2026-10-08 fuer die folgenden fuenf Punkte jeweils „OK“:

```text
S7_CONTENT_PAGES_LAYOUT=PASS_OWNER_OBSERVED
S8_INPUT=PASS_OWNER_OBSERVED
S9_FAIL_CLOSED=PASS_OWNER_OBSERVED
S10_DEVICE_NAME=PASS_OWNER_OBSERVED
UI_LABELS_AND_KEY_SIZES=PASS_OWNER_OBSERVED   (Beschriftungen/Tastengroessen, anschliessende UI-Pruefung)
```

Zuordnung zu den bisherigen `NOT_RUN`-Eintraegen: `S7_CONTENT_PAGES_LAYOUT`,
`S8_S9_INPUT_AND_FAIL_CLOSED` (hier in S8 und S9 getrennt gemeldet),
`S10_DEVICE_NAME_CHANGE_AND_PERSISTENCE` und `S10_LABEL_WIDTHS_EINSTELL`
(zusammen mit der Tastengroessenpruefung unter `UI_LABELS_AND_KEY_SIZES`).

## Pruefgrenzen

- Die Meldung ist eine Owner-Beobachtung. Es existiert keine UART-Aufnahme, kein
  `commit_probe_*`-Log und keine Messung der Treffbarkeit in Pixeln.
- Aus „OK“ wird nicht abgeleitet, dass Geraetename-Aenderung und **Persistenz
  nach Reset** einzeln geprueft wurden; fuer S10 gilt nur „Owner meldet OK“.
  Eine Einzelbestaetigung der Persistenz ist offen, bis der Owner sie
  ausdruecklich meldet.
- Aus der Meldung folgen weder Ressourcen- noch Stabilitaetsnachweise. Der
  D10-Ressourcennachweis (Sprache S3, Programm S6, Geraetename S10) ist davon
  unberuehrt und wird separat erhoben.
- Bestehende PASS-Nachweise (S1, S3, S4, WLAN-Seite, S6, S10-Menue/Tastatur,
  RAM/LVGL-Fix, Boot- und 2-h-UART-Stabilitaet) wurden nicht wiederholt.
- Service-Eintrag und Webzugang-Seite: unveraendert wie im Smoke vom 07.10.2026,
  nicht Teil dieses Nachtrags.
