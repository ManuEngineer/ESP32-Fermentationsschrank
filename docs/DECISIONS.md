# Technische Entscheidungen

Entscheidungen werden nicht nur im Code, sondern auch hier festgehalten.

## Vorlage

### ADR-000: Titel

- **Status:** proposed | accepted | superseded
- **Datum:** YYYY-MM-DD
- **Kontext:** Welches Problem wird geloest?
- **Entscheidung:** Welche Variante wird verwendet?
- **Alternativen:** Welche Varianten wurden verworfen?
- **Folgen:** Welche Vor- und Nachteile entstehen?

---

### ADR-001: PlatformIO mit Arduino Framework

- **Status:** accepted
- **Datum:** YYYY-MM-DD
- **Kontext:** Einheitlicher Build fuer lokale Entwicklung, Codex und CI.
- **Entscheidung:** PlatformIO wird als Buildsystem verwendet; das erste
  Firmwareziel nutzt das Arduino Framework.
- **Alternativen:** ESP-IDF direkt, Arduino IDE.
- **Folgen:** Einfacher Einstieg und reproduzierbarer Build; spezielle
  ESP-IDF-Funktionen koennen spaeter bei Bedarf eingebunden werden.
