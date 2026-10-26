# Offene Hardware-, Inbetriebnahme- und Budgetpunkte

## Zweck

Diese Datei ist die nachweisgebundene Checkliste fuer Punkte, die reale
Hardware, thermische Messungen oder reproduzierbare Build- und
Ressourcenergebnisse benoetigen.

Ein Punkt wird nur abgehakt, wenn der konkrete Nachweis im zugehoerigen Issue,
PR, Messprotokoll, Hardwarebericht oder Buildartefakt verlinkt ist. Annahmen,
Lieferantenangaben zu aehnlicher Hardware und erfolgreiche Simulationen ersetzen
keinen realen Nachweis.

Kennzeichnungen:

- `TBD_HARDWARE`: durch reale Komponente, Datenblatt und praktische Pruefung zu
  klaeren;
- `TBD_COMMISSIONING`: durch dokumentierte thermische oder regelungstechnische
  Messung zu klaeren;
- `TBD_IMPLEMENTATION_BUDGET`: durch reproduzierbare Builds und Belastungstests
  zu klaeren.

Die vollstaendige Abgrenzung zu Zukunftsfunktionen steht ausschliesslich in
[`SPECIFICATION_REVIEW.md`](SPECIFICATION_REVIEW.md).

## `TBD_HARDWARE`

### Controllerboard und Ressourcenbasis

- [ ] tatsaechliche Platinenrevision und Modulbeschriftung dokumentieren
- [ ] erkannte Flashgroesse bestaetigen
- [ ] PSRAM erkennen; Firmware darf sie nicht voraussetzen
- [ ] reale Partitionstabelle und Appgroesse messen
- [ ] freien Heap, niedrigsten Heap und groessten freien Block erfassen
- [ ] UART-/FT232RL-Flash- und Recoveryweg bestaetigen

Nachverfolgung: `UNASSIGNED`; #29 ist geschlossene historische Plattform-
und Bring-up-Evidenz. Der Owner ordnet verbleibende reale Board-/Ressourcen-
Nachweise einem späteren passenden Hardware- oder Release-Scope zu.

### GPIOs und Bootpegel

- [x] GPIO-Designzuordnung und Bootstrapping-Vorbewertung im Boardprofil-SSOT
      aus #130 festgelegt (`PLANNED_NOT_CONFIRMED`)
- [ ] Bootstrapping-Eignung der real verwendeten Anschlüsse gegen das konkrete
      ESP32-/Board bestätigen
- [ ] funktional nachweisen, dass relevante Aktoren beim Boot und Reset nicht
      unkontrolliert betrieben werden
- [ ] #32: PCB-feste Kanal-/Verbraucherzuordnung funktional bestaetigen

Nachverfolgung: Design-SSOT #130 (geschlossen); funktionale Hardware-
Nachweise liegen bei den jeweiligen offenen owning Scopes #30, #31, #32 und
#33. Kein Designstatus ist dadurch ein funktionaler PASS.

### Lokale RTC für das konkrete Fermenter-R1-Profil

- [ ] tatsächliche DS3231-Variante am gelieferten Modul physisch bestätigen;
      `DS3231SN` und `DS3231M` werden vorher nicht angenommen
- [ ] I2C-Erreichbarkeit, OSF-/EOSC-Verhalten, Batterieretention und trusted
      UTC für die bestätigte Variante funktional nachweisen
- [ ] Offline-Start eines neuen produktiven Laufs mit der nachgewiesenen
      lokalen trusted RTC funktional nachweisen

Nachverfolgung: `UNASSIGNED`; #126 ist geschlossene historische/digitale
Adapter-Evidenz. Der Owner entscheidet später über den tatsächlichen
Hardware-Tracking-Scope. Diese offenen Nachweise sind kein Anlass für ein
neues RTC-Issue oder einen RTC-PR in #136.

### Temperatursensoren

- [ ] Bustopologie festlegen: drei getrennte Busse oder feste Sensoren gemeinsam
- [ ] Pull-ups und Leitungslaengen aus Verdrahtungs-/Board-SSOT bestimmen;
      Busfunktion, ROM, CRC, Hot-Plug und Fehlerreaktion pruefen, ohne ein
      generelles Spannungs- oder Pegelmess-Gate einzufuehren
- [ ] ROM-Adressen der festen Sensoren dokumentieren
- [ ] Produktfuehleranschluss waehlen; M8-3, GX12-3 oder andere verriegelbare
      3-polige Loesung vergleichen
- [ ] Verpolungs-, Fehlsteck- und ESD-Schutz des externen Anschlusses pruefen
- [ ] Lebensmitteltauglichkeit und Reinigbarkeit des Produktfuehlers bestaetigen
- [ ] Hot-Plug praktisch testen

Nachverfolgung: #30

### Display und Touch

- [ ] Displaycontroller ILI9341 praktisch bestaetigen
- [ ] Touchcontroller praktisch identifizieren; XPT2046 nicht ungeprueft annehmen
- [ ] SPI-Pins, Rotation, Reset und Hintergrundbeleuchtung festlegen
- [ ] Touchrohwerte, Kalibrierung und 10-Sekunden-Boot-Recovery testen
- [ ] Speicher- und Geschwindigkeitsbedarf der realen Darstellung messen

Nachverfolgung: #31

### Luefter, Summer und MOSFET-Ausgaenge

- [ ] Innen- und Aussenluefterdaten erfassen; Stromaufnahme und Anlaufverhalten
      nur soweit ein konkretes Hardwaregate dies erfordert und geeignete
      Mittel vorhanden sind
- [ ] Kanal-/Verbraucherfunktion kontrolliert einzeln pruefen
- [ ] Boot-/Resetverhalten mit sicherem Einzelverbraucher funktional beobachten
- [ ] Summerfunktion, Kanal und Lautstaerke festlegen
- [ ] Nachlaufverhalten auf realer Hardware pruefen

Nachverfolgung: #32

### BTS7960 und Leistungspfad

- [ ] genaue Modulvariante und Pinbeschriftung dokumentieren
- [ ] Enable- und Richtungspins festlegen
- [ ] Pull-down-/fail-low Beschaltung als vorhandenen Aufbau dokumentieren
- [ ] Adapterinterlock nachweisen
- [ ] Richtung spaeter ueber begrenzte Servicepulse funktional bestimmen
- [x] R_IS/L_IS fuer R1 bewusst unbeschaltet/deaktiviert; GPIO34/GPIO35
      bleiben als ADC1-Reserve fuer eine moegliche spaetere Integration
      reserviert (`FUTURE_RELEASE`), nicht verworfen. Keine R1-Mess-,
      Implementierungs- oder DoD-Pflicht; `R1_BLOCKED_BY_R_IS_L_IS=NO`
- [ ] Sicherungstyp, Halter, Stecker und Leitungsquerschnitte dokumentieren
- [ ] erste begrenzte Heiz- und Kuehlpulse sicher durchfuehren

Nachverfolgung: #33

## `TBD_COMMISSIONING`

### Sensoren und thermischer Aufbau

- [ ] Referenzmessgeraete dokumentieren
- [ ] individuelle Offsets aller drei DS18B20 bestimmen
- [ ] Sensorpositionen und Temperaturverteilung messen
- [ ] leeren Schrank vermessen
- [ ] kleine und grosse Referenzmasse vermessen
- [ ] Produkttraegheit und Luftreaktion dokumentieren

Nachverfolgung: #34

### Regelparameter

- [ ] PI-Parameter Luft/Produkt und Heizen/Kuehlen bestimmen
- [ ] Schaltfenster und Verhalten kleiner Impulse festlegen
- [ ] Mindest-Ein- und Mindest-Auszeit festlegen
- [ ] Richtungswechselhysterese und Totzeit validieren
- [ ] Innen- und Aussenluefternachlauf festlegen
- [ ] Filterstaerken, Zielband, Qualifikationsdauer und Gnadenzeit festlegen
- [ ] Stabilitaetszeit fuer automatische Produktfuehler-Rueckkehr bestimmen

Nachverfolgung: #35

### Sicherheitsgrenzen

- [ ] fruehe Luftbegrenzungen bestimmen
- [ ] Prozesswarnschwellen bestimmen
- [ ] Sicherheits-Eingriffsgrenzen bestimmen
- [ ] harte obere und untere Notgrenzen bestimmen
- [ ] begrenzte Gegenrichtungsversuche validieren
- [ ] Kuehlkoerpergrenzen und Temperaturtrends festlegen
- [ ] einmalige Temperatursicherung auswaehlen, Rating und Montageort dokumentieren

Nachverfolgung: #35

### Standardprogramme

- [ ] Temperaturen und Zeiten der vier Standards praktisch festlegen
- [ ] Vorheiz- und Zielerreichungsgrenzen pruefen
- [ ] Abschluss- und Kuehlhalteverhalten validieren
- [ ] mindestens einen praktischen Lauf je Standardprogramm dokumentieren

Nachverfolgung: #36

## `TBD_IMPLEMENTATION_BUDGET`

### Flash und Partition

- [ ] Single-App-Partitionsplan fuer Release 1 festlegen
- [ ] Appgroesse sowie tatsaechlich vorhandene R1-Konfigurations- und
      Laufpersistenz dokumentieren; kein Journal-/Historienbudget als R1-Gate
- [ ] notwendigen freien Flash-/Partitionssicherheitsabstand definieren
- [ ] vorhandene Webressourcen und drei Sprachen messen
- [ ] NVS-Partitionsgroesse fuer vorhandene Konfiguration, Slots und Secrets
      bestimmen (Backend gemaess ADR-016)

`R2_VORGEMERKT / historische Referenz, kein R1-Abnahme- oder Ressourcen-Gate:`
Journal, Lauf-/Messhistorie, Bereinigung/Archiv, Exporte samt Groessen und
parallel temporaerem Speicher, portables Backup/Import sowie das historische
5-Detail-/50-Zusammenfassungsmodell. Die Vormerkung ist keine R2-Lieferzusage.

Nachverfolgung der aktiven R1-Werte: #9, #10 und #37; #29/#90 bleiben
geschlossene historische Nachweise, keine offenen Owner.

### RAM und Laufzeit

- [ ] Warn- und kritische Schwellen fuer freien Heap festlegen
- [ ] Mindestwert fuer den groessten freien Block festlegen
- [ ] Groessen vorhandener R1-Ringpuffer und Warteschlangen dokumentieren
- [ ] `CommandDecision` mit maximalem 48/96/1024-Kandidaten auf dem realen
      ESP32 ohne PSRAM messen: Task-Stack-High-Water-Mark fuer die produktiv
      verwendeten Kommandowege
- [ ] freien Heap und groessten zusammenhaengenden Block vor, waehrend und
      nach `CommandDecision`-Erzeugung und -Anwendung messen
- [ ] Verhalten bei fehlgeschlagener Allokation nachweisen: keine Teilmutation,
      keine unvollstaendige Persistenzfreigabe und keine neue Aktorfreigabe
- [ ] vorhandene Web-, Touch-/Display- und Regelpfade unter realer
      Parallelbelastung messen
- [ ] aktuelle Fehlerdiagnose und erforderliche Laufzeitzaehler innerhalb des
      gemessenen R1-Budgets bewerten; kein Journalarchiv voraussetzen

Nachverfolgung: #10, #28 und #37; #29 dokumentiert hierzu historische reale
ESP32-Evidenz, ist aber kein offener Owner. #37 bestaetigt die reale Last und
Reserven gemaess seinem bestehenden Abnahmescope.

### Lokaler Werksreset (Issue #19, R1-Pflicht)

- [x] Dauer des langen Gedrueckthaltens: Ownerentscheid 5000 ms, durchgehend
      auf dem Halteziel, fest (`kApprovedFactoryResetHoldMillis` in
      `main/app_main.cpp`; keine separate Laufzeitkonfiguration)
- [x] Ablauf A (PIN-geschuetzt, **R1-Software umgesetzt**): in PR #202
      gemergt (`b903de8`); die zugehoerigen realen Hardwarenachweise bleiben
      gemaess #192 separat offen und werden durch diesen Softwarestand nicht
      als bestanden bewertet
- [x] Werksreset im `ResetEligibleNoRuntime` (Ownerentscheid S1 = B, Plan
      Revision 8, Ownerfreigabe `aa665f1`): software-seitig umgesetzt
      (`SIM-19-S1-01..06`); Hardwarenachweis in #192 (`HW-19-R03` `BLOCKED`, siehe unten)
- [ ] Hardwarenachweise `HW-19-R01..R03` (`ACCEPTANCE_TESTS.md`) in einem
      separaten Hardware-Folgeissue #192 (Ownerentscheid O-HW = A); Stand 2026-10-11: `HW-19-R01` `PASS` mit akzeptierter Abweichung (zuvor am 2026-10-10 `FAIL`: Stack Overflow beim vollen Hold, Bootloop; Offline-Diagnose in der #192-Evidence 3b, Fix im PR #200 nach freigegebener Plan Revision 3 (`43a65d3`) umgesetzt, statischer Stacknachweis in Evidence 3c, Review akzeptiert; G5 erteilt und ausgefuehrt: App-only-Flash `b9564e6` ohne Erase, Geraet bootet stabil, Display normal und Einstellungen zurueckgestellt (Ownerbeobachtung, `PASS_OWNER_OBSERVED`); G5/P3b `PASS`, interner Handoff-Pfad `NOT_RESOLVED`, Evidence 3d, kein Reset-Nachweis, FOLLOW-UP: UI-Command-Stackpfad (`UNVERIFIED_STATIC`) und Aufnahme von `scripts/analyze_issue_192_handoff_stack.py` in die Gates; G2-R3/P3c am 2026-10-11 ausgefuehrt, Evidence 3e: Reset funktional durchgefuehrt, `stack_hwm_bytes` nach dem Reset 5 432 B < Ziel 6 144 B, Heap-Minimum 4 084 B, vom Owner am 2026-10-11 als begruendete Abweichung akzeptiert, Heap-Minimum als Messbefund ohne Softwarefix; deshalb `HW-19-R01` `PASS` mit akzeptierter Abweichung, kein zusaetzlicher Hardwaretest/keine Implementierung allein wegen dieser Werte), `HW-19-R02` `NOT_RUN` (Teilaspekte beobachtet), `HW-19-R03` `BLOCKED`
### R1-Persistenz und Aufbewahrung

- [ ] Kritische Config-/Run-Persistenz, atomare Transaktionen, Write-before-Apply,
      Fehlerfaelle und Wiederanlauf gemaess den bestehenden R1-Vertraegen und
      `ACCEPTANCE_TESTS.md` nachweisen
- [ ] aktuelle Fehlerdiagnose und tatsaechlich vorhandene R1-Daten auf dem
      realen Ziel pruefen; kein persistentes Journal als Start-, Safety- oder
      Recovery-Voraussetzung behaupten

`O-R3/K1=B` ist am 2026-10-26 ausdruecklich entschieden: ein persistentes
Ereignisjournal ist kein R1-Start-, Safety- oder Recovery-Gate. Kritische
Persistenz-, Interlock-, Safety- und Recovery-Gates bleiben unveraendert.

`R2_VORGEMERKT / historische Referenz, kein R1-Abnahme- oder Ressourcen-Gate:`
Journal, Lauf-/Messhistorie, Bereinigung/Archiv, Laufexporte, Backup/Import und
5-Detail-/50-Zusammenfassungs-Aufbewahrung. Die historische 5/50-Vorgabe ist
bei Wiederaufnahme neu zu bewerten; daraus folgt keine R2-Lieferzusage.

Nachverfolgung der bestehenden R1-Persistenz: #19 und #37; die R2-Vormerkung
hebt keine bestehenden Hardware- oder Releaseabnahme auf.

## Hardware- und Releaseabnahme

- [ ] vollstaendige Hardwareabnahme gemaess `ACCEPTANCE_TESTS.md`
- [ ] verpflichtende Fehlerinjektionen am realen Aufbau
- [ ] Stromunterbrechungen in allen wesentlichen Phasen
- [ ] lokale und Webbedienung auf realem ESP32
- [ ] siebentaegiger Dauer- und Belastungstest
- [ ] alle Release-Gates bewerten

Nachverfolgung: #36 und #37
