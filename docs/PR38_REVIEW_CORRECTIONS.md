# Historischer Kompatibilitaetshinweis zu PR #38

## Status

Dieses Dokument ist **nicht normativ**. Es bleibt ausschliesslich bestehen,
damit historische Plaene und Fachtexte mit dem frueheren Dateiverweis weiterhin
nachvollziehbar bleiben.

Die Reviewkorrekturen aus PR #38 wurden in die aktuellen kanonischen Quellen
uebertragen. Fuer neue Planung, Umsetzung und Reviews gelten ausschliesslich die
folgenden Quellen:

- Release-Scope und Dokumentationsprioritaet:
  [`SPECIFICATION_REVIEW.md`](SPECIFICATION_REVIEW.md)
- kritische Persistenzfehler, Transaktionsmarker und Persistenzfehler-Latch:
  [`CRITICAL_PERSISTENCE_SAFETY.md`](CRITICAL_PERSISTENCE_SAFETY.md)
- Boot, Brownout, Watchdog und `SAFE_BOOT`:
  [`SYSTEM_SAFETY_AND_RECOVERY.md`](SYSTEM_SAFETY_AND_RECOVERY.md)
- Laufkontrollpunkte und Transaktionsvertrag:
  [`RUN_PERSISTENCE.md`](RUN_PERSISTENCE.md)
- Unterbrechung, Zeitunsicherheit und Wiederanlauf:
  [`RECOVERY_AND_INTERRUPTION.md`](RECOVERY_AND_INTERRUPTION.md)
- Zustandsnamen und Uebergaenge:
  [`STATE_MACHINE.md`](STATE_MACHINE.md)
- Aktorzeiten und Peltierfreigabe:
  [`ACTUATOR_TIMING.md`](ACTUATOR_TIMING.md)
- verbindliche Testorakel und Release-Gates:
  [`ACCEPTANCE_TESTS.md`](ACCEPTANCE_TESTS.md)

Diese Datei definiert keine eigene Prioritaet, keine Release-Anforderung und
keinen parallelen Safety-, Persistenz- oder Recoveryvertrag. Bei einem
Widerspruch ist sie zu ignorieren und die zustaendige aktuelle Quelle zu
verwenden.

Die urspruenglichen Reviewtexte und ihre Entwicklung bleiben in der Git-Historie
und in PR #38 erhalten.
