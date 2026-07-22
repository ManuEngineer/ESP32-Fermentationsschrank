@AGENTS.md

## Claude-Code-spezifische Ergänzungen

- Nutze für größere Refactorings den Plan Mode.

### Repository-Bestandsaufnahme (2026-07-22)

- Stand: Issue #9 (PlatformIO-Grundlage) ist abgeschlossen; PR #39 wurde
  squash-gemergt nach `main` (Commit `55b916e`, "Closes #9"). Der Branch
  `feat/issue-9-platformio-foundation` ist inhaltsgleich mit `origin/main`.
  Lokale `main`-Referenz kann nach `git pull` auf einem anderen Branch
  veraltet sein – vor dem naechsten Issue-Branch explizit fast-forwarden.
- Architektur (ADR-013: `main.cpp` → `device_platform`/`fermentation_app`) ist
  vollständig und korrekt umgesetzt; keine inhaltlichen Abweichungen zu
  `AGENTS.md` gefunden.
- Sicherheits-Invarianten fuer Issue #9 (kein Web-OTA, keine echten Aktoren,
  `HARDWARE_UNVERIFIED`-Start) sind per `#error`-Guards/`static_assert` in
  `include/app_config.hpp` sowie native Tests erzwungen, nicht nur dokumentiert.
  Ein spaeter Fix (Commit `c1367fd`) schloss zusaetzlich eine Luecke in
  `hasSafeDefaults()`: fuer `esp32_release` fehlte die Pruefung von
  `startupHardwareState`, nicht nur der `actuatorPolicy`.
- Kein Lint-/Static-Analysis-Tooling konfiguriert.
- Die zuvor abweichenden Prioritätslisten bei Dokument-Widersprüchen wurden
  konsolidiert: `AGENTS.md` verweist jetzt ausschliesslich auf
  `docs/SPECIFICATION_REVIEW.md` als verbindliche Quelle; `docs/DECISIONS.md`
  ist das zentrale ADR-Register.
- `config/hardware.yaml` existiert lokal, ist aber laut `.gitignore` korrekt
  nicht getrackt (bestätigt via `git ls-files`) – keine Geheimnisse, aber
  reale (unbestätigte) Hardware-Recherchenotizen.
