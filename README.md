# ESP32-Fermentationsschrank

Sichere, lokal bedienbare ESP32-Firmware fuer einen Fermentationsschrank zum
geregelten Heizen und Kuehlen. Regelung und Safety bleiben ohne Cloud, Internet,
WLAN oder Weboberflaeche funktionsfaehig.

## Projektstand

Der aktuelle Arbeitsstand, laufende Pull Requests, naechste Schritte, Blocker
und Ownerentscheidungen stehen ausschliesslich in
[`docs/ROADMAP.md`](docs/ROADMAP.md).

## Technische Einordnung

- ESP-IDF `v6.0.2` ist der einzige ESP32-Produktionspfad.
- PlatformIO dient ausschliesslich dem nativen Hosttestpfad.
- Fachlogik, Plattformports, ESP-IDF-Adapter und Test-Support sind getrennt.
- Unbestaetigte Hardware und offene Safety-Gates bleiben fail-closed.

Die verbindliche Modularchitektur steht in
[`ADR-013`](docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md).

## Dokumentationseinstieg

| Thema | Quelle |
|---|---|
| aktueller Stand und Reihenfolge | [`docs/ROADMAP.md`](docs/ROADMAP.md) |
| Release-1-Basis und Dokumentationsprioritaet | [`docs/SPECIFICATION_REVIEW.md`](docs/SPECIFICATION_REVIEW.md) |
| Architekturentscheidungen | [`docs/DECISIONS.md`](docs/DECISIONS.md) |
| Engineering-Grundsaetze | [`docs/ENGINEERING_PRINCIPLES.md`](docs/ENGINEERING_PRINCIPLES.md) |
| Agentenworkflow | [`docs/AGENT_WORKFLOW.md`](docs/AGENT_WORKFLOW.md) |
| Builds, Tests und CI | [`docs/CI_AND_QUALITY_GATES.md`](docs/CI_AND_QUALITY_GATES.md) |
| reale Hardware | [`docs/HARDWARE.md`](docs/HARDWARE.md) |
| offene Mess- und Abnahmepunkte | [`docs/OPEN_POINTS.md`](docs/OPEN_POINTS.md) |
| Modulindex | [`lib/README.md`](lib/README.md) |

Aufgabenspezifische Fachvertraege werden ueber Issue, freigegebenen Plan,
relevante ADRs und die Lesematrix in `AGENTS.md` bestimmt. Das gesamte
Dokumentationsverzeichnis ist keine pauschale Pflichtlektuere.

## Lizenzstatus

Die allgemeine Projektlizenz ist noch nicht festgelegt. Bis zu einem
ausdruecklichen Entscheid besteht keine allgemeine Freigabe zur Nutzung,
Veraenderung, Weitergabe oder kommerziellen Verwertung der selbst erstellten
Projektinhalte. Drittanbieterinhalte behalten ihre jeweiligen Originallizenzen
und Rechte.

Der kanonische vorlaeufige Stand steht in
[`docs/LICENSE_STATUS.md`](docs/LICENSE_STATUS.md).
