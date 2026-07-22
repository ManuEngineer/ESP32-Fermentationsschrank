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
- **Entscheidung:** Die Firmware verwendet keine Kandidatenpins. Zahlenwerte
  duerfen nur in Beispiel- oder lokaler Hardwarekonfiguration mit explizitem
  Status `TBD_HARDWARE` beziehungsweise `confirmed_test` dokumentiert werden.
  Die lokale bestaetigte `config/pins.yaml` bleibt ignoriert.
- **Alternativen:** Plausible Standardpins als Kandidaten in Firmware verwenden.
- **Folgen:** Reale Aktoren bleiben bis zur Hardwareverifikation gesperrt.

## ADR-003: Spezifikation vor Fermentationssteuerung

- **Status:** fulfilled; superseded by ADR-012 for implementation workflow
- **Datum:** 2026-07-20
- **Kontext:** Elektrische Grenzen, Sensorzuordnung und Sicherheitsanforderungen
  des realen Aufbaus waren offen.
- **Entscheidung:** Zunaechst Projektstruktur und vollstaendige Spezifikation,
  keine Aktorsteuerung aufgrund unbestaetigter Annahmen.
- **Alternativen:** Vorlaeufige Regelungslogik implementieren.
- **Folgen:** Die Spezifikation wurde auf `docs/software-specification` erstellt.
  Nach ihrem Merge beginnt die Implementierung issueweise gemaess ADR-012.

## ADR-004: Produkt- oder luftgefuehrter Betrieb

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Ein dauerhaftes Referenzglas bildet unterschiedliche Produktmassen
  und vorgewaermte Produkte nicht verlaesslich ab. Ein direkter Produktfuehler
  soll ausserdem nicht fuer jeden Lauf zwingend sein.
- **Entscheidung:** Mit angeschlossenem und ausgewaehltem Produktfuehler ist
  dieser der primaere Prozesssensor; Schrankluft begrenzt und ueberwacht den
  Prozess. Ohne Produktfuehler ist Schrankluft der primaere Prozesssensor. Der
  Modus wird vor dem Start sichtbar bestaetigt und darf nicht unbemerkt wechseln.
- **Alternativen:** Referenzflasche; Produktfuehler zwingend; nur Lufttemperatur.
- **Folgen:** Der Produktfuehler ist abnehmbar und liegt auf einem getrennten
  externen 1-Wire-Bus. Steckverbinder bleibt `TBD_HARDWARE`.

## ADR-005: Zielqualifikation vor Timerstart

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Ein einmaliges Erreichen des Zielwerts reicht fuer einen
  reproduzierbaren Timerstart nicht aus.
- **Entscheidung:** Vor der Fermentationszeit liegt eine getrennte
  Zielqualifikation. Der massgebende Sensor muss fuer eine definierte Zeit im
  Zielband liegen. Kurze Ausreisser duerfen innerhalb festgelegter Grenzen
  toleriert werden. Erst danach startet der Timer.
- **Alternativen:** Timer beim Programmstart oder beim ersten Zielkontakt.
- **Folgen:** Vorheizen, Zielerreichung und Zielqualifikation zaehlen nicht zur
  Fermentationsdauer.

## ADR-006: Optionales Vorheizen mit zweiter Bestaetigung

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Der leere Schrank soll vor dem Einsetzen bereits vorgewaermter
  Milch temperiert werden koennen.
- **Entscheidung:** Vorheizen ist pro Lauf ein- oder ausschaltbar. Nach dem
  Temperieren fordert das Geraet zum Einsetzen auf. Erst ein zweiter bewusster
  Start beziehungsweise `Weiter` beginnt Zielerreichung und Zielqualifikation.
- **Alternativen:** Produkt immer vor dem ersten Start einsetzen; automatischer
  Timerstart direkt nach Vorheizen.
- **Folgen:** Die Zustandsmaschine besitzt `WAITING_FOR_PRODUCT` und eine lokale
  akustische Meldung.

## ADR-007: Warnung statt sofortigem Abbruch bei langer Zielerreichung

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Grosse Produktmasse, Oeffnen oder geringe Leistung koennen die
  Zielerreichung verzoegern, ohne einen sicheren Weiterbetrieb auszuschliessen.
- **Entscheidung:** Programme besitzen eine maximale erwartete
  Zielerreichungszeit. Bei Ueberschreitung entstehen Warnung und
  Protokolleintrag. Die Regelung versucht weiter, solange kein Sicherheitsfehler
  vorliegt.
- **Alternativen:** unbegrenzt ohne Meldung; sofortiger Abbruch.
- **Folgen:** Prozesswarnungen und Sicherheitsfehler sind getrennt modelliert.

## ADR-008: Festes 4-MB-Ressourcenbudget ohne PSRAM-Abhaengigkeit

- **Status:** accepted; amended by ADR-011
- **Datum:** 2026-07-21
- **Kontext:** Die bestellte Controllerboard-Variante ist mit 4 MB Flash
  beschrieben. Firmware, Weboberflaeche, drei Sprachen, Laufpersistenz und
  Temperaturhistorie konkurrieren um den begrenzten Speicher.
- **Entscheidung:** Release 1 muss mit 4 MB Flash funktionieren und darf keine
  PSRAM voraussetzen. Firmware, Webressourcen, Konfiguration, aktiver
  Laufzustand, Sicherheitsjournal und Historie erhalten feste Budgets. Aktiver
  Lauf und Sicherheit haben Vorrang.
- **Alternativen:** groessere Modulvariante voraussetzen; unbegrenzte Historie;
  PSRAM als Voraussetzung.
- **Folgen:** Partitions-, RAM- und Flashbudgets werden mit realen Builds und der
  Hardware gemessen. Release 1 reserviert gemaess ADR-011 keine dualen OTA-Slots.

## ADR-009: Normale Sicherungen enthalten keine Geheimnisse

- **Status:** accepted
- **Datum:** 2026-07-21
- **Kontext:** Programme und Einstellungen sollen portabel gesichert werden,
  ohne WLAN-Passwoerter, Webpasswort, Service-PIN oder Sitzungen zu verbreiten.
- **Entscheidung:** Der normale Sicherungsexport enthaelt keine Geheimnisse.
  Webpasswort und Service-PIN werden als gesalzene Pruefinformation gespeichert.
  Das wiederverwendbare WLAN-Passwort wird getrennt behandelt und nicht
  exportiert.
- **Alternativen:** portable Sicherung inklusive Geheimnissen; keine Sicherung.
- **Folgen:** Nach Import werden nicht enthaltene Zugangsdaten neu eingerichtet.
  Eine rohe Flashkopie ist kein portabler Anwendungsimport.

## ADR-010: Vergessene Service-PIN erfordert vollstaendigen Werksreset

- **Status:** accepted
- **Datum:** 2026-07-21
- **Kontext:** Ein reiner PIN-Reset wuerde bei physischem Zugriff Zugang zu einer
  bestehenden geschuetzten Konfiguration ermoeglichen.
- **Entscheidung:** Bei vergessener Service-PIN ist nur ein vollstaendiger lokaler
  Werksreset moeglich. Er loescht Benutzerprogramme, Einstellungen,
  Zugangsdaten und Historien, stellt Standards wieder her und behaelt die
  geraetespezifische Touchkalibrierung.
- **Alternativen:** PIN allein zuruecksetzen; nur neues Flashen.
- **Folgen:** Der physische Resetweg ist mehrstufig, lokal und von der
  Touchkalibrierung getrennt.

## ADR-011: UART-Update fuer Release 1, Web-OTA spaeter

- **Status:** accepted
- **Datum:** 2026-07-21
- **Kontext:** Das Projekt befindet sich in Entwicklung, jede Installation muss
  initial ueber UART geflasht werden und 4 MB Flash sind knapp.
- **Entscheidung:** FT232RL/UART ist der verbindliche Update- und Recoveryweg fuer
  Release 1. Ein Single-App-Partitionsplan ist zulaessig. Web-OTA, duale
  Firmware-Slots, signierte Webpakete und automatisches Rollback sind
  `FUTURE_RELEASE`.
- **Alternativen:** Web-OTA zwingend in Release 1; unsicheres Ueberschreiben der
  einzigen laufenden Apppartition.
- **Folgen:** Release 1 bindet keine ungenutzten OTA-Bibliotheken oder
  OTA-Speicherreserven ein. Eine spaetere Partitionsumstellung darf ein erneutes
  UART-Flashen erfordern.

## ADR-012: Software-first mit gemeinsamem Bring-up-Profil

- **Status:** accepted
- **Datum:** 2026-07-21
- **Kontext:** Die reale Hardware trifft spaeter ein, waehrend der groesste Teil
  des fachlichen Systems vorher entwickelt werden soll.
- **Entscheidung:** Fachlicher Kern, Simulation, Persistenz, UI-Modelle und Web
  werden vor der Hardware weitgehend umgesetzt. Hardwarezugriffe liegen hinter
  Ports und Adaptern. Die gleiche Codebasis besitzt `native`, `esp32_bringup`
  und `esp32_release`; es gibt kein separates Wegwerf-Testprojekt.
- **Alternativen:** auf Hardware warten; separate Testfirmware; direkter
  Hardwarezugriff im Kern.
- **Folgen:** Software-Issues koennen durch native Tests abgeschlossen werden,
  waehrend reale Verifikation separat `BLOCKED_HARDWARE` bleibt. Aktoren werden
  im Bring-up erst nach unbelasteter Pegelmessung schrittweise freigegeben.

## ADR-013: Wiederverwendbare ESP32-Geraeteplattform

- **Status:** accepted
- **Datum:** 2026-07-22
- **Kontext:** Weitere ESP32-Projekte sollen gemeinsame technische Grundlagen
  wiederverwenden koennen, ohne Fermentationslogik zu kopieren.
- **Entscheidung:** Projektgeruest, anwendungsneutrale `device_platform` und
  konkrete `fermentation_app` werden getrennt. `main.cpp` bleibt Composition
  Root. Eine Auslagerung in ein eigenes Plattform-Repository erfolgt erst bei
  einem zweiten realen Anwendungsfall oder klarem Wartungsvorteil.
- **Alternativen:** komplettes Fermentationsprojekt als Template kopieren;
  sofortige spekulative Universalbibliothek.
- **Folgen:** Neue Module muessen bewusst Plattform oder Anwendung zugeordnet und
  nativ testbar gehalten werden. Die ausfuehrliche Entscheidung steht in
  [`ADR-013_REUSABLE_DEVICE_PLATFORM.md`](ADR-013_REUSABLE_DEVICE_PLATFORM.md).
