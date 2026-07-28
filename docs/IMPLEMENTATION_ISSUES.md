# Implementierungs-Epics und GitHub-Issues

## Status

Dieses Dokument bildet die verbindliche geplante Implementierungsstruktur ab.
Die Release-1-Spezifikation ist gemergt; erledigte, planbare und blockierte
Arbeiten werden nach ihrem aktuellen Status getrennt ausgewiesen.

## Statuskennzeichnungen

- `PLANNED_SPEC_PENDING`: geplant, aber Detailvertrag oder freigegebener
  Implementierungsplan fehlt noch
- `TRACKING`: koordinierendes Issue, keine direkte Gesamtimplementierung
- `COMPLETED`: umgesetzt und gemergt
- `READY`: fachlich und technisch startbereit
- `BLOCKED_DEPENDENCY`: benannte fachliche oder technische Abhaengigkeit offen
- `BLOCKED_HARDWARE`: reale Hardware oder Messung fehlt
- `TBD_COMMISSIONING`: Wert oder Freigabe wird bei thermischer Inbetriebnahme bestimmt
- `TBD_IMPLEMENTATION_BUDGET`: Entscheidung benoetigt reale Build-/Ressourcenmessung
- `BLOCKED`: andere benannte Abhaengigkeit verhindert die Umsetzung

Die Kennzeichnungen stehen im Issue-Text. GitHub-Labels sind optional.

## Epic-Uebersicht

| Epic | GitHub | Zweck |
|---|---:|---|
| E0 | #2 | Projektgrundlage, Tests, Abstraktionen und Simulator |
| E1 | #3 | Programme und fachlicher Softwarekern |
| E2 | #4 | Konfiguration, Persistenz und Wiederanlauf |
| E3 | #5 | Sensor-, Regel- und Sicherheitskern |
| E4 | #6 | Lokale Bedienung, Web und Diagnose |
| E5 | #7 | ESP32- und Hardwareintegration |
| E6 | #8 | Inbetriebnahme und Release 1 |

## E0 – Projektgrundlage und Testbarkeit

- #9 `PlatformIO-Profile und Projektgrundlage einrichten`
- #10 `Native Tests, CI, virtuelle Zeit und Buildberichte`
- #11 `Hardwareabstraktionen, Mockadapter und Simulator`

```text
#9 -> #10 -> #11
```

#9 ist nach Merge der Spezifikation das erste Implementierungs-Issue.

## E1 – Programme und fachlicher Softwarekern

- #12 `Programmmodelle, Schema und Standardprogramme`
- #13 `Unveraenderlichen Laufschnappschuss und Laufrevisionen implementieren`
- #14 `Zustandsmaschine und Prozessablaeufe implementieren`
- #15 `Laufkommandos, Meldungen und Bedienaktionen implementieren`

```text
#9/#10 -> #12 -> #13 -> #14 -> #15
```

#14 benoetigt virtuelle Zeit und Mockadapter aus #10/#11.

## E2 – Konfiguration, Persistenz und Wiederanlauf

- #16 `Konfigurationsebenen, Validierung und atomare Revisionen`
- #54 `Plattformpersistenz und Wireformat implementieren`
- #55 `Typisierte Konfigurationsdokumente implementieren`
- #56 `Active-/Fallback-Manifeste, Vorschau und Runtimeaktivierung implementieren`
- #57 `Bootstrap, StorageEpoch und Recovery implementieren`
- #17 `Laufpersistenz und Kontrollpunkte implementieren`
- #18 `Wiederanlauf und temperaturgewichteten Fortschritt implementieren`
- #19 `Journale, Aufbewahrung, Bereinigung, Backup und Import`

```text
#12 -> #16 TRACKING
#54 COMPLETED -> #55 COMPLETED -> #56 READY -> #57 BLOCKED_DEPENDENCY
#13/#14/#15/#54 -> #17
#10/#14/#17/#20 -> #18
#16/#17 -> #19
```

#16 wird nicht als einzelner Implementierungs-PR umgesetzt. #54 und #55 sind
abgeschlossen. #56 implementiert den Variante-B-Active-/Fallback-Kern;
#57 folgt nach dessen Merge mit Bootstrap, `StorageEpoch`, Korruptionssperre und
wiederaufnehmbarem Werksreset. Persistentes Pending, Aktivierungsintent und
leere Connectivity-/Authentication-Domaenen sind kein Release-1-Scope.

Der Draft-PR zu #56 liefert die typisierten Producer
`ConfigurationCommitIndeterminate` und `ConfigurationRuntimeFailure`, die
gemeinsame `ConfigurationMutationLease`, High-Water-basierte Identitaeten ohne
separate persistente `MutationSequence`, den begrenzten Preview-/Readervertrag
und den nicht fehlschlagenden Publish nach dem Root-Linearisierungspunkt. Bis
zum Merge und unabhaengigen Abschlussreview bleibt der Live-Status von #56
`READY`; #57 bleibt `BLOCKED_DEPENDENCY` und wird nicht vorweggenommen.

#17 haengt nicht pauschal von #16, #56 oder #57 ab. Seine harten Grundlagen
#13, #14, #15 und #54 sind abgeschlossen; der eigene Plan-first-Schritt bleibt
vor einer Statusaenderung erforderlich. #55 darf als gemergte Grundlage
verwendet werden, ist aber kein fachlicher Blocker. Der reale
ESP32-Speicheradapter folgt bei der Hardwareintegration.

## E3 – Sensor-, Regel- und Sicherheitskern

- #20 `Sensorqualitaet, Filterung und Plausibilitaet implementieren`
- #21 `Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik`
- #22 `Zeitproportionale PI-Regelung und Luftbegrenzung`
- #23 `Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik`
- #24 `Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion`

```text
#10/#11 -> #20 -> #21 -> #22 -> #23
#14/#15/#17/#20/#21/#23 -> #24
#56/#57 Producer-Vertraege -> CONFIGURATION_SAFETY_INTEGRATION_GATE -> #24 vollstaendig
```

Alle Ausgaenge enden in dieser Phase bei abstrakten Aktorbefehlen und Mocks.
#24 erhaelt spaeter typisierte `ConfigurationRuntimeFailure`- und
`ConfigurationUnavailable`-/`ConfigurationIntegrityFailure`-Eingaben sowie den
unbestimmten Root-Commitzustand aus #56/#57, aber keine neue pauschale oder
zyklische Abhaengigkeit auf #16, #56 oder #57. #56/#57 duerfen ihre Producer-
Vertraege und #24 darf seinen Fehlerkern unabhaengig implementieren.

Das nachgelagerte `CONFIGURATION_SAFETY_INTEGRATION_GATE` ist dennoch ein
verbindliches Abschlusskriterium fuer #24. Bevor #24 als vollstaendig
abgeschlossen gelten darf, muessen die realen Producer-Vertraege systemweit
auf persistente Verriegelung, sichere Bootprioritaet beziehungsweise
`SAFE_BOOT`, keine normale Aktorfreigabe und reproduzierbare Fehlerinjektion
abgebildet und getestet sein. Wird der #24-Core zuerst gemergt, bleibt #24 bis
zu dieser Integration offen. Reale Aktoradapter oder eine produktive
Aktorfreigabe duerfen das Gate nicht umgehen.

Die Gate-Matrix umfasst mindestens `ConfigurationRuntimeFailure`,
`ConfigurationUnavailable`, `ConfigurationIntegrityFailure`, einen nach
`CommitOutcomeUnknown` nicht eindeutig aufloesbaren Rootzustand, Neustart ohne
Verlust notwendiger Verriegelung, keine Aktorfreigabe bei unbekanntem Zustand
und Recovery nur gemaess #24-Fehlerresetvertrag.

## E4 – Lokale Bedienung, Web und Diagnose

- #25 `Gemeinsame UI-Modelle, Navigation und Mehrsprachigkeit`
- #26 `Lokale Touchoberflaeche fuer Programme, Lauf und Service`
- #27 `Web-API, Weboberflaeche, Anmeldung und Bedienkonflikte`
- #28 `Diagnose, Diagramme, Serviceablauf und Exporte`

```text
#12/#14/#15/#20/#24 -> #25
#25/#12/#15 -> #26
#25/#12/#15/#16 -> #27
#19/#20/#22/#23/#24/#25 -> #28
```

View-Modelle, Navigation, Texte, Weboberflaeche, Programmeditor, Laufansichten,
Diagnose und Serviceablaeufe werden vor der Hardware gegen den Simulator
entwickelt. Reale Display- und Touchintegration steht in #31.

## E5 – ESP32- und Hardwareintegration

- #29 `ESP32-Bring-up, Partition, Ressourcen und sichere Ausgangszustaende`
- #30 `DS18B20-Busse und reale Sensoradapter integrieren`
- #31 `Display- und Touchadapter integrieren und kalibrieren`
- #32 `Luefter, Summer und Onboard-MOSFET-Ausgaenge integrieren`
- #33 `BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen`

```text
#9/#10/#11/#24 -> #29
#20/#21/#29 -> #30
#25/#26/#29 -> #31
#23/#24/#28/#29 -> #32
#23/#24/#29/#30/#32 -> #33
```

Diese Issues bleiben bis zur realen Hardware `BLOCKED_HARDWARE`.

### Elektrische Reihenfolge

1. Controllerboard und Ausgaenge ohne Aktoren messen.
2. Sensoren, Display und Touch einzeln integrieren.
3. Luefter und Summer einzeln anschliessen und messen.
4. BTS7960 ohne Peltier pruefen.
5. H-Brueckenausgang und Polaritaet mit Multimeter bestaetigen.
6. Peltier erst mit Sicherung, Lueftern, Kuehlkoerper und gueltigen
   Sicherheitssensoren anschliessen.
7. Nur begrenzte Servicepulse fuer Heizen und Kuehlen ausfuehren.

## E6 – Inbetriebnahme und Release 1

- #34 `Sensorvergleich, Offsets und thermische Grundvermessung`
- #35 `PI-Parameter, Luftbegrenzungen und Sicherheitsgrenzen festlegen`
- #36 `Hardwareabnahme, Fehlerinjektionen und Standardprogramme validieren`
- #37 `Siebentaegigen Belastungstest und Release-1-Abnahme durchfuehren`

```text
#29-#33 -> #34 -> #35 -> #36 -> #37
```

Diese Issues bleiben bis zur realen thermischen Inbetriebnahme
`TBD_COMMISSIONING`.

## Meilensteinzuordnung

| Meilenstein | Issues |
|---|---|
| M0 – Softwaregrundlage und simuliertes System | #9–#11 |
| M1 – Getesteter Softwarekern | #12–#24 |
| M2 – Bedienbarer Simulator | #25–#28 |
| M3 – Hardware Bring-up | #29–#33 |
| M4 – Sichere Temperatursteuerung | #30, #32–#35 |
| M5 – Vollstaendige Integration | #26–#33, #36 |
| M6 – Release 1 | #34–#37 |

## Branch- und Pull-Request-Regeln

- Akzeptierte Spezifikation und Live-Abhaengigkeiten zuerst pruefen.
- Ein Branch und Draft-PR pro Implementierungs-Issue.
- Vor nicht trivialer Umsetzung den Plan-first-Workflow aus `AGENTS.md`
  vollstaendig durchlaufen.
- Kleine, pruefbare Pull Requests.
- Keine umfangreiche direkte Implementierung auf `main`.
- Hardwareblockaden verhindern nicht die Entwicklung unabhaengiger Softwareteile.

## Definition of Done

Ein Issue ist nur abgeschlossen, wenn alle zutreffenden Punkte erfuellt sind:

- Implementierung vollstaendig
- native, simulierte oder Hardwaretests vorhanden und bestanden
- ESP32-Zielbuild erfolgreich, soweit relevant
- Ressourcenwirkung geprueft oder sichtbar hardwareabhaengig markiert
- Fehlerfaelle behandelt
- Dokumentation aktualisiert
- keine Geheimnisse eingecheckt
- keine unbestaetigte Hardwareannahme als Tatsache implementiert
- Akzeptanzkriterien erfuellt

Ein hardwareunabhaengiges Software-Issue darf vor Hardwareankunft abgeschlossen
werden. Die reale Validierung bleibt dann in einem verknuepften
`BLOCKED_HARDWARE`-Issue sichtbar.

## Freigaberegel

Ein Zielstatus `READY` ersetzt keine Implementierungsfreigabe. Fuer jede nicht
triviale Umsetzung ist zunaechst ein eigener versionierter Plan im Draft-PR zu
committen und zu pushen. Implementiert wird ausschliesslich nach dem exakten
Ownerkommentar mit freigegebenem Plan-Commit-SHA. Live-Issue-Status und
Abhaengigkeiten werden vor jeder Arbeit erneut geprueft.
