# Implementierungsplan fuer Issue #17

## Planstatus

- Issue: `#17 – [E2.2] Laufpersistenz und Kontrollpunkte implementieren`
- Ausgangsbranch: `main`
- Ausgangs-Commit: `6909b90f518190131eb41c1c707a5b8738d5ba3f`
- Planbranch: `plan/issue-17-run-persistence-checkpoints`
- Planstatus: `PLAN_DRAFT`
- Live-Issue-Status bei Planerstellung: `PLANNED_SPEC_PENDING`
- Harte Abhaengigkeiten: #13, #14, #15 und #54; alle live abgeschlossen.

```text
IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

Dieser Plan autorisiert keine Implementierung. Sie darf erst nach einem
eindeutigen Ownerkommentar fuer genau den Commit mit dieser Planversion
beginnen:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

Jede materielle Abweichung, insbesondere eine Erweiterung um
Konfigurationspersistenz, Boot-/Aktorentscheidung, Journal oder produktiven
Flashadapter, macht die Freigabe ungueltig und verlangt einen neuen Plancommit.

## Ziel

Issue #17 liefert ein begrenztes, typisiertes und nativ testbares Fundament
fuer aktive Laufkontrollpunkte. Es soll:

1. den unveraenderlichen Laufschnappschuss aus #13, seine Laufrevisionen sowie
   den wiederherstellbaren Prozessruntimezustand aus #14 in einem eigenen
   fachlichen Kontrollpunkt modellieren und validieren;
2. einen kompakten, deterministischen, versionierten und integritaetsgeprueften
   Schema-1-Payloadcodec bereitstellen;
3. ueber die vorhandenen anwendungsneutralen #54-Bausteine mindestens zwei
   rotierende Kontrollpunktrevisionen schreiben, lesen und den juengsten
   vollstaendig nutzbaren Kontrollpunkt auswaehlen;
4. ereignisbezogene Speicherpflichten und das Intervall von einer bis 60
   Minuten (Werkseinstellung fuenf Minuten) als reine, testbare
   Anwendungsentscheidung ausdruecklich machen;
5. einen kontrollierten Schreibablauf bereitstellen, der `WriteError`,
   Kapazitaetsfehler und `CommitOutcomeUnknown` sauber unterscheidet und
   niemals einen teilweise unbestaetigten Kontrollpunkt als erfolgreich meldet;
6. die Grundlage fuer eine spaetere Recoverybewertung liefern, ohne die
   Recoveryentscheidung, Aktorfreigabe oder Zeit-/Temperaturfortschrittslogik
   aus #18 selbst zu implementieren.

Das Ergebnis bleibt hardware- und backendunabhaengig. Es verwendet nur
`device_platform::IStateStore`; ein NVS-/Preferences-/ESP-IDF-Adapter,
Composition-Root-Verkabelung und reale Flashmessungen sind nicht Teil dieses
Issues.

## Nicht-Ziele und ausdruecklich verbotene Vorwegnahmen

Nicht Bestandteil von #17 sind:

- `RECOVERY_EVALUATION`, phasenbezogene Wiederanlaufentscheidung,
  `SAFE_BOOT`, Fehlerreset, Sensor- oder Aktorfreigabe aus #18 beziehungsweise
  #24;
- Berechnung oder Korrektur temperaturgewichteten Fortschritts, Ausfallzeit,
  UTC-Intervalls, Konfidenz oder automatischen Phasenabschlusses aus #18;
- Konfigurationsgraphen, Active-/Fallback-Manifeste, Pending, Intent,
  `RuntimeConfigurationSnapshot`, `StorageEpoch`-Bootstrap, Werksreset,
  Secret- oder Konfigurationspersistenz aus #16/#56/#57;
- Journale, dauerhaftes Messhistorienformat, Aufbewahrung, Bereinigung,
  Export, Backup oder Import aus #19;
- produktiver NVS-/Preferences-/LittleFS-Adapter, Partitionierung,
  Verschluesselung, reale Flashatomizitaet, Flashlebensdauer oder ein
  Hardware-/Watchdogversuch;
- GPIO, BTS7960, Sensoradapter, Display, WLAN, Arduino- oder ESP-IDF-Typen;
- eine allgemeine Datenbank-, Event-Sourcing-, Repository- oder
  Transaktionsplattform;
- Live-Issue-, Label-, Milestone- oder Statusaenderungen, insbesondere keine
  Umstellung von #17 auf `READY`.

## Verbindliche Quellen und Entscheidungen

Die Prioritaet aus `docs/SPECIFICATION_REVIEW.md` bleibt verbindlich. Fuer
diesen Plan sind insbesondere massgeblich:

- das Live-Issue #17 einschliesslich seines Owner-Synchronisationskommentars;
- Root-`AGENTS.md` sowie die Modulregeln in `lib/device_platform`,
  `lib/device_platform_test_support` und `lib/fermentation_app`;
- `docs/RUN_PERSISTENCE.md`;
- `docs/RECOVERY_AND_INTERRUPTION.md`;
- ADR-018 in `docs/DECISIONS.md`;
- `docs/IMPLEMENTATION_ISSUES.md`;
- die gemergten Grundlagen aus #13, #14, #15 und #54 und ihre vorhandenen
  Implementierungen und Tests.

Verbindliche Ableitungen fuer #17:

- Direkte GPIO-, H-Bruecken- und letzte Heiz-/Kuehlfreigaben werden nie im
  Kontrollpunkt gespeichert oder als Bootbefehl wiederhergestellt.
- Ein Lauf bleibt durch den #13-Programmschnappschuss und seine begrenzten
  Laufrevisionen unveraenderlich nachvollziehbar.
- Ereignisse mit Laufwirkung werden unabhaengig vom Periodenintervall sofort
  kontrollpunktiert; der Sensorzyklus schreibt nicht im Zwei-Sekunden-Takt.
- Der Kandidat mit der hoechsten Revision ist nur verwendbar, wenn Envelope,
  Recordidentitaet, Payloadschema und fachliche Laufinvarianten vollstaendig
  gueltig sind. Sonst wird der naechstaeltere gueltige Kandidat verwendet.
- Ohne vollstaendig rekonstruierbaren Laufschnappschuss wird nichts geraten.
  Die konkrete sichere Boot- und Recoveryreaktion bleibt jedoch #18/#24.
- ADR-018 koppelt #17 nur an seinen tatsaechlichen fachlichen Vertrag; es
  erzeugt keine pauschale Abhaengigkeit auf #16/#56/#57.

`IMPLEMENTATION_ISSUES.md` nennt #57 noch als laufenden Draft-PR, waehrend
Issue #57 live geschlossen ist. Das ist als Dokumentationsbefund festzuhalten,
veraendert aber weder die harten #17-Abhaengigkeiten noch den hier geplanten
Storeport und wird in diesem PR nicht korrigiert.

## Aktuelle Ausgangslage und Wiederverwendung

### Vorhandene fachliche Grundlage

- #13: `ActiveRun`, `RunProgramSnapshot`, `EffectiveRunValues` und bis zu 32
  `RunRevision`-Eintraege. `ActiveRun::restore()` prueft bereits
  Quellprogramm, Reihenfolge, monotone Epochen, Zeitstempel und
  Anpassungsmetadaten.
- #14: `ProcessRunSnapshot`, `ProcessRuntimeState`, Prozesszustandsautomat und
  die expliziten Boot-/Recoveryereignisse `BootRecoverRun`, `RecoveryResume`
  und `RecoveryReject`. Eine Recoveryentscheidung selbst fehlt absichtlich.
- #15: Laufkommandos und Meldungen nutzen die vorhandenen fachlichen
  Werttypen; kein Persistenzdienst existiert.
- #54: begrenzter binaersicherer `IStateStore`, atomarer Replace pro Key,
  getrennte Lese-/Schreibstatus, `CommitOutcomeUnknown`, starke technische
  Typen, Envelope V1 mit CRC, Kandidatenscan, gebundenes Payload-Nachladen und
  Round-Robin-Slotwahl.
- #54-Testunterstuetzung: `SimulatedPersistentStateStore` modelliert Erfolg,
  Kapazitaet, Stromausfall vor Commit, Stromausfall nach Commit mit unbekanntem
  Ergebnis, Leseausfall, NotFound, persistente Korruption und Neustart.

### Fehlende fachliche Bausteine

Es gibt noch keinen Laufrecord, keinen Laufpayloadcodec, keine kurzen
Laufslotkeys, keine fachliche Kontrollpunktrotation, keine Entscheidung fuer
Sofort- gegen periodische Persistenz und keinen Testerweis des Rueckfalls einer
Laufrevision. Diese Verantwortungen gehoeren in `fermentation_app`, nicht in
den technischen Store.

## Geplanter Modul- und Dateischnitt

### `lib/device_platform`

Keine Aenderung geplant. `IStateStore`, Envelope, CRC, starke Typen,
Kandidatenscan und Rotation sind ausreichend. Ein Bedarf an einer neuen
generischen Store- oder Transaktionsschnittstelle waere eine materielle
Planabweichung.

### `lib/device_platform_test_support`

Keine Aenderung geplant. Der vorhandene `SimulatedPersistentStateStore`
genuegt. #17-spezifische Testdaten und etwaige kleine Zugriffsjournale bleiben
lokal in den Anwendungstests, damit keine Fermentationssemantik in einem
anwendungsneutralen Testmodul entsteht.

### `lib/fermentation_app`

Neue Produktionsdateien:

- `run_persistence_limits.hpp`
  - einzige Quelle fuer feste #17-Grenzen: Schema- und Record-Type-ID,
    Anzahl Laufslots, Payload-/Envelope-/Readbackobergrenze,
    Revisionsobergrenze und Intervallminimum, -standard und -maximum;
  - keine konfigurierbaren Laufzeitwerte und keine Hardwarebudgets.
- `run_checkpoint.hpp` und `run_checkpoint.cpp`
  - typisierte fachliche Daten: Kontrollpunktrevision, Ausloeser,
    Zeitqualitaet, letzter verlaesslicher UTC-Anker, monotone Laufzeit,
    Persistenzmetadaten sowie der kombinierte `ActiveRun`-/Prozesskontext;
  - Plausibilitaet, Gleichheit und eine explizite Wiederherstellbarkeits-
    klassifikation, aber keine Recoveryaktion.
- `run_checkpoint_codec.hpp` und `run_checkpoint_codec.cpp`
  - einziges deterministisches, begrenztes Schema-1-Binaerformat fuer den
    fachlichen Payload; vollstaendige Dekodierung und Validierung vor
    Rueckgabe; keine Storezugriffe und keine Duplikation von Envelope/CRC.
- `run_checkpoint_store.hpp` und `run_checkpoint_store.cpp`
  - fachliche Keys, Envelopebindung, Kandidatenauswahl, sichere Rotation,
    Write-/Readback-/Unknown-Outcome-Matrix und Laden des neuesten gueltigen
    Kontrollpunkts ueber `IStateStore`;
  - verweist nur auf eine vom Aufrufer gelieferte gueltige `StorageEpoch`, ohne
    Bootstrap- oder Konfigurationssemantik zu kennen.
- `run_checkpoint_service.hpp` und `run_checkpoint_service.cpp`
  - kleine anwendungsinterne Orchestrierung fuer Ausloeser und Fälligkeit;
    erzeugt nur auf expliziten fachlichen Ereignissen oder nach abgelaufenem
    Intervall einen Schreibauftrag und liefert ein typisiertes Ergebnis;
  - besitzt keine Schleife, keine reale Uhr, keine Sensorabfrage und keine
    Aktorwirkung. Zeit wird als Wert vom Aufrufer uebergeben.

Voraussichtlich geaenderte bestehende Dateien:

- `lib/fermentation_app/src/fermentation_application.*` nur, falls die
  bestehende fachliche Composition einen schmalen injected
  `RunCheckpointService`-Handoff bereits ohne Plattformadapter aufnehmen kann.
  Ist dies nicht moeglich, bleibt die Integration aus dem Plan heraus; eine
  Erweiterung von Composition Roots oder ein produktiver Storeanschluss ist
  keine stillschweigende Alternative.
- `lib/fermentation_app/src/run_snapshot.*` und
  `process_state_machine.*` nur fuer eng begrenzte, benoetigte read-only
  Vergleichs- oder Validierungshilfen. Neue Lauf- oder Recoverysemantik ist
  dort nicht geplant.

Falls eine dieser bestehenden Dateien mehr als eine fachlich neutrale Hilfe
braucht, wird vor der Aenderung angehalten und der Plan erneuert.

### Native Tests

Neue Testdateien:

- `test/test_run_checkpoint_codec/test_run_checkpoint_codec.cpp` fuer
  Schema-Goldenwerte, deterministische Reihenfolge, Grenzwerte, unbekannte
  Enums, Trunkierung, Laengen-, Schema- und Validierungsfehler.
- `test/test_run_checkpoint_store/test_run_checkpoint_store.cpp` fuer
  Envelope-/Keybindung, Rotation, neueste gueltige Auswahl, Rueckfall,
  Read-/Kapazitaets-/Korruptions-/Unknown-Outcome-Matrix und Neustart.
- `test/test_run_checkpoint_service/test_run_checkpoint_service.cpp` fuer
  Fälligkeit, 1/5/60-Minuten-Grenzen, einmalige Ereignisbehandlung, keine
  Sensorzyklus-Schreiblast und Fehlerweitergabe.

Bestehende #13-, #14-, #15- und #54-Tests bleiben unveraendert als
Regressionsnachweis und werden zusammen mit den neuen gezielt ausgefuehrt.

## Daten-, Zustands- und Schnittstellenvertrag

### Kontrollpunktinhalt

Ein gueltiger Laufkontrollpunkt besitzt mindestens:

- nicht-null Kontrollpunktrevision und Schema;
- einen vollstaendig durch `ActiveRun::restore()` validierbaren
  `RunProgramSnapshot`, effektive Laufwerte und begrenzte Laufrevisionen;
- einen durch #14 validierbaren `ProcessRunSnapshot` und
  `ProcessRuntimeState`, jedoch keinen direkten Aktorzustand;
- aktuelle Prozessphase und ihren monotone Zeitbezug;
- kumulierten temperaturgewichteten Fortschrittswert als bereits berechneten
  fachlichen Wert, sofern vorhanden, nicht als eigene biologische Kurve;
- nominelle Dauer, Verlaengerungen und Korrekturen aus dem Laufmodell;
- letzten monotonen Zeitstand, optionalen letzten verlaesslichen UTC-Anker und
  expliziten Zeitqualitaetsstatus;
- letzte gueltige Temperaturen und Qualitaetszustaende der dokumentierten
  Sensorrollen als begrenzte Werte; fehlende Werte bleiben explizit fehlend;
- Zielqualifikations- und Prozessfortschrittszustand, letzte Zustandsaenderung
  sowie Kontrollpunktausloeser;
- konfigurierte Kontrollpunktintervallinformation und Hinweise auf
  ausgelassene/verspaetete Kontrollpunkte, soweit vom Aufrufer vorhanden;
- keine Meldungshistorie, Rohmessreihe, Exportdaten oder unbeschraenkten
  Anhang.

Der konkrete Name und die genau typisierte Darstellung jedes bereits in #18
fachlich entschiedenen Zeit-/Fortschrittswerts werden vor Implementierung am
tatsaechlichen #13/#14-Vertrag festgelegt. Eine neue biologische Formel,
Zeitkorrektur oder Sensor-Fallbackpolicy wird nicht in #17 erfunden.

### Speicherschnittstelle und Revisionen

- Der Codec produziert nur den fachlichen Payload. `StorageEnvelope` V1
  liefert die technische Recordidentitaet, `StorageEpoch`, Schema,
  Versionwert, optionalen UTC-Wert, Laenge und CRC.
- Mindestens zwei, als Konstante festgelegte kurze Slots enthalten die
  aktuelle und eine aeltere Rueckfallrevision. Die genaue feste Anzahl muss
  vor Implementierung gegen Payloadgroesse und #54-Scanlimit nachgewiesen
  werden; mehr als zwei ist nur mit dokumentierter Ressourcenbegruendung
  zulaessig.
- Der Store schreibt ausschliesslich einen bereits vollstaendig validierten,
  begrenzten neuen Record in einen nicht geschuetzten naechsten Slot. Erfolg
  wird erst nach `Success` oder einem vollstaendigen Readback bei
  `CommitOutcomeUnknown` gemeldet.
- `WriteError` und `CapacityError` aendern den zuvor bestaetigten lokalen
  Kontrollpunkt nicht. Bei nicht bestaetigbarem `CommitOutcomeUnknown` wird
  kein Nachfolgeschreiben versucht und der Befund typisiert an den Aufrufer
  gegeben.
- Das Laden scannt nur Metadaten, laedt dann Kandidaten absteigend und gibt den
  ersten vollstaendig technischen und fachlichen gueltigen Record zurueck.
  Es verwechselt `NotFound` nie mit Lese- oder Integritaetsfehlern.

### Ereignisse und Zeit

`RunCheckpointService` unterscheidet mindestens `RunStart`, Prozessphasen-
wechsel, Produkt-eingesetzt-Bestaetigung, Primaersensorwechsel,
laufrelevanten Sensorstatuswechsel, Laufanpassung, automatische
Fortschrittskorrektur, Warnung/Fehler mit Laufwirkung, Quittierung/Reset mit
Zustandswirkung, Kuehl-/Haltebeginn und -ende, Abbruch, Abschluss und
spaetere Wiederanlaufentscheidung als fachliche Ausloeser. Der Service nimmt
nur den bereits entschiedenen Ausloeser entgegen; welche Komponente ein
Ereignis erzeugt oder wie Recovery entschieden wird, bleibt ausserhalb.

Ein periodischer Kontrollpunkt wird fruehestens nach dem gespeicherten
Intervall fällig. Die feste Validierung akzeptiert nur 1 bis 60 Minuten und
setzt 5 Minuten als Werkseinstellung. Ein Ereigniswrite verschiebt die
naechste periodische Fälligkeit nachvollziehbar; mehrfache Aufrufe im selben
Zeitpunkt erzeugen keinen zweiten Record. Rueckwaertige monotone Zeit wird
typisiert abgelehnt statt eine Fälligkeit zu erfinden.

## Fehler-, Recovery-, Security- und Safetygrenzen

- Jeder Boot, Reset oder Persistenzbefund bleibt ausserhalb des Kontrollpunkt-
  stores aktorsicher: #17 gibt keinen GPIO-, Heiz-, Kuehl- oder
  Wiederanlaufbefehl aus.
- Ein unlesbarer, korrupter oder fachlich ungueltiger neuester Record wird
  sichtbar als Rueckfall-/Nichtrekonstruierbarkeitsbefund geliefert, aber
  nicht still repariert oder geloescht.
- Fehlt jeder vollstaendige Kandidat, meldet der Store klar, ob kein Record
  gefunden, ein technischer Speicherbefund oder keine rekonstruierbare
  Laufrevision vorliegt. #18/#24 bilden dies spaeter auf `SAFE_BOOT`, Fault
  oder Recoveryentscheidung ab.
- Der Record speichert keine Credentials, Secrets, direkte Hardwarepegel oder
  UI-/Web-Sitzungen. Die Payloadgroesse ist vor Dekodierung begrenzt.
- `StorageEpoch` wird nicht aus Konfiguration abgeleitet und nicht durch #17
  mutiert. Ein fremder oder ungueltiger Epochrecord ist kein Kandidat.
- Ein Persistenzfehler-Latch und seine physische Redundanz sind nicht Teil
  dieses Plans; die Fehlersignatur muss aber so typisiert sein, dass #24 sie
  spaeter fail-closed behandeln kann.

## Ressourcenwirkung und Messgates

Der Plan begrenzt statisch die Zahl von Slots, Laufrevisionen und Payload-/
Envelope-/Readbackbytes. Der Codec und die Kandidatenauswahl duerfen nicht
mit einer Messhistorie, JSON-Dokumenten oder einer Slotzahl mal Payloadgroesse
wachsen. Beim Scan wird wie in #54 nur ein Kandidatenpayload nachgeladen.

Vor Abschluss einer Implementierung sind mindestens native Peakallokation,
kodierte maximale Payload- und Envelopegroesse sowie die Flashwirkung der
ESP-IDF-Profile zu messen und gegen die 4-MB-/ohne-PSRAM-Grenze zu berichten.
Reale NVS-Kapazitaet, atomarer Replace bei Stromunterbruch, Schreibdauer,
Flashverschleiss, Regelzyklusjitter und Watchdogwirkung bleiben
`MEASUREMENT_REQUIRED` beziehungsweise `SPIKE_REQUIRED`; sie werden weder
durch den Simulator noch durch diesen Plan als bestanden behauptet.

## Umsetzungsreihenfolge und kleiner PR-/Commit-Schnitt

Nach Ownerfreigabe bleibt die Umsetzung im selben Draft-PR und erfolgt in
kleinen, nachvollziehbaren Commits:

1. Grenzen und rein fachliche Kontrollpunktmodelle mit Validierung;
2. deterministischer Schema-1-Payloadcodec und Negativ-/Golden-Tests;
3. Store, Slotrotation und vollständige Cut-Point-/Rueckfalltests;
4. Fälligkeitsservice und Ereignis-/Intervalltests;
5. nur falls durch die vorhandene Anwendungsschnittstelle ohne
   Plattformintegration moeglich: schmaler injected Handoff;
6. Dokumentation, Ressourcenbericht und vollständige Qualitaetspruefung.

Jeder neue Produktionsport, jedes geaenderte Persistenzwireformat ausserhalb
des Laufpayloads, eine neue Abhaengigkeit oder eine Composition-Root- bzw.
Hardwareintegration ist eine materielle Planabweichung.

## Teststrategie und Abnahmekriterien

Vor einem spaeteren Abschluss sind auszufuehren:

- gezielte neue native Tests und der komplette native Testsatz;
- Builds aller vorgesehenen Profile gemaess aktuellem ESP-IDF-Vertrag;
- `git diff --check`, Format-, Konsistenz- und Secretpruefung;
- gezielter Nachweis der Plan-/Diff-Korrelation und SOLID-/DRY-/KISS-Pruefung.

Die fachliche Testmatrix umfasst mindestens:

| Bereich | Nachweis |
| --- | --- |
| Vollstaendigkeit | Start, Phasenwechsel, Anpassungsrevision und Prozessruntime lassen sich nach Encode/Decode exakt validieren. |
| Unveraenderlichkeit | Eine Aenderung des Quellprogramms aendert den gespeicherten Laufschnappschuss nicht. |
| Grenzen | Revisionen, Enums, optionale Zeitwerte, Laengen und 1/5/60-Minuten-Intervallgrenzen werden getestet; 0, >60 und rueckwaertige Zeit werden abgelehnt. |
| Sofort vs. periodisch | Jeder definierte Laufereignisausloeser schreibt einmal sofort; wiederholte Sensorzyklen vor Fälligkeit schreiben nicht. |
| Stromunterbruch | Vor Commit bleibt die alte Revision; nach Commit mit unbekanntem Ergebnis klaert Readback den Stand; Neustart behaelt nur committete Daten. |
| Rueckfall | Neuester CRC-, Envelope-, Schema-, Epoch-, Payload- oder Fachfehler faellt eindeutig auf den neuesten aelteren gueltigen Kontrollpunkt zurueck. |
| Nichtrekonstruierbar | Ohne vollstaendigen Laufrecord wird kein Pseudo-Run erzeugt und keine Aktorwirkung ausgeloest. |
| Storefehler | NotFound, ReadError, CapacityError und CommitOutcomeUnknown bleiben unterscheidbar und werden nicht als Erfolg geglaettet. |
| Ressourcen | Maximale Payload/Envelope/Readback- und native Peakwerte bleiben begrenzt und werden getrennt von realen Flashmessungen berichtet. |

Die Issue-Akzeptanzkriterien gelten erst als erfuellt, wenn der aktive Lauf
eindeutig rekonstruierbar oder explizit nicht rekonstruierbar ist, der
Korruptrückfall nachgewiesen ist, kein elektrischer Zustand persistiert wird,
die Schreiblast die Intervallgrenzen einhaelt und keine Konfigurations- oder
Aktorsemantik eingezogen wurde.

## Dokumentation nach freigegebener Implementierung

Voraussichtlich anzupassen sind nur die Nachweise, die der tatsaechliche Diff
erfordert:

- `docs/RUN_PERSISTENCE.md` fuer den final implementierten Record-/Grenz- und
  Fehlermatrixnachweis;
- `docs/IMPLEMENTATION_ISSUES.md` fuer eine sachliche Umsetzungsreferenz;
- `CHANGELOG.md` fuer die tatsaechliche Implementierung und Validierung.

Keine dieser Dateien wird in der Planungsphase geaendert.

## SOLID-, DRY- und KISS-Bewertung

| Prinzip | Planbewertung |
| --- | --- |
| Single Responsibility | Modell/Validierung, Codec, Store und Fälligkeitsorchestrierung haben getrennte Aufgaben. Prozess- und Recoveryentscheidungen bleiben in ihren bestehenden bzw. nachgelagerten Verantwortungen. |
| Open/Closed | Der Store haengt am bestehenden `IStateStore`; neue Backendadapter oder spätere Recovery-Policies erfordern keine Aenderung des Kontrollpunktcodecs. |
| Liskov Substitution | Jeder `IStateStore` mit seinem dokumentierten atomaren Replace-/Statusvertrag ist nutzbar; Tests beweisen dies mit der vorhandenen Simulation und lokalen Fakes. |
| Interface Segregation | Kein breiter Persistenz- oder Hardwaredienst: Codec hat keinen Store, Service keine Uhr-/Sensor-/Aktor-API, Store nur den vorhandenen schmalen Port. |
| Dependency Inversion | `fermentation_app` nutzt technische Abstraktionen aus `device_platform`, nie konkrete ESP-IDF-, NVS-, Arduino- oder Testsupport-Klassen. |
| DRY | Envelope, CRC, Slotmetadaten, technische Rotation, `ActiveRun::restore()` und Prozessvalidierung werden wiederverwendet. Es entsteht genau ein Laufpayloadcodec und eine zentrale Limits-Datei. |
| KISS | Zwei oder mehr feste Revisionen, ein kompakter Payload und eine kleine Fälligkeitslogik genuegen. Keine Datenbank, kein Journal, kein Eventbus und keine vorgezogene Recovery-/Konfigurationsplattform. |

Keine begruendete Abweichung ist geplant. Eine abstraktere gemeinsame
Persistenzschicht oder ein eigener Zeit-/Recoveryframework waere nicht KISS
und waere ohne neuen belegten Bedarf nicht zulaessig.

## Offene Gates und Ownerentscheidungen

- `TBD_IMPLEMENTATION_BUDGET`: genaue Payload-, Heap-, Flash- und
  ESP-IDF-Profilwirkung erst nach Implementierung messen.
- `MEASUREMENT_REQUIRED`: reale NVS-/Flashatomizitaet, Kapazitaet,
  Verschleiss, Commitdauer, Jitter und Watchdogwirkung.
- `SPIKE_REQUIRED`: reale Stromunterbrueche an spaeterem Produktionsadapter.
- `TBD_COMMISSIONING`: die konkrete Serviceeinstellung innerhalb 1–60 Minuten,
  maximale automatisch akzeptierte Zeitunsicherheit und thermische Modelle.
- `FINAL_SELECTION_PENDING`: exakte feste Laufslotanzahl nur nach begrenztem
  Payload-/Ressourcennachweis; mindestens zwei bleiben verpflichtend.
- Ownerfreigabe fuer genau den Plancommit ist vor jeder Implementierung
  erforderlich.

## Plan-Selbstpruefung

- Scope: nur #17-Laufrecord, Codec, kontrollpunktierte Rotation und
  Fälligkeit; keine vorgezogene #18/#19-/Hardwarearbeit.
- Abhaengigkeiten: die vier harten Grundlagen sind live abgeschlossen;
  Konfigurationspersistenz bleibt nur am expliziten Epochparameter beruehrt.
- Safety: kein Aktorzustand und keine Bootfreigabe werden gespeichert oder
  abgeleitet.
- Testbarkeit: alle neuen Verantwortungen sind mit `IStateStore` und Werten
  im nativen Profil testbar.
- Ressourcen: Grenzen und reale Messgates sind explizit statt stillschweigend
  offengehalten.

Ergebnis der Plan-Selbstpruefung: `PASS`.
