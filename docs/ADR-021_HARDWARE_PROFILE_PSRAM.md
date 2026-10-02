# ADR-021: Hardware-Profile; PSRAM als Kapazitaetserweiterung

> Vorschlag. Nach Ownerfreigabe wird der Eintrag in das ADR-Register
> `docs/DECISIONS.md` uebernommen (siehe Plan, Abschnitt 9).

- **Status:** proposed; praezisiert ADR-008
- **Datum:** 2026-10-02
- **Kontext:** Die Plattform (ADR-013) soll weitere Geraetetypen tragen.
  Ein spaeteres Profil mit ESP32-S3 und PSRAM ist wahrscheinlich; das
  aktuelle Geraet bleibt ESP32-WROOM-32E ohne PSRAM.
- **Entscheidung:** Das kleinste Profil (WROOM-32E ohne PSRAM) ist die
  Referenz; jede Plattformfunktion muss dort mit Reserve laufen. Ein
  Hardware-Profil (Boardprofil + sdkconfig-Overlay) setzt Kapazitaeten,
  Budgets und Puffergroessen. Die Speicherplatzierung (intern, DMA, PSRAM)
  entscheidet die Plattform hinter einer Schnittstelle nach Zweck; Apps
  kennen keine Speicherregionen. PSRAM ist nie Voraussetzung der Plattform
  oder einer App, nur Kapazitaetserweiterung eines Profils.
- **Alternativen:** separates Codepfad pro Hardware; PSRAM als Voraussetzung
  ab der naechsten Generation.
- **Folgen:** ADR-008 bleibt fuer Release 1 gueltig. Ein `esp32s3`-Build
  wird vorbereitet, sobald die Platzierungsschnittstelle existiert.
