# G1′ – Evidence und gezielte Rest-Coverage auf a9

Vorbereitung 2026-10-18; keine neue Hardware-Evidence. Produkt-PR-Reviewbasis
`2e37b0e3deee3f5770e3f0df3e174074c78711d6`, Laufzeitquelle `a9a1156`.
Seit d35ee85 ist unter main/lib nur die Web-Shell geändert; F4, Renderer,
Touch, Partitionen und Defaults sind identisch. Overlay-Firmwaredateien werden
bytegleich aus bb4f43a übernommen, Hosttools bytegleich aus 893f3bb.
Die Kombination mit der neuen Web-Shell benötigt ein neues konkretes Image.

| Gegenstand | Bereits nachgewiesen / Status | Gezielt noch erforderlich | Grenze des Boards/Verfahrens |
|---|---|---|---|
| Vektor a = 6656 B; Skip 13312 / Resume 16640 B; Marge 3328 B | Native Implementierung/Regressionen übernommen; unveränderter Code | Tatsächliche Gate-Blockwerte und Seitenkontext im neuen Image | Keine gesamte UI-/OOM-Schranke |
| Historische Seiten 0,1,12,13,16,18,19,26; de/en | Hardware-Coverage im G1P-2 historisch OBSERVED, Lauf insgesamt FAIL/HOLD | Nur aktuelle Anker Home/Status und fehlende Kombinationen; keine komplette de/en-Wiederholung | Historische Beobachtung wird nicht zu aktuellem Image-PASS |
| 21 Sprache, 22 Netzwerk, 23 Uhr, 24 Webzugang, 17 Diagnose | NOT_PROVEN auf Hardware | Öffnen/Zurück ohne Netzwerk-/Webzugang-Aktion; reale Kontakte/Loslassen | Fehlende Navigation oder Kalibrierung: NOT_PROVEN, keine Reparatur |
| 2 Programmübersicht, 6 Programmaktionen, 3 Editor, 25 Zahleneditor, 27 Texteditor | Hardware NOT_PROVEN | Vorhandenes Programm anzeigen, Editor/Kandidaten nur ansehen, abbrechen; Keyboard-Modi ohne Speichern | Keine neuen Programme, Katalog-/Namensänderung oder maximal lange künstliche Texte |
| 7 manuelle Auswahl, 8 Halten, 9 Zeitbetrieb | Hardware NOT_PROVEN | Entwürfe nur anzeigen und abbrechen | Kein Start, keine Sensor-/Commissioning-Simulation |
| es (Locale 3) + de/en (1/2) | es NOT_PROVEN; de/en historisch OBSERVED | Schwerpunkt es auf Home/Status/Menü/Uhr/Programm-/Editorseite; de/en nur Anker + fehlende Textvarianten | Sprachwahl ist normaler persistenter User-Konfigurationscommit; im Gate ausdrücklich benannt, keine direkte NVS-Manipulation |
| 28 Werksreset Warning/Confirm/Hold; reale Touch-/Release-Fälle | Hardware NOT_PROVEN; native Hold-A/B-Beweise nur Software | PIN-Seite → PIN vergessen → Warnung/Bestätigung; kurze Berührung max. 1 s, vollständiges Loslassen, Abbrechen | Nie 5 s halten; bei fehlender Verfügbarkeit nicht erzwingen |
| Skip → Resume, Hysterese, gehalten/Loslassen/neuer Kontakt nach Skip (Hold A/B) | Native Regressionen gültig, Hardware NOT_PROVEN | Nur bei natürlichen, sicheren Gate-Ereignissen korrelieren | Kein gezielter Speicherdruck, keine Gate-Umgehung. Ohne Ereignisse NOT_PROVEN; keine vollständige Hold-A/B-Hardwarequalifikation |
| 14 Meldungsdetail; 20 Recovery und besondere Meldungs-/Servicezustände | NOT_PROVEN | Nur schon vorhandene, gefahrlos erreichbare Ansichten | Keine Meldungen/Faults/Recovery erzeugen, kein Service-PIN-Versuch |
| 10 Prozess, 11 Stopdialog, 15 Abschluss; aktive/wartende/fault/safeboot-Varianten | NOT_PROVEN / am vorgesehenen Standby-Board ohne Zustandsänderung nicht erreichbar | Kein Versuch in diesem Gate | D9 separat: gültige Sensor-/Commissioning-Werte und Programmstart werden nicht vorausgesetzt |
| 4/5 Löschdialoge, Speichern/Übernehmen, destruktive Endzustände | NOT_PROVEN / aus diesem sicheren Gate ausgeschlossen | Keine destruktiven Pfade | Keine künstliche Behauptung vollständiger Seiten-/Text-/Zustands-Coverage |
| b und transiente Spitzen | NOT_PROVEN; 16768 B aus G1P-2 = gemeinsame Heap-Schwankung | Phase 0/1/2 mit pre/post/low, block_pre/post/low, Text-/Befehlsgrößen + manuellen Zeitmarken | WLAN/HTTP parallel; Samples verpassen Transienten. Kein kausal isoliertes b, keine 33536-B-Schwelle |
| Stack | G1P-2 historisch main 10984 B/24576 B → strikter FAIL/HOLD | Main-/HTTPD-HWM separat; G1′ strict 11160/2080 B | Owner-Trend-Ausnahme gilt nur G2-Web. Neuer G1′ kann erneut an main-stack-reserve stoppen |
| Recovery | G1′ noch NOT_PROVEN | Gleiche eine lebende Session, Home, Ausgangs-Locale, Browser geschlossen: Referenz 60 s und Nachlauf 120 s | Nur UI-Kontextvergleich, kein Programm-R_ref und keine R1-U-/90%-Abnahme |

## Messvorbehalt und Entscheidung

Die Null-Allokations-Probe nutzt feste Records; Logging erst nach Ende der
äußeren Mess-Scope samt Modellen. Phase 0 umfasst Vorbereitungsallokationen,
Phase 1 Touch-Zielfindung, Phase 2 Render. Heap-/Block-Abfragen sind nicht atomar;
`min_free` ist Boot-Historie, keine aktuelle Recovery- oder Phasenmessung.
Aggregate pro Sekunde bewahren den beobachteten größten Delta-Record sowie
Minima und Maximaltextwerte; sie sind kein vollständiger Einzelkontakttrace.
Bei Overflow/fehlender Korrelation keine Abdeckung behaupten. Ein Skip ist
kein Bedarf von 0 B. Ungemessene Textlängen/Localekombinationen bleiben offen.

Das bestehende Overlay kann die fehlenden sicheren Pfade und gemeinsame
Checkpoint-Reserven ergänzen, aber mit gewöhnlichem parallelem Browserpolling
weder b kausal isolieren noch alle transienten Spitzen begrenzen.
`2 × max(6656,b)` bleibt die Planformel; b wird hier nicht durch einen
unklaren gemeinsamen Heap-Delta ersetzt. Der geerbte Analyzer gibt eine
explizit NOT_QUALIFIED markierte arithmetische Zahl aus; sie wird nicht als
Schwellenvorschlag verwendet. Es wird keine neue F4-Zahl abgeleitet.

**Gate-Vorbereitung möglich; F4-Schwellenqualifikation bleibt HOLD.** Das ist
ein gezielt begrenzter Evidence-Zuwachs, keine Zusage, RES-01 damit vollständig
zu schließen. Skip-/Hold-/Peak- und nicht erreichbare Varianten bleiben
NOT_PROVEN. G1-IMAGE01 und G1P-2 bleiben historisch FAIL/HOLD.
