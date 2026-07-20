# Technische Entscheidungen

## ADR-001: PlatformIO mit Arduino Framework

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Reproduzierbarer Build fuer lokalen Einsatz und CI.
- **Entscheidung:** PlatformIO mit Arduino Framework, C++17 und dem generischen
  Ziel `esp32dev` fuer das vorgesehene ESP32-WROOM-32E-Modul.
- **Alternativen:** ESP-IDF direkt, Arduino IDE.
- **Folgen:** Das Ziel bestaetigt keine konkrete Boardrevision oder Pinbelegung.

## ADR-002: Keine GPIO-Zuweisung vor Hardwarebestaetigung

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Importierte Komponentenangaben enthalten keine verifizierte
  Anschlussbelegung oder aktive Pegel.
- **Entscheidung:** Die Firmware verwendet keine Kandidatenpins und
  `src/main.cpp` konfiguriert auch keinen vermeintlichen Onboard-LED-Pin.
  Zahlenwerte duerfen nur in `config/pins.example.yaml` mit explizitem Status
  `candidate_unconfirmed` dokumentiert werden; die lokale bestaetigte
  `config/pins.yaml` bleibt ignoriert.
- **Alternativen:** Plausible Standardpins als Kandidaten in Firmware verwenden.
- **Folgen:** Der Build ist hardwareunabhaengig sicher, ein Hardwaretest ist bis
  zur Verifikation bewusst eingeschraenkt.

## ADR-003: Noch keine Fermentationssteuerung

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Elektrische Grenzen, Sensorzuordnung und Sicherheitsanforderungen
  des realen Aufbaus sind noch offen.
- **Entscheidung:** Zunaechst nur Projektstruktur, Dokumentation, Metadaten und
  ein serieller Diagnoseeinstieg.
- **Alternativen:** Vorlaeufige Regelungslogik implementieren.
- **Folgen:** Keine unbeabsichtigte Aktorfreigabe durch unbestaetigte Annahmen.

## ADR-004: Produkt- oder luftgefuehrter Betrieb

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Vorgewaermte Joghurtmilch und ein vorgeheizter Schrank lassen sich
  durch ein dauerhaftes Referenzglas nicht verlaesslich abbilden. Ein direkter
  Produktfuehler soll ausserdem nicht fuer jeden Lauf zwingend sein.
- **Entscheidung:** Mit angeschlossenem und ausgewaehltem Produktfuehler ist
  dieser der primaere Prozesssensor; der Luftfuehler unterstuetzt Regelung und
  Sicherheit. Ohne Produktfuehler ist der Luftfuehler der primaere
  Prozesssensor. Der Modus wird vor dem Start sichtbar bestaetigt und darf
  waehrend eines Laufs nicht unbemerkt wechseln.
- **Alternativen:** Immer Referenzflasche; Produktfuehler zwingend;
  ausschliesslich Lufttemperatur.
- **Folgen:** Der Produktfuehler soll abnehmbar werden. Steckverbinder,
  Hot-Plug-Verhalten und Lebensmitteleignung bleiben zu klaeren.

## ADR-005: Zielqualifikation vor Timerstart

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Ein einmaliges Erreichen des Zielwerts reicht fuer einen
  reproduzierbaren Timerstart nicht aus. Der Begriff `Stabilisierung` war
  missverstaendlich.
- **Entscheidung:** Vor der Fermentationszeit liegt eine getrennte kurze
  Zielqualifikation. Der massgebende Sensor muss fuer eine definierte Zeit
  ausreichend im Zielband liegen. Kurze Ausreisser duerfen innerhalb
  festgelegter Grenzen ignoriert werden. Erst danach startet der Timer.
- **Alternativen:** Timer beim Programmstart oder beim ersten Zielkontakt.
- **Folgen:** Vorheizen, Zielerreichung und Zielqualifikation zaehlen nicht zur
  Fermentationsdauer.

## ADR-006: Optionales Vorheizen mit zweiter Bestaetigung

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Der leere Schrank soll vor dem Einsetzen bereits vorgewaermter
  Milch temperiert werden koennen.
- **Entscheidung:** Vorheizen ist pro Lauf ein- oder ausschaltbar. Nach dem
  Temperieren des leeren Schranks fordert das Geraet zum Einsetzen auf. Erst ein
  zweiter bewusster Start beziehungsweise `Weiter` beginnt die erneute
  Zielqualifikation fuer den gewaehlten Sensorbetrieb.
- **Alternativen:** Produkt immer vor dem ersten Start einsetzen; automatischer
  Timerstart direkt nach Vorheizen.
- **Folgen:** Die Zustandsmaschine benoetigt einen Wartezustand fuer den
  Produkteinsatz und ein lokales akustisches Signal.

## ADR-007: Warnung statt sofortigem Abbruch bei langer Zielerreichung

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Grosse Produktmasse, Tueröffnung oder geringe Leistung koennen die
  Zielerreichung verzoegern, ohne einen sicheren Weiterbetrieb auszuschliessen.
- **Entscheidung:** Jedes Programm erhaelt eine maximale erwartete
  Zielerreichungszeit. Bei Ueberschreitung entstehen sichtbare und akustische
  Warnung sowie ein Protokolleintrag. Die Regelung versucht standardmaessig
  weiter, solange kein Sicherheitsfehler vorliegt.
- **Alternativen:** Unbegrenzt ohne Meldung; sofortiger Programmabbruch.
- **Folgen:** Prozesswarnungen und Sicherheitsfehler werden getrennt modelliert.
