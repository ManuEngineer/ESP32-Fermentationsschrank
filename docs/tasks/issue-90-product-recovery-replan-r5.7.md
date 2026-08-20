# Issue #90 – Scope-, Gate- und Verifikationsgrenzen des Product-Recovery-Plans (R5.7)

## Status, Zweck und Owner-Gate

Diese Datei ist die vollständige eigenständige kanonische Planrevision R5.7
für PR #117. Sie korrigiert ausschließlich die Scope-, Owner-Gate- und
Verifikationsgrenzen der R5.6-Produkt-Recoveryplanung. Die verbindliche
Release-1-Produktentscheidung aus R5.6 bleibt unverändert; sie wird in R5.7
nicht erneut als Architekturfrage geöffnet.

Bis zur Freigabe der exakten Plan-Commit-SHA gilt:

`PRODUCT_RECOVERY_REPLAN_PENDING_R5_7_PLAN_APPROVAL`

Verifizierter Ausgangsstand dieser Planrevision:

- PR #117 ist OPEN und Draft;
- Branch: `agent/issue-90-nvs-adapter-plan`;
- aktueller PR-Head vor R5.7: `a84ae061abc7199d8285cd142c139cd829cb66e9`;
- Base-Branch: `agent/issue-29-esp32-bringup-plan`;
- R5.6, nicht ownerfreigegeben und nicht umzusetzen:
  `docs/tasks/issue-90-product-recovery-replan-r5.6.md @
  a84ae061abc7199d8285cd142c139cd829cb66e9`;
- R5.5, weiterhin nicht ownerfreigegeben und unangewendet:
  `docs/tasks/issue-90-architecture-replan-r5.5.md @
  1dfa7eb390ededec4dc58efee7138d3fffdd39ff`;
- vorheriger ownerfreigegebener, am definierten Blocker angelangter und
  unveränderter R5.4-Plan:
  `docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md @
  c35ce0342898f0e19d3cce5e6a7eaa077f73bad6`;
- korrigierter Phase-A.1-Herstellerbericht:
  `docs/ISSUE_90_MANUFACTURER_ANALYSIS.md @
  440180f5b712a1eda427292207ebc7e9861cd284`;
- produktive ESP-IDF-Baseline bleibt `v6.0.2 @
  7101770dc6db2667b3c477cc31365dd1acd6db4e`.

R5.4 bleibt die historische immutable Owner-Blockerrevision. R5.5 und R5.6
bleiben historische, nicht ownerfreigegebene und nicht umzusetzende
Planrevisionen. R5.7 wird erst nach der ausdrücklichen Freigabe seiner
exakten SHA zur normativen Planwahrheit.

Die Freigabe dieser exakten R5.7-SHA autorisiert ausdrücklich **keinen**
Umsetzungsslice. Jeder Slice 1–6 benötigt vor seinem Beginn eine eigene,
explizite Ownerfreigabe. PASS-Evidenz eines Slices autorisiert niemals den
nächsten Slice automatisch.

## Plan-only-Grenze dieser Runde

Diese R5.7-Runde erstellt und synchronisiert nur Plan-, Roadmap- und die
zugelassenen dynamischen PR-/Issue-/Handover-Metadaten. Nicht ausgeführt
werden:

- Produktionscode- oder Composition-Root-Änderungen;
- `IStateStore`-API-, Status- oder NVS-Adapteränderungen;
- Test-, Fault-Injection-, Oracle-, Runner- oder Harnessänderungen;
- Dependency-, Backend-, Partition- oder ESP-IDF-Pinänderungen;
- LittleFS-, FlashDB-, FatFs-, SPIFFS- oder Eigen-Flash-Evaluation;
- materielle Änderung von ADR-016 oder anderen kanonischen Fachverträgen;
- Host-, ESP-IDF-, Board-, Power-Cut- oder Hardwaretests;
- reale UI-, Display-, Touch-, Lüfter-, MOSFET-, BTS7960- oder
  Peltierintegration;
- R5.5 oder R5.6 umsetzen;
- PR auf Ready for review setzen, mergen, Auto-Merge aktivieren, Issue #90
  schließen, Branch löschen oder Force-Push.

Die R5.7-Datei bleibt nach Ownerfreigabe byte-identisch auf ihrer
freigegebenen SHA. Sie wird danach nicht für neuen HEAD, Testergebnisse,
Gate-Status oder laufende Evidenz fortgeschrieben. Diese dynamischen Angaben
gehören in `docs/ROADMAP.md`, den PR-Body, Issue #90, genau einen aktuellen
`SESSION HANDOVER` sowie vorgesehene Evidence-/Build-/Hardwareberichte.
Eine neue Planrevision ist nur bei materieller Abweichung von Produktvertrag,
Scope, Architektur, API, Recoverysemantik, Backend-/Dependencyentscheidung,
Teststrategie oder Hardware-Abnahmevertrag erforderlich.

## Unveränderte R5.6-Produktentscheidung

R5.7 übernimmt ohne Neubewertung den realen Release-1-Produktvertrag aus
R5.6:

- Release 1 ist kein hochverfügbarer Transaktionscontroller, Medizinprodukt
  oder Safety-PLC;
- nach Stromausfall muss die Firmware zuverlässig booten;
- das Gerät muss danach sicher, benutzbar und diagnostizierbar bleiben;
- persistente Daten dürfen nur nach vollständiger Validierung auf der
  zuständigen Ebene verwendet werden;
- beschädigte, unvollständige, widersprüchliche oder nicht eindeutig
  rekonstruierbare Daten dürfen nie still aktiviert werden;
- aus unklarer Persistenz- oder Recoverylage darf keine produktive
  Aktorfreigabe entstehen;
- ein Lauf soll nach Möglichkeit fortgesetzt werden, aber Resume ist
  Best-Effort und keine Verfügbarkeits- oder Safety-Garantie;
- Verlust des gerade geschriebenen Records, des neuesten noch nicht sicher
  aktivierten Konfigurationsstands, des jüngsten Checkpoints oder eines
  einzelnen Laufs ist zulässig;
- ein nicht eindeutig rekonstruierbarer Run wird kontrolliert nicht
  automatisch fortgesetzt;
- eine stille Factory-New-Annahme, ein falsches Resume, ein Bootloop oder
  eine stille Aktorfreigabe bleiben unzulässig.

Die Einzel-Key-Forderung

```text
existing key -> same-key overwrite -> Cut an jedem internen Backendfenster
-> immer exakt OLD oder NEW
```

ist keine Release-1-Produktanforderung. Der bestätigte Callback-12-/
`NotFound`-Befund bleibt eine reale und sichtbare Backendcharakterisierung.
Er wird nicht gelöscht, umetikettiert oder als Backend-PASS dargestellt.
Nach dem neuen Produktvertrag ist er allein jedoch kein Produkt-Release-
Blocker, wenn die darüberliegende Recoveryarchitektur den Zustand erkennt,
keinen ungültigen Record aktiviert und sicher auf gültigen Zustand oder
Recovery-required fällt.

## R5.7 korrigierte Scopegrenzen

### Keine noch nicht vorhandene UI als #90-Abnahmegate

Issue #90 muss einen Recovery-/Fehlerzustand typisiert beziehungsweise
eindeutig klassifizierbar und für spätere Projektion beobachtbar machen. Dafür
genügen Hosttest, Harness, UART oder Logs. R5.7 verwendet für diese Aussage
vorzugsweise:

`RECOVERY_STATUS_OBSERVABLE`

Die Klassifikation bedeutet:

- der Zustand ist programmatisch oder über Harness/UART/Logs eindeutig
  feststellbar;
- seine Ursache beziehungsweise Recoveryklasse wird nicht als Erfolg
  verschleiert;
- eine spätere UI kann den Zustand ohne neue Fachentscheidung projizieren.

`UI_DIAGNOSTICS_AVAILABLE` ist kein #90-Produktgate. Issue #90 beweist nicht:

- eine reale lokale Touch-UI;
- reale Recovery-Screens;
- Bildschirmbedienbarkeit;
- Display- oder Touchfunktion;
- eine UI-Komposition, die im aktuellen Produktstand noch nicht existiert.

Die spätere Verantwortung bleibt getrennt:

- #25 übernimmt die #90-Status- und Recoveryzustände in
  rendererunabhängige View-/Statusmodelle;
- #26 beweist lokale simulierte Shell-/Bedien- und Recoverydarstellung;
- #31 beweist reale Display-/Touchdarstellung und Kalibrierung.

R5.7 erfindet keine neue UI-API allein für Issue #90.

### Actor-free Hardwaregrenze

Alle realen #90-Hardwaretests sind actor-free. Das bestehende #90-/Bring-up-
Profil hält reale Aktoren deaktiviert. #90 darf auf dem Board nur beweisen:

- Firmwareboot und Reboot;
- ESP-IDF-/NVS-/Flashpersistenz und deren Fehler-/Recoveryklassifikation;
- dass der logische Safety-/Actuator-Gate bei unklarer Recovery geschlossen
  bleibt;
- dass keine produktive Aktoranforderung freigegeben wird;
- dass Harness, UART oder Logs den Recoveryzustand beobachten lassen.

Das sind keine elektrischen Aussagen über Ausgänge. #90 behauptet nicht:

- geprüfte physische Gate-/MOSFET-Pegel;
- sichere reale Lüfter- oder Summeransteuerung;
- BTS7960-/Peltier-Sicherheit;
- eine produktive physische Aktorfreigabe.

Die Grenzen bleiben: offene unbelastete MCU-/Gate-/Bootpegel gehören zu #29,
reale Lüfter-, Summer- und MOSFET-Hardware zu #32 und reale
BTS7960-/Peltierhardware zu #33.

### Kein versteckter Vollprodukt- oder Composition-Root-Scope

Der aktuelle Source-Stand hat noch keine vollständige reale Persistenz-,
Sensor-, Planner- und Safety-Komposition in
`FermentationApplication::begin()`/`update()`. Der reale #90-Boardpfad ist
der separate `APP_ISSUE_90_NVS_HARDWARE_TEST`-Harness.

R5.7 plant deshalb bewusst zwei Verifikationsebenen:

1. **Host:** vorhandene produktive Konfigurations-, Run-Persistenz- und
   Safety-Komponenten gemeinsam gegen die Produkt-Recovery-Orakel prüfen.
2. **Board:** der bestehende #90-Harness prüft reale ESP-IDF-/NVS-/Flash-
   semantik an repräsentativen Konfigurations- und Run-Records, Reboot,
   Persistenz, Ressourcen und Recoveryklassifikation actor-free.

Der Boardharness muss keinen vollständigen Fermenterworkflow, keine echte
UI und keine produktive Aktorkette nachbilden. Eine spätere Änderung an
`app_main`, `FermentationApplication` oder dem Composition Root darf nicht
still in Slice 4 hineinrutschen. Sollte Slice 3 nachweislich eine solche
Komposition für den #90-Produktvertrag benötigen, muss ein exakter Scope mit
Begründung und eigenem Owner-Subgate vorgelegt werden. Default bleibt: kein
unnötiger Vollprodukt-Integrationsscope in #90.

### Aussagekraft manueller Power-Unterbrechungen

Ohne kontrolliertes Power-Cut-Fixture beweist ein manueller Power-Off nicht,
an welchem internen NVS-Callback, BLOB-DATA-Schritt, Indexschritt oder
GC-Fenster die Versorgung unterbrochen wurde. R5.7 formuliert reale
Hardwareevidenz deshalb nur als:

- ein sauberer Kontrolllauf;
- mindestens drei gezielte Wiederholungen je ausgewähltem Szenario;
- manuelle Unterbrechung während aktiver Konfigurations- oder
  Run-repräsentativer Schreiblast;
- anschließender realer Boot, NVS-Initialisierung, Zustandsklassifikation und
  actor-free Gatebeobachtung.

Diese Tests ergänzen die präzise Host-Fault-Injection und ersetzen sie nicht.
Ein externes exakt getriggertes Fixture ist kein Release-1-Muss.

### Unklarer Run wird nicht still destruktiv verworfen

Bei einem unklaren oder nicht rekonstruierbaren Run gilt:

- kein automatisches Resume;
- logischer Aktor-/Produktiv-Gate geschlossen;
- `RUN_RECOVERY_REQUIRED`, `RUN_ABORT_REQUIRED` oder ein bereits vorhandenes
  äquivalentes Modell;
- vorhandene Persistenzevidenz wird während der Bootanalyse nicht einfach
  gelöscht;
- der Zustand wird nicht still als `NoActiveRun` neu persistiert;
- ein späterer expliziter Benutzer-, Start-, Reset- oder Discard-Befehl darf
  den unklaren Run kontrolliert ersetzen oder verwerfen.

Eine einfache spätere Meldung wie „Der vorherige Lauf konnte nach dem
Stromausfall nicht sicher wiederhergestellt werden. Neuen Lauf starten?“ ist
mit Release 1 vereinbar. Erforderlich ist nicht eine komplexe Recovery-UI,
sondern ein korrekt beobachtbarer, nicht destruktiver und fail-closed Zustand.

## Drei Vertragsebenen und der minimale Storevertrag

### Ebene A – physisches Backend

Das Backend, voraussichtlich weiterhin ESP-IDF NVS, muss Fehler sauber
melden, nach einem bestätigten erfolgreichen Commit den vollständigen Wert
lesbar liefern und keine Erfolgsmeldung erfinden. Vorhandene Integritäts-,
Flash-, Wear- und GC-Mechanismen werden genutzt.

Nach echtem Power-Cut darf ein in Bearbeitung befindlicher Record alt, neu,
fehlend oder unlesbar/fehlerhaft sein. Das ist kein erfolgreicher Record,
aber nach R5.6 auch nicht allein ein Release-Blocker des Backends. Die
Backendebene muss den Zustand korrekt und unterscheidbar nach oben geben.

### Ebene B – Persistenz-/Recoveryarchitektur

Die höhere Ebene validiert vollständige Records über Envelope, CRC, Schema,
`StorageEpoch`, Referenzen und fachliche Invarianten. Sie verwendet die
bestehenden Generationen, Root-/Manifest-/Fallback- und Slotverträge,
erkennt verlorene oder ungültige Records und aktiviert nie einen Teil-,
Misch-, Prepared- oder Orphan-Record.

### Ebene C – Produktverhalten

Nach Power-Cut bootet das Gerät, der Zustand ist über den für #90 zulässigen
Programm-/Harness-/UART-/Logpfad beobachtbar, das logische Gate bleibt bei
unklarem Zustand geschlossen, und gültige Konfiguration kann in die für
Release 1 zulässige Recovery-/Setup-Semantik gelangen. Ein Run wird nur aus
einer eindeutig validierten Sicht best effort angeboten.

### Konkrete `IStateStore`-Planfolge

Die vorhandenen Statusfamilien bleiben der Ausgangspunkt:

- Write: `Success`, `WriteError`, `CapacityError`,
  `CommitOutcomeUnknown`;
- Read: `Success`, `NotFound`, `ReadError`, `CapacityError`.

`Success` darf nur einen tatsächlich bestätigten erfolgreichen Write/Commit
bezeichnen. `CommitOutcomeUnknown` bleibt Unknown und wird nicht als Erfolg,
alten Wert oder neuen Wert geraten. Read-`Success` liefert vollständige Bytes;
höhere Schichten prüfen deren fachliche Gültigkeit. Die generische Port-API
bleibt anwendungsneutral und enthält keine NVS-, ESP-IDF- oder
Fermentationsdetails.

Nach `CommitOutcomeUnknown` darf Readback eines zuvor vorhandenen Records
neben einer vollständig validierbaren Bytefolge auch `NotFound`, `ReadError`,
`CapacityError` oder einen vollständig lesbaren, höher ungültigen Record
ergeben. Diese Fälle werden als:

`RECORD_OUTCOME_INDETERMINATE_OR_LOST`

klassifiziert. Sie bedeuten weder Written noch NotWritten und stellen keinen
automatisch wiederhergestellten alten oder neuen Zustand dar. Die zuständige
höhere Persistenzschicht sucht anhand von Generationen, Roots, Manifesten,
Slots und expliziten Referenzen nach einem gültigen Fallback.

Slice 1 prüft zunächst die bereits vorhandenen Typen statt eine neue API zu
erfinden:

- Konfiguration: `ConfigurationRecordOutcomeIndeterminate`,
  `ConfigurationCommitIndeterminate` und typisierte Ursachen;
- Run: `RunPersistenceStoreWriteResult::Indeterminate`,
  `RunPersistenceLoadStatus::NotReconstructible` und
  `NotReconstructibleOrphanedState`;
- physischer Port: vorhandene Read-/Write-Statusfamilien.

Nur ein nachgewiesener, nicht ausdrückbarer Consumerzustand darf eine kleine
Statuspräzisierung begründen. Eine API-Erweiterung allein für sprachliche
Schönheit ist ausgeschlossen. R5.7 selbst ändert die API nicht.

## Wiederverwendung der vorhandenen Recoveryarchitektur

### Konfiguration

Die bestehende Konfigurationspersistenz bleibt vollständig im Scope der
späteren Produkt-Orakel, ohne neue parallele Generationsarchitektur:

- User-Dokumente `uc0..uc3`;
- Service-Dokumente `sc0..sc3`;
- Programm-Kataloge `pc0..pc3`;
- Manifest-Records `cm0..cm2`;
- Root-Records `cr0/cr1`;
- Bootstrap-Records `cb0/cb1`;
- Dokumentrevisionen;
- `ActiveConfigurationManifest`;
- aktive Generation und genau eine vorherige Fallbackgeneration;
- `StorageEpoch`;
- bestehende Copy-Migrationen.

Die Größen- und Inhaltsinventur aus R5.6 bleibt die Planbasis: unter anderem
kleine User-/Service-/Manifest-/Root-/Bootstraprecords und ein großer
Programmkatalog bis zur bestehenden realen Storegrenze. Slice 2 muss für
repräsentative reale Größen und die vorhandenen Graphreferenzen prüfen, nicht
nur einzelne Keys isoliert.

Zulässige Konfigurationsoutcomes:

```text
A) neueste vollständig gültige Konfiguration
B) letzte vollständig gültige Fallbackgeneration
C) kein sicherer Graph rekonstruierbar
   -> Recovery/SAFE_BOOT/Setup, keine stille Aktoraktivierung
```

Nicht zulässig sind teilweise neue plus teilweise alte Generationen,
beschädigte Kataloge, fehlende referenzierte Records als stilles Factory-New
oder ein heimlicher Reset. Ein alter kanonischer Root darf gültig bleiben,
wenn ein unreferenzierter neuer Zielrecord verloren ist. Ein referenzierter
fehlender oder ungültiger Record macht den Zielgraphen ungültig; es wird nur
auf einen vollständig validierten alten Graphen zurückgefallen. Sind weder
neuer noch alter Graph vollständig validierbar, muss der Zustand eindeutig
Recovery-required sein.

Slice 3 muss insbesondere prüfen, ob Root-/Manifest-/Bootstrapauflösung,
Fallbackreferenz und `StorageEpoch` diese Menge bereits erfüllen und ob nur
Statusabbildungen/Tests oder tatsächlich Produktionskorrekturen fehlen.

### Run

Die bestehende Run-Persistenz bleibt erhalten:

- Checkpointslots `rc0` und `rc1`;
- persistenter Kopf und Referenzen `rh0`;
- Checkpointrevisionen;
- Current-/Fallbackreferenzen;
- `Prepared`/`Committed`;
- Recovery-, Orphan- und High-Water-Logik.

Die zulässige Produktmenge ist:

```text
Power-Cut -> Reboot -> letzte eindeutig valide Run-Sicht suchen

eindeutig valide aktuelle Sicht:
    NEW_VALID_RESUME / Best-Effort-Resume

eindeutig älterer valider Checkpoint:
    OLDER_VALID_CHECKPOINT_RESUME

nicht eindeutig rekonstruierbar:
    RUN_RECOVERY_REQUIRED / RUN_ABORT_REQUIRED
    kein automatisches Resume, logischer Gate geschlossen
```

Der jüngste Checkpoint darf verloren gehen. `PreparedInterrupted`,
`NotReconstructible`, `NotReconstructibleOrphanedState`, fehlendes `rh0` und
unklare Current-/Fallbackreferenzen dürfen nicht durch „höchste Revision
gewinnt“ oder bloße `rc0`-/`rc1`-Existenz zu einem Resume werden. Ein älterer
Checkpoint ist nur bei explizit validierter Referenz sowie vollständiger
Envelope-/CRC-/Schema-/Epoch-/Fachprüfung zulässig.

Slice 3 prüft die aktuelle Projektion, insbesondere
`RunPersistenceStoreWriteResult::Indeterminate`,
`RunPersistenceLoadStatus::FallbackRecovered`, `Current`,
`PreparedInterrupted`, `NotReconstructible` und die bestehende
`SafetyCore`-Klassifikation. Ein Run-spezifischer Recovery-/Abortzustand soll
bei gültiger Konfiguration nicht unnötig das gesamte Gerät unbenutzbar
machen. Eine kleine Korrektur ist aber nur nach einem echten roten
Produktfall zulässig.

### Safe Boot und logischer Aktor-Gate

R5.7 unterscheidet klar zwischen sicherem logischem Verhalten und späterer
physischer Aktorabnahme:

- Firmware und der für #90 zugelassene Beobachtungspfad müssen booten;
- ein unklarer Zustand hält den logischen Safety-/Actuator-Gate geschlossen;
- #90 gibt keine produktive Aktoranforderung frei;
- eine gültige Konfiguration darf Diagnose, Reparatur, Setup oder einen
  expliziten Neustartpfad ermöglichen;
- ein unrekonstruierbarer Run darf nicht automatisch fortgesetzt werden,
  ohne deshalb automatisch alle gültigen Produktfunktionen zu zerstören.

Wenn die bestehende `SAFE_BOOT`-Semantik genau diesen fail-closed,
diagnostizierbaren Zustand liefert, wird sie wiederverwendet. Falls sie
heute wegen eines einzelnen unklaren Runs unnötig global blockiert, darf
Slice 4 nur eine kleine run-spezifische Projektion korrigieren. Keine zweite
Recoveryplattform und kein neuer Safety-Latch werden vorsorglich geplant.

## Backendcharakterisierung getrennt vom Produkt-Release-Gate

### Innere Backendklassifikation

Der vollständige NVS-BDL-Cut-Test bleibt als:

`BACKEND_POWER_CUT_CHARACTERIZATION`

Der konkrete Befund bleibt beispielsweise:

`FAIL_CALLBACK_12_NOT_FOUND`

und wird zusätzlich als:

`KNOWN_BACKEND_LIMITATION`

klassifizierbar. Der innere FAIL wird niemals zu PASS umetikettiert. Callback
12 wird nicht gefiltert und `NotFound` nicht zu „OLD“ erklärt. Eine
unerwartete Änderung des bekannten Befunds bleibt reviewpflichtig.

### Eigenständiges Produktgate

Das Release-Gate von Issue #90 misst nach jedem sinnvoll erreichbaren
mutierenden Backend-Cut beziehungsweise nach der für den Harness definierten
Cutmatrix die höhere Ebene: simulierter Reboot, vollständiges Reload,
Validierung, Recoveryprojektion und logischer Gate.

Maschinenlesbar ist mindestens folgende Trennung zu planen:

```text
backend_characterization:
    observed / known_limitation / unexpected_change

product_recovery_gate:
    PASS | FAIL | NOT_RUN
```

Der kurze `ci-regression`-Pfad darf bei unverändertem bekannten Befund grün
bleiben. Ein Exhaustive-/Characterization-Lauf darf seine konkrete
Callback-12-FAIL-Evidenz behalten, ohne den Produkt-Release-Gesamtstatus
zwangsläufig rot zu machen. Ein rotes Produktorakel bleibt unabhängig davon
ein echter Release-FAIL.

Produktoutcomes für Konfiguration:

- `NEW_VALID_CONFIGURATION`;
- `OLD_VALID_CONFIGURATION` oder `FALLBACK_VALID_CONFIGURATION`;
- `CONFIGURATION_RECOVERY_REQUIRED`.

Produktoutcomes für Run:

- `NEW_VALID_RESUME`;
- `OLDER_VALID_CHECKPOINT_RESUME`;
- `RUN_RECOVERY_REQUIRED` oder `RUN_ABORT_REQUIRED`.

Jeder Fall muss zusätzlich ausschließen: Partial/Mixed/Corrupt-Akzeptanz,
Prepared-/Orphan-Aktivierung, stilles Factory-New, falsches Resume und
logische Aktorfreigabe bei unklarer Recovery.

## Keine vorsorgliche Backend- oder Versionsöffnung

NVS bleibt die Default-Hypothese. R5.7 öffnet keine LittleFS-, FlashDB-,
FatFs-, SPIFFS- oder weitere Backendbewertung, solange NVS zusammen mit der
vorhandenen Recoveryarchitektur den verbindlichen Produktvertrag erfüllt.
Der Callback-12-Befund allein öffnet keinen Adopt-or-build-Vergleich.

Nur ein konkreter, nach Slice 3 belegter Produkt-FAIL darf einen neuen
Ownerentscheid und eine separate Adopt-or-build-Planrevision auslösen. Dann
müssten Herstelleroptionen und geeignete Embedded-KV-Komponenten vollständig
mit Version/Commit, Lizenz, Pflege, ESP-IDF-Kompatibilität, Power-Loss-
Semantik, Wear/GC, Größe, RAM/Stack, Partition, 4-MB-Budget,
Fault-Injection, Migration und Device-Platform-Wiederverwendung verglichen
werden. Eigenentwicklung bleibt letzte Referenzoption.

Die produktive Baseline bleibt `ESP-IDF v6.0.2 @
7101770dc6db2667b3c477cc31365dd1acd6db4e`. Kein Minor-Version-Wechsel,
Backport, Partitionstrick oder Dependencywechsel ist eine versteckte Lösung
dieser Planrevision. OTA bleibt `FUTURE_OPTIONAL` und ist irrelevant.

## Spätere ADR- und Vertragsfolge

R5.7 ändert ADR-016 und die kanonischen Fachquellen noch nicht materiell.
Nach Ownerfreigabe und im jeweils ausdrücklich freigegebenen Slice sind
mindestens folgende Aktualisierungen zu planen:

- `docs/ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md`: NVS bleibt gewählt,
  sofern der Produkt-Recoverybeweis PASS ist; „atomar pro NVS-Eintrag“ wird
  nicht als Multi-Page-Same-Key-Power-Cut-Garantie behauptet; Recordverlust,
  Validierung, Generation/Slot/Recovery und fail-closed Produktverhalten
  werden getrennt beschrieben;
- generischer `IStateStore`-Vertrag und Statusdokumente: bestätigter Erfolg,
  Unknown, Readback und `RECORD_OUTCOME_INDETERMINATE_OR_LOST` ohne
  Fermentations- oder ESP-IDF-Details;
- `docs/CONFIGURATION_PERSISTENCE.md`: vollständig gültige neue
  Konfiguration, vollständig gültige alte/Fallbackgeneration oder
  Recovery-required; keine Mischgeneration und kein stilles Factory-New;
- `docs/RUN_PERSISTENCE.md`: Best-Effort-Resume, explizit validierter älterer
  Checkpoint und kontrollierter Nicht-Resume ohne höchste-Revision-Heuristik;
- `docs/SETTINGS_AND_STORAGE.md`: Release-1-Verfügbarkeits- und
  Recoverygrenze ohne Einzel-Key-Hochverfügbarkeitsgarantie;
- `docs/CI_AND_QUALITY_GATES.md`: getrennte maschinenlesbare
  Backendcharakterisierung und Produkt-Recovery-Gate;
- relevante Quality-/Test-Gate-Dokumentation: actor-free #90-Grenze,
  beobachtbarer Status, Host-FI als Detailnachweis und gezielte manuelle
  Board-Power-Unterbrechung ohne exakte interne Cutbehauptung.

Die ADR-Folge darf weder eine falsche Herstellerbehauptung noch eine
überzogene Einzel-Key-Garantie einführen. Eine materielle Vertrags- oder
Architekturabweichung erfordert einen neuen Ownerentscheid und gegebenenfalls
eine neue Planrevision.

## Owner-gatete Umsetzungsslices

Die folgende Reihenfolge ist eine Planung, keine Vorautorisierung. Vor jedem
Slice steht eine ausdrückliche Ownerfreigabe; nach jedem Slice folgen
gezielte Nachweise, exakter Commit/HEAD, aktualisierte dynamische Statusquellen
und STOP am Ownerreview.

### Slice 1 – kanonischer Vertrags-/ADR-Abgleich

Nur Dokumentation, Vertragsabgleich und Prüfung, ob die vorhandenen
Statusfamilien ausreichen. Backendcharakterisierung und Produktgate werden
normativ getrennt beschrieben. Eine generische Statusänderung ist nur
zulässig, wenn ein konkreter fehlender Consumerzustand bewiesen ist.

Gate: klare, widerspruchsfreie Verträge für Backend, Record, Konfiguration,
Run, Safe Boot, logischen Aktor-Gate und beobachtbaren #90-Status; keine UI-,
Hardware- oder Composition-Root-Abnahme vorausgesetzt.

### Slice 2 – Produkt-Recovery-Orakel

Host-Fault-Injection mit simuliertem Reboot/Reload für Konfigurations- und
Run-Schreibpfade; `SafetyCore`-Projektion und Statusbeobachtung einbeziehen;
Backendcharakterisierung unverändert behalten und separat ausgeben.

Gate: alle zulässigen und unzulässigen Produktoutcomes reproduzierbar
klassifiziert; Callback-12-FAIL bleibt offen; kein UI- oder physischer
Aktorbeweis wird in das Orakel verschoben.

### Slice 3 – vorhandenen Produktionscode gegen das Orakel prüfen

Zuerst den bestehenden Produktionscode unverändert gegen Slice-2-Orakel
prüfen. Root-/Graph-/Fallback-/Bootstrapauflösung, `rc0`/`rc1`/`rh0`,
Prepared-/Orphanfälle, Unknown-Mappings und logische Gateprojektion
auswerten. Echte Produkt-FAILs von akzeptierter Backendcharakterisierung
trennen.

Gate: belegte Lückenliste mit PASS/FAIL/BLOCKED/NOT_RUN und Entscheidung, ob
der vorhandene Bestand genügt. Keine prophylaktische neue Recovery- oder
Backendschicht.

### Slice 4 – minimale Produktionskorrekturen

Nur für belegte Produkt-FAILs aus Slice 3:

1. vorhandene Generation-/Fallback-/Slotlogik korrigieren;
2. kleine bestehende Status-/Recoveryprojektion korrigieren;
3. nur danach eine zusätzliche Abstraktion als eigener Ownerentscheid
   vorlegen.

Kein stiller Backendwechsel und keine stillschweigende Einführung einer
vollständigen `app_main`-/`FermentationApplication`-Komposition. Wird ein
neuer materieller Composition-Root-Scope, ein API-Bruch, ein Backendwechsel
oder eine andere Architekturentscheidung erforderlich, endet der Slice mit
STOP und einem neuen Owner-Gate beziehungsweise einer neuen Planrevision.

### Slice 5 – Final Software Verification

Nach Review der freigegebenen Änderungen: relevante Native-Suite,
Produkt-Recovery-Fault-Matrix, getrennte NVS-Charakterisierung,
ESP-IDF-Profile auf v6.0.2, Static Analysis, Capacity/4-MB, RAM/Stack,
Write-/Wearbudget, Release-Isolation sowie Architektur-, Lizenz- und
Artefaktgates. Ein Backend-FAIL und ein Produkt-FAIL werden getrennt
berichtet; keiner wird verschleiert.

### Slice 6 – gezieltes actor-free reales Board

Nur nach grünem Host-/Softwarevertrag: NVS-/Flash-/Rebootnachweise,
repräsentative persistente Zustände, ein sauberer Kontrolllauf und mindestens
drei manuelle Power-Unterbrechungen je ausgewähltem Szenario während aktiver
Schreiblast. Danach werden Boot, NVS-Initialisierung, persistenter Zustand,
Recoveryklassifikation und logischer actor-free Gate über Harness/UART/Logs
beobachtet.

Dieser Slice beweist keine reale UI, kein Display/Touch, keine physische
Gate-/MOSFET-/Lüfter-/BTS7960-/Peltier-Sicherheit und keinen vollständigen
Fermentations-End-to-End-Lauf. Die Erkenntnis bleibt auf den #90-Harness und
den Release-1-Produktvertrag begrenzt.

## Issue-#90-Abnahmeziel unter R5.7

Issue #90 darf erst nach den ausdrücklich ownerfreigegebenen und bestandenen
Umsetzungsslices abgeschlossen werden. Seine eigene DoD lautet:

1. Der generische ESP-IDF-NVS-Adapter ist korrekt, fehlergetreu und
   anwendungsneutral integriert.
2. Backendfehler, Unknown und `NotFound` werden nie als falscher Erfolg
   interpretiert.
3. Konfigurations-Fault-Injection erzeugt nur eine vollständig gültige neue
   Konfiguration, einen vollständig gültigen alten/Fallbackgraphen oder
   `CONFIGURATION_RECOVERY_REQUIRED`.
4. Run-Fault-Injection erzeugt nur ein eindeutig gültiges Resume, ein
   ausdrücklich validiertes älteres Checkpoint-Resume oder
   `RUN_RECOVERY_REQUIRED`/`RUN_ABORT_REQUIRED`.
5. Kein beschädigter, unvollständiger, gemischter, vorbereiteter oder
   orphanierter Record wird produktiv aktiviert.
6. Das logische Safety-/Actuator-Gate bleibt bei unklarer Recovery
   geschlossen; #90 gibt keine produktive Aktoranforderung frei.
7. Recovery- und Fehlerstatus sind über programmatische beziehungsweise
   Harness-/UART-/Logbeobachtung eindeutig klassifizierbar
   (`RECOVERY_STATUS_OBSERVABLE`).
8. Die actor-free Boardverifikation beweist Boot, NVS-/Persistenzpfad,
   Reboot und Recoveryklassifikation, nicht physische Aktorsicherheit.
9. Ressourcen-, 4-MB-, RAM-/Stack-, Wear-/Write-, Build-, Analyse- und
   Lizenzgates sind bestanden.
10. Callback 12/`NotFound` bleibt als bekannte Backendlimitation offen
    dokumentiert und maschinenlesbar vom Produktgate getrennt.

Nicht Teil der #90-DoD sind reale lokale UI-/Recovery-Screens,
Display-/Touchfunktion, reale Lüfter-/MOSFET-Abnahme, BTS7960-/Peltier-
Abnahme oder ein vollständig zusammengesetzter produktiver
Fermentations-End-to-End-Lauf. Diese Nachweise bleiben bei #25, #26, #31,
#32, #33 und den jeweils vorgesehenen späteren Produktgates.

## R5.7-Nachweisstatus und Referenzen

Für diese Planrunde gelten:

- Live-Kontext und Bestandsanalyse: `PASS`;
- R5.4: historische ownerfreigegebene Blockerrevision, unverändert;
- R5.5/R5.6-Freigabe: `NOT_RUN` / nicht erteilt;
- Implementierung: `NOT_RUN` / nicht freigegeben;
- Test-, Oracle-, Runner- und Harnessänderung: `NOT_RUN`;
- Backend-, Dependency-, ESP-IDF-, Partition- und Hardwareänderung:
  `NOT_RUN`;
- Callback-12-/`NotFound`-Befund: bestehendes `FAIL`, nicht neu ausgeführt;
- reale UI- und physische Aktorsicherheit für #90: nicht Teil des Scopes;
- R5.7-Planfreigabe: `BLOCKED` bis zur ausdrücklichen Ownerentscheidung über
  die exakte R5.7-SHA;
- nach der Planfreigabe bleibt jeder Umsetzungsslice bis zu seiner eigenen
  ausdrücklichen Ownerfreigabe `NOT_RUN`.

Interne Primärquellen für die spätere Umsetzung und den Bestandsbeweis:

- `docs/ROADMAP.md`;
- `docs/tasks/issue-90-product-recovery-replan-r5.6.md`;
- `docs/tasks/issue-90-architecture-replan-r5.5.md`;
- `docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md`;
- `docs/ISSUE_90_MANUFACTURER_ANALYSIS.md`;
- `docs/ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md`;
- `docs/CONFIGURATION_PERSISTENCE.md`;
- `docs/RUN_PERSISTENCE.md`;
- `docs/SETTINGS_AND_STORAGE.md`;
- `docs/CI_AND_QUALITY_GATES.md`;
- `lib/device_platform/src/state_store.hpp`;
- `lib/fermentation_app/src/configuration_recovery_service.hpp`;
- `lib/fermentation_app/src/configuration_service.hpp`;
- `lib/fermentation_app/src/run_persistence_store.hpp`;
- `lib/fermentation_app/src/run_persistence_coordinator.hpp`;
- `lib/fermentation_app/src/safety_core.cpp`;
- #25, #26, #29, #31, #32 und #33 für die ausdrücklich getrennten
  UI-, Pegel-, Display-/Touch- und physischen Aktorgrenzen.

R5.7 führt keine neue externe Backendquelle ein und verändert keine
produktive Version. Nach Veröffentlichung der Plan-SHA wird der PR auf
`PRODUCT_RECOVERY_REPLAN_PENDING_R5_7_PLAN_APPROVAL` geführt und am
Owner-Gate angehalten.
