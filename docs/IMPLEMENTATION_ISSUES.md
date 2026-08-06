# Implementierungs-Epics und Issue-Struktur

## Zweck

Dieses Dokument beschreibt die stabile Epic-, Issue- und
Abhaengigkeitsstruktur fuer Release 1. Es fuehrt keine Live-Statusangaben,
PR-Chroniken oder abgeschlossenen Umsetzungsberichte.

Aktueller Status, laufende Pull Requests, naechste Arbeit und Blocker stehen in
[`ROADMAP.md`](ROADMAP.md) und den jeweiligen GitHub-Issues. Der technische
Phasenablauf steht in [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md).

## Epic-Uebersicht

| Epic | Tracking-Issue | Verantwortung |
|---|---:|---|
| E0 | #2 | Projektgrundlage, Tests, Plattformports und Simulator |
| E1 | #3 | Programme und fachlicher Laufkern |
| E2 | #4 | Konfiguration, Persistenz, Recovery und Aufbewahrung |
| E3 | #5 | Sensor-, Regel-, Aktor- und Safetykern |
| E4 | #6 | lokale Bedienung, Web und Diagnose |
| E5 | #7 | ESP-IDF- und reale Hardwareintegration |
| E6 | #8 | thermische Inbetriebnahme und Release 1 |

Epics koordinieren. Nicht triviale Umsetzung erfolgt in den untergeordneten
Issues und ihrem eigenen Plan-first-Draft-PR.

## E0 – Projektgrundlage und Testbarkeit

- #9 – Buildprofile und Projektgrundlage
- #10 – native Tests, CI, virtuelle Zeit und Buildberichte
- #11 – Hardwareabstraktionen, Testadapter und Simulator

```text
#9 -> #10 -> #11
```

## E1 – Programme und fachlicher Laufkern

- #12 – Programmmodelle, Schema und Standardprogramme
- #13 – unveraenderlicher Laufschnappschuss und Laufrevisionen
- #14 – Zustandsmaschine und Prozessablaeufe
- #15 – Laufkommandos, Meldungen und Bedienaktionen

```text
#9/#10 -> #12 -> #13 -> #14 -> #15
#10/#11 -> #14
```

## E2 – Konfiguration, Persistenz und Recovery

- #16 – Tracking fuer den Konfigurationskern und seine uebergreifenden Gates
- #54 – Plattformpersistenz und Wireformat
- #55 – typisierte Konfigurationsdokumente
- #56 – Active-/Fallback-Graph, Vorschau und Runtimeaktivierung
- #57 – Bootstrap, `StorageEpoch`, Korruptionssperre und Recovery
- #17 – Laufpersistenz und Kontrollpunkte
- #18 – Wiederanlauf und temperaturgewichteter Fortschritt
- #19 – Journale, Aufbewahrung, Bereinigung, Backup und Import

```text
#12 -> #16
#54 -> #55 -> #56 -> #57
#13/#14/#15/#54 -> #17
#10/#14/#17/#20 -> #18
#16/#17 -> #19
```

Der Konfigurationskern und die Laufpersistenz bleiben getrennte fachliche
Vertraege. #17 verwendet allgemeine Persistenzports, ist aber kein Teil des
Konfigurationsgraphs.

## E3 – Sensor-, Regel- und Safetykern

- #20 – Sensorqualitaet, Filterung und Plausibilitaet
- #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik
- #22 – zeitproportionale PI-Regelung und Luftbegrenzung
- #23 – Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik
- #24 – Fehlerklassen, Verriegelung, `SAFE_BOOT` und Fehlerinjektion

```text
#10/#11 -> #20 -> #21 -> #22 -> #23
#14/#15/#17/#20/#21/#23 -> #24
#56/#57 -> CONFIGURATION_SAFETY_INTEGRATION_GATE -> Abschluss #24
```

#56 und #57 produzieren typisierte Konfigurations-, Integritaets- und
unbestimmte Commitzustaende. #24 bildet diese auf systemweite Verriegelung,
sichere Bootprioritaet, `SAFE_BOOT`, keine normale Aktorfreigabe und
reproduzierbare Fehlerinjektion ab. Das Gate ist ein Abschlusskriterium, keine
zyklische Implementierungsabhaengigkeit.

Alle E3-Ausgaenge bleiben bis E5 abstrakte Aktorbefehle.

## E4 – Lokale Bedienung, Web und Diagnose

- #25 – gemeinsame UI-Modelle, Device-Shell und Mehrsprachigkeit
- #26 – lokale Touch-Shell und Fermentations-Workspace
- #27 – Web-API, Weboberflaeche, Anmeldung und Bedienkonflikte
- #28 – Diagnose, Diagramme, Serviceablauf und Exporte

```text
#12/#14/#15/#20/#24 -> #25
#25/#12/#14/#15/#20/#24 -> #26
#25/#12/#15/#16 -> #27
#19/#20/#22/#23/#24/#25 -> #28
```

E4 arbeitet gegen gemeinsame Fach- und Plattformvertraege. Reale Display-,
Touch- und Netzwerkadapter bleiben E5.

## E5 – ESP-IDF- und Hardwareintegration

- #29 – ESP32-Bring-up, Partition, Ressourcen und sichere Ausgangszustaende
- #30 – DS18B20-Busse und reale Sensoradapter
- #31 – Renderer, Display-/Touchadapter und Kalibrierung
- #32 – Luefter, Summer und Onboard-MOSFET-Ausgaenge
- #33 – BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen
- #89 – WLAN-Onboarding und Provisionierung evaluieren
- #90 – ESP-IDF-NVS-Adapter fuer `IStateStore`

```text
#9/#10/#11 -> #29
#20/#21/#29 -> #30
#25/#26/#29 -> #31
#23/#24/#28/#29 -> #32
#23/#24/#29/#30/#32 -> #33
#29/#57 -> #89
#29/#54 -> #90
```

#29 liefert zuerst eine aktorfreie Hardware- und Ressourcenbaseline. Produktive
Aktoradapter und belastete Ausgangstests verlangen zusaetzlich den Safetykern
und die jeweiligen elektrischen Gates.

#89 evaluiert browserbasiertes WLAN-Onboarding ergebnisoffen nach
Espressif-first und Adopt-or-build. #90 implementiert ausschliesslich den
vorhandenen allgemeinen `IStateStore`-Vertrag; weder Issue darf Fachlogik oder
bestehende Persistenzformate still ersetzen.

## E6 – Inbetriebnahme und Release 1

- #34 – Sensorvergleich, Offsets und thermische Grundvermessung
- #35 – PI-Parameter, Luftbegrenzungen und Sicherheitsgrenzen
- #36 – Hardwareabnahme, Fehlerinjektionen und Standardprogramme
- #37 – siebentaegiger Belastungstest und Release-1-Abnahme

```text
#29-#33 -> #34
#22/#23/#24/#33/#34 -> #35
#28/#29-#35 -> #36
#36 -> #37
```

Thermische Werte und reale Freigaben bleiben offen, bis die zugehoerigen
Messungen und Release-Gates bestanden sind.

## Meilensteinzuordnung

| Meilenstein | Issues |
|---|---|
| M0 – Softwaregrundlage und Simulator | #9–#11 |
| M1 – fachlicher, persistenter und sicherer Softwarekern | #12–#24 sowie #54–#57 |
| M2 – bedienbarer Simulator | #25–#28 |
| M3 – Hardware-Bring-up | #29–#33, #90 |
| M4 – sichere Temperatursteuerung | #30, #32–#35 |
| M5 – vollstaendige Integration | #26–#36 sowie #89 |
| M6 – Release 1 | #34–#37 |

## Strukturelle Regeln

- Das Live-Issue ist die Quelle fuer konkreten Scope, Status, Akzeptanzkriterien
  und aktuelle Abhaengigkeiten.
- `ROADMAP.md` bestimmt die aktuelle Reihenfolge, nicht diese statische Karte.
- Neue technische Folgeissues werden dem fachlich passenden Epic zugeordnet und
  duerfen keine zweite Produktanforderung erzeugen.
- Hardware- oder Commissioningblockaden verhindern nicht die Entwicklung klar
  unabhaengiger Softwareteile.
- Ein Issue darf nur abgeschlossen werden, wenn seine eigenen Kriterien sowie
  alle benannten Abschluss- und Integrationsgates erfuellt sind.
- Planung, Umsetzung, Review, Tests und Ownerrechte richten sich nach
  `AGENT_WORKFLOW.md`, `CI_AND_QUALITY_GATES.md` und der Root-`AGENTS.md`.
