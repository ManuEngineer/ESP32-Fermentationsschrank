# Issue #168 – Application-owned Runtime-Evidence und UI-/Command-Projection

## Planstatus und Baseline

```text
ISSUE=168
SCOPE=APPLICATION_RUNTIME_EVIDENCE_AND_UI_COMMAND_PROJECTION
BASE_BRANCH=main
BASE_SHA=1f1755e5e706fb668472920545b5302fcef1df16
PLAN_STATUS=DRAFT_OWNER_APPROVAL_REQUIRED
IMPLEMENTATION=NOT_STARTED
IMPLEMENTATION_AUTHORIZATION=NO
CONSUMER=ISSUE_27_PR167_AFTER_ISSUE168_MERGE
ACTUATOR_RELEASE=NO
```

Dieser Plan ist der vorgelagerte, kleine Application-/Composition-Scope fuer
Issue #27. Er wird auf dem aktuellen kanonischen `main` erstellt. Die
Umsetzung endet nach dem Plan-Commit bis zur ausdruecklichen Freigabe genau
dieses Plan-Commits.

## Anlass und Problem

`FermentationApplicationOwningEvidence` ist im aktuellen Stand eine bereits
bewertete Eingabe an die Application-Grenze. Sie wird unter anderem von den
Run-/Start-/Stop-/Completion-Kommandos erwartet, ist aber nicht durch einen
Renderer, Touch-Client oder Web-Adapter erzeugbar. Diese externen Aufrufer
duerfen weder Safety-, Sensor-, Planner- noch Recovery-Evidence behaupten oder
zusammensetzen.

Der aktuelle `main` besitzt bereits die rendererunabhaengigen UI-Modelle und
den reinen `FermentationUiProjector`, aber noch keinen vollstaendigen
produktiven Runtime-Producer, der die Live-Sensor- und Safety-Evidence in der
`FermentationApplication` zusammenfuehrt. Insbesondere ist im aktuellen
`main` kein laufender DS18B20-Producer in `app_main` oder der Application-
Composition vorhanden. Deshalb muss der software-only Zustand ohne #30 echte
Sensorwerte sichtbar als nicht verfuegbar/ungueltig behandeln. Nullwerte,
erfundene Temperaturen und implizite Sensorfreigaben sind unzulaessig.

Ziel ist die kleinste Application-owned Composition/Handoff-Erweiterung, mit
der `FermentationApplication` die aktuelle Evidence selbst aufloest und
`FermentationUiSnapshot` sowie UI-/Web-Kommandos aus dieser kanonischen
Application-Quelle bedient. #27 soll diese Grenze nach dem Merge dieses
Vorgaengers konsumieren; #27 wird in diesem Scope nicht weiterimplementiert.

## Repository-first-Bestandsaufnahme

### Bereits vorhandene Owner und Vertraege

| Bereich | Kanonische Quelle auf `BASE_SHA` | Verwendung in diesem Scope |
|---|---|---|
| Run-/Prozess-/Recoveryzustand | `FermentationApplication::runtimeRunState_`, `RunPersistenceCoordinator`, `ConfigurationRecoveryService`, `PresentationState` und Lifecycle-Zustand | Application bleibt Owner der aktuellen Run-, Recovery-, Konfigurations- und Lebenszyklus-Sicht. |
| Sensorwerte und Qualitaet | `device_platform`-Vertraege `TemperatureReading`, `ITemperatureSource`, `SensorQualityPipeline` und `SensorQualitySnapshot` | Vorhandene Qualitaets-/Wertvertraege wiederverwenden; keine zweite Pipeline oder erfundene Livequelle. |
| Rollen und Plausibilitaet | `CrossRolePlausibilityContext` sowie die bestehenden #20/#21-Auswertungen | Application fuehrt die vorhandenen Ergebnisse fuer den aktuellen Kontext zusammen; keine neue Klassifikation. |
| Safety und Aktorfreigabe | bestehende #24-Vertraege, `ActuationEvidence`, `ActuationInterlock` und vorhandene Planner-/Recovery-Evidence | Bestehende fail-closed-Auswertung auf aktuelle Application-Evidence anwenden; keine neuen Safety-Regeln oder Aktorfreigaben. |
| Konfiguration und Revisionen | `ConfigurationService`, User-Configuration-/Lease-Vertraege, `RunCommandState` und vorhandene Revisionen | Snapshot und Kommandos lesen die vorhandenen Revisionen aus dem Application-Owner. |
| UI-Projektion | `FermentationUiProjectionInput`, `FermentationUiProjector`, `FermentationUiSnapshot` und `TemperatureView` | Projector bleibt pure Projektion; die Application liefert den kanonisch zusammengesetzten Input. |
| Fachkommandos | `FermentationApplication::prepareStartProgram`, `prepareStartManualHolding`, `prepareStartManualTimed`, `prepareStop`, `prepareCompletion`, `prepareEnvelope` sowie bestehende Persistenz-/Run-Command-Vertraege | Caller liefern nur Intent, Werte, Bestaetigung und erwartete Revisionen; Evidence wird intern aufgeloest. |

### Abgrenzung zu abgeschlossenen oder spaeteren Scopes

Die bestehenden #20/#21-, #24-, #25/#26- und #144/#152-Vertraege werden
konsumiert, nicht neu entschieden. #119 bleibt der geschlossene historische
und nicht wiederzubelebende Composition-/StateStore-Pfad.

Der Scope enthaelt weder DS18B20-Treiber, GPIO-/ROM-/CRC-/Hot-Plug-
Nachweise noch reale Sensorhardware aus #30. Er enthaelt auch keinen
Display-/Touch-/Rendererpfad aus #31, keine Diagnose-/Service-/Exportlogik aus
#28, keine Werte-/Safety-Commissioning-Entscheidungen aus #35 und keine
produktive Per-Run-Aktoraktivierung aus #106.

## Zielvertrag

### Application als alleiniger Evidence-Owner

Die `FermentationApplication` erhaelt eine kleine, application-interne
Runtime-Evidence-Komposition bzw. einen Handoff. Die konkrete Form soll in den
bereits vorhandenen Application-Dateien bleiben, solange dies ohne neue
allgemeine Provider-, Event- oder Service-Locator-Abstraktion moeglich ist.

Die Application bildet fuer jeden Snapshot- oder Command-Aufruf einen
konsistenten aktuellen Owner-Kontext aus:

- aktuellem Run-, Recovery-, Lifecycle- und Persistenzzustand;
- vorhandenen Programm-, Konfigurations- und erwarteten Revisionen;
- den vorhandenen Sensorwert-/Qualitaets-/Rolleninformationen, sofern eine
  kanonische Producer-Quelle sie geliefert hat;
- dem bestehenden #21-Plausibilitaetskontext;
- der bestehenden #24-Safety-/Interlock-Auswertung fuer die konkrete Aktion.

Fehlt ein Producer, bleibt der Wert im vorhandenen UI-Vertrag optional bzw.
als bestehende ungueltige/stale Qualitaet sichtbar. Es wird kein Nullwert als
Temperatur, keine Default-Safety-Freigabe und keine scheinbare Web-
Sicherheitslage erzeugt. Sensorabhaengige Start-, Aenderungs-, Kuehl- und
Recoveryentscheidungen bleiben ohne ausreichende Evidence fail-closed. Nicht
sensorabhaengige sichere Stop-/Fehlerpfade behalten ihre bestehenden
Semantiken; es wird keine pauschale neue Ablehnung eingefuehrt.

### Snapshot und Commands

Die Application stellt eine ownerseitige Snapshot-Methode
`FermentationApplication::uiSnapshot()` (oder nur dann einen gleichwertigen
bereits vorhandenen Namen, wenn die Bestandspruefung dies zwingend nahelegt)
bereit. Sie erzeugt keinen zweiten Snapshotvertrag, sondern fuellt den
bestehenden `FermentationUiProjectionInput` und ruft den vorhandenen reinen
`FermentationUiProjector` auf.

Die bestehenden `prepare*`-Pfade werden so angepasst, dass externe Aufrufer
nicht mehr `FermentationApplicationOwningEvidence` injizieren koennen. Sie
uebergeben ausschliesslich den typisierten UI-/Command-Intent, benoetigte
fachliche Werte, Bestaetigungen und erwartete Revisionen. Die Application
loest daraus die aktuelle Evidence intern auf und delegiert die Mutation an
die bestehenden Run-/Persistenz-Owner. Command-Identitaet, erwartete
Revisionen, Exactly-once-/Persistenzsemantik und bestehende Recovery-
Ergebnisse bleiben unveraendert.

Damit kann ein Renderer oder Web-Adapter keine Safety-, Sensor-, Planner-
oder Recovery-Evidence mehr als Eingabe der Application erzeugen. Er liefert
nur Absicht, Nutzdaten, Bestaetigung und Revisionserwartung.

### Zukuenftiger #30-Handoff

Wenn #30 spaeter reale DS18B20-Werte liefert, werden seine bereits
kanonisierten `TemperatureReading`-/Qualitaetsdaten in denselben
Application-owned Runtime-Handoff eingespeist. #30 wird dadurch nicht zum
UI- oder Safety-Owner und erzeugt keinen zweiten Sensorstatus. In diesem
Scope wird nur der softwareseitige Vertrag fuer diesen Handoff und eine
native Testquelle festgelegt; DS18B20-Adapter, GPIO, ROM, CRC, Bus- und
Hot-Plug-Arbeit bleiben ausgeschlossen.

## Erlaubter Implementierungsumfang

Nur die kleinste direkt betroffene Application-/Composition-/Projection-
Aenderung ist zulaessig:

1. Application-interne Runtime-Evidence-Zusammenfuehrung in
   `lib/fermentation_app`, vorzugsweise in den bestehenden
   `fermentation_application.{hpp,cpp}`-Dateien.
2. Ownerseitige `uiSnapshot()`-Projektion gegen den bestehenden
   `FermentationUiProjector`; keine neue UI-State-Machine und kein paralleles
   View-Modell.
3. Anpassung der bestehenden Application-Command-Grenze, damit externe
   Aufrufer keine `FermentationApplicationOwningEvidence` mehr liefern.
4. Nur direkt betroffene native Tests fuer Snapshot, Evidence-Aufloesung,
   Fail-closed-Verhalten, Revisionen und Command-Aufrufe.

Ein neues kleines application-internes Struct ist nur zulaessig, wenn die
bestehenden Typen die Komposition nicht ausdruecken koennen. Es darf keine
allgemeine Runtime-Provider-, Eventbus-, Service-Locator- oder zweite
State-Machine-Abstraktion entstehen. Aenderungen an `device_platform` sind
nur zulaessig, wenn ein bereits bestehender abstrakter Temperatur-/Qualitaets-
Vertrag konkret in den Application-Handoff eingebunden werden muss; neue
fermentation-spezifische Ports gehoeren nicht dorthin.

## Nicht im Scope

- #27 Web/API/Auth, HTTP, Sessions, Auth, Webassets oder KDF;
- #30 DS18B20-Hardware, Adapter, GPIO, ROM-/CRC-/Hot-Plug- und reale
  Sensorpruefungen;
- #31 Display, Touch, Renderer, Bibliothek und Kalibrierung;
- #28 Diagnose, Service, Exporte, historische Laufdaten oder Diagramme;
- #35 PI-, Luft-, Aktor- und Safety-Parameter sowie Commissioning-Werte;
- #106 produktive Per-Run-Bindung oder Aktoraktivierung;
- neue Safety-Logik, neue Sensorqualitaets-/Rollenklassifikation oder neue
  Recovery-Policy;
- zweite Persistenz-, Connectivity-, Auth-, Sensor- oder UI-Wahrheit;
- neue generische Provider-/Event-/Service-Locator-Infrastruktur;
- ESP-IDF-, HTTP-, Hardware-, Aktor-, Renderer- oder Bibliotheksarbeit;
- erfundene Produktionswerte, Default-Temperaturen oder simulierte
  Hardware-/Client-Evidence.

`ACTUATOR_RELEASE=NO` bleibt waehrend des gesamten Scopes unveraendert.

## Gezielte native Regressionen

Die Tests muessen die bestehende Semantik pruefen, nicht einen neuen
Parallelvertrag einfuehren:

1. `UNAVAILABLE`/fehlende Producerquelle wird im Snapshot sichtbar; es gibt
   keinen impliziten Nullwert, keine Persistenzmutation und keine Sensor-
   Safety-Freigabe.
2. Eine native kanonische Testquelle kann einen
   `TemperatureReading`-/Qualitaetshandoff speisen; Snapshot, Rollen- und
   Plausibilitaetskontext verwenden danach exakt diese Quelle.
3. Valid, stale und failed Sensorqualitaet werden entsprechend den
   bestehenden #20/#21-Vertraegen behandelt; fehlende oder widerspruechliche
   Evidence bleibt fail-closed.
4. Start-, Cooling-, Aenderungs-, Completion-, Stop- und Recovery-Kommandos
   loesen die aktuelle Evidence in der Application auf. Kein oeffentlicher
   Command-/UI-Aufruf kann `FermentationApplicationOwningEvidence` einsetzen.
5. Bestehende erwartete Revisionen, Run-Identity, Persistenz- und
   Exactly-once-Semantik bleiben erhalten; stale Commands werden wie bisher
   abgelehnt.
6. Snapshot-Projektion enthaelt aktuelle Application-/Service-/Network-
   Quellen und Refresh-/Revisionswerte ohne doppelte Projection-Logik.
7. Ein nativer #30-Handoff-Test prueft nur die kuenftige abstrakte
   Einspeisegrenze; kein DS18B20-Code und keine Hardwareannahme.
8. Direkt betroffene native Tests sowie
   `scripts/check_architecture_boundaries.py` und `git diff --check` bleiben
   PASS. ESP-IDF-, Hardware- und Web-CI-Laeufe gehoeren nicht in die
   Planphase.

## Umsetzung und Nachweis

Die spaetere Umsetzung kann in kleinen Builder-Commits erfolgen:

1. Application-owned Evidence-Komposition und `uiSnapshot()` gegen die
   bestehenden Projektionstypen;
2. Anpassung der Command-Grenze und direkte native Regressionen;
3. gezielte Konsistenz-/Architekturpruefungen und Handover-Evidence.

Jeder Commit bleibt innerhalb dieses Plans. Bei einer notwendigen materiellen
Abweichung an Architektur, Persistenz, Wireformat, Recovery, Safety,
Abhaengigkeiten oder Teststrategie ist vor weiterer Umsetzung eine neue
Planrevision erforderlich.

Akzeptiert ist der Scope erst, wenn die Application alleinige Quelle der
aktuellen Runtime-Evidence ist, Renderer/Touch/Web keine solche Evidence mehr
einspeisen koennen, fehlende Producer sichtbar unavailable bleiben, die
bestehenden UI-/Command-/Safety-Vertraege nicht dupliziert werden und #30
spaeter denselben Handoff verwenden kann.

## Reihenfolge und Owner-Gates

1. Issue #168 wird auf aktuellem `main` gefuehrt; dieser Draft-PR enthaelt
   nur Plan-/Roadmap-Artefakte.
2. Der Owner muss die exakte Plan-Commit-SHA freigeben.
3. Erst danach darf Issue #168 implementiert werden; bis dahin gilt
   `IMPLEMENTATION_AUTHORIZATION=NO`.
4. Nach Merge von #168 wird PR #167 auf den neuen kanonischen `main` gebracht.
   Der #27-Plan wird dann als Verbraucher dieses Application-Scopes revalidiert
   und bei Bedarf als neue Planrevision freigegeben. Die #27-Implementierung
   wird bis dahin nicht fortgesetzt.
5. Danach gelten fuer #27 wieder dessen eigene unabhängige Review-,
   Pre-Ready-, CI- und Merge-Gates.

## Offene Entscheidungen vor Umsetzung

- Die Implementierung muss zuerst bestaetigen, dass
  `FermentationApplication::uiSnapshot()` und die bestehende interne
  Application-Komposition ohne neue allgemeine Provider-/Event-Infrastruktur
  ausreichen.
- Falls der aktuelle Quellstand fuer eine sichere Application-owned
  Sensor-/Safety-Komposition einen materiell neuen Producer, eine neue
  Persistenzwahrheit oder eine andere Architekturgrenze benoetigt, ist hier
  anzuhalten und eine Planrevision vorzulegen. Es darf kein Fallback-Owner,
  kein Web-/Renderer-Owner und keine erfundene Produktionsquelle eingefuehrt
  werden.

```text
OWNER_PLAN_APPROVAL_REQUIRED=YES
IMPLEMENTATION_AUTHORIZATION=NO
ACTUATOR_RELEASE=NO
```
