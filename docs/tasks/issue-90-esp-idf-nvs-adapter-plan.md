# Issue #90 – ESP-IDF-NVS-Adapter für `IStateStore`

## Status, Basis und Freigabegrenze

Der einzig gültige aktuelle Status dieses Plans und der bereits im PR
vorhandenen #90-Implementation ist bis zur Ownerfreigabe der exakten neuen
Plan-Commit-SHA:

`IMPLEMENTATION_BLOCKED_PENDING_R5_1_PLAN_APPROVAL`

### Revision R5.1 – vollständige kanonische Planrevision

R5.1 ersetzt R5 vollständig. R5 und R4 sind nur noch historische Provenienz
und dürfen weder patchweise ergänzt noch mit dieser Revision zusammengesetzt
werden. Die frühere freigegebene Plan-SHA war
`da693e8a24735ff2cc09f019b119083f3792882e`; der nicht freigegebene
R5-Plan-Head war `1e8cbfdf364199064683d52252ca0997a7217080`. Keine dieser
SHAs ist die neue R5.1-SHA.

Die R5.1-SHA wird im Plantext absichtlich nicht vorweggenommen. Nach dem
Commit werden ausschließlich die exakte neue SHA, der Planpfad und die
Freigabelogik im PR-Body und im aktuellen `SESSION HANDOVER` veröffentlicht.

Verifizierte Kontextbaseline dieser Revision:

- aktueller PR-#117-Head vor dieser Revision:
  `1e8cbfdf364199064683d52252ca0997a7217080`;
- Base-Branch: `agent/issue-29-esp32-bringup-plan`;
- Base-SHA: `30fa0a8264e2c4564d324340c6bebc204147f477`;
- Merge-Commit von #116 nach #117:
  `17100cc87427d7bcbeb25a8f53920abe686869d6`;
- build-relevanter synchronisierter Source-Commit:
  `17100cc87427d7bcbeb25a8f53920abe686869d6`;
- ursprünglicher #117-Implementierungs-/Build-Source-Commit:
  `c506ae616c528d78b2349209b99997dbb738f0a1`;
- gepinntes ESP-IDF: `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`.

Diese Runde ändert ausschließlich den versionierten Plan, die minimale
Roadmap-Synchronisierung und die externen Status-/Handovertexte. Produktions-
code, Testcode, Testorakel, Harness, Runner, UART, Partition,
Buildkonfiguration und Hardwarezustand bleiben unverändert. Vor einer
exakten Ownerfreigabe der R5.1-SHA ist jede Umsetzung weiterhin gesperrt.

## Erhaltene Callback-12-Evidenz: kein neuer Diagnoseauftrag

Auf dem synchronisierten PR-Stand `17100cc` besteht der vollständige
`ISSUE90_HOST_MODE=exhaustive`-Lauf weiterhin nicht:

```text
PASS binary-empty
PASS invalid-configuration
PASS bounded-read
PASS blob-boundaries
PASS maximum-record
PASS error-mapping
FAIL old-or-new-bdl-cut: old-or-new read status=1 callback=12
```

`StateStoreReadStatus::1` ist im bestehenden Port `NotFound`, nicht ein
zulässiger Old-or-New-Erfolg. Der Fehler ist damit präziser als
`old-or-new-bdl-cut / callback 12 / NotFound` zu führen; ein `ReadError` wäre
ebenso ein Fehler, falls er auf demselben Pfad auftritt.

Der aufgezeichnete mutierende Trace des failing Cuts endet bei:

```text
callback 12: Write address=0x2000 length=4
```

Das ist im gepinnten ESP-IDF-v6.0.2-NVS-Pfad der Page-State-Write aus
`Page::markFull()` beziehungsweise `Page::alterPageState()`. Davor wurden
bereits neue `BLOB_DATA`-Bytes und die zugehörigen 4-Byte-Entry-State-/Range-
Writes persistiert; der neue `BLOB_IDX` ist noch nicht persistiert. Das
freigegebene BDL-Double friert beim ausgewählten Callback die persistente
Byteabbildung ein und lässt nachfolgende Operationen fehlschlagen. Nach
Deinit/Reinit bleibt die Seite deshalb in einem aktiven Recovery-Zustand mit
neuen orphaned Blob-Daten und altem Index. Die NVS-Recovery behandelt den
neuen Eintrag als Schlüsselduplikat, entfernt dabei den alten Index und der
anschließende Adapter-Read liefert `NotFound` statt des vollständigen alten
oder neuen Werts.

Damit ist die Ursache die fehlende Old-or-New-Sichtbarkeit des gepinnten
ESP-IDF-NVS-BLOB-Recoverypfads bei einem internen 4-Byte-Page-State-Cut. Der
Adapter selbst kann diese Reihenfolge nicht steuern: `nvs_set_blob()` besitzt
die internen Daten-, Entry-State-, Page-State- und Index-Schritte. Der Test
prüft weiterhin den unveränderten `IStateStore`-Vertrag; das Orakel wird nicht
gelockert. Die vollständige Callbackauswahl wird ebenfalls nicht wieder auf
lange Writes verkürzt, weil R5.1 die Anforderung für alle
mutierenden `Write`-/`Erase`-Callbacks erhält.

Die Diagnose wurde zusätzlich gegen die alternative Harness-Reihenfolge
geprüft, in der der ausgewählte Block noch mutiert und erst danach der
Fehlerzustand einsetzt. Dadurch verschiebt sich der Fehler auf einen früheren
4-Byte-Entry-State-Cut; ein vollständiger Lauf wird damit nicht grün. Diese
Variante ist daher keine belastbare Korrektur, sondern bestätigt, dass die
Sichtbarkeitslücke nicht durch eine einzelne Erwartung oder Callbacknummer
behoben wird.

## Espressif-first-Abgleich und konkrete R5.1-Architekturentscheidung

### Herstellervertrag und Bedeutung des Befunds

Die offizielle ESP-IDF-v6.0.2-Dokumentation beschreibt NVS als gegen
unerwarteten Stromverlust ausgelegt: Ausschalten zu jedem Zeitpunkt soll nach
dem nächsten Start keinen Verlust bestehender Daten verursachen; verloren
gehen darf nur das gerade neu geschriebene Key-Value-Paar. Die NVS-API
beschreibt außerdem atomare Updates je Key-Value-Paar. Diese Aussagen sind
der zu prüfende Herstellervertrag, nicht nur eine Testannahme.

Der reproduzierte BDL-Cut ist zunächst eine plausible Power-Loss-Semantik:
Der BDL persistiert jeden abgeschlossenen Write, der konfigurierte Cut stoppt
den nächsten Write, und Deinit/Reinit entspricht dem Neustart. Damit darf der
Fall nicht als künstlich „nicht real“ wegklassifiziert werden. Gleichzeitig
ist ein BDL-Callback-Cut noch kein realer Versorgungstest; die spätere
Hardwareprüfung bleibt separat und benötigt ein kontrollierbares Fixture.

Die gepinnte v6.0.2-Quelle wird konkret gegen diese Aussage gestellt:

- `Storage::writeMultiPageBlob()` schreibt `BLOB_DATA`, Page-/Entry-State und
  anschließend `BLOB_IDX` in mehreren getrennten Raw-Writes;
- `Page::markFull()`/`alterPageState()` führt den beobachteten 4-Byte-Write bei
  `0x2000` aus;
- `nvs_pagemanager.cpp` enthält die Page-Recovery-/GC-Reihenfolge;
- der offizielle v6.0.2-BDL-Hosttest aktiviert
  `CONFIG_NVS_BDL_STACK=y`, verwendet die Espressif-BDL-Anforderungen und
  enthält bereits ein Power-Off-/Old-or-New-Orakel, aber nicht diese
  vollständige #90-BDL-Cut-Matrix.

Der Abgleich entscheidet ausdrücklich zwischen Vertragsbefund und
Fehlerbefund:

1. Der Cut ist vom NVS-Vertrag gedeckt und ein gepinnter oder minimaler
   Espressif-Fix stellt für alle Cuts wieder vollständiges Alt-oder-Neu her.
2. Der Cut wird vom Herstellervertrag nicht gedeckt oder der Fehler bleibt im
   direkten Pfad trotz Herstellerkorrektur bestehen. Dann ist der direkte
   `IStateStore`-Vertrag mit v6.0.2 nicht belegbar und Issue #90 wird für die
   Umsetzung `BLOCKED`; es wird keine halbfertige Eigenarchitektur begonnen.

### Schritt 1A – Herstelleranalyse und Evidenz (durch Planfreigabe erlaubt)

Der erste Umsetzungsschritt nach Freigabe dieses Plans ist ausschließlich
Analyse und Evidenz. Er darf keine Projektabhängigkeit, keinen ESP-IDF-Pin
und keinen Produktions-, Harness-, Orakel-, UART-, Runner- oder Hardwarepfad
ändern:

1. Die exakt gepinnte ESP-IDF-v6.0.2-Quelle, die relevante `nvs_flash`-
   Kconfig, `nvs_set_blob()`, `nvs_commit()`, `Page::markFull()`,
   `PageManager::requestNewPage()`, Recovery und die vorhandenen BDL-Tests
   werden auf `7101770dc6db2667b3c477cc31365dd1acd6db4e` analysiert und mit
   dem #90-Trace verknüpft.
2. Der kleinste reproduzierbare Callback-/Power-Loss-Fall wird als
   upstream-vergleichbarer Testfall gesichert: alter Blob, neuer Blob,
   vollständiger mutierender Write-/Erase-Cut, Reinit und vollständiges
   Readback. Der Fall enthält Callback 12 sowie die alternativen 4-Byte-Cuts.
3. Offizielle Espressif-Releases, Branchstände und Fixes werden als Evidenz
   evaluiert. `f0c7d9b6c603658c832858d0a4f25b5a05ea1760`
   (`fix(nvs_flash): Fixed order of page state change to allow recovery`)
   ist dabei ausdrücklich nur die historische Provenienz bzw. eine bereits
   äquivalente Herstelleränderung in der gepinnten v6.0.2-Codebasis:
   `nvs_pagemanager.cpp::requestNewPage()` enthält dort bereits die
   relevante Reihenfolge `erasedPage->markFreeing()`, danach
   `activatePage()`, danach `copyItems()`, einschließlich des
   Recovery-Kommentars. Der Callback-12-/`NotFound`-Befund tritt also
   bereits mit dieser Herstellerkorrektur auf. `f0c7d9b6...` ist deshalb
   kein offener Kandidat, dessen Test allein den aktuellen Befund lösen
   könnte. Aus Commitdatum oder Branchlage wird keine lineare
   "neuere Version"-Beziehung abgeleitet.
4. Der nächste Herstellerabgleich sucht ausschließlich nach einer weiteren
   offiziellen Espressif-Korrektur, einem relevanten Release-/Branchstand
   oder einem belegten anderen Recovery-Fix. Gegen jeden Vergleichsstand
   wird dieselbe vollständige #90-BDL-Cut-Matrix ausgeführt: alle
   mutierenden `Write`-/`Erase`-Callbacks einschließlich 4-Byte-Cuts,
   `BLOB_DATA`, `BLOB_IDX`, Altwertentfernung, GC-/PageManager-Copy und
   Page-Erase, ergänzt um den Commit-Kontrollpunkt.
5. Exakte Source-/IDF-SHAs, Trace, Orakel, Testausgabe und Kompatibilitäts-
   auswirkung werden dokumentiert. Schritt 1A übernimmt keinen Vendor-Patch
   und ändert keine Projektabhängigkeit.

### Owner-Subgate für den Herstellerpfad

Wenn Schritt 1A einen konkreten Herstellerfix, Releasewechsel oder Backport
als belastbare Lösung nachweist, wird vor jeder Abhängigkeit- oder Patch-
änderung die gesonderte Ownerfreigabe genau dieses Entscheids eingeholt. Die
Vorlage nennt mindestens:

- exakten Zielstand beziehungsweise Fix-SHA;
- vollständiges Ergebnis der identischen BDL-Cut-Matrix;
- On-Flash-/Recovery- und Kompatibilitätsauswirkung;
- Source-/IDF-Provenienz und den unveränderten Old-or-New-Vertrag.

### Schritt 1B – Änderung erst nach Owner-Subgate

Erst nach dieser exakten Ownerfreigabe darf der ESP-IDF-Pin bzw. die
Dependency auf den freigegebenen Zielstand geändert oder der exakt
freigegebene Vendor-/Backport-Fix übernommen werden. Danach werden alle
R5.1-Gates auf dem neuen exakten Source-/IDF-Stand wiederholt, einschließlich
der vollständigen BDL-Cut-Matrix, der übrigen Host-/Adapter-/Parser-/
Runner-Gates, der Profile, Provenienz, Artefakte und späteren Hardwaregrenzen.

Wenn Schritt 1A keinen belastbaren direkten Herstellerpfad ergibt oder der
direkte Pfad auch mit dem belegten weiteren Herstellerstand den unveränderten
Old-or-New-Vertrag nicht vollständig erfüllt, lautet die Empfehlung
`BLOCKED`; die Umsetzung hält an. Es wird kein Adapter-Workaround begonnen.

### R5.1-Empfehlung: direkter NVS-Pfad mit Espressif-Korrektur, sonst BLOCKED

R5.1 empfiehlt konkret, `NvsStateStore` als direkten, anwendungsneutralen
ESP-IDF-NVS-Adapter beizubehalten und die Atomizitätskorrektur ausschließlich
im Herstellerpfad zu lösen: eine ownerfreigegebene, exakt versionierte
ESP-IDF-Korrektur beziehungsweise ein gleichwertiger offizieller Fix ist die
kleinste zulässige Richtung. Die bestehende direkte Key-/Blob-Abbildung, der
`IStateStore`-Port und der Schema-1-Wirevertrag bleiben dabei unverändert.

R5.1 plant ausdrücklich **keine** zusätzliche Transaktions-, Generation-,
Index-, Selector- oder eigene Flash-/GC-Schicht. Der Grund ist nicht nur KISS:
ADR-016 weist Atomizität, Integrität, Wear-Leveling und Recovery bewusst dem
Herstellerbackend zu. Eine zweite Persistenzarchitektur würde den Scope,
Kapazitäts-/Wear-Vertrag und die Recoveryverantwortung materiell erweitern.

Wenn der Herstellerabgleich den direkten Pfad nicht belastbar repariert,
empfiehlt R5.1 statt einer nachträglichen Architekturwahl den Status
`BLOCKED`: Issue #90 darf dann erst nach einem eigenen Ownerauftrag oder einer
neuen vollständigen Planrevision mit neuem Scope weitergehen. Diese
Blockadeempfehlung ist die festgelegte R5.1-Ausweichgrenze und keine offene Wahl
für die Implementierungsphase.

### Direkter Backendvertrag und Ebenengrenzen

Der empfohlene Pfad bleibt vollständig bestimmt:

| Logischer Vertrag | Physische v6.0.2-Abbildung |
|---|---|
| `IStateStore::StateStoreKey` | exakt derselbe String als NVS-Key; 1–15 Bytes aus `[A-Za-z0-9_.-]`, keine Hash-/Trunkierung |
| Store-Namespace | `fermentation`, als ein NVS-Namespace-Entry |
| Partition | Label `state_store`, Typ/Subtyp `data,nvs`, 69 Seiten / 276 KiB im R1-Kandidatenlayout |
| Wert | exakt ein binärsicheres NVS-BLOB je logischem Key; kein Envelope-/Wireformat-Umbau durch den Adapter |
| Commitpunkt | `nvs_set_blob()` plus `nvs_commit()`; der Herstellerpfad bestimmt die interne Write-/GC-Reihenfolge |
| Kollisionen | ausgeschlossen durch die getrennte portseitige `StateStoreKey`-Validierung und direkte 1:1-Abbildung; keine zusammengesetzten Namen |

Die `StateStoreKey`-Länge wird nicht durch Namespacepräfixe,
Partitionsnamen oder Generationen verbraucht. Alle gültigen Schlüssel können
direkt und verlustfrei gespeichert werden. Vorhandene NVS-Daten mit dieser
Abbildung bleiben bei einem reinen Herstellerfix kompatibel. Ändert ein
Upgrade das On-Flash-Format oder die Recoverykompatibilität, muss das vor
Ownerfreigabe des Upgradepfads separat bewiesen werden; automatisches Löschen,
Formatieren oder Migration wird nicht erfunden.

Für jeden möglichen Cut gilt im direkten Vertrag: ein vollständiger alter oder
neuer Blob ist zulässig, bei vorher nicht vorhandenem Key zusätzlich
`NotFound`; ein Teilwert, Mischwert, beschädigter Blob oder `NotFound` nach
vorher vorhandenem Wert ist FAIL. `CommitOutcomeUnknown` bleibt Unknown und
wird durch exakten Readback aufgelöst; `NotFound` wird nie als Erfolg oder als
Umbenennung in `ReadError` akzeptiert. `ReadError` nach der zweiten
`nvs_get_blob()`-Abfrage bleibt ebenfalls ein echter Fehler.

Die Aussage "keine eigene A/B-/Generations-/Selector-Schicht" gilt
ausschließlich für eine zusätzliche Persistenzschicht innerhalb von
`device_platform_esp_idf::NvsStateStore`. Sie verbietet keine bereits
kanonischen höheren Verträge:

A. **Bestehende Konfigurationsgenerationen bleiben verbindlich.** Dokument-
revisionen, `ActiveConfigurationManifest`, `RootRecord`, die aktive
Konfigurationsgeneration, genau eine vorherige Rückfallgeneration,
`StorageEpoch`, Copy-Migrationen und die Aktivierung erst durch den neuen
Root bleiben unverändert. Diese Logik liegt oberhalb von `IStateStore` /
`NvsStateStore` und wird weder entfernt noch in den Adapter verschoben.

B. **Bestehende Laufpersistenz-Redundanz bleibt verbindlich.** Die vorhandenen
Run-Slots/-Heads und ihre Verträge, insbesondere `rc0`, `rc1` und `rh0`,
bleiben unverändert. Diese Redundanz liegt oberhalb des generischen NVS-
Adapters und ist keine vom Adapter erfundene A/B-Schicht.

C. **Zukünftiges Firmware-OTA-A/B bleibt ausdrücklich möglich.** Der
Espressif-first-Weg kann später mindestens zwei OTA-App-Slots
(`ota_0`, `ota_1`), `otadata`, den ESP-IDF-Bootloader-/OTA-Mechanismus
sowie geeigneten Rollback mit `PENDING_VERIFY` und Validierung verwenden.
Konkrete OTA-Partitionierung und Flashbudget sind nicht Scope von Issue #90
und werden durch diesen Plan nicht vorweggenommen. #90 darf aber keine
unnötige Architekturentscheidung enthalten, die einen späteren Espressif-
OTA-Weg ohne eigenen Ownerentscheid faktisch verunmöglicht.

D. **Exakt ausgeschlossen ist nur die zusätzliche Adapter-Schicht.** Nicht
zulässig als spontane #90-Lösung ist:

```text
IStateStore
    -> NvsStateStore
        -> eigene Adapter-A/B-/Generation-/Selector-Persistenz
            -> ESP-IDF NVS
```

Zulässig und gewollt bleibt:

```text
Fach-/Persistenzverträge
    -> Konfigurationsgeneration / Fallback / Run-Slots
        -> IStateStore
            -> NvsStateStore
                -> ESP-IDF NVS
```

Sowie separat auf Firmwareebene:

```text
ESP-IDF Bootloader / OTA
    -> ota_0 / ota_1 / otadata / Rollback
```

Garbage Collection und Erase bleiben ausschließlich der ESP-IDF-NVS-
Implementierung und dem dafür geplanten Orakel. ADR-016, `DECISIONS.md`,
die direkte Key-Mapping-Aussage, `CONFIGURATION_PERSISTENCE.md`,
`RUN_PERSISTENCE.md` und das Schema-1-Wireformat erhalten keine zweite
fachliche Persistenzwahrheit. Kapazitätsmehrbedarf und zusätzliche Wearlast
durch eine vom Adapter eingeführte A/B-/Generationsschicht entstehen nicht;
die bestehende 69-Seiten-Annahme wird gegen 4 MB und die vorhandene
NVS-Write-/GC-Last geprüft.

## R5.1-Owner-Gate vor Umsetzung

Der Owner muss nach Veröffentlichung der R5.1-SHA zwei Dinge ausdrücklich
bestätigen: den unveränderten direkten NVS-/Old-or-New-Pfad und, sofern
Schritt 1A einen belastbaren Herstellerfix ergibt, den exakten Dependency-/
Patchentscheid im separaten Owner-Subgate. Eine allgemeine Zustimmung ohne die
neue exakte R5.1-SHA oder ohne diesen konkreten Herstellerentscheid
autorisiert nichts.

Das Readback-Orakel, alle 4-Byte-Cuts, `NotFound`-Semantik und der
Schema-1-Wirevertrag bleiben unverändert. Nach R5.1-Freigabe gilt ausschließlich
die geteilte 1A-/Subgate-/1B-Reihenfolge aus dem Abschnitt
„Umsetzungs- und Commit-Schnitte“. Der vorhandene PR-Code und die offenen
R6-Befunde bleiben bis dahin unverändert.

## R5.1-Owner-Gate vor Umsetzung

Der Owner muss nach Veröffentlichung der R5.1-SHA nur noch die festgelegte
Herstellerpfadrichtung und deren Freigabevoraussetzung bestätigen: direkter
NVS-Pfad mit exakt versionierter Espressif-Korrektur, andernfalls die
ausdrückliche `BLOCKED`-Empfehlung. Eine Freigabe der alten R4-SHA oder eine
allgemeine Zustimmung ohne die neue exakte R5.1-SHA autorisiert nichts.

Das Readback-Orakel, alle 4-Byte-Cuts, `NotFound`-Semantik und der
Schema-1-Wirevertrag bleiben unverändert. Nach R5.1-Freigabe gilt die
verbindliche Reihenfolge aus dem Abschnitt „Umsetzungs- und Commit-Schnitte“.
Der vorhandene PR-Code und die offenen R6-Befunde bleiben bis dahin
unverändert.

PR #117 enthält bereits die frühere `NvsStateStore`-Implementation, den
stateful BDL-Hosttest, die Kapazitätsprüfung, den On-Target-Harness sowie die
zugehörige Runner-/CI-/Berichtsintegration. Diese bestehende Implementation
enthält die bekannten Reviewbefunde weiterhin. Die neue Planfreigabe gibt
ausschließlich die in R5.1 beschriebenen Korrekturen frei; sie autorisiert
keinen Produktions-, Harness-, Oracle-, Runner- oder Hardwarepfad vor dem
Owner-Gate.

Bisherige Host-/Build-PASS des PR sind bis zur Umsetzung und Prüfung dieser
Korrekturen kein aktueller Abschlussnachweis. Vor der neuen Planfreigabe werden
keine Firmware-, Test-, CI-, Harness-, Bericht- oder sonstigen
Implementierungsänderungen vorgenommen.

Der Plan ist auf PR #116 gestapelt:

- Branch: `agent/issue-90-nvs-adapter-plan`
- Base-Branch: `agent/issue-29-esp32-bringup-plan`
- Base-SHA: `30fa0a8264e2c4564d324340c6bebc204147f477`
- Planpfad: `docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md`
- Abhängigkeit: `STACKED_ON_PR_116`; PR #116 bleibt Draft, Issue #29 bleibt
  offen.

PR #116 liefert die aktuelle Software-/Build-Basis. Board-, Flash-, UART-,
Reset-, Recovery- und Smoke-Nachweise sind auf dem tatsächlich getesteten
realen Board dokumentiert. Die sicheren unbelasteten MCU-/Gate-/Bootpegel
bleiben `NOT_RUN`; die physische PCB-Revision/Silkscreen ist nach
Ownerentscheidung kein Abnahmekriterium. Dieses separate #29-Pegel-Restgate
blockiert weder die #90-Softwarearbeit noch reale #90-Nachweise. Nicht
ausgeführte reale Standard-Flash-/Partitions-, Power-Cut-, Readback-,
Ressourcen- und Wear-Nachweise bleiben jeweils ehrlich `NOT_RUN` oder
`BLOCKED`.

## Scope und unveränderte Verträge

Issue #90 implementiert den konkreten ESP-IDF-Adapter für den bestehenden
`device_platform::IStateStore`-Port in `device_platform_esp_idf`. Es wird weder
ein neuer Persistenzport noch eine neue Fach- oder Persistenzarchitektur
eingeführt.

Unverändert bleiben:

- der direkte, verlustfreie `StateStoreKey`-zu-NVS-Schlüssel gemäß ADR-016;
- Namespace- und Partitionsabgrenzung als explizite Konfiguration des owning
  context; die konkrete R1-Konfiguration wird nicht zur universellen
  Adapterannahme;
- `StateStoreReadStatus`, `StateStoreWriteStatus` einschließlich
  `CommitOutcomeUnknown`;
- Envelope-, Schema-, Active-/Fallback-/Head-, Lauf- und Wire-Verträge;
- `IStateStore` als generischer Schlüssel-/Binärwert-Port;
- keine Fachlogikänderung, kein Ersatz des bestehenden Stores, keine CBOR-/
  LittleFS-Migration und keine Änderung des Schema-1-Wireformats.

Nicht Bestandteil sind NVS-/Flashverschlüsselung, eine `nvs_keys`-Partition
oder ein Schutzversprechen für gespeicherte Secrets. Das separate
Security-/Releasegate `EVALUATE_BEFORE_RELEASE` bleibt unverändert bestehen.

## Ownerreview-R6: Befunde, die R5.1 erhält oder ausdrücklich ersetzt

Seit dem damaligen Review-Head `0e6b9eb86751b8a3b01fd64630eea273580ede3b`
wurden keine #90-Produktions-, Harness- oder Runnerkorrekturen umgesetzt.
Die Synchronisierung nach #116 hat nur Basis-, Evidenz-, Plan- und
Statusinhalte verändert. Deshalb bleiben die folgenden Umsetzungsschnitte
offen und sind nach R5.1-Freigabe verbindlich:

| R6-Befund | R5.1-Behandlung |
|---|---|
| Read-Race: zweites `nvs_get_blob()` liefert `NOT_FOUND` | unverändert offen; nach erfolgreicher Größenabfrage ist dies `ReadError`, niemals `NotFound`/Erfolg; Race- und Größenänderungstests müssen den Status beweisen |
| exakte ESP-IDF-Grenzen von `NvsStateStoreConfig` | unverändert offen; Partitionslabel maximal 16 Zeichen (`NVS_PART_NAME_MAX_SIZE = 16`, Nullterminator nicht mitgezählt), Namespace maximal 15 Zeichen (`NVS_NS_NAME_MAX_SIZE = 16` einschließlich Nullterminator), leer/eingebettetes `NUL` fail-closed vor jedem NVS-Aufruf; keine zusätzliche Zeichenmengenrestriktion ohne ESP-IDF-Vertrag; portseitige `StateStoreKey`-Validierung bleibt getrennt |
| tatsächlich vollständige genehmigte Host-Exhaustive-Matrix | unverändert offen; jeder mutierende `Write`-/`Erase`-Callback einschließlich 4-Byte-Cuts, BLOB_DATA, BLOB_IDX, GC/Copy und Erase bleibt enthalten |
| reproduzierbare Fehler-/JSON-Artefakte | unverändert offen; Scenario, Seed, Pattern, Cut, Status, erwartetes Alt/Neu, Readback, SHA-256 und Provenienz werden maschinenlesbar archiviert |
| Host-Testpartition `state_store,data,nvs,0x9000,0x45000` | unverändert offen; exakt diese 69-Seiten-Testpartition wird mechanisch geprüft und nicht mit der Produktions-CSV verwechselt |
| span-aware Raw-Page-Parser | unverändert offen; Header-CRC nur für echte NVS-Metadaten-/Item-Header prüfen, `span` validieren, Continuation-/Payload-Entries überspringen/als Payload behandeln, Span-Zustand konsistent prüfen und überlappende/zu lange/out-of-range/widersprüchliche Spans fail-closed als korrupte Evidenz behandeln; `BLOB_DATA`, `BLOB_IDX`, `live`, `removed` und GC-/Copy-Bezüge nur aus gültigen Metadaten-Items ableiten; Parserfehler niemals als `GC_ERASE_DETECTED` oder Hardware-PASS werten |
| unmittelbare Baseline direkt vor dem geschnittenen Zielwrite | unverändert offen; vor jedem geschnittenen Zielwrite unmittelbare Alt-Länge/Alt-SHA und vollständigen erwarteten Zustand aller Nicht-Zielkeys erfassen; `ROTATE_BEGIN.old_sha256`/`old_len`, Nicht-Ziel-Readback und Window-/Phasenklassifikation müssen exakt diese Baseline verwenden; früher PREFILL-Snapshot ist bei `target_rotation > 0` kein Altzustand |
| UART-/Runner-Vertrag ohne stille Protokollabweichung | unverändert offen; eine versionierte Befehls-/Marker-/Statusmatrix wird gemeinsam in Harness und Runner umgesetzt; fehlende, unerwartete oder nicht parsebare Marker sind FAIL/NOT_RUN |
| reale Flashprüfung exakt gegen 4 MB | unverändert offen; Chip-ID/Flashgröße, Partitionstabelle, Offsets, Größen und Reserve werden gegen 4 MiB geprüft; ein Build allein genügt nicht |
| vollständiger Hardware-Artefaktvertrag | unverändert offen; Source-/Firmware-/Plan-/IDF-SHA, Profil, Partition, UART, Reset, Raw-Page, NVS-Stats, Readback, Ressourcen, Latenz, Wear und Abschlussstatus werden pfad-/secretbereinigt referenziert |
| belastbares GC-/Erase-Orakel | unverändert offen; Host-BDL-Callbacks und reale Raw-Page-/Readback-/Stats-Evidenz bleiben getrennte Nachweise; Statistikänderung allein ist kein Erase |
| Release-Isolation inklusive Release-ELF | unverändert offen; Bring-up-Quelle, Compile-Definition, Marker, Symbole und nur testseitige Dependencies dürfen im Release-ELF/Graph nicht erscheinen |
| belastbarer Harness-Scratch-/Stacknachweis | unverändert offen; statischer interner Scratch bleibt höchstens 16.240 B, Harness-Funktionsstack höchstens 400 B; automatische Kopien und PSRAM-Abhängigkeit sind unzulässig |
| Lizenz-/Evaluation-Dokumente | unverändert offen; Espressif-Quellen, BDL-Hosttestbestand, Apache-2.0-/Drittbestandteile und eine mögliche Upgrade-/Backportquelle werden vor Veröffentlichung vollständig referenziert |
| Roadmap-/Statussynchronität | unverändert offen; Plan, Roadmap, PR-Body, Issue #90 und genau ein aktueller Handover führen denselben Status `IMPLEMENTATION_BLOCKED_PENDING_R5_1_PLAN_APPROVAL` |

R5.1 ersetzt materiell nur die noch offene R4-Architekturwahl durch die
konkrete Empfehlung „direkter NVS-Pfad mit Espressif-Korrektur, sonst
`BLOCKED`“ sowie die falsche R4-SHA-/Statusprovenienz. Die ausdrückliche
Abhängigkeit „reale #90-Matrix erst nach offenem #29-Hardwaregate“ wird durch
den aktuellen Boardstatus ersetzt: das #29-Pegel-Restgate blockiert die
zulässigen aktorfreien NVS-Tests nicht. Ein USB-Anschluss allein erzeugt
jedoch kein Power-Cut-Fixture; DTR/RTS-Reset zählt nicht als Power-Loss.

## Repository- und Fachquellen

Die folgenden Quellen sind vor Umsetzung erneut auf dem freigegebenen Base-/PR-
Stand zu prüfen. Die Links zu Repositorydateien sind zugleich die Rückverfol-
gung für die Kapazitätsinventur.

| Quelle | Verbindliche Aussage für #90 |
|---|---|
| [`state_store.hpp`](../../lib/device_platform/src/state_store.hpp) | getrennte Read-/Write-Enums, `maxBytes` nur im Read-Vertrag, alte/neue Atomizitätsgarantie und konservatives `CommitOutcomeUnknown` |
| [`state_store_key.hpp`](../../lib/device_platform/src/state_store_key.hpp) | gültige Schlüssel: 1–15 Bytes, `[A-Za-z0-9_.-]`; daher direkte verlustfreie NVS-Abbildung ohne Adapter-Hash |
| [`storage_envelope.hpp`](../../lib/device_platform/src/storage_envelope.hpp) | Envelope-Version, Header-/Payloadgrößen und technische Maximalrecordgröße |
| [`storage_slot_candidates.hpp`](../../lib/device_platform/src/storage_slot_candidates.hpp) | begrenzte Slotgruppen und Read-/Capacity-Propagation |
| [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) | exakter Konfigurationsschlüsselbestand und Recordtypen: `uc0..uc3`, `sc0..sc3`, `pc0..pc3`, `cm0..cm2`, `cr0..cr1`, `cb0..cb1` |
| [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) | User-/Programmpayload, Envelope-Maxima und Slotanzahlen |
| [`configuration_document_codec.cpp`](../../lib/fermentation_app/src/configuration_document_codec.cpp) und [`configuration_graph_store.cpp`](../../lib/fermentation_app/src/configuration_graph_store.cpp) | tatsächliche Konfigurations-Recordgrenzen und Aufrufer-`maxBytes` |
| [`run_persistence_contract.hpp`](../../lib/fermentation_app/src/run_persistence_contract.hpp) | 8.192-B-Run-Payload und zwei Checkpointslots |
| [`run_persistence_codec.cpp`](../../lib/fermentation_app/src/run_persistence_codec.cpp) und [`run_persistence_coordinator.cpp`](../../lib/fermentation_app/src/run_persistence_coordinator.cpp) | 8.240-B-Checkpointrecord, 256-B-Headrecord, `rc0`, `rc1`, `rh0` und Schreibreihenfolge |
| [`CONFIGURATION_PERSISTENCE.md`](../../docs/CONFIGURATION_PERSISTENCE.md) | generischer Store-, Recovery-, Readback- und Ressourcenvertrag |
| [`RUN_PERSISTENCE.md`](../../docs/RUN_PERSISTENCE.md) und [`RECOVERY_AND_INTERRUPTION.md`](../../docs/RECOVERY_AND_INTERRUPTION.md) | Lauf-/Unterbrechungs-/Recoverygrenzen; kein zweiter Adaptervertrag |
| [`ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md`](../../docs/ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md) und [`DECISIONS.md`](../../docs/DECISIONS.md) | ESP-IDF-NVS als produktives Backend, direkter begrenzter Schlüsselraum, keine Eigenimplementierung von Recordspeicher/GC |
| [`ADR-013_REUSABLE_DEVICE_PLATFORM.md`](../../docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md), [`ARCHITECTURE.md`](../../docs/ARCHITECTURE.md), lokale [`AGENTS.md`](../../lib/device_platform_esp_idf/AGENTS.md) | Produktionsadapter ausschließlich in `device_platform_esp_idf`; keine Fach- oder Composition-Root-Logik dort |
| [`HARDWARE.md`](../../docs/HARDWARE.md), [`OPEN_POINTS.md`](../../docs/OPEN_POINTS.md), [`RESOURCE_BUDGET_AND_MAINTENANCE.md`](../../docs/RESOURCE_BUDGET_AND_MAINTENANCE.md) | 4-MB-/kein-PSRAM-Basis, offene reale Flash-/Ressourcen-/Hardwaregates und Wear-/Schreiblastgate |
| [`ACCEPTANCE_TESTS.md`](../../docs/ACCEPTANCE_TESTS.md), [`ESP_IDF_UPGRADE_CONTRACT.md`](../../docs/ESP_IDF_UPGRADE_CONTRACT.md) | PASS/BLOCKED/NOT_RUN-Orakel, aktorfreie Hardwaregrenze und gepinnte ESP-IDF-Profile |
| [`CI_AND_QUALITY_GATES.md`](../../docs/CI_AND_QUALITY_GATES.md), [`.github/workflows/build.yml`](../../.github/workflows/build.yml) | zulässige Befehle, CI-Einbindung, Draft-/Owner-Gate und Artefakte |
| [`CMakeLists.txt`](../../CMakeLists.txt), [`main/CMakeLists.txt`](../../main/CMakeLists.txt), [`main/app_main.cpp`](../../main/app_main.cpp), [`sdkconfig.defaults`](../../sdkconfig.defaults) | aktueller ESP-IDF-Kompositions-/Build-Stand; aktuell kein produktiver `IStateStore`-Verbraucher und Single-App-Baseline |
| [`ISSUE_29_BUILD_REPORT.md`](../../docs/ISSUE_29_BUILD_REPORT.md) und Roadmap | Board/UART/Flash/Recovery/Smoke: real nachgewiesen/PASS; sichere unbelastete MCU-/Gate-/Bootpegel: `NOT_RUN`; PCB-Revision/Silkscreen: nach Ownerentscheidung kein Abnahmekriterium; das Pegel-Restgate blockiert sichere aktorfreie #90-NVS-Tests nicht |

Die Live-Anforderungen sind [Issue #90](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/90), die Hardwareabhängigkeit [Issue #29](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/29) und der aktuelle Stack [PR #116](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/116). Eine historische Planfassung ersetzt diese konsolidierte Fassung nicht.

## Anwendungsneutrale Adapterkonfiguration und konkreter R1-Vertrag

`device_platform_esp_idf::NvsStateStore` implementiert ausschließlich den
anwendungsneutralen NVS-Mechanismus. Partition und Namespace sind keine festen
Klassenkonstanten und kein Fermentationsvertrag, sondern werden vom owning
context explizit übergeben:

```text
Generischer Adapter:
device_platform_esp_idf::NvsStateStore
        ^
        |
owning context / Composition Root / #90-Harness
        |
        +-- partition = state_store
        +-- namespace = fermentation   # konkrete R1-Konfiguration
```

Die konkrete Adapterkonfiguration bleibt klein und lifetime-sicher: Eine
`NvsStateStoreConfig` (oder gleichwertige explizite Konstruktorparameter)
besitzt die Partitions- und Namespacewerte als eigene, nicht referenzierte
Strings. `NvsStateStore` erhält diese Konfiguration beim Konstruieren; es gibt
keinen Defaultkonstruktor und keine versteckten `state_store`-/`fermentation`-
Konstanten. Vor jedem NVS-Aufruf werden die Konfigurationswerte fail-closed
geprüft:

- Partitionslabel: maximal 16 Zeichen; `NVS_PART_NAME_MAX_SIZE = 16`, der
  Nullterminator wird nicht mitgezählt;
- Namespace: maximal 15 Zeichen; `NVS_NS_NAME_MAX_SIZE = 16` schließt den
  Nullterminator ein;
- leere Partitionslabel- oder Namespacewerte sind ungültig;
- eingebettete `NUL`-Bytes sind ungültig;
- für Partitionslabel und Namespace wird keine zusätzliche, selbst erfundene
  Zeichenmengenrestriktion eingeführt, sofern der gepinnte ESP-IDF-Vertrag sie
  nicht verlangt.

Bei jeder ungültigen Konfiguration führt der Adapter vor dem ersten NVS-Aufruf
keinen NVS-Aufruf aus, mutiert nichts und bildet den Fehler exakt auf bestehende
Portstatus ab:

- `write`: `StateStoreWriteStatus::WriteError`;
- `read`: `StateStoreReadStatus::ReadError`.

Die bestehende portseitige `StateStoreKey`-Validierung bleibt davon getrennt:
Sie definiert weiterhin den gültigen 1–15-Byte-Keyraum und seine Zeichen
unabhängig vom ESP-IDF-Partitionslabel-/Namespacevertrag. Es entsteht kein neuer
Status und keine zweite Konfigurationsarchitektur. Die geprüfte Konfiguration
wird anschließend für alle Operationen verwendet.

Für R1 bleiben die vom owning context gelieferten Werte verbindlich:

- Partitionslabel: `state_store`;
- Partitionstyp/Subtyp: `data,nvs`;
- Namespace: `fermentation`;
- IStateStore-Schlüssel: direkt und unverändert als NVS-Key;
- Defaultpartition `nvs`: nicht für IStateStore-Records.

Diese Konfigurationspräzisierung ändert weder `IStateStore` noch
`StateStoreKey`, den kanonischen Keybestand, Envelope-, Slot-, Wire- oder
Recoveryverträge. NVS bleibt das produktive Backend. Es entsteht weder ein
zweiter Persistenzport noch eine neue Persistenzarchitektur.

Solange kein produktiver `IStateStore`-Consumer verdrahtet ist, liefert der
#90-Bring-up-Harness diese R1-Konfiguration. Später übernimmt dies die
ESP-IDF-Composition-Root beziehungsweise die konkrete Hardware-/Anwendungs-
komposition. Init/Deinit verbleibt beim owning context und wandert nicht in
`NvsStateStore`. Eine Registry, Factory, DI-Plattform oder benutzerkonfigu-
rierbare Persistenzplattform wird nicht eingeführt. Aus der expliziten
Konfiguration werden keine ESP32-S3-, Multi-Board-, OTA-, PSRAM- oder
LittleFS-Erweiterungen abgeleitet.

Die Trennung verhindert, dass die Adapterkapazität mit anderen Konsumenten
vermischt wird. Der gepinnte ESP-IDF-WLAN-Vertrag aktiviert
`CONFIG_ESP_WIFI_NVS_ENABLED` standardmäßig und verwendet standardmäßig
`WIFI_STORAGE_FLASH`; dieser Verbraucher gehört zur separaten Defaultpartition
`nvs` und erhält kein stillschweigendes Budget aus `state_store`. Eine spätere
produktive WLAN-Verdrahtung muss `nvs` mit eigenem Kapazitäts- und Hardwaregate
initialisieren.

### Vorgesehener R1-Auswahlentscheid (wird mit exakter Planfreigabe verbindlich)

Mit der Ownerfreigabe der exakten neuen Plan-Commit-SHA soll für die #90-
R1-Konfiguration `state_store = 69 * 4096 = 282.624 B` (276 KiB) verbindlich
werden. Die Rechnung unten beweist eine Untergrenze von 49 Seiten; 69 Seiten
sollen der mit dieser exakten Planfreigabe verbindliche Auswahlwert sein und
enthalten 20 zusätzliche Seiten für Seitenpackung, Fragmentierung und die
begrenzten Update-/GC-Situationen. Die frühere Ownerfreigabe von
`da693e8...` hat diesen R1-Auswahlwert bestätigt; bis zur neuen R5.1-Freigabe
bleibt die Umsetzung dennoch im Status
`IMPLEMENTATION_BLOCKED_PENDING_R5_1_PLAN_APPROVAL`; weder die
Reviewkorrekturen noch die R1-Partitionsentscheidung gelten bereits als für die
weitere Umsetzung freigegeben.

Nach dieser Ownerfreigabe soll die Umsetzung dieser normalen
Partitionstabellenänderung ohne zweiten Planentscheid zulässig sein, wenn alle
folgenden Annahmen bestätigt sind:

1. #29 bestätigt den realen 4-MB-Flash und die ESP32-Standardpartitionierung;
2. die vollständige Inventarrechnung bleibt bei höchstens 69 Seiten und die
   deterministische Vorbefüllungs-/Rotationsprüfung erreicht die geforderten
   freien Seiten und den GC-Nachweis;
3. beide ESP-IDF-Profile bauen mit der in diesem Plan beschriebenen
   1-MB-Factory-App, dem `state_store`-Layout und einer positiven
   Flashreserve;
4. es werden keine NVS-Secrets, keine Verschlüsselung und keine zusätzlichen
   #90-fremden Verbraucher in `state_store` aufgenommen.

Eine abweichende reale Flashbasis, ein rechnerisches Ergebnis über 69 Seiten,
ein nicht passendes App-/Alignmentbudget oder ein zusätzlicher Verbraucher ist
eine materielle Abweichung: Umsetzung anhalten, neue Plan-SHA erzeugen und
erneute Ownerfreigabe einholen. Bei bestätigten Annahmen bleibt nach der
Ownerfreigabe kein pauschaler zweiter Planstopp vor der Tabellenänderung
bestehen.

## Lifecycle, Eigentum und Produktionsbindung

`NvsStateStore` führt weder `nvs_flash_init_partition()` noch
`nvs_flash_deinit_partition()` aus und besitzt keine allgemeine Lease-,
Referenz- oder Laufzeit-Shutdown-Infrastruktur.

Sobald ein tatsächlicher produktiver `IStateStore`-Verbraucher verdrahtet wird,
besitzt der jeweilige ESP-IDF Composition Root diesen einfachen R1-Ablauf und
liefert dem Adapter die geprüfte Konfiguration:

1. die R1-Konfiguration mit Partition `state_store` und Namespace
   `fermentation` erzeugen und fail-closed validieren;
2. `nvs_flash_init_partition(config.partitionLabel.c_str())` genau einmal vor
   der Store-Konstruktion aufrufen;
3. nur bei `ESP_OK` `NvsStateStore(config)` konstruieren;
4. danach den tatsächlichen Verbraucher konstruieren und ihm den Store
   übergeben;
5. bei jeder Abweichung von `ESP_OK` fail-closed starten: Fehler loggen,
   Store/Verbraucher nicht konstruieren, keine normale Anwendungsschleife und
   kein Aktorpfad;
6. beim kontrollierten Ende zuerst Verbraucher, dann `NvsStateStore`, zuletzt
   `nvs_flash_deinit_partition(config.partitionLabel.c_str())` zerstören.

`ESP_ERR_NVS_NO_FREE_PAGES`, `ESP_ERR_NVS_NEW_VERSION_FOUND`,
`ESP_ERR_NVS_NOT_FOUND`, `ESP_ERR_INVALID_ARG`, `ESP_ERR_NO_MEM`,
`ESP_ERR_NVS_INVALID_STATE`, `ESP_ERR_NVS_NOT_INITIALIZED` und unbekannte
Flashfehler werden nicht gelöscht, nicht formatiert, nicht automatisch
reinitialisiert und nicht blind wiederholt. Ein Deinitfehler wird diagnostisch
festgehalten und beendet die weitere Nutzung; er erzeugt keine Reparatur.

Der aktuelle [`main/app_main.cpp`](../../main/app_main.cpp) konstruiert keinen
produktiven `IStateStore`-Verbraucher. Deshalb ändert #90 diesen Composition
Root nicht stillschweigend. Die produktive Root-Integration ist ein ausdrücklich
bedingter Implementierungsschnitt am Ort des ersten realen Verbrauchers und
wird dort gemeinsam mit diesem Verbraucher getestet. Testseitige Init-/Deinit-
Orchestrierung liegt ausschließlich unter `test/`, nicht im Produktionsmodul.

Ein Handle wird pro `read`-/`write`-Operation geöffnet und vor der Rückkehr
geschlossen. Ein langlebiges Handle ist in R1 nicht erforderlich; es gibt keine
konkrete Nebenläufigkeits-, Transaktions- oder Recoveryanforderung dagegen.
Die Wahl ist die einfachste Besitz- und Ressourcenregel und behauptet nicht,
uncommittete Zustände zu verhindern. Der Store besitzt seine geprüfte
Konfiguration und die Operationslebensdauer; der Root besitzt Initialisierung,
Partition und übergeordnete Objekte.

## Produktionsadapter und Fehlervertrag

Vorgesehene Produktionsdateien:

- `lib/device_platform_esp_idf/src/nvs_state_store.hpp`;
- `lib/device_platform_esp_idf/src/nvs_state_store.cpp`;
- `lib/device_platform_esp_idf/CMakeLists.txt` mit `PRIV_REQUIRES nvs_flash`.

Der Adapter implementiert ausschließlich `device_platform::IStateStore`. Der
portseitige öffentliche Header und der Fachkern bleiben frei von ESP-IDF. Die
konkrete `NvsStateStore`-Klasse erhält eine explizite, eigene
`NvsStateStoreConfig`, exponiert keinen rohen Handle und keine Lifecycle-
Operation; ihr Handle bleibt ausschließlich innerhalb der
Operationsimplementierung. Alle `nvs_open_from_partition`-Aufrufe verwenden
die geprüften Instanzwerte. Eine Abhängigkeit auf `fermentation_app` entsteht
nicht.

`nvs_set_blob()` mutiert in der gepinnten Basis tatsächlich während des
Aufrufs: neue `BLOB_DATA`-Chunks, neuer `BLOB_IDX`, Entfernung des alten
Blob-Index und Entfernung alter Datenchunks. `nvs_commit()` ist in dieser
Version aktuell ein No-op: Bei gültigem Handle prüft `nvs_api.cpp` das Handle
und `NVSHandleSimple::commit()` liefert `ESP_OK`, ohne weitere Flashmutation.
Der Adapter ruft `nvs_commit()` dennoch auf, weil dieser Aufruf Teil des
Adapterablaufs ist; jeder nicht erfolgreiche Commit bleibt konservativ
`CommitOutcomeUnknown`.

### Vollständige Fehlerabbildung

`maxBytes` wird ausschließlich im Read-Pfad verwendet. Es ist kein
Schreibfehlerargument und darf in der Set-/Commit-Matrix nicht als Grund für
`WriteError`, `CapacityError` oder `CommitOutcomeUnknown` erscheinen.

| Phase / ESP-IDF-Aufruf | Ergebnisabbildung | Garantie / Begründung |
|---|---|---|
| Root: `nvs_flash_init_partition` | `ESP_OK` erlaubt Root-Fortsetzung; jeder andere Status ist Startup-Failure und fail-closed | Kein Store existiert; kein Löschen, Formatieren oder Retry |
| Read-open: `nvs_open_from_partition(..., NVS_READONLY, ...)` | `ESP_ERR_NVS_NOT_FOUND` → `NotFound`; jeder andere Fehler → `ReadError` | Vor `nvs_get_blob` keine Mutation; gültige explizite Partition-/Namespace-/Keywerte machen `INVALID_NAME` und `INVALID_ARG` im Normalpfad unmöglich |
| Write-open: `nvs_open_from_partition(..., NVS_READWRITE, ...)` | Jeder Fehler → `WriteError` | `nvs_set_blob` wurde nicht erreicht; alter Wert sicher unverändert. `INVALID_NAME`/`INVALID_ARG` sind bei validierten Konstanten unmöglich |
| Größenabfrage: `nvs_get_blob(handle, key, nullptr, &requiredBytes)` | `ESP_OK` → weiter; `NOT_FOUND` → `NotFound`; jeder andere Fehler → `ReadError` | `INVALID_LENGTH` ist bei nicht-nulligem `length` und nulligem Ausgabepuffer ein kontrolliert unmöglicher Normalpfad; ein injizierter oder unbekannter Fehler bleibt ReadError |
| lokale Read-Grenze nach Größenabfrage | `requiredBytes > maxBytes` → `CapacityError` | Keine Allokation und kein zweiter Read; der gespeicherte alte Wert bleibt unangetastet |
| zweiter Read: `nvs_get_blob(handle, key, buffer, &readBytes)` | `ESP_OK` nur bei exakt `readBytes == requiredBytes` → `Success`; `NOT_FOUND` nach erfolgreicher Größenabfrage → `ReadError`; `INVALID_LENGTH` mit beobachteter Größe `> maxBytes` → `CapacityError`, sonst `ReadError`; alles andere → `ReadError` | Keine Wiederholung und kein Teilwert. Eine Änderung zwischen beiden Abfragen wird nicht verschleiert |
| lokale Write-Vorprüfung | Länge oberhalb der aus gepinnten NVS-Konstanten abgeleiteten maximalen Blobgröße → `CapacityError` | `nvs_set_blob` wird nicht aufgerufen; alter Wert sicher unverändert. `maxBytes` ist hier unzulässig |
| Write-open erfolgreich, `nvs_set_blob` → `ESP_OK` | Danach `nvs_commit` aufrufen | Set hat bereits mutiert; erst der Commitstatus entscheidet den API-Status |
| `nvs_set_blob` → `ESP_ERR_NVS_VALUE_TOO_LONG` | `CapacityError`, nur entsprechend der gepinnten Vorprüfungs-/Chunkgrenze | Dieser Fehler liegt vor dem erfolgreichen neuen Blob-Index; die alte logische Version bleibt. Ein andersherum nicht eindeutig auflösbarer Längenfehler wird als Unknown behandelt |
| `nvs_set_blob` → `ESP_ERR_NVS_NOT_ENOUGH_SPACE`, `ESP_ERR_NVS_NO_FREE_PAGES`, `ESP_ERR_NO_MEM`, `ESP_ERR_NVS_INVALID_STATE`, `ESP_ERR_FLASH_OP_FAIL`, `ESP_ERR_NVS_REMOVE_FAILED` oder unbekannter Fehler | `CommitOutcomeUnknown` | Der Aufruf kann bereits neue Chunks, Index, Page-GC oder Entfernung begonnen haben. Der alte Wert darf nicht sicher behauptet werden; `ESP_ERR_NVS_REMOVE_FAILED` ist ausdrücklich konservativ Unknown |
| `nvs_set_blob` → `ESP_ERR_NVS_INVALID_HANDLE`, `ESP_ERR_NVS_READ_ONLY`, kontrolliert unmögliche `INVALID_ARG`/`INVALID_NAME` vor Storage-Mutation | `WriteError` nur, wenn der Testnachweis die Vor-Mutationsprüfung dieses konkreten Pfads bestätigt; sonst `CommitOutcomeUnknown` | Feste Keys, RW-Handle, nicht-nulliger Empty-Sentinel und Lifecycle machen diese Fehler im Produktionsnormalpfad unmöglich. Ein nicht eindeutig phasenauflösbarer Fehler nach Mutationsbeginn bleibt Unknown |
| `nvs_commit` → `ESP_OK` nach erfolgreichem Set | `Success` | Der gepinnte Commit ist No-op; der erfolgreiche Set-Pfad hat den vollständigen neuen Wert gespeichert |
| `nvs_commit` → jeder nicht erfolgreiche Status | `CommitOutcomeUnknown` | Auch ein injizierter oder zukünftiger Commitfehler darf nicht als sicher unveränderter Zustand ausgegeben werden |

Ein leerer Wert wird mit einem nicht-nulligen privaten Sentinel und Länge 0 an
`nvs_set_blob` übergeben. Damit werden keine null-pointer-spezifischen Annahmen
eingeführt. Die Read-Größenabfrage liefert 0; danach wird höchstens ein
Sentinelpuffer ohne Wertallokation verwendet und ein leerer `std::string` mit
`Success` zurückgegeben.

## Deterministischer Read-Pfad

Jeder Read besteht aus genau zwei `nvs_get_blob`-Aufrufen:

1. `nvs_get_blob(handle, key, nullptr, &requiredBytes)`;
2. `requiredBytes <= maxBytes` prüfen;
3. nur dann einen Puffer von exakt `requiredBytes` Bytes allokieren;
4. `nvs_get_blob(handle, key, buffer, &readBytes)` mit exakt dieser Länge;
5. nur bei vollständiger, exakt gleich großer Rückgabe den Wert ausgeben.

Es gibt niemals eine Allokation oberhalb `maxBytes`, keine proportional größere
Reserve und keine blinde Wiederholung. Wächst der Wert zwischen Abfrage und
Lesen, wird bei einer beobachteten neuen Größe oberhalb des Limits
`CapacityError` geliefert, sonst `ReadError`; ein zwischenzeitliches
`NOT_FOUND`, eine unerwartete Längenänderung oder ein Teilwert ist `ReadError`.
Schrumpft er, wird ebenfalls kein verkürzter Wert still akzeptiert. Das
zustandsbehaftete Host-Double erzwingt alle diese Races.

## Vollständige Kapazitätsinventur

Die folgende Inventur verwendet die kanonischen Schlüssel und Maxima. Die
angegebenen Bytes sind die maximal zulässigen gespeicherten Envelope-Records,
nicht neue Adapterlimits.

| Recordgruppe | konkrete Schlüsselquelle | Anzahl | maximales Record | gepinnte NVS-Entries je Record |
|---|---|---:|---:|---:|
| User-Konfiguration | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `uc0..uc3`; [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) `kMaximumUserConfigurationPayloadBytes + 45` | 4 | 301 B | 12 |
| Service-Konfiguration | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `sc0..sc3`; [`configuration_graph_store.cpp`](../../lib/fermentation_app/src/configuration_graph_store.cpp) ruft das leere Servicepayload mit 45-B-Envelopegrenze auf | 4 | 45 B | 4 |
| Program-Katalog | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `pc0..pc3`; [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) `kMaximumProgramCatalogPayloadBytes + 45` | 4 | 32.813 B | 1.036 |
| Manifest | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `cm0..cm2`; [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) `kMaximumConfigurationManifestEnvelopeBytes` | 3 | 149 B | 7 |
| Root | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `cr0..cr1`; [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) `kMaximumConfigurationRootEnvelopeBytes` | 2 | 114 B | 6 |
| Bootstrap | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `cb0..cb1`; [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) `kMaximumConfigurationBootstrapEnvelopeBytes` | 2 | 42 B | 4 |
| Run-Checkpoint | [`run_persistence_store.cpp`](../../lib/fermentation_app/src/run_persistence_store.cpp) → `rc0`, `rc1`; [`run_persistence_coordinator.cpp`](../../lib/fermentation_app/src/run_persistence_coordinator.cpp) `kMaximumCheckpointRecordBytes` | 2 | 8.240 B | 262 |
| Run-Head | [`run_persistence_store.cpp`](../../lib/fermentation_app/src/run_persistence_store.cpp) → `rh0`; [`run_persistence_coordinator.cpp`](../../lib/fermentation_app/src/run_persistence_coordinator.cpp) `kMaximumHeadRecordBytes` | 1 | 256 B | 10 |

Damit gelten 22 gleichzeitig mögliche Schlüssel und:

```text
4*12 + 4*4 + 4*1036 + 3*7 + 2*6 + 2*4 + 2*262 + 1*10 = 4.783 Entries
Namespace fermentation                                      =     1 Entry
persistenter Maximalbestand                                 = 4.784 Entries
größter simultaner Austauschrecord (pc0..pc3)              = 1.036 Entries
Peak vor alter Recordentfernung                             = 5.820 Entries
zwei freie Seiten für Update-/GC-Reserve                    =   252 Entries
Mindestbudget                                               = 6.072 Entries
ceil(6.072 / 126 Entries je Seite)                          =    49 Seiten
49 * 4.096 B                                                = 196 KiB Untergrenze
```

Die Entrywerte folgen ausschließlich den gepinnten Konstanten
`NVS_CONST_ENTRY_SIZE = 32`, `NVS_CONST_ENTRY_COUNT = 126` und
`NVS_CONST_CHUNK_MAX_SIZE = 4.000`:

- ein einseitiger variabler Blob benötigt eine variable Metadaten-Entry plus
  `ceil(bytes / 32)` Payload-Entries und zusätzlich einen separaten
  `BLOB_IDX`-Entry: `2 + ceil(bytes / 32)` Entries;
- ein mehrseitiger Blob benötigt je Datenchunk Metadaten plus gerundete
  Datenentries sowie genau einen zusätzlichen `BLOB_IDX`;
- 32.813 B ergeben acht 4.000-B-Chunks, einen 813-B-Rest und einen Index:
  `8*126 + (1+ceil(813/32)) + 1 = 1.036`;
- 8.240 B ergeben `2*4.000 B + 240 B` und einschließlich des separaten
  `BLOB_IDX` `2*126 + (1+ceil(240/32)) + 1 = 262` Entries;
- ein leerer Blob benötigt den BLOB-Daten-/Indexpfad, wird aber nicht als
  Nullpointer geschrieben.

`nvs_get_stats().available_entries` zieht gemäß `nvs_pagemanager.cpp` eine
volle Seite als NVS-Reserve ab. Deshalb fordert der Nachweis zwei freie Seiten
im stabilen Vorbefüllungszustand: eine Seite für die NVS-Reserve und eine
zusätzliche Seite für den Update-/GC-Übergang. Die Capacity-Prüfung muss sowohl
den Entrybestand als auch die PageManager-Situation und die tatsächliche
Seitenpackung prüfen.

Die aktuelle 24-KB-Buildbaseline ist schon rechnerisch FAIL:

- 24.576 B sind kleiner als der einzelne maximale Konfigurationsrecord von
  32.813 B;
- bei sechs NVS-Seiten lässt `writeMultiPageBlob` höchstens fünf nutzbare
  Datenpages zu, also höchstens 20.000 B vor der weiteren Chunk-/Indexlogik;
- damit kann der maximale einzelne `ProgramCatalog`-Record dort nicht
  aufgenommen werden.

Der spätere Nachweis wird durch `scripts/issue_90_nvs_capacity.py` erzeugt und
als `docs/ISSUE_90_CAPACITY_REPORT.md` abgelegt. Das Skript importiert oder
liest die kanonischen Limits nicht als zweite Fachwahrheit: Es prüft die
Inventur gegen die Quellen, die gepinnten NVS-Konstanten, die Chunkgrenzen,
den Namespace-Entry, zwei freie Seiten und das 69-Seiten-Auswahlfenster.

Für die Run-Persistence prüft das Skript zusätzlich die exakte relevante
Schlüsselmenge mechanisch: `{rc0, rc1, rh0}`. Die Prüfung ist ein Setvergleich
gegen die kanonischen Run-Persistence-Quellen, nicht nur ein
"Schlüssel-vorhanden"-Check. Jeder fehlende oder zusätzlich persistente
Run-Key, jede Änderung der Slotanzahl oder jede abweichende Zuordnung lässt den
Capacity-Check scheitern.

## Konkreter 4-MB-Weg und Tabelle

Die aktuelle Single-App-Baseline wird durch die gepinnte
`partitions_singleapp.csv` beschrieben: `nvs` 24 KiB ab `0x9000`, `phy_init`
4 KiB ab `0xf000`, `factory` ab `0x10000`. Nach Planfreigabe und bestätigten
Annahmen wird sie projektspezifisch ersetzt durch:

- CSV: `partitions/issue_90_state_store.csv`;
- gemeinsame Produktionsdefaults: `sdkconfig.defaults`;
- Kconfig:

  ```text
  CONFIG_PARTITION_TABLE_CUSTOM=y
  CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/issue_90_state_store.csv"
  CONFIG_NVS_BDL_STACK=n
  CONFIG_NVS_FLASH_VERIFY_ERASE=y
  CONFIG_NVS_FLASH_ERASE_ATTEMPTS=2
  ```

- `main/CMakeLists.txt` erhält keine eigene Tabellenlogik; ESP-IDF-Kconfig/
  CMake bindet die CSV ein;
- `lib/device_platform_esp_idf/CMakeLists.txt` erhält nur die notwendige
  private Laufzeitabhängigkeit `nvs_flash`.

Das vollständige 4-MB-Kandidatenlayout für die #90-Adapterentscheidung ist:

```text
0x000000–0x000FFF  Bootloader-reservierter Anfang
0x001000–0x007FFF  Bootloaderbereich gemäß Build
0x008000–0x008FFF  Partitionstabelle
0x009000–0x00EFFF  nvs,         data,nvs,    24 KiB
0x00F000–0x00FFFF  phy_init,    data,phy,     4 KiB
0x010000–0x054FFF  state_store, data,nvs,   276 KiB / 69 Seiten
0x055000–0x05FFFF  Alignment-Lücke vor App
0x060000–0x15FFFF  factory,     app,factory,   1 MiB
0x160000–0x3FFFFF  verbleibender Flash      2.625 MiB
```

Die Appgröße wird nicht geraten: beide Profile bauen nach Adapterintegration
mit `python3 scripts/build_esp_idf_profiles.py all`; der Report dokumentiert
Firmware-BIN, ELF-/Partition-Offsets, Alignment und verbleibende Reserve. Ein
Ergebnis außerhalb des 4-MB-/69-Seiten-/1-MB-App-Fensters öffnet das oben
beschriebene materielle Owner-Gate.

## Ausführbarer Hosttest: gepinnte NVS-BDL-Basis

Der primäre Testpfad ist ein ESP-IDF-v6.0.2-Linux-Hostprojekt mit der
gepinnten NVS-Hostimplementation, nicht ein IDF-unabhängiger Rückgabecode-
Mock. Der Testbaum verwendet strikt eine eigene Konfiguration:

```text
CONFIG_IDF_TARGET="linux"
CONFIG_NVS_BDL_STACK=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_nvs_host_test.csv"
```

Die Testinitialisierung ruft für die konkrete R1-Konfiguration ausschließlich
`nvs_flash_init_partition_bdl("state_store", bdl)` auf. Der Harness
konstruiert den Adapter zusätzlich mit explizit gelieferten Werten
`partition = "state_store"` und `namespace = "fermentation"`; diese Werte
sind Test-/R1-Konfiguration und keine Adapterdefaults. Das Double muss den
`esp_blockdev`-Vertrag erfüllen: `read_size=1`, `write_size=1`,
`erase_size=4096`, Partitionsgröße als 4096-Vielfaches, `0xff` nach Erase,
Read/Write/Erase/Release-Callbacks und keine Verschlüsselungsflagge. Es wird
ein zustandsbehafteter Byte-/Erase-Trace geführt; die Test-BDL-Lebensdauer
gehört dem Testbaum und endet erst nach `nvs_flash_deinit_partition`.

`CONFIG_NVS_BDL_STACK=y` und `nvs_flash_init_partition_bdl()` dürfen nicht in
den Produktionsdefaults oder in den normalen ESP32-Profilen erscheinen. Der
Produktionspfad bleibt die normale NVS-Partition mit
`nvs_flash_init_partition()`; der BDL-Hosttest prüft NVS-Storage-/Recovery-
Semantik, nicht den Standard-ESP32-Flash-/Partitionspfad. Diesen realen Pfad
belegt ausschließlich die spätere Hardwarematrix.

Vorgesehene Testdateien:

- `test/esp_idf_nvs_adapter_host/CMakeLists.txt`;
- `test/esp_idf_nvs_adapter_host/sdkconfig.defaults`;
- `test/esp_idf_nvs_adapter_host/partitions_nvs_host_test.csv`;
- `test/esp_idf_nvs_adapter_host/main/CMakeLists.txt`;
- `test/esp_idf_nvs_adapter_host/main/test_nvs_state_store_host.cpp`;
- `test/esp_idf_nvs_adapter_host/main/stateful_block_device.hpp/.cpp`;
- `test/esp_idf_nvs_adapter_host/main/nvs_api_fault_seam.hpp/.cpp`.

Die Host-CSV enthält mindestens die Testpartition
`state_store,data,nvs,0x9000,0x45000`; sie ist ein BDL-Testmedium und keine
Produktionspartitionstabelle.

Der Fault-Seam bleibt testbaumprivat: Er verwendet nur testseitige Linker-
Wrapper (`--wrap`) für `nvs_open_from_partition`, `nvs_get_blob`,
`nvs_set_blob` und `nvs_commit`, delegiert standardmäßig an die echten
ESP-IDF-Funktionen und injiziert nur ausgewählte Fehler/Races. Er ersetzt
keinen Port, definiert keine zweite NVS-Schnittstelle, besitzt keine
Produktionslebensdauer und wird nicht in `lib/device_platform_esp_idf/private/`
abgelegt. Insbesondere entsteht dort kein
`nvs_lifecycle_test_fixture.hpp` und keine Composition-Root-Logik.

Testseitige Init-/Deinit-Reihenfolge, BDL-Besitz, Fehler-Injection und
Reinitialisierung werden in `test/` orchestriert. Damit werden alle Open-,
Größen-, Read-, Set- und Commit-Fehler ausführbar geprüft, ohne das
Produktionsmodul zu einer Testabstraktion oder einem zweiten öffentlichen
Persistenzport zu machen.

Reproduzierbarer Hostlauf nach Planfreigabe:

```bash
test -n "${IDF_PATH:-}" && test -f "$IDF_PATH/export.sh"
. "$IDF_PATH/export.sh"
cd test/esp_idf_nvs_adapter_host
idf.py --preview set-target linux
idf.py build
./build/issue_90_nvs_adapter_host.elf --ci-regression
```

Der vollständige Verifikationslauf verwendet denselben Build und
`./build/issue_90_nvs_adapter_host.elf --exhaustive --seed 0`; alle erzeugten
Logs und JSON-Messartefakte werden mit Commit-SHA, IDF-SHA, Seed und Szenario
beschriftet.

## Power-Cut-, Readback-, Erase- und GC-Nachweise

### Hostmatrix und Orakel

Das BDL-Double friert bei einem konfigurierten Callback-Zähler die persistente
Byteabbildung ein und gibt für alle folgenden Operationen einen Flashfehler
zurück. Es werden keine Exceptions über die C-ABI geworfen. Der Test zerstört
anschließend Handles/Storage, initialisiert dieselben Bytes neu und führt den
Readback durch.

Die exhaustive Matrix enumeriert mindestens:

- jeden `BLOB_DATA`-Chunk des neuen Werts;
- den neuen `BLOB_IDX`;
- jeden GC-/PageManager-Copy-Write;
- die Entfernung des alten Blob-Index;
- jeden alten Datenchunk und jeden Page-Erase-Callback;
- den `nvs_commit()`-Kontrollpunkt, obwohl er in v6.0.2 keine Flashmutation
  ausführt.

Für den vollständigen Owner-Verifikationslauf ist die Schnittmenge der
Unterbrechungspunkte der vollständige aufgezeichnete Satz aller mutierenden
BDL-Callbacks: jeder `Write`- und jeder `Erase`-Callback wird geschnitten.
Die Auswahl darf nicht anhand von `length` verkleinert werden; insbesondere
Entry-State-/Range-State-Änderungen mit `length == 4` bleiben enthalten. Eine
semantische Phasenklassifikation darf die geforderten BLOB_DATA-, BLOB_IDX-,
Altwertentfernungs-, GC/PageManager-Copy- und Page-Erase-Mutationen nur
zusätzlich benennen, nicht aus dem Cut-Satz entfernen. Ist ein Callback nicht
eindeutig einer dieser Phasen zuordenbar, bleibt er als reproduzierbarer
mutierender Cut-Punkt enthalten; ein fehlender oder verworfener Callback ist
im Exhaustive-Lauf FAIL. Der kurze CI-Regressionssatz darf weiterhin eine
kleinere, feste deterministische Auswahl verwenden.

Die Traceklassifikation verwendet Callbackposition, Adresse/Länge und den
gepinnten NVS-Seiten-/Entryinhalt; sie behauptet keine interne Phase, die nicht
aus dem Artefakt ableitbar ist.

Pflichtszenarien sind: absent→empty, empty→binary, kleiner→größer,
32/33-B-Grenze, 4.000/4.001-B-Chunkgrenze, maximaler 32.813-B-Programmkatalog,
8.240-B-Checkpoint, bestehender maximaler Wert→neuer maximaler Wert und ein
vorbefüllter GC-Fall. Drei feste Bytepatterns (`00`, `a5`, `ff`) und drei
Wiederholungsdurchläufe werden im Vollauf verwendet.

Nach jedem Unterbruch gilt ausschließlich:

- vollständiger alter Wert; oder
- vollständiger neuer Wert; bei vorher absent zusätzlich `NotFound`.

Teilwerte, Mischwerte, fremde Länge, CRC-/Envelope-Fehler, ein nicht
reinitialisierbarer Storage oder ein nicht eindeutig lesbarer Zustand sind
FAIL. Ein `CommitOutcomeUnknown` wird nie als unverändert ausgegeben; der
Readback löst den tatsächlichen alten/neuen Stand auf. Bei unauflösbarem
Readback bleibt die Anwendung fail-closed.

Jeder Fall protokolliert mindestens Scenario, Seed, Pattern, Callback-/
Operationnummer, abgeleitete Mutationsphase, Setstatus, Commitstatus,
Reinitialisierungsstatus, Readstatus, Länge und SHA-256 von erwartetem alt,
erwartetem neuem und gelesenem Wert.

Für normale, nicht absichtlich unterbrochene `PREFILL`- und
Rotationspfade ist ausschließlich `StateStoreWriteStatus::Success` ein
bestandenes Ergebnis. `CommitOutcomeUnknown` oder jeder andere Fehler ist dort
FAIL und wird fail-closed behandelt; er darf nicht als erfolgreicher Clean-
Write oder als stillschweigend akzeptierte Alt-/Neu-Auswahl zählen.
`CommitOutcomeUnknown` mit dem Alt-/Neu-Readback-Orakel gehört ausschließlich
zu den gezielten Fehler-, BDL-Cut- und Power-Cut-Recoveryfällen.

### Reproduzierbarer GC-/Erase-Fall

Der Hosttest führt zuerst die 22 Schlüssel mit allen oben genannten Maxima in
die 69-Seiten-Testpartition ein. Danach rotiert er in fester Reihenfolge
`pc0`, `pc1`, `rc0`, `rc1`, `rh0` mit neuen, byteverschiedenen Maximalrecords,
bis der erste Page-Erase-Callback beobachtet wird, höchstens 2.048
Schreiboperationen. Kein beobachteter Erase innerhalb dieser Grenze ist FAIL.
Der Fall wird anschließend an jedem Copy-/Erase-Callback unterbrochen und
reinitialisiert. Erwartet werden `ESP_OK` bei der Reinitialisierung sowie das
vollständige Alt-oder-Neu-Orakel für jeden betroffenen Schlüssel; jeder andere
Ausgang ist FAIL beziehungsweise bei unauflösbarem Zustand fail-closed.

Damit basiert der Power-Cut-Nachweis nicht nur auf einer frischen Partition
oder einem einfachen Blobwrite. Die Produktionsoption
`CONFIG_NVS_FLASH_VERIFY_ERASE=y` und exakt zwei Erase-Versuche wird aus der
gepinnten Kconfig/`nvs_partition.cpp` übernommen. Die Verifikation liest bei
der normalen ESP-Partition den gelöschten Bereich zurück und versucht einen
Fehler höchstens zweimal; das ist eine begrenzte Hersteller-/Konfigurations-
entscheidung, kein automatisches Löschen, Formatieren oder Init-Retry. Der
BDL-Zweig ruft dagegen direkt den BDL-Erase-Callback auf und testet deshalb
nicht die Standard-Flash-Erase-Verifikation.

### Schmaler On-Target-Harness für den realen ESP32-Pfad

Der reale Nachweis benötigt neben dem Runner eine testseitige Firmwareseite.
Nach Planfreigabe werden dafür ausschließlich im bestehenden
`esp32_bringup`-Profil folgende Pfade ergänzt:

- `main/issue_90_nvs_hardware_verification.hpp` und
  `main/issue_90_nvs_hardware_verification.cpp` enthalten den schmalen
  UART-Harness, die deterministische Arbeitslast, die Ressourcen-/NVS-
  Statusausgabe und die testseitige Raw-Page-Evidenz;
- `main/CMakeLists.txt` nimmt diese Quellen nur unter
  `CONFIG_APP_PROFILE_ESP32_BRINGUP` auf und setzt ausschließlich dort
  `APP_ISSUE_90_NVS_HARDWARE_TEST=1`;
- `main/app_main.cpp` ruft den Harness nur unter dieser Compile-Time-Guard
  auf und startet in diesem Profilpfad danach keine normale
  Anwendungsschleife. Das Releaseprofil erhält weder Quelle noch Definition;
  ein Profilisolationscheck prüft das zusätzlich in beiden
  `compile_commands.json` und im Release-ELF auf fehlende `ISSUE90`-Symbole /
  Marker.

Die Harnessquelle und alle ausschließlich dafür nötigen Abhängigkeiten werden
mit derselben Bring-up-Grenze isoliert. Das gilt insbesondere für zusätzliche
UART-, Partitions- und Hash-/Evidenzabhängigkeiten in
`main/CMakeLists.txt` (beispielsweise `esp_driver_uart`, `esp_partition`,
`driver` oder eine nur für den Harness benötigte Hashkomponente). Sie dürfen
nur im `APP_ISSUE_90_NVS_HARDWARE_TEST`-/Bring-up-Pfad in den Componentgraphen
gelangen. Bereits vom produktiven `main` benötigte Abhängigkeiten bleiben
erhalten und werden nicht künstlich entfernt. Der Release-Nachweis prüft
Compile-Database, Componentgraph und ELF gemeinsam auf fehlende
Harnessquellen, `APP_ISSUE_90_NVS_HARDWARE_TEST`, `ISSUE90`-Marker/Symbole und
ausschließlich für #90 neu eingeführte Dependencies.

Damit folgt #90 dem vorhandenen `esp32_bringup`-/Compile-Time-Isolationsmuster
aus `main/CMakeLists.txt` und `app_main.cpp`. Es entsteht keine zweite
Wegwerf-Anwendung, kein Testfixture unter
`lib/device_platform_esp_idf/private/` und kein öffentlicher Persistenzport.
Der Harness verwendet im Testbaum den echten `NvsStateStore` über den
unveränderten `IStateStore`-Vertrag.

#### No-PSRAM-Speicher- und Stackvertrag

Page-/GC-Evidenz darf nicht als automatische Mehr-KiB-Objekte im
unveränderten ESP-IDF-Main-Task-Stack liegen. Die Umsetzung verwendet deshalb
einen ausschließlich internen, No-PSRAM-tauglichen, statisch begrenzten
Test-BSS-Speicherpfad für die zwei festen `PageEvidence[69]`-Evidenzmengen und
den Readbuffer (oder einen technisch gleichwertigen explizit gemessenen
internen Heap-/Bring-up-Testtask-Pfad). Automatische Kopien dieser Arrays sind
unzulässig. Die Obergrenze wird aus den konkreten `sizeof`-Werten gebildet,
per `static_assert` gegen das feste Harness-Scratchbudget geprüft und im
Linker-/Map-/ELF-Nachweis als interne Speicherbelegung ausgewiesen. Es gibt
keine PSRAM-Abhängigkeit. Ein alternativer dedizierter Testtask muss seine
Stackgröße ebenfalls explizit festlegen und per Compile-/Map-/Stacknachweis
belegen; der unveränderte 3.584-B-Main-Task-Stack darf nicht als Reserve
herangezogen werden. Reale Heap-, Largest-Block- und Stack-HWM-Werte bleiben
bis zum Hardwarelauf zusätzlich `BLOCKED/NOT_RUN`.

Solange noch kein produktiver `IStateStore`-Verbraucher existiert, besitzt
dieser Harness als Test-Orchestrator den Partitionslebenszyklus und die
konkrete R1-Konfiguration: Er ruft
`nvs_flash_init_partition(config.partitionLabel.c_str())` über den normalen
ESP-IDF-Flashpfad auf, prüft strikt `ESP_OK`, konstruiert erst danach den
echten `NvsStateStore(config)` und zerstört bei normalem Ende zuerst den Store,
dann den Handle-/Operationskontext und zuletzt
`nvs_flash_deinit_partition(config.partitionLabel.c_str())`.
`NvsStateStore` selbst erhält keine Init-/Deinit-Methode. Nach einem
Power-Cut gibt es kein Deinit; der nächste Boot initialisiert die Partition
erneut. Jede Initialisierungsabweichung ist fail-closed und führt weder zu
Erase, Formatierung, Retry noch zur normalen Anwendungs-/Aktor-Schleife.

#### Deterministisches UART-Protokoll

Der Runner
`scripts/issue_90_nvs_hardware_verification.py` spricht ausschließlich ein
versioniertes, zeilenorientiertes Protokoll. Unerwartete, fehlende oder nicht
parsebare Marker sind `FAIL`/`NOT_RUN`, niemals ein impliziter Erfolg.

Der Harness akzeptiert genau diese Befehle:

```text
PREFILL seed=0
ROTATE max_writes=2048
READBACK_ALL
REBOOT
STOP
```

Für einen externen Power-Cut wird zusätzlich `CUT_ARM token=<deterministic-token>`
akzeptiert. Der Harness antwortet mit mindestens diesen vollständigen Markern
(Statuscodes sind ESP-IDF-Codes in Hex):

```text
ISSUE90 READY protocol=1 idf=7101770dc6db2667b3c477cc31365dd1acd6db4e profile=esp32_bringup partition=state_store pages=69
ISSUE90 PREFILL_DONE keys=22
ISSUE90 CUT_ARMED token=<token>
ISSUE90 ROTATE_BEGIN seq=<n> key=<key> old_sha256=<sha> new_sha256=<sha>
ISSUE90 ROTATE_RESULT seq=<n> set=<esp_err> commit=<esp_err>
ISSUE90 GC_ERASE_DETECTED page=<n> old_seq=<n> new_seq=<n> evidence_sha256=<sha>
ISSUE90 READBACK key=<key> status=<status> len=<n> sha256=<sha>
ISSUE90 STATS used_entries=<n> free_entries=<n> available_entries=<n> total_entries=<n>
ISSUE90 RESOURCE stage=<name> free_heap=<n> largest_block=<n> stack_hwm=<n>
ISSUE90 REBOOTING
ISSUE90 PASS reason=<reason>
ISSUE90 FAIL reason=<reason>
```

#### Kalibrierter Power-Cut-Window-Vertrag

Die vier Fenster `blob_data`, `blob_index`, `old_removal` und `gc_erase` sind
eigene, technisch kalibrierte Testparameter und keine bloßen Szenarionamen.
Die Kalibrierung ist testseitig und instrumentiert weder den produktiven
NVS-Adapter noch führt sie einen neuen Persistenzport ein:

1. Der Runner stellt einen explizit dokumentierten, sauberen
   Kalibrier-Ausgangszustand her, startet die festgelegte Rotation und misst
   die Zeitbasis ab dem zugehörigen `ROTATE_BEGIN`.
2. Für jedes Fenster wird eine reproduzierbare Parameterspur mit mindestens
   `window`, Sequenz/Key, `calibration_run_id`, `delay_us` zwischen
   `ROTATE_BEGIN` und `TRIP`, Hook-/Restore-Timeouts und Seed/Pattern geführt.
   Die Kandidaten werden über einen testseitigen Sweep und Wiederholungen
   bestimmt; der Power-Controller erhält keine implizite Semantik.
3. Ein Kandidat wird nur angenommen, wenn die nachfolgende Raw-Page-/Entry-
   Evidenz und das Readback die tatsächlich getroffene Phase eindeutig
   klassifizieren: BLOB_DATA, BLOB_IDX, Altwertentfernung oder der reale
   GC-/Page-Erase-Fall. Nicht eindeutig klassifizierbare oder nur durch den
   Zeitpunkt behauptete Kandidaten werden verworfen.
4. Der Runner verwendet im Ownerlauf ausschließlich die akzeptierten,
   window-spezifischen `delay_us`-/Triggerparameter und löst `TRIP` damit
   reproduzierbar relativ zu `ROTATE_BEGIN` aus. Ein Zielwindow gilt erst als
   verschieden, wenn seine Evidenzklasse von den drei anderen getrennt
   nachgewiesen ist. Für den Host-BDL bleibt der tatsächliche `Erase`-Callback
   ein verpflichtender Exhaustive-Cut-/GC-Nachweispunkt. Der reale ESP32-
   Standard-Flashpfad besitzt diesen BDL-Callback nicht: Dort darf
   `gc_erase` ausschließlich durch das starke On-Target-Raw-Page-/Readback-
   Orakel mit gelöschter vorher gültiger Seite, neuer Sequenz-/Belegungs-
   struktur, lebenden kopierten Records, konsistenten NVS-Stats und
   Vorher-/Nachher-Hashes akzeptiert werden. Es gibt dafür keinen internen
   oder produktiven Erase-Hook und keine ESP-IDF-Eigeninstrumentierung.

Kalibrierlauf, Sweepkandidaten, akzeptierte Parameter, Zielwindow,
Triggerzeitpunkt, Hookantworten, Raw-Page-Klassifikation und Readback werden
im Artefakt mit Run-ID, Source-/Plan-SHA und Zeitstempeln archiviert. Der
Owner-Verifikationslauf führt mindestens zehn Wiederholungen je dieser vier
tatsächlich verschiedenen Fenster aus. Fehlt eine stabile Kalibrierung oder
trifft ein Lauf kein eindeutig belegtes Zielwindow, ist er FAIL/BLOCKED und
nicht PASS.

`PREFILL` schreibt deterministisch die vollständige Inventur aus 19
Konfigurationsschlüsseln (`uc0..uc3`, `sc0..sc3`, `pc0..pc3`, `cm0..cm2`,
`cr0..cr1`, `cb0..cb1`) und drei Lauf-/Checkpointschlüsseln (`rc0`, `rc1`,
`rh0`), jeweils mit dem berechneten Maximalrecord und festem Seed. `ROTATE`
verwendet danach die feste Sequenz `pc0`, `pc1`, `rc0`, `rc1`, `rh0` mit
byteverschiedenen Maximalrecords bis zum ersten nachgewiesenen GC/Erase oder
höchstens 2.048 Schreiboperationen. Jede Operation ruft den echten Adapter
auf und meldet Set-/Commitstatus, `nvs_get_stats()` und Ressourcenstände an
den Runner. `READBACK_ALL` liefert für alle 22 Records exakte Länge und
SHA-256; der Runner vergleicht ausschließlich vollständige alte oder neue
Bytes. `REBOOT` veranlasst einen echten `esp_restart()`, worauf ein neuer
`READY`-Marker und derselbe vollständige Readback folgen.

Zwei Baselines mit ausdrücklich getrennten Zwecken sind verbindlich:

### Clean-Reboot-Baseline

Nach dem deterministischen `PREFILL seed=0` wird der erwartete Zustand
aller 22 Keys unveränderlich mit Länge und SHA-256 gespeichert. Diese
Baseline dient ausschließlich sauberen Neustartkontrollen: Jede der drei
Kontrollen vergleicht alle 22 Werte exakt mit ihr; ein abweichender Hash,
eine abweichende Länge oder ein fehlender Wert ist FAIL.

### Cut-/Target-Baseline

Unmittelbar vor jedem geschnittenen Zielwrite wird eine neue, zielbezogene
Baseline erfasst. Sie enthält alle bereits erfolgreich abgeschlossenen
Vorbereitungsschreibungen und Rotationselemente bis genau zu diesem Zielwrite,
mindestens die Alt-Länge und den Alt-SHA-256 des Zielkeys sowie den vollständigen
erwarteten Zustand der nicht betroffenen Keys.

- `ROTATE_BEGIN.old_sha256` und `old_len` müssen exakt der unmittelbaren
  Cut-/Target-Baseline entsprechen.
- Nach dem Cut müssen alle nicht betroffenen Keys exakt dieser Baseline
  entsprechen.
- Nur der Zielkey darf vollständig dem Altwert oder vollständig dem Neuwert
  entsprechen; bei vorher absent bleibt zusätzlich `NotFound` zulässig.
- Dieselbe unmittelbare Baseline wird für Window-/Phasenklassifikation,
  erwartetes Readback und die Artefaktprüfung verwendet.
- Ein früher PREFILL-Snapshot darf bei `target_rotation > 0` nicht als
  Altzustand des späteren Zielwrites verwendet werden.

Jede unabhängige Power-Cut-Wiederholung beginnt entweder aus einem explizit
testseitig zurückgesetzten/gelöschten Ausgangszustand oder aus einer
dokumentierten, nachweislich deterministischen fortlaufenden Sequenz. Eine
bloße erneute Vorbefüllung auf einer durch frühere Cuts veränderten
NVS-Seiten-/GC-Lage gilt nicht als identische Baseline. Ein notwendiger
Reset/Erase bleibt ausschließlich Bring-up-/Testlogik und wird nie Bestandteil
von `NvsStateStore`.

Die Partition- und NVS-Statusmeldung kommt testseitig aus
`esp_partition_find_first()`/`esp_partition_get()` und `nvs_get_stats()` und
enthält Label, Adresse, Größe, Typ/Subtyp, Eintragsstatistik sowie die
Messpunkte `startup`, `prefill`, `rotation`, `readback` und `post-reboot`.
Ressourcen werden mit den bestehenden ESP-IDF-Statusquellen für freien Heap,
größten freien 8-Bit-Block und Stack-High-Water-Mark erhoben. Diese
Testausgaben sind Artefakte, kein neuer produktiver API-Vertrag.

#### Nachweis eines tatsächlichen Page-GC/Erase

Der Harness liest ausschließlich testseitig die 69-Seiten-Partition über
`esp_partition_read()` in 4-KiB-Seitensnapshots und dekodiert die gepinnte
NVS-Seiten-/Entry-Struktur anhand von `nvs_constants.h`. Für jeden
Rotationsschritt werden Vorher-/Nachher-Snapshot, Seite, Sequenznummer,
Seitenstatus, belegte Entries und SHA-256 des 4-KiB-Inhalts archiviert.

Der span-aware Raw-Page-Parser ist selbst Teil des Korrekturvertrags:

- Header-CRC wird nur für Entries geprüft, die tatsächlich NVS-Metadaten-/
  Item-Header sind; Continuation-/Payload-Entries werden nicht fälschlich als
  eigenständige Header behandelt.
- `span` wird aus dem Metadaten-Item gelesen und gegen die zulässige
  Entry-/Seitenreichweite validiert. Continuation-/Payload-Entries werden
  entsprechend diesem Span übersprungen beziehungsweise als Payload
  behandelt.
- Die Entry-State-Konsistenz wird über den gesamten Span geprüft, nicht nur
  am ersten Entry. Überlappende, zu lange, out-of-range oder anderweitig
  widersprüchliche Spans sind korrupte Evidenz und fail-closed.
- `BLOB_DATA`, `BLOB_IDX`, `live`, `removed` und GC-/Copy-Bezüge werden
  ausschließlich aus gültigen Metadaten-Items mit konsistentem Span abgeleitet.
- Ein Parserfehler darf niemals zu `GC_ERASE_DETECTED` oder zu einem
  Hardware-PASS führen; er ist FAIL beziehungsweise für den nicht ausführbaren
  Nachweis `BLOCKED/NOT_RUN`.

`GC_ERASE_DETECTED` darf nur ausgegeben werden, wenn alle drei Bedingungen
gemeinsam erfüllt sind: (a) eine zuvor gültige, nichtleere NVS-Seite mit
Sequenznummer und Einträgen ist nach genau diesem `nvs_set_blob()`-Schritt
vollständig `0xff`/gelöscht, (b) eine andere Seite besitzt danach die erwartete
neue Sequenz-/Belegungsstruktur und mindestens die kopierten lebenden Records,
und (c) `nvs_get_stats()`, der vollständige Readback und die gespeicherten
Vorher-/Nachher-Hashes sind konsistent. Ein leerer Vorrat, eine bloße
Statistikänderung oder ein erwarteter Rotationszähler ist kein GC-/Erase-
Nachweis. Der reale Standard-Flashpfad wird damit über auslesbare
Partitions-/Page-Evidenz und nicht über einen erfundenen produktiven Hook
belegt.

Der Harness speichert bei `GC_ERASE_DETECTED` und nach jeder Reinitialisierung
die Rohsnapshots, Statuszeilen, Resetursache, A/B-Erwartungen und Hashes unter
`build/issue_90_hardware_verification/`. Ein Power-Cut während einer
`ROTATE_BEGIN`-bis-`ROTATE_RESULT`-Operation muss nach dem nächsten Boot
entweder den vollständigen alten oder den vollständigen neuen Record liefern;
bei einem Cut während GC/Erase wird zusätzlich die oben definierte Recovery-
Evidenz erwartet. Ein nicht lesbarer, teilweiser oder nicht eindeutig
zuordenbarer Zustand ist FAIL und führt zu fail-closed.

### Reale ESP32-Matrix

Nach erfolgreicher Software-/Host-Verifikation führt
`scripts/issue_90_nvs_hardware_verification.py` über den beschriebenen
On-Target-Harness denselben vorbefüllten 69-Seiten-Workload über die normale
ESP32-Partition aus. Das offene #29-Pegel-Restgate blockiert diese aktorfreien
NVS-Nachweise nicht. Die Matrix umfasst:

1. saubere Vorbefüllung und Readback aller 22 Schlüssel;
2. die deterministische Rotationsfolge bis zum nachweislichen Page-GC/Erase;
3. Power-Cut-Fenster über die kalibrierte Dauer der Blob-/GC-/Erasefolgen;
4. mindestens zehn Wiederholungen je Fenster und drei saubere Neustart-
   Kontrollen je Szenario;
5. Reset-/Bootlogs, reale Partitionserfassung, `nvs_get_stats`, Set-/Commit-
   Status und A/B-SHA-256-Readbacks.

Ein externer Cut-Aufbau wird nur mit ownerverifizierter Versorgung, Reset-
und Steuerleitung verwendet; keine GPIO-, Pegel- oder Zeitannahme wird im
Plan erfunden. Der Runner erhält dafür über `--power-cut-hook
"${POWER_CUT_HOOK}"` ein vorbereitetes, vom Repository-Root aus
repository-relativ aufrufbares Testwerkzeug (beispielsweise
`./scripts/issue_90_power_cut_hook.py`; absolute Maschinenpfade werden
abgelehnt). Dieses Werkzeug akzeptiert auf stdin `ARM token=<token>`,
`TRIP` und `RESTORE` und muss jeweils exakt `ARMED`, `TRIPPED` und `RESTORED`
bestätigen. Der Runner sendet `ARM` vor `CUT_ARM`, wartet auf den zugehörigen
`ROTATE_BEGIN`-Marker, löst `TRIP` aus, erwartet UART-Verlust vor
`ROTATE_RESULT`, stellt mit `RESTORE` die Versorgung wieder her und wartet
auf den neuen `READY`-Marker. Der vollständige Cut-/Reboot-/Readbackdatensatz
enthält Token, Marker, Hookantworten und Zeitstempel. Fehlt eine
ownerverifizierte Hook-/Versorgungsbindung, ist der Fall BLOCKED/NOT_RUN.

Der Hosttest belegt interne Storage-/Recovery-Semantik und die vollständige
Mutationsphasenmatrix; der ESP32-Test belegt den normalen
Flash-/Partition-/Erasepfad einschließlich echter Page-Evidenz. Ein
Init-/Readback-Fehler, eine Teil-/Mischversion, fehlender GC-/Erase-Nachweis
oder ein unsicherer Hardwareaufbau ist BLOCKED/FAIL und niemals PASS.

Der reproduzierbare Ablauf nach Freigabe der Hardwarefixture lautet:

```bash
test -n "${IDF_PATH:-}" && test -f "$IDF_PATH/export.sh"
. "$IDF_PATH/export.sh"
test -n "${ESP_PORT:-}"
test -n "${POWER_CUT_HOOK:-}"
python3 scripts/build_esp_idf_profiles.py all
python3 scripts/issue_90_nvs_hardware_verification.py \
  --port "$ESP_PORT" --profile esp32_bringup --scenario prefilled_gc \
  --repetitions 10 --power-cut-hook "$POWER_CUT_HOOK" \
  --artifact-dir build/issue_90_hardware_verification
```

Das neue Runner-Skript setzt den aktorfreien Testmodus, wartet auf die
bekannte Testsequenz, koordiniert die ownerverifizierte Cut-Steuerung und
schreibt nur die oben definierten Logs/Hashes/Reset-/Partitions-/NVS-Stats.
Ein manueller `idf.py monitor`-Lauf allein ist kein Power-Cut- oder Readback-
Nachweis.

#### Verbindlicher Hardware-Artefaktvertrag

`--artifact-dir` ist ein repository-relativer, vom Runner kontrollierter
Ausgabepfad. Für jeden Lauf wird dort ein reproduzierbares Manifest mit
Run-ID und UTC-Zeitstempeln geschrieben. Es referenziert mindestens Source-
und Firmware-SHA, die freigegebene Plan-SHA, ESP-IDF-SHA und Profil, die real
ermittelte Partitions-/Flash-Evidenz, Power-Cut-Window und akzeptierte
Kalibrierparameter, UART- und Hook-/Controller-Logs, Tokens und Trigger,
Readback-Längen/-Hashes, NVS-/GC-/Erase-/Raw-Page-Evidenz, Ressourcenlogs und
den Abschlussstatus jeder Wiederholung. Die Logs bleiben mit ihren
Repository-relativen Pfaden und Hashes referenzierbar; absolute Maschinen-
pfade und Secrets werden abgelehnt. Alle erzeugten Textartefakte werden vor
einem Erfolgsupload durch den Secret-/Pfadscan geprüft.

## Qualitätsgates und CI-Regressionsschutz

Der neue Hosttest wird nach der Umsetzung verbindlich in die kanonischen
Definitionen aufgenommen:

- [`docs/CI_AND_QUALITY_GATES.md`](../../docs/CI_AND_QUALITY_GATES.md) erhält
  den ESP-IDF-Linux-BDL-Hosttest, die genaue `$IDF_PATH`-Aktivierung, die
  Statusbegriffe und die zwei Reproduktionsläufe;
- [`.github/workflows/build.yml`](../../.github/workflows/build.yml) erhält
  nach ESP-IDF-Installation/`export.sh` einen Schritt, der im Repositorypfad
  `test/esp_idf_nvs_adapter_host` `idf.py --preview set-target linux`,
  `idf.py build` und `./build/issue_90_nvs_adapter_host.elf
  --ci-regression` ausführt;
- Hostlog, JSON-Matrix und IDF-/Source-SHA werden als getrennte Artefakte
  gesichert und durch Secret-/Pfadscan abgedeckt; erforderliche Anpassungen an
  `scripts/check_ci_artifact_scan_coverage.py` werden im Implementierungsschnitt
  mitgeführt.
- Die Uploadreihenfolge ist verbindlich: erst generierte Issue-90-Textartefakte
  erzeugen, dann `check_secrets.py --scan-path` erfolgreich über genau diese
  Pfade ausführen, danach `check_ci_artifact_scan_coverage.py` ausführen und
  erst nach beiden Erfolgen `actions/upload-artifact` für
  `issue-90-nvs-host` ausführen. Kein Erfolgsartefakt darf vor dem
  Secret-/Pfadscan hochgeladen werden. Der Coverage-Guard prüft deshalb sowohl
  die vollständige Pfadabdeckung als auch die Reihenfolge Scan-vor-Upload und
  schlägt bei einer Regression fehl.

Der verbindliche CI-Regressionssatz ist deterministisch und umfasst die
Grenzgrößen, empty/binary, Read-Race, Open-/Size-/Read-/Set-/Commit-Fehler,
einen maximalen Programmkatalog und einen vorbefüllten GC-Erase-Fall mit
festem Seed. Die exhaustive Matrix ist für jeden CI-Lauf nicht erforderlich;
sie bleibt aber als reproduzierbarer Owner-/Hardware-Verifikationslauf
`--exhaustive --seed 0` verpflichtend vor dem Hardwaregate. Ein fehlender
Hostlauf ist `BLOCKED`/`NOT_RUN`, nicht PASS.

### Build-/Source-Provenienzvertrag

Der gezielte ESP-IDF-Build-, Static-Analysis- und Ressourcenbericht wird nur
aus einem sauberen committed Checkout erzeugt. `Source-Git-SHA` ist exakt der
Commit, dessen Inhalte gebaut und analysiert wurden; im lokalen Owner-/Draft-
Lauf entspricht er `git rev-parse HEAD` nach erfolgreicher
`git status --porcelain`-Leerprüfung. Der lokale Objektstore ist dafür nur eine
Konsistenzprüfung und kein Erreichbarkeitsnachweis.

Vor einem PASS prüft der Lauf den Source-Commit zusätzlich gegen GitHub/Remote:

1. `gh api repos/ManuEngineer/ESP32-Fermentationsschrank/commits/<source-sha>`
   muss den exakten `Source-Git-SHA` erfolgreich remote auflösen;
2. `gh api --paginate
   repos/ManuEngineer/ESP32-Fermentationsschrank/pulls/117/commits?per_page=100`
   muss denselben SHA im aktuellen PR-Verlauf finden. Damit ist der Commit
   entweder der aktuelle PR-Head oder ein nachweisbarer Vorfahr; ein nur lokal
   vorhandener, nicht gepushter Zwischencommit bleibt ungültig, selbst wenn
   `git cat-file -e <sha>^{commit}` lokal erfolgreich ist.

Für einen normalen lokalen Build ist `Build-Commit` semantisch identisch mit
`Source-Git-SHA`; ein erfundener oder temporärer lokaler Build-Commit ist
unzulässig. Falls CI einen ephemeren PR-Merge-Commit baut, wird dieser separat
als `CI-Merge-SHA`/`Build-Commit` mit seiner ephemeren Semantik dokumentiert;
`Source-Git-SHA` bleibt die PR-Source-SHA und wird nicht durch den Merge-SHA
ersetzt. Fehlende Remote-/PR-Auflösung, eine Abweichung zwischen gebauten
Inhalten und `Source-Git-SHA` oder ein Working-Tree-/lokaler Zwischenstand ist
`FAIL`/`NOT_RUN` und kein kanonischer PASS-Nachweis.

## Umsetzungs- und Commit-Schnitte nach Planfreigabe

Die Umsetzung bleibt in diesen nachweisbaren Schnitten. Der aktuelle Plan-
Commit enthält keinen dieser Produktions- oder Testpfade.

### Verbindliche Reihenfolge nach R5.1-Freigabe

Die Umsetzung und Abschlussbewertung darf nur in dieser Reihenfolge
fortschreiten:

1. **Schritt 1A – Analyse/Evidenz:** gepinnte v6.0.2-Quelle und Tests
   analysieren, den minimalen reproduzierbaren Callback-/Power-Loss-Fall
   sichern, offizielle Espressif-Releases/Branches/Fixes evaluieren, dieselbe
   vollständige BDL-Cut-Matrix gegen einen Vergleichsstand ausführen und
   exakte Source-/IDF-SHAs sowie Ergebnisse dokumentieren. Keine
   Projektabhängigkeit ändern und keinen Vendor-Patch übernehmen.
2. **Owner-Subgate:** Wenn 1A einen konkreten Herstellerfix,
   Releasewechsel oder Backport als Lösung nachweist, exakten Zielstand/
   Fix-SHA, vollständiges Testergebnis und Kompatibilitätsauswirkung vorlegen
   und genau diesen Dependency-/Patchentscheid vom Owner freigeben lassen.
3. **Schritt 1B – erst nach Ownerfreigabe:** ESP-IDF-Version/Pin oder den
   exakt freigegebenen Vendor-/Backport-Fix ändern und danach alle
   R5.1-Gates auf dem neuen exakten Source-/IDF-Stand wiederholen. Ergibt 1A
   keinen belastbaren direkten Herstellerpfad oder erfüllt der direkte Pfad
   auch mit dem belegten Herstellerstand den Old-or-New-Vertrag nicht
   vollständig, `BLOCKED` setzen und anhalten.
4. Alle verbleibenden Adapter-, Harness-, Parser- und Runnerkorrekturen
   einschließlich Read-Race, exakter Konfiguration, span-aware Parser,
   unmittelbarer Baseline, UART-Vertrag und Artefakte;
5. die gezielte Host-CI-Regressionssuite ausführen;
6. den vollständigen `--exhaustive --seed 0`-Ownerlauf über die vollständige
   mutierende Callback-Matrix ausführen;
7. Capacity-Nachweis und Produktionspartition gegen die 69-Seiten-Annahme
   sowie exakt 4 MB ausführen;
8. beide ESP-IDF-Profile `esp32_bringup` und `esp32_release` bauen;
9. Static Analysis beider Profile ausführen;
10. Stack-, Scratch-, Release-Isolations- und Release-ELF-Nachweise führen;
11. Architektur-, Secret-/Pfad-, Artefakt-, Syntax-, Format- und Diff-Gates
    ausführen;
12. erst nach Host-Exhaustive-PASS die auf dem angeschlossenen Board
    möglichen realen #90-Nachweise ausführen: Flash/Partition, normaler
    NVS-Init/Write/Readback, Reboot/Recovery, NVS-Stats, Raw-Page/GC/Erase,
    Heap/Largest-Block/Stack-HWM, Latenzen sowie sichere aktorfreie
    Schreiblast-/Wear-Messungen;
13. Power-Cut nur ausführen und als PASS bewerten, wenn eine kontrollierbare,
    ownerverifizierte Versorgung/Hook tatsächlich vorhanden ist; DTR/RTS-
    Reset und ein bloßer USB-Anschluss sind kein Power-Loss-Fixture;
14. finale Build-, Source-, Hardware- und Artefaktprovenienz sichern und
    anschließend Ownerreview abwarten.

Ein Host-Exhaustive-FAIL blockiert weiterhin jeden Abschluss-PASS. Er
blockiert nicht die reine technische Herstelleranalyse; wenn deren direkter
Herstellerpfad nicht belastbar ist, bleibt der Status `BLOCKED`.

### Detaillierte Commit-Schnitte innerhalb dieser Reihenfolge

1. **Adapterkern und Abhängigkeit**
   - `nvs_state_store.hpp/.cpp`, explizite lifetime-sichere
     `NvsStateStoreConfig`, direkte Key-/Namespace-Abbildung über die
     Instanzkonfiguration, per-operation Handle, zweistufiger Read und
     vollständige Statusmatrix;
   - `lib/device_platform_esp_idf/CMakeLists.txt` mit `PRIV_REQUIRES nvs_flash`;
   - Nachweis: kein `fermentation`-Default und keine `fermentation_app`-
     Abhängigkeit im Produktionsadapter, gültige R1-Konfiguration sowie
     fail-closed Verhalten bei ungültiger Konfiguration; gezielter
     Komponentenbuild und Format-/Diffprüfung.

2. **Testbaum und BDL-Seam**
   - Hostprojekt, `esp_blockdev`-Double, testseitige Linker-Wrappers und
     stateful Old-or-New-/Race-/Error-Tests; die A/B-Bezeichnung ist hier
     ausschließlich Testorakel für vollständige Alt-/Neu-Werte, keine
     Adapterpersistenz;
   - kein `private/nvs_lifecycle_test_fixture.hpp`, kein Produktionsfixture,
     kein zweiter öffentlicher Port;
   - Nachweis: `--ci-regression` PASS und vollständiger Hostlauf als eigener
     Verifikationsartefakt.

3. **On-Target-Hardwaretest und Profilisolation**
   - `main/issue_90_nvs_hardware_verification.hpp/.cpp`, die Guard-
     Einbindung in `main/app_main.cpp` und die
     `CONFIG_APP_PROFILE_ESP32_BRINGUP`-Erweiterung in `main/CMakeLists.txt`;
   - `scripts/issue_90_nvs_hardware_verification.py` sowie der schmale
     testseitige `scripts/issue_90_power_cut_hook.py` mit dem versionierten
     UART-/Hook-Protokoll und den repository-relativen Artefaktpfaden;
   - Nachweis: echter `NvsStateStore` mit expliziter R1-Konfiguration,
     22-Schlüssel-Vorbefüllung,
     deterministische Rotation, Neustart, A/B-Hash-Readback, NVS-/Ressourcen-
     Marker und Raw-Page-Beweis für tatsächlichen GC/Erase; Release enthält
     weder Harness-Quelle noch `ISSUE90`-Marker.

4. **Lifecycle-/Composition-Root-Gate**
   - Testbaum beweist Init-/Deinit-/BDL-Besitz, explizite R1-Konfiguration und
     Zerstörungsreihenfolge;
   - der produktive Pfad in `main/app_main.cpp` bleibt unverändert, solange
     kein produktiver `IStateStore`-Verbraucher existiert; die ausschließlich
     testseitige Guard-/Harness-Einbindung ist Schnitt 3;
   - bei späterer echter Verbraucherbindung: Root-Init/Storekonstruktion/
     Verbraucher-/Store-/Deinit-Reihenfolge gemeinsam im betroffenen Root;
   - Nachweis: Initfehler verhindert Konstruktion und Runtime; kein Erase/
     Format/Retry.

5. **Kapazität und Partition**
   - `scripts/issue_90_nvs_capacity.py`,
     `docs/ISSUE_90_CAPACITY_REPORT.md`,
     `partitions/issue_90_state_store.csv`, `sdkconfig.defaults`;
   - 49-Seiten-Untergrenze, 69-Seiten-Auswahl, maximaler Einzelrecord,
     zwei freie Seiten, GC-/Erase-Workload, Appgröße und Flashreserve;
   - Nachweis: bei bestätigten Annahmen direkt umsetzbar; bei Abweichung
     materielles Owner-Gate mit neuer Plan-SHA.

6. **Software-/CI-Integration**
   - `docs/CI_AND_QUALITY_GATES.md`, `.github/workflows/build.yml` und bei
     Artefaktbedarf `scripts/check_ci_artifact_scan_coverage.py`;
   - gezielte Befehle:

     ```bash
     . "$IDF_PATH/export.sh"
     python3 scripts/build_esp_idf_profiles.py all
     python3 scripts/run_esp_idf_static_analysis.py all
     python3 scripts/build_report.py --output build-report.md --append \
       --esp-idf-profiles bringup release --source-git-sha "$(git rev-parse HEAD)"
     python3 scripts/check_architecture_boundaries.py
     python3 scripts/check_secrets.py
     python3 scripts/selftest_quality_gates.py
     git diff --check
     ```

   - Nachweis: beide Profile, Static Analysis, Architektur, Secrets, Gate-
     Selbsttests und Host-CI-Regressionssatz mit PASS/BLOCKED/NOT_RUN.

7. **Hardware-/Power-Cut-Verifikation**
   - ownerverifizierte normale ESP32-Partition, UART-/Reset-/Power-Cut-
     Fixture, den On-Target-Harness aus Schnitt 3,
     `scripts/issue_90_nvs_hardware_verification.py`, vorbefüllten GC-Fall
     und A/B-Orakel;
   - Nachweis: vollständige Matrix, Logs, Partitionsdump, Resetursachen,
     Readbacks, Raw-Page-Evidenz und reale Resource-/Erase-Artefakte. Kein
     Build ersetzt diesen Nachweis.

8. **Ressourcen, Schreiblast und Wear-Grenze**
   - `docs/ISSUE_90_BUILD_REPORT.md` und
     `docs/ISSUE_90_HARDWARE_VERIFICATION.md`;
   - Flash-/Appgröße, DRAM, Heap, größter Block, Stack-HWM, NVS-Stats,
     Initzeit, Set-/Read-Latenz und Schreiblastbudget;
   - repräsentativer Test mit 10.000 deterministischen Rotationen oder dem
     dokumentierten kleineren Testfenster, falls die Hardwaregrenze vorher
     fail-closed greift;
   - Herstellervertrag aus gepinnter NVS-Implementierung, berechnetes
     Schreiblastbudget, NVS-Statistiken und Belastungstest werden getrennt
     ausgewiesen. Es wird kein unbegrenzter Wear-Leveling-/Lebensdauernachweis
     behauptet.

9. **Lizenz und Herkunft**
   - `docs/THIRD_PARTY_COMPONENTS.md`,
     `docs/audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md`,
     `docs/audits/COMPONENT_EVALUATIONS.md` und bei Bedarf
     `docs/ADOPT_OR_BUILD.md` aktualisieren;
   - verwendeter Dateisatz, Herkunft, exakter Commit, Apache-2.0-Lizenz,
     Host-BDL-Referenz und Repository-Notice dokumentieren;
   - `docs/LICENSE_STATUS.md` bleibt unverändert, sofern die Prüfung keine
     konkrete Projektlizenzabweichung findet.

## Ressourcen-, Wear- und Securitygrenze

Das tatsächliche Schreiblastbudget wird aus den kanonischen Aufrufern
abgeleitet: Konfigurationsmutationen, Manifest-/Rootfolge und die in
`RUN_PERSISTENCE.md`/`run_persistence_coordinator.cpp` festgelegten
Checkpoint-/Head-Schreibfolgen. Es werden keine Einzelmessungen aus dem
Zwei-Sekunden-Zyklus, keine unbounded Historie und keine OTA-/PSRAM-Reserve
hinzugefügt.

Der Herstellervertrag ist begrenzt: ESP-IDF NVS bietet die gepinnte
Seiten-/Entry-/GC-/Erase-Mechanik. Der #90-Nachweis ergänzt ihn um die
berechnete konkrete Schreiblast, `nvs_get_stats()` vor/während/nach dem
Belastungsfenster und den repräsentativen 10.000-Rotationen-Test. Die
Nachweisgrenze lautet ausdrücklich: beobachtetes Verhalten in diesem
Workload-/Hardwarefenster, keine garantierte Lebensdauer über alle Geräte-
temperaturen, Flashchargen oder unbeschränkte Laufzeiten.

NVS-Verschlüsselung, Flashverschlüsselung, `nvs_keys` und Secret-at-rest-
Schutz werden in #90 weder aktiviert noch behauptet. Das separate
Security-/Releasegate bleibt offen.

## Gepinnte ESP-IDF-Herstellerquellen

Alle verbindlichen Herstellerbehauptungen beziehen sich ausschließlich auf
die ESP-IDF-v6.0.2-Quellbasis am exakten Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e`. Maßgebliche
Dokumentationsbelege sind die versionierten Dateien dieses Source-Commits:

- [`docs/en/api-reference/storage/nvs_flash.rst`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/docs/en/api-reference/storage/nvs_flash.rst):
  Power-Off-/Recovery-Garantie, atomare Key-Value-Updates und
  `nvs_commit()`;
- [`docs/en/api-guides/file-system-considerations.rst`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/docs/en/api-guides/file-system-considerations.rst):
  "Sudden power-loss protection ... atomic updates".

Eine `stable`-Webseite darf höchstens als bequemer Navigationslink zusätzlich
genannt werden; sie ist keine unveränderliche Beweisquelle. Header und
Source-Dateien werden weiterhin ausschließlich über den exakten Commit
referenziert.

- [Espressif-Commit `f0c7d9b6c603658c832858d0a4f25b5a05ea1760`](https://github.com/espressif/esp-idf/commit/f0c7d9b6c603658c832858d0a4f25b5a05ea1760):
  historische Provenienz bzw. bereits äquivalente Herstelleränderung im
  gepinnten v6.0.2-Verhalten, kein offener Kandidat und keine Projekt-
  abhängigkeit; der nächste Abgleich sucht nach einem weiteren offiziellen
  Fix, relevanten Release-/Branchstand oder belegten anderen Recovery-Fix.

- [`nvs.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/include/nvs.h)
- [`nvs_flash.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/include/nvs_flash.h)
- [`esp_partition.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_partition/include/esp_partition.h)
- [`nvs_api.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_api.cpp)
- [`nvs_handle_simple.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_handle_simple.cpp)
- [`nvs_storage.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_storage.cpp)
- [`nvs_constants.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/private_include/nvs_constants.h)
- [`nvs_pagemanager.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_pagemanager.cpp)
- [`nvs_partition.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_partition.cpp)
- [`nvs_page.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_page.cpp)
- [`Kconfig`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/Kconfig)
- [`esp_blockdev.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_blockdev/include/esp_blockdev.h)
- [`bdl_ramdisk.hpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/host_test/nvs_host_test/main/bdl_ramdisk.hpp)
- [`bdl_ramdisk.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/host_test/nvs_host_test/main/bdl_ramdisk.cpp)
- [`nvs_host_test/sdkconfig.ci.esp_blockdev`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/host_test/nvs_host_test/sdkconfig.ci.esp_blockdev)
- [`partitions_singleapp.csv`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/partition_table/partitions_singleapp.csv)
- [`esp_wifi.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_wifi/include/esp_wifi.h)
- [`esp_wifi_types_generic.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_wifi/include/esp_wifi_types_generic.h)
- [`esp_wifi/Kconfig`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_wifi/Kconfig)

## Roadmap, Draft-PR und Owner-Gate

Die Roadmap wird nur so weit geändert, dass #90 die parallele Software-/Host-
phase, den offenen Callback-12-Befund und das weiterhin offene #29-Pegel-
Restgate korrekt zeigt. Das Pegel-Restgate blockiert #90 fachlich nicht.
Anforderungen und die vollständige Kapazitätsrechnung bleiben in diesem Plan.

Der Draft-PR führt nach dem Plancommit exakt aus:

```text
Plan: docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md
Plan commit: nach dem Commit im PR-Body und im aktuellen SESSION HANDOVER veröffentlichen
Base branch: agent/issue-29-esp32-bringup-plan
Base SHA: 30fa0a8264e2c4564d324340c6bebc204147f477
Dependency: STACKED_ON_PR_116; PR #116 Draft; Issue #29 offen mit NOT_RUN-Pegel-Restgate
Existing implementation: already present in PR #117; known review findings remain
and are frozen until approval of this exact revised plan.
Previous approved plan: da693e8a24735ff2cc09f019b119083f3792882e
Implementation: IMPLEMENTATION_BLOCKED_PENDING_R5_1_PLAN_APPROVAL
Open finding: exhaustive callback 12 / NotFound; no implementation before R5.1 approval
Recommended backend: direct NVS with an owner-approved exact Espressif correction;
if the manufacturer path cannot satisfy the unchanged old-or-new contract,
recommend BLOCKED and do not invent adapter-side transaction persistence
Proposed partition decision: `state_store = 69 pages / 276 KiB`; remains the
direct-NVS R1 selection only while the explicit capacity and 4-MB assumptions
above hold
```

Der PR bleibt Draft. Es gibt kein Ready for review, keinen Merge, kein
Auto-Merge, kein Issueschließen, kein Branchlöschen und keinen Force-Push.
Nach Veröffentlichung wird genau ein aktueller `SESSION HANDOVER` geführt.

## Retarget nach PR #116

Nach dem Merge von PR #116 wird live verifiziert, dass dessen identische
Commits in `main` enthalten sind. Ein neuer Branchverweis oder ein reines
Retarget des unveränderten #90-Branches auf `main` verändert weder Commit-SHA
noch Plan-SHA und benötigt keine neue Freigabe. Eine neue Plan-SHA entsteht
erst durch neu erzeugte Commits, etwa Rebase, Cherry-Pick, Branch-Neuerzeugung
mit neuen Commits oder materielle Plan-/Grundlagenänderung; dann ist vor
Umsetzung erneut die exakte neue Plan-SHA freizugeben.
